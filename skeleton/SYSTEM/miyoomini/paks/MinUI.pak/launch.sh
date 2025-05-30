#!/bin/sh
# MiniUI.pak

if [ -z "$LCD_INIT" ]; then
	# an update may have already initilized the LCD
	/mnt/SDCARD/.system/miyoomini/bin/blank.elf

	# init backlight
	echo 0 > /sys/class/pwm/pwmchip0/export
	echo 800 > /sys/class/pwm/pwmchip0/pwm0/period
	echo 6 > /sys/class/pwm/pwmchip0/pwm0/duty_cycle
	echo 1 > /sys/class/pwm/pwmchip0/pwm0/enable

	# init lcd
	cat /proc/ls
	sleep 0.5
fi

# init charger detection
if [ ! -f /sys/devices/gpiochip0/gpio/gpio59/direction ]; then
	echo 59 > /sys/class/gpio/export
	echo in > /sys/devices/gpiochip0/gpio/gpio59/direction
fi


#######################################

if [ -f /customer/app/axp_test ]; then
	IS_PLUS=true
else
	IS_PLUS=false
fi
export IS_PLUS
export PLATFORM="miyoomini"
export PATH=/mnt/SDCARD/.system/miyoomini/bin:$PATH
export SDCARD_PATH="/mnt/SDCARD"
export BIOS_PATH="$SDCARD_PATH/Bios"
export SAVES_PATH="$SDCARD_PATH/Saves"
export SYSTEM_PATH="$SDCARD_PATH/.system/$PLATFORM"
export CORES_PATH="$SYSTEM_PATH/cores"
export USERDATA_PATH="$SDCARD_PATH/.userdata/$PLATFORM"
export SHARED_USERDATA_PATH="$SDCARD_PATH/.userdata/shared"
export LOGS_PATH="$USERDATA_PATH/logs"
export DATETIME_PATH="$SHARED_USERDATA_PATH/datetime.txt" # used by bin/shutdown

mkdir -p "$USERDATA_PATH"
mkdir -p "$LOGS_PATH"
mkdir -p "$SHARED_USERDATA_PATH/.minui"

#######################################

export CPU_SPEED_MENU=504000
export CPU_SPEED_POWERSAVE=1104000
export CPU_SPEED_GAME=1296000
export CPU_SPEED_PERF=1488000
if $IS_PLUS; then
    export CPU_SPEED_MAX=1800000
else
    export CPU_SPEED_MAX=1700000
fi
echo userspace > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
overclock.elf $CPU_SPEED_PERF

MIYOO_VERSION=`/etc/fw_printenv miyoo_version`
export MIYOO_VERSION=${MIYOO_VERSION#miyoo_version=}

########################################

max_attempts=5
attempt=0
IS_MMV4=false
while [ "$attempt" -lt "$max_attempts" ]; do
    screen_resolution=$(grep 'Current TimingWidth=' /proc/mi_modules/fb/mi_fb0 | sed 's/Current TimingWidth=\([0-9]*\),TimingWidth=\([0-9]*\),.*/\1x\2/')
    if [ -n "$screen_resolution" ]; then
        echo "get_screen_resolution: success, resolution: $screen_resolution"
		if [ "$screen_resolution" = "752x560" ]; then
			IS_MMV4=true  
			break
		fi        
		if [ "$screen_resolution" = "640x480" ]; then
			IS_MMV4=false
			break   
		fi
    fi
    attempt=$((attempt + 1))
    sleep 0.3
done

export IS_MMV4


#######################################

# killall tee # NOTE: killing tee is somehow responsible for audioserver crashes
rm -f "$SDCARD_PATH/update.log"

#######################################

export LD_LIBRARY_PATH=$SYSTEM_PATH/lib:$LD_LIBRARY_PATH
export PATH=$SYSTEM_PATH/bin:$PATH

#######################################

if $IS_PLUS; then
    /customer/app/audioserver -60 & #> $SDCARD_PATH/audioserver.txt &
    export LD_PRELOAD=/customer/lib/libpadsp.so
else
    if [ -f /customer/lib/libpadsp.so ]; then
        LD_PRELOAD=as_preload.so audioserver.mod &
        export LD_PRELOAD=libpadsp.so
    fi
fi

#######################################
#echo "STARTING!!!!!" > /mnt/SDCARD/out.txt
lumon.elf & # adjust lcd luma and saturation //wait for command executed before continuing
# I edited it to allow adjusting screen params and color temperature

if $IS_PLUS; then
	CHARGING=`/customer/app/axp_test | awk -F'[,: {}]+' '{print $7}'`
	if [ "$CHARGING" == "3" ]; then
		batmon.elf # &> /mnt/SDCARD/batmon.txt
	fi
else
	CHARGING=`cat /sys/devices/gpiochip0/gpio/gpio59/value`
	if [ "$CHARGING" == "1" ]; then
		batmon.elf # &> /mnt/SDCARD/batmon.txt
	fi
fi

keymon.elf & # &> /mnt/SDCARD/out1.txt &

#######################################

# init datetime
if [ -f "$DATETIME_PATH" ] && [ ! -f "$SHARED_USERDATA_PATH/enable-rtc" ]; then
	DATETIME=`cat "$DATETIME_PATH"`
	date +'%F %T' -s "$DATETIME"
	DATETIME=`date +'%s'`
	date -u -s "@$DATETIME"
fi

#######################################

AUTO_PATH=$USERDATA_PATH/auto.sh
if [ -f "$AUTO_PATH" ]; then
	"$AUTO_PATH"
fi

cd $(dirname "$0")

#######################################

EXEC_PATH=/tmp/minui_exec
NEXT_PATH="/tmp/next"
touch "$EXEC_PATH"  && sync
while [ -f "$EXEC_PATH" ]; do
	overclock.elf $CPU_SPEED_MENU
	minui.elf &> $LOGS_PATH/minui.txt
	
	echo `date +'%F %T'` > "$DATETIME_PATH"
	sync
	
	if [ -f $NEXT_PATH ]; then
		CMD=`cat $NEXT_PATH`
		eval $CMD
		rm -f $NEXT_PATH
		if [ -f "/tmp/using-swap" ]; then
			swapoff $USERDATA_PATH/swapfile
			rm -f "/tmp/using-swap"
		fi
		
		echo `date +'%F %T'` > "$DATETIME_PATH"
		overclock.elf $CPU_SPEED_MENU
		sync
	fi
done

shutdown # just in case
