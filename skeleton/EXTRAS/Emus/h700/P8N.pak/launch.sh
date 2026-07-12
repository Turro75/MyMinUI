#!/bin/sh

progdir="${SDCARD_PATH}/Tools/${PLATFORM}/Pico-8 Splore.pak/pico8-native"
cd "${progdir}"

# set CPU speed
#overclock.elf $CPU_SPEED_MENU
overclock.elf $CPU_SPEED_GAME
#overclock.elf $CPU_SPEED_PERF
#overclock.elf $CPU_SPEED_MAX

LD_LIBRARY_PATH=${SDCARD_PATH}/.system/${PLATFORM}/lib:$LD_LIBRARY_PATH SDL_PATH_DSP=/tmp/dsp SDL_AUDIODRIVER=dsp LD_PRELOAD=${SDCARD_PATH}/.system/${PLATFORM}/lib/libredirect_dsp.so HOME="${progdir}" "${BIOS_PATH}/P8/pico8_dyn" -v -run "${1}"  > $LOGS_PATH/P8N.txt

overclock.elf $CPU_SPEED_MENU
