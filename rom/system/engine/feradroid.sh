echo "FeraDroid Engine"
echo "version 18.1"
echo "By FeraVolt."

if [ -e /system/usr/vendor/prop/firstboot ];
then
   /system/xbin/sysrw
   sh /system/engine/fix.sh
   sleep 54
   sleep 54
   sleep 54
   /system/xbin/sysrw
   rm -f /system/usr/vendor/prop/firstboot
   rm -f /system/usr/vendor/prop/notferalab
   exit
else

/system/xbin/sysrw
/system/xbin/run-parts /system/engine/tweaks
chmod 644 /system/build.prop
chmod -R 777 /system/etc/sysctl.conf
rm -f /data/cache/*.apk
rm -f /data/cache/*.tmp
rm -f /data/dalvik-cache/*.apk
rm -f /data/dalvik-cache/*.tmp
rm -Rf /system/lost+found/*
rm -f /data/tombstones/*
rm -f /mnt/sdcard/LOST.DIR/*
rm -Rf /mnt/sdcard/LOST.DIR
rm -f /mnt/sdcard/found000/*
rm -Rf /mnt/sdcard/found000
rm -f /mnt/sdcard/fix_permissions.log
chmod 000 /data/tombstones
echo "1536,2048,4096,5120,7680,18432" > /sys/module/lowmemorykiller/parameters/minfree
echo "0,3,5,7,14,15" > /sys/module/lowmemorykiller/parameters/adj
sqlite3 /data/data/com.android.providers.settings/databases/settings.db "INSERT INTO secure (name, value) VALUES ('wifi_country_code', 'JP');"
/system/xbin/zram
/system/xbin/fix
sh /system/engine/boost.sh
sh /system/engine/adblock.sh
echo "FeraDroid Engine >> Ready"
fi
/system/xbin/sysro
sleep 45
sh /system/engine/boost.sh
sleep 45
sh /system/engine/boost.sh
