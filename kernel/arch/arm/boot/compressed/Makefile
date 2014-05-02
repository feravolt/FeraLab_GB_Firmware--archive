OBJS	=
plus_sec := $(call as-instr,.arch_extension sec,+sec)
AFLAGS_head.o := -DTEXT_OFFSET=$(TEXT_OFFSET)
AFLAGS_head.o += -Wa,-march=armv7-a$(plus_sec)
HEAD = head.o
AFLAGS_misc.o +=-Wa,-march=armv7-a$(plus_sec)
MISC = misc.o
FONTC	= $(srctree)/drivers/video/console/font_acorn_8x8.c

ifeq ($(CONFIG_ARCH_ACORN),y)
OBJS		+= ll_char_wr.o font.o
endif

ifeq ($(CONFIG_ARCH_SHARK),y)
OBJS		+= head-shark.o ofw-shark.o
endif

ifeq ($(CONFIG_ARCH_L7200),y)
OBJS		+= head-l7200.o
endif

ifeq ($(CONFIG_ARCH_P720T),y)
OBJS		+= head-sa1100.o
endif

ifeq ($(CONFIG_ARCH_SA1100),y)
OBJS		+= head-sa1100.o
endif

ifeq ($(CONFIG_CPU_XSCALE),y)
OBJS		+= head-xscale.o
endif

ifeq ($(CONFIG_PXA_SHARPSL),y)
OBJS		+= head-sharpsl.o
endif

ifeq ($(CONFIG_CPU_BIG_ENDIAN),y)
ifeq ($(CONFIG_CPU_CP15),y)
OBJS		+= big-endian.o
else
endif
endif

ifeq ($(CONFIG_ZBOOT_ROM),y)
ZTEXTADDR	:= $(CONFIG_ZBOOT_ROM_TEXT)
ZBSSADDR	:= $(CONFIG_ZBOOT_ROM_BSS)
else
ZTEXTADDR	:= 0
ZBSSADDR	:= ALIGN(4)
endif

SEDFLAGS	= s/TEXT_START/$(ZTEXTADDR)/;s/BSS_START/$(ZBSSADDR)/

targets       := vmlinux vmlinux.lds piggy.gz piggy.o font.o font.c \
		 head.o misc.o $(OBJS)

EXTRA_CFLAGS := -fpic -fno-builtin
EXTRA_AFLAGS := -Wa,-march=all

ifeq ($(CONFIG_FUNCTION_TRACER),y)
ORIG_CFLAGS := $(KBUILD_CFLAGS)
KBUILD_CFLAGS = $(subst -pg, , $(ORIG_CFLAGS))
endif

LDFLAGS_vmlinux := --defsym zreladdr=$(ZRELADDR)
ifneq ($(INITRD_PHYS),)
LDFLAGS_vmlinux += --defsym initrd_phys=$(INITRD_PHYS)
endif
ifneq ($(PARAMS_PHYS),)
LDFLAGS_vmlinux += --defsym params_phys=$(PARAMS_PHYS)
endif
LDFLAGS_vmlinux += -p --no-undefined -X \
	$(shell $(CC) $(KBUILD_CFLAGS) --print-libgcc-file-name) -T

$(obj)/vmlinux: $(obj)/vmlinux.lds $(obj)/$(HEAD) $(obj)/$(MISC) $(obj)/piggy.o \
	 	$(addprefix $(obj)/, $(OBJS)) FORCE
	$(call if_changed,ld)
	@:

$(obj)/piggy.gz: $(obj)/../Image FORCE
	$(call if_changed,gzip)

AFLAGS_piggy.$(suffix_y).o += -Wa,-march=armv7-a$(plus_sec)
$(obj)/piggy.o:  $(obj)/piggy.gz FORCE

CFLAGS_font.o := -Dstatic=
CFLAGS_misc.o := -Dstatic=

$(obj)/font.c: $(FONTC)
	$(call cmd,shipped)

$(obj)/vmlinux.lds: $(obj)/vmlinux.lds.in arch/arm/boot/Makefile .config
	@sed "$(SEDFLAGS)" < $< > $@

