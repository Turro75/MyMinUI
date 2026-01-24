#!/bin/sh
# NOTE: becomes emulationstation
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
MOUNTEDTMP=$( mount | grep "rootfs.ext2" )
#echo $MOUNTEDTMP >> /mnt/SDCARD/log.txt
if [ "${MOUNTEDTMP}" = "" ]; then
#    echo "NOT YET MOUNTED" >> /mnt/SDCARD/log.txt
    MOUNTED=0
else 
#    echo "ALREADY MOUNTED" >> /mnt/SDCARD/log.txt
    MOUNTED=1
fi

PLATFORM="m21"
FWNAME=MinUI
SDCARD_PATH="/mnt/SDCARD"
UPDATE_PATH="${SDCARD_PATH}/MinUI.zip"
SYSTEM_PATH="${SDCARD_PATH}/.system"

export LD_LIBRARY_PATH=${SDCARD_PATH}/${PLATFORM}/libmusl:$LD_LIBRARY_PATH
export SDL_NOMOUSE=1
#export NEWDTB=0

CPU_PATH=/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
#echo performance > "$CPU_PATH"
PID=-1
# install/update
# is there an update available?
if [ -f ${SDCARD_PATH}/My${FWNAME}-*-${PLATFORM}.zip ]; then
        NEWFILE=$(ls ${SDCARD_PATH}/My${FWNAME}-*-${PLATFORM}.zip)
#	echo "Trovato release file" >> $SDCARD_PATH/log.txt
#	echo "Sono nella directory " $(pwd) >> $SDCARD_PATH/log.txt

	if [ -d "${SYSTEM_PATH}/${PLATFORM}" ]; then
	    ACTION="updating"
	else
	    ACTION="installing"
	fi
	${SDCARD_PATH}/${PLATFORM}/binmusl/show.elf ${SDCARD_PATH}/${PLATFORM}/$ACTION.png 60 &
	PID=$!
#	echo $PID >>  ${SDCARD_PATH}/ciao.txt
	#echo "Found Release file $NEWFILE ! ACTION = $ACTION" >> $LOGFILE
        ${SDCARD_PATH}/${PLATFORM}/binmusl/unzip -o $NEWFILE -d $SDCARD_PATH -x "m21/*" #&>> $LOGFILE
	sync

	#remove useless dirs
#	rm -rf $SDCARD_PATH/rg35xx
	rm -rf $SDCARD_PATH/trimui
#	rm -rf $SDCARD_PATH/miyoo354
	rm -rf $NEWFILE
	sync
#	echo "Finita fase 1" >> $SDCARD_PATH/log.txt
	
fi


#same as original MinUI install/update process
if [ -f "$UPDATE_PATH" ]; then

	if [ $PID -eq -1 ]; then
		if [ -d "${SYSTEM_PATH}/${PLATFORM}" ]; then
		    ACTION="updating"
		else
		    ACTION="installing"
		fi
		${SDCARD_PATH}/${PLATFORM}/binmusl/show.elf ${SDCARD_PATH}/${PLATFORM}/$ACTION.png 60 &
		PID=$!
	fi
	#echo "Found Release file $NEWFILE ! ACTION = $ACTION" >> $LOGFILE
        ${SDCARD_PATH}/${PLATFORM}/binmusl/unzip -o $UPDATE_PATH -d $SDCARD_PATH #&>> $LOGFILE
	sync
	# the updated system finishes the install/update
	rm -rf ${UPDATE_PATH}
	$SYSTEM_PATH/$PLATFORM/bin/install.sh
fi

if [ $PID -ne -1 ];then
    kill -3 $PID
fi

ROOTFS_MOUNTPOINT=/overlay
ROOTFS_IMG=${SYSTEM_PATH}/${PLATFORM}/rootfs.ext2

if [ "${MOUNTED}" = "0" ]; then
    ${SDCARD_PATH}/${PLATFORM}/binmusl/fuse2fs ${ROOTFS_IMG} ${ROOTFS_MOUNTPOINT} 2&> /dev/null
    sync
fi
#ls -l ${ROOTFS_MOUNTPOINT}/* >> /mnt/SDCARD/ls.txt && sync
if [ -f ${ROOTFS_MOUNTPOINT}/bin/busybox ]; then
    rm -rf ${ROOTFS_MOUNTPOINT}/tmp/*
    mkdir -p ${ROOTFS_MOUNTPOINT}/mnt/SDCARD
    if [ ! -f ${ROOTFS_MOUNTPOINT}/etc/asound.conf ]; then
	cp /etc/asound.conf ${ROOTFS_MOUNTPOINT}/etc/asound.conf
	chmod 666 ${ROOTFS_MOUNTPOINT}/etc/asound.conf
	sync
    fi

    if [ ! -f ${ROOTFS_MOUNTPOINT}/etc/fb.modes ]; then
	cp $SDCARD_PATH/fb.modes ${ROOTFS_MOUNTPOINT}/etc/fb.modes
	chmod 666 ${ROOTFS_MOUNTPOINT}/etc/fb.modes
	sync
    fi


    if [ "${MOUNTED}" = "0" ]; then
    #mount all other fs as minui on rg35xx og
	for f in dev dev/pts dev/usb-ffs/adb proc sys sys/kernel/debug sys/kernel/config mnt/SDCARD tmp
	do
		mount -o bind /${f} ${ROOTFS_MOUNTPOINT}/${f}
	done
    fi
    export PATH=/bin:/sbin:/usr/bin:/usr/sbin
    export LD_LIBRARY_PATH=/usr/lib/:/lib/
    export HOME=$SDCARD_PATH

    cat /dev/zero > /dev/fb0
#evaluate if adding swap file or not

    if [ "${MOUNTED}" = "1" ]; then
		killall -9 launch.sh
		killall -9 keymon.elf
		killall -9 minui.elf
		killall -9 minarch.elf
    fi
	if [ -f "/mnt/SDCARD/m21/thisism22" ]; then
		rm -f /mnt/SDCARD/m21/thisism22
	fi
	if [ "$0" = "/mnt/SDCARD/tomato" ]; then
		echo 1 > /mnt/SDCARD/m21/thisism22
	fi
    chroot $ROOTFS_MOUNTPOINT ${SYSTEM_PATH}/${PLATFORM}/paks/MinUI.pak/launch.sh #&> $SDCARD_PATH/chroot.txt
    sync
fi

#umount ${ROOTFS_MOUNTPOINT}
sync

echo "Exiting" > ${SDCARD_PATH}/bootfailed.txt

sync
halt # under no circumstances should stock be allowed to touch this card
