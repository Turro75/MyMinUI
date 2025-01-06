#!/bin/sh
# NOTE: becomes .tmp_update/my282.sh

PLATFORM="my282"
FWNAME=MinUI
SDCARD_PATH="/mnt/SDCARD"
UPDATE_PATH="$SDCARD_PATH/MinUI.zip"
SYSTEM_PATH="$SDCARD_PATH/.system"

CPU_PATH=/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
echo performance > "$CPU_PATH"

# install/update
# is there an update available?
if [ -f ${SDCARD_PATH}/My${FWNAME}-*-${PLATFORM}.zip ]; then
    NEWFILE=$(ls ${SDCARD_PATH}/My${FWNAME}-*-${PLATFORM}.zip)
#	echo "Trovato release file" >> $SDCARD_PATH/log.txt
    cd $(dirname "$0")/$PLATFORM
#	echo "Sono nella directory " $(pwd) >> $SDCARD_PATH/log.txt
	
	if [ -d "${SYSTEM_PATH}/${PLATFORM}" ]; then
		./show.elf ./updating.png
	else
		./show.elf ./installing.png
	fi

	#echo "Found Release file $NEWFILE ! ACTION = $ACTION" >> $LOGFILE
       ./unzip -o $NEWFILE -d $SDCARD_PATH #&>> $LOGFILE
	sync

#	mv $SDCARD_PATH/.tmp_update $SDCARD_PATH/.tmp_update-old
	cp -Rf $SDCARD_PATH/miyoo/app/.tmp_update $SDCARD_PATH/
	rm -rf $SDCARD_PATH/.tmp_update-old

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
#	echo "Trovato MinUI file" >> $SDCARD_PATH/log.txt
    cd $(dirname "$0")/$PLATFORM
#	echo "Sono nella directory " $(pwd) >> $SDCARD_PATH/log.txt

	if [ -d "${SYSTEM_PATH}/${PLATFORM}" ]; then
		./show.elf ./updating.png
	else
		./show.elf ./installing.png
	fi

#	mv $SDCARD_PATH/.tmp_update $SDCARD_PATH/.tmp_update-old
	./unzip -o "$UPDATE_PATH" -d "$SDCARD_PATH"
	rm -f "$UPDATE_PATH"
	rm -rf $SDCARD_PATH/.tmp_update-old

	# the updated system finishes the install/update
	$SYSTEM_PATH/$PLATFORM/bin/install.sh
	./show.elf ./done.png
fi

# or launch (and keep launched)
LAUNCH_PATH="$SYSTEM_PATH/$PLATFORM/paks/MinUI.pak/launch.sh"
while [ -f "$LAUNCH_PATH" ] ; do
	"$LAUNCH_PATH"
done

poweroff # under no circumstances should stock be allowed to touch this card
