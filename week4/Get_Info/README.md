<!-- toc -->
在二级引导器中要检查 CPU 是否支持 64 位的工作模式、收集内存布局信息，设置内核栈和内核字体，检查是否合乎操作系统最低运行要求。设置操作系统需要的 MMU 页表、设置显卡模式、释放中文字体文件。
- [检查与收集机器信息](#检查与收集机器信息)
  - [bstartparm.c](#bstartparmc)
  - [include/ldrtype.h](#includeldrtypeh)
- [检查CPU](#检查cpu)
  - [chkcpmm.c](#chkcpmmc)
- [获取内存布局](#获取内存布局)
  - [chkcpmm.c](#chkcpmm_1c)
  - [realparm.asm](#realparmasm)
- [初始化内核栈](#初始化内核栈)
  - [chkcpmm.c](#chkcpmm_2c)
- [放置内核文件与字库文件](#放置内核文件与字库文件)
  - [fs.c](#fsc)
- [将内存信息转存到字体文件之后](#将内存信息转存到字体文件之后)
  - [chkcpmm.c](#chkcpmm_4c)
- [建立 MMU 页表数据](#建立-mmu-页表数据)
  - [chkcpmm.c](#chkcpmm_3c)
- [设置图形模式](#设置图形模式)
  - [graph.c](#graphc)
  - [ldrtype.h](#ldrtypeh)
- [显示logo](#显示logo)
- [串联起来](#串联起来)
  - [bstartparm.c](#bstartparm2c)
- [显示LOGO](#显示logo)
  - [graph.c](#graph_1c)
- [进入HuOS](#进入huos)
  - [init_entry.asm](#init_entryasm)
<!-- tocstop -->

# 检查与收集机器信息
## bstartparm.c
ldrkrl_entry() -> init_bstartparm() -> machbstart_t_init()
管理检查 CPU 模式、收集内存信息，设置内核栈，设置内核字体、建立内核 MMU 页表数据
```
//初始化machbstart_t结构体，清0,并设置一个标志
void machbstart_t_init(machbstart_t* initp)
{
    memset(initp,0,sizeof(machbstart_t));
    initp->mb_migc = MBS_MIGC;
    return;
}
void init_bstartparm()  //二级引导器ldrkrl_entry最后要进入的主函数
{
    machbstart_t* mbsp = MBSPADR;  //1MB的内存地址
    machbstart_t_init(mbsp);
    ...
    return;
}
```
## include/ldrtype.h
**include/ldrtype.h中定义了MBS_MIGC**  
```
#define MBS_MIGC (u64_t)((((u64_t)'H')<<56)|(((u64_t)'U')<<48)|(((u64_t)'O')<<40)|(((u64_t)'S')<<32)|(((u64_t)'M')<<24)|(((u64_t)'B')<<16)|(((u64_t)'S')<<8)|((u64_t)'P'))
```
machbstart_t结构体在 HuOS1.0/initldr/include/ldrtype.h 文件中。[二级引导器一节中就定义了](../Build_sec_bootstrap/README.md)  
存储二级引导器收集到的信息的数据结构，这个结构放在内存 1MB 的地方  
```
typedef struct s_MACHBSTART
{
    u64_t   mb_krlinitstack;  //内核栈地址
    u64_t   mb_krlitstacksz;  //内核栈大小
    u64_t   mb_imgpadr;       //操作系统映像
    u64_t   mb_imgsz;         //操作系统映像大小
    u64_t   mb_bfontpadr;     //操作系统字体地址
    u64_t   mb_bfontsz;       //操作系统字体大小
    u64_t   mb_fvrmphyadr;    //机器显存地址
    u64_t   mb_fvrmsz;        //机器显存大小
    u64_t   mb_cpumode;       //机器CPU工作模式
    u64_t   mb_memsz;         //机器内存大小
    u64_t   mb_e820padr;      //机器e820数组地址
    u64_t   mb_e820nr;        //机器e820数组元素个数
    u64_t   mb_e820sz;        //机器e820数组大小
    //……
    u64_t   mb_pml4padr;      //机器页表数据地址
    u64_t   mb_subpageslen;   //机器页表个数
    u64_t   mb_kpmapphymemsz; //操作系统映射空间大小
    //……
    graph_t mb_ghparm;        //图形信息
}__attribute__((packed)) machbstart_t;
```
目前init_bstartparm() 函数只是调用了一个 machbstart_t_init() 函数，在 1MB 内存地址处初始化了一个machbstart_t机器信息结构  
**物理内存在物理地址空间中是一段一段的，描述一段内存有一个数据结构**  
```
#define RAM_USABLE 1   //可用内存
#define RAM_RESERV 2   //保留内存不可使用
#define RAM_ACPIREC 3  //ACPI表相关的
#define RAM_ACPINVS 4  //ACPI NVS空间
#define RAM_AREACON 5  //包含坏内存
typedef struct s_e820{
    u64_t saddr;       /* 内存开始地址 */
    u64_t lsize;       /* 内存大小 */
    u32_t type;        /* 内存类型 */
}e820map_t;
```

# 检查CPU
## chkcpmm.c
**交给 init_chkcpu() 函数负责**。  
由于我们要 CPUID 指令来检查 CPU 是否支持 64 位长模式，所以这个函数中需要调用：chk_cpuid、chk_cpu_longmode 来干两件事，一个是检查 CPU 否支持 CPUID 指令，然后另一个用 CPUID 指令检查 CPU 支持 64 位长模式。  
检查 CPU 是否支持 CPUID 指令和检查 CPU 是否支持长模式，只要其中一步检查失败，我们就打印一条相应的提示信息，然后主动死机。 
```
//push是普通将字压栈，pushl是标志入栈指令
/* 通过改写Eflags寄存器的第21位（IF位），观察其位的变化判断是否支持CPUID */
int chk_cpuid()
{
    int rets = 0;
    __asm__ __volatile__(
        "pushfl \n\t"      压入堆栈，表示中断    Pushes the EFLAGS register onto the top of the stack
        "popl %%eax \n\t"                      pops the value from the top of the stack into the EAX register
        "movl %%eax,%%ebx \n\t"                copies the value in EAX to EBX
        "xorl $0x0200000,%%eax \n\t"           performs a bitwise exclusive-or operation between EAX and 0x0200000
        "pushl %%eax \n\t"                     pushes the updated value of EAX back onto the stack
        "popfl \n\t"       出栈，结束中断       pops the value from the top of the stack into the EFLAGS register,改变中断位
        "pushfl \n\t"                          pushes the current value of the EFLAGS register onto the stack
        "popl %%eax \n\t"                      pops the value from the top of the stack into the EAX register
        "xorl %%ebx,%%eax \n\t"                将之前的eflags值与与0x0200000按位或后的值做按位或判断21位的值是不是1
        "jz 1f \n\t"                           jumps to label 1 if zero flag is set, which occurs IF interrupt flag not changed
        "movl $1,%0 \n\t"                      moves the value 1 into the variable "rets"
        "jmp 2f \n\t"                          jumps to label 2 if one flag is set, which occurs IF interrupt flag changed
        "1: movl $0,%0 \n\t"                   moves value 0 into "rets" if the interrupt flag has not been changed
        "2: \n\t"                              code ends here
        : "=c"(rets)
        :
        :);
    return rets;
}

//检查CPU是否支持长模式
int chk_cpu_longmode()
{
    int rets = 0;
    __asm__ __volatile__(
        "movl $0x80000000,%%eax \n\t"  //把0x80000000放入eax
        "cpuid \n\t"                   //调用CPUID指令
        "cmpl $0x80000001,%%eax \n\t"  //看eax中返回结果,check if the CPU supports the x86-64 instruction set extension
        "setnb %%al \n\t"              //如果比较结果不相等，“setnb %al”指令将 AL 寄存器的低字节设置为 1。否则，它将其设置为 0
        "jb 1f \n\t"                   //如果设置了进位标志，则“jb 1f”指令跳转到标签 1，代表比较结果小于 0x80000001
        "movl $0x80000001,%%eax \n\t"  
        "cpuid \n\t"                   //把eax中放入0x800000001调用CPUID指令，检查edx中的返回数据，返回 EDX 寄存器中的特征信息。
        "bt $29,%%edx  \n\t"           //检查是否长模式，第29位是否为1
        "setcb %%al \n\t"              //如果该位已设置，“setcb %al”指令将 AL 寄存器的低字节设置为 1，否则将其设置为 0。
        "1: \n\t"
        "movzx %%al,%%eax \n\t"        //将 AL 的低字节复制到 EAX 寄存器中，并将其零扩展到完整的 32 位寄存器。
        : "=a"(rets)                   //store the result of the operation in the variable "rets"
        :
        :);
    return rets;
}

//检查CPU主函数
void init_chkcpu(machbstart_t *mbsp)
{
    if (!chk_cpuid())
    {
        kerror("CPU dnt support CPUID, sys is die!");
        CLI_HALT();
    }
    if (!chk_cpu_longmode())
    {
        kerror("CPU dnt support 64bits mode, sys is die!");
        CLI_HALT();
    }
    mbsp->mb_cpumode = 0x40;        //如果成功则设置机器信息结构的cpu模式为64位
    return;
}
```
当在EAX设置为0x80000000的情况下调用CPUID指令时，它将返回基本CPUID信息的最高输入值。这允许程序通过检查EAX中返回的值来确定CPU的有效扩展函数的数量。在这种情况下，它用于检查CPU是否支持x86-64指令集扩展（通过检查最高输入值是否大于或等于0x80000001）   
如果前一次操作有进位，则“setcb”将目标位设置为1，否则将其设置为0。而“setnb”如果没有从上一个操作中借用，则将目标位设置为1，否则将其设置为0  
**最后设置机器信息结构中的 mb_cpumode 字段为 64时, mbsp 正是传递进来的机器信息 machbstart_t 结构体的指针。**  

# 获取内存布局
**init_mem -> mmap -> -> realadr_call_entry -> BIOS中断__getmmap获得e820map 结构数组**  
物理内存在物理地址空间中是一段一段的，描述一段内存有一个数据结构  
```
#define RAM_USABLE 1   //可用内存
#define RAM_RESERV 2   //保留内存不可使用
#define RAM_ACPIREC 3  //ACPI表相关的
#define RAM_ACPINVS 4  //ACPI NVS空间
#define RAM_AREACON 5  //包含坏内存
typedef struct s_e820{
    u64_t saddr;       /* 内存开始地址 */
    u64_t lsize;       /* 内存大小 */
    u32_t type;        /* 内存类型 */
}e820map_t;
```
ACPI（高级配置和电源接口）表是一组数据结构，用于向操作系统提供有关计算机硬件配置的信息。这些表存储在系统的固件中，并由操作系统用于配置和控制各种系统设备和组件  
## chkcpmm_1.c
获取内存布局信息就是获取这个结构体的数组，这个工作我们交给 init_mem 函数来干，这个函数需要完成两件事：
1. 获取上述e820map_t结构体数组
2. 检查内存大小，因为内核对内存容量有要求，不能太小  
```
void mmap(e820map_t **retemp, u32_t *retemnr)
{
    realadr_call_entry(RLINTNR(0), 0, 0);   //调用此函数来调用BIOS中断的func_table段，从而调用__getmmap函数
    *retemnr = *((u32_t *)(E80MAP_NR));
    *retemp = (e820map_t *)(*((u32_t *)(E80MAP_ADRADR)));
    return;
}
```
mmap 函数正是通过前面讲的 realadr_call_entry 函数，来调用实模式下的 _getmmap 函数的，并且在 _getmmap 函数中调用 BIOS 中断的  
```
#define ETYBAK_ADR 0x2000
#define PM32_EIP_OFF (ETYBAK_ADR)
#define PM32_ESP_OFF (ETYBAK_ADR+4)
#define E80MAP_NR (ETYBAK_ADR+64)      //保存e820map_t结构数组元素个数的地址
#define E80MAP_ADRADR (ETYBAK_ADR+68)  //保存e820map_t结构数组的开始地址
void init_mem(machbstart_t *mbsp)
{
    e820map_t *retemp;
    u32_t retemnr = 0;
    mmap(&retemp, &retemnr);  
    //init_mem 函数在调用 mmap 函数后，就会得到 e820map 结构数组，其首地址和数组元素个数由 retemp，retemnr 两个变量分别提供
    if (retemnr == 0)
    {
        kerror("no e820map\n");
    }
    //根据e820map_t结构数据检查内存大小
    if (chk_memsize(retemp, retemnr, 0x100000, 0x8000000) == NULL)
    {
        kerror("Your computer is low on memory, the memory cannot be less than 128MB!");  //128MB->0x8000000
    }
    mbsp->mb_e820padr = (u64_t)((u32_t)(retemp));      //把e820map_t结构数组的首地址传给mbsp->mb_e820padr 
    mbsp->mb_e820nr = (u64_t)retemnr;                  //把e820map_t结构数组元素个数传给mbsp->mb_e820nr 
    mbsp->mb_e820sz = retemnr * (sizeof(e820map_t));   //把e820map_t结构数组大小传给mbsp->mb_e820sz 
    mbsp->mb_memsz = get_memsize(retemp, retemnr);     //根据e820map_t结构数据计算内存大小
    return;
}
``` 
最难写的是 mmap 函数。不过如果你理解了前面调用 BIOS 的机制，就会发现，只要调用了 BIOS 中断，就能获取 e820map 结构数组  
## realparm.asm
```
_getmmap:
  push ds
  push es
  push ss
  mov esi,0
  mov dword[E80MAP_NR],esi
  mov dword[E80MAP_ADRADR],E80MAP_ADR   ;e820map结构体开始地址
  xor ebx,ebx
  mov edi,E80MAP_ADR             ;此时edi是e820map开始地址，esi是结构数组元素个数
loop:
  mov eax,0e820h                 ;获取e820map结构参数
  mov ecx,20                     ;e820map结构大小
  mov edx,0534d4150h             ;获取e820map结构参数必须是这个数据
  int 15h                        ;BIOS的15h中断
  jc .1
  add edi,20
  cmp edi,E80MAP_ADR+0x1000      ;0x1000用来作为一个地址值来限制循环的次数
  jg .1
  inc esi
  cmp ebx,0
  jne loop                       ;循环获取e820map结构
  jmp .2
.1:
  mov esi,0                      ;出错处理，e820map结构数组元素个数为0
.2:
  mov dword[E80MAP_NR],esi       ;e820map结构数组元素个数
  pop ss
  pop es
  pop ds
  ret
```
用带有函数 0xE820 的 BIOS 中断 0x15 来检索有关内存映射的信息，并将该信息存储在内存映射结构中。循环继续，直到 BIOS 返回错误或内存映射结构已满  
esi寄存器经常用于存储数据源的地址。dword[]操作的是四字节（32位保护模式）的内存地址（BIOS 中断中调用的函数表中的函数在实模式下启动，但是在执行中被转换成保护模式，可以使用 4 个字节的数据类型）。  
JG (Jump if Greater) 指令用于判断上一次比较操作的结果，如果结果为“第一个操作数大于第二个操作数”，就会跳转到指定的地址执行。  
JC (Jump if Carry) 指令用于判断上一次运算是否产生了进位或借位，如果产生了进位或借位，就会跳转到指定的地址执行。  
如果跳转到.1后, 代码会继续顺序执行.2代码  

# 初始化内核栈
## chkcpmm_2.c
就是在机器信息结构 machbstart_t 中，记录一下栈地址和栈大小，供内核在启动时使用
```
#define IKSTACK_PHYADR (0x90000-0x10)
#define IKSTACK_SIZE 0x1000
//初始化内核栈
void init_krlinitstack(machbstart_t *mbsp)
{
    if (1 > move_krlimg(mbsp, (u64_t)(0x8f000), 0x1001))    //0x8f000～（0x8f000+0x1001）是内核栈空间
    //move_krlimg主要负责判断一个地址空间是否和内存中存放的内容有冲突。
    {
        kerror("iks_moveimg err");
    }
    mbsp->mb_krlinitstack = IKSTACK_PHYADR;  //栈顶地址
    mbsp->mb_krlitstacksz = IKSTACK_SIZE;    //栈大小是4KB
    return;
}
```
因为我们的内存中已经放置了**机器信息结构、内存视图结构数组、二级引导器、内核映像文件**，所以在处理内存空间时不能和内存中已经存在的他们冲突，否则就要覆盖他们的数据。  
0x8f000～（0x8f000+0x1001），正是我们的内核栈空间，需要检测它是否和其它空间有冲突。  

# 放置内核文件与字库文件
## fs.c
内核已经编译成了一个独立的二进制程序，和其它文件一起被打包到内核映像文件中了。所以我们必须要从映像中把它解包出来，将其放在特定的物理内存空间中才可以，放置字库文件和放置内核文件的原理一样。
```
//放置内核文件
void init_krlfile(machbstart_t *mbsp)
{
    /*
    r_file_to_padr由于内核是代码数据，所以必须要复制到指定的内存空间中。在映像中查找相应的文件并复制到对应的地址，返回文件的大小，这里是查找kernel.bin文件
    IMGKRNL_PHYADR:内核映像在物理内存中的位置
    */
    u64_t sz = r_file_to_padr(mbsp, IMGKRNL_PHYADR, "kernel.bin");
    if (0 == sz)
    {
        kerror("r_file_to_padr err");
    }
    //放置完成后更新机器信息结构中的数据
    mbsp->mb_krlimgpadr = IMGKRNL_PHYADR;
    mbsp->mb_krlsz = sz;
    //mbsp->mb_nextwtpadr始终要保持指向下一段空闲内存的首地址
    mbsp->mb_nextwtpadr = P4K_ALIGN(mbsp->mb_krlimgpadr + mbsp->mb_krlsz);
    mbsp->mb_kalldendpadr = mbsp->mb_krlimgpadr + mbsp->mb_krlsz;
    return;
}

//放置字库文件
void init_defutfont(machbstart_t *mbsp)
{
    u64_t sz = 0;
    //获取下一段空闲内存空间的首地址
    u32_t dfadr = (u32_t)mbsp->mb_nextwtpadr;
    //在映像中查找相应的文件，并复制到对应的地址，并返回文件的大小，这里是查找font.fnt文件
    sz = r_file_to_padr(mbsp, dfadr, "font.fnt");
    if (0 == sz)
    {
        kerror("r_file_to_padr err");
    }
    //放置完成后更新机器信息结构中的数据
    mbsp->mb_bfontpadr = (u64_t)(dfadr);
    mbsp->mb_bfontsz = sz;
    //更新机器信息结构中下一段空闲内存的首地址
    mbsp->mb_nextwtpadr = P4K_ALIGN((u32_t)(dfadr) + sz);
    mbsp->mb_kalldendpadr = mbsp->mb_bfontpadr + mbsp->mb_bfontsz;
    return;
}
```

# 将内存信息转存到字体文件之后
对于初始化后的内存，内核本身的内存映射还是不可访问的，所以 init_mem820 函数的作用是跳过内核初始化内存，也就是将e820内存放在内核和字库文件之后。  
这样做的原因我猜想是因为内存布局信息放在内核文件和内核字库文件之后保证了这些信息不会被修改或者删除，确保了操作系统正确地分配内存。
## chkcpmm_4.c
```
void init_meme820(machbstart_t *mbsp)
{
    e820map_t *semp = (e820map_t *)((u32_t)(mbsp->mb_e820padr));     //e820数组地址
    u64_t senr = mbsp->mb_e820nr;                                    //个数
    e820map_t *demp = (e820map_t *)((u32_t)(mbsp->mb_nextwtpadr));   //mb_nextwtpadr 正是跳过内核起始地址+内核大小后的第一个页地址
    //将 semp 指针处的 senr * (sizeof(e820map_t) 内存拷贝到mb_nextwtpadr，也就是将e820内存放在内核和字库文件之后
    if (1 > move_krlimg(mbsp, (u64_t)((u32_t)demp), (senr * (sizeof(e820map_t)))))
    {
        kerror("move_krlimg err");
    }

    m2mcopy(semp, demp, (sint_t)(senr * (sizeof(e820map_t))));
    mbsp->mb_e820padr = (u64_t)((u32_t)(demp));
    mbsp->mb_e820sz = senr * (sizeof(e820map_t));
    mbsp->mb_nextwtpadr = P4K_ALIGN((u32_t)(demp) + (u32_t)(senr * (sizeof(e820map_t))));
    mbsp->mb_kalldendpadr = mbsp->mb_e820padr + mbsp->mb_e820sz;
    return;
}
```

# 建立 MMU 页表数据
## chkcpmm_3.c
我们在二级引导器中建立 MMU 页表数据，目的就是要在内核加载运行之初开启长模式时，MMU 需要的页表数据已经准备好了  
由于我们的内核虚拟地址空间从 0xffff800000000000 开始，所以我们这个虚拟地址映射到从物理地址 0 开始，大小都是 0x400000000 即 16GB。也就是说我们要虚拟地址空间：0xffff800000000000～0xffff800400000000 映射到物理地址空间 0～0x400000000（1GB）。  
**为了简化编程，选择使用长模式下的 2MB 分页方式:**  
在这种分页方式下，64 位虚拟地址被分为 5 个位段 ：保留位段、顶级页目录索引、页目录指针索引、页目录索引  
页内偏移，顶级页目录、页目录指针、页目录各占有 4KB 大小，其中各有 512 个条目，每个条目 8 字节 64 位大小  
1KB = 1024个字节，1字节 = 1B，0x1000 = 4096字节 = 4KB  0x200000 = 2MB
![2MB分页](./images/2mb.webp)
```
#define KINITPAGE_PHYADR 0x1000000
void init_bstartpages(machbstart_t *mbsp)
{
    //顶级页目录
    u64_t *p = (u64_t *)(KINITPAGE_PHYADR);//16MB地址处
    //页目录指针
    u64_t *pdpte = (u64_t *)(KINITPAGE_PHYADR + 0x1000);
    //页目录
    u64_t *pde = (u64_t *)(KINITPAGE_PHYADR + 0x2000);
    //物理地址从0开始
    u64_t adr = 0;
    //将内核映像移动到物理内存中KINITPAGE_PHYADR这个地址处，其大小为(0x1000 * 16 + 0x2000)
    //这里我理解的是1个顶级页目录，1个页目录指针，16个页目录，一个页目录4kb大小有512个条目，一个条目8字节64位大小
    if (1 > move_krlimg(mbsp, (u64_t)(KINITPAGE_PHYADR), (0x1000 * 16 + 0x2000)))
    {
        kerror("move_krlimg err");
    }
    //将顶级页目录、页目录指针的空间清0
    for (uint_t mi = 0; mi < PGENTY_SIZE; mi++)
    {
        p[mi] = 0;
        pdpte[mi] = 0;
    }
    //映射
    for (uint_t pdei = 0; pdei < 16; pdei++)
    {
        pdpte[pdei] = (u64_t)((u32_t)pde | KPDPTE_RW | KPDPTE_P);
        for (uint_t pdeii = 0; pdeii < PGENTY_SIZE; pdeii++)
        {//大页KPDE_PS 2MB，可读写KPDE_RW，存在KPDE_P
            pde[pdeii] = 0 | adr | KPDE_PS | KPDE_RW | KPDE_P;
            adr += 0x200000;
        }
        pde = (u64_t *)((u32_t)pde + 0x1000);
    }
    //让顶级页目录中第0项和第((KRNL_VIRTUAL_ADDRESS_START) >> KPML4_SHIFT) & 0x1ff项，指向同一个页目录指针页。确保内核在启动初期，虚拟地址和物理地址是保持相同的  
    p[((KRNL_VIRTUAL_ADDRESS_START) >> KPML4_SHIFT) & 0x1ff] = (u64_t)((u32_t)pdpte | KPML4_RW | KPML4_P);
    p[0] = (u64_t)((u32_t)pdpte | KPML4_RW | KPML4_P);
    //把页表首地址保存在机器信息结构中
    mbsp->mb_pml4padr = (u64_t)(KINITPAGE_PHYADR);
    mbsp->mb_subpageslen = (u64_t)(0x1000 * 16 + 0x2000);
    mbsp->mb_kpmapphymemsz = (u64_t)(0x400000000);
    return;
}
```
映射的核心逻辑由两重循环控制，外层循环控制页目录指针顶，只有 16 项，其中每一项都指向一个页目录，每个页目录中有 512 个物理页地址。物理地址每次增加 2MB，这是由 26～30 行的内层循环控制，每执行一次外层循环就要执行 512 次内层循环。（位16G内存空间）  
最后，顶级页目录中第 0 项和第 ((KRNL_VIRTUAL_ADDRESS_START) >> KPML4_SHIFT) & 0x1ff 项，指向同一个页目录指针页，这样的话就能让虚拟地址：0xffff800000000000～0xffff800400000000 和虚拟地址：0～0x400000000，访问到同一个物理地址空间 0～0x400000000，这样做是有目的，内核在启动初期，虚拟地址和物理地址要保持相同。  
长模式下，39 - 48 位是顶级目录索引，右移 39 位，且只保留 0 - 8 位（即 &0x1FF）可获得 0xffff 8000 0000 0000 在顶级目录中的索引项。这就是传说中的平坦映射。是因为开启了mmu之后，cpu看到的都认为是虚拟地址，但是这个时候有些指令依赖符号跳转时还是按照物理地址跳转的，所以也需要物理地址跳转的地址还得是物理地址，只好把虚拟地址和物理地址搞成相同的。

# 设置图形模式
## graph.c
在计算机加电启动时，计算机上显卡会自动进入文本模式，文本模式只能显示 ASCII 字符，不能显示汉字和图形，所以我们要让显卡切换到图形模式。  
切换显卡模式依然要用 BIOS 中断，因此需要在**实模式**编写切换显卡模式的汇编代码。  
```
void init_graph(machbstart_t* mbsp)
{
    //初始化图形数据结构
    graph_t_init(&mbsp->mb_ghparm);
    //获取VBE模式，通过BIOS中断
    get_vbemode(mbsp);
    //获取一个具体VBE模式的信息，通过BIOS中断
    get_vbemodeinfo(mbsp);
    //设置VBE模式，通过BIOS中断
    set_vbemodeinfo();
    return;
}
```
VBE 是显卡的一个图形规范标准，它定义了显卡的几种图形模式，每个模式包括屏幕分辨率，像素格式与大小，显存大小。调用 BIOS 10h 中断可以返回这些数据结构。  
## ldrtype.h
这里我们选择使用了 VBE 的 118h 模式，该模式下屏幕分辨率为 1024x768，显存大小是 16.8MB。显存开始地址一般为 0xe0000000。屏幕分辨率为 1024x768，即把屏幕分成 768 行，每行 1024 个像素点，但每个像素点占用显存的 32 位数据（4 字节，红、绿、蓝、透明各占 8 位）。我们只要往对应的显存地址写入相应的像素数据，屏幕对应的位置就能显示了。每个像素点，我们可以用如下数据结构表示：  
```
typedef struct s_PIXCL
{
    u8_t cl_b; //蓝
    u8_t cl_g; //绿
    u8_t cl_r; //红
    u8_t cl_a; //透明
}__attribute__((packed)) pixcl_t;

#define BGRA(r,g,b) ((0|(r<<16)|(g<<8)|b))
//通常情况下用pixl_t 和 BGRA宏
typedef u32_t pixl_t;
```
看屏幕像素点和显存位置对应的计算方式：
```
u32_t* dispmem = (u32_t*)mbsp->mb_ghparm.gh_framphyadr;
dispmem[x + (y * 1024)] = pix;
//x，y是像素的位置
```

# 显示LOGO
在二级引导器中，已经写好了显示 logo 函数，而 logo 文件是个 24 位的位图文件，目前为了简单起见，我们只支持这种格式的图片文件。  
```
void logo(machbstart_t* mbsp)
{
    u32_t retadr=0,sz=0;
    //在映像文件中获取logo.bmp文件
    get_file_rpadrandsz("logo.bmp",mbsp,&retadr,&sz);
    if(0==retadr)
    {
        kerror("logo getfilerpadrsz err");
    }
    //显示logo文件中的图像数据
    bmp_print((void*)retadr,mbsp);
    return;
}
void init_graph(machbstart_t* mbsp)
{    
    //……前面代码省略
    //显示
    logo(mbsp);
    return;
}
```
在图格式的文件中，除了文件头的数据就是图形像素点的数据，只不过 24 位的位图每个像素占用 3 字节，并且位置是倒排的，即第一个像素的数据是在文件的最后，依次类推。我们只要依次将位图文件的数据，按照倒排次序写入显存中，这样就可以显示了。我们需要把二级引导器的文件和 logo 文件打包成映像文件，然后放在虚拟硬盘中。复制文件到虚拟硬盘中得先 mount，然后复制，最后转换成 VDI 格式的虚拟硬盘，再挂载到虚拟机上启动就行了。这也是为什么要手动建立硬盘的原因，打包命令如下。
```
lmoskrlimg -m k -lhf initldrimh.bin -o Cosmos.eki -f initldrsve.bin initldrkrl.bin font.fnt logo.bmp
```

# 串联起来
所有的实施工作的函数已经完成了，现在我们需要在 init_bstartparm() 函数中把它们串联起来，即按先后顺序，依次调用它们，实现检查、收集机器信息，设置工作环境  
[检查CPU](#检查cpu) -> [获取内存布局](#获取内存布局) -> [初始化内核栈](#初始化内核栈) -> [放置内核文件](#放置内核文件与字库文件) -> [放置字库文件](#放置内核文件与字库文件) -> [建立MMU页表](#建立-mmu-页表数据) -> [设置图形模式](#设置图形模式) -> [显示logo](#显示logo)
## bstartparm2.c
```
void init_bstartparm()
{
    machbstart_t *mbsp = MBSPADR;
    machbstart_t_init(mbsp);
    //检查CPU
    init_chkcpu(mbsp);
    //获取内存布局
    init_mem(mbsp);
    //初始化内核栈
    init_krlinitstack(mbsp);
    //放置内核文件
    init_krlfile(mbsp);
    //放置字库文件
    init_defutfont(mbsp);
    //将 init_mem() 中获取到的内存信息转存到字体文件之后
    init_meme820(mbsp); 
    //建立MMU页表
    init_bstartpages(mbsp);
    //设置图形模式
    init_graph(mbsp);
  	//显示logo
	  logo(mbsp);
    return;
}
```
init_mem820() 的作用: 将 init_mem() 中获取到的内存信息转存到字体文件之后，其实就是为它找一个安稳的地方存放。在操作系统启动之后，内核文件和内核字库文件已经被加载到内存中并被操作系统使用，如果在这个时候硬盘上的文件被修改或删除，对于操作系统来说是没有影响的。  

# 显示LOGO
## graph_1.c
在二级引导器中，已经写好了显示 logo 函数，而 logo 文件是个 24 位的位图文件，目前为了简单起见，我们只支持这种格式的图片文件。  
```
void logo(machbstart_t* mbsp)
{
    u32_t retadr=0,sz=0;
    //在映像文件中获取logo.bmp文件
    get_file_rpadrandsz("logo.bmp",mbsp,&retadr,&sz);
    if(0==retadr)
    {
        kerror("logo getfilerpadrsz err");
    }
    //显示logo文件中的图像数据
    bmp_print((void*)retadr,mbsp);
    return;
}

void init_graph(machbstart_t* mbsp)
{    
    //……
    //显示
    logo(mbsp);
    return;
}
```
在图格式的文件中，除了文件头的数据就是图形像素点的数据，只不过 24 位的位图每个像素占用 3 字节，并且位置是倒排的，即第一个像素的数据是在文件的最后，依次类推。我们只要依次将位图文件的数据，按照倒排次序写入显存中，这样就可以显示了。  
我们需要把二级引导器的文件和 logo 文件打包成映像文件，然后放在虚拟硬盘中。复制文件到虚拟硬盘中得先 mount，然后复制，最后转换成 VDI 格式的虚拟硬盘，再挂载到虚拟机上启动就行了。这也是为什么要手动建立硬盘的原因，打包命令如下。  
```
lmoskrlimg -m k -lhf initldrimh.bin -o HuOS.eki -f initldrsve.bin initldrkrl.bin font.fnt logo.bmp
```

# 进入HuOS
我们在调用 HuOS 第一个 C 函数之前，我们依然要写一小段汇编代码，切换 CPU 到长模式，初始化 CPU 寄存器和 C 语言要用的栈。因为目前代码执行流在二级引导器中，进入到 HuOS 中后在二级引导器中初始过的东西就都不能用了  
因为 CPU 进入了长模式，寄存器的位宽都变了，所以需要重新初始化。先在 HuOS/hal/x86/ 下建立一个 init_entry.asm 文件  
## init_entry.asm
```
[section .start.text]
[BITS 32]
_start:
    ;加载GDT
    cli                             ;清除IF标志
    mov ax,0x10                     ;设置段寄存器的值
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov fs,ax
    mov gs,ax
    lgdt [eGdtPtr]                  ;用lgdt加载全局描述符表
    
    ;开启 PAE 设置 MMU 并加载在二级引导器中准备好的 MMU 页表
    mov eax, cr4
    bts eax, 5                      ; CR4.PAE = 1
    mov cr4, eax
    mov eax, PML4T_BADR             ;加载MMU顶级页目录
    mov cr3, eax  
    
    ;开启 64bits long-mode 开启长模式并打开 Cache
    mov ecx, IA32_EFER
    rdmsr
    bts eax, 8                      ; IA32_EFER.LME =1
    wrmsr
    ;开启 PE 和 paging
    mov eax, cr0
    bts eax, 0                      ; CR0.PE =1
    bts eax, 31
    ;开启 CACHE       
    btr eax,29                    ; CR0.NW=0
    btr eax,30                    ; CR0.CD=0  CACHE
    mov cr0, eax                  ; IA32_EFER.LMA = 1
    jmp 08:entry64

[BITS 64]
entry64:
    ;初始化长模式下的寄存器
    mov ax,0x10
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov fs,ax
    mov gs,ax
    xor rax,rax
    xor rbx,rbx
    xor rbp,rbp
    xor rcx,rcx
    xor rdx,rdx
    xor rdi,rdi
    xor rsi,rsi
    xor r8,r8
    xor r9,r9
    xor r10,r10
    xor r11,r11
    xor r12,r12
    xor r13,r13
    xor r14,r14
    xor r15,r15
    ;读取二级引导器准备的机器信息结构中的栈地址，并用这个数据设置 RSP 寄存器
    mov rbx,MBSP_ADR
    mov rax,KRLVIRADR
    mov rcx,[rbx+KINITSTACK_OFF]
    add rax,rcx
    xor rcx,rcx
    xor rbx,rbx
    mov rsp,rax
    push 0
    ;把 8 和 hal_start 函数的地址压入栈中
    push 0x8
    mov rax,hal_start                 ;调用内核主函数
    push rax
    dw 0xcb48                         ;一条特殊的返回指令，把栈中的数据分别弹出到RIP，CS寄存器，这正是为了调用OS的第一个C函数 hal_start。 
    jmp $

[section .start.data]
[BITS 32]
x64_GDT:
enull_x64_dsc:  dq 0  
ekrnl_c64_dsc:  dq 0x0020980000000000   ; 64-bit 内核代码段
ekrnl_d64_dsc:  dq 0x0000920000000000   ; 64-bit 内核数据段
euser_c64_dsc:  dq 0x0020f80000000000   ; 64-bit 用户代码段
euser_d64_dsc:  dq 0x0000f20000000000   ; 64-bit 用户数据段
eGdtLen      equ  $ - enull_x64_dsc   ; GDT长度
eGdtPtr:    dw eGdtLen - 1             ; GDT界限
        dq ex64_GDT
```
上述代码中，1～11 行表示加载 70～75 行的 GDT，13～17 行是设置 MMU 并加载在二级引导器中准备好的 MMU 页表，19～30 行是开启长模式并打开 Cache，34～54 行则是初始化长模式下的寄存器，55～61 行是读取二级引导器准备的机器信息结构中的栈地址，并用这个数据设置 RSP 寄存器。  
最关键的是 63～66 行，它开始把 8 和 hal_start 函数的地址压入栈中。dw 0xcb48 是直接写一条指令的机器码——0xcb48，这是一条返回指令。这个返回指令有点特殊，它会把栈中的数据分别弹出到 RIP，CS 寄存器，这正是为了调用我们 HuOS 的第一个 C 函数 hal_start。  

**最后就可以试着启动系统了**  
![res](./images/res.png)
















