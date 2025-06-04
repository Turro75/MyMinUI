#!/bin/sh
MYDIR=`dirname "$0"`

export HOME="${MYDIR}/drastic"
export SDL_VIDEODRIVER=NDS

overclock.elf ${CPU_SPEED_MAX}
#sv=`cat /proc/sys/vm/swappiness`
#echo 10 > /proc/sys/vm/swappiness
#echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor

cd ${MYDIR}/drastic
#LD_TRACE_LOADED_OBJECTS=1 LD_LIBRARY_PATH=${MYDIR}/drastic/lib:$LD_LIBRARY_PATH ./drastic &> $LOGS_PATH/NDS_ldd.txt
#LD_LIBRARY_PATH=${MYDIR}/drastic/lib:$LD_LIBRARY_PATH ./drastic "$1" > ${LOGS_PATH}/NDS.txt 2>&1
./lib/ld-linux-armhf.so.3 --library-path lib ./drastic "$1" > ${LOGS_PATH}/NDS.txt 2>&1
sync

#echo $sv > /proc/sys/vm/swappiness
#echo ondemand > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
overclock.elf ${CPU_SPEED_MENU}
