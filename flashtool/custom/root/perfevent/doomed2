#!/system/bin/sh
#
# DooMing Script: Part 2
# Part of Easy Rooting Toolkit by DooMLoRD@XDA
#
echo ---------------------------------------------------------------
echo   Launching final rooting process...
echo ---------------------------------------------------------------
echo "--- Rooting!"
echo "--- Remounting rootfs"
/data/local/tmp/busybox mount -o remount,rw /
echo "--- Killing RIC service (specific for new Xperia devices)"
/data/local/tmp/busybox pkill -f /system/bin/ric; /data/local/tmp/busybox pkill -f /sbin/ric; /data/local/tmp/busybox mount -o remount,rw /system; chmod 644 /system/bin/ric; chmod 644 /sbin/ric
echo "--- Installing busybox"
echo "--- Remounting /system"
/data/local/tmp/busybox mount -o remount,rw /system
echo "--- copying busybox to /system/xbin/"
dd if=/data/local/tmp/busybox of=/system/xbin/busybox
echo "--- correcting ownership"
chown root.shell /system/xbin/busybox
echo "--- correcting permissions"
chmod 04755 /system/xbin/busybox
echo "--- installing busybox"
/system/xbin/busybox --install -s /system/xbin
rm -r /data/local/tmp/busybox
echo "--- pushing SU binary"
rm /system/xbin/su
dd  if=/data/local/tmp/su of=/system/xbin/su
echo "--- correcting ownership"
chown root.shell /system/xbin/su
echo "--- correcting permissions"
chmod 06755 /system/xbin/su
echo "--- correcting symlinks"
rm /system/bin/su
ln -s /system/xbin/su /system/bin/su
echo "--- pushing Superuser app"
dd  if=/data/local/tmp/Superuser.apk of=/system/app/Superuser.apk
echo "--- correcting permissions"
chmod 644 /system/app/Superuser.apk
echo "--- DONE!"
exit
