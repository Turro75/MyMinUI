#!/bin/sh

########################################


export PLATFORM="r36s"
export SDCARD_PATH="/MyMinUI"
export BIOS_PATH="$SDCARD_PATH/Bios"
export SAVES_PATH="$SDCARD_PATH/Saves"
export CHEATS_PATH="$SDCARD_PATH/Cheats"
export SYSTEM_PATH="$SDCARD_PATH/.system/$PLATFORM"
#export CORES_PATH="/home/ark/.config/retroarch32/cores"
export CORES_PATH="$SYSTEM_PATH/cores"

export USERDATA_PATH="$SDCARD_PATH/.userdata/$PLATFORM"
export SHARED_USERDATA_PATH="$SDCARD_PATH/.userdata/shared"
export LOGS_PATH="$USERDATA_PATH/logs"
export DATETIME_PATH="$SHARED_USERDATA_PATH/datetime.txt"

mkdir -p "$USERDATA_PATH"
mkdir -p "$CHEATS_PATH"
mkdir -p "$LOGS_PATH"
mkdir -p "$SHARED_USERDATA_PATH/.minui"


export PATH=$SYSTEM_PATH/bin:$PATH
export LD_LIBRARY_PATH=$SYSTEM_PATH/lib:$LD_LIBRARY_PATH
export SDL_NOMOUSE=1

#######################################
if [ -e "/dev/input/by-path/platform-fe5b0000.i2c-event" ]; then
	#rg353
	export CPU_SPEED_MENU=1104000
	export CPU_SPEED_POWERSAVE=1104000
	export CPU_SPEED_GAME=1416000
	export CPU_SPEED_PERF=1608000
else
	#r36s
	export CPU_SPEED_MENU=1008000
	export CPU_SPEED_POWERSAVE=1008000
	export CPU_SPEED_GAME=1200000
	export CPU_SPEED_PERF=1296000
fi
export CPU_SPEED_MAX=$(cat /sys/devices/system/cpu/cpufreq/policy0/cpuinfo_max_freq) #1512000  #not on the plus?
export GOVERNOR_PATH="/sys/devices/system/cpu/cpufreq/policy0/scaling_governor"
export GOVERNOR_CPUSPEED_PATH="/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed"

sudo sh -c "echo -n userspace > ${GOVERNOR_PATH}"
sudo sh -c "echo -n ${CPU_SPEED_PERF} > ${GOVERNOR_CPUSPEED_PATH}"

#######################################

keymon.elf & #> $LOGS_PATH/keymon.txt 2>&1 &

#######################################

# init datetime
#if [ -f "$DATETIME_PATH" ]; then
#	DATETIME=`cat "$DATETIME_PATH"`
#	date +'%F %T' -s "$DATETIME"
#	DATETIME=`date +'%s'`
#	date -u -s "@$DATETIME"
#	hwclock --utc -w
#fi

#######################################

AUTO_PATH="$USERDATA_PATH/auto.sh"
if [ -f "$AUTO_PATH" ]; then
	"$AUTO_PATH" # > $LOGS_PATH/auto.txt 2>&1
fi

cd $(dirname "$0")

#######################################

EXEC_PATH="/tmp/minui_exec"
NEXT_PATH="/tmp/next"
#STILL_RUNNING=0
touch "$EXEC_PATH" && sync
while [ -f "$EXEC_PATH" ]; do
		echo 0 | sudo tee /sys/class/graphics/fbcon/cursor_blink
        sudo sh -c "echo $CPU_SPEED_GAME > ${GOVERNOR_CPUSPEED_PATH}"
		sudo systemctl stop oga_events
        minui.elf > $LOGS_PATH/minui.txt 2>&1
	#echo `date +'%F %T'` > "$DATETIME_PATH"
	sync
	
	if [ -f $NEXT_PATH ]; then
		    CMD=`cat $NEXT_PATH`
#		    echo $CMD >> $USERDATA_PATH/ciao.txt
		    eval ${CMD}
		    rm -f $NEXT_PATH
		    sudo sh -c "echo $CPU_SPEED_GAME > ${GOVERNOR_CPUSPEED_PATH}"
	#		echo `date +'%F %T'` > "$DATETIME_PATH"
		    sync
	fi

	# physical powerswitch, enter low power mode
	if [ -f "/tmp/poweroff" ]; then
		rm -f "/tmp/poweroff"
		killall keymon.elf
		shutdown
		# TODO: figure out how to control led?
		while :; do
			sleep 5
		done
	fi
done

