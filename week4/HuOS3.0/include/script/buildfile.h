#ifndef BUILDFILE_H
#define BUILDFILE_H
#include "config.h"
#ifdef CFG_X86_PLATFORM

#define BUILD_HALY_OBJS init_entry.o hal_start.o halinit.o halglobal.o\
                        halplatform.o bdvideo.o halcpuctrl.o halprint.o\
                        halmm.o halintupt.o kernel.o i8259.o halgdtidt.o
#define BUILD_KRNL_OBJS krlinit.o
#define BUILD_MEMY_OBJS
#define BUILD_FSYS_OBJS
#define BUILD_DRIV_OBJS
#define BUILD_LIBS_OBJS
#define BUILD_TASK_OBJS

#define BUILD_LINK_OBJS BUILD_HALY_OBJS\
                        BUILD_KRNL_OBJS BUILD_MEMY_OBJS\
                        BUILD_FSYS_OBJS BUILD_DRIV_OBJS\
                        BUILD_LIBS_OBJS BUILD_TASK_OBJS
#define LINKR_IPUT_FILE BUILD_LINK_OBJS
#define LINKR_OPUT_FILE Cosmos.elf
#define KERNL_ELFF_FILE LINKR_OPUT_FILE
#define KERNL_BINF_FILE HuOS.bin

#endif
                          
#endif // BUILDFILE_H
