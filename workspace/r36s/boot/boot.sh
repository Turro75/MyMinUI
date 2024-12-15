#!/bin/sh
# NOTE: becomes r36s.sh

PLATFORM="r36s"
FWNAME=MinUI
SDCARD_PATH="/MyMinUI"
UPDATE_PATH="${SDCARD_PATH}/MinUI.zip"
SYSTEM_PATH="${SDCARD_PATH}/.system"
LOGFILE="/roms/MyMinUI/log.txt"
echo "Start" > $LOGFILE
export LD_LIBRARY_PATH=${SDCARD_PATH}/${PLATFORM}/libmusl:$LD_LIBRARY_PATH
export SDL_NOMOUSE=1
echo 0 | sudo  tee /sys/class/graphics/fbcon/cursor_blink

if [ -e $SDCARD_PATH ]; then
	sudo rm -rf $SDCARD_PATH
fi

#check if a second SDCARD is inserted
if [ -e /dev/mmcblk1p1 ]; then
	 sudo mkdir -p $SDCARD_PATH
	 sudo mount /dev/mmcblk1p1 $SDCARD_PATH	
else
	 ln -s /roms/MyMinUI $SDCARD_PATH
fi
sudo mount >> $LOGFILE
#ps ax >> $LOGFILE
#ls $SDCARD_PATH/ >> $LOGFILE
# chmod 777 $SDCARD_PATH
# install/update
# is there an update available?
if [ -f ${SDCARD_PATH}/My${FWNAME}-*-${PLATFORM}.zip ]; then
        NEWFILE=$(ls ${SDCARD_PATH}/My${FWNAME}-*-${PLATFORM}.zip)
	#echo "Trovato release file" >> $LOGFILE
	#echo "Sono nella directory " $(pwd) >> $LOGFILE

	if [ -d "${SYSTEM_PATH}/${PLATFORM}" ]; then
	    ACTION="updating"
	else
	    ACTION="installing"
	fi
	sudo  ${SDCARD_PATH}/${PLATFORM}/show.elf ${SDCARD_PATH}/${PLATFORM}/$ACTION.png >> $LOGFILE
	#echo "Found Release file $NEWFILE ! ACTION = $ACTION" >> $LOGFILE
        sudo ${SDCARD_PATH}/${PLATFORM}/unzip -o $NEWFILE -d $SDCARD_PATH -x "r36s/*" >> $LOGFILE
	sync

	#remove useless dirs
#	rm -rf $SDCARD_PATH/rg35xx
	sudo rm -rf $SDCARD_PATH/trimui
#	rm -rf $SDCARD_PATH/miyoo354
	rm -rf $NEWFILE
	sync

	echo "End phase 1" >> $LOGFILE
	
fi


#same as original MinUI install/update process
if [ -f "$UPDATE_PATH" ]; then

	if [ -d "${SYSTEM_PATH}/${PLATFORM}" ]; then
	    ACTION="updating"
	else
	    ACTION="installing"
	fi
	sudo ${SDCARD_PATH}/${PLATFORM}/show.elf ${SDCARD_PATH}/${PLATFORM}/$ACTION.png
	#echo "Found Release file $NEWFILE ! ACTION = $ACTION" >> $LOGFILE
        sudo ${SDCARD_PATH}/${PLATFORM}/unzip -o $UPDATE_PATH -d $SDCARD_PATH #&>> $LOGFILE
	sync
	# the updated system finishes the install/update
	sudo rm -rf ${UPDATE_PATH}
	sudo $SYSTEM_PATH/$PLATFORM/bin/install.sh

fi

#ROOTFS_MOUNTPOINT=/overlay
#ROOTFS_IMG=${SYSTEM_PATH}/${PLATFORM}/rootfs.ext2
#${SDCARD_PATH}/${PLATFORM}/binmusl/fuse2fs ${ROOTFS_IMG} ${ROOTFS_MOUNTPOINT} 2&> /dev/null

#sync
#ls -l ${ROOTFS_MOUNTPOINT}/* >> /mnt/SDCARD/ls.txt && sync

#if [ -f ${ROOTFS_MOUNTPOINT}/bin/busybox ]; then
#    rm -rf ${ROOTFS_MOUNTPOINT}/tmp/*
#    mkdir -p ${ROOTFS_MOUNTPOINT}/mnt/SDCARD
#    if [ ! -f ${ROOTFS_MOUNTPOINT}/etc/asound.conf ]; then
#	cp /etc/asound.conf ${ROOTFS_MOUNTPOINT}/etc/asound.conf
#	chmod 666 ${ROOTFS_MOUNTPOINT}/etc/asound.conf
#	sync
 #   fi

#    if [ ! -f ${ROOTFS_MOUNTPOINT}/etc/fb.modes ]; then
#	cp $SDCARD_PATH/fb.modes ${ROOTFS_MOUNTPOINT}/etc/fb.modes
#	chmod 666 ${ROOTFS_MOUNTPOINT}/etc/fb.modes
#	sync
#    fi



#mount all other fs as minui on rg35xx og
#    for f in dev dev/pts proc sys mnt/SDCARD tmp
#    do
#	mount -o bind /${f} ${ROOTFS_MOUNTPOINT}/${f}
#    done
    export PATH=/bin:/sbin:/usr/bin:/usr/sbin
    export LD_LIBRARY_PATH=/usr/lib/:/lib/
    export HOME=$SDCARD_PATH

# add custom extra folders for advanced systems not present in standard MyMinUI
	if [ ! -d "${SDCARD_PATH}/Roms/PSP (PSP)" ]; then 
		sudo mkdir -p "${SDCARD_PATH}/Roms/PSP (PSP)"; 
		sudo mkdir -p "${SDCARD_PATH}/Bios/PSP";	
		fi
	if [ ! -d "${SDCARD_PATH}/Roms/Nintendo 64 (N64)" ]; then 
		sudo mkdir -p "${SDCARD_PATH}/Roms/Nintendo 64 (N64)"; 
		sudo mkdir -p "${SDCARD_PATH}/Bios/N64";
	fi
	if [ ! -d "${SDCARD_PATH}/Roms/Dreamcast (DC)" ]; then 
		sudo mkdir -p "${SDCARD_PATH}/Roms/Dreamcast (DC)"; 
		sudo mkdir -p "${SDCARD_PATH}/Bios/DC";
	fi

	dd if=/dev/zero of=/dev/fb0 bs=1228800 count=1
#evaluate if adding swap file or not
#    ls -l ${SYSTEM_PATH}/${PLATFORM}/paks/MinUI.pak/* >> $LOGFILE
    sudo ${SYSTEM_PATH}/${PLATFORM}/paks/MinUI.pak/launch.sh
    sync
fi


#umount ${ROOTFS_MOUNTPOINT}
sync

echo "Exiting" > ${SDCARD_PATH}/bootfailed.txt

sync
shutdown now # under no circumstances should stock be allowed to touch this card
