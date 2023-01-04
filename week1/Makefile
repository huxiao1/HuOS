MAKEFLAGS = -sR
MKDIR = mkdir
RMDIR = rmdir
CP = cp
CD = cd
DD = dd
RM = rm

ASM		= nasm
CC		= gcc
LD		= ld
OBJCOPY	= objcopy

ASMBFLAGS	= -f elf -w-orphan-labels
CFLAGS		= -c -Os -std=c99 -m32 -Wall -Wshadow -W -Wconversion -Wno-sign-conversion  -fno-stack-protector -fomit-frame-pointer -fno-builtin -fno-common  -ffreestanding  -Wno-unused-parameter -Wunused-variable
LDFLAGS		= -s -static -T hello.lds -n -Map HelloOS.map 
OJCYFLAGS	= -S -O binary

HELLOOS_OBJS :=
HELLOOS_OBJS += entry.o main.o vgastr.o
HELLOOS_ELF = HuOS.elf
HELLOOS_BIN = HuOS.bin

.PHONY : build clean all link bin

#The all target depends on the clean, build, link, and bin targets
#which means that running make all will first clean any existing build artifacts, then build the project, link the object files, and generate the final binary image.
all: clean build link bin

clean:
	$(RM) -f *.o *.bin *.elf

#The build target depends on the object files (entry.o, main.o, vgastr.o), which means that running make build will build these object files.
build: $(HELLOOS_OBJS)

# entry.o main.o vgastr.o => HuOS.elf => HuOS.bin
# The link target depends on the HELLOOS_ELF file. 
# This means that the HELLOOS_ELF file must be up-to-date (either already built or marked as a target to be built) before the link target can be considered up-to-date.
link: $(HELLOOS_ELF)
$(HELLOOS_ELF): $(HELLOOS_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(HELLOOS_OBJS)
bin: $(HELLOOS_BIN)
$(HELLOOS_BIN): $(HELLOOS_ELF)
	$(OBJCOPY) $(OJCYFLAGS) $< $@

# *.c/.asm =>entry.o main.o vgastr.o
%.o : %.asm
	$(ASM) $(ASMBFLAGS) -o $@ $<
%.o : %.c
	$(CC) $(CFLAGS) -o $@ $<