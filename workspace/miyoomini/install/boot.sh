#!/bin/sh
# NOTE: becomes .tmp_update/miyoomini.sh

PLATFORM="miyoomini"
FWNAME=MinUI
SDCARD_PATH="/mnt/SDCARD"
UPDATE_PATH="$SDCARD_PATH/MinUI.zip"
SYSTEM_PATH="$SDCARD_PATH/.system"

CPU_PATH=/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
echo performance > "$CPU_PATH"

SWAPFILE=${SYSTEM_PATH}/${PLATFORM}/myswapfile
PID=-1
# is there an update available?
if [ -f ${SDCARD_PATH}/My${FWNAME}-*-${PLATFORM}.zip ]; then
    NEWFILE=$(ls ${SDCARD_PATH}/My${FWNAME}-*-${PLATFORM}.zip)
    cd $(dirname "$0")/$PLATFORM
	
	# init backlight
	echo 0 > /sys/class/pwm/pwmchip0/export
	echo 800 > /sys/class/pwm/pwmchip0/pwm0/period
	echo 50 > /sys/class/pwm/pwmchip0/pwm0/duty_cycle
	echo 1 > /sys/class/pwm/pwmchip0/pwm0/enable

	# init lcd
	cat /proc/ls
	sleep 1
	export LCD_INIT=1
	
	if [ -d "${SYSTEM_PATH}/${PLATFORM}" ]; then
		ACTION="updating"
	else
		ACTION="installing"
	fi
	./show.elf ./${ACTION}.png &
	PID=$!

	#echo "Found Release file $NEWFILE ! ACTION = $ACTION" >> $LOGFILE
        busybox unzip -o $NEWFILE -d $SDCARD_PATH #&>> $LOGFILE
	sync

	#mv $SDCARD_PATH/.tmp_update $SDCARD_PATH/.tmp_update-old
	cp -Rf $SDCARD_PATH/miyoo354/app/.tmp_update $SDCARD_PATH/
	rm -rf $SDCARD_PATH/.tmp_update-old

	#remove useless dirs for miyoomini
    rm -rf $SDCARD_PATH/rg35xx
	rm -rf $SDCARD_PATH/trimui	
	rm -rf $NEWFILE
	sync
fi

#same as original MinUI install/update process
if [ -f "$UPDATE_PATH" ]; then 
	cd $(dirname "$0")/$PLATFORM
	
	# init backlight
	echo 0 > /sys/class/pwm/pwmchip0/export
	echo 800 > /sys/class/pwm/pwmchip0/pwm0/period
	echo 50 > /sys/class/pwm/pwmchip0/pwm0/duty_cycle
	echo 1 > /sys/class/pwm/pwmchip0/pwm0/enable

	# init lcd
	cat /proc/ls
	sleep 1
	export LCD_INIT=1

	if [ $PID -eq -1 ];then
		if [ -d "${SYSTEM_PATH}/${PLATFORM}" ]; then
			ACTION="updating"
		else
			ACTION="installing"
		fi
		./show.elf ./${ACTION}.png &
		PID=$!
	fi

	mv $SDCARD_PATH/.tmp_update $SDCARD_PATH/.tmp_update-old
	unzip -o "$UPDATE_PATH" -d "$SDCARD_PATH"
	rm -f "$UPDATE_PATH"
	rm -rf $SDCARD_PATH/.tmp_update-old
	
	# the updated system finishes the install/update
	$SYSTEM_PATH/$PLATFORM/bin/install.sh
	sync	
	if [ -f ${SWAPFILE}.gz ]; then
		#the swapfile does not exists, let's create it
    	rm -rf $SWAPFILE
    	gzip -d ${SWAPFILE}.gz
    	chmod 600 $SWAPFILE
    	mkswap $SWAPFILE
	fi
	sync
fi

if [ $PID -ne -1 ]; then
    kill -3 $PID
fi

if [ -f $SWAPFILE ]; then
	swapon $SWAPFILE
fi

# or launch (and keep launched)
LAUNCH_PATH="$SYSTEM_PATH/$PLATFORM/paks/MinUI.pak/launch.sh"
while [ -f "$LAUNCH_PATH" ] ; do
	"$LAUNCH_PATH"
done

reboot # under no circumstances should stock be allowed to touch this card
