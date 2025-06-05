#!/bin/sh
CUST_LOGO=0
CUST_CPUCLOCK=1
USE_752x560_RES=0

mydir=$(dirname "$0")/drastic


# set CPU speed
#overclock.elf $CPU_SPEED_MENU
overclock.elf $CPU_SPEED_GAME
#overclock.elf $CPU_SPEED_PERF
#overclock.elf $CPU_SPEED_MAX

#inspired by from pico-8 wrapper onionos
get_curvol() {
    awk '/LineOut/ {if (!printed) {gsub(",", "", $8); print $8; printed=1}}' /proc/mi_modules/mi_ao/mi_ao0
}

wait_for_device() {
    local start_time
    local elapsed_time
    start_time=$(date +%s)
    while [ ! -e /proc/mi_modules/mi_ao/mi_ao0 ]; do
        sleep 0.1
        elapsed_time=$(( $(date +%s) - start_time ))
        if [ "$elapsed_time" -ge 4 ]; then
            #echo "FAILED">>$SDCARD_PATH/audioserver.txt
            return 1
        fi
    done
   # keymon.elf &
}


set_snd_level() {
    local target_vol="$1"
    wait_for_device
    echo "set_ao_mute 0" > /proc/mi_modules/mi_ao/mi_ao0
    echo "set_ao_volume 0 ${target_vol}" > /proc/mi_modules/mi_ao/mi_ao0
    echo "set_ao_volume 1 ${target_vol}" > /proc/mi_modules/mi_ao/mi_ao0
}

cd "${mydir}"
if [ ! -f "/tmp/.show_hotkeys" ]; then
    touch /tmp/.show_hotkeys
    LD_LIBRARY_PATH=./libs:/customer/lib:/config/lib ./show_hotkeys
fi


TMP_LD_LIBRARY_PATH=$LD_LIBRARY_PATH
export HOME=$mydir
export PATH=$mydir:$PATH
export LD_LIBRARY_PATH=$mydir/lib:$LD_LIBRARY_PATH
export SDL_VIDEODRIVER=NDS
export SDL_AUDIODRIVER=alsa




#purge_devil

if [  -d "/customer/app/skin_large" ]; then
    USE_752x560_RES=0
fi

if [ "$USE_752x560_RES" == "1" ]; then
    fbset -g 752 560 752 1120 32
fi

cd $mydir
if [ "$CUST_LOGO" == "1" ]; then
    ./png2raw
fi

if [ "$CUST_CPUCLOCK" == "1" ]; then
    overclock.elf $CPU_SPEED_PERF
fi

curvol=$(get_curvol)

if $IS_PLUS; then
    killall -9 audioserver
else
    killall -9 audioserver.mod
fi
sleep 0.2
set_snd_level "${curvol}" &

if [ -f /mnt/SDCARD/Bios/NDS/drastic ]; then
    /mnt/SDCARD/Bios/NDS/drastic "$1" > ${LOGS_PATH}/NDS.txt 2>&1
fi
sync

if [  -d "/customer/app/skin_large" ]; then
    USE_752x560_RES=0
fi

if [ "$USE_752x560_RES" == "1" ]; then
    fbset -g 640 480 640 960 32
fi

#killall -9 keymon.elf
sleep 0.2
if $IS_PLUS; then
	/customer/app/audioserver -60 & # &> $SDCARD_PATH/audioserver.txt &
	export LD_PRELOAD=/customer/lib/libpadsp.so
else
	if [ -f /customer/lib/libpadsp.so ]; then
	     ${SDCARD_PATH}/.system/miyoomini/bin/audioserver.mod -60 &
	fi
fi
wait_for_device &
overclock.elf $CPU_SPEED_MENU
LD_LIBRARY_PATH=$TMP_LD_LIBRARY_PATH