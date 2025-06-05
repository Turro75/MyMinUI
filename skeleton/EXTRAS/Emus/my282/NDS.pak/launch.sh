#!/bin/sh
MYDIR=`dirname "$0"`

export HOME="${MYDIR}/drastic"
export SDL_VIDEODRIVER=NDS

overclock.elf ${CPU_SPEED_MAX}

cd ${MYDIR}/drastic

if [ -f /mnt/SDCARD/Bios/NDS/drastic ]; then
   ./lib/ld-linux-armhf.so.3 --library-path lib /mnt/SDCARD/Bios/NDS/drastic "$1" > ${LOGS_PATH}/NDS.txt 2>&1
fi
sync

overclock.elf ${CPU_SPEED_MENU}
