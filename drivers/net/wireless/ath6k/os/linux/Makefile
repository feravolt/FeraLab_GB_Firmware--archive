REV ?= 2
EXTRA_CFLAGS += -D__linux__ -Wno-error
EXTRA_CFLAGS += -I$(ATH_SRC_BASE)/include
EXTRA_CFLAGS += -I$(ATH_SRC_BASE)/../include
EXTRA_CFLAGS += -I$(ATH_SRC_BASE)/../include/AR6002
EXTRA_CFLAGS += -I$(ATH_SRC_BASE)/wlan/include
EXTRA_CFLAGS += -I$(ATH_SRC_BASE)/os/linux/include
EXTRA_CFLAGS += -I$(ATH_SRC_BASE)/os/
EXTRA_CFLAGS += -I$(ATH_SRC_BASE)/bmi/include

EXTRA_CFLAGS += -DLINUX -DDEBUG -D__KERNEL__ \
                -DTCMD -DSEND_EVENT_TO_APP -DUSER_KEYS \
                -DNO_SYNC_FLUSH -DWAPI_ENABLE #\
                -DMULTIPLE_FRAMES_PER_INTERRUPT -DAR6000REV$(REV) \
                -DBLOCK_TX_PATH_FLAG \

EXTRA_CFLAGS += -DANDROID_ENV \
                  -DSOFTMAC_USED \
                  -DREGION_CODE_FILE_USED

EXTRA_CFLAGS += -DKERNEL_2_6
obj-m += ar6000.o
-include $(ATH_SRC_BASE)/hif/$(ATH_HIF_TYPE)/Makefile
ar6000-objs += ../../htc2/AR6000/ar6k.o        \
			   ../../htc2/AR6000/ar6k_events.o \
               ../../htc2/htc_send.o       \
               ../../htc2/htc_recv.o       \
               ../../htc2/htc_services.o   \
               ../../htc2/htc.o     		  \
               ../../bmi/src/bmi.o            \
               ar6000_drv.o           			\
               ar6000_raw_if.o					\
               netbuf.o							\
               wireless_ext.o					\
               ioctl.o							\
               engine.o							\
               ../../miscdrv/common_drv.o		\
               ../../miscdrv/credit_dist.o					\
               ../../wmi/wmi.o                       \
               ../../wlan/src/wlan_node.o            \
               ../../wlan/src/wlan_recv_beacon.o     \
               ../../wlan/src/wlan_utils.o			 

ar6000-objs += eeprom.o

