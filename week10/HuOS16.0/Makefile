###################################################################
#		主控自动化编译配置文件 Makefile			  #
#				彭东  
###################################################################

MAKEFLAGS = -sR

MKDIR = mkdir
RMDIR = rmdir
CP = cp
CD = cd
DD = dd
RM = rm
LKIMG = ./lmoskrlimg -m k
VM = qemu-system-x86_64
DBUGVM = bochs -q
IMGTOVDI = qemu-img convert -f raw -O vdi
IMGTOVMDK = qemu-img convert -f raw -O vmdk
MAKE = make
X86BARD = -f ./Makefile.x86

VVMRLMOSFLGS = -C $(BUILD_PATH) -f vbox.mkf

VBOXVMFLGS = -C $(VM_PATH) -f vbox.mkf
VMFLAGES = -smp 4 -hda $(VMDKFNAME) -m 1024 #-enable-kvm #+sse,sse2,sse4.1,sse4.2,sse4a,ssse3
UEFIDEBUGVMFLAGES = -cpu SandyBridge -m 4096 -drive if=pflash,format=raw,file=$(OVMF_PATH)OVMF_CODE.fd,readonly=on -drive if=pflash,format=raw,file=$(OVMF_PATH)OVMF_VARS.fd,readonly=on -drive format=raw,file=fat:rw:$(OVMF_PATH)esp -net none 
UEFIVMFLAGES = -enable-kvm -cpu host -m 4096 -drive if=pflash,format=raw,file=$(OVMF_PATH)OVMF_CODE.fd,readonly=on -drive if=pflash,format=raw,file=$(OVMF_PATH)OVMF_VARS.fd,readonly=on -drive format=raw,file=fat:rw:$(OVMF_PATH)esp -net none 

IMGSECTNR = 204800
PHYDSK = /dev/sdb
VDIFNAME = hd.vdi
VMDKFNAME = hd.img
KRNLEXCIMG = Cosmos.bin

LOGOFILE = *.bmp
FONTFILE = font.fnt
APPSFILE = *.app
ELFSFILE = *.elf
DBUGFILE = *.debug
KERNEL_BUILD_PATH = ./build
EXKNL_PATH = ./exckrnl
OVMF_PATH = ./ovmf/
DSTPATH = ../exckrnl
#RELEDSTPATH = ../release
LEGACY_INITLDR_BUILD_PATH =./initldr/legacy/build/
UEFI_INITLDR_BUILD_PATH =./initldr/uefi/build/
#CPLILDR_PATH =./release/
LEGACY_RELEASE_PATH = ./release/legacy/
UEFI_RELEASE_PATH = ./release/uefi/
INSTALL_PATH =/boot/
INSTALLSRCFILE_PATH =./release/Cosmos.eki
SRCFILE = $(KRNLEXCIMG) $(APPSFILE) $(DBUGFILE) $(ELFSFILE)
RSRCFILE = $(KRNLEXCIMG) $(APPSFILE) $(DBUGFILE) $(ELFSFILE) #$(VDIFNAME) $(VMDKFNAME)

INITLDRIMH = initldrimh.bin
INITLDRKRL = initldrkrl.bin
INITLDRSVE = initldrsve.bin

LEGACY_CPLILDRSRC = $(LEGACY_INITLDR_BUILD_PATH)$(INITLDRSVE) $(LEGACY_INITLDR_BUILD_PATH)$(INITLDRKRL) $(LEGACY_INITLDR_BUILD_PATH)$(INITLDRIMH)
LEGACY_CPLILDRDEBUGSRC = $(LEGACY_INITLDR_BUILD_PATH)*.debug
LEGACY_CPLILDRELFSSRC = $(LEGACY_INITLDR_BUILD_PATH)*.elf
UEFI_CPLILDRSRC = $(UEFI_INITLDR_BUILD_PATH)$(INITLDRSVE) $(UEFI_INITLDR_BUILD_PATH)$(INITLDRKRL) $(UEFI_INITLDR_BUILD_PATH)$(INITLDRIMH)
UEFI_CPLILDRDEBUGSRC = $(UEFI_INITLDR_BUILD_PATH)*.debug
UEFI_CPLILDRELFSSRC = $(UEFI_INITLDR_BUILD_PATH)*.elf

LKIMG_INFILE = $(INITLDRSVE) $(INITLDRKRL) $(KRNLEXCIMG) $(FONTFILE) $(LOGOFILE) $(APPSFILE)
.PHONY : install x32 build print clean all krnlexc cpkrnl wrimg phymod exc vdi vdiexc cprelease release createimg

build: clean print all

all:
	$(MAKE) $(X86BARD)
clean:
	$(CD) $(KERNEL_BUILD_PATH); $(RM) -f *.o *.bin *.i *.krnl *.s *.map *.lib *.btoj *.vdi *.elf *vmdk *.lds *.mk *.mki krnlobjs.mh *.app *.bc *.dbg *.debug
	$(CD) $(EXKNL_PATH); $(RM) -f *.o *.bin *.i *.krnl *.s *.map *.lib *.btoj *.vdi *.elf *vmdk *.lds *.mk *.mki krnlobjs.mh *.app *.bc *.dbg *.debug
	$(CD) $(LEGACY_RELEASE_PATH); $(RM) -f *.o *.bin *.i *.krnl *.s *.eki *.map *.lib *.btoj *.elf *.vdi *vmdk *.lds *.mk *.mki krnlobjs.mh *.app *.dbg *.debug
	$(CD) $(UEFI_RELEASE_PATH); $(RM) -f *.o *.bin *.i *.krnl *.s *.eki *.map *.lib *.btoj *.elf *.vdi *vmdk *.lds *.mk *.mki krnlobjs.mh *.app *.dbg *.debug
	@echo '清理全部已构建文件... ^_^'

print:
	@echo '*********正在开始编译构建系统*************'


krnlexc: cpkrnl wrimg exc
dbugkrnl: cpkrnl wrimg dbugexc
vdiexc:  vdi
	$(MAKE) $(VBOXVMFLGS)
vdi:cpkrnl wrimg
	$(CD) $(EXKNL_PATH) && $(IMGTOVDI) $(DSKIMG) $(VDIFNAME)
	$(CD) $(EXKNL_PATH) && $(IMGTOVMDK) $(DSKIMG) $(VMDKFNAME)
cplmildr:
	$(CP) $(CPFLAGES) $(LEGACY_CPLILDRSRC) $(LEGACY_RELEASE_PATH)
	$(CP) $(CPFLAGES) $(LEGACY_CPLILDRDEBUGSRC) $(LEGACY_RELEASE_PATH)
	$(CP) $(CPFLAGES) $(LEGACY_CPLILDRELFSSRC) $(LEGACY_RELEASE_PATH)
	$(CP) $(CPFLAGES) $(UEFI_CPLILDRSRC) $(UEFI_RELEASE_PATH)
	$(CP) $(CPFLAGES) $(UEFI_CPLILDRDEBUGSRC) $(UEFI_RELEASE_PATH)
	$(CP) $(CPFLAGES) $(UEFI_CPLILDRELFSSRC) $(UEFI_RELEASE_PATH)

cpkrnl:
	$(CD) $(KERNEL_BUILD_PATH) && $(CP) $(CPFLAGES) $(SRCFILE) $(DSTPATH)
	$(CD) $(KERNEL_BUILD_PATH) && $(CP) $(CPFLAGES) $(SRCFILE) .$(LEGACY_RELEASE_PATH)
	$(CD) $(KERNEL_BUILD_PATH) && $(CP) $(CPFLAGES) $(SRCFILE) .$(UEFI_RELEASE_PATH)
exc:
	$(CD) $(EXKNL_PATH) && $(VM) $(VMFLAGES)
dbugexc:
	$(CD) $(EXKNL_PATH) && $(DBUGVM)

KIMG:
	@echo '正在生成Legcy Cosmos内核映像文件，请稍后……'
	$(CD) $(LEGACY_RELEASE_PATH) && $(LKIMG) -lhf $(INITLDRIMH) -o Cosmos.eki -f $(LKIMG_INFILE)
	@echo '正在生成UEFI Cosmos内核映像文件，请稍后……'
	$(CD) $(UEFI_RELEASE_PATH) && $(LKIMG) -lhf $(INITLDRIMH) -o Cosmos.eki -f $(LKIMG_INFILE)

KVMRUN:
	$(VM) $(VMFLAGES)
#$(MAKE) $(KVMRLMOSFLGS)
UEFI_DBG:
	$(CD) $(UEFI_RELEASE_PATH) && $(CP) $(CPFLAGES) Cosmos.eki ../.$(OVMF_PATH)esp/Kernel/ 

UEFI_RUN:
	$(CD) $(UEFI_RELEASE_PATH) && $(CP) $(CPFLAGES) Cosmos.eki ../.$(OVMF_PATH)esp/Kernel/ 
	sudo $(VM) $(UEFIVMFLAGES)

VBOXRUN:
	$(MAKE) $(VVMRLMOSFLGS)
VBOXDBG:
	$(MAKE) $(VVMRLMOSFLGS) dbg

release: clean all cplmildr cpkrnl KIMG 

kvmtest: release KVMRUN
uefitest: release UEFI_RUN
uefidebug: release UEFI_DBG
vboxtest: release VBOXRUN
vboxdbug: release VBOXDBG


createimg:
	$(DD) bs=512 if=/dev/zero of=$(DSKIMG) count=$(IMGSECTNR)

install:
	@echo '下面将开始安装Cosmos内核，请确认……'
	sudo $(CP) $(CPFLAGES) $(INSTALLSRCFILE_PATH) $(INSTALL_PATH)
	@echo 'Cosmos内核已经安装完成，请重启计算机……'
