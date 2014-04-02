#!/system/xbin/sh
### FeraDroid Engine v18 | By FeraVolt. 2014 ###

ramused=$((`free | awk '{ print $3 }' | sed -n 2p`/1024))
ramkbytesfree=`free | awk '{ print $4 }' | sed -n 2p`
ramkbytescached=`cat /proc/meminfo | grep Cached | awk '{print $2}' | sed -n 1p`
ramfree=$(($ramkbytesfree/1024))
ramcached=$(($ramkbytescached/1024))
ramreportedfree=$(($ramfree + $ramcached))
echo "Used RAM: $ramused MB"
echo "Reported Free RAM: $ramreportedfree MB"
echo "Real Free RAM: $ramfree MB"
echo " "
echo "Clearing page-caches..."
busybox sync;
sleep 1
echo 3 > /proc/sys/vm/drop_caches
sleep 2;
echo 2 > /proc/sys/vm/drop_caches
busybox sync;
sleep 2
echo 3 > /proc/sys/vm/drop_caches
busybox sync;
sleep 1
echo "NOW:"
echo "Used RAM: $ramused MB"
echo "Reported Free RAM: $ramreportedfree MB"
echo "Real Free RAM: $ramfree MB"
sleep 2

