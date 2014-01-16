#!/system/bin/sh
### FeraDroid Engine v16 | By FeraVolt. 2013

sysrw

zipalign="yes"
clear
sleep 1
id=`id`; id=`echo ${id#*=}`; id=`echo ${id%%\(*}`; id=`echo ${id%%gid*}`
if [ "$id" != "0" ] && [ "$id" != "root" ]; then
	sleep 1
	echo "You are NOT running this script as root"
	sleep 1
	exit 69
elif [ ! "`which zipalign`" ]; then
	zipalign=
	sleep 1
	echo "Zipalign binary was NOT found."
	echo ""
	sleep 1
fi 2>/dev/null
busybox mount -o remount,rw /data 2>/dev/null
busybox mount -o remount,rw /system 2>/dev/null
busybox mount -o remount,rw `busybox mount | grep system | awk '{print $1,$3}' | sed -n 1p` 2>/dev/null
rm /data/fixaligntemp 2>/dev/null
START=`busybox date +%s`
BEGAN=`date`
TOTAL=`cat /d*/system/packages.xml | grep -E "^<package.*serId" | wc -l`
INCREMENT=3
PROGRESS=0
PROGRESS_BAR=""
echo "Start Fix Alignment: $BEGAN"
if [ "$zipalign" ];
 then echo " Start Wheel Alignment ( \"ZepAlign\" ): $BEGAN" 
fi
sync

cat /d*/system/packages.xml | grep -E "^<package.*serId" | while read pkgline; do
	if [ ! -f "/data/fixaligntemp" ]; then ALIGNED=0; FAILED=0; ALREADY=0; SKIPPED=0; fi
	PKGNAME=`echo $pkgline | sed 's%.* name="\(.*\)".*%\1%' | cut -d '"' -f1`
	CODEPATH=`echo $pkgline | sed 's%.* codePath="\(.*\)".*%\1%' |  cut -d '"' -f1`
	DATAPATH=/d*/d*/$PKGNAME
	PKGUID=`echo $pkgline | sed 's%.*serId="\(.*\)".*%\1%' | cut -d '"' -f1`
	PROGRESS=$(($PROGRESS+1))
	PERCENT=$(( $PROGRESS * 100 / $TOTAL ))
	if [ "$PERCENT" -eq "$INCREMENT" ]; then
		INCREMENT=$(( $INCREMENT + 3 ))
		PROGRESS_BAR="$PROGRESS_BAR="
	fi
	clear
	echo ""
	echo -n "                                        >"
	echo -e "\r       $PROGRESS_BAR>"
	echo "       \"Fix Alignment\"                    "
	echo -n "                                        >"
	echo -e "\r       $PROGRESS_BAR>"
	echo ""
	echo "       Processing Apps - $PERCENT% ($PROGRESS of $TOTAL)"
	echo " Fix Aligning $PKGNAME..."
	echo ""
	if [ -e "$CODEPATH" ]; then
		if [ "$zipalign" ]; then
			if [ "$(busybox basename $CODEPATH )" = "framework-res.apk" ] || [ "$(busybox basename $CODEPATH )" = "com.htc.resources.apk" ]; then
				echo " NOT ZipAligning (Problematic) $CODEPATH..."
				SKIPPED=$(($SKIPPED+1))
			else
				zipalign -c 4 $CODEPATH
				ZIPCHECK=$?
				if [ "$ZIPCHECK" -eq 1 ]; then
					echo " ZipAligning $CODEPATH..."
					zipalign -f 4 $CODEPATH /cache/$(busybox basename $CODEPATH )
					rc="$?"
					if [ "$rc" -eq 0 ]; then
						if [ -e "/cache/$(busybox basename $CODEPATH )" ]; then
							busybox cp -f -p /cache/$(busybox basename $CODEPATH ) $CODEPATH
							ALIGNED=$(($ALIGNED+1))
						else
							echo " ZipAligning $CODEPATH... Failed (No Output File!)"
							FAILED=$(($FAILED+1))
						fi
					else echo "ZipAligning $CODEPATH... Failed (rc: $rc!)"
						FAILED=$(($FAILED+1))
					fi
					if [ -e "/cache/$(busybox basename $CODEPATH )" ]; then busybox rm /cache/$(busybox basename $CODEPATH ); fi
				else
					echo " ZipAlign already completed on $CODEPATH "
					ALREADY=$(($ALREADY+1))
				fi
				echo "$ALIGNED $FAILED $ALREADY $SKIPPED" > /data/fixaligntemp
			fi
		fi
		APPDIR=`busybox dirname $CODEPATH`
		if [ "$APPDIR" = "/system/app" ] || [ "$APPDIR" = "/vendor/app" ] || [ "$APPDIR" = "/system/framework" ]; then
			busybox chown 0 $CODEPATH
			busybox chown :0 $CODEPATH
			busybox chmod 644 $CODEPATH
		elif [ "$APPDIR" = "/data/app" ]; then
			busybox chown 1000 $CODEPATH
			busybox chown :1000 $CODEPATH
			busybox chmod 644 $CODEPATH
		elif [ "$APPDIR" = "/data/app-private" ]; then
			busybox chown 1000 $CODEPATH
			busybox chown :$PKGUID $CODEPATH
			busybox chmod 640 $CODEPATH
		fi
		if [ -d "$DATAPATH" ]; then
			busybox chmod 755 $DATAPATH
			busybox chown $PKGUID $DATAPATH
			busybox chown :$PKGUID $DATAPATH
			DIRS=`busybox find $DATAPATH -mindepth 1 -type d`
			for file in $DIRS; do
				PERM=755
				NEWUID=$PKGUID
				NEWGID=$PKGUID
				FNAME=`busybox basename $file`
				case $FNAME in
							lib)busybox chmod 755 $file
								NEWUID=1000
								NEWGID=1000
								PERM=755;;
				   shared_prefs)busybox chmod 771 $file
								PERM=660;;
					  databases)busybox chmod 771 $file
								PERM=660;;
						  cache)busybox chmod 771 $file
								PERM=600;;
						  files)busybox chmod 771 $file
								PERM=775;;
							  *)busybox chmod 771 $file
								PERM=771;;
				esac
				busybox chown $NEWUID $file
				busybox chown :$NEWGID $file
				busybox find $file -type f -maxdepth 2 ! -perm $PERM -exec busybox chmod $PERM {} ';'
				busybox find $file -type f -maxdepth 1 ! -user $NEWUID -exec busybox chown $NEWUID {} ';'
				busybox find $file -type f -maxdepth 1 ! -group $NEWGID -exec busybox chown :$NEWGID {} ';'
			done
		fi
		echo " Fixed Permissions..."
	fi 2>/dev/null
done
sync
echo $line
busybox mount -o remount,ro /system 2>/dev/null
busybox mount -o remount,ro `busybox mount | grep system | awk '{print $1,$3}' | sed -n 1p` 2>/dev/null
STOP=`busybox date +%s`
ENDED=`date`
RUNTIME=`busybox expr $STOP - $START`
HOURS=`busybox expr $RUNTIME / 3600`
REMAINDER=`busybox expr $RUNTIME % 3600`
MINS=`busybox expr $REMAINDER / 60`
SECS=`busybox expr $REMAINDER % 60`
RUNTIME=`busybox printf "%02d:%02d:%02d\n" "$HOURS" "$MINS" "$SECS"`
if [ "$zipalign" ]; then
	ALIGNED=`awk '{print $1}' /data/fixaligntemp`
	FAILED=`awk '{print $2}' /data/fixaligntemp`
	ALREADY=`awk '{print $3}' /data/fixaligntemp`
	SKIPPED=`awk '{print $4}' /data/fixaligntemp`
	sleep 1
	rm /data/fixaligntemp
	echo " Done \"ZepAligning\" ALL data and system APKs..."
	echo ""
	sleep 1
 	echo " $TOTAL Apps were processed!"
 	echo ""
 	echo " $ALIGNED Apps were zipaligned..."
	echo " $FAILED Apps were NOT zipaligned due to error..."
	echo " $SKIPPED Apps were skipped..."
	echo " $ALREADY Apps were already zipaligned..."
 	sleep 1
	echo ""
	echo $line
fi
echo ""
sleep 1
echo " FIXED Permissions For ALL $TOTAL Apps..."
echo $line
echo ""
sleep 1
echo "      Start Time: $BEGAN"
echo "       Stop Time: $ENDED"
echo " Completion Time: $RUNTIME"
echo ""
sleep 1
if [ "$zipalign" ]; then echo "Done!"; fi
echo ""

