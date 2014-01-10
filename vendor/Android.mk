ifeq ($(BOARD_WPA_SUPPLICANT_DRIVER),AR6000)
ifneq ($(TARGET_SIMULATOR),true)

LOCAL_PATH := $(call my-dir)
PRODUCT_COPY_FILES += \
		   $(LOCAL_PATH)/target/AR6002/hw2.0/bin/athwlan.bin.z77:system/lib/modules/athwlan.bin.z77 \
		   $(LOCAL_PATH)/target/AR6002/hw2.0/bin/data.patch.hw2_0.bin:system/lib/modules/data.patch.hw2_0.bin

ATHEROS_PATH := $(LOCAL_PATH)

ifneq ($(PREBUILT_KERNEL_MODULES),true)

ATHEROS_SRC_BASE := $(LOCAL_PATH)/host
ATHEROS_M_PATH := $(ATHEROS_SRC_BASE)/os/linux
ATHEROS_M_OUT := $(TARGET_OUT_INTERMEDIATES)/$(ATHEROS_M_PATH)
ATHEROS_OUT := $(TARGET_OUT)/lib/modules
ATHEROS_KO := $(ATHEROS_M_OUT)/ar6000.ko

ALL_PREBUILT += $(ATHEROS_OUT)/wifi.ko

$(ATHEROS_OUT):
	mkdir -p $(ATHEROS_OUT)

$(ATHEROS_M_OUT):
	mkdir -p $(ATHEROS_M_OUT)

# build atheros
$(ATHEROS_KO): $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/arch/arm/boot/zImage $(TARGET_PREBUILT_KERNEL) $(ATHEROS_OUT) $(ATHEROS_M_OUT)
	ATH_BSP_TYPE=QSD8K_BSP3200 \
	TARGET_TYPE=AR6002 \
	ATH_BUILD_TYPE=QUALCOMM_ARM_NATIVEMMC \
	ATH_ANDROID_ENV=yes \
	ATH_SRC_BASE=../$(ATHEROS_SRC_BASE) \
	ATH_BUS_SUBTYPE=linux_sdio \
	$(MAKE) \
	ARCH=arm  \
	CROSS_COMPILE=arm-eabi- \
	-C $(KERNEL_OUT) \
	ATH_HIF_TYPE=sdio \
	M=../$(ATHEROS_M_PATH) \
	modules 


# copy ar6000.ko
$(ATHEROS_OUT)/wifi.ko: $(ATHEROS_KO) | $(ACP)
	$(transform-prebuilt-to-target)


include $(call all-subdir-makefiles)

endif
endif
endif
