ATH_LINUXPATH := /home/feravolt/ferra/kernel
ATH_BUILD_TYPE=QUALCOMM_ARM_NATIVEMMC
_BSP_REV=3240
ATH_BSP_TYPE=QSD8K_BSP$(_BSP_REV)
ATH_ANDROID_ROOT=/opt/qc8k

  $(info ***********************************)
  $(info PLATFORM: $(ATH_BUILD_TYPE))
  $(info BSP:      $(ATH_BSP_TYPE))
  $(info ***********************************)

ATH_BUILD_SYSTEM_TOOLS:= no
ATH_ANDROID_ENV := yes
ATH_EEPROM_FILE_USED := no 
ATH_ARCH_CPU_TYPE := arm 
ATH_CROSS_COMPILE_TYPE := /home/feravolt/ferra/toolchain/bin/arm-gnueabi-
ATH_BUS_TYPE := SDIO
ATH_OS_SUB_TYPE := linux_2_6
ATH_BUS_SUBTYPE := linux_sdio


