### FeraLab ###

echo "FeraDroid Engine"
echo "By FeraVolt."
echo "Firing up..."

chmod 777 /sbin/sysrw
chmod 777 /sbin/sysro
/sbin/sysrw
rm -f /system/lib/modules/ar6000.ko
cp /modules/wifi.ko /system/lib/modules/wifi.ko
chmod 666 /system/lib/modules/*
mount -t debugfs debugfs /sys/kernel/debug
echo 0 > /sys/kernel/debug/msm_fb/0/vsync_enable
echo 16 > /sys/kernel/debug/msm_fb/mdp/mdp_usec_diff_treshold
echo 30 > /sys/kernel/debug/msm_fb/mdp/vs_rdcnt_slow
echo 1 > /sys/kernel/mm/ksm/deferred_timer
echo 1 > /sys/kernel/fast_charge/force_fast_charge
mount -o bind /system/engine /engine
chmod 777 /engine
chmod 777 /cache
cd /cache
mkdir download
cd
cd /data/local
mkdir download
cd
mount -o bind /data/local/download /cache/download
chmod 777 /cache/download
rm /cache/download/downloadfile*.apk >/dev/null 2>&1
cd /engine
/sbin/sysrw
mkdir sysinit
mount -o bind /system/etc/init.d /system/engine/sysinit
cd
chmod -R 777 /engine/*
chmod -R 777 /engine/sysinit/*
chmod -R 777 /engine/tweaks/*
chmod -R 777 /system/usr/vendor/prop/*
/sbin/rngd -t 2 -T 1 -s 256 --fill-watermark=80%
sleep 2
echo -8 > /proc/$(pgrep rngd)/oom_adj
renice 5 `pidof rngd`
/system/xbin/run-parts /system/etc/init.d

