;*****************************************************************************
;*		内核初始化入口文件init_entry.asm				    *
;*				 	     
;*****************************************************************************
%define MBSP_ADR 0x100000
%define IA32_EFER 0C0000080H
%define PML4T_BADR 0x1000000  
%define KRLVIRADR 0xffff800000000000
%define KINITSTACK_OFF 16
global _start
global x64_GDT
extern hal_start

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
    lgdt [eGdtPtr]                   ;用lgdt加载全局描述符表
;开启 PAE 设置 MMU 并加载在二级引导器中准备好的 MMU 页表
    mov eax, cr4
    bts eax, 5                      ; CR4.PAE = 1
    mov cr4, eax
    mov eax, PML4T_BADR             ;加载MMU顶级页目录
    mov cr3, eax	
;开启 64bits long-mode
    mov ecx, IA32_EFER
    rdmsr
    bts eax, 8                      ; IA32_EFER.LME =1
    wrmsr
;开启 PE 和 paging
    mov eax, cr0
    bts eax, 0                      ; CR0.PE =1
    bts eax, 31
;开启 CACHE       
    btr eax,29						; CR0.NW=0
    btr eax,30						; CR0.CD=0  CACHE
        
    mov cr0, eax                    ; IA32_EFER.LMA = 1
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
    mov rax,hal_start   			; 调用内核主函数
	push rax
    dw 0xcb48						; iret返回指令,一条特殊的返回指令，把栈中的数据分别弹出到RIP，CS寄存器，这正是为了调用OS的第一个C函数
    jmp $

		
[section .start.data]
[BITS 32]
ex64_GDT:
enull_x64_dsc:	dq 0	
ekrnl_c64_dsc:  dq 0x0020980000000000           ; 64-bit 内核代码段
ekrnl_d64_dsc:  dq 0x0000920000000000           ; 64-bit 内核数据段

euser_c64_dsc:  dq 0x0020f80000000000           ; 64-bit 用户代码段
euser_d64_dsc:  dq 0x0000f20000000000           ; 64-bit 用户数据段
eGdtLen			equ	$ - enull_x64_dsc			; GDT长度
eGdtPtr:		dw eGdtLen - 1					; GDT界限
				dq ex64_GDT

