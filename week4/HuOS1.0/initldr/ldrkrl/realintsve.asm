%include "ldrasm.inc"     ;表示将ldrasm.inc文件中的指令包含到当前程序中
global _start
[section .text]
[bits 16]
_start:
_16_mode:
	mov	bp,0x20           ;0x20是指向GDT中的16位数据段描述符
	mov	ds, bp
	mov	es, bp
	mov	ss, bp
	mov	ebp, cr0
	and	ebp, 0xfffffffe
	mov	cr0, ebp          ;CR0.P=0 关闭保护模式
	jmp	0:real_entry      ;刷新CS影子寄存器，真正进入实模式
real_entry:
	mov bp, cs
	mov ds, bp
	mov es, bp
	mov ss, bp            ;重新设置实模式下的段寄存器 都是CS中值，即为0 
	mov sp, 08000h        ;设置栈
	mov bp,func_table
	add bp,ax
	call [bp]             ;调用函数表中的汇编函数，ax是C函数中传递进来的
	cli
	call disable_nmi
	mov	ebp, cr0
	or	ebp, 1
	mov	cr0, ebp          ;CR0.P=1 开启保护模式
	jmp dword 0x8 :_32bits_mode
[BITS 32]
_32bits_mode:
	mov bp, 0x10          ;重新设置保护模式下的段寄存器0x10是32位数据段描述符的索引
	mov ds, bp
	mov ss, bp
	mov esi,[PM32_EIP_OFF];加载先前保存的EIP
	mov esp,[PM32_ESP_OFF];加载先前保存的ESP
	jmp esi               ;eip=esi 回到了realadr_call_entry函数中

[BITS 16]
DispStr:
	mov bp,ax
	mov cx,23
	mov ax,01301h
	mov bx,000ch
	mov dh,10
	mov dl,25
	mov bl,15
	int 10h
	ret
cleardisp:
	mov ax,0600h     	;这段代码是为了清屏
	mov bx,0700h
	mov cx,0
	mov dx,0184fh
	int 10h			;调用的BOIS的10号
	ret

_getmmap:
	push ds
	push es
	push ss
	mov esi,0
	mov dword[E80MAP_NR],esi
	mov dword[E80MAP_ADRADR],E80MAP_ADR  ;e820map结构体开始地址

	xor ebx,ebx
	mov edi,E80MAP_ADR                   ;此时edi是e820map开始地址，esi是结构数组元素个数
loop:
	mov eax,0e820h                       ;获取e820map结构参数
	mov ecx,20                           ;e820map结构大小
	mov edx,0534d4150h                   ;获取e820map结构参数必须是这个数据
	int 15h                              ;BIOS的15h中断
	jc .1

	add edi,20
	cmp edi,E80MAP_ADR+0x1000            ;0x1000用来作为一个地址值来限制循环的次数
	jg .1

	inc esi

	cmp ebx,0
	jne loop                              ;循环获取e820map结构

	jmp .2

.1:
	mov esi,0                              ;出错处理，e820map结构数组元素个数为0

.2:
	mov dword[E80MAP_NR],esi               ;e820map结构数组元素个数
	pop ss
	pop es
	pop ds
	ret
_read:
	push ds
	push es
	push ss
	xor eax,eax
	mov ah,0x42
	mov dl,0x80
	mov si,RWHDPACK_ADR
	int 0x13
	jc  .err
	pop ss
	pop es
	pop ds
	ret
.err:
	mov ax,int131errmsg
	call DispStr
	jmp $
	pop ss
	pop es
	pop ds
	ret

_getvbemode:
        push es
        push ax
        push di
        mov di,VBEINFO_ADR
        mov ax,0
        mov es,ax
        mov ax,0x4f00
        int 0x10
        cmp ax,0x004f
        jz .ok
        mov ax,getvbmodeerrmsg
        call DispStr
        jmp $
 .ok:
        pop di
        pop ax
        pop es
        ret
_getvbeonemodeinfo:
        push es
        push ax
        push di
        push cx
        mov di,VBEMINFO_ADR
        mov ax,0
        mov es,ax
        mov cx,0x118
        mov ax,0x4f01
        int 0x10
        cmp ax,0x004f
        jz .ok
        mov ax,getvbmodinfoeerrmsg
        call DispStr
        jmp $
 .ok:
        pop cx
        pop di
        pop ax
        pop es
        ret
_setvbemode:
        push ax
        push bx
        mov bx,0x4118
        mov ax,0x4f02
        int 0x10
        cmp ax,0x004f
        jz .ok
        mov ax,setvbmodeerrmsg
        call DispStr
        jmp $
 .ok:
        pop bx
        pop ax
        ret
disable_nmi:
	push ax
	in al, 0x70     ; port 0x70NMI_EN_PORT
	or al, 0x80	; disable all NMI source
	out 0x70,al	; NMI_EN_PORT
	pop ax
	ret

func_table:                    ;函数表
	dw _getmmap                ;获取内存布局视图的函数
	dw _read                   ;读取硬盘的函数
        dw _getvbemode         ;获取显卡VBE模式
        dw _getvbeonemodeinfo  ;获取显卡VBE模式的数据
        dw _setvbemode         ;设置显卡VBE模式


int131errmsg: db     "int 13 read hdsk  error"
        db 0
getvbmodeerrmsg: db     "get vbemode err"
        db 0
getvbmodinfoeerrmsg: db     "get vbemodeinfo err"
                db 0
setvbmodeerrmsg: db     "set vbemode err"
        db 0
