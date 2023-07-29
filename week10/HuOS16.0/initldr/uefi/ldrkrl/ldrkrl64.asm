%include "ldrasm.inc"
global _start
global realadr_call_entry
global IDT_PTR
extern ldrkrl_entry
[section .text]
[bits 64]
_start:
_entry:
	cli
	call ldrkrl_entry
	xor rbx,rbx
	mov eax,0xa5
	mov rdi,0x2000000
	jmp rdi
	jmp $

