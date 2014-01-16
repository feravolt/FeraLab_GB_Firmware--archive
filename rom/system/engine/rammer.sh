#!/system/bin/sh
### FeraDroid Engine v16 | By FeraVolt. 2013###

while true;
do

ram=$((`free | awk '{ print $3 }' | sed -n 2p`/1024))
ramkbytescached=`cat /proc/meminfo | grep Cached | awk '{print $2}' | sed -n 1p`
ramkbytesfree=`free | awk '{ print $4 }' | sed -n 2p`
ramfree=$(($ramkbytesfree/1024))
ramcached=$(($ramkbytescached/1024))
ramreportedfree=$(($ramfree + $ramcached))
ramm=$(($ram + $ramfree))

old='360'
hmm='512'
nne='760'

if [[ "$ramm" -le "$old" ]]
then
echo "Device with $ramm MB RAM"
critical='7'
low='12'
better='18'
good='27'
fi

if [[ "$ramm" -gt "$old" ]] && [[ "$ramm" -le "$hmm" ]]
then
echo "Device with $ramm MB RAM"
critical='8'
low='16'
better='27'
good='32'
fi

if [[ "$ramm" -gt "$hmm" ]] && [[ "$ramm" -le "$nne" ]]
then
echo "Device with $ramm MB RAM"
critical='16'
low='27'
better='32'
good='48'
fi

if [[ "$ramm" -gt "$nne" ]]
then
echo "Device with $ramm MB RAM"
critical='16'
low='32'
better='48'
good='96'
fi

clean=0
sleep 1
   	if [[ $clean == 1 ]]
	then
		echo "Processing.."
		sync;
		sleep 1
		echo 3 > /proc/sys/vm/drop_caches
		sleep 3
		sync;
		sleep 1
		echo 1 > /proc/sys/vm/drop_caches
		sleep 2
		sync;
		sleep 2
	  if [[ "$ramfree" -ge "$good" ]]
		then
			clean=0
	  fi
	fi

	if [[ "$ramfree" -le "$critical" ]]
   then
		echo " "
		echo "RAM is critical! Only $ramfree MB Free!"
		clean=1
   fi

   if [[ "$ramfree" -le "$low" ]] && [[ $clean == 1 ]]
   then
		echo " "
		echo "Only $ramfree MB Free! RAM is very low!"
		clean=1
		ramkbytesfree=`free | awk '{ print $4 }' | sed -n 2p`
		ramfree=$(($ramkbytesfree/1024))
		echo "Free RAM now: $ramfree MB"
   fi

   if [[ "$ramfree" -le "$better" ]] && [[ "$ramfree" -gt "$low" ]] && [[ $clean == 1 ]]
   then
		echo " "
		echo "RAM is better. $ramfree MB Free left."
		clean=1
		ramkbytesfree=`free | awk '{ print $4 }' | sed -n 2p`
		ramfree=$(($ramkbytesfree/1024))
		echo "Free RAM now: $ramfree MB"
   fi

   if [[ "$ramfree" -ge "$good" ]]
   then
		echo " "
		echo "RAM optimzied."
		clean=0
   fi
   sleep 1
	echo " "
	echo "Current RAM values are:"
	echo " "
	echo "Total:                        $ramm MB"
	echo "Used:                         $ram MB"
	echo "Currently cached:             $ramcached MB"
	echo "Reported Free:                $ramreportedfree MB"
	echo "Real Free:                    $ramfree MB"
	echo " "
	sleep 50
done

