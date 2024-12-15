#!/bin/sh

progdir="${SDCARD_PATH}/Tools/${PLATFORM}/Pico-8 Splore.pak/pico8-native"
cd "${progdir}"
#export SDL_VIDEODRIVER=directfb
#export SDL_AUDIODRIVER=alsa


# set CPU speed
#overclock.elf $CPU_SPEED_MENU
overclock.elf $CPU_SPEED_GAME
#overclock.elf $CPU_SPEED_PERF
#overclock.elf $CPU_SPEED_MAX

HOME="${progdir}" "${BIOS_PATH}/P8/pico8_64" -v -run "${1}"  > $LOGS_PATH/P8N.txt

overclock.elf $CPU_SPEED_MENU
