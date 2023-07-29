###################################################################
#		主控自动化编译配置文件 Makefile
#				彭东  
###################################################################

MAKEFLAGS = -sR

MKDIR = mkdir
RMDIR = rmdir
CP = cp
CD = cd
DD = dd
RM = rm
PREMENTMFLGS = -C $(BUILD_PATH) -f pretreatment.mkf
LEGABUILD_PATH = ./legacy/ 
UEFIBUILD_PATH = ./uefi/
MAKE = make
LEGACYMFLGS = -C $(LEGABUILD_PATH) -f Makefile
UEFIMFLGS = -C $(UEFIBUILD_PATH) -f Makefile 
DSTPATH = ../exckrnl
RELEDSTPATH = ../release
SRCFILE = $(BOOTEXCIMG) $(KRNLEXCIMG) $(LDEREXCIMG) $(SHELEXCIMG)
RSRCFILE = $(BOOTEXCIMG) $(KRNLEXCIMG) $(LDEREXCIMG) $(SHELEXCIMG) $(VDIFNAME) $(VMDKFNAME)
.PHONY : all build print clean legacy uefi

all: clean print build

build: legacy uefi
legacy:
	$(MAKE) $(LEGACYMFLGS)
	@echo '恭喜我，Legacy引导编译构建完成！ ^_^'
uefi:
	$(MAKE) $(UEFIMFLGS)
	@echo '恭喜我，UEFI引导编译构建完成！ ^_^'
clean:
	$(CD) $(LEGABUILD_PATH) && $(MAKE) clean
	$(CD) $(UEFIBUILD_PATH) && $(MAKE) clean
	

print:
	@echo '*********正在开始编译构建引导初始化组件*************'