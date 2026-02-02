#!/bin/sh

##SECOND STAGE, it runs on the sdcard which contains roms so it is easier editing it in case of updates

PLATFORM="r36s"
FWNAME=MinUI
SDCARD_PATH="/MyMinUI"
UPDATE_PATH="${SDCARD_PATH}/MinUI.zip"
SYSTEM_PATH="${SDCARD_PATH}/.system"
LOGFILE="$SDCARD_PATH/.userdata/$PLATFORM/logs/bootlog.txt"
mkdir -p "$SDCARD_PATH/.userdata/$PLATFORM/logs"
echo "Start MyMinUI second stage" > $LOGFILE
export SDL_NOMOUSE=1
PID=-1
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
	sudo LD_LIBRARY_PATH="${SDCARD_PATH}/${PLATFORM}:${LD_LIBRARY_PATH}"   ${SDCARD_PATH}/${PLATFORM}/show.elf ${SDCARD_PATH}/${PLATFORM}/$ACTION.png 60 &
	PID=$!
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
	if [ $PID -eq -1 ]; then
		sudo LD_LIBRARY_PATH="${SDCARD_PATH}/${PLATFORM}:${LD_LIBRARY_PATH}"   ${SDCARD_PATH}/${PLATFORM}/show.elf ${SDCARD_PATH}/${PLATFORM}/$ACTION.png 60 &
		PID=$!
	fi
	#echo "Found Release file $NEWFILE ! ACTION = $ACTION" >> $LOGFILE
	sudo ${SDCARD_PATH}/${PLATFORM}/unzip -o $UPDATE_PATH -d $SDCARD_PATH #&>> $LOGFILE
	sync
	# the updated system finishes the install/update
	sudo rm -rf ${UPDATE_PATH}
	sudo $SYSTEM_PATH/$PLATFORM/bin/install.sh
else
	echo "No update file found on $UPDATE_PATH" >> $LOGFILE
fi

if [ $PID -ne -1 ]; then
	kill -3 $PID
	# just updated, reboot here to prevent black screen on some device 
	/usr/sbin/reboot -f
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
