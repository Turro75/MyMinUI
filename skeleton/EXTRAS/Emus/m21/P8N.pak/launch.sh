#!/bin/sh

progdir="${SDCARD_PATH}/Tools/${PLATFORM}/Pico-8 Splore.pak/pico8-native"
cd "${progdir}"
export SDL_VIDEODRIVER=directfb
export SDL_AUDIODRIVER=alsa


# set CPU speed
#overclock.elf $CPU_SPEED_MENU
overclock.elf $CPU_SPEED_GAME
#overclock.elf $CPU_SPEED_PERF
#overclock.elf $CPU_SPEED_MAX

/bin/cat "${progdir}/basicfbrc.txt" > "${progdir}/.directfbrc"
/bin/cat "${progdir}/basicconfig.txt" > "${progdir}/.lexaloffle/pico-8/config.txt"



#check if is an m22
if [ -f /mnt/SDCARD/m21/thisism22 ]; then
     /bin/echo "mode=480x854" >> "${progdir}/.directfbrc"
    # check if hdmi out as transform_screen must be 0
    ISHDMI=$(cat /sys/class/extcon/extcon0/state)
    if [ "${ISHDMI}" == "HDMI=1" ]; then
        /bin/echo "transform_screen 0" >> "${progdir}/.lexaloffle/pico-8/config.txt"
    else
        /bin/echo "transform_screen 133" >> "${progdir}/.lexaloffle/pico-8/config.txt"
    fi
else
    echo "mode=640x480" >> "${progdir}/.directfbrc"
    echo "transform_screen 0" >> "${progdir}/.lexaloffle/pico-8/config.txt"
fi

HOME="${progdir}" "${BIOS_PATH}/P8/pico8_dyn" -v -run "${1}"  &> $LOGS_PATH/P8N.txt

overclock.elf $CPU_SPEED_MENU
