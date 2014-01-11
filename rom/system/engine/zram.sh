#!/system/xbin/sh
### FeraDroid Engine v15 | By FeraVolt. 2013###
export PATH=/sbin

rzscontrol /dev/block/ramzswap0 -i -d 20480
busybox swapon /dev/block/ramzswap0
echo 40 > /proc/sys/vm/swappiness
