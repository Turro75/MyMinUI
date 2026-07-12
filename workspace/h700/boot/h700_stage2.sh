#!/bin/sh

SDCARD_PATH=/mnt/sdcard
TF1_PATH=/mnt/mmc
LOGFILE=${TF1_PATH}/bootlog.txt

if ! [ -e $SDCARD_PATH ]; then
	echo "/mnt/sdcard not found, something went wrong, better to reboot and try again" >> $LOGFILE
	sync && reboot -p
	exit 0
fi


SYSTEM_DIR=/.system
PLATFORM=h700
FWNAME=MinUI
SYSTEM_FRAG=${SYSTEM_DIR}/${PLATFORM}
UPDATE_FRAG=/MinUI.zip
SYSTEM_PATH=${SDCARD_PATH}${SYSTEM_FRAG}
UPDATE_PATH=${SDCARD_PATH}${UPDATE_FRAG}

# is there an update available?
export SDL_NOMOUSE=1
PID=-1
#clear the screen from the bootlogo
cat /dev/zero > /dev/fb0
# install/update
# is there an update available?
if [ -e ${SDCARD_PATH}/My${FWNAME}-*-${PLATFORM}.zip ]; then
	NEWFILE=$(ls ${SDCARD_PATH}/My${FWNAME}-*-${PLATFORM}.zip)
	#echo "Trovato release file" >> $LOGFILE
	#echo "Sono nella directory " $(pwd) >> $LOGFILE

	if [ -d "${SYSTEM_PATH}" ]; then
	    ACTION="updating"
	else
	    ACTION="installing"
	fi
	echo "performing: ${SDCARD_PATH}/${PLATFORM}/show.elf ${SDCARD_PATH}/${PLATFORM}/${ACTION}.png 60 &" >> $LOGFILE
	LD_LIBRARY_PATH="${SDCARD_PATH}/${PLATFORM}:${LD_LIBRARY_PATH}"   ${SDCARD_PATH}/${PLATFORM}/show.elf ${SDCARD_PATH}/${PLATFORM}/${ACTION}.png 60 &
	PID=$!
	#echo "Found Release file $NEWFILE ! ACTION = $ACTION" >> $LOGFILE
	unzip -o $NEWFILE -d $SDCARD_PATH -x "${PLATFORM}/*" >> $LOGFILE
	sync
		# install bootlogo.bmp
	if [ $ACTION = "installing" ]; then
		echo "replace bootlogo" >> $LOGFILE

		MODES=`cat /sys/class/graphics/fb0/modes`
		case $MODES in
			*"480x640"*)
				SUFFIX="-ccw"
				;;
		esac

		case "$RGXX_MODEL" in
			RGcubexx)
				SUFFIX="-s"
				;;
			RG34xx*)
				SUFFIX="-w"
				;;
		esac

		BOOT_DEVICE=/dev/mmcblk0p2
		BOOT_PATH=/mnt/boot
		mkdir -p $BOOT_PATH
		mount -t vfat -o rw,utf8,noatime $BOOT_DEVICE $BOOT_PATH
		cp ${SDCARD_PATH}/${PLATFORM}/bootlogo$SUFFIX.bmp $BOOT_PATH/bootlogo.bmp
		umount $BOOT_PATH
		rm -rf $BOOT_PATH
	fi
	#remove useless dirs
#	rm -rf $SDCARD_PATH/rg35xx
	sudo rm -rf $SDCARD_PATH/trimui
#	rm -rf $SDCARD_PATH/miyoo354
	rm -rf $NEWFILE
	sync
	echo "End phase 1" >> $LOGFILE
	if [ $PID -ne -1 ]; then
	    kill -3 $PID
	    PID=-1
	fi	
else
	echo "No release file found on ${SDCARD_PATH}/My${FWNAME}" >> $LOGFILE
fi
	#update dmenu.bin if changed then reboot otherwise continue
#	oldfilemd5=$(md5 /mnt/mmc/dmenu.bin | cut -d" " -f1 )
#	newfilemd5=$(md5 ${SDCARD_PATH}/${PLATFORM}/dmenu.bin | cut -d" " -f1 )
#	#echo "OLD = $oldfilemd5" >  $SDCARD_PATH/md5.txt
#	#echo "NEW = $newfilemd5" >> $SDCARD_PATH/md5.txt
#	if [ "$oldfilemd5" != "$newfilemd5" ]; then
	#	cp ${SDCARD_PATH}/${PLATFORM}/dmenu.bin /mnt/mmc/dmenu.bin
		#echo "replacing dmenu.bin" >> $SDCARD_PATH/md5.txt
#		sync #&& reboot
#	fi 
#fi

echo "Checking for $UPDATE_PATH update file" >> $LOGFILE
#same as original MinUI install/update process
if [ -e "$UPDATE_PATH" ]; then
	echo "Found update file $UPDATE_PATH" >> $LOGFILE
	if [ -d "${SYSTEM_PATH}" ]; then
	    ACTION="updating"
	else
	    ACTION="installing"
	fi

	if [ $PID -eq -1 ]; then
		echo "performing: ${SDCARD_PATH}/${PLATFORM}/show.elf ${SDCARD_PATH}/${PLATFORM}/${ACTION}.png 60 &" >> $LOGFILE
		LD_LIBRARY_PATH="${SDCARD_PATH}/${PLATFORM}:${LD_LIBRARY_PATH}"   ${SDCARD_PATH}/${PLATFORM}/show.elf ${SDCARD_PATH}/${PLATFORM}/${ACTION}.png 60 &
		PID=$!
	fi
	#echo "Found Release file $NEWFILE ! ACTION = $ACTION" >> $LOGFILE\
	unzip -o $UPDATE_PATH -d $SDCARD_PATH #&>> $LOGFILE
	sync
	# the updated system finishes the install/update
	rm -rf ${UPDATE_PATH}
	$SYSTEM_PATH/bin/install.sh
	if [ -e $SDCARD_PATH/.userdata/$PLATFORM/msettings.bin ]; then
		rm $SDCARD_PATH/.userdata/$PLATFORM/msettings.bin
	fi
else
	echo "No update file found on $UPDATE_PATH" >> $LOGFILE
fi

if [ $PID -ne -1 ]; then
	kill -3 $PID
	# just updated, reboot here to prevent black screen on some device 
	#/usr/sbin/reboot -f
fi

export PATH=/usr/sbin:/usr/bin:/sbin:/bin:$PATH
export LD_LIBRARY_PATH=/usr/lib32/:/lib/
export HOME=$SDCARD_PATH

echo "SYSTEMPATH=${SYSTEM_PATH}" >> $LOGFILE
ls -l ${SYSTEM_PATH}/paks/MinUI.pak/* >> $LOGFILE

if [ -f ${SYSTEM_PATH}/paks/MinUI.pak/launch.sh ] ; then
    echo "Launching MyMinUI" >> $LOGFILE
    sudo ${SYSTEM_PATH}/paks/MinUI.pak/launch.sh
else
    echo "Error: launch.sh not found!" >> $LOGFILE
    echo "Exiting" > ${SDCARD_PATH}/bootfailed.txt
fi

sync && reboot -p
exit 0

