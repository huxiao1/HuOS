<!-- toc -->
如何在内核中注册设备
- [设备的注册流程](#设备的注册流程)
- [驱动程序表](#驱动程序表)
- [运行驱动程序](#运行驱动程序)
    - [调用驱动程序入口函数](#调用驱动程序入口函数)
    - [驱动程序入口函数例子](#驱动程序入口函数例子)
    - [设备联系驱动（实现上面伪代码接口函数）](#设备联系驱动实现上面伪代码接口函数)
    - [安装中断回调函数](#安装中断回调函数)
    - [驱动加入内核](#驱动加入内核)

<!-- tocstop -->

# 设备的注册流程
我们先从全局了解一下设备的注册流程，然后了解怎么加载驱动，最后探索怎么让驱动建立一个设备，并在内核中注册。  
例如：  
1. 操作系统会收到一个中断。
2. USB 总线驱动的中断处理程序会执行。
3. 调用操作系统内核相关的服务，查找 USB 鼠标对应的驱动程序。
4. 操作系统加载驱动程序。
5. 驱动程序开始执行，向操作系统内核注册一个鼠标设备。这就是一般操作系统加载驱动的粗略过程。对于安装在主板上的设备，操作系统会枚举设备信息，然后加载驱动程序，让驱动程序创建并注册相应的设备。当然，你还可以手动加载驱动程序。

为了简单起见，我们的 os 不会这样复杂，暂时也不支持设备热拨插功能。我们让 os 自动加载驱动，在驱动中向 os 注册相应的设备。  
![1](./images/1.png)  

# 驱动程序表
把驱动程序和内核链接到一起，省略了加载驱动程序的过程，因为加载程序不仅仅是把驱动程序放在内存中就可以了，还要进行程序链接相关的操作，这个操作极其复杂。  
驱动程序表让内核知道驱动程序的存在  
```c
//HuOS/kernel/krlglobal.c
KRL_DEFGLOB_VARIABLE(drventyexit_t,osdrvetytabl)[]={NULL};
```
drventyexit_t是函数指针类型，这个函数指针数组中放的就是驱动程序的入口函数，而内核只需要扫描这个函数指针数组，就可以调用到每个驱动程序了。  
有了这个函数指针数组，接着我们还需要写好这个驱动程序的初始化函数  
```c
void init_krldriver()
{
    //遍历驱动程序表中的每个驱动程序入口函数
    for (uint_t ei = 0; osdrvetytabl[ei] != NULL; ei++)
    {
        //运行一个驱动程序入口
        if (krlrun_driverentry(osdrvetytabl[ei]) == DFCERRSTUS)
        {
            hal_sysdie("init driver err");
        }
    }
    return;
}

void init_krl()
{
    init_krlmm();
    init_krldevice();
    init_krldriver();
    //……
    return;
}
//一定要先初始化设备表，然后才能初始化驱动程序，否则在驱动程序中建立的设备和驱动就无处安放了
```

# 运行驱动程序
## 调用驱动程序入口函数
直接调用驱动程序入口函数是不行的，要先给它准备一个重要的参数，也就是驱动描述符指针  
```c
drvstus_t krlrun_driverentry(drventyexit_t drventry)
{
    driver_t *drvp = new_driver_dsc();//建立driver_t实例变量
    if (drvp == NULL)
    {
        return DFCERRSTUS;
    }
    if (drventry(drvp, 0, NULL) == DFCERRSTUS)//运行驱动程序入口函数
    {
        return DFCERRSTUS;
    }
    if (krldriver_add_system(drvp) == DFCERRSTUS)//把驱动程序加入系统
    {
        return DFCERRSTUS;
    }
    return DFCOKSTUS;
}
//drvp 就是一个驱动描述符指针
//DFCOKSTUS 是个宏，表示成功的状态
```
## 驱动程序入口函数例子
首先要建立建立一个设备描述符，接着把驱动程序的功能函数设置到 driver_t 结构中的 drv_dipfun 数组中，并将设备挂载到驱动上，然后要向内核注册设备，最后驱动程序初始化自己的物理设备，安装中断回调函数。  
```c
drvstus_t systick_entry(driver_t* drvp,uint_t val,void* p)
{
    if(drvp==NULL) //drvp是内核传递进来的参数，不能为NULL
    {
        return DFCERRSTUS;
    }
    device_t* devp=new_device_dsc();//建立设备描述符结构的变量实例
    if(devp==NULL)//不能失败
    {
        return DFCERRSTUS;
    }
    systick_set_device(devp,drvp);//驱动程序的功能函数设置到driver_t结构中的drv_dipfun数组中
    if(krldev_add_driver(devp,drvp)==DFCERRSTUS)//将设备挂载到驱动中
    {
        if(del_device_dsc(devp)==DFCERRSTUS)//注意释放资源
        {
            return DFCERRSTUS;
        }
        return DFCERRSTUS;
    }
    if(krlnew_device(devp)==DFCERRSTUS)//向内核注册设备
    {
        if(del_device_dsc(devp)==DFCERRSTUS)//注意释放资源
        {
            return DFCERRSTUS;
        }
        return DFCERRSTUS;
    }
    //安装中断回调函数systick_handle
    if(krlnew_devhandle(devp,systick_handle,20)==DFCERRSTUS)
    {
        return DFCERRSTUS;  //注意释放资源
    }
    init_8254();//初始化物理设备
    if(krlenable_intline(20)==DFCERRSTUS)
    {
        return DFCERRSTUS;
    }
    return DFCOKSTUS;
}
```
是一个驱动程序框架，这个例子告诉我们，操作系统内核要为驱动程序开发者提供哪些功能接口函数，这在很多通用操作系统上叫作驱动模型。  
## 设备联系驱动（实现上面伪代码接口函数）
1. 将设备挂载到驱动上，让设备和驱动产生联系，确保驱动能找到设备，设备能找到驱动
```c
drvstus_t krldev_add_driver(device_t *devp, driver_t *drvp)
{
    list_h_t *lst;
    device_t *fdevp;
    //遍历这个驱动上所有设备
    list_for_each(lst, &drvp->drv_alldevlist)
    {
        fdevp = list_entry(lst, device_t, dev_indrvlst);
        //比较设备ID有相同的则返回错误
      //遍历驱动设备链表中的所有设备，看看有没有设备 ID 冲突
        if (krlcmp_devid(&devp->dev_id, &fdevp->dev_id) == TRUE)
        {
            return DFCERRSTUS;
        }
    }
    //将设备挂载到驱动上
    list_add(&devp->dev_indrvlst, &drvp->drv_alldevlist);
    devp->dev_drv = drvp;//让设备中dev_drv字段指向管理自己的驱动
    return DFCOKSTUS;
}
```
2. 向内核注册设备
注册过程应该由内核来实现并提供接口，在这个注册设备的过程中，内核会通过设备的类型和 ID，把用来表示设备的 device_t 结构挂载到设备表中  
```c
drvstus_t krlnew_device(device_t *devp)
{
    device_t *findevp;
    drvstus_t rets = DFCERRSTUS;
    cpuflg_t cpufg;
    list_h_t *lstp;
    devtable_t *dtbp = &osdevtable;
    uint_t devmty = devp->dev_id.dev_mtype;
    if (devp->dev_drv == NULL)//没有驱动的设备不行
    {
        return DFCERRSTUS;
    }
    krlspinlock_cli(&dtbp->devt_lock, &cpufg);//加锁
    //遍历设备类型链表上的所有设备
    list_for_each(lstp, &dtbp->devt_devclsl[devmty].dtl_list)
    {
        findevp = list_entry(lstp, device_t, dev_intbllst);
        //不能有设备ID相同的设备，如果有则出错
        if (krlcmp_devid(&devp->dev_id, &findevp->dev_id) == TRUE)
        {
            rets = DFCERRSTUS;
            goto return_step;
        }
    }
    //先把设备加入设备表的全局设备链表
    list_add(&devp->dev_intbllst, &dtbp->devt_devclsl[devmty].dtl_list);
    //将设备加入对应设备类型的链表中
    list_add(&devp->dev_list, &dtbp->devt_devlist);
    dtbp->devt_devclsl[devmty].dtl_nr++;//设备计数加一
    dtbp->devt_devnr++;//总的设备数加一
    rets = DFCOKSTUS;
return_step:
    krlspinunlock_sti(&dtbp->devt_lock, &cpufg);//解锁
    return rets;
}
```
设备表中有没有设备 ID 冲突，如果没有的话就加入设备类型链表和全局设备链表中，最后对其计数器变量加一。  

### 安装中断回调函数
设备要和 CPU 进行通信，都是通过中断的形式进行。收到中断信号后，CPU 就会开始处理中断，转而调用中断处理框架函数，最后会调用设备驱动程序提供的中断回调函数，对该设备发出的中断进行具体处理。  
内核就要提供相应的接口用于安装中断回调函数，使得驱动程序开发者专注于设备本身，不用分心去了解内核的中断框架  
```c
//中断回调函数类型
typedef drvstus_t (*intflthandle_t)(uint_t ift_nr,void* device,void* sframe);
//安装中断回调函数接口
drvstus_t krlnew_devhandle(device_t *devp, intflthandle_t handle, uint_t phyiline)
{
    //调用内核层中断框架接口函数
    intserdsc_t *sdp = krladd_irqhandle(devp, handle, phyiline);
    if (sdp == NULL)
    {
        return DFCERRSTUS;
    }
    cpuflg_t cpufg;
    krlspinlock_cli(&devp->dev_lock, &cpufg);
    //将中断服务描述符结构挂入这个设备结构中
    list_add(&sdp->s_indevlst, &devp->dev_intserlst);
    devp->dev_intlnenr++;
    krlspinunlock_sti(&devp->dev_lock, &cpufg);
    return DFCOKSTUS;
}
//krlnew_devhandle 函数有三个参数，分别是安装中断回调函数的设备，驱动程序提供的中断回调函数，还有一个是设备在中断控制器中断线的号码。
```
krladd_irqhandle函数不应该在 krldevice.c 文件中写，而是要在 HuOS/kernel/ 目录下建立一个 krlintupt.c 文件，在这个文件模块中写。  
它的主要工作是创建了一个 intserdsc_t 结构，用来保存设备和其驱动程序提供的中断回调函数。同时，通过 intserdsc_t 结构也让中断处理框架和设备驱动联系起来了。  
```c
typedef struct s_INTSERDSC{
    list_h_t    s_list;        //在中断异常描述符中的链表
    list_h_t    s_indevlst;    //在设备描述描述符中的链表
    u32_t       s_flg;         //标志
    intfltdsc_t* s_intfltp;    //指向中断异常描述符
    void*       s_device;      //指向设备描述符
    uint_t      s_indx;        //中断回调函数运行计数
    intflthandle_t s_handle;   //中断处理的回调函数指针
}intserdsc_t;

intserdsc_t *krladd_irqhandle(void *device, intflthandle_t handle, uint_t phyiline)
{
    //根据设备中断线返回对应中断异常描述符
    intfltdsc_t *intp = hal_retn_intfltdsc(phyiline);
    if (intp == NULL)
    {
        return NULL;
    }
    intserdsc_t *serdscp = (intserdsc_t *)krlnew(sizeof(intserdsc_t));//建立一个intserdsc_t结构体实例变量
    if (serdscp == NULL)
    {
        return NULL;
    }
    //初始化intserdsc_t结构体实例变量，并把设备指针和回调函数放入其中
    intserdsc_t_init(serdscp, 0, intp, device, handle);
    //把intserdsc_t结构体实例变量挂载到中断异常描述符结构中
    if (hal_add_ihandle(intp, serdscp) == FALSE)
    {
        if (krldelete((adr_t)serdscp, sizeof(intserdsc_t)) == FALSE)
        {
            hal_sysdie("krladd_irqhandle ERR");
        }
        return NULL;
    }
    return serdscp;
}
```
中断处理框架既能找到对应的 intserdsc_t 结构，又能从 intserdsc_t 结构中得到中断回调函数和对应的设备描述符，从而调用中断回调函数，进行具体设备的中断处理。  
### 驱动加入内核
接下来，操作系统内核会根据返回状态，决定是否将该驱动程序加入到操作系统内核中  
所谓将驱动程序加入到操作系统内核，无非就是将 driver_t 结构的实例变量挂载到设备表中。  
```c
drvstus_t krldriver_add_system(driver_t *drvp)
{
    cpuflg_t cpufg;
    devtable_t *dtbp = &osdevtable;//设备表
    krlspinlock_cli(&dtbp->devt_lock, &cpufg);//加锁
    list_add(&drvp->drv_list, &dtbp->devt_drvlist);//挂载
    dtbp->devt_drvnr++;//增加驱动程序计数
    krlspinunlock_sti(&dtbp->devt_lock, &cpufg);//解锁
    return DFCOKSTUS;
}
```
驱动程序不需要分门别类，所以我们只把它挂载到设备表中一个全局驱动程序链表上就行了，最后简单地增加一下驱动程序计数变量，用来表明有多少个驱动程序。  
**一个驱动程序开始是由内核加载运行，然后调用由内核提供的接口建立设备，最后向内核注册设备和驱动，完成驱动和内核的握手动作。**  