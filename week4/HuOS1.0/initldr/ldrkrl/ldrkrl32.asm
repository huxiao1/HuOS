%include "ldrasm.inc"
global _start
global realadr_call_entry
global IDT_PTR
extern ldrkrl_entry
[section .text]
[bits 32]
_start:
_entry:
	cli
	lgdt [GDT_PTR]              ;加载GDT地址到GDTR寄存器
	lidt [IDT_PTR]              ;加载IDT地址到IDTR寄存器
	jmp dword 0x8 :_32bits_mode ;长跳转刷新CS影子寄存器

_32bits_mode:
	mov ax, 0x10	;数据段选择子(目的)
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
	xor esp,esp
	mov esp,0x90000  ;使得栈底指向了0x90000
	call ldrkrl_entry;调用ldrkrl_entry函数
	xor ebx,ebx
	jmp 0x2000000    ;跳转到0x2000000的内存地址
	jmp $


realadr_call_entry:
	pushad           ;保存通用寄存器
	push    ds		 ;保存4个段寄存器
	push    es
	push    fs
	push    gs
	call save_eip_jmp;调用save_eip_jmp 
	pop	gs
	pop	fs
	pop	es
	pop	ds           ;恢复4个段寄存器
	popad            ;恢复通用寄存器
	ret
save_eip_jmp:
	pop esi                   ;弹出call save_eip_jmp时保存的eip到esi寄存器中
	mov [PM32_EIP_OFF],esi    ;把eip保存到特定的内存空间中,EIP寄存器里存储的是CPU下次要执行的指令的地址
	mov [PM32_ESP_OFF],esp    ;把esp保存到特定的内存空间中,始终指向栈顶
	jmp dword far [cpmty_mode];长跳转这里表示把cpmty_mode处的第一个4字节装入eip，把其后的2字节装入cs
cpmty_mode:
	dd 0x1000
	dw 0x18
	jmp $

GDT_START:
knull_dsc: dq 0
kcode_dsc: dq 0x00cf9a000000ffff ;a-e
kdata_dsc: dq 0x00cf92000000ffff
k16cd_dsc: dq 0x00009a000000ffff ;a-e
k16da_dsc: dq 0x000092000000ffff
GDT_END:

GDT_PTR:
GDTLEN	dw GDT_END-GDT_START-1	;GDT界限
GDTBASE	dd GDT_START

IDT_PTR:
IDTLEN	dw 0x3ff
IDTBAS	dd 0
