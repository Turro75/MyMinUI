#!/bin/sh

progdir="${SDCARD_PATH}/Tools/${PLATFORM}/Pico-8 Splore.pak/pico8-native"
thisdir=$(dirname "$0")
cd "$progdir"
export LD_LIBRARY_PATH="$progdir/../patch:${LD_LIBRARY_PATH}"
export SDL_VIDEODRIVER=mali
#export SDL_AUDIODRIVER=alsa
export SDL_JOYSTICKDRIVER=a30


# set CPU speed
#overclock.elf $CPU_SPEED_MENU
overclock.elf $CPU_SPEED_GAME
#overclock.elf $CPU_SPEED_PERF
#overclock.elf $CPU_SPEED_MAX

HOME="${progdir}" "${BIOS_PATH}/P8/pico8_dyn" -v -run "${1}"  &> $LOGS_PATH/P8N.txt

overclock.elf $CPU_SPEED_MENU
