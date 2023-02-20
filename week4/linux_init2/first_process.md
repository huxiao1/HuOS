<!-- toc -->
本章介绍Linux 上 GRUB 是怎样启动，以及内核里的“实权人物”——vmlinuz 内核文件是如何产生和运转的
- [解压后内核初始化](#解压后内核初始化)
- [从_start开始](#从_start开始)
- [setup_header 结构](#setup_header-结构)
- [16位的main函数](#16位的main函数)
- [startup_32函数](#startup_32-函数)
- [startup_64函数](#startup_64函数)
- [extract_kernel 函数(解压函数)](#extract_kernel-函数解压函数)
- [Linux内核的startup_64](#linux内核的startup_64)
- [Linux内核的第一个C函数](#linux内核的第一个c函数)
- [start_kernel函数](#start_kernel函数)
- [包装函数arch_call_rest_init](#包装函数arch_call_rest_init)
- [Linux的第一个用户进程](#linux的第一个用户进程)
<!-- tocstop -->

# 解压后内核初始化
先从 setup.bin 文件的入口 _start 开始，了解启动信息结构，接着由 16 位 main 函数切换 CPU 到保护模式，然后跳入 vmlinux.bin 文件中的 startup_32 函数重新加载段描述符。  
如果是 64 位的系统，就要进入 startup_64 函数，切换到 CPU 到长模式，最后调用 extract_kernel 函数解压 Linux 内核，并进入内核的 startup_64 函数，由此 Linux 内核开始运行。  

# 从_start开始
vmlinux(elf) ->(去掉文件的符号信息和重定位信息) -> 压缩 -> vmlinux.bin.gz(包含在piggy.S中) -> 加上 linux/arch/x86/boot/compressed 目录下的一些目标文件 -> vmlinux.bin -> 结合setup.bin生成bzImage -> 复制到/boot下成为vmlinuz  
/arch/x86/boot/下的head.S文件和main.c文件 -> setup.bin  
CPU 是无法识别压缩文件中的指令直接运行的，必须先进行解压后，然后解析 elf 格式的文件，把其中的指令段和数据段加载到指定的内存空间中，才能由 CPU 执行。_start 正是 setup.bin 文件的入口，在 head.S 文件中定义    
```s
#linux/arch/x86/boot/head.S
  .code16
  .section ".bstext", "ax"
  .global bootsect_start
bootsect_start:
  ljmp  $BOOTSEG, $start2
start2:
#……
#这里的512字段bootsector对于硬盘启动是用不到的
//前面提到过，硬盘中 MBR 是由 GRUB 写入的 boot.img，因此这里的 linux/arch/x86/boot/head.S 中的 bootsector 对于硬盘启动是无用的。GRUB 将 vmlinuz 的 setup.bin 部分读到内存地址 0x90000 处，然后跳转到 0x90200 开始执行，恰好跳过了前面 512 字节的 bootsector，从 _start 开始。

#……
  .globl  _start
_start:
    .byte  0xeb    # short (2-byte) jump
    .byte  start_of_setup-1f #这指令是用.byte定义出来的，跳转start_of_setup-1f
#……
#这里是一个庞大的数据结构，没展示出来，与linux/arch/x86/include/uapi/asm/bootparam.h文件中的struct setup_header一一对应。这个数据结构定义了启动时所需的默认参数
#……
start_of_setup:
  movw  %ds, %ax
  movw  %ax, %es   #ds = es
  cld               #主要指定si、di寄存器的自增方向，即si++ di++

  movw  %ss, %dx
  cmpw  %ax, %dx  # ds 是否等于 ss
  movw  %sp, %dx     
  je  2f    
  # 如果ss为空则建立新栈
  movw  $_end, %dx
  testb  $CAN_USE_HEAP, loadflags
  jz  1f
  movw  heap_end_ptr, %dx
1:  addw  $STACK_SIZE, %dx
  jnc  2f
  xorw  %dx, %dx  
2:
  andw  $~3, %dx
  jnz  3f
  movw  $0xfffc, %dx  
3:  movw  %ax, %ss
  movzwl  %dx, %esp  
  sti      # 栈已经初始化好，开中断
  pushw  %ds
  pushw  $6f
  lretw      # cs=ds ip=6：跳转到标号6处
6:
  cmpl  $0x5a5aaa55, setup_sig #检查setup标记
  jne  setup_bad
  movw  $__bss_start, %di
  movw  $_end+3, %cx
  xorl  %eax, %eax
  subw  %di, %cx
  shrw  $2, %cx
  rep; stosl          #清空setup程序的bss段
  calll  main  #调用C语言main函数 
```
代码解析:  
SI和DI寄存器是x86 CPU架构中的通用寄存器，用于存储指针和偏移量。这两个寄存器通常与字符串或数组处理有关。SI寄存器（Source Index）通常用于存储源数据或源字符串的指针和偏移量，而DI寄存器（Destination Index）通常用于存储目标数据或目标字符串的指针和偏移量。这两个寄存器常常与字符串复制、比较和移动等操作相关。  
例如，如果要将一个字符串复制到另一个内存区域，可以使用SI和DI寄存器分别指向源字符串和目标字符串的内存地址，然后使用rep movsb指令将源字符串的内容复制到目标字符串中。在执行字符串处理操作后，可以使用SI和DI寄存器中存储的指针和偏移量来定位数据。  

可执行程序包括BSS段、数据段、代码段（也称文本段）。BSS（Block Started by Symbol）通常是指用来存放程序中未初始化的全局变量和静态变量的一块内存区域。特点是:可读写的，在程序执行之前BSS段会自动清0。所以，未初始的全局变量在程序执行之前已经成0了。

## setup_header 结构
这个数据结构定义了启动时所需的默认参数  
```s
linux/arch/x86/include/uapi/asm/bootparam.h

struct setup_header {    
__u8    setup_sects;        //setup大小
__u16   root_flags;         //根标志   
__u32   syssize;            //系统文件大小
__u16   ram_size;           //内存大小
__u16   vid_mode;    
__u16   root_dev;           //根设备号
__u16   boot_flag;          //引导标志
//……
__u32   realmode_swtch;     //切换回实模式的函数地址     
__u16   start_sys_seg;    
__u16   kernel_version;     //内核版本    
__u8    type_of_loader;     //引导器类型 我们这里是GRUB
__u8    loadflags;          //加载内核的标志 
__u16   setup_move_size;    //移动setup的大小
__u32   code32_start;       //将要跳转到32位模式下的地址 
__u32   ramdisk_image;      //初始化内存盘映像地址，里面有内核驱动模块 
__u32   ramdisk_size;       //初始化内存盘映像大小
//……
} __attribute__((packed));
```
而__attribute__((packed))是GCC编译器的扩展特性之一，用于告诉编译器，按照结构体中字段定义的顺序，尽可能紧凑地分配内存，以减少浪费空间的情况。  
## 16位的main函数
我们通常用 C 编译器编译的代码，是 32 位保护模式下的或者是 64 位长模式的，却很少编译成 16 位实模式下的，其实 setup.bin 大部分代码都是 16 位实模式下的。  
```c
linux/arch/x86/boot/main.c

//定义boot_params变量
struct boot_params boot_params __attribute__((aligned(16))); //以16字节对齐
//这段代码的作用是在代码的结束位置（即_end符号）之后创建一个堆（heap）空间，以供程序在运行时动态地申请和释放内存，HEAP指针变量是堆空间的起始地址，表示从这个地址开始的内存空间是堆空间。heap_end变量则是堆空间的结束地址，表示目前堆空间的大小。
//_end变量是在链接时由链接器定义的符号，表示代码段的结束位置。因此，HEAP和heap_end的初始化值都是_end，就意味着堆空间的起始位置和结束位置都是在代码段之后，即堆空间和代码段是分离的，不会相互干扰
char *HEAP = _end;
char *heap_end = _end; 
//……
void main(void){
    //把先前setup_header结构复制到boot_params结构中的hdr变量中，在linux/arch/x86/include/uapi/asm/bootparam.h文件中你会发现boot_params结构中的hdr的类型正是setup_header结构  
    copy_boot_params();
    //初始化早期引导所用的console    
    console_init();    
    //初始化堆 
    init_heap();
    //检查CPU是否支持运行Linux    
    if (validate_cpu()) {        
        puts("Unable to boot - please use a kernel appropriate for your CPU.\n");
        die();
    }
    //告诉BIOS我们打算在什么CPU模式下运行它
    set_bios_mode();
    //查看物理内存空间布局    
    detect_memory();
    //初始化键盘
    keyboard_init();
    //查询Intel的(IST)信息。    
    query_ist();
    /*查询APM BIOS电源管理信息。*/
    #if defined(CONFIG_APM) || defined(CONFIG_APM_MODULE)   
    query_apm_bios();
    #endif
    //查询EDD BIOS扩展数据区域的信息
    #if defined(CONFIG_EDD) || defined(CONFIG_EDD_MODULE) 
    query_edd();
    #endif
    //设置显卡的图形模式    
    set_video();
    //进入CPU保护模式，不会返回了       
    go_to_protected_mode();
}
```
继续 go_to_protected_mode() 函数
```c
linux/arch/x86/boot/pm.c

//linux/arch/x86/boot/pm.c
void go_to_protected_mode(void){    
    //安装切换实模式的函数
    realmode_switch_hook();
    //开启a20地址线，是为了能访问1MB以上的内存空间
    if (enable_a20()) {        
        puts("A20 gate not responding, unable to boot...\n");
        die();    
    }
    //重置协处理器，早期x86上的浮点运算单元是以协处理器的方式存在的    
    reset_coprocessor();
    //屏蔽8259所示的中断源   
    mask_all_interrupts();
    //安装中断描述符表和全局描述符表，    
    setup_idt();    
    setup_gdt();
    //保护模式下长跳转到boot_params.hdr.code32_start
    protected_mode_jump(boot_params.hdr.code32_start,(u32)&boot_params + (ds() << 4));
}
//protected_mode_jump 是个汇编函数，在 linux/arch/x86/boot/pmjump.S 文件中
//代码逻辑和保护模式切换是一样的。只是多了处理参数的逻辑，即跳转到 boot_params.hdr.code32_start 中的地址。这个地址在 linux/arch/x86/boot/head.S 文件中设为 0x100000，如下所示。
/*
code32_start:
long  0x100000
*/
```
**需要注意的是，GRUB 会把 vmlinuz 中的 vmlinux.bin 部分，放在 1MB 开始的内存空间中。通过这一跳转，正式进入 vmlinux.bin 中。**  
```
中断描述符是中断向量表中每个中断向量对应的一个描述符，用于描述操作系统如何处理该中断。每个中断描述符包括了中断处理程序的入口地址、中断处理程序所在代码段的段选择子以及一些标志位等信息。中断描述符通过中断向量表（Interrupt Vector Table）来索引，中断向量表是一个256项的表，每个表项对应一个中断向量，用于映射中断向量到对应的中断描述符。
```
## startup_32 函数
startup_32 中需要重新加载段描述符，之后计算 vmlinux.bin 文件的编译生成的地址和实际加载地址的偏移，然后重新设置内核栈，检测 CPU 是否支持长模式，接着再次计算 vmlinux.bin 加载地址的偏移，来确定对其中 vmlinux.bin.gz 解压缩的地址。  
如果 CPU 支持长模式的话，就要设置 64 位的全局描述表，开启 CPU 的 PAE 物理地址扩展特性。再设置最初的 MMU 页表，最后开启分页并进入长模式，跳转到 startup_64，代码如下。  
```s
  .code32                    ; 指定汇编器使用 32 位代码
SYM_FUNC_START(startup_32)
  cld                        ; 清空方向标志寄存器 DF，确保字符串操作等指令以正常顺序执行
  cli
  leal  (BP_scratch+4)(%esi), %esp ; 将ESP寄存器设置为BP_scratch+4+ESI，这个地址用于保存启动时用到的临时值
  call  1f                  ; 调用标号为 1 的地址
1:  popl  %ebp
  subl  $ rva(1b), %ebp     ; 计算偏移量
    #重新加载全局段描述符表
  leal  rva(gdt)(%ebp), %eax  ; 将全局描述符表的地址存储到 EAX 寄存器
  movl  %eax, 2(%eax)         ; 将 GDT 表的地址复制到第 2 个字节的位置
  lgdt  (%eax)                ; 将 GDT 表装入 GDTR 寄存器
    #……篇幅所限未全部展示代码
    #重新设置栈
  leal  rva(boot_stack_end)(%ebp), %esp  ; 将 ESP 设置为栈顶
    #检测CPU是否支持长模式
  call  verify_cpu
  testl  %eax, %eax           ; 测试返回值，判断 CPU 是否支持长模式
  jnz  .Lno_longmode          ; 如果不支持，则跳转到 .Lno_longmode 标号所在行
    #……计算偏移的代码略过
    #开启PAE
  movl  %cr4, %eax
  orl  $X86_CR4_PAE, %eax
  movl  %eax, %cr4
    #……建立MMU页表的代码略过
    #开启长模式
  movl  $MSR_EFER, %ecx
  rdmsr
  btsl  $_EFER_LME, %eax
    #获取startup_64的地址
  leal  rva(startup_64)(%ebp), %eax
    #……篇幅所限未全部展示代码
    #内核代码段描述符索和startup_64的地址引压入栈
  pushl  $__KERNEL_CS
  pushl  %eax
    #开启分页和保护模式
  movl  $(X86_CR0_PG | X86_CR0_PE), %eax 
  movl  %eax, %cr0
    #弹出刚刚栈中压入的内核代码段描述符和startup_64的地址到CS和RIP中，实现跳转，真正进入长模式。
  lret
SYM_FUNC_END(startup_32）
```
总的来说，这段代码的作用是：  
1. 初始化一些寄存器，如 ESP 和 EBP
2. 加载全局描述符表 GDT
3. 检测 CPU 是否支持长模式
4. 如果支持，启用 PAE 模式，并创建页表
5. 开启长模式，将程序跳转到 startup_64 函数执行，进入 64 位模式
```
在汇编语言中，f 表示前向引用（forward reference），在这里表示跳转到代码段的标号 1 所在的位置执行。具体来说，这里的 1 是一个局部标号（local label），只在当前代码段内有意义。1f 与 1b 相对应，1b 表示后向引用（backward reference），用于跳转到 1 所在的位置之前执行的指令。

使用 f 和 b 前向/后向引用，可以在汇编代码中方便地跳转到之前或之后定义的标号，从而实现代码的控制流。
```
## startup_64函数
现在，我们终于开启了 CPU 长模式，从 startup_64 开始真正进入了 64 位的时代。startup_64 函数同样也是在 linux/arch/x86/boot/compressed/head64.S 文件中定义的。  
startup_64 函数中，初始化长模式下数据段寄存器，确定最终解压缩地址，然后拷贝压缩 vmlinux.bin 到该地址，跳转到 decompress_kernel 地址处，开始解压 vmlinux.bin.gz，代码如下。  
```s
#由于在这段代码中并没有出现新的段定义指令，因此默认情况下，汇编器将这些代码放置在前面定义的.code64段中，不需要再显式地定义一个新的.text段。
  .code64
  .org 0x200
SYM_CODE_START(startup_64)
  cld
  cli
  #初始化长模式下数据段寄存器
  xorl  %eax, %eax
  movl  %eax, %ds
  movl  %eax, %es
  movl  %eax, %ss
  movl  %eax, %fs
  movl  %eax, %gs
  #……重新确定内核映像加载地址的代码略过
  #重新初始化64位长模式下的栈
  leaq  rva(boot_stack_end)(%rbx), %rsp
  #……建立最新5级MMU页表的代码略过
  #确定最终解压缩地址，然后拷贝压缩vmlinux.bin到该地址
  pushq  %rsi
  leaq  (_bss-8)(%rip), %rsi
  leaq  rva(_bss-8)(%rbx), %rdi
  movl  $(_bss - startup_32), %ecx
  shrl  $3, %ecx
  std
  rep  movsq
  cld
  popq  %rsi
  #跳转到重定位的Lrelocated处
  leaq  rva(.Lrelocated)(%rbx), %rax
  jmp  *%rax
SYM_CODE_END(startup_64)

  .text
SYM_FUNC_START_LOCAL_NOALIGN(.Lrelocated)
    #清理程序文件中需要的BSS段
  xorl  %eax, %eax
  leaq    _bss(%rip), %rdi
  leaq    _ebss(%rip), %rcx
  subq  %rdi, %rcx
  shrq  $3, %rcx
  rep  stosq
  #……省略无关代码
  pushq  %rsi      
  movq  %rsi, %rdi    
  leaq  boot_heap(%rip), %rsi
  #准备参数：被解压数据的开始地址   
  leaq  input_data(%rip), %rdx
  #准备参数：被解压数据的长度   
  movl  input_len(%rip), %ecx
  #准备参数：解压数据后的开始地址     
  movq  %rbp, %r8
  #准备参数：解压数据后的长度
  movl  output_len(%rip), %r9d
  #调用解压函数解压vmlinux.bin.gz，返回入口地址
  call  extract_kernel
  popq  %rsi
  #跳转到内核入口地址 
  jmp  *%rax
SYM_FUNC_END(.Lrelocated)
```
代码解析:  
```leaq  rva(boot_stack_end)(%rbx), %rsp```将boot_stack_end的地址（rva表示一个相对虚拟地址）存储在%rbx寄存器中，并将%rsp寄存器的值设置为boot_stack_end的地址加上%rbx寄存器的值。这实际上是将boot_stack_end的地址加上%rbx的值（它是一个内存地址偏移量）计算出内存地址，并将这个地址存储在%rsp寄存器中，这将把堆栈指针设置为boot_stack_end的地址加上%rbx的值。  
**RSI寄存器**是源索引寄存器（Source Index Register）的缩写，它通常用作源操作数的指针或偏移量，例如在字符串操作或数组操作中。它还用于传递函数参数，如函数调用时，第一个参数通常存储在RSI中。  
**RDI寄存器**是目的索引寄存器（Destination Index Register）的缩写，通常用作目标操作数的指针或偏移量，例如在字符串操作或数组操作中。它也用于传递函数参数，如函数调用时，第二个参数通常存储在RDI中。  
**RCX寄存器**通常用作循环计数器、存储函数参数或者临时存储数据等。  
**RIP寄存器**是指令指针寄存器（Instruction Pointer Register）的缩写，它包含当前正在执行的指令的地址。在程序执行时，RIP会不断地指向下一条要执行的指令的地址，从而实现程序的顺序执行。当遇到跳转指令（如JMP或CALL）时，RIP会跳转到新的地址执行指令。  
**RBX寄存器**是一个通用寄存器，可以用于存储各种数据，例如指针、偏移量、计数器或其他数据。它的名称表示“基址寄存器”，因为它通常用于存储内存访问的基地址或偏移量，从而实现数组或数据结构的访问。  
**ECX寄存器**可以用于存储各种数据，例如指针、偏移量、计数器或其他数据，但通常用于存储计数器或循环计数器的值，从而实现循环等计算功能。在循环中，可以使用ECX寄存器存储循环次数，并在每次迭代中递减计数器，直到计数器的值为0时跳出循环。  
**RDX寄存器**可以存储64位的数据，通常用于存储除数或被除数的高32位，或者存储一些临时变量。在调用一些系统函数或执行一些I/O操作时，rdx寄存器还可以用于传递参数。rdx寄存器是64位寄存器中的一员，与32位寄存器edx、16位寄存器dx以及8位寄存器dl都是对应的，但它们的使用位数不同。  
**rbp寄存器**是栈帧指针寄存器，用于存储当前函数的栈帧的基地址，它指向当前函数的栈底。在函数执行期间，局部变量和函数参数都是通过rbp和栈指针rsp相对地址的形式进行访问的。  
**r8寄存器**是第八个通用寄存器，用于存储数据或地址。在函数中，它可以用于存储临时数据或函数参数。在调用函数时，前6个参数会被依次传递给寄存器rdi、rsi、rdx、rcx、r8和r9，如果参数的数量超过6个，则剩余的参数会被依次压入栈中。  
**rsp寄存器**是x86-64架构中的栈指针寄存器，用于存储栈的顶部地址。栈是一个LIFO（Last-In-First-Out）数据结构，常用于函数调用、中断处理等场景中保存现场和参数等信息。
```cli```是Clear Interrupt Flag的缩写，是一个x86汇编指令，用于清除中断标志（Interrupt Flag，简称IF）。中断标志是x86 CPU中的一个标志寄存器，用于控制CPU是否响应可屏蔽中断（可屏蔽中断是指可以被通过程序指令屏蔽的中断，而非非屏蔽中断）。  
```.org 0x200```在汇编语言中用于指定程序装载的地址。在该段代码中，".org 0x200"指令告诉编译器将程序装载到内存的地址0x200处。需要注意的是，这里的地址0x200并不是绝对的物理地址，而是相对于操作系统加载程序的基址的偏移量。  
```shrl  $3, %ecx```使用了逻辑右移指令SHRL来将寄存器ECX的值向右移动3位（即除以8）。在该段代码中，这行指令的作用是将一个数据段（BSS段）的大小转换为对应的字节数，并将结果存储在寄存器ECX中。这个结果在下面的REP STOSQ指令中使用，用于向BSS段中写入相应数量的零值。因为每个操作数占8个字节，因此将大小除以8可以得到需要写入多少个操作数，即需要执行多少次REP MOVSQ指令。  
```std```是Set Direction Flag的缩写，它是一个控制标志寄存器EFLAGS的标志位。STD指令可以将EFLAGS中的方向标志位DF（Direction Flag）设置为1，表示字符串操作应该从高地址向低地址方向进行，也就是逆向操作。相反，当DF标志位被清零时，字符串操作将从低地址向高地址方向进行，这是正向操作。  
```REP MOVSQ```是一个指令前缀，它用于重复执行一次MOV SQ（Move Quadword），也就是将数据从一个内存位置复制到另一个内存位置。其中，"Quadword"是指64位的数据单元，也就是8个字节。在该段代码中，REP MOVSQ指令被用于复制内核的BSS段数据。由于DF标志位被设置为1，所以数据是从高地址向低地址复制的。复制的源地址在RSI寄存器中，目的地址在RDI寄存器中，需要复制的数据长度保存在ECX寄存器中，每次复制8个字节。  
```cld```是Clear Direction Flag的缩写，是一个x86汇编指令，用于清除方向标志（Direction Flag，简称DF）。方向标志是x86 CPU中的一个标志寄存器，用于控制字符串操作（比如MOVSB、MOVSW、MOVSD、CMPSB、CMPSW、CMPSD、LODSB、LODSW、LODSD、STOSB、STOSW、STOSD等指令）的方向。在该段代码中，CLD指令被用于清除方向标志，以确保接下来的字符串操作是从低地址向高地址执行的。这样，在复制BSS段数据时，数据就会从低地址向高地址复制，避免了复制数据时的错误。  
```leaq _bss(%rip), %rdi```将存储_bss符号地址的内存地址（即_bss的地址）加载到rdi寄存器中，作为目的地址。  
```leaq _ebss(%rip), %rcx```将存储_ebss符号地址的内存地址（即_ebss的地址）加载到rcx寄存器中，作为源地址。  
```rep stosq```清零指定数量的存储器区域。REP指令会重复执行下一条指令（这里是STOSQ）rcx寄存器指定的次数，即清零rcx寄存器中的长字个数。STOSQ指令将rax寄存器的值存储到目的地址（rdi寄存器指定）中，然后增加rdi的值以指向下一个目的地址，最后根据方向标志寄存器DF的值（默认值为0，表示向前）增加或减少ecx寄存器中的值，然后重复上述操作直到ecx的值为0。```stosq```会将%rax作为目的地址进行存储操作。  
## extract_kernel 函数(解压函数)
extract_kernel 函数根据 piggy.o 中的信息从 vmlinux.bin.gz 中解压出 vmlinux。extract_kernel 函数不仅仅是解压，还需要解析 elf 格式。  
```c
linux/arch/x86/boot/compressed/misc.c

asmlinkage __visible void *extract_kernel(
                                void *rmode, memptr heap,
                                unsigned char *input_data,
                                unsigned long input_len,
                                unsigned char *output,
                                unsigned long output_len
                                ){    
    const unsigned long kernel_total_size = VO__end - VO__text;
    unsigned long virt_addr = LOAD_PHYSICAL_ADDR;    
    unsigned long needed_size;
    //省略了无关性代码
    debug_putstr("\nDecompressing Linux... ");    
    //调用具体的解压缩算法解压
    __decompress(input_data, input_len, NULL, NULL, output, output_len, NULL, error);
    //解压出的vmlinux是elf格式，所以要解析出里面的指令数据段和常规数据段
    //返回vmlinux的入口点即Linux内核程序的开始地址  
    parse_elf(output); 
    handle_relocations(output, output_len, virt_addr);    
    debug_putstr("done.\nBooting the kernel.\n");
    return output;
}
```
**extract_kernel 函数**调用 __decompress 函数，对 vmlinux.bin.gz 使用特定的解压算法进行解压。解压算法是编译内核的配置选项决定的。但是，__decompress 函数解压出来的是 vmlinux 文件是 elf 格式的，所以还要调用 **parse_elf 函数**进一步解析 elf 格式，把 vmlinux 中的指令段、数据段、BSS 段，根据 elf 中信息和要求放入特定的内存空间，返回指令段的入口地址。  
在 **Lrelocated 函数**的最后一条指令：jmp *rax，其中的 rax 中就是保存的 extract_kernel 函数返回的入口点，就是从这里开始进入了 Linux 内核。  
## Linux内核的startup_64
此处的startup_64与之前的完全不是一个，这个是内核态的。他是内核的入口函数  
```s
#linux/arch/x86/kernel/head_64.S  

    .code64
SYM_CODE_START_NOALIGN(startup_64)
  #切换栈
  leaq  (__end_init_task - SIZEOF_PTREGS)(%rip), %rsp
  #跳转到.Lon_kernel_cs:
  pushq  $__KERNEL_CS
  leaq  .Lon_kernel_cs(%rip), %rax
  pushq  %rax
  lretq
.Lon_kernel_cs:
    #对于第一个CPU，则会跳转secondary_startup_64函数中1标号处
  jmp 1f
SYM_CODE_END(startup_64)
```
代码解析:  
```leaq (__end_init_task - SIZEOF_PTREGS)(%rip), %rsp```是将栈指针%rsp设置为__end_init_task - SIZEOF_PTREGS的地址。__end_init_task是一个符号，代表着内核数据段结束的地址，SIZEOF_PTREGS是一个常数，表示上下文保存寄存器的大小。  
```pushq $__KERNEL_CS```是将__KERNEL_CS压入栈中，__KERNEL_CS是一个符号，表示内核代码段选择器。  
```leaq .Lon_kernel_cs(%rip)```, %rax是将.Lon_kernel_cs的地址赋值给%rax寄存器，%rip是指令指针寄存器，这里用于获取下一条指令.Lon_kernel_cs的地址。  
```lretq```是从堆栈中弹出返回地址，并跳转到该地址。由于之前将内核代码段选择器和.Lon_kernel_cs的地址压入了栈中，因此lretq指令会将内核代码段选择器和.Lon_kernel_cs的地址作为返回地址，并跳转到.Lon_kernel_cs  
该段代码的目的是为了完成一些启动时的初始化工作，然后将控制权转移到.Lon_kernel_cs处继续执行内核的初始化过程。  
## Linux内核的第一个C函数
在 secondary_startup_64 函数的最后，调用的 x86_64_start_kernel 函数是用 C 语言写的，那么**它一定就是 Linux 内核的第一个 C 函数**    
此函数中又一次处理了页表，处理页表就是处理Linux内核虚拟地址空间。然后，x86_64_start_kernel 函数复制了引导信息，即 struct boot_params 结构体。  
最后调用了 x86_64_start_reservations 函数，其中处理了平台固件相关的东西，就是调用了大名鼎鼎的 start_kernel 函数。  
_struct boot_params是16位开始定义的_  
```c
linux/arch/x86/kernel/head64.c

asmlinkage __visible void __init x86_64_start_kernel(char * real_mode_data){   
    //重新设置早期页表
    reset_early_page_tables();
    //清理BSS段
    clear_bss();
    //清理之前的顶层页目录
    clear_page(init_top_pgt);
    //复制引导信息
    copy_bootdata(__va(real_mode_data));
    //加载BSP CPU的微码
    load_ucode_bsp();
    //让顶层页目录指向重新设置早期页表
    init_top_pgt[511] = early_top_pgt[511];
    x86_64_start_reservations(real_mode_data);
}

void __init x86_64_start_reservations(char *real_mode_data){  
   //略过无关的代码
    start_kernel();
}
```
**代码解析**:    
asmlinkage通知编译器函数参数在堆栈中传递，而不是通过CPU寄存器。对于一些使用特定寄存器传递参数的体系结构，比如x86，该修饰符是必需的。  

__visible用于将函数标记为可见符号（visible symbol），这些符号可在可重定位目标文件和动态共享库中被链接器看到。这在编写内核和驱动程序时非常有用。  

__init 是一个宏定义，用于告诉编译器这段代码只会在系统初始化时运行，并在初始化完成后会被丢弃。这个宏定义通常用于定义一些只在系统启动阶段需要用到的函数和数据，可以帮助减小系统的内存占用。  
当编译器编译代码时，带有 __init 宏定义的函数和数据将被放入 .init 段中。在 Linux 内核启动时，加载程序会运行 .init 段中的代码，并在所有初始化完成后丢弃这个段，从而释放其占用的内存。使用 __init 宏定义可以帮助优化内核代码的内存占用，特别是在嵌入式设备等资源受限的环境中，这一点显得尤为重要。  

__va 是一个宏，表示将一个物理地址转换为虚拟地址。在 Linux 内核中，有时需要将物理地址转换为虚拟地址来进行内存映射等操作。通常，物理地址在内核中用 unsigned long 类型表示，而虚拟地址在内核中用 void * 类型表示。因此，__va 宏将一个 unsigned long 类型的物理地址转换为 void * 类型的虚拟地址。  
在给定的代码中，copy_bootdata 函数接受一个 char * 类型的参数 real_mode_data，该参数是一个指向引导信息的物理地址的指针。__va 宏被用于将 real_mode_data 转换为一个指向引导信息的虚拟地址的指针，以便进行数据的复制。  
## start_kernel函数
这个函数是真正内核的开始，Linux 内核所有功能的初始化函数都是在 start_kernel 函数中调用的。一旦 start_kernel 函数执行完成，Linux 内核就具备了向应用程序提供一系列功能服务的能力。start_kernel 函数中调用了大量 Linux 内核功能的初始化函数。  
```c
/linux/init/main.c

void start_kernel(void){    
    char *command_line;    
    char *after_dashes;
    //CPU组早期初始化
    cgroup_init_early();
    //关中断
    local_irq_disable();
    //ARCH层初始化
    setup_arch(&command_line);
    //日志初始化      
    setup_log_buf(0);    
    sort_main_extable();
    //陷阱门初始化    
    trap_init();
    //内存初始化    
    mm_init();
    ftrace_init();
    //调度器初始化
    sched_init();
    //工作队列初始化
    workqueue_init_early();
    //RCU锁初始化
    rcu_init();
    //IRQ 中断请求初始化
    early_irq_init();    
    init_IRQ();    
    tick_init();    
    rcu_init_nohz();
    //定时器初始化 
    init_timers();    
    hrtimers_init();
    //软中断初始化    
    softirq_init();    
    timekeeping_init();
    mem_encrypt_init();
    //每个cpu页面集初始化
    setup_per_cpu_pageset();    
    //fork初始化建立进程
    fork_init();    
    proc_caches_init();    
    uts_ns_init();
    //内核缓冲区初始化    
    buffer_init();    
    key_init();    
    //安全相关的初始化
    security_init();  
    //VFS数据结构内存池初始化  
    vfs_caches_init();
    //页缓存初始化    
    pagecache_init();
    //进程信号初始化    
    signals_init();    
    //运行第一个进程 
    arch_call_rest_init();
}
```
代码解析:  
**VFS（Virtual File System）**是 Linux 操作系统中的一个抽象层，它提供了一种机制，使得文件系统可以以一种统一的方式被操作和访问。在 VFS 的架构下，应用程序并不需要知道数据在磁盘上的具体位置和实现细节，而是通过使用通用的文件系统 API 来访问文件系统。这样，Linux 内核就能够支持各种不同的文件系统，比如 ext4、NTFS、FAT 等。  

**陷阱门**是一种特殊的门，用于实现中断和异常处理。当发生中断或异常时，处理器将执行陷阱门描述符中的代码，并将控制权转移到陷阱门指向的代码。陷阱门可以被用来实现系统调用和软件中断，以及处理诸如除零、缺页、非法指令等异常。陷阱门的设置需要一些特殊的操作，包括在中断描述符表中分配一个条目、设置适当的中断处理程序、设置堆栈指针和设置特权级等。  

**RCU (Read-Copy-Update)** 是一种并发编程技术，主要用于解决在多处理器环境下的共享数据并发读写问题。在多处理器环境下，读写锁和自旋锁等常用锁机制的性能可能会受到限制。RCU锁可以通过延迟实际的释放操作，使得读取操作可以安全地访问共享数据，而不需要加锁，从而提高了并发读取性能。RCU 锁的基本思想是：每当共享数据结构发生修改时，不会直接更新数据结构，而是创建一个副本，更新副本，然后让其他 CPU 持有对原始数据结构的引用，直到副本中不再引用原始数据结构时，才能安全地删除原始数据结构。Linux 内核中有一个称为 rcu 的子系统，实现了 RCU 锁。在这个子系统中，使用了一种称为“质量保证”的技术，可以在等待一定时间后，确保共享数据结构的副本中没有指向原始数据结构的引用，从而安全地删除原始数据结构。RCU锁主要用于在内核中实现锁-free或者low-lock算法，例如进程管理、网络包处理等高性能场景。

**CPU 页面集**是一种用于管理虚拟内存的数据结构。它是一组指向页面表的指针，用于在操作系统内核中管理虚拟地址空间和物理内存。每个进程都有自己的页面集，其中包含用于转换虚拟地址到物理地址的页面表。当进程访问虚拟地址时，CPU 会使用页面集中的页面表将其转换为物理地址，以便访问实际的内存。CPU 页面集通常包含一个或多个级别的页表，以及一些辅助结构来帮助操作系统管理虚拟内存。它通常会缓存最近使用的页面表项，以提高性能。CPU 页面集还可以实现内存保护机制，例如页级别的保护和访问权限控制。
**在不同的操作系统和 CPU 架构中，CPU 页面集的实现和名称可能有所不同。例如，x86 架构上的 CPU 页面集称为页表，而在 ARM 架构上则称为页表或转换表。**

**内核缓冲区和页缓存**都是 Linux 系统中用于提高文件系统和存储设备性能的重要机制。内核缓冲区是内核中维护的一块缓存，用于加速数据的读写。当应用程序向存储设备读取或写入数据时，数据会首先存储在内核缓冲区中，然后再将数据从内核缓冲区复制到存储设备中，或者从存储设备中读取数据到内核缓冲区中。内核缓冲区的大小可以通过系统参数来配置，通常会自动调整以适应系统的内存容量和使用情况。页缓存是 Linux 系统中一种高速缓存，用于加速磁盘文件的读取。当文件系统需要读取文件时，文件的数据块会被缓存到页缓存中，如果之后需要再次读取该文件，系统可以直接从页缓存中获取数据，而无需再次读取磁盘。页缓存的大小也可以通过系统参数来配置，通常会根据系统的内存容量和使用情况自动调整。内核缓冲区和页缓存在 Linux 系统中都是通过内存映射技术实现的。
## 包装函数arch_call_rest_init
```c
void __init __weak arch_call_rest_init(void){    
    rest_init();
}
```
rest_init： 建立了两个 Linux 内核线程
```c
noinline void __ref rest_init(void){    struct task_struct *tsk;
    int pid;
    //建立kernel_init线程
    pid = kernel_thread(kernel_init, NULL, CLONE_FS);   
    //建立khreadd线程 
    pid = kernel_thread(kthreadd, NULL, CLONE_FS | CLONE_FILES);
}
```
代码解析:  
**noinline**是一种函数属性，用于禁止编译器将函数内联。通常情况下，编译器会自动决定哪些函数应该被内联，以提高代码执行效率。但有些函数可能会因为特殊的原因不适合被内联，例如函数体过大、递归调用等。在这种情况下，可以使用noinline属性告诉编译器不要将该函数内联。  
**__ref**是一种对象属性，用于标记某个对象是内核中的一个引用计数对象。引用计数对象是内核中的一种常见数据结构，用于管理内核对象的生命周期。在内核中，引用计数对象通常会包含一个引用计数字段，该字段记录了当前有多少个对象引用了该引用计数对象。当引用计数减为零时，表示该引用计数对象不再被使用，可以被安全地释放。__ref属性可以让编译器自动识别引用计数对象，从而生成对应的引用计数代码。  
**CLONE_FS** 表示新进程和父进程共享相同的文件系统信息，包括挂载点和当前工作目录。如果新进程修改了当前工作目录，那么父进程也会受到影响，反之亦然。  
**CLONE_FILES** 表示新进程和父进程共享相同的文件描述符表，即它们可以共享同一个文件句柄。如果新进程关闭了某个文件描述符，那么父进程也会受到影响，反之亦然。  

Linux 内核线程可以执行一个内核函数， 只不过这个函数有独立的线程上下文，可以被 Linux 的进程调度器调度，对于 kernel_init 线程来说，执行的就是 kernel_init 函数。
## Linux的第一个用户进程
Linux 内核的第一个用户态进程是在 kernel_init 线程建立的，而 kernel_init 线程执行的就是 kernel_init 函数
```c
static int __ref kernel_init(void *unused){   
     int ret;
     if (ramdisk_execute_command) {       
         ret = run_init_process(ramdisk_execute_command);        
         if (!ret)            
             return 0;        
         pr_err("Failed to execute %s (error %d)\n",ramdisk_execute_command, ret);    
     }
     if (execute_command) {        
         ret = run_init_process(execute_command);        
         if (!ret)            
         return 0;        
         panic("Requested init %s failed (error %d).", execute_command, ret);    
     }
    if (!try_to_run_init_process("/sbin/init") || !try_to_run_init_process("/etc/init") || !try_to_run_init_process("/bin/init") || !try_to_run_init_process("/bin/sh"))        
    return 0;
panic("No working init found.  Try passing init= option to kernel."  "See Linux Documentation/admin-guide/init.rst for guidance.");
}
//ramdisk_execute_command 和 execute_command 都是内核启动时传递的参数，它们可以在 GRUB 启动选项中设置
//try_to_run_init_process 和 run_init_process 函数的核心都是调用 sys_fork 函数建立进程的，这里我们不用关注它的实现细节。
```
但是，通常我们不会传递参数，所以这个函数会执行到上述代码的 15 行，依次尝试以 //sbin/init、/etc/init、/bin/init、/bin/sh 这些文件为可执行文件建立进程，但是只要其中之一成功就行了。  
**Linux 之所以如此复杂，是因为它把完成各种功能的模块组装了一起，而我们 HuOS 则把内核之前的初始化工作，分离出来，形成二级引导器，二级引导器也是由多文件模块组成的，最后用我们的映像工具把它们封装在一起。**  