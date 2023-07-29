###################################################################
# krnlbuidcmd自动化编译配置文件 Makefile #
# 彭东
###################################################################
ASM = nasm
CC = clang-11
LD = ld
DD = dd
RM = rm
DEBUGFLAG = -g
OPTIMIXZED = -O0
CCSSEINS = -mno-avx -mno-avx2 -mno-sse -mno-sse2 -mno-sse3 -mno-ssse3 -mno-sse4.1 -mno-sse4.2
OYFLAGS = -O binary
CFLAGS = $(HEADFILE_PATH) -c -m64 -march=x86-64 $(OPTIMIXZED) $(CCSSEINS) $(DEBUGFLAG) -mtune=generic -mcmodel=large -mno-red-zone -std=c11 -Wno-address-of-packed-member -fexec-charset=UTF-8 -Wall -Wshadow -W -Wconversion -Wno-sign-conversion -fno-stack-protector -fno-zero-initialized-in-bss -fomit-frame-pointer -fno-builtin -fno-common -fno-ident -ffreestanding -Wno-unused-parameter -Wunused-variable -save-temps #-fdata-sections -gstabs+
LDFLAGS = -s -static -T cosmoslink.lds -n -Map cosmos.map
CPPFLGSLDS = $(HEADFILE_PATH) -E -P
ASMFLGS = $(HEADFILE_PATH) -f elf64 $(DEBUGFLAG)
OBJCOPY = objcopy
OJCYFLAGS = -S -O binary
