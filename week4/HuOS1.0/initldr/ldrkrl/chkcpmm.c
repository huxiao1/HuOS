
#include "cmctl.h"

unsigned int acpi_get_bios_ebda()
{
    unsigned int address = *(unsigned short *)0x40E;
    address <<= 4;
    return address;
}

int acpi_checksum(unsigned char *ap, s32_t len)
{
    int sum = 0;
    while (len--)
    {
        sum += *ap++;
    }
    return sum & 0xFF;
}

mrsdp_t *acpi_rsdp_isok(mrsdp_t *rdp)
{
    if (rdp->rp_len == 0 || rdp->rp_revn == 0)
    {
        return NULL;
    }
    if (0 == acpi_checksum((unsigned char *)rdp, (s32_t)rdp->rp_len))
    {

        return rdp;
    }

    return NULL;
}

mrsdp_t *findacpi_rsdp_core(void *findstart, u32_t findlen)
{
    if (NULL == findstart || 1024 > findlen)
    {
        return NULL;
    }

    u8_t *tmpdp = (u8_t *)findstart;

    mrsdp_t *retdrp = NULL;
    for (u64_t i = 0; i <= findlen; i++)
    {
        if (('R' == tmpdp[i]) && ('S' == tmpdp[i + 1]) && ('D' == tmpdp[i + 2]) && (' ' == tmpdp[i + 3]) &&
            ('P' == tmpdp[i + 4]) && ('T' == tmpdp[i + 5]) && ('R' == tmpdp[i + 6]) && (' ' == tmpdp[i + 7]))
        {
            retdrp = acpi_rsdp_isok((mrsdp_t *)(&tmpdp[i]));
            if (NULL != retdrp)
            {
                return retdrp;
            }
        }
    }
    return NULL;
}

mrsdp_t *find_acpi_rsdp()
{

    void *fndp = (void *)acpi_get_bios_ebda();
    mrsdp_t *rdp = findacpi_rsdp_core(fndp, 1024);
    if (NULL != rdp)
    {
        return rdp;
    }

    fndp = (void *)(0xe0000);
    rdp = findacpi_rsdp_core(fndp, (0xfffff - 0xe0000));
    if (NULL != rdp)
    {
        return rdp;
    }
    return NULL;
}

void init_acpi(machbstart_t *mbsp)
{
    mrsdp_t *rdp = NULL;
    rdp = find_acpi_rsdp();
    if (NULL == rdp)
    {
        kerror("Your computer is not support ACPI!!");
    }
    m2mcopy(rdp, &mbsp->mb_mrsdp, (sint_t)((sizeof(mrsdp_t))));
    if (acpi_rsdp_isok(&mbsp->mb_mrsdp) == NULL)
    {
        kerror("Your computer is not support ACPI!!");
    }
    return;
}

void init_mem(machbstart_t *mbsp)
{
    e820map_t *retemp;
    u32_t retemnr = 0;
    mbsp->mb_ebdaphyadr = acpi_get_bios_ebda();
    mmap(&retemp, &retemnr);
    //init_mem 函数在调用 mmap 函数后，就会得到 e820map 结构数组，其首地址和数组元素个数由 retemp，retemnr 两个变量分别提供。
    if (retemnr == 0)
    {
        kerror("no e820map\n");
    }
    //根据e820map_t结构数据检查内存大小
    if (chk_memsize(retemp, retemnr, 0x100000, 0x8000000) == NULL)
    {
        kerror("Your computer is low on memory, the memory cannot be less than 128MB!");  //128MB->0x8000000
    }
    mbsp->mb_e820padr = (u64_t)((u32_t)(retemp));       //把e820map_t结构数组的首地址传给mbsp->mb_e820padr
    mbsp->mb_e820nr = (u64_t)retemnr;                   //把e820map_t结构数组元素个数传给mbsp->mb_e820nr 
    mbsp->mb_e820sz = retemnr * (sizeof(e820map_t));    //把e820map_t结构数组大小传给mbsp->mb_e820sz 
    mbsp->mb_memsz = get_memsize(retemp, retemnr);      //根据e820map_t结构数据计算内存大小
    init_acpi(mbsp);
    return;
}
//检查CPU主函数
void init_chkcpu(machbstart_t *mbsp)
{
    if (!chk_cpuid())
    {
        kerror("Your CPU is not support CPUID sys is die!");
        CLI_HALT();
    }

    if (!chk_cpu_longmode())
    {
        kerror("Your CPU is not support 64bits mode sys is die!");
        CLI_HALT();
    }
    mbsp->mb_cpumode = 0x40;   //如果成功则设置机器信息结构的cpu模式为64位
    return;
}
void init_krlinitstack(machbstart_t *mbsp)
{
    if (1 > move_krlimg(mbsp, (u64_t)(0x8f000), 0x1001))
    {
        kerror("iks_moveimg err");
    }
    mbsp->mb_krlinitstack = IKSTACK_PHYADR;
    mbsp->mb_krlitstacksz = IKSTACK_SIZE;
    return;
}

//长模式2MB分页模式，其页内偏移，顶级页目录、页目录指针、页目录各占有 4KB 大小，其中各有 512 个条目，每个条目 8 字节 64 位大小
//1KB = 1024个字节，1字节 = 1B，0x1000 = 4096字节 = 4KB  0x200000 = 2MB
void init_bstartpages(machbstart_t *mbsp)
{
    //顶级页目录 16MB地址处
    u64_t *p = (u64_t *)(KINITPAGE_PHYADR);
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
        {
            //大页KPDE_PS 2MB，可读写KPDE_RW，存在KPDE_P
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

//跳过内核初始化内存
//将 init_mem() 中获取到的内存信息转存到字体文件之后
void init_meme820(machbstart_t *mbsp)
{
    e820map_t *semp = (e820map_t *)((u32_t)(mbsp->mb_e820padr));         //e820数组地址
    u64_t senr = mbsp->mb_e820nr;                                        //个数
    e820map_t *demp = (e820map_t *)((u32_t)(mbsp->mb_nextwtpadr));       //mb_nextwtpadr 正是跳过内核起始地址+内核大小后的第一个页地址
    if (1 > move_krlimg(mbsp, (u64_t)((u32_t)demp), (senr * (sizeof(e820map_t)))))  ////将 semp 指针处的 senr * (sizeof(e820map_t) 内存拷贝到mb_nextwtpadr，也就是将e820内存放在内核和字库文件之后
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
void mmap(e820map_t **retemp, u32_t *retemnr)
{
    realadr_call_entry(RLINTNR(0), 0, 0);
    *retemnr = *((u32_t *)(E80MAP_NR));
    *retemp = (e820map_t *)(*((u32_t *)(E80MAP_ADRADR)));
    return;
}

e820map_t *chk_memsize(e820map_t *e8p, u32_t enr, u64_t sadr, u64_t size)
{
    u64_t len = sadr + size;
    if (enr == 0 || e8p == NULL)
    {
        return NULL;
    }
    for (u32_t i = 0; i < enr; i++)
    {
        if (e8p[i].type == RAM_USABLE)
        {
            if ((sadr >= e8p[i].saddr) && (len < (e8p[i].saddr + e8p[i].lsize)))
            {
                return &e8p[i];
            }
        }
    }
    return NULL;
}

u64_t get_memsize(e820map_t *e8p, u32_t enr)
{
    u64_t len = 0;
    if (enr == 0 || e8p == NULL)
    {
        return 0;
    }
    for (u32_t i = 0; i < enr; i++)
    {
        if (e8p[i].type == RAM_USABLE)
        {
            len += e8p[i].lsize;
        }
    }
    return len;
}

//push是普通将字压栈，pushl是标志入栈指令
/* 通过改写Eflags寄存器的第21位（IF位），观察其位的变化判断是否支持CPUID */
int chk_cpuid()
{
    int rets = 0;
    __asm__ __volatile__(
        "pushfl \n\t"                    //Pushes the EFLAGS register onto the top of the stack
        "popl %%eax \n\t"                //pops the value from the top of the stack into the EAX register
        "movl %%eax,%%ebx \n\t"          //copies the value in EAX to EBX
        "xorl $0x0200000,%%eax \n\t"     //performs a bitwise exclusive-or operation between EAX and 0x0200000
        "pushl %%eax \n\t"               //pushes the updated value of EAX back onto the stack
        "popfl \n\t"                     //pops the value from the top of the stack into the EFLAGS register,改变中断位
        "pushfl \n\t"                    //pushes the current value of the EFLAGS register onto the stack
        "popl %%eax \n\t"                //pops the value from the top of the stack into the EAX register
        "xorl %%ebx,%%eax \n\t"          //将之前的eflags值与与0x0200000按位或后的值做按位或判断21位的值是不是1
        "jz 1f \n\t"                     //jumps to label 1 if zero flag is set, which occurs IF interrupt flag not changed
        "movl $1,%0 \n\t"                //moves the value 1 into the variable "rets"
        "jmp 2f \n\t"                    //jumps to label 2 if one flag is set, which occurs IF interrupt flag changed
        "1: movl $0,%0 \n\t"             //moves value 0 into "rets" if the interrupt flag has not been changed
        "2: \n\t"                        //code ends here
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
        "movl $0x80000000,%%eax \n\t"    //把0x80000000放入eax
        "cpuid \n\t"                     //调用CPUID指令
        "cmpl $0x80000001,%%eax \n\t"    //看eax中返回结果,check if the CPU supports the x86-64 instruction set extension
        "setnb %%al \n\t"                //如果比较结果不相等，“setnb %al”指令将 AL 寄存器的低字节设置为 1。否则，它将其设置为 0
        "jb 1f \n\t"                     //如果设置了进位标志，则“jb 1f”指令跳转到标签 1，代表比较结果小于 0x80000001
        "movl $0x80000001,%%eax \n\t"    
        "cpuid \n\t"                     //把eax中放入0x800000001调用CPUID指令，检查edx中的返回数据，返回 EDX 寄存器中的特征信息。
        "bt $29,%%edx  \n\t"             // long mode  support 位,检查是否长模式，第29位是否为1
        "setcb %%al \n\t"                //如果该位已设置，“setcb %al”指令将 AL 寄存器的低字节设置为 1，否则将其设置为 0。
        "1: \n\t"                        
        "movzx %%al,%%eax \n\t"          //将 AL 的低字节复制到 EAX 寄存器中，并将其零扩展到完整的 32 位寄存器。
        : "=a"(rets)                     //store the result of the operation in the variable "rets"
        :
        :);
    return rets;
}

void init_chkmm()
{

    e820map_t *map = (e820map_t *)EMAP_PTR;
    u16_t *map_nr = (u16_t *)EMAP_NR_PTR;
    u64_t mmsz = 0;

    for (int j = 0; j < (*map_nr); j++)
    {
        if (map->type == RAM_USABLE)
        {
            mmsz += map->lsize;
        }
        map++;
    }

    if (mmsz < BASE_MEM_SZ) //0x3F00000
    {
        kprint("Your computer is low on memory, the memory cannot be less than 64MB!");
        CLI_HALT();
    }

    if (!chk_cpuid())
    {
        kprint("Your CPU is not support CPUID sys is die!");
        CLI_HALT();
    }

    if (!chk_cpu_longmode())
    {
        kprint("Your CPU is not support 64bits mode sys is die!");
        CLI_HALT();
    }
    ldr_createpage_and_open();
    //for(;;);
    return;
}

void out_char(char *c)
{

    char *str = c, *p = (char *)0xb8000;

    while (*str)
    {
        *p = *str;
        p += 2;
        str++;
    }

    return;
}

void init_bstartpagesold(machbstart_t *mbsp)
{

    if (1 > move_krlimg(mbsp, (u64_t)(PML4T_BADR), 0x3000))
    {
        kerror("ip_moveimg err");
    }

    pt64_t *pml4p = (pt64_t *)PML4T_BADR, *pdptp = (pt64_t *)PDPTE_BADR, *pdep = (pt64_t *)PDE_BADR; 
    for (int pi = 0; pi < PG_SIZE; pi++)
    {
        pml4p[pi] = 0;
        pdptp[pi] = 0;

        pdep[pi] = 0;
    }

    pml4p[0] = 0 | PDPTE_BADR | PDT_S_RW | PDT_S_PNT;
    pdptp[0] = 0 | PDE_BADR | PDT_S_RW | PDT_S_PNT;
    pml4p[256] = 0 | PDPTE_BADR | PDT_S_RW | PDT_S_PNT;

    pt64_t tmpba = 0, tmpbd = 0 | PDT_S_SIZE | PDT_S_RW | PDT_S_PNT;

    for (int di = 0; di < PG_SIZE; di++)
    {
        pdep[di] = tmpbd;
        tmpba += 0x200000;
        tmpbd = tmpba | PDT_S_SIZE | PDT_S_RW | PDT_S_PNT;
    }
    mbsp->mb_pml4padr = (u64_t)((u32_t)pml4p);
    mbsp->mb_subpageslen = 0x3000;
    mbsp->mb_kpmapphymemsz = (0x200000 * 512);
    return;
}

void ldr_createpage_and_open()
{
    pt64_t *pml4p = (pt64_t *)PML4T_BADR, *pdptp = (pt64_t *)PDPTE_BADR, *pdep = (pt64_t *)PDE_BADR; 
    for (int pi = 0; pi < PG_SIZE; pi++)
    {
        pml4p[pi] = 0;
        pdptp[pi] = 0;
        pdep[pi] = 0;
    }

    pml4p[0] = 0 | PDPTE_BADR | PDT_S_RW | PDT_S_PNT;
    pdptp[0] = 0 | PDE_BADR | PDT_S_RW | PDT_S_PNT;
  
    pml4p[256] = 0 | PDPTE_BADR | PDT_S_RW | PDT_S_PNT;

    pt64_t tmpba = 0, tmpbd = 0 | PDT_S_SIZE | PDT_S_RW | PDT_S_PNT;

    for (int di = 0; di < PG_SIZE; di++)
    {
        pdep[di] = tmpbd;
        tmpba += 0x200000;
        tmpbd = tmpba | PDT_S_SIZE | PDT_S_RW | PDT_S_PNT;
    }

    return;
}
