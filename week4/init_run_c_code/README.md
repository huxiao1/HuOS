<!-- toc -->
继续在这个 hal_start 函数里，首先执行板级初始化，其实就是 [hal 层（硬件抽象层）初始化](#硬件抽象层初始化)。其中执行了平台初始化，hal 层的内存初始化，中断初始化，最后进入到内核层的初始化。
- [函数调用顺序](#总体调用)
- [第一个C函数](#第一个C函数)
- [硬件抽象层初始化](#硬件抽象层初始化)
    - [初始化平台](#初始化平台)
    - [初始化内存](#初始化内存)
<!-- tocstop -->
# 总体调用
**一、HAL层调用链**  
hal_start()  

A、先去处理HAL层的初始化  
->init_hal()  

->->init_halplaltform()  
初始化平台  
->->->init_machbstart()  
主要是把二级引导器建立的机器信息结构，复制到了hal层一份给内核使用，同时也为释放二级引导器占用内存做好准备。  
->->->init_bdvideo()  
初始化图形机构; 初始化BGA显卡 或 VBE图形显卡信息【函数指针的使用】; 清空屏幕; 找到"background.bmp"，并显示背景图片  
->->->->init_dftgraph()  
初始了 dftgraph_t 结构体类型的变量 kdftgh，初始化进行现存操作的接口  
->->->->hal_dspversion()  
输出版本号等信息【vsprintfk】; 其中，用ret_charsinfo根据字体文件获取字符像素信息  

->->move_img2maxpadr()  
将移动initldrsve.bin到最大地址  

->->init_halmm()  
初始化内存  
->->->init_phymmarge  
申请phymmarge_t内存; 根据 e820map_t 结构数组，复制数据到phymmarge_t 结构数组; 按内存开始地址进行排序  

->->init_halintupt()  
初始化中断  
->->->init_descriptor()  
初始化GDT描述符x64_gdt  
->->->init_idt_descriptor()  
初始化IDT描述符x64_idt，绑定了中断编号及中断处理函数  
->->->init_intfltdsc()  
初始化中断异常表machintflt，拷贝了中断相关信息  
->->->init_i8259()  
初始化8529芯片中断  
->->->i8259_enabled_line(0)  
好像是取消mask，开启中断请求  
 
->init_krl()  
最后，跳转去处理内核初始化  

**二、中断调用链，以硬件中断为例**  
A、kernel.inc中，通过宏定义，进行了中断定义。以硬件中断为例，可以在kernel.inc中看到：
宏为HARWINT，硬件中断分发器函数为hal_hwint_allocator
```
%macro  HARWINT 1
    保存现场......
    mov   rdi, %1
    mov   rsi,rsp
    call    hal_hwint_allocator
    恢复现场......
%endmacro
```
B、而在kernel.asm中，定义了各种硬件中断编号，比如hxi_hwint00，作为中断处理入口
```
ALIGN   16
hxi_hwint00:
    HARWINT (INT_VECTOR_IRQ0+0)
```
C、有硬件中断时，会先到达中断处理入口，然后调用到硬件中断分发器函数hal_hwint_allocator
第一个参数为中断编号，在rdi
第二个参数为中断发生时的栈指针，在rsi
然后调用异常处理函数hal_do_hwint

D、hal_do_hwint
加锁
调用中断回调函数hal_run_intflthandle
释放锁

E、hal_run_intflthandle
先获取中断异常表machintflt
然后调用i_serlist 链表上所有挂载intserdsc_t 结构中的中断处理的回调函数，是否处理由函数自己判断

F、中断处理完毕

G、异常处理类似，只是触发源头不太一样而已

# 第一个C函数
这是第一个 C 函数，也是初始化函数
```
HuOS/hal/x86/hal_start.c

void hal_start()
{
    //第一步：初始化hal层
    init_hal();
    //第二步：初始化内核层
    init_krl();
    for(;;);  //最后的死循环却有点奇怪，其实它的目的很简单，就是避免这个函数返回，因为这个返回了就无处可去，避免走回头路。
    return;
}
```

# 硬件抽象层初始化
hal 层，把硬件相关的操作集中在这个层，并向上提供接口，目的是让内核上层不用关注硬件相关的细节，也能方便以后移植和扩展。(目前编写的是x86平台的硬件抽象层)  
```
HuOS/hal/x86/halinit.c

void init_hal()
{
    //初始化平台
    init_halplaltform();
    //初始化内存
    //初始化中断
    return;
}
```
## 初始化平台
1. 把二级引导器建立的机器信息结构复制到 hal 层中的一个全局变量中，方便内核中的其它代码使用里面的信息，**之后二级引导器建立的数据所占用的内存都会被释放**。
2. 要初始化图形显示驱动，内核在运行过程要在屏幕上输出信息。
```
HuOS/hal/x86/halplatform.c

void machbstart_t_init(machbstart_t *initp)  //machbstart_t是硬件信息定义的结构体
{
    //清零
    memset(initp, 0, sizeof(machbstart_t));
    return;
}

//从物理地址1MB处的硬件信息复制到kmachbsp（kmachbsp是结构体变量），并把kmachbsp清零，最后进行内存拷贝，将1MB处的信息复制到kmachbsp
void init_machbstart()
{
    machbstart_t *kmbsp = &kmachbsp;  //kmachbsp是个结构体变量，结构体类型是 machbstart_t，这个结构和二级引导器所使用的一模一样。同时，它还是一个 hal 层的全局变量
    machbstart_t *smbsp = MBSPADR;    //物理地址1MB处
    machbstart_t_init(kmbsp);
    //复制，要把地址转换成虚拟地址
    memcopy((void *)phyadr_to_viradr((adr_t)smbsp), (void *)kmbsp, sizeof(machbstart_t));
    return;
}

//平台初始化函数
void init_halplaltform()
{
    //复制机器信息结构
    init_machbstart();
    //初始化图形显示驱动
    init_bdvideo();
    return;
}
```
kmachbsp同时还是hal层的全局变量，我们想专门有个文件定义所有 hal 层的全局变量，于是我们在 HuOS/hal/x86/ 下建立一个 halglobal.c 文件
```
HuOS/hal/x86/halglobal.c

//全局变量定义变量放在data段
#define HAL_DEFGLOB_VARIABLE(vartype,varname) \
EXTERN  __attribute__((section(".data"))) vartype varname

HAL_DEFGLOB_VARIABLE(machbstart_t,kmachbsp);
```
宏扩展为带有“attribute（（section（“.data”）））”编译器指令的外部声明。此指令指定变量“varname”应放置在生成的对象文件的“.data”部分中。  
“EXTERN”关键字表示此变量在其他地方定义（可能在单独的源文件中），并在此处声明，以使其在当前源文件中可访问。  
在 HuOS/hal/x86/ 下的 bdvideo.c 文件中，写好 init_bdvideo 函数  
```
HuOS/hal/x86/bdvideo.c

void init_bdvideo()
{
    dftgraph_t *kghp = &kdftgh;
    //初始化图形数据结构，里面放有图形模式，分辨率，图形驱动函数指针
    init_dftgraph();
    //初始bga图形显卡的函数指针
    init_bga();
    //初始vbe图形显卡的函数指针
    init_vbe();
    //清空屏幕 为黑色
    fill_graph(kghp, BGRA(0, 0, 0));
    //显示背景图片 
    set_charsdxwflush(0, 0);
    hal_background();
    return;
}
```
init_dftgraph() 函数初始了 dftgraph_t 结构体类型的变量 kdftgh，我们在 halglobal.c 文件中定义这个变量，结构类型我们这样来定义。  
```
HuOS/hal/x86/halglobal.c

typedef struct s_DFTGRAPH
{
    u64_t gh_mode;         //图形模式
    u64_t gh_x;            //水平像素点
    u64_t gh_y;            //垂直像素点
    u64_t gh_framphyadr;   //显存物理地址 
    u64_t gh_fvrmphyadr;   //显存虚拟地址
    u64_t gh_fvrmsz;       //显存大小
    u64_t gh_onepixbits;   //一个像素字占用的数据位数
    u64_t gh_onepixbyte;
    u64_t gh_vbemodenr;    //vbe模式号
    u64_t gh_bank;         //显存的bank数
    u64_t gh_curdipbnk;    //当前bank
    u64_t gh_nextbnk;      //下一个bank
    u64_t gh_banksz;       //bank大小
    u64_t gh_fontadr;      //字库地址
    u64_t gh_fontsz;       //字库大小
    u64_t gh_fnthight;     //字体高度
    u64_t gh_nxtcharsx;    //下一字符显示的x坐标
    u64_t gh_nxtcharsy;    //下一字符显示的y坐标
    u64_t gh_linesz;       //字符行高
    pixl_t gh_deffontpx;   //默认字体大小
    u64_t gh_chardxw;
    u64_t gh_flush;
    u64_t gh_framnr;
    u64_t gh_fshdata;      //刷新相关的
    dftghops_t gh_opfun;   //图形驱动操作函数指针结构体
}dftgraph_t;

typedef struct s_DFTGHOPS
{
    //读写显存数据
    size_t (*dgo_read)(void* ghpdev,void* outp,size_t rdsz);
    size_t (*dgo_write)(void* ghpdev,void* inp,size_t wesz);
    sint_t (*dgo_ioctrl)(void* ghpdev,void* outp,uint_t iocode);
    //刷新
    void   (*dgo_flush)(void* ghpdev);
    sint_t (*dgo_set_bank)(void* ghpdev, sint_t bnr);
    //读写像素
    pixl_t (*dgo_readpix)(void* ghpdev,uint_t x,uint_t y);
    void   (*dgo_writepix)(void* ghpdev,pixl_t pix,uint_t x,uint_t y);
    //直接读写像素 
    pixl_t (*dgo_dxreadpix)(void* ghpdev,uint_t x,uint_t y);
    void   (*dgo_dxwritepix)(void* ghpdev,pixl_t pix,uint_t x,uint_t y);
    //设置x，y坐标和偏移
    sint_t (*dgo_set_xy)(void* ghpdev,uint_t x,uint_t y);
    sint_t (*dgo_set_vwh)(void* ghpdev,uint_t vwt,uint_t vhi);
    sint_t (*dgo_set_xyoffset)(void* ghpdev,uint_t xoff,uint_t yoff);
    //获取x，y坐标和偏移
    sint_t (*dgo_get_xy)(void* ghpdev,uint_t* rx,uint_t* ry);
    sint_t (*dgo_get_vwh)(void* ghpdev,uint_t* rvwt,uint_t* rvhi);
    sint_t (*dgo_get_xyoffset)(void* ghpdev,uint_t* rxoff,uint_t* ryoff);
}dftghops_t;

//刷新显存
void flush_videoram(dftgraph_t *kghp)
{
    kghp->gh_opfun.dgo_flush(kghp);
    return;
}
```
我们正是把这些实际的图形驱动函数的地址填入了这个结构体中，然后通过这个结构体，我们就可以调用到相应的函数了。
背景图片——background.bmp在打包映像文件时包含进去，可以随时替换，只要满足 1024*768，24 位的位图文件就行。
下面把这些函数调用起来：
```
HuOS/hal/x86/halinit.c

void init_hal()
{
    init_halplaltform();
    return;
}

HuOS/hal/x86/hal_start.c

void hal_start()
{
    init_hal();//初始化hal层，其中会调用初始化平台函数，在那里会调用初始化图形驱动
    for(;;);
    return;
}
```
## 初始化内存
hal 层的内存初始化比较容易，只要向内存管理器提供内存空间布局信息就可以。  
之前在[二级引导器](../Build_sec_bootstrap/README.md)中已经获取了内存布局信息（BIOS 中的 dw _getmmap ；获取内存布局视图的函数），但 HuOS 的内存管理器需要保存更多的信息，最好是顺序的内存布局信息，这样可以增加额外的功能属性，同时降低代码的复杂度。BIOS 提供的结构无法满足前面这些要求，因此需要以 BIOS 提供的结构为基础，设计一套新的数据结构。  
```
HuOS/hal/x86/halmm.c

#define PMR_T_OSAPUSERRAM 1
#define PMR_T_RESERVRAM 2
#define PMR_T_HWUSERRAM 8
#define PMR_T_ARACONRAM 0xf
#define PMR_T_BUGRAM 0xff
#define PMR_F_X86_32 (1<<0)
#define PMR_F_X86_64 (1<<1)
#define PMR_F_ARM_32 (1<<2)
#define PMR_F_ARM_64 (1<<3)
#define PMR_F_HAL_MASK 0xff

typedef struct s_PHYMMARGE
{
    spinlock_t pmr_lock;//保护这个结构是自旋锁
    u32_t pmr_type;     //内存地址空间类型
    u32_t pmr_stype;
    u32_t pmr_dtype;    //内存地址空间的子类型，见上面的宏
    u32_t pmr_flgs;     //结构的标志与状态
    u32_t pmr_stus;
    u64_t pmr_saddr;    //内存空间的开始地址
    u64_t pmr_lsize;    //内存空间的大小
    u64_t pmr_end;      //内存空间的结束地址
    u64_t pmr_rrvmsaddr;//内存保留空间的开始地址
    u64_t pmr_rrvmend;  //内存保留空间的结束地址
    void* pmr_prip;     //结构的私有数据指针，以后扩展所用
    void* pmr_extp;     //结构的扩展数据指针，以后扩展所用
}phymmarge_t;
```
_有些情况下内核要另起炉灶，不想把所有的内存空间都交给内存管理器去管理，所以要保留一部分内存空间，这就是上面结构中那两个 pmr_rrvmsaddr、pmr_rrvmend 字段的作用。_  
有了数据结构，我们还要写代码来操作它。根据 e820map_t 结构数组，建立了一个 phymmarge_t 结构数组，init_one_pmrge 函数正是把 e820map_t 结构中的信息复制到 phymmarge_t 结构中来。  
_e820map_t是一种数据结构，表示内存映射。它是x86架构下PC系统的内存管理中用于表示物理内存地址空间分布的数组。数组中的每一项表示一个内存区块，描述该区块的起始地址、长度以及用途（例如，是否可用作系统内存）。_  
```
//根据 e820map_t 结构数组，建立一个 phymmarge_t 结构数组
u64_t initpmrge_core(e820map_t *e8sp, u64_t e8nr, phymmarge_t *pmargesp)
{
    u64_t retnr = 0;
    for (u64_t i = 0; i < e8nr; i++)
    {
        //根据一个e820map_t结构建立一个phymmarge_t结构
        if (init_one_pmrge(&e8sp[i], &pmargesp[i]) == FALSE)
        {
            return retnr;
        }
        retnr++;
    }
    return retnr;
}

void init_phymmarge()
{
    machbstart_t *mbsp = &kmachbsp;     //machbstart_t是硬件信息定义结构体，存储到kmachbsp
    phymmarge_t *pmarge_adr = NULL;     //定义phymmarge_t结构体
    u64_t pmrgesz = 0;

    //根据machbstart_t机器信息结构计算获得phymmarge_t结构的开始地址和大小
    ret_phymmarge_adrandsz(mbsp, &pmarge_adr, &pmrgesz);

    u64_t tmppmrphyadr = mbsp->mb_nextwtpadr;
    e820map_t *e8p = (e820map_t *)((adr_t)(mbsp->mb_e820padr));    
    //建立phymmarge_t结构
    u64_t ipmgnr = initpmrge_core(e8p, mbsp->mb_e820nr, pmarge_adr);
    
    //把phymmarge_t结构的地址大小个数保存machbstart_t机器信息结构中
    mbsp->mb_e820expadr = tmppmrphyadr;
    mbsp->mb_e820exnr = ipmgnr;
    mbsp->mb_e820exsz = ipmgnr * sizeof(phymmarge_t);
    mbsp->mb_nextwtpadr = PAGE_ALIGN(mbsp->mb_e820expadr + mbsp->mb_e820exsz);
    
    //phymmarge_t结构中地址空间从低到高进行排序
    phymmarge_sort(pmarge_adr, ipmgnr);
    return;
}
                                     
//这里 init_halmm 函数中还调用了 init_memmgr 函数，这个正是这我们内存管理器初始化函数，我会在内存管理的那节课展开讲。而 init_halmm 函数将要被 init_hal 函数调用。
```
把这些函数，用一个总管函数调动起来，init_halmm  
```
void init_halmm()
{
    init_phymmarge();
    //init_memmgr();
    return;
}
```
## 初始化中断
中断被分为两类：  
**异常**是同步的，原因是错误和故障，不修复错误就不能继续运行。所以这时，CPU 会跳到这种错误的处理代码那里开始运行，运行完了会返回。是同步的原因是如果不修改程序中的错误，下次运行程序到这里同样会发生异常。  
**中断**是异步的，我们通常说的中断就是这种类型，它是因为外部事件而产生的。通常设备需要 CPU 关注时，会给 CPU 发送一个中断信号，所以这时 CPU 会跳到处理这种事件的代码那里开始运行，运行完了会返回。由于不确定何种设备何时发出这种中断信号，所以它是异步的。  
_在 x86 CPU 上，最多支持 256 个中断，还记得前面所说的中断表和中断门描述符吗，这意味着我们要准备 256 个中断门描述符和 256 个中断处理程序的入口。_  
中断表其实是个 **gate_t 结构的数组**，由 CPU 的 IDTR 寄存器指向，IDTMAX 为 256。
```
typedef struct s_GATE 
{         
    u16_t   offset_low;     /* 偏移 */         
    u16_t   selector;       /* 段选择子 */         
    u8_t    dcount;         /* 该字段只在调用门描述符中有效。如果在利用调用门调用子程序时引起特权级的转换和堆栈的改变，需要将外层堆栈中的参数复制到内层堆栈。该双字计数字段就是用于说明这种情况发生时，要复制的双字参数的数量。*/         
    u8_t    attr;           /* P(1) DPL(2) DT(1) TYPE(4) */         
    u16_t   offset_high;    /* 偏移的高位段 */         
    u32_t   offset_high_h;         
    u32_t   offset_resv; 
}__attribute__((packed)) gate_t;  //作用就是告诉编译器取消结构在编译过程中的优化对齐,按照实际占用字节数进行对齐 

//定义中断表 
HAL_DEFGLOB_VARIABLE(gate_t,x64_idt)[IDTMAX];
```


## 初始化中断控制器


