#!/bin/sh

#set here the dir where splore looks for carts
romdir="${SDCARD_PATH}/Roms/Pico-8 (P8)"

thisdir=$(dirname "$0")
progdir="$thisdir/pico8-native"
cd "$progdir"
echo "PICO-8 Starting Splore" > "${thisdir}/log.txt"

export SDL_VIDEODRIVER=directfb
export SDL_AUDIODRIVER=alsa

ISHDMI=$(cat /sys/class/switch/hdmi/state)
echo $ISHDMI > /sys/class/graphics/fb0/mirror_to_hdmi
if [ "${ISHDMI}" == "1" ]; then
	# route the audio to hdmi
	export AUDIODEV=hdmi
	# turn off screen backlight
	echo 1 > /sys/class/backlight/backlight.2/bl_power
else
	export AUDIODEV=default
	echo 0 > /sys/class/backlight/backlight.2/bl_power
fi

# set CPU speed
#overclock.elf $CPU_SPEED_MENU 1
overclock.elf $CPU_SPEED_GAME 1
#overclock.elf $CPU_SPEED_PERF 1
#overclock.elf $CPU_SPEED_MAX 1

HOME="${progdir}" "${BIOS_PATH}/P8/pico8_dyn" -v -splore -root_path "${romdir}" &> $LOGS_PATH/pico8_splore.txt
echo 0 > /sys/class/graphics/fb0/mirror_to_hdmi
overclock.elf $CPU_SPEED_MENU 1 
