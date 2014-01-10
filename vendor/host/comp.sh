export ATH_BUILD_TYPE=QUALCOMM_ARM_NATIVEMMC
export ATH_BSP_TYPE=QSD8K_BSP3200
export TARGET_TYPE=AR6002

while [ "$#" -eq 0 ]
do
	echo "Usage: "
	echo "    ./comp.sh 1 : make out ar6000.ko"
	echo "	  ./comp.sh 2 : make clean " 
	exit -1
done

case $1 in
	1)
		make V=1
        	;;
	2)
		make clean
		rm -rf .output/${PLATFORM_NAME}/image/*
        	;;
	*)
		echo "Unsupported argument"
		exit -1
esac


