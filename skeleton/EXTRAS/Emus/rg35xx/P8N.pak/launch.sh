#!/bin/sh

progdir="${SDCARD_PATH}/Tools/${PLATFORM}/Pico-8 Splore.pak/pico8-native"
thisdir=$(dirname "$0")
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
HOME="${progdir}" "${BIOS_PATH}/P8/pico8_dyn" -v -run "${1}"  &> $LOGS_PATH/P8N.txt
echo 0 > /sys/class/graphics/fb0/mirror_to_hdmi
overclock.elf $CPU_SPEED_MENU 1 