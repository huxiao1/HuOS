<!-- toc -->
进程得不到所需的某个资源时就会进入等待状态，直到这种资源可用时，才会被唤醒。
- [进程的等待和唤醒机制](#进程的等待和唤醒机制)
    - [进程等待结构](#进程等待结构)
    - [进程等待](#进程等待)
- [空转进程](#空转进程)
    - [建立空转进程](#建立空转进程)
    - [空转进程运行](#空转进程运行)
- [多进程运行](#多进程运行)
<!-- tocstop -->

# 进程的等待和唤醒机制
## 进程等待结构
在实现进程的等待与唤醒的机制之前，我们需要设计一种数据结构，用于挂载等待的进程，在唤醒的时候才可以找到那些等待的进程
```c
typedef struct s_KWLST
{
    spinlock_t wl_lock;  //自旋锁
    uint_t   wl_tdnr;    //等待进程的个数
    list_h_t wl_list;    //挂载等待进程的链表头
}kwlst_t;
```
这个结构在前面讲信号量的时候，我们已经见过了。这是因为它经常被包含在信号量等上层数据结构中，**而信号量结构，通常用于保护访问受限的共享资源**
## 进程等待
让进程进入等待状态的机制也是一个函数。这个函数会设置进程状态为等待状态，让进程从调度系统数据结构中脱离，最后让进程加入到 kwlst_t 等待结构中。
```c
void krlsched_wait(kwlst_t *wlst)
{
    cpuflg_t cufg, tcufg;
    uint_t cpuid = hal_retn_cpuid();
    schdata_t *schdap = &osschedcls.scls_schda[cpuid];
    //获取当前正在运行的进程
    thread_t *tdp = krlsched_retn_currthread();
    uint_t pity = tdp->td_priority;
    krlspinlock_cli(&schdap->sda_lock, &cufg);
    krlspinlock_cli(&tdp->td_lock, &tcufg);
    tdp->td_stus = TDSTUS_WAIT;//设置进程状态为等待状态
    list_del(&tdp->td_list);//脱链,将进程从当前优先级的运行队列中删除
    krlspinunlock_sti(&tdp->td_lock, &tcufg);
    //如果当前进程是当前优先级运行队列的头进程，则将该优先级的当前运行进程设为空
    if (schdap->sda_thdlst[pity].tdl_curruntd == tdp)
    {
        schdap->sda_thdlst[pity].tdl_curruntd = NULL;
    }
    schdap->sda_thdlst[pity].tdl_nr--;
    krlspinunlock_sti(&schdap->sda_lock, &cufg);
    krlwlst_add_thread(wlst, tdp);//将进程加入等待结构中
    return;
}
```
这个函数使当前正在运行的进程进入等待状态。而当前正在运行的进程正是调用这个函数的进程，所以一个进程想要进入等待状态，只要调用这个函数就好了。
## 进程唤醒
进程的唤醒则是进程等待的反向操作行为，即从等待数据结构中获取进程，然后设置进程的状态为运行状态，最后将这个进程加入到进程调度系统数据结构中
```c
void krlsched_up(kwlst_t *wlst)
{
    cpuflg_t cufg, tcufg;
    uint_t cpuid = hal_retn_cpuid();
    schdata_t *schdap = &osschedcls.scls_schda[cpuid];
    thread_t *tdp;
    uint_t pity;
    //取出等待数据结构第一个进程并从等待数据结构中删除
    tdp = krlwlst_del_thread(wlst);
    pity = tdp->td_priority;//获取进程的优先级
    krlspinlock_cli(&schdap->sda_lock, &cufg);
    krlspinlock_cli(&tdp->td_lock, &tcufg);
    tdp->td_stus = TDSTUS_RUN;//设置进程的状态为运行状态
    krlspinunlock_sti(&tdp->td_lock, &tcufg);
    list_add_tail(&tdp->td_list, &(schdap->sda_thdlst[pity].tdl_lsth));//加入进程优先级链表
    schdap->sda_thdlst[pity].tdl_nr++;
    krlspinunlock_sti(&schdap->sda_lock, &cufg);
    return;
}
```

# 空转进程
**是我们系统下的第一个进程**。空转进程是操作系统在没任何进程可以调度运行的时候，就选择调度空转进程来运行，可以说空转进程是进程调度器最后的选择。  
_现在几乎所有的操作系统，都有一个或者几个空转进程（多 CPU 的情况下，每个 CPU 一个空转进程）_  
## 建立空转进程
我们 HuOS 的空转进程是个内核进程，按照常理，我们只要调用上节课实现的建立进程的接口，创建一个内核进程就好了。但是我们的空转进程有点特殊，它是内核进程没错，但它不加入调度系统，而是一个专用的指针指向它的。  
由于空转进程是个独立的模块，我们建立一个新文件 HuOS7.0/kernel/krlcpuidle.c  
```c
//为该进程分配一个内核栈，创建一个 thread_t 结构体，设置其属性，并初始化其内核栈，最终返回该进程的 thread_t 结构体指针
thread_t *new_cpuidle_thread()
{
    thread_t *ret_td = NULL;
    bool_t acs = FALSE;
    adr_t krlstkadr = NULL;
    uint_t cpuid = hal_retn_cpuid();
    schdata_t *schdap = &osschedcls.scls_schda[cpuid];
    //分配进程的内核栈
    krlstkadr = krlnew(DAFT_TDKRLSTKSZ);
    if (krlstkadr == NULL)
    {
        return NULL;
    }
    //分配thread_t结构体变量
    ret_td = krlnew_thread_dsc();
    if (ret_td == NULL)
    {
        acs = krldelete(krlstkadr, DAFT_TDKRLSTKSZ);
        if (acs == FALSE)
        {
            return NULL;
        }
        return NULL;
    }
    //设置进程具有系统权限
    ret_td->td_privilege = PRILG_SYS;
    ret_td->td_priority = PRITY_MIN;
    //设置进程的内核栈顶和内核栈开始地址
    ret_td->td_krlstktop = krlstkadr + (adr_t)(DAFT_TDKRLSTKSZ - 1);
    ret_td->td_krlstkstart = krlstkadr;
    //初始化进程的内核栈
    krlthread_kernstack_init(ret_td, (void *)krlcpuidle_main, KMOD_EFLAGS);
    //设置调度系统数据结构的空转进程和当前进程为ret_td
    schdap->sda_cpuidle = ret_td;
    schdap->sda_currtd = ret_td;
    return ret_td;
}

//新建空转进程
void new_cpuidle()
{
    thread_t *thp = new_cpuidle_thread();//建立空转进程
    if (thp == NULL)
    {
        //失败则主动死机
        hal_sysdie("newcpuilde err");
    }
    kprint("CPUIDLETASK: %x\n", (uint_t)thp);
    return;
}
```
```
栈: 从高到低
结构体: 从低到高

在x86架构中，堆栈是由高地址向低地址生长的。因此，内核栈的栈顶地址通常是内核栈的开始地址加上栈的大小减去1，即 (krlstkadr + DAFT_TDKRLSTKSZ - 1)。这是因为在堆栈中，栈顶地址指向栈中最后一个元素的地址。因此，DAFT_TDKRLSTKSZ - 1 计算出了内核栈的最后一个字节的地址，而加上 krlstkadr 就可以得到内核栈的栈顶地址。
```
建立空转进程由 new_cpuidle 函数调用 new_cpuidle_thread 函数完成，new_cpuidle_thread 函数的操作和前面建立内核进程差不多，只不过在函数的最后，让调度系统数据结构的空转进程和当前进程的指针，指向了刚刚建立的进程。  
上述代码中调用初始内核栈函数时，将 krlcpuidle_main 函数传了进去，这就是空转进程的主函数  
```c
void krlcpuidle_main()
{
    uint_t i = 0;
    for (;; i++)
    {
        kprint("空转进程运行:%x\n", i);//打印
        krlschedul();//调度进程
    }
    return;
}
```
空转进程的主函数本质就是个死循环，在死循环中打印一行信息，然后进行进程调度，这个函数就是永无休止地执行这两个步骤。
## 空转进程运行
由于是第一进程，所以没法用调度器来调度它，我们得手动启动它，才可以运行
```c
void krlcpuidle_start()
{
    uint_t cpuid = hal_retn_cpuid();
    schdata_t *schdap = &osschedcls.scls_schda[cpuid];
    //取得空转进程
    thread_t *tdp = schdap->sda_cpuidle;
    //设置空转进程的tss和R0特权级的栈
    tdp->td_context.ctx_nexttss = &x64tss[cpuid];
    tdp->td_context.ctx_nexttss->rsp0 = tdp->td_krlstktop;
    //设置空转进程的状态为运行状态
    tdp->td_stus = TDSTUS_RUN;
    //启动进程运行
    retnfrom_first_sched(tdp);
    return;
}
```
首先就是取出空转进程，然后设置一下机器上下文结构和运行状态，最后调用 retnfrom_first_sched 函数，恢复进程内核栈中的内容，让进程启动运行  
我们应该把建立空转进程和启动空转进程运行函数封装起来，放在一个初始化空转进程的函数中，并在内核层初始化 init_krl 函数的最后调用  
```c
void init_krl()
{
    init_krlsched();//初始化进程调度器
    init_krlcpuidle();//初始化空转进程
    die(0);//防止init_krl函数返回
    return;
}

//初始化空转进程
void init_krlcpuidle()
{
    new_cpuidle();//建立空转进程
    krlcpuidle_start();//启动空转进程运行
    return;
}
```

# 多进程运行
其实我们在空转进程中调用了调度器函数，然后进程调度器会发现系统中没有进程，又不得不调度空转进程，所以最后结果就是：空转进程调用进程调度器，而调度器又选择了空转进程，导致形成了一个闭环。但是我们现在想要看看多个进程会是什么情况，就需要建立多个进程  
我们在 init_ab_thread 函数中建立两个内核进程，分别运行两个函数，这两个函数会打印信息，init_ab_thread 函数由 init_krlcpuidle 函数调用。这样在初始化空转进程的时候，就建立了进程 A 和进程 B  
```c
//进程A主函数
void thread_a_main()
{
    uint_t i = 0;
    for (;; i++) {
        kprint("进程A运行:%x\n", i);
        krlschedul();
    }
    return;
}

//进程B主函数
void thread_b_main()
{
    uint_t i = 0;
    for (;; i++) {
        kprint("进程B运行:%x\n", i);
        krlschedul();
    }
    return;
}

void init_ab_thread()
{
    krlnew_thread((void*)thread_a_main, KERNTHREAD_FLG, 
                PRILG_SYS, PRITY_MIN, DAFT_TDUSRSTKSZ, DAFT_TDKRLSTKSZ);//建立进程A
    krlnew_thread((void*)thread_b_main, KERNTHREAD_FLG, 
                PRILG_SYS, PRITY_MIN, DAFT_TDUSRSTKSZ, DAFT_TDKRLSTKSZ);//建立进程B
    return;
}

void init_krlcpuidle()
{
    new_cpuidle();//建立空转进程
    init_ab_thread();//初始化建立A、B进程
    krlcpuidle_start();//开始运行空转进程
    return;
}
```
```
进程进入等待状态后，这进程会立马停止运行吗？
并不会，进程进入等待只是进程状态发生了改变，进程还未让出当前CPU的执行权，待调度后，即 krlschedul()，会寻找已经准备好的其它进程，切换CPU上下文，让出CPU，此时该进程才会真正的停止。
```