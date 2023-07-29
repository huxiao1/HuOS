###################################################################
# krnlbuidcmd自动化编译配置文件 Makefile #
# 彭东
###################################################################
ASM = nasm
CC = gcc
LD = ld
DD = dd
RM = rm
OBJCOPY = objcopy
OJCYFLAGS = -S -O binary
DEBUGLDFLAGS = -s
DEBUGFLAG =
OPTIMIXZED = -O2
BUILDTTEMP =
CCSSEINS = -mno-avx -mno-avx2 -mno-sse -mno-sse2 -mno-sse3 -mno-ssse3 -mno-sse4.1 -mno-sse4.2
LDRIMG = ./lmoskrlimg
ASMBFLAGS = -I $(HEADFILE_PATH) -f elf $(DEBUGFLAG)
ASMKFLAGS = -I $(HEADFILE_PATH) -f elf64 $(DEBUGFLAG) #-mregparm=0-finline-functions-mcmodel=medium -mcmodel=large
BTCFLAGS = -I $(HEADFILE_PATH) -c $(DEBUGFLAG) $(OPTIMIXZED) $(CCSSEINS) $(BUILDTTEMP) -std=c99 -m32 -Wall -Wshadow -W -Wconversion -Wno-sign-conversion -fno-stack-protector -fomit-frame-pointer -fno-builtin -fno-common -fno-ident -ffreestanding -fno-stack-protector -fomit-frame-pointer -Wno-unused-parameter -Wunused-variable
CPPFLGSLDS = $(HEADFILE_PATH) -E -P
CSCFLAGS = -I $(HEADFILE_PATH) -c -fno-builtin -fno-common -fno-stack-protector -fno-ident -ffreestanding
SCFLAGS = -I $(HEADFILE_PATH) -S -std=c99 -fno-ident -Wall -fno-builtin -fno-common -fno-stack-protector
LDFLAGS = $(DEBUGLDFLAGS) -static -T boot.lds -n -Map boot.map
FDLDRLDFLG = $(DEBUGLDFLAGS) -Ttext 0 -n -Map fdldr.map
LOADERLDFLAGS = $(DEBUGLDFLAGS) -T ldrld.lds -n -Map hdldr.map
LDRIMHLDFLAGS = $(DEBUGLDFLAGS) -T initldrimh.lds -n -Map initldrimh.map
LDRKRLLDFLAGS = $(DEBUGLDFLAGS) -T initldrkrl.lds -n -Map initldrkrl.map
LDRSVELDFLAGS = $(DEBUGLDFLAGS) -T initldrsve.lds -n -Map initldrsve.map
LDRIMGFLAGS = -m k
KERNELLDFLAGS = $(DEBUGLDFLAGS) -static -T kernelld.lds -n -Map kernel.map #--entry=_start#-Ttext 0x500 target remote localhost:1234
DBGKERNELLDFLAGS = -static -T kernelld.lds -n -Map kernel.map #--entry=_start#-Ttext 0x500 target remote localhost:1234
INITSHELLLDFLAGS = $(DEBUGLDFLAGS) -static -T app.lds -n -Map initshellldr.map
