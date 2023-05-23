<!-- toc -->
如何处理内核IO包
- [如何处理内核IO包](#如何处理内核IO包)
- [创建和删除IO包](#创建和删除IO包)
- [向设备发送IO包](#向设备发送IO包)
- [驱动程序实例](#驱动程序实例)
    - [systick 设备驱动程序的整体框架](#systick-设备驱动程序的整体框架)
    - [systick 设备驱动程序的入口](#systick-设备驱动程序的入口)
        - [配置设备和驱动](#配置设备和驱动)
    - [打开与关闭设备函数](#打开与关闭设备函数)
    - [systick 设备中断回调函数](#systick-设备中断回调函数)
<!-- tocstop -->

# 如何处理内核IO包
内核要求设备完成任务，无非是调用设备的驱动程序函数，把完成任务的细节用参数的形式传递给设备的驱动程序。  
由于参数很多，而且各种操作所需的参数又不相同，所以我们就想到了更高效的管理方法，也就是把各种操作所需的各种参数封装在一个数据结构中，称为 I/O 包。  
```c
typedef struct s_OBJNODE
{
    spinlock_t  on_lock;        //自旋锁
    list_h_t    on_list;        //链表
    sem_t       on_complesem;   //完成信号量
    uint_t      on_flgs;        //标志
    uint_t      on_stus;        //状态
    sint_t      on_opercode;    //操作码
    uint_t      on_objtype;     //对象类型
    void*       on_objadr;      //对象地址
    uint_t      on_acsflgs;     //访问设备、文件标志
    uint_t      on_acsstus;     //访问设备、文件状态
    uint_t      on_currops;     //对应于读写数据的当前位置
    uint_t      on_len;         //对应于读写数据的长度
    uint_t      on_ioctrd;      //IO控制码
    buf_t       on_buf;         //对应于读写数据的缓冲区
    uint_t      on_bufcurops;   //对应于读写数据的缓冲区的当前位置
    size_t      on_bufsz;       //对应于读写数据的缓冲区的大小
    uint_t      on_count;       //对应于对象节点的计数
    void*       on_safedsc;     //对应于对象节点的安全描述符
    void*       on_fname;       //对应于访问数据文件的名称
    void*       on_finode;      //对应于访问数据文件的结点
    void*       on_extp;        //用于扩展
}objnode_t;
```
objnode_t 的数据结构中包括了各个驱动程序功能函数的所有参数

# 创建和删除IO包
在 HuOS/kernel/ 目录下建立一个新的 krlobjnode.c 文件
```c
//建立objnode_t结构
objnode_t *krlnew_objnode()
{
    objnode_t *ondp = (objnode_t *)krlnew((size_t)sizeof(objnode_t));//分配objnode_t结构的内存空间
    if (ondp == NULL)
    {
        return NULL;
    }
    objnode_t_init(ondp);//初始化objnode_t结构
    return ondp;
}

//删除objnode_t结构
bool_t krldel_objnode(objnode_t *onodep)
{
    if (krldelete((adr_t)onodep, (size_t)sizeof(objnode_t)) == FALSE)//删除objnode_t结构的内存空间
    {
        hal_sysdie("krldel_objnode err");
        return FALSE;
    }
    return TRUE;
}
```
objnode_t_init 函数会初始化 objnode_t 结构中的字段，因为其中有自旋锁、链表、信号量，而这些结构并不能简单地初始为 0，否则可以直接使用 memset 之类的函数把那个内存空间清零就行了。  

# 向设备发送IO包
下一步，就需要把这个 I/O 发送给具体设备的驱动程序。  
实现一个函数，专门用于完成这个功能，它标志着一个设备驱动程序开始运行，经它之后内核就实际的控制权交给驱动程序，由驱动程序代表内核操控设备。  
这个函数属于驱动模型函数，所以要在 krldevice.c 文件中实现这个函数  
```c
//发送设备IO
drvstus_t krldev_io(objnode_t *nodep)
{
    //获取设备对象
    device_t *devp = (device_t *)(nodep->on_objadr);
    if ((nodep->on_objtype != OBJN_TY_DEV && nodep->on_objtype != OBJN_TY_FIL) || nodep->on_objadr == NULL)
    {
        //检查操作对象类型是不是文件或者设备，对象地址是不是为空
        return DFCERRSTUS;
    }
    if (nodep->on_opercode < 0 || nodep->on_opercode >= IOIF_CODE_MAX)
    {
        //检查IO操作码是不是合乎要求
        return DFCERRSTUS;
    }
    return krldev_call_driver(devp, nodep->on_opercode, 0, 0, NULL, nodep);//调用设备驱动
}

//调用设备驱动
drvstus_t krldev_call_driver(device_t *devp, uint_t iocode, uint_t val1, uint_t val2, void *p1, void *p2)
{
    driver_t *drvp = NULL;
    if (devp == NULL || iocode >= IOIF_CODE_MAX)
    {
        //检查设备和IO操作码
        return DFCERRSTUS;
    }
    drvp = devp->dev_drv;
    if (drvp == NULL)//检查设备是否有驱动程序
    {
        return DFCERRSTUS;
    }
    //用IO操作码为索引调用驱动程序功能分派函数数组中的函数
    return drvp->drv_dipfun[iocode](devp, p2);
}
```
krldev_io 函数，只接受一个参数，也就是 objnode_t 结构的指针。它会首先检查 objnode_t 结构中的 IO 操作码是不是合乎要求的，还要检查被操作的对象即设备是不是为空，然后调用 krldev_call_driver 函数。  
krldev_call_driver 函数会再次确认传递进来的设备和 IO 操作码，然后重点检查设备有没有驱动程序。这一切检查通过之后，我们就用 IO 操作码为索引调用驱动程序功能分派函数数组中的函数，并把设备和 objnode_t 结构传递进去。  
现在一个设备的驱动程序就能正式开始工作，开始响应处理内核发来的 I/O 包了。可是我们还没有驱动呢，所以下面我们就去实现一个驱动程序。  

# 驱动程序实例
systick 设备的主要功能和作用是每隔 1ms 产生一个中断，相当于一个定时器，每次时间到达就产生一个中断向系统报告又过了 1ms，相当于千分之一秒，即每秒钟内产生 1000 次中断。  
以 8254 定时器为基础，实现 HuOS 系统的 systick 设备。我们先从 systick 设备驱动程序的整体框架入手，然后建立 systick 设备，最后一步一步实现 systick 设备驱动程序。  
## systick 设备驱动程序的整体框架
在 HuOS/drivers/ 目录下建立一个 drvtick.c 文件
```c
//驱动程序入口和退出函数
drvstus_t systick_entry(driver_t *drvp, uint_t val, void *p)
{
    return DFCERRSTUS;
}
drvstus_t systick_exit(driver_t *drvp, uint_t val, void *p)
{
    return DFCERRSTUS;
}

//设备中断处理函数
drvstus_t systick_handle(uint_t ift_nr, void *devp, void *sframe)
{
    return DFCEERSTUS;
}

//打开、关闭设备函数
drvstus_t systick_open(device_t *devp, void *iopack)
{
    return DFCERRSTUS;
}
drvstus_t systick_close(device_t *devp, void *iopack)
{
    return DFCERRSTUS;
}

//读、写设备数据函数
drvstus_t systick_read(device_t *devp, void *iopack)
{
    return DFCERRSTUS;
}
drvstus_t systick_write(device_t *devp, void *iopack)
{
    return DFCERRSTUS;
}

//调整读写设备数据位置函数
drvstus_t systick_lseek(device_t *devp, void *iopack)
{
    return DFCERRSTUS;
}

//控制设备函数
drvstus_t systick_ioctrl(device_t *devp, void *iopack)
{
    return DFCERRSTUS;
}

//开启、停止设备函数
drvstus_t systick_dev_start(device_t *devp, void *iopack)
{
    return DFCERRSTUS;
}
drvstus_t systick_dev_stop(device_t *devp, void *iopack)
{
    return DFCERRSTUS;
}

//设置设备电源函数
drvstus_t systick_set_powerstus(device_t *devp, void *iopack)
{
    return DFCERRSTUS;
}

//枚举设备函数
drvstus_t systick_enum_dev(device_t *devp, void *iopack)
{
    return DFCERRSTUS;
}

//刷新设备缓存函数
drvstus_t systick_flush(device_t *devp, void *iopack)
{
    return DFCERRSTUS;
}

//设备关机函数
drvstus_t systick_shutdown(device_t *devp, void *iopack)
{
    return DFCERRSTUS;
}
```
在各个函数可以返回一个错误状态，而不做任何实际工作，但是必须要有这个函数。这样在内核发来任何设备功能请求时，驱动程序才能给予适当的响应。这样，一个驱动程序的整体框架就确定了。  
## systick 设备驱动程序的入口
建立设备，向内核注册设备，安装中断回调函数等操作  
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
    systick_set_driver(drvp);
    systick_set_device(devp,drvp);//驱动程序的功能函数设置到driver_t结构中的drv_dipfun数组中
    if(krldev_add_driver(devp,drvp)==DFCERRSTUS)//将设备挂载到驱动中
    {
        if(del_device_dsc(devp)==DFCERRSTUS)//注意释放资源。
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
        return DFCERRSTUS;  //注意释放资源。
    }
    init_8254();//初始化物理设备
    if(krlenable_intline(0x20)==DFCERRSTUS)
    {
        return DFCERRSTUS;
    }
    return DFCOKSTUS;
}
```
krlenable_intline 函数，它的主要功能是开启一个中断源上的中断。而 init_8254 函数则是为了初始化 8254，它就是一个古老且常用的定时器。  
有了驱动程序入口函数，驱动程序并不会自动运行。根据前面我们的设计，需要把这个驱动程序入口函数放入驱动表中。  
```c
//HuOS/kernel/krlglobal.c
KRL_DEFGLOB_VARIABLE(drventyexit_t,osdrvetytabl)[]={systick_entry,NULL};
```
drventyexit_t是函数指针类型，这个函数指针数组中放的就是驱动程序的入口函数，而内核只需要扫描这个函数指针数组，就可以调用到每个驱动程序了。  
HuOS 在启动的时候，就会执行初始驱动初始化 init_krldriver 函数，接着这个函数就会启动运行 systick 设备驱动程序入口函数。我们的 systick_entry 函数一旦执行，就会建立 systick 设备，不断的产生时钟中断。  
### 配置设备和驱动
在驱动程序入口函数中设置一些标志、状态、名称、驱动功能派发函数等等。有了这些信息，设备才能加入到驱动程序中，然后注册到内核，这样才能被内核所识别。  
```c
void systick_set_driver(driver_t *drvp)
{
    //设置驱动程序功能派发函数
    drvp->drv_dipfun[IOIF_CODE_OPEN] = systick_open;
    drvp->drv_dipfun[IOIF_CODE_CLOSE] = systick_close;
    drvp->drv_dipfun[IOIF_CODE_READ] = systick_read;
    drvp->drv_dipfun[IOIF_CODE_WRITE] = systick_write;
    drvp->drv_dipfun[IOIF_CODE_LSEEK] = systick_lseek;
    drvp->drv_dipfun[IOIF_CODE_IOCTRL] = systick_ioctrl;
    drvp->drv_dipfun[IOIF_CODE_DEV_START] = systick_dev_start;
    drvp->drv_dipfun[IOIF_CODE_DEV_STOP] = systick_dev_stop;
    drvp->drv_dipfun[IOIF_CODE_SET_POWERSTUS] = systick_set_powerstus;
    drvp->drv_dipfun[IOIF_CODE_ENUM_DEV] = systick_enum_dev;
    drvp->drv_dipfun[IOIF_CODE_FLUSH] = systick_flush;
    drvp->drv_dipfun[IOIF_CODE_SHUTDOWN] = systick_shutdown;
    drvp->drv_name = "systick0drv";//设置驱动程序名称
    return;
}
//驱动功能函数在该数组中的元素位置，正好与 IO 操作码一一对应，当内核用 IO 操作码调用驱动时，就是调用了这个数据中的函数
```
新建的设备也需要配置相关的信息才能工作，比如需要指定设备，设备状态与标志，设备类型、设备名称这些信息。  
```c
//设备类型非常重要，内核正是通过类型来区分各种设备的
void systick_set_device(device_t *devp, driver_t *drvp)
{
    devp->dev_flgs = DEVFLG_SHARE;//设备可共享访问
    devp->dev_stus = DEVSTS_NORML;//设备正常状态
    devp->dev_id.dev_mtype = SYSTICK_DEVICE;//设备主类型
    devp->dev_id.dev_stype = 0;//设备子类型
    devp->dev_id.dev_nr = 0; //设备号
    devp->dev_name = "systick0";//设置设备名称
    return;
}
```
## 打开与关闭设备函数
```c
//打开设备
drvstus_t systick_open(device_t *devp, void *iopack)
{
    krldev_inc_devcount(devp);//增加设备计数
    return DFCOKSTUS;//返回成功完成的状态
}
//关闭设备
drvstus_t systick_close(device_t *devp, void *iopack)
{
    krldev_dec_devcount(devp);//减少设备计数
    return DFCOKSTUS;//返回成功完成的状态
}
//只是简单地增加与减少设备的引用计数，然后返回成功完成的状态就行了。
//而增加与减少设备的引用计数，是为了统计有多少个进程打开了这个设备，当设备引用计数为 0 时，就说明没有进程使用该设备。
```
## systick 设备中断回调函数
systick 设备产生的中断，以及在中断回调函数中执行的操作，即周期性的执行系统中的某些动作，比如更新系统时间，比如控制一个进程占用 CPU 的运行时间等，这些操作都需要在 systick 设备中断回调函数中执行。  
systick 设备每秒钟产生 1000 次中断，那么 1 秒钟就会调用 1000 次这个中断回调函数，这里我们只要写出这个函数就行了  
```c
drvstus_t systick_handle(uint_t ift_nr, void *devp, void *sframe)
{
    kprint("systick_handle run devname:%s intptnr:%d\n", ((device_t *)devp)->dev_name, ift_nr);
    return DFCOKSTUS;
}
//暂时什么也没干，就输出一条信息
```
我们要对内核层初始化函数修改一下，禁止进程运行，以免进程输出的信息打扰我们观察结果  
```c
void init_krl()
{
    init_krlmm();
    init_krldevice();//初始化设备
    init_krldriver();//初始化驱动程序
    init_krlsched();
    //init_krlcpuidle();禁止进程运行
    STI();//打开CPU响应中断的能力
    die(0);//进入死循环
    return;
}
```
我们在每个进程中都要主动调用进程调度器函数，否则进程就会永远霸占 CPU，永远运行下去。这是因为，我们没有定时器可以周期性地检查进程运行了多长时间，如果进程的运行时间超过了，就应该强制调度，让别的进程开始运行。  
```c
//更新进程运行时间的代码，只需要在这个中断回调函数中调用就好
drvstus_t systick_handle(uint_t ift_nr, void *devp, void *sframe)
{
    krlthd_inc_tick(krlsched_retn_currthread());//更新当前进程的tick
    return DFCOKSTUS;
}
```
krlthd_inc_tick 函数需要一个进程指针的参数，而 krlsched_retn_currthread 函数是返回当前正在运行进程的指针。在 krlthd_inc_tick 函数中对进程的 tick 值加 1，如果大于 20（也就是 20 毫秒）就重新置 0，并进行调度。  