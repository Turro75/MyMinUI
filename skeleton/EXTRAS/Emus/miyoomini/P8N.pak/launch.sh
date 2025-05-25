#!/bin/sh


# set CPU speed
#overclock.elf $CPU_SPEED_MENU
overclock.elf $CPU_SPEED_GAME
#overclock.elf $CPU_SPEED_PERF
#overclock.elf $CPU_SPEED_MAX

if [ -f "$1" ]; then
	GAME="$1"
fi

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

basedir="${SDCARD_PATH}/Tools/${PLATFORM}/Pico-8 Splore.pak"
progdir="$basedir/pico8-native"
thisdir=$(dirname "$0")

export LD_LIBRARY_PATH="$basedir/patch:$progdir/lib:/mnt/SDCARD/.system/miyoomini/lib:/lib:/config/lib:/mnt/SDCARD/miyoo/lib:/mnt/SDCARD/.tmp_update/lib:/mnt/SDCARD/.tmp_update/lib/parasyte:/sbin:/usr/sbin:/bin:/usr/bin"
export SDL_VIDEODRIVER=mmiyoo
export SDL_AUDIODRIVER=mmiyoo
export EGL_VIDEODRIVER=mmiyoo

#audioserver must be killed to let sdl2 taking control of audio 
curvol=$(get_curvol)
#killall -9 keymon.elf
if $IS_PLUS; then
    killall -9 audioserver
else
    killall -9 audioserver.mod
fi
sleep 0.2
set_snd_level "${curvol}" &
cd "${progdir}" 2&> "${thisdir}/log.txt"
echo $(pwd) >> "${thisdir}/log.txt"
cp "$basedir/patch/onioncfg.json" "$progdir/cfg/"
HOME="${progdir}" "${BIOS_PATH}/P8/pico8_dyn" -v -run "${1}"  &> $LOGS_PATH/P8N.txt

#killall -9 keymon.elf
sleep 0.1
if $IS_PLUS; then
    /customer/app/audioserver -60 &
else
    ${SDCARD_PATH}/.system/miyoomini/bin/audioserver.mod -60 &
fi
wait_for_device &
overclock.elf $CPU_SPEED_MENU
