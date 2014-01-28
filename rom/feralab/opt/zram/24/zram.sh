#!/system/xbin/sh
### FeraDroid Engine v17 | By FeraVolt. 2013###
export PATH=/sbin

rzscontrol /dev/block/ramzswap0 -i -d 24576
busybox swapon /dev/block/ramzswap0
echo 18 > /proc/sys/vm/swappiness

