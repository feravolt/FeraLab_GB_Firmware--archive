### FeraLab ###

echo "FeraDroid Engine"
echo "By FeraVolt."
echo "Firing up..."

export PATH=/sbin
chmod 777 /sbin/sysrw
chmod 777 /sbin/sysro
/sbin/sysrw
rm -f /system/lib/modules/ar6000.ko
cp /modules/wifi.ko /system/lib/modules/wifi.ko
chmod 666 /system/lib/modules/wifi.ko
mount -t debugfs debugfs /sys/kernel/debug
echo 0 > /sys/kernel/debug/msm_fb/0/vsync_enable
mount -o bind /system/engine /engine
chmod 777 /engine
chmod 777 /cache
cd /engine
/sbin/sysrw
mkdir sysinit
mount -o bind /system/etc/init.d /system/engine/sysinit
cd
chmod -R 777 /engine/*
chmod -R 777 /engine/sysinit/*
chmod -R 777 /engine/tweaks/*
chmod -R 777 /system/usr/vendor/prop/*
/system/xbin/run-parts /system/etc/init.d

