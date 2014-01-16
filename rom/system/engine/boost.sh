#!/system/xbin/sh
### FeraDroid Engine v16 | By FeraVolt. 2013

busybox sync;
sleep 1
echo 3 > /proc/sys/vm/drop_caches
sleep 1;
busybox sync;
sleep 1
echo 3 > /proc/sys/vm/drop_caches
busybox sync;
sleep 1
ramused=$((`free | awk '{ print $3 }' | sed -n 2p`/1024))
ramkbytesfree=`free | awk '{ print $4 }' | sed -n 2p`
ramkbytescached=`cat /proc/meminfo | grep Cached | awk '{print $2}' | sed -n 1p`
ramfree=$(($ramkbytesfree/1024))
ramcached=$(($ramkbytescached/1024))
ramreportedfree=$(($ramfree + $ramcached))
echo "Used RAM: $ramused MB"
echo "Reported Free RAM: $ramreportedfree MB"
echo "Real Free RAM: $ramfree MB"

