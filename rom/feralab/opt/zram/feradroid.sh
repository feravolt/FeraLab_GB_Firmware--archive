#!/system/xbin/sh

echo "FeraDroid Engine"
echo "version 16.02"
echo "By FeraVolt."

if [ -e /system/usr/vendor/prop/firstboot ];
then
   sysrw
   sh /system/engine/fix.sh
   sh /system/engine/installbusybox.sh
   sysrw
   rm -f /system/usr/vendor/prop/firstboot
   rm -f /system/usr/vendor/prop/notferalab
   boost
   sleep 63
   rm -f /system/engine/installbusybox.sh
   exit
else

sysrw
/system/xbin/run-parts /system/engine/tweaks
chmod 644 /system/build.prop
chmod -R 777 /system/etc/sysctl.conf
chmod 777 /system/etc/sysctl.conf
chmod 000 /data/tombstones
rm -f /data/cache/*.apk
rm -f /data/cache/*.tmp
rm -f /data/dalvik-cache/*.apk
rm -f /data/dalvik-cache/*.tmp
rm -Rf /system/lost+found/*
rm -f /data/tombstones/*
rm -Rf /mnt/sdcard/LOST.DIR/*
rm -Rf /mnt/sdcard/LOST.DIR
rm -Rf /mnt/sdcard/found000/*
rm -Rf /mnt/sdcard/found000
rm -f /mnt/sdcard/fix_permissions.log
sqlite3 /data/data/com.android.providers.settings/databases/settings.db "INSERT INTO secure (name, value) VALUES ('wifi_country_code', 'JP');"
sysctl -p
renice -18 `pidof com.android.phone`
renice -18 `pgrep com.sonyericsson.home`
nozram
fix
sh /system/engine/rammer.sh
echo "FeraDroid Engine >> Ready"

fi
