<!-- toc -->
分配和释放虚拟内存  
- [虚拟地址的空间的分配与释放](#虚拟地址的空间的分配与释放)
    - [虚拟地址空间分配接口](#虚拟地址空间分配接口)
    - [分配时查找虚拟地址区间](#分配时查找虚拟地址区间)
    - [虚拟地址空间释放接口](#虚拟地址空间释放接口)
    - [释放时查找虚拟地址区间](#释放时查找虚拟地址区间)
- [测试环节：虚拟空间能正常访问么](#测试环节虚拟空间能正常访问么)
<!-- tocstop -->

# 虚拟地址的空间的分配与释放
整个虚拟地址空间是由一个个虚拟地址区间组成的。那么分配一个虚拟地址空间就是在整个虚拟地址空间分割出一个区域，而释放一块虚拟地址空间，就是把这个区域合并到整个虚拟地址空间中去。  
## 虚拟地址空间分配接口
分配虚拟地址空间应该有大小、有类型、有相关标志，还有从哪里开始分配等信息  
```c
krlvadrsmem.c

//start是要分配的虚拟地址空间的开始地址，vassize是要分配的虚拟地址空间的大小，vaslimits是虚拟地址空间的限制，vastype是虚拟地址空间的类型
duiaadr_t vma_new_vadrs_core(mmadrsdsc_t *mm, adr_t start, size_t vassize, u64_t vaslimits, u32_t vastype)
{
    adr_t retadrs = NULL;
    kmvarsdsc_t *newkmvd = NULL, *currkmvd = NULL;
    virmemadrs_t *vma = &mm->msd_virmemadrs;
    knl_spinlock(&vma->vs_lock);
    //查找虚拟地址区间
    currkmvd = vma_find_kmvarsdsc(vma, start, vassize);
    if (NULL == currkmvd)
    {
        retadrs = NULL;
        goto out;
    }
    //进行虚拟地址区间进行检查看能否复用这个数据结构
    if (((NULL == start) || (start == currkmvd->kva_end)) && (vaslimits == currkmvd->kva_limits) && (vastype == currkmvd->kva_maptype))
    {
        //能复用的话，当前虚拟地址区间的结束地址返回
        retadrs = currkmvd->kva_end;
        //扩展当前虚拟地址区间的结束地址为分配虚拟地址区间的大小
        currkmvd->kva_end += vassize;
        vma->vs_currkmvdsc = currkmvd;
        goto out;
    }
    //建立一个新的kmvarsdsc_t虚拟地址区间结构
    newkmvd = new_kmvarsdsc();
    if (NULL == newkmvd)
    {
        retadrs = NULL;
        goto out;
    }
    //如果分配的开始地址为NULL就由系统动态决定
    if (NULL == start)
    {
        //当然是接着当前虚拟地址区间之后开始
        newkmvd->kva_start = currkmvd->kva_end;
    }
    else
    {
        //否则这个新的虚拟地址区间的开始就是请求分配的开始地址
        newkmvd->kva_start = start;
    }
    //设置新的虚拟地址区间的结束地址
    newkmvd->kva_end = newkmvd->kva_start + vassize;
    newkmvd->kva_limits = vaslimits;
    newkmvd->kva_maptype = vastype;
    newkmvd->kva_mcstruct = vma;
    vma->vs_currkmvdsc = newkmvd;
    //将新的虚拟地址区间加入到virmemadrs_t结构中
    list_add(&newkmvd->kva_list, &currkmvd->kva_list);
    //看看新的虚拟地址区间是否是最后一个
    if (list_is_last(&newkmvd->kva_list, &vma->vs_list) == TRUE)
    {
        vma->vs_endkmvdsc = newkmvd;
    }
    //返回新的虚拟地址区间的开始地址
    retadrs = newkmvd->kva_start;
out:
    knl_spinunlock(&vma->vs_lock);
    return retadrs;
}

//start是要分配的虚拟地址空间的开始地址，vassize是要分配的虚拟地址空间的大小，vaslimits是虚拟地址空间的限制，vastype是虚拟地址空间的类型
//分配虚拟地址空间的接口
adr_t vma_new_vadrs(mmadrsdsc_t *mm, adr_t start, size_t vassize, u64_t vaslimits, u32_t vastype)
{
    if (NULL == mm || 1 > vassize)
    {
        return NULL;
    }
    if (NULL != start)
    {
        //进行参数检查，开始地址要和页面（4KB 0x1000）对齐，结束地址不能超过整个虚拟地址空间
        if (((start & 0xfff) != 0) || (0x1000 > start) || (USER_VIRTUAL_ADDRESS_END < (start + vassize)))
        {
            return NULL;
        }
    }
    //调用虚拟地址空间分配的核心函数
    return vma_new_vadrs_core(mm, start, VADSZ_ALIGN(vassize), vaslimits, vastype);
}
```
接口函数进行参数检查，然后调用核心函数完成实际的工作。在核心函数中，会调用 vma_find_kmvarsdsc 函数去查找 virmemadrs_t 结构中的所有 kmvarsdsc_t 结构，找出合适的虚拟地址区间。  
我们允许应用程序指定分配虚拟地址空间的开始地址，也可以由系统决定，**但是应用程序指定的话，分配更容易失败，因为很可能指定的开始地址已经被占用了**  
## 分配时查找虚拟地址区间
之前的vma_find_kmvarsdsc查找虚拟地址空间函数并没有实现: 主要是根据分配的开始地址和大小，在 virmemadrs_t 结构中查找相应的 kmvarsdsc_t 结构。  
它是如何查找的呢？举个例子，比如 virmemadrs_t 结构中有两个 kmvarsdsc_t 结构，A_kmvarsdsc_t 结构表示 0x1000～0x4000 的虚拟地址空间，B_kmvarsdsc_t 结构表示 0x7000～0x9000 的虚拟地址空间。这时，我们分配 2KB 的虚拟地址空间，vma_find_kmvarsdsc 函数查找发现 A_kmvarsdsc_t 结构和 B_kmvarsdsc_t 结构之间正好有 0x4000～0x7000 的空间，刚好放得下 0x2000 大小的空间，于是这个函数就会返回 A_kmvarsdsc_t 结构，否则就会继续向后查找。  
```c
//检查kmvarsdsc_t结构
kmvarsdsc_t *vma_find_kmvarsdsc_is_ok(virmemadrs_t *vmalocked, kmvarsdsc_t *curr, adr_t start, size_t vassize)
{
    kmvarsdsc_t *nextkmvd = NULL;
    adr_t newend = start + (adr_t)vassize;
    //如果curr不是最后一个先检查当前kmvarsdsc_t结构
    if (list_is_last(&curr->kva_list, &vmalocked->vs_list) == FALSE)
    {
        //就获取curr的下一个kmvarsdsc_t结构
        nextkmvd = list_next_entry(curr, kmvarsdsc_t, kva_list);
        //由系统动态决定分配虚拟空间的开始地址
        if (NULL == start)
        {
            //如果curr的结束地址加上分配的大小小于等于下一个kmvarsdsc_t结构的开始地址就返回curr
            if ((curr->kva_end + (adr_t)vassize) <= nextkmvd->kva_start)
            {
                return curr;
            }
        }
        else
        {
            //否则比较应用指定分配的开始、结束地址是不是在curr和下一个kmvarsdsc_t结构之间
            if ((curr->kva_end <= start) && (newend <= nextkmvd->kva_start))
            {
                return curr;
            }
        }
    }
    else
    {
        //否则curr为最后一个kmvarsdsc_t结构
        if (NULL == start)
        {
            //curr的结束地址加上分配空间的大小是不是小于整个虚拟地址空间
            if ((curr->kva_end + (adr_t)vassize) < vmalocked->vs_isalcend)
            {
                return curr;
            }
        }
        else
        {
            //否则比较应用指定分配的开始、结束地址是不是在curr的结束地址和整个虚拟地址空间的结束地址之间
            if ((curr->kva_end <= start) && (newend < vmalocked->vs_isalcend))
            {
                return curr;
            }
        }
    }
    return NULL;
}

//查找kmvarsdsc_t结构
kmvarsdsc_t *vma_find_kmvarsdsc(virmemadrs_t *vmalocked, adr_t start, size_t vassize)
{
    kmvarsdsc_t *kmvdcurrent = NULL, *curr = vmalocked->vs_currkmvdsc;
    adr_t newend = start + vassize;
    list_h_t *listpos = NULL;
    //分配的虚拟空间大小小于4KB不行
    if (0x1000 > vassize)
    {
        return NULL;
    }
    //将要分配虚拟地址空间的结束地址大于整个虚拟地址空间 不行
    if (newend > vmalocked->vs_isalcend)
    {
        return NULL;
    }

    if (NULL != curr)
    {
        //先检查当前kmvarsdsc_t结构行不行
        kmvdcurrent = vma_find_kmvarsdsc_is_ok(vmalocked, curr, start, vassize);
        if (NULL != kmvdcurrent)
        {
            return kmvdcurrent;
        }
    }
    //遍历virmemadrs_t中的所有的kmvarsdsc_t结构
    list_for_each(listpos, &vmalocked->vs_list)
    {
        curr = list_entry(listpos, kmvarsdsc_t, kva_list);
        //检查每个kmvarsdsc_t结构
        kmvdcurrent = vma_find_kmvarsdsc_is_ok(vmalocked, curr, start, vassize);
        if (NULL != kmvdcurrent)
        {
            //如果符合要求就返回
            return kmvdcurrent;
        }
    }
    return NULL;
}
```
## 虚拟地址空间释放接口
```c
//释放虚拟地址空间的核心函数
bool_t vma_del_vadrs_core(mmadrsdsc_t *mm, adr_t start, size_t vassize)
{
    bool_t rets = FALSE;
    kmvarsdsc_t *newkmvd = NULL, *delkmvd = NULL;
    virmemadrs_t *vma = &mm->msd_virmemadrs;
    knl_spinlock(&vma->vs_lock);
    //查找要释放虚拟地址空间的kmvarsdsc_t结构
    delkmvd = vma_del_find_kmvarsdsc(vma, start, vassize);
    if (NULL == delkmvd)
    {
        rets = FALSE;
        goto out;
    }
    //第一种情况要释放的虚拟地址空间正好等于查找的kmvarsdsc_t结构
    if ((delkmvd->kva_start == start) && (delkmvd->kva_end == (start + (adr_t)vassize)))
    {
        //脱链
        list_del(&delkmvd->kva_list);
        //删除kmvarsdsc_t结构
        del_kmvarsdsc(delkmvd);
        vma->vs_kmvdscnr--;
        rets = TRUE;
        goto out;
    }
    //第二种情况要释放的虚拟地址空间是在查找的kmvarsdsc_t结构的上半部分
    if ((delkmvd->kva_start == start) && (delkmvd->kva_end > (start + (adr_t)vassize)))
    {
        //所以直接把查找的kmvarsdsc_t结构的开始地址设置为释放虚拟地址空间的结束地址
        delkmvd->kva_start = start + (adr_t)vassize;
        rets = TRUE;
        goto out;
    }
    //第三种情况要释放的虚拟地址空间是在查找的kmvarsdsc_t结构的下半部分
    if ((delkmvd->kva_start < start) && (delkmvd->kva_end == (start + (adr_t)vassize)))
    {
        //所以直接把查找的kmvarsdsc_t结构的结束地址设置为释放虚拟地址空间的开始地址
        delkmvd->kva_end = start;
        rets = TRUE;
        goto out;
    }
    //第四种情况要释放的虚拟地址空间是在查找的kmvarsdsc_t结构的中间
    if ((delkmvd->kva_start < start) && (delkmvd->kva_end > (start + (adr_t)vassize)))
    {
        //所以要再新建一个kmvarsdsc_t结构来处理释放虚拟地址空间之后的下半虚拟部分地址空间
        newkmvd = new_kmvarsdsc();
        if (NULL == newkmvd)
        {
            rets = FALSE;
            goto out;
        }
        //让新的kmvarsdsc_t结构指向查找的kmvarsdsc_t结构的后半部分虚拟地址空间
        newkmvd->kva_end = delkmvd->kva_end;
        newkmvd->kva_start = start + (adr_t)vassize;
        //和查找到的kmvarsdsc_t结构保持一致
        newkmvd->kva_limits = delkmvd->kva_limits;
        newkmvd->kva_maptype = delkmvd->kva_maptype;
        newkmvd->kva_mcstruct = vma;
        delkmvd->kva_end = start;
        //加入链表
        list_add(&newkmvd->kva_list, &delkmvd->kva_list);
        vma->vs_kmvdscnr++;
        //是否为最后一个kmvarsdsc_t结构
        if (list_is_last(&newkmvd->kva_list, &vma->vs_list) == TRUE)
        {
            vma->vs_endkmvdsc = newkmvd;
            vma->vs_currkmvdsc = newkmvd;
        }
        else
        {
            vma->vs_currkmvdsc = newkmvd;
        }
        rets = TRUE;
        goto out;
    }
    rets = FALSE;
out:
    knl_spinunlock(&vma->vs_lock);
    return rets;
}

//释放虚拟地址空间的接口
//mmadrsdsc_t *mm：进程的虚拟地址空间描述符
bool_t vma_del_vadrs(mmadrsdsc_t *mm, adr_t start, size_t vassize)
{    //对参数进行检查
    if (NULL == mm || 1 > vassize || NULL == start)
    {
        return FALSE;
    }
    //调用核心处理函数
    return vma_del_vadrs_core(mm, start, VADSZ_ALIGN(vassize));
}
```
![解读](./images/jiedu.jpg)
因为分配虚拟地址空间时，我们为了节约 kmvarsdsc_t 结构占用的内存空间，规定只要分配的虚拟地址空间上一个虚拟地址空间是连续且类型相同的，我们就借用上一个 kmvarsdsc_t 结构，而不是重新分配一个 kmvarsdsc_t 结构表示新分配的虚拟地址空间。你可以想像一下，一个应用每次分配一个页面的虚拟地址空间，不停地分配，而每个新分配的虚拟地址空间都有一个 kmvarsdsc_t 结构对应，这样物理内存将很快被耗尽。  
## 释放时查找虚拟地址区间
虚拟地址空间的核心处理函数 vma_del_vadrs_core 函数中，调用了 vma_del_find_kmvarsdsc 函数，用于查找要释放虚拟地址空间的 kmvarsdsc_t 结构，可是为什么不用分配虚拟地址空间时那个查找函数（vma_find_kmvarsdsc）呢？  
因为释放时查找的要求不一样。释放时仅仅需要保证，释放的虚拟地址空间的开始地址和结束地址，他们落在某一个 kmvarsdsc_t 结构表示的虚拟地址区间就行，所以我们还是另写一个函数  
```c
kmvarsdsc_t *vma_del_find_kmvarsdsc(virmemadrs_t *vmalocked, adr_t start, size_t vassize)
{
    kmvarsdsc_t *curr = vmalocked->vs_currkmvdsc;
    adr_t newend = start + (adr_t)vassize;
    list_h_t *listpos = NULL;

    if (NULL != curr)
    {
        //释放的虚拟地址空间落在了当前kmvarsdsc_t结构表示的虚拟地址区间
        if ((curr->kva_start) <= start && (newend <= curr->kva_end))
        {
            return curr;
        }
    }
    //遍历所有的kmvarsdsc_t结构
    list_for_each(listpos, &vmalocked->vs_list)
    {
        curr = list_entry(listpos, kmvarsdsc_t, kva_list);
        //释放的虚拟地址空间是否落在了其中的某个kmvarsdsc_t结构表示的虚拟地址区间
        if ((start >= curr->kva_start) && (newend <= curr->kva_end))
        {
            return curr;
        }
    }
    return NULL;
}
```
释放时，查找虚拟地址区间的函数非常简单，仅仅是检查释放的虚拟地址空间是否落在查找 kmvarsdsc_t 结构表示的虚拟地址区间中，而可能的四种变换形式，交给核心释放函数处理。  

# 测试环节：虚拟空间能正常访问么







































