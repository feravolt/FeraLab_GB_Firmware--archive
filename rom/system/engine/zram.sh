#!/system/xbin/sh
### FeraDroid Engine v16 | By FeraVolt. 2013###
export PATH=/sbin

rzscontrol /dev/block/ramzswap0 -i -d 20480
busybox swapon /dev/block/ramzswap0
echo 20 > /proc/sys/vm/swappiness
