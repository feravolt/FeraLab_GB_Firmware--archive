#!/system/xbin/sh
### FeraDroid Engine v17 | By FeraVolt. 2013###
export PATH=/sbin

rzscontrol /dev/block/ramzswap0 -i -d 98304
busybox swapon /dev/block/ramzswap0
echo 45 > /proc/sys/vm/swappiness

