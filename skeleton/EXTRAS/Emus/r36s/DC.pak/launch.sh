#!/bin/sh

#EMU=retrorun32
EMU=retrorun
#EMU=retroarch32
#EMU=retroarch
CORE=flycast_extreme
#CORE=reicast_extreme
CORE=flycast



CPU_OC=${CPU_SPEED_MAX} # 1.512 GHz
#CPU_OC=${CPU_SPEED_PERF}  # 1.344 GHz
#CPU_OC=${CPU_SPEED_GAME}  # 1.2 GHz
#CPU_OC=${CPU_SPEED_POWERSAVE}  # 816MHz
#CPU_OC=${CPU_SPEED_MENU}  # 576MHz

CURDIR="$(dirname "$0")"
#$1 is the rom filename
#$2 is the save state slot number
#$3 is the save state file
#$4 is 1 if SELECT is pressed then use RUN2 instead of RUN
#$RUN is the deafult libretro frontend for this core.pak
#$RUN2 is the alternative libretro frontend
#"$(dirname "$0")" is the current directory
#$EMU_EXE is the name of the core to be launched
#$CPU_OC sets the desired level of cpu clock
#echo launch_rom.sh  "$1" "$2" "$3" "$4" ${RUN} ${RUN2} "$CURDIR" "$EMU_EXE" $CPU_OC > $CURDIR/this.log
overclock.elf ${CPU_OC}

if [ ! -f $SDCARD_PATH/Saves/DC ]; then
    mkdir -p $SDCARD_PATH/Saves/DC
fi

$CURDIR/dreamcast.sh "$EMU" "$CORE" "$1" > $LOGS_PATH/DC.txt

#launch_rom.sh  "$1" "$2" "$3" "$4" ${RUN} ${RUN2} "$CURDIR" "$EMU_EXE" $CPU_OC
overclock.elf ${CPU_SPEED_MENU}
