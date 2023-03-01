<!-- toc -->
虽然之前内存管理相关的数据结构已经定义好了，但是我们还没有在内存中建立对应的实例变量。在代码中实际操作的数据结构必须在内存中有相应的变量，这节我们就去建立对应的实例变量，并初始化它们。  
- [总结](#总结)
- [初始化](#初始化)
- [内存页结构初始化（msadsc_t）](#内存页结构初始化msadsc_t)
- [内存区结构初始化](#内存区结构初始化)
- [处理初始内存占用问题](#处理初始内存占用问题)
- [合并内存页到内存区](#合并内存页到内存区)
- [初始化汇总](#初始化汇总)
<!-- tocstop -->

# 总结
init_hal->init_halmm->init_memmgr  
//每个页对应一个msadsc_t 结构体，循环填充msadsc_t 结构体数组  
->init_msadsc  
//初始化三类memarea_t，硬件区、内核区、用户区  
->init_memarea  
//对已使用的页打上标记，包括：BIOS中断表、内核栈、内核、内核映像  
->init_search_krloccupymm(&kmachbsp);       
//将页面按地址范围，分配给内存区  
//然后按顺序依次查找最长连续的页面，根据连续页面的长度，
//将这些页面的msadsc_t挂载到memdivmer_t 结构下的bafhlst_t数组dm_mdmlielst中  
->init_merlove_mem();  
//物理地址转为虚拟地址，便于以后使用  
->init_memmgrob();  

**在 4GB 的物理内存的情况下，msadsc_t 结构实例变量本身占用多大的内存空间？**  
用了虚拟机进行测试，但无论内存大小，总有56K内存没能找到：  
1、4G内存情况如下：  
理论内存：0x1 0000 0000 = 4,194,304K  
可用内存：0xfff8fc00 = 4,193,855K  
预留区域：0x52400  = 329K  
硬件使用：0x10000  = 64K  
没能找到：0xE000 = 56K  

msadsc_t结构体大小为40，使用内存总计为：  
4,193,855K/4K*40=41,938,520=接近40M  

2、2G内存情况如下  
理论内存：0x8000 0000 =2,097,152K  
可用内存：0x7ff8fc00 = 2,096,703K  
预留区域：0x52400  = 329K  
硬件使用：0x10000  = 64K  
没能找到：0xE000 = 56K  

msadsc_t结构体大小为40，使用内存总计为：  
2,096,703K/4K*40=20,967,030=接近20M  

3、1G内存情况如下  
理论内存：0x4000 0000= 1,048,576K  
可用内存：0x3ff8fc00 = 1,048,127K  
预留区域：0x52400  = 329K  
硬件使用：0x10000  = 64K  
没能找到：0xE000 = 56K  

msadsc_t结构体大小为40，使用内存总计为：  
1,048,127K/4K*40=10,481,270=接近10M  

三、如果想节约msadsc_t内存的话，感觉有几种方案：  
1、最简单的方法，就是大内存时采用更大的分页，但应用在申请内存时，同样会有更多内存浪费  
2、也可以用更复杂的页面管理机制，比如相同属性的连续页面不要用多个单独msadsc_t表示，而用一个msadsc_t表示并标明其范围，并通过skiplist等数据结构加速查询。但无论是申请内存还是归还内存时，性能会有所下降，感觉得不偿失。  
3、页面分组情况较少的时候，可以通过每个组建立一个链表记录哪些页面属于某个链表，而msadsc_t中只记录地址等少量信息，不适合复杂系统。  

# 初始化
_之前，我们在 hal 层初始化中，初始化了从二级引导器中获取的内存布局信息，也就是那个 e820map_t 数组，并把这个数组转换成了 phymmarge_t 结构数组，还对它做了排序。但我们 HuOS 物理内存管理器剩下的部分还没有完成初始化。_  

HuOS 的物理内存管理器，我们依然要放在 HuOS 的 hal 层。
因为物理内存还和硬件平台相关，所以我们要在 HuOS/hal/x86/ 目录下建立一个 memmgrinit.c 文件，在这个文件中写入一个 HuOS 物理内存管理器初始化的大总管——init_memmgr 函数，并在 init_halmm 函数中调用它
```c
//HuOS/hal/x86/halmm.c
//hal层的内存初始化函数
void init_halmm()
{
    init_phymmarge();
    init_memmgr();
    return;
}

//HuOS/hal/x86/memmgrinit.c
//HuOS物理内存管理器初始化
void init_memmgr()
{
    //初始化内存页结构msadsc_t
    //初始化内存区结构memarea_t
    return;
}
```

# 内存页结构初始化（msadsc_t）
一个 msadsc_t 结构体变量代表一个物理内存页，而物理内存由多个页组成，所以最终会形成一个 msadsc_t 结构体数组。  
1. 我们只需要找一个内存地址，作为 msadsc_t 结构体数组的开始地址，当然这个内存地址必须是可用的，而且之后内存空间足以存放 msadsc_t 结构体数组  
2. 给这个开始地址加上 0x1000，如此循环，直到其结束地址  
3. 当这个 phymmarge_t 结构体的地址区间，它对应的所有 msadsc_t 结构体都建立完成之后，就开始下一个 phymmarge_t 结构体。依次类推，最后，我们就能建好所有可用物理内存页面对应的 msadsc_t 结构体。  
```c
HuOS/hal/x86/msadsc.c

void write_one_msadsc(msadsc_t *msap, u64_t phyadr)
{
    //对msadsc_t结构做基本的初始化，比如链表、锁、标志位
    msadsc_t_init(msap);
    //这是把一个64位的变量地址转换成phyadrflgs_t*类型方便取得其中的地址位段
    phyadrflgs_t *tmp = (phyadrflgs_t *)(&phyadr);
    //把页的物理地址写入到msadsc_t结构中
    msap->md_phyadrs.paf_padrs = tmp->paf_padrs;
    return;
}

//mbsp->mb_e820exnr存储phymmarge_t的数量
u64_t init_msadsc_core(machbstart_t *mbsp, msadsc_t *msavstart, u64_t msanr)
{
    //获取phymmarge_t结构数组开始地址
    phymmarge_t *pmagep = (phymmarge_t *)phyadr_to_viradr((adr_t)mbsp->mb_e820expadr);
    u64_t mdindx = 0;
    //扫描phymmarge_t结构数组
    for (u64_t i = 0; i < mbsp->mb_e820exnr; i++)
    {
        //判断phymmarge_t结构的类型是不是可用内存
        if (PMR_T_OSAPUSERRAM == pmagep[i].pmr_type)
        {
            //遍历phymmarge_t结构的地址区间
            for (u64_t start = pmagep[i].pmr_saddr; start < pmagep[i].pmr_end; start += 4096)
            {
                //每次加上4KB-1比较是否小于等于phymmarge_t结构的结束地址
                if ((start + 4096 - 1) <= pmagep[i].pmr_end)
                {
                    //与当前地址为参数写入第mdindx个msadsc结构
                    write_one_msadsc(&msavstart[mdindx], start);
                    mdindx++;
                }
            }
        }
    }
    return mdindx;
}

void init_msadsc()
{
    u64_t coremdnr = 0, msadscnr = 0;
    msadsc_t *msadscvp = NULL;
    machbstart_t *mbsp = &kmachbsp;
    //计算msadsc_t结构数组的开始地址和数组元素个数
    if (ret_msadsc_vadrandsz(mbsp, &msadscvp, &msadscnr) == FALSE)
    {
//ret_msadsc_vadrandsz 函数也是遍历 phymmarge_t 结构数组，计算出有多大的可用内存空间，可以分成多少个页面，需要多少个 msadsc_t 结构。
        system_error("init_msadsc ret_msadsc_vadrandsz err\n");
    }
    //开始真正初始化msadsc_t结构数组
    coremdnr = init_msadsc_core(mbsp, msadscvp, msadscnr);
    if (coremdnr != msadscnr)
    {
        system_error("init_msadsc init_msadsc_core err\n");
    }
    //将msadsc_t结构数组的开始的物理地址写入kmachbsp结构中 
    mbsp->mb_memmappadr = viradr_to_phyadr((adr_t)msadscvp);
    //将msadsc_t结构数组的元素个数写入kmachbsp结构中 
    mbsp->mb_memmapnr = coremdnr;
    //将msadsc_t结构数组的大小写入kmachbsp结构中 
    mbsp->mb_memmapsz = coremdnr * sizeof(msadsc_t);
    //计算下一个空闲内存的开始地址 
    mbsp->mb_nextwtpadr = PAGE_ALIGN(mbsp->mb_memmappadr + mbsp->mb_memmapsz);
    return;
}
```

# 内存区结构初始化
整个物理地址空间在逻辑上分成了三个区：硬件区、内核区、用户区，这就要求我们要在内存中建立三个 memarea_t 结构体的实例变量。在内存中找个空闲空间，存放这三个 memarea_t 结构体  
```c
HuOS/hal/x86/memarea.c

void bafhlst_t_init(bafhlst_t *initp, u32_t stus, uint_t oder, uint_t oderpnr)
{
    //初始化bafhlst_t结构体的基本数据
    knl_spinlock_init(&initp->af_lock);
    initp->af_stus = stus;
    initp->af_oder = oder;
    initp->af_oderpnr = oderpnr;
    initp->af_fobjnr = 0;
    initp->af_mobjnr = 0;
    initp->af_alcindx = 0;
    initp->af_freindx = 0;
    list_init(&initp->af_frelst);
    list_init(&initp->af_alclst);
    list_init(&initp->af_ovelst);
    return;
}

void memdivmer_t_init(memdivmer_t *initp)
{
    //初始化memdivmer_t结构体的基本数据
    knl_spinlock_init(&initp->dm_lock);
    initp->dm_stus = 0;
    initp->dm_divnr = 0;
    initp->dm_mernr = 0;
    //循环初始化memdivmer_t结构体中dm_mdmlielst数组中的每个bafhlst_t结构的基本数据
    for (uint_t li = 0; li < MDIVMER_ARR_LMAX; li++)
    {
        bafhlst_t_init(&initp->dm_mdmlielst[li], BAFH_STUS_DIVM, li, (1UL << li));
    }
    bafhlst_t_init(&initp->dm_onemsalst, BAFH_STUS_ONEM, 0, 1UL);
    return;
}

void memarea_t_init(memarea_t *initp)
{
    //初始化memarea_t结构体的基本数据
    list_init(&initp->ma_list);
    knl_spinlock_init(&initp->ma_lock);
    initp->ma_stus = 0;
    initp->ma_flgs = 0;
    initp->ma_type = MA_TYPE_INIT;
    initp->ma_maxpages = 0;
    initp->ma_allocpages = 0;
    initp->ma_freepages = 0;
    initp->ma_resvpages = 0;
    initp->ma_horizline = 0;
    initp->ma_logicstart = 0;
    initp->ma_logicend = 0;
    initp->ma_logicsz = 0;
    //初始化memarea_t结构体中的memdivmer_t结构体
    memdivmer_t_init(&initp->ma_mdmdata);
  //调用了 memdivmer_t_init 函数，而在 memdivmer_t_init 函数中又调用了 bafhlst_t_init 函数，这保证了那些被包含的数据结构得到了初始化。
    initp->ma_privp = NULL;
    return;
}

bool_t init_memarea_core(machbstart_t *mbsp)
{
    //获取memarea_t结构开始地址
    u64_t phymarea = mbsp->mb_nextwtpadr;
    //检查内存空间够不够放下MEMAREA_MAX个memarea_t结构实例变量
    if (initchkadr_is_ok(mbsp, phymarea, (sizeof(memarea_t) * MEMAREA_MAX)) != 0)
    {
        return FALSE;
    }
    memarea_t *virmarea = (memarea_t *)phyadr_to_viradr((adr_t)phymarea);
    for (uint_t mai = 0; mai < MEMAREA_MAX; mai++)
    {   //循环初始化每个memarea_t结构实例变量
        memarea_t_init(&virmarea[mai]);
      //调用了 memarea_t_init 函数，对 MEMAREA_MAX 个 memarea_t 结构进行了基本的初始化。
    }
    //设置硬件区的类型和空间大小
    virmarea[0].ma_type = MA_TYPE_HWAD;
    virmarea[0].ma_logicstart = MA_HWAD_LSTART;
    virmarea[0].ma_logicend = MA_HWAD_LEND;
    virmarea[0].ma_logicsz = MA_HWAD_LSZ;
    //设置内核区的类型和空间大小
    virmarea[1].ma_type = MA_TYPE_KRNL;
    virmarea[1].ma_logicstart = MA_KRNL_LSTART;
    virmarea[1].ma_logicend = MA_KRNL_LEND;
    virmarea[1].ma_logicsz = MA_KRNL_LSZ;
    //设置应用区的类型和空间大小
    virmarea[2].ma_type = MA_TYPE_PROC;
    virmarea[2].ma_logicstart = MA_PROC_LSTART;
    virmarea[2].ma_logicend = MA_PROC_LEND;
    virmarea[2].ma_logicsz = MA_PROC_LSZ;
    //将memarea_t结构的开始的物理地址写入kmachbsp结构中 
    mbsp->mb_memznpadr = phymarea;
    //将memarea_t结构的个数写入kmachbsp结构中 
    mbsp->mb_memznnr = MEMAREA_MAX;
    //将所有memarea_t结构的大小写入kmachbsp结构中 
    mbsp->mb_memznsz = sizeof(memarea_t) * MEMAREA_MAX;
    //计算下一个空闲内存的开始地址 
    mbsp->mb_nextwtpadr = PAGE_ALIGN(phymarea + sizeof(memarea_t) * MEMAREA_MAX);
    return TRUE;
}

//初始化内存区
void init_memarea()
{
    //真正初始化内存区
    if (init_memarea_core(&kmachbsp) == FALSE)
    {
        system_error("init_memarea_core fail");
    }
    return;
}
```

# 处理初始内存占用问题
目前我们的内存中已经有很多数据了，有内核本身的执行文件，有字体文件，有 MMU 页表，有打包的内核映像文件，还有刚刚建立的内存页和内存区的数据结构，这些数据都要占用实际的物理内存。  
我们建立内存页结构 msadsc_t，所有的都是空闲状态，而它们每一个都表示一个实际的物理内存页。  
```
内核本身的执行文件(ELF)包含了完整的内核代码和数据，而内核映像文件(gzip)只包含内核的基本代码，不包含驱动程序等。这使得内核本身的执行文件更适合用于升级操作系统内核或者调试内核问题，而内核映像文件更适合用于安装操作系统或者制作自定义的 Live CD。
```
目前调用内存分配接口进行内存分配会按既定的分配算法查找空闲的 msadsc_t 结构，那一定会找到内核占用的内存页所对应的 msadsc_t 结构，并把这个内存页分配出去，然后得到这个页面的程序对其进行改写。**这样内核数据就会被覆盖，这种情况是我们绝对不能允许的。**  
所以，我们要把这些已经占用的内存页面所对应的 msadsc_t 结构标记出来，标记成已分配，这样内存分配算法就不会找到它们了。我们只要给出被占用内存的起始地址和结束地址，然后从起始地址开始查找对应的 msadsc_t 结构，再把它标记为已经分配，最后直到查找到结束地址为止  

```c
HuOS/hal/x86/msadsc.c

//搜索一段内存地址空间所对应的msadsc_t结构
u64_t search_segment_occupymsadsc(msadsc_t *msastart, u64_t msanr, u64_t ocpystat, u64_t ocpyend)
{
    u64_t mphyadr = 0, fsmsnr = 0;
    msadsc_t *fstatmp = NULL;
    for (u64_t mnr = 0; mnr < msanr; mnr++)
    {
        if ((msastart[mnr].md_phyadrs.paf_padrs << PSHRSIZE) == ocpystat)
        {
            //找出开始地址对应的第一个msadsc_t结构，就跳转到step1
            fstatmp = &msastart[mnr];
            goto step1;
        }
    }
step1:
    fsmsnr = 0;
    if (NULL == fstatmp)
    {
        return 0;
    }
    for (u64_t tmpadr = ocpystat; tmpadr < ocpyend; tmpadr += PAGESIZE, fsmsnr++)
    {
        //从开始地址对应的第一个msadsc_t结构开始设置，直到结束地址对应的最后一个masdsc_t结构
        mphyadr = fstatmp[fsmsnr].md_phyadrs.paf_padrs << PSHRSIZE;
        if (mphyadr != tmpadr)
        {
            return 0;
        }
        if (MF_MOCTY_FREE != fstatmp[fsmsnr].md_indxflgs.mf_mocty ||
            0 != fstatmp[fsmsnr].md_indxflgs.mf_uindx ||
            PAF_NO_ALLOC != fstatmp[fsmsnr].md_phyadrs.paf_alloc)
        {
            return 0;
        }
        //设置msadsc_t结构为已经分配，已经分配给内核
        fstatmp[fsmsnr].md_indxflgs.mf_mocty = MF_MOCTY_KRNL;
        fstatmp[fsmsnr].md_indxflgs.mf_uindx++;
        fstatmp[fsmsnr].md_phyadrs.paf_alloc = PAF_ALLOC;
    }
    //进行一些数据的正确性检查
    u64_t ocpysz = ocpyend - ocpystat;
    if ((ocpysz & 0xfff) != 0)
    {
        if (((ocpysz >> PSHRSIZE) + 1) != fsmsnr)
        {
            return 0;
        }
        return fsmsnr;
    }
    if ((ocpysz >> PSHRSIZE) != fsmsnr)
    {
        return 0;
    }
    return fsmsnr;
}

bool_t search_krloccupymsadsc_core(machbstart_t *mbsp)
{
    u64_t retschmnr = 0;
    msadsc_t *msadstat = (msadsc_t *)phyadr_to_viradr((adr_t)mbsp->mb_memmappadr);
    u64_t msanr = mbsp->mb_memmapnr;
    //搜索BIOS中断表占用的内存页所对应msadsc_t结构
    retschmnr = search_segment_occupymsadsc(msadstat, msanr, 0, 0x1000);
    if (0 == retschmnr)
    {
        return FALSE;
    }
    //搜索内核栈占用的内存页所对应msadsc_t结构
    retschmnr = search_segment_occupymsadsc(msadstat, msanr, mbsp->mb_krlinitstack & (~(0xfffUL)), mbsp->mb_krlinitstack);
    if (0 == retschmnr)
    {
        return FALSE;
    }
    //搜索内核占用的内存页所对应msadsc_t结构
    retschmnr = search_segment_occupymsadsc(msadstat, msanr, mbsp->mb_krlimgpadr, mbsp->mb_nextwtpadr);
    if (0 == retschmnr)
    {
        return FALSE;
    }
    //搜索内核映像文件占用的内存页所对应msadsc_t结构
    retschmnr = search_segment_occupymsadsc(msadstat, msanr, mbsp->mb_imgpadr, mbsp->mb_imgpadr + mbsp->mb_imgsz);
    if (0 == retschmnr)
    {
        return FALSE;
    }
    return TRUE;
}

//初始化搜索内核占用的内存页面
void init_search_krloccupymm(machbstart_t *mbsp)
{
    //实际初始化搜索内核占用的内存页面
    if (search_krloccupymsadsc_core(mbsp) == FALSE)
    {
        system_error("search_krloccupymsadsc_core fail\n");
    }
    return;
}
//由 init_search_krloccupymm 函数入口，search_krloccupymsadsc_core 函数驱动，由 search_segment_occupymsadsc 函数完成实际的工作。
//由于初始化阶段各种数据占用的开始、结束地址和大小，这些信息都保存在 machbstart_t 类型的 kmachbsp 变量中，所以函数与 machbstart_t 类型的指针为参数。
```
代码解析:  
msastart：指向内存描述符(msadsc_t)结构数组的指针，用于描述整个内存空间的使用情况。  
msanr：内存描述符结构数组的长度，即内存描述符的个数。  
ocpystat：需要分配的连续物理地址空间的起始地址。  
ocpyend：需要分配的连续物理地址空间的结束地址。  

**该函数的实现过程如下：**  
- 遍历内存描述符结构数组，寻找到起始地址所在的内存描述符结构，并跳转到标签“step1”。
- 对起始地址所在的内存描述符结构以及后续的结构进行检查和设置，直到结束地址所在的内存描述符结构。
- 检查所分配的物理地址空间是否满足一些数据的正确性要求，包括物理页面大小的对齐以及分配页面数量的正确性。
- 返回分配的物理页面数量。

其实 phymmarge_t、msadsc_t、memarea_t 这些结构的实例变量和 MMU 页表，它们所占用的内存空间已经涵盖在了内核自身占用的内存空间。以后只要在初始化内存页结构和内存区结构之后调用 init_search_krloccupymm 函数即可。  

# 合并内存页到内存区
目前依然没有让内存页和内存区联系起来，即让 msadsc_t 结构挂载到内存区对应的数组中。只有这样，我们才能提高内存管理器的分配速度。  

1. 确定内存页属于哪个区，即标定一系列 msadsc_t 结构是属于哪个 memarea_t 结构的。
2. 把特定的内存页合并，然后挂载到特定的内存区下的 memdivmer_t 结构中的 dm_mdmlielst 数组中。

先遍历每个 memarea_t 结构，遍历过程中根据特定的 memarea_t 结构，然后去扫描整个 msadsc_t 结构数组，最后依次对比 msadsc_t 的物理地址，看它是否落在 memarea_t 结构的地址区间中。如果是，就把这个 memarea_t 结构的类型值写入 msadsc_t 结构中，这样就一个一个打上了标签，遍历 memarea_t 结构结束之后，每个 msadsc_t 结构就只归属于某一个 memarea_t 结构了。  
```c
hal/x86/memarea.c

//给msadsc_t结构打上标签
uint_t merlove_setallmarflgs_onmemarea(memarea_t *mareap, msadsc_t *mstat, uint_t msanr)
{
    u32_t muindx = 0;
    msadflgs_t *mdfp = NULL;
    //获取内存区类型
    switch (mareap->ma_type){
    case MA_TYPE_HWAD:
        muindx = MF_MARTY_HWD << 5;  //硬件区标签
        mdfp = (msadflgs_t *)(&muindx);
        break;
    case MA_TYPE_KRNL:
        muindx = MF_MARTY_KRL << 5;  //内核区标签
        mdfp = (msadflgs_t *)(&muindx);
        break;
    case MA_TYPE_PROC:
        muindx = MF_MARTY_PRC << 5;  //应用区标签
        mdfp = (msadflgs_t *)(&muindx);
        break;
    }
    u64_t phyadr = 0;
    uint_t retnr = 0;
    //扫描所有的msadsc_t结构
    for (uint_t mix = 0; mix < msanr; mix++)
    {
        if (MF_MARTY_INIT == mstat[mix].md_indxflgs.mf_marty)
        {    //获取msadsc_t结构对应的地址
            phyadr = mstat[mix].md_phyadrs.paf_padrs << PSHRSIZE;
            //和内存区的地址区间比较 
            if (phyadr >= mareap->ma_logicstart && ((phyadr + PAGESIZE) - 1) <= mareap->ma_logicend)
            {
                //设置msadsc_t结构的标签
                mstat[mix].md_indxflgs.mf_marty = mdfp->mf_marty;
                retnr++;
            }
        }
    }
    return retnr;
}

bool_t merlove_mem_core(machbstart_t *mbsp)
{
    //获取msadsc_t结构的首地址
    msadsc_t *mstatp = (msadsc_t *)phyadr_to_viradr((adr_t)mbsp->mb_memmappadr);
    //获取msadsc_t结构的个数
    uint_t msanr = (uint_t)mbsp->mb_memmapnr, maxp = 0;
    //获取memarea_t结构的首地址
    memarea_t *marea = (memarea_t *)phyadr_to_viradr((adr_t)mbsp->mb_memznpadr);
    uint_t sretf = ~0UL, tretf = ~0UL;
    //遍历每个memarea_t结构
    for (uint_t mi = 0; mi < (uint_t)mbsp->mb_memznnr; mi++)
    {
        //针对其中一个memarea_t结构给msadsc_t结构打上标签
        sretf = merlove_setallmarflgs_onmemarea(&marea[mi], mstatp, msanr);
        if ((~0UL) == sretf)
        {
            return FALSE;
        }
    }
     //遍历每个memarea_t结构
    for (uint_t maidx = 0; maidx < (uint_t)mbsp->mb_memznnr; maidx++)
    {
        //针对其中一个memarea_t结构对msadsc_t结构进行合并
        if (merlove_mem_onmemarea(&marea[maidx], mstatp, msanr) == FALSE)
        {
            return FALSE;
        }
        maxp += marea[maidx].ma_maxpages;
    }
    return TRUE;
}

//初始化页面合并
void init_merlove_mem()
{
    if (merlove_mem_core(&kmachbsp) == FALSE)
    {
        system_error("merlove_mem_core fail\n");
    }
    return;
}
```
merlove_mem_core 函数才是真正干活的。这个 merlove_mem_core 函数有两个遍历内存区，第一次遍历是为了完成上述第一步：确定内存页属于哪个区。  
当确定内存页属于哪个区之后，就来到了第二次遍历 memarea_t 结构，合并其中的 msadsc_t 结构，并把它们挂载到其中的 memdivmer_t 结构下的 dm_mdmlielst 数组中。  
这个操作就稍微有点复杂了。  
1. 它要保证其中所有的 msadsc_t 结构挂载到 dm_mdmlielst 数组中合适的 bafhlst_t 结构中。
2. 它要保证多个 msadsc_t 结构有最大的连续性。  
```
举个例子，比如一个内存区中有 12 个页面，其中 10 个页面是连续的地址为 0～0x9000，还有两个页面其中一个地址为 0xb000，另一个地址为 0xe000。
这样需要多个页面保持最大的连续性，还有在 m_mdmlielst 数组中找到合适的 bafhlst_t 结构。
0～0x7000 这 8 个页面就要挂载到 m_mdmlielst 数组中第 3 个 bafhlst_t 结构中；0x8000～0x9000 这 2 个页面要挂载到 m_mdmlielst 数组中第 1 个 bafhlst_t 结构中，而 0xb000 和 0xe000 这 2 个页面都要挂载到 m_mdmlielst 数组中第 0 个 bafhlst_t 结构中。
```
遍历每个内存区，然后针对其中每一个内存区进行 msadsc_t 结构的合并操作，完成这个操作的是 merlove_mem_onmemarea
```c
bool_t continumsadsc_add_bafhlst(memarea_t *mareap, bafhlst_t *bafhp, msadsc_t *fstat, msadsc_t *fend, uint_t fmnr)
{
    fstat->md_indxflgs.mf_olkty = MF_OLKTY_ODER;
    //开始的msadsc_t结构指向最后的msadsc_t结构 
    fstat->md_odlink = fend;
    fend->md_indxflgs.mf_olkty = MF_OLKTY_BAFH;
    //最后的msadsc_t结构指向它属于的bafhlst_t结构
    fend->md_odlink = bafhp;
    //把多个地址连续的msadsc_t结构的开始的那个msadsc_t结构挂载到bafhlst_t结构的af_frelst中
    list_add(&fstat->md_list, &bafhp->af_frelst);
    //更新bafhlst_t的统计数据
    bafhp->af_fobjnr++;
    bafhp->af_mobjnr++;
    //更新内存区的统计数据
    mareap->ma_maxpages += fmnr;
    mareap->ma_freepages += fmnr;
    mareap->ma_allmsadscnr += fmnr;
    return TRUE;
}

bool_t continumsadsc_mareabafh_core(memarea_t *mareap, msadsc_t **rfstat, msadsc_t **rfend, uint_t *rfmnr)
{
    uint_t retval = *rfmnr, tmpmnr = 0;
    msadsc_t *mstat = *rfstat, *mend = *rfend;
    //根据地址连续的msadsc_t结构的数量查找合适bafhlst_t结构
    bafhlst_t *bafhp = find_continumsa_inbafhlst(mareap, retval);
    //判断bafhlst_t结构状态和类型对不对
    if ((BAFH_STUS_DIVP == bafhp->af_stus || BAFH_STUS_DIVM == bafhp->af_stus) && MA_TYPE_PROC != mareap->ma_type)
    {
        //看地址连续的msadsc_t结构的数量是不是正好是bafhp->af_oderpnr
        tmpmnr = retval - bafhp->af_oderpnr;
        //根据地址连续的msadsc_t结构挂载到bafhlst_t结构中
        if (continumsadsc_add_bafhlst(mareap, bafhp, mstat, &mstat[bafhp->af_oderpnr - 1], bafhp->af_oderpnr) == FALSE)
        {
            return FALSE;
        }
        //如果地址连续的msadsc_t结构的数量正好是bafhp->af_oderpnr则完成，否则返回再次进入此函数 
        if (tmpmnr == 0)
        {
            *rfmnr = tmpmnr;
            *rfend = NULL;
            return TRUE;
        }
        //挂载bafhp->af_oderpnr地址连续的msadsc_t结构到bafhlst_t中
        *rfstat = &mstat[bafhp->af_oderpnr];
        //还剩多少个地址连续的msadsc_t结构
        *rfmnr = tmpmnr;
        return TRUE;
    }
    return FALSE;
}

bool_t merlove_continumsadsc_mareabafh(memarea_t *mareap, msadsc_t *mstat, msadsc_t *mend, uint_t mnr)
{
    uint_t mnridx = mnr;
    msadsc_t *fstat = mstat, *fend = mend;
    //如果mnridx > 0并且NULL != fend就循环调用continumsadsc_mareabafh_core函数，而mnridx和fend由这个函数控制
    for (; (mnridx > 0 && NULL != fend);)
    {
    //为一段地址连续的msadsc_t结构寻找合适m_mdmlielst数组中的bafhlst_t结构
        continumsadsc_mareabafh_core(mareap, &fstat, &fend, &mnridx)
    }
    return TRUE;
}

bool_t merlove_scan_continumsadsc(memarea_t *mareap, msadsc_t *fmstat, uint_t *fntmsanr, uint_t fmsanr,
                                         msadsc_t **retmsastatp, msadsc_t **retmsaendp, uint_t *retfmnr)
{
    u32_t muindx = 0;
    msadflgs_t *mdfp = NULL;

    msadsc_t *msastat = fmstat;
    uint_t retfindmnr = 0;
    bool_t rets = FALSE;
    uint_t tmidx = *fntmsanr;
    //从外层函数的fntmnr变量开始遍历所有msadsc_t结构
    for (; tmidx < fmsanr; tmidx++)
    {
    //一个msadsc_t结构是否属于这个内存区，是否空闲
        if (msastat[tmidx].md_indxflgs.mf_marty == mdfp->mf_marty &&
            0 == msastat[tmidx].md_indxflgs.mf_uindx &&
            MF_MOCTY_FREE == msastat[tmidx].md_indxflgs.mf_mocty &&
            PAF_NO_ALLOC == msastat[tmidx].md_phyadrs.paf_alloc)
        {
        //返回从这个msadsc_t结构开始到下一个非空闲、地址非连续的msadsc_t结构对应的msadsc_t结构索引号到retfindmnr变量中
            rets = scan_len_msadsc(&msastat[tmidx], mdfp, fmsanr, &retfindmnr);
            //下一轮开始的msadsc_t结构索引
            *fntmsanr = tmidx + retfindmnr + 1;
            //当前地址连续msadsc_t结构的开始地址
            *retmsastatp = &msastat[tmidx];
            //当前地址连续msadsc_t结构的结束地址
            *retmsaendp = &msastat[tmidx + retfindmnr];
            //当前有多少个地址连续msadsc_t结构
            *retfmnr = retfindmnr + 1;
            return TRUE;
        }
    }
    return FALSE;
}

bool_t merlove_mem_onmemarea(memarea_t *mareap, msadsc_t *mstat, uint_t msanr)
{
    msadsc_t *retstatmsap = NULL, *retendmsap = NULL, *fntmsap = mstat;
    uint_t retfindmnr = 0;
    uint_t fntmnr = 0;
    bool_t retscan = FALSE;
    
    for (; fntmnr < msanr;)
    {
        //获取最多且地址连续的msadsc_t结构体的开始、结束地址、一共多少个msadsc_t结构体，下一次循环的fntmnr
        retscan = merlove_scan_continumsadsc(mareap, fntmsap, &fntmnr, msanr, &retstatmsap, &retendmsap, &retfindmnr);
        if (NULL != retstatmsap && NULL != retendmsap)
        {
        //把一组连续的msadsc_t结构体挂载到合适的m_mdmlielst数组中的bafhlst_t结构中
        merlove_continumsadsc_mareabafh(mareap, retstatmsap, retendmsap, retfindmnr)
        }
    }
    return TRUE;
}
```
此代码第一步，通过 merlove_scan_continumsadsc 函数，返回最多且地址连续的 msadsc_t 结构体的开始、结束地址、一共多少个 msadsc_t 结构体，下一轮开始的 msadsc_t 结构体的索引号。  
第二步，根据第一步获取的信息调用 merlove_continumsadsc_mareabafh 函数，把第一步返回那一组连续的 msadsc_t 结构体，挂载到合适的 m_mdmlielst 数组中的 bafhlst_t 结构中。  

# 初始化汇总
到目前为止，我们一起写了这么多的内存初始化相关的代码，但是我们没有调用它们。它们的调用次序很重要，谁先谁后都有严格的规定，这关乎内存管理初始化的成败  
```c
void init_memmgr()
{
    //初始化内存页结构
    init_msadsc();
    //初始化内存区结构
    init_memarea();
  //init_msadsc、init_memarea 函数是可以交换次序的，它们俩互不影响，但它们俩必须最先开始调用，而后面的函数要依赖它们生成的数据结构
    //处理内存占用
    init_search_krloccupymm(&kmachbsp);
    //合并内存页到内存区中
    init_merlove_mem();
    init_memmgrob();  //搁置
    return;
}
```
phymmarge_t 结构体的地址和数量、msadsc_t 结构体的地址和数据、memarea_t 结构体的地址和数量都保存在了 kmachbsp 变量中，这个变量其实不是用来管理内存的，而且它里面放的是物理地址。但内核使用的是虚拟地址，每次都要转换极不方便，所以我们要设计一个专用的数据结构，用于内存管理。  
```c
HuOS/include/halinc/halglobal.c

HAL_DEFGLOB_VARIABLE(memmgrob_t,memmgrob);

typedef struct s_MEMMGROB
{
    list_h_t mo_list;
    spinlock_t mo_lock;        //保护自身自旋锁
    uint_t mo_stus;            //状态
    uint_t mo_flgs;            //标志
    u64_t mo_memsz;            //内存大小
    u64_t mo_maxpages;         //内存最大页面数
    u64_t mo_freepages;        //内存最大空闲页面数
    u64_t mo_alocpages;        //内存最大分配页面数
    u64_t mo_resvpages;        //内存保留页面数
    u64_t mo_horizline;        //内存分配水位线
    phymmarge_t* mo_pmagestat; //内存空间布局结构指针
    u64_t mo_pmagenr;
    msadsc_t* mo_msadscstat;   //内存页面结构指针
    u64_t mo_msanr;
    memarea_t* mo_mareastat;   //内存区结构指针 
    u64_t mo_mareanr;
}memmgrob_t;

//HuOS/hal/x86/memmgrinit.c
void memmgrob_t_init(memmgrob_t *initp)
{
    list_init(&initp->mo_list);
    knl_spinlock_init(&initp->mo_lock);
    initp->mo_stus = 0;
    initp->mo_flgs = 0;
    initp->mo_memsz = 0;
    initp->mo_maxpages = 0;
    initp->mo_freepages = 0;
    initp->mo_alocpages = 0;
    initp->mo_resvpages = 0;
    initp->mo_horizline = 0;
    initp->mo_pmagestat = NULL;
    initp->mo_pmagenr = 0;
    initp->mo_msadscstat = NULL;
    initp->mo_msanr = 0;
    initp->mo_mareastat = NULL;
    initp->mo_mareanr = 0;
    return;
}

void init_memmgrob()
{
    machbstart_t *mbsp = &kmachbsp;
    memmgrob_t *mobp = &memmgrob;
    memmgrob_t_init(mobp);
    mobp->mo_pmagestat = (phymmarge_t *)phyadr_to_viradr((adr_t)mbsp->mb_e820expadr);
    mobp->mo_pmagenr = mbsp->mb_e820exnr;
    mobp->mo_msadscstat = (msadsc_t *)phyadr_to_viradr((adr_t)mbsp->mb_memmappadr);
    mobp->mo_msanr = mbsp->mb_memmapnr;
    mobp->mo_mareastat = (memarea_t *)phyadr_to_viradr((adr_t)mbsp->mb_memznpadr);
    mobp->mo_mareanr = mbsp->mb_memznnr;
    mobp->mo_memsz = mbsp->mb_memmapnr << PSHRSIZE;
    mobp->mo_maxpages = mbsp->mb_memmapnr;
    uint_t aidx = 0;
    for (uint_t i = 0; i < mobp->mo_msanr; i++)
    {
        if (1 == mobp->mo_msadscstat[i].md_indxflgs.mf_uindx &&
            MF_MOCTY_KRNL == mobp->mo_msadscstat[i].md_indxflgs.mf_mocty &&
            PAF_ALLOC == mobp->mo_msadscstat[i].md_phyadrs.paf_alloc)
        {
            aidx++;
        }
    }
    mobp->mo_alocpages = aidx;
    mobp->mo_freepages = mobp->mo_maxpages - mobp->mo_alocpages;
    return;
}
```