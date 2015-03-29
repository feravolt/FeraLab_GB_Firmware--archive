#!/system/xbin/sh
### FeraDroid Engine v18 | By FeraVolt. 2015
export PATH=/sbin

echo 90 > /proc/sys/vm/swappiness
sleep 1
rzscontrol /dev/block/ramzswap0 -i -d 131072
sleep 1
swapon /dev/block/ramzswap0
free

