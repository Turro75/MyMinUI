#!/bin/sh
# NOTE: becomes r36s.sh

PLATFORM="r36s"
FWNAME=MinUI
SDCARD_PATH="/MyMinUI"
UPDATE_PATH="${SDCARD_PATH}/MinUI.zip"
SYSTEM_PATH="${SDCARD_PATH}/.system"
LOGFILE="/roms/MyMinUI/log.txt"
echo "Start MyMinUI" > $LOGFILE
export SDL_NOMOUSE=1
echo 0 | sudo  tee /sys/class/graphics/fbcon/cursor_blink
sudo systemctl stop oga_events

if [ -e ${SDCARD_PATH}/bootfailed.txt ]; then
	sudo rm -rf ${SDCARD_PATH}/bootfailed.txt
fi
if [ -e $SDCARD_PATH ]; then
	sudo rm -rf $SDCARD_PATH
fi

echo "Listing /dev/input/by-path/* devices:" >> $LOGFILE
ls -l /dev/input/by-path/* >> $LOGFILE


echo "Checking TF2 slot presence" >> $LOGFILE
TF1DISKNUM=$(mount | grep "/ type ext4" | cut -d'p' -f1 | cut -d'k' -f2)
TF2DISKNUM=$((${TF1DISKNUM}+1))
echo "TF1 disk num is $TF1DISKNUM" >> $LOGFILE
echo "TF2 disk num is $TF2DISKNUM" >> $LOGFILE
TF2PATH="/dev/mmcblk${TF2DISKNUM}p1"
if [ -e $TF2PATH ]; then
	echo "TF2 detected at $TF2PATH" >> $LOGFILE
	sudo mkdir -p $SDCARD_PATH
	sudo chmod 777 $SDCARD_PATH
	sudo chown  ark:ark $SDCARD_PATH 
	sudo mount $TF2PATH $SDCARD_PATH -o rw,defaults,noatime,uid=1002,gid=1002,fmask=0000,dmask=0000,errors=remount-ro
	mv $LOGFILE $SDCARD_PATH/log.txt
	LOGFILE="$SDCARD_PATH/log.txt"
else
	echo "TF2 not detected, using TF1 slot" >> $LOGFILE
	echo "generating symlink from /roms/MyMinUI -> $SDCARD_PATH" >> $LOGFILE
	ln -s /roms/MyMinUI $SDCARD_PATH
fi

#echo "Listing content of $SDCARD_PATH" >> $LOGFILE
#ls -al $SDCARD_PATH/* >> $LOGFILE
#ls $SDCARD_PATH/.system/r36s/* >> $LOGFILE
echo "listing mounts:" >> $LOGFILE
sudo mount >> $LOGFILE
echo "Listing /dev/mmcblk* devices:" >> $LOGFILE
ls -l /dev/mmcblk* >> $LOGFILE

dd if=/dev/zero of=/dev/fb0 bs=1228800 count=1

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
	
else
	echo "No release file found on ${SDCARD_PATH}/My${FWNAME}" >> $LOGFILE
fi

echo "Checking for $UPDATE_PATH update file" >> $LOGFILE
#same as original MinUI install/update process
if [ -f "$UPDATE_PATH" ]; then
	echo "Found update file $UPDATE_PATH" >> $LOGFILE
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
else
	echo "No update file found on $UPDATE_PATH" >> $LOGFILE
fi

export PATH=/bin:/sbin:/usr/bin:/usr/sbin
export LD_LIBRARY_PATH=/usr/lib/:/lib/
export HOME=$SDCARD_PATH

# add custom extra folders for advanced systems not present in standard MyMinUI
sudo mkdir -p "${SDCARD_PATH}/Roms/PSP (PSP)"; 
sudo mkdir -p "${SDCARD_PATH}/Bios/PSP";
sudo mkdir -p "${SDCARD_PATH}/Roms/Nintendo 64 (N64)"; 
sudo mkdir -p "${SDCARD_PATH}/Bios/N64";
sudo mkdir -p "${SDCARD_PATH}/Roms/Dreamcast (DC)"; 
sudo mkdir -p "${SDCARD_PATH}/Bios/DC";

dd if=/dev/zero of=/dev/fb0 bs=1228800 count=1

ls -l ${SYSTEM_PATH}/${PLATFORM}/paks/MinUI.pak/* >> $LOGFILE

if [ -f ${SYSTEM_PATH}/${PLATFORM}/paks/MinUI.pak/launch.sh ] ; then
    echo "Launching MyMinUI" >> $LOGFILE
    sudo ${SYSTEM_PATH}/${PLATFORM}/paks/MinUI.pak/launch.sh
else
    echo "Error: launch.sh not found!" >> $LOGFILE
    echo "Exiting" > ${SDCARD_PATH}/bootfailed.txt
fi
    
#umount ${ROOTFS_MOUNTPOINT}
sync
shutdown now # under no circumstances should stock be allowed to touch this card
