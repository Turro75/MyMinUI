#!/bin/sh


# set CPU speed
#overclock.elf $CPU_SPEED_MENU
overclock.elf $CPU_SPEED_GAME
#overclock.elf $CPU_SPEED_PERF
#overclock.elf $CPU_SPEED_MAX

if [ -f "$1" ]; then
	GAME="$1"
fi


basedir="${SDCARD_PATH}/Tools/${PLATFORM}/Pico-8 Splore.pak"
progdir="$basedir/pico8-native"
thisdir=$(dirname "$0")

cp "$basedir/patch/onioncfg.json" "$progdir/cfg/"
SDL_AUDIODRIVER=dsp EGL_VIDEODRIVER=mmiyoo SDL_VIDEODRIVER=mmiyoo HOME="${progdir}" "${BIOS_PATH}/P8/pico8_dyn" -v -run "${1}"  &> $LOGS_PATH/P8N.txt

overclock.elf $CPU_SPEED_MENU
