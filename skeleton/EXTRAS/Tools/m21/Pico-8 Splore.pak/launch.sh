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

export SDL_VIDEODRIVER=directfb
export SDL_AUDIODRIVER=alsa

HOME="${progdir}" "${progdir}/pico8_dyn" -v -splore -root_path "${romdir}" &> "${thisdir}/log.txt"

overclock.elf $CPU_SPEED_MENU
