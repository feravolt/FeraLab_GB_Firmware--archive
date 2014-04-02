#!/system/xbin/sh
### FeraDroid Engine v18 | By FeraVolt. 2014 ###
export PATH=/sbin

rzscontrol /dev/block/ramzswap0 -i -d 65536
busybox swapon /dev/block/ramzswap0
echo 36 > /proc/sys/vm/swappiness

