ifdef ATH_MAKE_INCLUDE_OVERRIDE
_LOCALMAKE_INCLUDE = $(ATH_MAKE_INCLUDE_OVERRIDE)
else
_LOCALMAKE_INCLUDE = localmake.linux.inc
endif

-include $(_LOCALMAKE_INCLUDE)

export  ATH_SRC_BASE
export  ATH_BUILD_TYPE
export  ATH_OS_SUB_TYPE
export  ATH_LINUXPATH
export  ATH_CROSS_COMPILE_TYPE
export  ATH_ARCH_CPU_TYPE
export  ATH_SDIO_STACK_BASE
export  ATH_BUS_TYPE
export  ATH_BUS_SUBTYPE
export  ATH_HC_DRIVERS
export  ATH_HTC_RAW_INT_ENV
export  ATH_ANDROID_ENV
export  ATH_EEPROM_FILE_USED

ATH_SRC_BASE :=$(shell pwd)
MAKE :=make
CC :=$(ATH_CROSS_COMPILE_TYPE)gcc
LD :=$(ATH_CROSS_COMPILE_TYPE)ld
STRIP :=$(ATH_CROSS_COMPILE_TYPE)strip
ATH_HIF_TYPE :=`echo $(ATH_BUS_TYPE) | tr [:upper:] [:lower:]`
ATH_LINUXPATH := /home/feravolt/ferra/kernel
export  STRIP
export  CC
export  LD
export  STRIP

ifdef ATH_CROSS_COMPILE_TYPE
_CROSS_COMPILE_LINE := ARCH=$(ATH_ARCH_CPU_TYPE) CROSS_COMPILE=$(ATH_CROSS_COMPILE_TYPE)
endif

ifneq ($(ATH_OS_SUB_TYPE),linux_2_4)
KMOD_EXTENSION :=ko
endif
ifeq ($(ATH_OS_SUB_TYPE),linux_2_4)
KMOD_EXTENSION :=o
endif

ifdef ATH_BUILD_OUTPUT_OVERRIDE
_COMPILED_OBJECTS_PATH :=$(ATH_BUILD_OUTPUT_OVERRIDE)
COMPILED_BIN_OBJECTS_PATH :=$(ATH_BUILD_OUTPUT_OVERRIDE)
COMPILED_IMAGE_OBJECTS_PATH :=$(ATH_BUILD_OUTPUT_OVERRIDE)
_MAKE_IMAGE_OUTPUT_DIR :=
_MAKE_BIN_OUTPUT_DIR :=
_CLEAN_IMAGE_OUTPUT_DIR :=
_CLEAN_BIN_OUTPUT_DIR :=
else
_COMPILED_OBJECTS_PATH := $(ATH_SRC_BASE)/.output/$(ATH_BUILD_TYPE)-$(ATH_BUS_TYPE)
COMPILED_BIN_OBJECTS_PATH := $(ATH_SRC_BASE)/.output/bin
COMPILED_IMAGE_OBJECTS_PATH :=$(_COMPILED_OBJECTS_PATH)/$(COMPILED_OBJECTS_PREFIX)image
_MAKE_IMAGE_OUTPUT_DIR := mkdir --parents $(COMPILED_IMAGE_OBJECTS_PATH)
_MAKE_BIN_OUTPUT_DIR := mkdir --parents $(COMPILED_BIN_OBJECTS_PATH)
_CLEAN_IMAGE_OUTPUT_DIR := rm -R -f $(COMPILED_IMAGE_OBJECTS_PATH)
_CLEAN_BIN_OUTPUT_DIR := rm -R -f $(COMPILED_BIN_OBJECTS_PATH)
endif

export COMPILED_IMAGE_OBJECTS_PATH
export COMPILED_BIN_OBJECTS_PATH

all: modules

modules:
ifndef ATH_BUILD_TYPE 
	@echo "Please edit the $(_LOCALMAKE_INCLUDE) file"
	exit 1
endif
	$(_MAKE_IMAGE_OUTPUT_DIR)
	$(_MAKE_BIN_OUTPUT_DIR)
	$(MAKE)  $(_CROSS_COMPILE_LINE) -C $(ATH_LINUXPATH) ATH_HIF_TYPE=$(ATH_HIF_TYPE) SUBDIRS=$(ATH_SRC_BASE)/os/linux modules
	$(MAKE) -C hif/$(ATH_HIF_TYPE)
	cp -f $(ATH_SRC_BASE)/os/linux/ar6000.$(KMOD_EXTENSION) $(COMPILED_IMAGE_OBJECTS_PATH)
	
clean:
	rm -f os/linux/ar6000.mod.c os/linux/*.o os/linux/*.ko wmi/*.o htc/AR6000/src/*.o htc/*.o \
	bmi/src/*.o wlan/src/*.o driver/* tools/bmiloader/bmiloader                               \
	tools/wmiconfig/wmiconfig tools/eeprom/eeprom.AR6002 \
	tools/recEvent/recEvent tools/wfa_sta/wfa_sta

	find $(ATH_SRC_BASE) \( -not -path "*.output*" -a -name '*.[oas]' -o -name core -o -name '.*.flags' -o -name '.ko' -o -name '.*.cmd' \) -type f -print \
		| grep -v lxdialog/ | xargs rm -f
	$(MAKE) -C hif/$(ATH_HIF_TYPE) clean

clobber:clean
	rm -rf .output
