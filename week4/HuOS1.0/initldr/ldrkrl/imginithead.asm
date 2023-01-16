MBT_HDR_FLAGS  EQU 0x00010003
MBT_HDR_MAGIC  EQU 0x1BADB002  ;多引导协议头魔数
MBT2_MAGIC  EQU 0xe85250d6     ;第二版多引导协议头魔数
global _start                  ;导出_start符号
extern inithead_entry          ;导入外部的inithead_entry函数符号

[section .text]                ;定义.start.text代码节
[bits 32]                      ;汇编成32位代码
_start:
	jmp _entry                   ;关中断

align 4
mbt_hdr:
	dd MBT_HDR_MAGIC
	dd MBT_HDR_FLAGS
	dd -(MBT_HDR_MAGIC+MBT_HDR_FLAGS)
	dd mbt_hdr
	dd _start
	dd 0
	dd 0
	dd _entry
; Grub所需要的头

ALIGN 8
mbhdr:
	DD	0xE85250D6
	DD	0
	DD	mhdrend - mbhdr
	DD	-(0xE85250D6 + 0 + (mhdrend - mbhdr))
	DW	2, 0
	DD	24
	DD	mbhdr
	DD	_start
	DD	0
	DD	0
	DW	3, 0
	DD	12
	DD	_entry 
	DD	0 
	DW	0, 0
	DD	8
mhdrend:
; Grub2所需要的头，包含两个头是为了同时兼容GRUB、GRUB2

_entry:
	;关中断
	cli
	;关不可屏蔽中断
	in al, 0x70
	or al, 0x80	
	out 0x70,al
	;加载GDT地址到GDTR寄存器
	lgdt [GDT_PTR]
	jmp dword 0x8 :_32bits_mode ;长跳转刷新CS影子寄存器，中断时保存当前代码段寄存器（CS register）的值的寄存器

_32bits_mode:
	mov ax, 0x10
	mov ds, ax
	mov ss, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	xor eax,eax
	xor ebx,ebx
	xor ecx,ecx
	xor edx,edx
	xor edi,edi
	xor esi,esi
	xor ebp,ebp
	;初始化栈，C语言需要栈才能工作
	xor esp,esp
	mov esp,0x7c00        ;设置栈顶为0x7c00
	call inithead_entry   ;调用inithead_entry函数在inithead.c中实现
	jmp 0x200000          ;跳转到0x200000地址


;CPU 工作模式所需要的数据
;GDT全局段描述符表
GDT_START:
knull_dsc: dq 0
kcode_dsc: dq 0x00cf9e000000ffff
kdata_dsc: dq 0x00cf92000000ffff
k16cd_dsc: dq 0x00009e000000ffff	;16位代码段描述符
k16da_dsc: dq 0x000092000000ffff	;16位数据段描述符
GDT_END:
GDT_PTR:
GDTLEN	dw GDT_END-GDT_START-1	;GDT界限
GDTBASE	dd GDT_START
