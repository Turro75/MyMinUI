#!/bin/sh

export PLATFORM="rg35xx"
export SDCARD_PATH="/mnt/sdcard"
export BIOS_PATH="$SDCARD_PATH/Bios"
export SAVES_PATH="$SDCARD_PATH/Saves"
export SYSTEM_PATH="$SDCARD_PATH/.system/$PLATFORM"
export CORES_PATH="$SYSTEM_PATH/cores"
export USERDATA_PATH="$SDCARD_PATH/.userdata/$PLATFORM"
export SHARED_USERDATA_PATH="$SDCARD_PATH/.userdata/shared"
export LOGS_PATH="$USERDATA_PATH/logs"
export DATETIME_PATH="$SHARED_USERDATA_PATH/datetime.txt"

#######################################

export PATH=$SYSTEM_PATH/bin:$PATH
export LD_LIBRARY_PATH=$SYSTEM_PATH/lib:$LD_LIBRARY_PATH

#######################################

echo noop > /sys/devices/b0238000.mmc/mmc_host/mmc0/emmc_boot_card/block/mmcblk0/queue/scheduler
echo noop > /sys/devices/b0230000.mmc/mmc_host/mmc1/sd_card/block/mmcblk1/queue/scheduler
echo on > /sys/devices/b0238000.mmc/mmc_host/mmc0/power/control
echo on > /sys/devices/b0230000.mmc/mmc_host/mmc1/power/control

#######################################
export NUM_CPU=3
export CPU_SPEED_MENU=504000
export CPU_SPEED_POWERSAVE=1104000
export CPU_SPEED_GAME=1200000
export CPU_SPEED_PERF=1392000
export CPU_SPEED_MAX=1488000

overclock.elf $CPU_SPEED_PERF $NUM_CPU

#######################################

keymon.elf & # &> $LOGS_PATH/keymon.txt &

#######################################

mkdir -p "$LOGS_PATH"
mkdir -p "$SHARED_USERDATA_PATH/.minui"
AUTO_PATH=$USERDATA_PATH/auto.sh
if [ -f "$AUTO_PATH" ]; then
	"$AUTO_PATH" # &> $LOGS_PATH/auto.txt
fi

cd $(dirname "$0")
# ./batmon.sh &> /mnt/sdcard/batmon.txt &

#######################################

EXEC_PATH=/tmp/minui_exec
NEXT_PATH="/tmp/next"
rm -rf $LOGS_PATH/minui.txt
touch "$EXEC_PATH" && sync
while [ -f "$EXEC_PATH" ]; do
	overclock.elf $CPU_SPEED_PERF $NUM_CPU
	cp -f /$SYSTEM_PATH/dat/audio.conf /usr/share/alsa/alsa.conf.d/audio.conf
	minui.elf >> $LOGS_PATH/minui.txt 2>&1
	echo `date +'%F %T'` > "$DATETIME_PATH"
	sync
	
	if [ -f $NEXT_PATH ]; then
		CMD=`cat $NEXT_PATH`
		eval $CMD
		rm -f $NEXT_PATH
		overclock.elf $CPU_SPEED_PERF $NUM_CPU
		echo `date +'%F %T'` > "$DATETIME_PATH"
		sync
	fi
done

shutdown # just in case