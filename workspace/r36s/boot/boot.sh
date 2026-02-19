#!/bin/sh
# NOTE: becomes r36s.sh

PLATFORM="r36s"
FWNAME=MinUI
SDCARD_PATH="/MyMinUI"
UPDATE_PATH="${SDCARD_PATH}/MinUI.zip"
SYSTEM_PATH="${SDCARD_PATH}/.system"
LOGFILE="/roms/MyMinUI/.userdata/${PLATFORM}/logs/bootlog.txt"
mkdir -p "/roms/MyMinUI/.userdata/${PLATFORM}/logs"
echo "Start MyMinUI" > $LOGFILE
export SDL_NOMOUSE=1
echo 0 | sudo  tee /sys/class/graphics/fbcon/cursor_blink

if [ -e ${SDCARD_PATH}/bootfailed.txt ]; then
	sudo rm -rf ${SDCARD_PATH}/bootfailed.txt
fi
if [ -e $SDCARD_PATH ]; then
	sudo rm -rf $SDCARD_PATH
fi

#echo "Listing /dev/input/by-path/* devices:" >> $LOGFILE
#ls -l /dev/input/by-path/* >> $LOGFILE


echo "Checking TF2 slot presence" >> $LOGFILE
#ArkOS
TF1DISKNUM=$(mount | grep "/ type ext4" | cut -d'p' -f1 | cut -d'k' -f2)
if [ -z "$TF1DISKNUM" ]; then
	#dArkOS
	TF1DISKNUM=$(mount | grep "/ type btrfs" | cut -d'p' -f1 | cut -d'k' -f2)
	echo "Detected dArkOS" >> $LOGFILE
	sudo systemctl stop ogage
else
	sudo systemctl stop oga_events
fi
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
	mv $LOGFILE $SDCARD_PATH/.userdata/${PLATFORM}/logs/bootlog.txt
	LOGFILE="$SDCARD_PATH/.userdata/${PLATFORM}/logs/bootlog.txt"
else
	echo "TF2 not detected, using TF1 slot" >> $LOGFILE
	echo "generating symlink from /roms/MyMinUI -> $SDCARD_PATH" >> $LOGFILE
	ln -s /roms/MyMinUI $SDCARD_PATH
fi

if [ -f $SDCARD_PATH/$PLATFORM/r36s_stage2.sh ]; then
    $SDCARD_PATH/$PLATFORM/r36s_stage2.sh
else
    echo "Error: $SDCARD_PATH/$PLATFORM/r36s_stage2.sh" >> $LOGFILE
    echo "Exiting" > ${SDCARD_PATH}/bootfailed.txt
fi
sync
sudo systemctl poweroff # under no circumstances should stock be allowed to touch this card
