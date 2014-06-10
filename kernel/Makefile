VERSION = 2
PATCHLEVEL = 6
SUBLEVEL = 32
EXTRAVERSION = .24
NAME = FeraKernel
MAKEFLAGS += -rR --no-print-directory

ifdef V
  ifeq ("$(origin V)", "command line")
    KBUILD_VERBOSE = $(V)
  endif
endif
ifndef KBUILD_VERBOSE
  KBUILD_VERBOSE = 0
endif

ifdef C
  ifeq ("$(origin C)", "command line")
    KBUILD_CHECKSRC = $(C)
  endif
endif
ifndef KBUILD_CHECKSRC
  KBUILD_CHECKSRC = 0
endif

ifdef SUBDIRS
  KBUILD_EXTMOD ?= $(SUBDIRS)
endif
ifdef M
  ifeq ("$(origin M)", "command line")
    KBUILD_EXTMOD := $(M)
  endif
endif

ifeq ($(KBUILD_SRC),)

ifdef O
  ifeq ("$(origin O)", "command line")
    KBUILD_OUTPUT := $(O)
  endif
endif

PHONY := _all
_all:

$(CURDIR)/Makefile Makefile: ;

ifneq ($(KBUILD_OUTPUT),)
saved-output := $(KBUILD_OUTPUT)
KBUILD_OUTPUT := $(shell cd $(KBUILD_OUTPUT) && /bin/pwd)
$(if $(KBUILD_OUTPUT),, \
     $(error output directory "$(saved-output)" does not exist))

PHONY += $(MAKECMDGOALS) sub-make

$(filter-out _all sub-make $(CURDIR)/Makefile, $(MAKECMDGOALS)) _all: sub-make
	$(Q)@:

sub-make: FORCE
	$(if $(KBUILD_VERBOSE:1=),@)$(MAKE) -C $(KBUILD_OUTPUT) \
	KBUILD_SRC=$(CURDIR) \
	KBUILD_EXTMOD="$(KBUILD_EXTMOD)" -f $(CURDIR)/Makefile \
	$(filter-out _all sub-make,$(MAKECMDGOALS))

skip-makefile := 1
endif
endif

ifeq ($(skip-makefile),)
PHONY += all
ifeq ($(KBUILD_EXTMOD),)
_all: all
else
_all: modules
endif

srctree		:= $(if $(KBUILD_SRC),$(KBUILD_SRC),$(CURDIR))
TOPDIR		:= $(srctree)
objtree		:= $(CURDIR)
src		:= $(srctree)
obj		:= $(objtree)
VPATH		:= $(srctree)$(if $(KBUILD_EXTMOD),:$(KBUILD_EXTMOD))
export srctree objtree VPATH TOPDIR

SUBARCH := $(shell uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc64/ \
				  -e s/arm.*/arm/ -e s/sa110/arm/ \
				  -e s/s390x/s390/ -e s/parisc64/parisc/ \
				  -e s/ppc.*/powerpc/ -e s/mips.*/mips/ \
				  -e s/sh.*/sh/ )

export KBUILD_BUILDHOST := $(SUBARCH)
ARCH		?= $(SUBARCH)
CROSS_COMPILE	?=
UTS_MACHINE 	:= $(ARCH)
SRCARCH 	:= $(ARCH)

ifeq ($(ARCH),i386)
        SRCARCH := x86
endif
ifeq ($(ARCH),x86_64)
        SRCARCH := x86
endif

ifeq ($(ARCH),sparc64)
       SRCARCH := sparc
endif

hdr-arch  := $(SRCARCH)

ifeq ($(ARCH),m68knommu)
       hdr-arch  := m68k
endif

KCONFIG_CONFIG	?= .config

CONFIG_SHELL := $(shell if [ -x "$$BASH" ]; then echo $$BASH; \
	  else if [ -x /bin/bash ]; then echo /bin/bash; \
	  else echo sh; fi ; fi)

HOSTCC       = gcc
HOSTCXX      = g++
HOSTCFLAGS   = -Wall -Wstrict-prototypes -Ofast -fomit-frame-pointer
HOSTCXXFLAGS = -Ofast
KBUILD_MODULES :=
KBUILD_BUILTIN := 1

ifeq ($(MAKECMDGOALS),modules)
  KBUILD_BUILTIN := $(if $(CONFIG_MODVERSIONS),1)
endif

ifneq ($(filter all _all modules,$(MAKECMDGOALS)),)
  KBUILD_MODULES := 1
endif

ifeq ($(MAKECMDGOALS),)
  KBUILD_MODULES := 1
endif

export KBUILD_MODULES KBUILD_BUILTIN
export KBUILD_CHECKSRC KBUILD_SRC KBUILD_EXTMOD

ifeq ($(KBUILD_VERBOSE),1)
  quiet =
  Q =
else
  quiet=quiet_
  Q = @
endif

ifneq ($(findstring s,$(MAKEFLAGS)),)
  quiet=silent_
endif

export quiet Q KBUILD_VERBOSE

MAKEFLAGS += --include-dir=$(srctree)

$(srctree)/scripts/Kbuild.include: ;
include $(srctree)/scripts/Kbuild.include

AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CC) -E
AR		= $(CROSS_COMPILE)ar
NM		= $(CROSS_COMPILE)nm
STRIP		= $(CROSS_COMPILE)strip
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump
AWK		= awk
GENKSYMS	= scripts/genksyms/genksyms
DEPMOD		= /sbin/depmod
KALLSYMS	= scripts/kallsyms
PERL		= perl
CHECK		= sparse
CHECKFLAGS     := -D__linux__ -Dlinux -D__STDC__ -Dunix -D__unix__ \
		  -Wbitwise -Wno-return-void $(CF)
MODFLAGS	= -DMODULE
CFLAGS_MODULE   = $(MODFLAGS) -fno-pic
AFLAGS_MODULE   = $(MODFLAGS)
LDFLAGS_MODULE  =
CFLAGS_KERNEL	= -ftree-vectorize -ffast-math -fsingle-precision-constant -mfpu=neon
AFLAGS_KERNEL	= -ftree-vectorize -ffast-math -fsingle-precision-constant -mfpu=neon

LINUXINCLUDE    := -Iinclude \
                   $(if $(KBUILD_SRC),-Iinclude2 -I$(srctree)/include) \
                   -I$(srctree)/arch/$(hdr-arch)/include               \
                   -include include/linux/autoconf.h
KBUILD_CPPFLAGS := -D__KERNEL__

KBUILD_CFLAGS := -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs \
	-fno-strict-aliasing -fno-common -mno-unaligned-access \
	-Wno-unused-but-set-variable -Wno-uninitialized \
	-fno-delete-null-pointer-checks -ftree-vectorize \
	-fivopts -fsched-spec-load -fgraphite-identity \
	-ftree-loop-distribution -Wno-unused-variable -fsingle-precision-constant \
	-march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp

KBUILD_AFLAGS   := -D__ASSEMBLY__
KERNELRELEASE = $(shell cat include/config/kernel.release 2> /dev/null)
KERNELVERSION = $(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION)

export VERSION PATCHLEVEL SUBLEVEL KERNELRELEASE KERNELVERSION
export ARCH SRCARCH CONFIG_SHELL HOSTCC HOSTCFLAGS CROSS_COMPILE AS LD CC
export CPP AR NM STRIP OBJCOPY OBJDUMP MAKE AWK GENKSYMS PERL UTS_MACHINE
export HOSTCXX HOSTCXXFLAGS LDFLAGS_MODULE CHECK CHECKFLAGS
export KBUILD_CPPFLAGS NOSTDINC_FLAGS LINUXINCLUDE OBJCOPYFLAGS LDFLAGS
export KBUILD_CFLAGS CFLAGS_KERNEL CFLAGS_MODULE
export KBUILD_AFLAGS AFLAGS_KERNEL AFLAGS_MODULE
export MODVERDIR := $(if $(KBUILD_EXTMOD),$(firstword $(KBUILD_EXTMOD))/).tmp_versions

RCS_FIND_IGNORE := \( -name SCCS -o -name BitKeeper -o -name .svn -o -name CVS -o -name .pc -o -name .hg -o -name .git \) -prune -o
export RCS_TAR_IGNORE := --exclude SCCS --exclude BitKeeper --exclude .svn --exclude CVS --exclude .pc --exclude .hg --exclude .git

PHONY += scripts_basic
scripts_basic:
	$(Q)$(MAKE) $(build)=scripts/basic

scripts/basic/%: scripts_basic ;

PHONY += outputmakefile

outputmakefile:
ifneq ($(KBUILD_SRC),)
	$(Q)ln -fsn $(srctree) source
	$(Q)$(CONFIG_SHELL) $(srctree)/scripts/mkmakefile \
	    $(srctree) $(objtree) $(VERSION) $(PATCHLEVEL)
endif

no-dot-config-targets := clean mrproper distclean \
			 cscope TAGS tags help %docs check% \
			 include/linux/version.h headers_% \
			 kernelrelease kernelversion

config-targets := 0
mixed-targets  := 0
dot-config     := 1

ifneq ($(filter $(no-dot-config-targets), $(MAKECMDGOALS)),)
	ifeq ($(filter-out $(no-dot-config-targets), $(MAKECMDGOALS)),)
		dot-config := 0
	endif
endif

ifeq ($(KBUILD_EXTMOD),)
        ifneq ($(filter config %config,$(MAKECMDGOALS)),)
                config-targets := 1
                ifneq ($(filter-out config %config,$(MAKECMDGOALS)),)
                        mixed-targets := 1
                endif
        endif
endif

ifeq ($(mixed-targets),1)

%:: FORCE
	$(Q)$(MAKE) -C $(srctree) KBUILD_SRC= $@

else
ifeq ($(config-targets),1)

include $(srctree)/arch/$(SRCARCH)/Makefile
export KBUILD_DEFCONFIG KBUILD_KCONFIG

config: scripts_basic outputmakefile FORCE
	$(Q)mkdir -p include/linux include/config
	$(Q)$(MAKE) $(build)=scripts/kconfig $@

%config: scripts_basic outputmakefile FORCE
	$(Q)mkdir -p include/linux include/config
	$(Q)$(MAKE) $(build)=scripts/kconfig $@

else

ifeq ($(KBUILD_EXTMOD),)
PHONY += scripts
scripts: scripts_basic include/config/auto.conf
	$(Q)$(MAKE) $(build)=$(@)

init-y		:= init/
drivers-y	:= drivers/ sound/ firmware/
net-y		:= net/
libs-y		:= lib/
core-y		:= usr/
endif

ifeq ($(dot-config),1)
-include include/config/auto.conf

ifeq ($(KBUILD_EXTMOD),)
-include include/config/auto.conf.cmd

$(KCONFIG_CONFIG) include/config/auto.conf.cmd: ;

include/config/auto.conf: $(KCONFIG_CONFIG) include/config/auto.conf.cmd
	$(Q)$(MAKE) -f $(srctree)/Makefile silentoldconfig
else
PHONY += include/config/auto.conf

include/config/auto.conf:
	$(Q)test -e include/linux/autoconf.h -a -e $@ || (		\
	echo;								\
	echo "  ERROR: Kernel configuration is invalid.";		\
	echo "         include/linux/autoconf.h or $@ are missing.";	\
	echo "         Run 'make oldconfig && make prepare' on kernel src to fix it.";	\
	echo;								\
	/bin/false)

endif

else
include/config/auto.conf: ;
endif

all: vmlinux

KBUILD_CFLAGS += -Ofast -Wno-address
KBUILD_CFLAGS += $(call cc-disable-warning,maybe-uninitialized) -fno-inline-functions
KBUILD_CFLAGS += $(call cc-disable-warning,array-bounds)
include $(srctree)/arch/$(SRCARCH)/Makefile
KBUILD_CFLAGS += $(call cc-option, -fno-stack-protector)

ifdef CONFIG_FRAME_POINTER
KBUILD_CFLAGS	+= -fno-omit-frame-pointer -fno-optimize-sibling-calls
else
KBUILD_CFLAGS	+= -fomit-frame-pointer
endif

ifdef CONFIG_DEBUG_INFO
KBUILD_CFLAGS	+= -g
KBUILD_AFLAGS	+= -gdwarf-2
endif

ifdef CONFIG_FUNCTION_TRACER
KBUILD_CFLAGS	+= -pg
endif

ifdef CONFIG_DEBUG_SECTION_MISMATCH
KBUILD_CFLAGS += $(call cc-option, -fno-inline-functions-called-once)
endif

NOSTDINC_FLAGS += -nostdinc -isystem $(shell $(CC) -print-file-name=include)
CHECKFLAGS     += $(NOSTDINC_FLAGS)

KBUILD_CFLAGS += $(call cc-option,-Wdeclaration-after-statement,)

KBUILD_CFLAGS += $(call cc-option,-Wno-pointer-sign,)

KBUILD_CFLAGS	+= $(call cc-option,-fwrapv)

KBUILD_CFLAGS	+= $(call cc-option,-fno-dwarf2-cfi-asm)

warn-assign = \
$(warning "WARNING: Appending $$K$(1) ($(K$(1))) from $(origin K$(1)) to kernel $$$(1)")

ifneq ($(KCPPFLAGS),)
        $(call warn-assign,CPPFLAGS)
        KBUILD_CPPFLAGS += $(KCPPFLAGS)
endif
ifneq ($(KAFLAGS),)
        $(call warn-assign,AFLAGS)
        KBUILD_AFLAGS += $(KAFLAGS)
endif
ifneq ($(KCFLAGS),)
        $(call warn-assign,CFLAGS)
        KBUILD_CFLAGS += $(KCFLAGS)
endif

LDFLAGS_BUILD_ID = $(patsubst -Wl$(comma)%,%,\
			      $(call ld-option, -Wl$(comma)--build-id,))
LDFLAGS_MODULE += $(LDFLAGS_BUILD_ID)
LDFLAGS_vmlinux += $(LDFLAGS_BUILD_ID)

export KBUILD_IMAGE ?= vmlinux

export	INSTALL_PATH ?= /boot

MODLIB	= $(INSTALL_MOD_PATH)/lib/modules/$(KERNELRELEASE)
export MODLIB

ifdef INSTALL_MOD_STRIP
ifeq ($(INSTALL_MOD_STRIP),1)
mod_strip_cmd = $(STRIP) --strip-debug
else
mod_strip_cmd = $(STRIP) $(INSTALL_MOD_STRIP)
endif
else
mod_strip_cmd = true
endif
export mod_strip_cmd

ifeq ($(KBUILD_EXTMOD),)
core-y		+= kernel/ mm/ fs/ ipc/ security/ crypto/ block/

vmlinux-dirs	:= $(patsubst %/,%,$(filter %/, $(init-y) $(init-m) \
		     $(core-y) $(core-m) $(drivers-y) $(drivers-m) \
		     $(net-y) $(net-m) $(libs-y) $(libs-m)))

vmlinux-alldirs	:= $(sort $(vmlinux-dirs) $(patsubst %/,%,$(filter %/, \
		     $(init-n) $(init-) \
		     $(core-n) $(core-) $(drivers-n) $(drivers-) \
		     $(net-n)  $(net-)  $(libs-n)    $(libs-))))

init-y		:= $(patsubst %/, %/built-in.o, $(init-y))
core-y		:= $(patsubst %/, %/built-in.o, $(core-y))
drivers-y	:= $(patsubst %/, %/built-in.o, $(drivers-y))
net-y		:= $(patsubst %/, %/built-in.o, $(net-y))
libs-y1		:= $(patsubst %/, %/lib.a, $(libs-y))
libs-y2		:= $(patsubst %/, %/built-in.o, $(libs-y))
libs-y		:= $(libs-y1) $(libs-y2)

vmlinux-init := $(head-y) $(init-y)
vmlinux-main := $(core-y) $(libs-y) $(drivers-y) $(net-y)
vmlinux-all  := $(vmlinux-init) $(vmlinux-main)
vmlinux-lds  := arch/$(SRCARCH)/kernel/vmlinux.lds
export KBUILD_VMLINUX_OBJS := $(vmlinux-all)

quiet_cmd_vmlinux__ ?= LD      $@
      cmd_vmlinux__ ?= $(LD) $(LDFLAGS) $(LDFLAGS_vmlinux) -o $@ \
      -T $(vmlinux-lds) $(vmlinux-init)                          \
      --start-group $(vmlinux-main) --end-group                  \
      $(filter-out $(vmlinux-lds) $(vmlinux-init) $(vmlinux-main) vmlinux.o FORCE ,$^)

quiet_cmd_vmlinux_version = GEN     .version
      cmd_vmlinux_version = set -e;                     \
	if [ ! -r .version ]; then			\
	  rm -f .version;				\
	  echo 1 >.version;				\
	else						\
	  mv .version .old_version;			\
	  expr 0$$(cat .old_version) + 1 >.version;	\
	fi;						\
	$(MAKE) $(build)=init

quiet_cmd_sysmap = SYSMAP
      cmd_sysmap = $(CONFIG_SHELL) $(srctree)/scripts/mksysmap

define rule_vmlinux__
	:
	$(if $(CONFIG_KALLSYMS),,+$(call cmd,vmlinux_version))

	$(call cmd,vmlinux__)
	$(Q)echo 'cmd_$@ := $(cmd_vmlinux__)' > $(@D)/.$(@F).cmd

	$(Q)$(if $($(quiet)cmd_sysmap),                                      \
	  echo '  $($(quiet)cmd_sysmap)  System.map' &&)                     \
	$(cmd_sysmap) $@ System.map;                                         \
	if [ $$? -ne 0 ]; then                                               \
		rm -f $@;                                                    \
		/bin/false;                                                  \
	fi;
	$(verify_kallsyms)
endef


ifdef CONFIG_KALLSYMS

ifdef CONFIG_KALLSYMS_EXTRA_PASS
last_kallsyms := 3
else
last_kallsyms := 2
endif

kallsyms.o := .tmp_kallsyms$(last_kallsyms).o

define verify_kallsyms
	$(Q)$(if $($(quiet)cmd_sysmap),                                      \
	  echo '  $($(quiet)cmd_sysmap)  .tmp_System.map' &&)                \
	  $(cmd_sysmap) .tmp_vmlinux$(last_kallsyms) .tmp_System.map
	$(Q)cmp -s System.map .tmp_System.map ||                             \
		(echo Inconsistent kallsyms data;                            \
		 echo Try setting CONFIG_KALLSYMS_EXTRA_PASS;                \
		 rm .tmp_kallsyms* ; /bin/false )
endef

cmd_ksym_ld = $(cmd_vmlinux__)
define rule_ksym_ld
	: 
	+$(call cmd,vmlinux_version)
	$(call cmd,vmlinux__)
	$(Q)echo 'cmd_$@ := $(cmd_vmlinux__)' > $(@D)/.$(@F).cmd
endef

quiet_cmd_kallsyms = KSYM    $@
      cmd_kallsyms = $(NM) -n $< | $(KALLSYMS) \
                     $(if $(CONFIG_KALLSYMS_ALL),--all-symbols) > $@

.tmp_kallsyms1.o .tmp_kallsyms2.o .tmp_kallsyms3.o: %.o: %.S scripts FORCE
	$(call if_changed_dep,as_o_S)

.tmp_kallsyms%.S: .tmp_vmlinux% $(KALLSYMS)
	$(call cmd,kallsyms)

.tmp_vmlinux1: $(vmlinux-lds) $(vmlinux-all) FORCE
	$(call if_changed_rule,ksym_ld)

.tmp_vmlinux2: $(vmlinux-lds) $(vmlinux-all) .tmp_kallsyms1.o FORCE
	$(call if_changed,vmlinux__)

.tmp_vmlinux3: $(vmlinux-lds) $(vmlinux-all) .tmp_kallsyms2.o FORCE
	$(call if_changed,vmlinux__)

$(KALLSYMS): scripts ;

debug_kallsyms: .tmp_map$(last_kallsyms)

.tmp_map%: .tmp_vmlinux% FORCE
	($(OBJDUMP) -h $< | $(AWK) '/^ +[0-9]/{print $$4 " 0 " $$2}'; $(NM) $<) | sort > $@

.tmp_map3: .tmp_map2

.tmp_map2: .tmp_map1

endif

quiet_cmd_vmlinux-modpost = LD      $@
      cmd_vmlinux-modpost = $(LD) $(LDFLAGS) -r -o $@                          \
	 $(vmlinux-init) --start-group $(vmlinux-main) --end-group             \
	 $(filter-out $(vmlinux-init) $(vmlinux-main) FORCE ,$^)
define rule_vmlinux-modpost
	:
	+$(call cmd,vmlinux-modpost)
	$(Q)$(MAKE) -f $(srctree)/scripts/Makefile.modpost $@
	$(Q)echo 'cmd_$@ := $(cmd_vmlinux-modpost)' > $(dot-target).cmd
endef

vmlinux: $(vmlinux-lds) $(vmlinux-init) $(vmlinux-main) vmlinux.o $(kallsyms.o) FORCE
ifdef CONFIG_HEADERS_CHECK
	$(Q)$(MAKE) -f $(srctree)/Makefile headers_check
endif
ifdef CONFIG_SAMPLES
	$(Q)$(MAKE) $(build)=samples
endif
ifdef CONFIG_BUILD_DOCSRC
	$(Q)$(MAKE) $(build)=Documentation
endif
	$(call vmlinux-modpost)
	$(call if_changed_rule,vmlinux__)
	$(Q)rm -f .old_version

ifdef CONFIG_KALLSYMS
.tmp_vmlinux1: vmlinux.o
endif

modpost-init := $(filter-out init/built-in.o, $(vmlinux-init))
vmlinux.o: $(modpost-init) $(vmlinux-main) FORCE
	$(call if_changed_rule,vmlinux-modpost)

$(sort $(vmlinux-init) $(vmlinux-main)) $(vmlinux-lds): $(vmlinux-dirs) ;

PHONY += $(vmlinux-dirs)
$(vmlinux-dirs): prepare scripts
	$(Q)$(MAKE) $(build)=$@

pattern = ".*/localversion[^~]*"
string  = $(shell cat /dev/null \
	   `find $(objtree) $(srctree) -maxdepth 1 -regex $(pattern) | sort -u`)

localver = $(subst $(space),, $(string) \
			      $(patsubst "%",%,$(CONFIG_LOCALVERSION)))

ifdef CONFIG_LOCALVERSION_AUTO

ifeq ($(wildcard .scmversion),)
        _localver-auto = $(shell $(CONFIG_SHELL) \
                         $(srctree)/scripts/setlocalversion $(srctree))
else
        _localver-auto = $(shell cat .scmversion 2> /dev/null)
endif

	localver-auto  = $(LOCALVERSION)$(_localver-auto)
endif

localver-full = $(localver)$(localver-auto)

kernelrelease = $(KERNELVERSION)$(localver-full)
include/config/kernel.release: include/config/auto.conf FORCE
	$(Q)rm -f $@
	$(Q)echo $(kernelrelease) > $@

PHONY += prepare archprepare prepare0 prepare1 prepare2 prepare3

prepare3: include/config/kernel.release
ifneq ($(KBUILD_SRC),)
	@$(kecho) '  Using $(srctree) as source for kernel'
	$(Q)if [ -f $(srctree)/.config -o -d $(srctree)/include/config ]; then \
		echo "  $(srctree) is not clean, please run 'make mrproper'";\
		echo "  in the '$(srctree)' directory.";\
		/bin/false; \
	fi;
	$(Q)if [ ! -d include2 ]; then                                  \
	    mkdir -p include2;                                          \
	    ln -fsn $(srctree)/include/asm-$(SRCARCH) include2/asm;     \
	fi
endif

prepare2: prepare3 outputmakefile

prepare1: prepare2 include/linux/version.h include/linux/utsrelease.h \
                   include/asm include/config/auto.conf
	$(cmd_crmodverdir)

archprepare: prepare1 scripts_basic

prepare0: archprepare FORCE
	$(Q)$(MAKE) $(build)=.
	$(Q)$(MAKE) $(build)=. missing-syscalls

prepare: prepare0

export CPPFLAGS_vmlinux.lds += -P -C -U$(ARCH)

define check-symlink
	set -e;                                                            \
	if [ -L include/asm ]; then                                        \
		asmlink=`readlink include/asm | cut -d '-' -f 2`;          \
		if [ "$$asmlink" != "$(SRCARCH)" ]; then                   \
			echo "ERROR: the symlink $@ points to asm-$$asmlink but asm-$(SRCARCH) was expected"; \
			echo "       set ARCH or save .config and run 'make mrproper' to fix it";             \
			exit 1;                                            \
		fi;                                                        \
		test -e $$asmlink || rm include/asm;                       \
	elif [ -d include/asm ]; then                                      \
		echo "ERROR: $@ is a directory but a symlink was expected";\
		exit 1;                                                    \
	fi
endef

define create-symlink
	if [ ! -L include/asm ]; then                                      \
			$(kecho) '  SYMLINK $@ -> include/asm-$(SRCARCH)'; \
			if [ ! -d include/asm-$(SRCARCH) ]; then           \
				mkdir -p include/asm-$(SRCARCH);           \
			fi;                                                \
			ln -fsn asm-$(SRCARCH) $@;                         \
	fi
endef

include/asm: FORCE
	$(Q)$(check-symlink)
	$(Q)$(create-symlink)

uts_len := 64
define filechk_utsrelease.h
	if [ `echo -n "$(KERNELRELEASE)" | wc -c ` -gt $(uts_len) ]; then \
	  echo '"$(KERNELRELEASE)" exceeds $(uts_len) characters' >&2;    \
	  exit 1;                                                         \
	fi;                                                               \
	(echo \#define UTS_RELEASE \"$(KERNELRELEASE)\";)
endef

define filechk_version.h
	(echo \#define LINUX_VERSION_CODE $(shell                             \
	expr $(VERSION) \* 65536 + $(PATCHLEVEL) \* 256 + $(SUBLEVEL));     \
	echo '#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))';)
endef

include/linux/version.h: $(srctree)/Makefile FORCE
	$(call filechk,version.h)

include/linux/utsrelease.h: include/config/kernel.release FORCE
	$(call filechk,utsrelease.h)

PHONY += headerdep
headerdep:
	$(Q)find include/ -name '*.h' | xargs --max-args 1 scripts/headerdep.pl

PHONY += depend dep
depend dep:
	@echo '*** Warning: make $@ is unnecessary now.'

INSTALL_FW_PATH=$(INSTALL_MOD_PATH)/lib/firmware
export INSTALL_FW_PATH

PHONY += firmware_install
firmware_install: FORCE
	@mkdir -p $(objtree)/firmware
	$(Q)$(MAKE) -f $(srctree)/scripts/Makefile.fwinst obj=firmware __fw_install

export INSTALL_HDR_PATH = $(objtree)/usr

hdr-inst := -rR -f $(srctree)/scripts/Makefile.headersinst obj
hdr-dir = $(strip                                                         \
          $(if $(wildcard $(srctree)/arch/$(hdr-arch)/include/asm/Kbuild), \
               arch/$(hdr-arch)/include/asm, include/asm-$(hdr-arch)))

hdr-dst = $(if $(KBUILD_HEADERS), dst=include/asm-$(hdr-arch), dst=include/asm)

PHONY += __headers
__headers: include/linux/version.h scripts_basic FORCE
	$(Q)$(MAKE) $(build)=scripts scripts/unifdef

PHONY += headers_install_all
headers_install_all:
	$(Q)$(CONFIG_SHELL) $(srctree)/scripts/headers.sh install

PHONY += headers_install
headers_install: __headers
	$(if $(wildcard $(srctree)/$(hdr-dir)/Kbuild),, \
	$(error Headers not exportable for the $(SRCARCH) architecture))
	$(Q)$(MAKE) $(hdr-inst)=include
	$(Q)$(MAKE) $(hdr-inst)=$(hdr-dir) $(hdr-dst)

PHONY += headers_check_all
headers_check_all: headers_install_all
	$(Q)$(CONFIG_SHELL) $(srctree)/scripts/headers.sh check

PHONY += headers_check
headers_check: headers_install
	$(Q)$(MAKE) $(hdr-inst)=include HDRCHECK=1
	$(Q)$(MAKE) $(hdr-inst)=$(hdr-dir) $(hdr-dst) HDRCHECK=1

ifdef CONFIG_MODULES

all: modules

PHONY += modules
modules: $(vmlinux-dirs) $(if $(KBUILD_BUILTIN),vmlinux)
	$(Q)$(AWK) '!x[$$0]++' $(vmlinux-dirs:%=$(objtree)/%/modules.order) > $(objtree)/modules.order
	@$(kecho) '  Building modules, stage 2.';
	$(Q)$(MAKE) -f $(srctree)/scripts/Makefile.modpost
	$(Q)$(MAKE) -f $(srctree)/scripts/Makefile.fwinst obj=firmware __fw_modbuild

PHONY += modules_prepare
modules_prepare: prepare scripts

PHONY += modules_install
modules_install: _modinst_ _modinst_post

PHONY += _modinst_
_modinst_:
	@if [ -z "`$(DEPMOD) -V 2>/dev/null | grep module-init-tools`" ]; then \
		echo "Warning: you may need to install module-init-tools"; \
		echo "See http://www.codemonkey.org.uk/docs/post-halloween-2.6.txt";\
		sleep 1; \
	fi
	@rm -rf $(MODLIB)/kernel
	@rm -f $(MODLIB)/source
	@mkdir -p $(MODLIB)/kernel
	@ln -s $(srctree) $(MODLIB)/source
	@if [ ! $(objtree) -ef  $(MODLIB)/build ]; then \
		rm -f $(MODLIB)/build ; \
		ln -s $(objtree) $(MODLIB)/build ; \
	fi
	@cp -f $(objtree)/modules.order $(MODLIB)/
	$(Q)$(MAKE) -f $(srctree)/scripts/Makefile.modinst

PHONY += _modinst_post
_modinst_post: _modinst_
	$(Q)$(MAKE) -f $(srctree)/scripts/Makefile.fwinst obj=firmware __fw_modinst
	$(call cmd,depmod)

else

modules modules_install: FORCE
	@echo
	@echo "The present kernel configuration has modules disabled."
	@echo "Type 'make config' and enable loadable module support."
	@echo "Then build a kernel with module support enabled."
	@echo
	@exit 1

endif

CLEAN_DIRS  += $(MODVERDIR)
CLEAN_DIRS  -= tools
CLEAN_FILES +=	vmlinux System.map \
                .tmp_kallsyms* .tmp_version .tmp_vmlinux* .tmp_System.map

MRPROPER_DIRS  += include/config include2 usr/include
MRPROPER_FILES += .config .config.old include/asm .version .old_version \
                  include/linux/autoconf.h include/linux/version.h      \
                  include/linux/utsrelease.h                            \
                  include/linux/bounds.h include/asm*/asm-offsets.h     \
		  Module.symvers Module.markers tags TAGS cscope*

clean: rm-dirs  := $(CLEAN_DIRS)
clean: rm-files := $(CLEAN_FILES)
clean-dirs      := $(addprefix _clean_,$(srctree) $(vmlinux-alldirs) Documentation samples)

PHONY += $(clean-dirs) clean archclean
$(clean-dirs):
	$(Q)$(MAKE) $(clean)=$(patsubst _clean_%,%,$@)

clean: archclean $(clean-dirs)
	$(call cmd,rmdirs)
	$(call cmd,rmfiles)
	@find . $(RCS_FIND_IGNORE) \
		\( -name '*.[oas]' -o -name '*.ko' -o -name '.*.cmd' \
		-o -name '.*.d' -o -name '.*.tmp' -o -name '*.mod.c' \
		-o -name '*.symtypes' -o -name 'modules.order' \
		-o -name 'Module.markers' -o -name '.tmp_*.o.*' \) \
		-type f -print | xargs rm -f

mrproper: rm-dirs  := $(wildcard $(MRPROPER_DIRS))
mrproper: rm-files := $(wildcard $(MRPROPER_FILES))
mrproper-dirs      := $(addprefix _mrproper_,Documentation/DocBook scripts)

PHONY += $(mrproper-dirs) mrproper archmrproper
$(mrproper-dirs):
	$(Q)$(MAKE) $(clean)=$(patsubst _mrproper_%,%,$@)

mrproper: clean archmrproper $(mrproper-dirs)
	$(call cmd,rmdirs)
	$(call cmd,rmfiles)

PHONY += distclean

distclean: mrproper
	@find $(srctree) $(RCS_FIND_IGNORE) \
		\( -name '*.orig' -o -name '*.rej' -o -name '*~' \
		-o -name '*.bak' -o -name '#*#' -o -name '.*.orig' \
		-o -name '.*.rej' -o -size 0 \
		-o -name '*%' -o -name '.*.cmd' -o -name 'core' \) \
		-type f -print | xargs rm -f

package-dir	:= $(srctree)/scripts/package

%pkg: include/config/kernel.release FORCE
	$(Q)$(MAKE) $(build)=$(package-dir) $@
rpm: include/config/kernel.release FORCE
	$(Q)$(MAKE) $(build)=$(package-dir) $@

boards := $(wildcard $(srctree)/arch/$(SRCARCH)/configs/*_defconfig)
boards := $(notdir $(boards))
board-dirs := $(dir $(wildcard $(srctree)/arch/$(SRCARCH)/configs/*/*_defconfig))
board-dirs := $(sort $(notdir $(board-dirs:/=)))

help:
	@echo  'Cleaning targets:'
	@echo  '  clean		  - Remove most generated files but keep the config and'
	@echo  '                    enough build support to build external modules'
	@echo  '  mrproper	  - Remove all generated files + config + various backup files'
	@echo  '  distclean	  - mrproper + remove editor backup and patch files'
	@echo  ''
	@echo  'Configuration targets:'
	@$(MAKE) -f $(srctree)/scripts/kconfig/Makefile help
	@echo  ''
	@echo  'Other generic targets:'
	@echo  '  all		  - Build all targets marked with [*]'
	@echo  '* vmlinux	  - Build the bare kernel'
	@echo  '* modules	  - Build all modules'
	@echo  '  modules_install - Install all modules to INSTALL_MOD_PATH (default: /)'
	@echo  '  firmware_install- Install all firmware to INSTALL_FW_PATH'
	@echo  '                    (default: $$(INSTALL_MOD_PATH)/lib/firmware)'
	@echo  '  dir/            - Build all files in dir and below'
	@echo  '  dir/file.[ois]  - Build specified target only'
	@echo  '  dir/file.ko     - Build module including final link'
	@echo  '  prepare         - Set up for building external modules'
	@echo  '  tags/TAGS	  - Generate tags file for editors'
	@echo  '  cscope	  - Generate cscope index'
	@echo  '  kernelrelease	  - Output the release version string'
	@echo  '  kernelversion	  - Output the version stored in Makefile'
	@echo  '  headers_install - Install sanitised kernel headers to INSTALL_HDR_PATH'; \
	 echo  '                    (default: $(INSTALL_HDR_PATH))'; \
	 echo  ''
	@echo  'Static analysers'
	@echo  '  checkstack      - Generate a list of stack hogs'
	@echo  '  namespacecheck  - Name space analysis on compiled kernel'
	@echo  '  versioncheck    - Sanity check on version.h usage'
	@echo  '  includecheck    - Check for duplicate included header files'
	@echo  '  export_report   - List the usages of all exported symbols'
	@echo  '  headers_check   - Sanity check on exported headers'
	@echo  '  headerdep       - Detect inclusion cycles in headers'; \
	 echo  ''
	@echo  'Kernel packaging:'
	@$(MAKE) $(build)=$(package-dir) help
	@echo  ''
	@echo  'Documentation targets:'
	@$(MAKE) -f $(srctree)/Documentation/DocBook/Makefile dochelp
	@echo  ''
	@echo  'Architecture specific targets ($(SRCARCH)):'
	@$(if $(archhelp),$(archhelp),\
		echo '  No architecture specific help defined for $(SRCARCH)')
	@echo  ''
	@$(if $(boards), \
		$(foreach b, $(boards), \
		printf "  %-24s - Build for %s\\n" $(b) $(subst _defconfig,,$(b));) \
		echo '')
	@$(if $(board-dirs), \
		$(foreach b, $(board-dirs), \
		printf "  %-16s - Show %s-specific targets\\n" help-$(b) $(b);) \
		printf "  %-16s - Show all of the above\\n" help-boards; \
		echo '')

	@echo  '  make V=0|1 [targets] 0 => quiet build (default), 1 => verbose build'
	@echo  '  make V=2   [targets] 2 => give reason for rebuild of target'
	@echo  '  make O=dir [targets] Locate all output files in "dir", including .config'
	@echo  '  make C=1   [targets] Check all c source with $$CHECK (sparse by default)'
	@echo  '  make C=2   [targets] Force check of all c source with $$CHECK'
	@echo  ''
	@echo  'Execute "make" or "make all" to build all targets marked with [*] '
	@echo  'For further info see the ./README file'


help-board-dirs := $(addprefix help-,$(board-dirs))

help-boards: $(help-board-dirs)

boards-per-dir = $(notdir $(wildcard $(srctree)/arch/$(SRCARCH)/configs/$*/*_defconfig))

$(help-board-dirs): help-%:
	@echo  'Architecture specific targets ($(SRCARCH) $*):'
	@$(if $(boards-per-dir), \
		$(foreach b, $(boards-per-dir), \
		printf "  %-24s - Build for %s\\n" $*/$(b) $(subst _defconfig,,$(b));) \
		echo '')

%docs: scripts_basic FORCE
	$(Q)$(MAKE) $(build)=Documentation/DocBook $@

else

KBUILD_MODULES := 1
PHONY += crmodverdir
crmodverdir:
	$(cmd_crmodverdir)

PHONY += $(objtree)/Module.symvers
$(objtree)/Module.symvers:
	@test -e $(objtree)/Module.symvers || ( \
	echo; \
	echo "  WARNING: Symbol version dump $(objtree)/Module.symvers"; \
	echo "           is missing; modules will have no dependencies and modversions."; \
	echo )

module-dirs := $(addprefix _module_,$(KBUILD_EXTMOD))
PHONY += $(module-dirs) modules
$(module-dirs): crmodverdir $(objtree)/Module.symvers
	$(Q)$(MAKE) $(build)=$(patsubst _module_%,%,$@)

modules: $(module-dirs)
	@$(kecho) '  Building modules, stage 2.';
	$(Q)$(MAKE) -f $(srctree)/scripts/Makefile.modpost

PHONY += modules_install
modules_install: _emodinst_ _emodinst_post

install-dir := $(if $(INSTALL_MOD_DIR),$(INSTALL_MOD_DIR),extra)
PHONY += _emodinst_
_emodinst_:
	$(Q)mkdir -p $(MODLIB)/$(install-dir)
	$(Q)$(MAKE) -f $(srctree)/scripts/Makefile.modinst

PHONY += _emodinst_post
_emodinst_post: _emodinst_
	$(call cmd,depmod)

clean-dirs := $(addprefix _clean_,$(KBUILD_EXTMOD))

PHONY += $(clean-dirs) clean
$(clean-dirs):
	$(Q)$(MAKE) $(clean)=$(patsubst _clean_%,%,$@)

clean:	rm-dirs := $(MODVERDIR)
clean: rm-files := $(KBUILD_EXTMOD)/Module.symvers
clean: $(clean-dirs)
	$(call cmd,rmdirs)
	$(call cmd,rmfiles)
	@find $(KBUILD_EXTMOD) $(RCS_FIND_IGNORE) \
		\( -name '*.[oas]' -o -name '*.ko' -o -name '.*.cmd' \
		-o -name '.*.d' -o -name '.*.tmp' -o -name '*.mod.c' \) \
		-type f -print | xargs rm -f

help:
	@echo  '  Building external modules.'
	@echo  '  Syntax: make -C path/to/kernel/src M=$$PWD target'
	@echo  ''
	@echo  '  modules         - default target, build the module(s)'
	@echo  '  modules_install - install the module'
	@echo  '  clean           - remove generated files in module directory only'
	@echo  ''

PHONY += prepare scripts
prepare: ;
scripts: ;
endif

quiet_cmd_tags = GEN     $@
      cmd_tags = $(CONFIG_SHELL) $(srctree)/scripts/tags.sh $@

tags TAGS cscope: FORCE
	$(call cmd,tags)

includecheck:
	find * $(RCS_FIND_IGNORE) \
		-name '*.[hcS]' -type f -print | sort \
		| xargs $(PERL) -w $(srctree)/scripts/checkincludes.pl

versioncheck:
	find * $(RCS_FIND_IGNORE) \
		-name '*.[hcS]' -type f -print | sort \
		| xargs $(PERL) -w $(srctree)/scripts/checkversion.pl

namespacecheck:
	$(PERL) $(srctree)/scripts/namespace.pl

export_report:
	$(PERL) $(srctree)/scripts/export_report.pl

endif
endif

PHONY += checkstack kernelrelease kernelversion

ifeq ($(ARCH), um)
CHECKSTACK_ARCH := $(SUBARCH)
else
CHECKSTACK_ARCH := $(ARCH)
endif
checkstack:
	$(OBJDUMP) -d vmlinux $$(find . -name '*.ko') | \
	$(PERL) $(src)/scripts/checkstack.pl $(CHECKSTACK_ARCH)

kernelrelease:
	$(if $(wildcard include/config/kernel.release), $(Q)echo $(KERNELRELEASE), \
	$(error kernelrelease not valid - run 'make prepare' to update it))
kernelversion:
	@echo $(KERNELVERSION)

ifeq ($(KBUILD_EXTMOD),)
        build-dir  = $(patsubst %/,%,$(dir $@))
        target-dir = $(dir $@)
else
        zap-slash=$(filter-out .,$(patsubst %/,%,$(dir $@)))
        build-dir  = $(KBUILD_EXTMOD)$(if $(zap-slash),/$(zap-slash))
        target-dir = $(if $(KBUILD_EXTMOD),$(dir $<),$(dir $@))
endif

%.s: %.c prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.i: %.c prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.o: %.c prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.lst: %.c prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.s: %.S prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.o: %.S prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.symtypes: %.c prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)

/: prepare scripts FORCE
	$(cmd_crmodverdir)
	$(Q)$(MAKE) KBUILD_MODULES=$(if $(CONFIG_MODULES),1) \
	$(build)=$(build-dir)
%/: prepare scripts FORCE
	$(cmd_crmodverdir)
	$(Q)$(MAKE) KBUILD_MODULES=$(if $(CONFIG_MODULES),1) \
	$(build)=$(build-dir)
%.ko: prepare scripts FORCE
	$(cmd_crmodverdir)
	$(Q)$(MAKE) KBUILD_MODULES=$(if $(CONFIG_MODULES),1)   \
	$(build)=$(build-dir) $(@:.ko=.o)
	$(Q)$(MAKE) -f $(srctree)/scripts/Makefile.modpost

quiet_cmd_rmdirs = $(if $(wildcard $(rm-dirs)),CLEAN   $(wildcard $(rm-dirs)))
      cmd_rmdirs = rm -rf $(rm-dirs)

quiet_cmd_rmfiles = $(if $(wildcard $(rm-files)),CLEAN   $(wildcard $(rm-files)))
      cmd_rmfiles = rm -f $(rm-files)

quiet_cmd_depmod = DEPMOD  $(KERNELRELEASE)
      cmd_depmod = \
	if [ -r System.map -a -x $(DEPMOD) ]; then                              \
		$(DEPMOD) -ae -F System.map                                     \
		$(if $(strip $(INSTALL_MOD_PATH)), -b $(INSTALL_MOD_PATH) )     \
		$(KERNELRELEASE);                                               \
	fi

cmd_crmodverdir = $(Q)mkdir -p $(MODVERDIR) \
                  $(if $(KBUILD_MODULES),; rm -f $(MODVERDIR)/*)

a_flags = -Wp,-MD,$(depfile) $(KBUILD_AFLAGS) $(AFLAGS_KERNEL) \
	  $(NOSTDINC_FLAGS) $(LINUXINCLUDE) $(KBUILD_CPPFLAGS) \
	  $(modkern_aflags) $(EXTRA_AFLAGS) $(AFLAGS_$(basetarget).o)

quiet_cmd_as_o_S = AS      $@
cmd_as_o_S       = $(CC) $(a_flags) -c -o $@ $<

targets := $(wildcard $(sort $(targets)))
cmd_files := $(wildcard .*.cmd $(foreach f,$(targets),$(dir $(f)).$(notdir $(f)).cmd))

ifneq ($(cmd_files),)
  $(cmd_files): ;
  include $(cmd_files)
endif

clean := -f $(if $(KBUILD_SRC),$(srctree)/)scripts/Makefile.clean obj

endif

PHONY += FORCE
FORCE:

.PHONY: $(PHONY)
