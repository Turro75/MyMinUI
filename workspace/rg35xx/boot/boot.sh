#!/system/bin/sh

/usbdbg.sh device

TF1_PATH=/mnt/mmc # ROMS partition
TF2_PATH=/mnt/sdcard
SDCARD_PATH=$TF1_PATH
SYSTEM_DIR=/.system
SYSTEM_FRAG=${SYSTEM_DIR}/rg35xx
UPDATE_FRAG=/MinUI.zip
SYSTEM_PATH=${SDCARD_PATH}${SYSTEM_FRAG}
UPDATE_PATH=${SDCARD_PATH}${UPDATE_FRAG}

mkdir /mnt/sdcard
if [ -e /dev/block/mmcblk1p1 ]; then
	SDCARD_DEVICE=/dev/block/mmcblk1p1
else
	SDCARD_DEVICE=/dev/block/mmcblk1
fi
mount -t vfat -o rw,utf8,noatime $SDCARD_DEVICE /mnt/sdcard
if [ $? -ne 0 ]; then
	mount -t exfat -o rw,utf8,noatime $SDCARD_DEVICE /mnt/sdcard
	if [ $? -ne 0 ]; then
		rm -rf $TF2_PATH
		ln -s $TF1_PATH $TF2_PATH
	fi
fi

if [ -d ${TF1_PATH}${SYSTEM_FRAG} ] || [ -f ${TF1_PATH}${UPDATE_FRAG} ]; then
	if [ ! -L $TF2_PATH ]; then
		# .system found on TF1 but TF2 is present
		# so unmount and symlink to TF1 path
		umount $TF2_PATH
		rm -rf $TF2_PATH
		ln -s $TF1_PATH $TF2_PATH
	fi
fi

SDCARD_PATH=$TF2_PATH
SYSTEM_PATH=${SDCARD_PATH}${SYSTEM_FRAG}
PLATFORM=rg35xx
FWNAME=MinUI
UPDATE_PATH=${SDCARD_PATH}${UPDATE_FRAG}
#SKIPS="trimui miyoo miyoo354 \".tmp_update\""
#LOGFILE=${SDCARD_PATH}/log.txt
SWAPFILE=${SYSTEM_PATH}/myswapfile
OLDMD5_PATH=${SYSTEM_PATH}/minui.md5

# is there an update available?
if [ -f ${SDCARD_PATH}/My${FWNAME}-*-${PLATFORM}.zip ]; then
    NEWFILE=$(ls ${SDCARD_PATH}/My${FWNAME}-*-${PLATFORM}.zip)
    FLAG_PATH=/misc/.minstalled
	if [ ! -f $FLAG_PATH ]; then
	   	ACTION=installing
	else
	    ACTION=updating
	fi
	# extract the zip file appended to the end of this script to tmp
	# and display one of the two images it contains 
	CUT=$((`busybox grep -n '^BINARY' $0 | busybox cut -d ':' -f 1 | busybox tail -1` + 1))
	busybox tail -n +$CUT "$0" | busybox uudecode -o /tmp/data
	busybox unzip -o /tmp/data -d /tmp
	busybox fbset -g 640 480 640 480 16
	dd if=/tmp/${ACTION} of=/dev/fb0
	sync

	#echo "Found Release file $NEWFILE ! ACTION = $ACTION" >> $LOGFILE
        busybox unzip -o $NEWFILE -d $SDCARD_PATH #&>> $LOGFILE
	sync
	#remove useless dirs for myminui
#       rm -rf ${SDCARD_PATH}/miyoo
#	rm -rf ${SDCARD_PATH}/miyoo354
	rm -rf ${SDCARD_PATH}/trimui
	
	rm -rf $NEWFILE
	sync
	#update dmenu.bin if changed then reboot otherwise continue
	oldfilemd5=$(md5 /misc/dmenu.bin | busybox cut -d" " -f0 )
	newfilemd5=$(md5 ${SDCARD_PATH}/${PLATFORM}/dmenu.bin | busybox cut -d" " -f0 )
	#echo "OLD = $oldfilemd5" >  $SDCARD_PATH/md5.txt
	#echo "NEW = $newfilemd5" >> $SDCARD_PATH/md5.txt
	if [ "$oldfilemd5" != "$newfilemd5" ]; then
		mount -o remount,rw /dev/block/actb /misc
		cp ${SDCARD_PATH}/${PLATFORM}/dmenu.bin /misc/dmenu.bin
		#echo "replacing dmenu.bin" >> $SDCARD_PATH/md5.txt
		sync && reboot
	fi 
fi

#same as original MinUI install/update process
if [ -f $UPDATE_PATH ]; then
	rm -rf ${SDCARD_PATH}/${PLATFORM}	
	FLAG_PATH=/misc/.minstalled
	if [ ! -f $FLAG_PATH ]; then
	   	ACTION=installing
	else
	    ACTION=updating
	fi
	# extract the zip file appended to the end of this script to tmp
	# and display one of the two images it contains 
	CUT=$((`busybox grep -n '^BINARY' $0 | busybox cut -d ':' -f 1 | busybox tail -1` + 1))
	busybox tail -n +$CUT "$0" | busybox uudecode -o /tmp/data
	busybox unzip -o /tmp/data -d /tmp
	busybox fbset -g 640 480 640 480 16
	dd if=/tmp/$ACTION of=/dev/fb0
	sync

	#check if is a real update
	NEWMD5=$(md5 ${UPDATE_PATH} | busybox cut -d" " -f0)
	OLDMD5=$(cat ${OLDMD5_PATH})
	UPDATED=0
	if [ "${NEWMD5}" != "${OLDMD5}" ]; then
		#real update, proceed
		UPDATED=1
		echo $NEWMD5 > $OLDMD5_PATH
		busybox unzip -o $UPDATE_PATH -d $SDCARD_PATH
	fi
#	preserve some dir needed by other devices
#	rm -rf ${SDCARD_PATH}/.tmp_update
	rm -rf $UPDATE_PATH
	sync
	
	if [ -f ${SWAPFILE}.gz ]; then
	#if the swapfile does not exists, let's create it
		if [ ! -f $SWAPFILE ]; then
			busybox gzip -d ${SWAPFILE}.gz
			sync
			chmod 600 $SWAPFILE
			mkswap $SWAPFILE
		fi
		rm -rf ${SWAPFILE}.gz
		sync
	fi
	# the updated system finishes the install/update
	if [ $UPDATED -eq 1 ]; then
		${SYSTEM_PATH}/bin/install.sh  #&>> $LOGFILE
	fi
	if [ -e $SDCARD_PATH/.userdata/$PLATFORM/msettings.bin ]; then
		rm $SDCARD_PATH/.userdata/$PLATFORM/msettings.bin
	fi
	
fi

if [ -f $SWAPFILE ]; then
	swapon $SWAPFILE
fi


ROOTFS_IMAGE=$SYSTEM_PATH/rootfs.ext2
if [ ! -f $ROOTFS_IMAGE ]; then
	# fallback to stock demenu.bin, based on dmenu_ln
	ACT="/tmp/.next"
	CMD="/mnt/vendor/bin/dmenu.bin"
	touch "$ACT"
	while [ -f $CMD ]; do
		if $CMD; then
			if [ -f "$ACT" ]; then
				if  ! sh $ACT; then
					echo
				fi
				rm -f "$ACT"
			fi
		fi
	done
	sync && reboot -p
fi

ROOTFS_MOUNTPOINT=/cfw
LOOPDEVICE=/dev/block/loop7
mkdir $ROOTFS_MOUNTPOINT
busybox losetup $LOOPDEVICE $ROOTFS_IMAGE
mount -r -w -o loop -t ext4 $LOOPDEVICE $ROOTFS_MOUNTPOINT
rm -rf $ROOTFS_MOUNTPOINT/tmp/*
mkdir $ROOTFS_MOUNTPOINT/mnt/mmc
mkdir $ROOTFS_MOUNTPOINT/mnt/sdcard
for f in dev dev/pts proc sys mnt/mmc mnt/sdcard # tmp doesn't work for some reason?
do
	mount -o bind /$f ${ROOTFS_MOUNTPOINT}/$f
done

export PATH=/usr/sbin:/usr/bin:/sbin:/bin:$PATH
export LD_LIBRARY_PATH=/usr/lib/:/lib/
export HOME=$SDCARD_PATH

busybox chroot $ROOTFS_MOUNTPOINT ${SYSTEM_PATH}/paks/MinUI.pak/launch.sh

umount $ROOTFS_MOUNTPOINT
busybox losetup --detach $LOOPDEVICE
sync && reboot -p

exit 0

