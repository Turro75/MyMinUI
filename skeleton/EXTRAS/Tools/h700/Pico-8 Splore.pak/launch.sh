#!/bin/sh
#set here the dir where splore looks for carts
romdir="${SDCARD_PATH}/Roms/Pico-8 (P8)"
    
# set CPU speed
#overclock.elf $CPU_SPEED_MENU
overclock.elf $CPU_SPEED_GAME
#overclock.elf $CPU_SPEED_PERF
#overclock.elf $CPU_SPEED_MAX

progdir=$(dirname "$0")/pico8-native
thisdir=$(dirname "$0")

LD_PRELOAD=${SDCARD_PATH}/.system/h700/lib/libredirect_dsp.so SDL_AUDIODRIVER=dsp SDL_PATH_DSP=/tmp/dsp HOME="${progdir}" "${BIOS_PATH}/P8/pico8_dyn" -v -splore -root_path "${romdir}" > $LOGS_PATH/pico8_splore.txt

overclock.elf $CPU_SPEED_MENU
