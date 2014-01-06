ifdef ATH_SDIO_STACK_BASE
_SDIO_STACK = YES
else
_SDIO_STACK = $(shell if [ -f $(ATH_SRC_BASE)/sdiostack/src/Makefile ]; then echo "YES"; else echo "NO"; fi)

ifeq ($(_SDIO_STACK), YES)
ATH_SDIO_STACK_BASE := $(ATH_SRC_BASE)/sdiostack
endif
endif 

ifeq ($(ATH_BUS_SUBTYPE),linux_sdio)
_HIF_SUB_TYPE = linux_sdio
_SDIO_STACK = NO
else
_HIF_SUB_TYPE = linux_athsdio
endif


ifeq ($(_SDIO_STACK), YES)
_SDIO_STACK_MAKE_PARAMS := CT_BUILD_TYPE=$(ATH_BUILD_TYPE) \
                           CT_OS_TYPE=linux \
                           CT_OS_SUB_TYPE=$(ATH_OS_SUB_TYPE) \
                           CT_LINUXPATH=$(ATH_LINUXPATH) \
                           CT_BUILD_TYPE=$(ATH_BUILD_TYPE) \
                           CT_CROSS_COMPILE_TYPE=$(ATH_CROSS_COMPILE_TYPE) \
                           CT_ARCH_CPU_TYPE=$(ATH_ARCH_CPU_TYPE) \
                           CT_HC_DRIVERS=$(ATH_HC_DRIVERS) \
                           CT_MAKE_INCLUDE_OVERRIDE=$(_LOCALMAKE_INCLUDE)   \
                           CT_BUILD_OUTPUT_OVERRIDE=$(COMPILED_IMAGE_OBJECTS_PATH) \
                           BUS_BUILD=1
endif
EXTRA_CFLAGS += -I$(ATH_SRC_BASE)/hif/sdio/$(_HIF_SUB_TYPE)/include
EXTRA_CFLAGS += -DSDIO
EXTRA_CFLAGS += -I$(ATH_SDIO_STACK_BASE)/src/include

all:
ifeq ($(_SDIO_STACK),YES)
	$(MAKE) $(_SDIO_STACK_MAKE_PARAMS) -C  $(ATH_SDIO_STACK_BASE)/src default
endif

clean:
ifeq ($(_SDIO_STACK),YES)
	$(MAKE) $(_SDIO_STACK_MAKE_PARAMS) -C $(ATH_SDIO_STACK_BASE)/src clean
endif

