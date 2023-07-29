###################################################################
#		krnlbuidcmd自动化编译配置文件 Makefile		  #
#				彭东  
###################################################################

#include "config.h"
#ifdef CFG_X86_PLATFORM
ASM 	= nasm
CC		= gcc
LD		= ld
DD		= dd
RM		= rm

#ifdef CFG_DEBUG_BUILD
DEBUGFLAG = -g
DEBUGLDFLAG =
OPTIMIXZED = -O0
BUILDTTEMP = -save-temps
#else
DEBUGFLAG = 
DEBUGLDFLAG = -s
OPTIMIXZED = -O2
BUILDTTEMP =
#endif


CCSSEINS = -mno-avx -mno-avx2 -mno-sse -mno-sse2 -mno-sse3 -mno-ssse3 -mno-sse4.1 -mno-sse4.2 
OYFLAGS		= -O binary
CFLAGS      = $(HEADFILE_PATH) -c -m64 -march=x86-64 $(OPTIMIXZED) $(CCSSEINS) $(DEBUGFLAG) $(BUILDTTEMP) -mtune=generic -mcmodel=large -mno-red-zone -std=c11 -Wno-address-of-packed-member -fexec-charset=UTF-8 -Wall -Wshadow  -W -Wconversion -Wno-sign-conversion -fno-stack-protector -fno-zero-initialized-in-bss -fomit-frame-pointer -fno-builtin -fno-common  -fno-ident -ffreestanding  -Wno-unused-parameter -Wunused-variable #-fdata-sections -gstabs+
LDFLAGS		= $(DEBUGLDFLAG) -static -T cosmoslink.lds -n -Map cosmos.map
CPPFLGSLDS	= $(HEADFILE_PATH) -E -P 
ASMFLGS		= $(HEADFILE_PATH) -f elf64 $(DEBUGFLAG)
OBJCOPY = objcopy
OJCYFLAGS = -S -O binary
#endif

