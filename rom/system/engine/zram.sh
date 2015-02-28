#!/system/xbin/sh
### FeraDroid Engine v18 | By FeraVolt. 2014 ###
export PATH=/sbin

echo 81 > /proc/sys/vm/swappiness
insmod /system/lib/modules/zram.ko
sleep 1
rzscontrol /dev/block/ramzswap0 -i -d 131072
sleep 1
busybox swapon /dev/block/ramzswap0
free

