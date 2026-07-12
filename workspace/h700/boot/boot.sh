#!/bin/sh

#become /mnt/mmc/dmenu.bin

TF1_PATH=/mnt/mmc # ROMS partition
TF2_PATH=/mnt/sdcard
PLATFORM=h700

LOGFILE=${TF1_PATH}/bootlog.txt

TF2_DEVICE=/dev/mmcblk1p1
SDCARD_PATH=$TF2_PATH
SYSTEM_DIR=/.system
SYSTEM_FRAG=${SYSTEM_DIR}/${PLATFORM}

if [ -L $TF2_PATH ]; then
	# TF2 is a symlink to TF1, delete it first
	umount $TF2_PATH
	rm -f $TF2_PATH
fi

if [ -e $TF2_DEVICE ]; then
	mkdir -p $TF2_PATH
	mount -t vfat,exfat -o rw,utf8,noatime $TF2_DEVICE $TF2_PATH
	mount | grep  -e $TF2_DEVICE -e $TF2_PATH > $LOGFILE
	if [ $? -ne 0 ]; then #failed to mount, create a link of /mnt/sdcardto /mnt/mmc
		rm -rf $TF2_PATH
		ln -s $TF1_PATH $TF2_PATH
	fi
fi


if [ -d ${TF1_PATH}${SYSTEM_FRAG} ] && [ ! -L $TF2_PATH ]; then #.system found on TF1, so delete /mnt/sdcard even if TF2 is present and create a link to /mnt/mmc
	rm -rf $TF2_PATH
	ln -s $TF1_PATH $TF2_PATH
fi

#ok now /mnt/sdcard points to /mnt/mmc if .system is present on TF1, otherwise it points to /mnt/sdcard where /dev/mmcbl1p1 is mounted
#time to switch to stage2 so this script does not need to be updated

if [ -e ${SDCARD_PATH}/${PLATFORM}/h700_stage2.sh ]; then
	#echo "Switching to stage2" >> $LOGFILE
	#echo "Switching to stage2" 
	${SDCARD_PATH}/${PLATFORM}/h700_stage2.sh >> $LOGFILE
else
	echo "Error: h700_stage2.sh not found!" >> $LOGFILE
	echo "Exiting" >> $LOGFILE
	/mnt/vendor/bin/dmenu.bin
fi

sync
exit 0