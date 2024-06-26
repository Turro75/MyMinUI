#!/bin/sh

EMU_EXE=pokemini

RUN=minarch
RUN2=retroarch

#CPU_OC=${CPU_SPEED_MAX} # 1.8 GHz MM+ / 1.7GHz MM
#CPU_OC=${CPU_SPEED_PERF}  # 1.5 GHz
CPU_OC=${CPU_SPEED_GAME}  # 1.3 GHz
#CPU_OC=${CPU_SPEED_POWERSAVE}  # 1.1 GHz
#CPU_OC=${CPU_SPEED_MENU}  # 0.5 GHz

CURDIR="$(dirname "$0")"
#$1 is the rom filename
#$2 is the save state slot number
#$3 is the save state file
#$4 is 1 if SELECT is pressed then use RUN2 instead of RUN
#$RUN is the deafult libretro frontend for this core.pak
#$RUN2 is the alternative libretro frontend
#"$(dirname "$0")" is the current directory
#$EMU_EXE is the name of the core to be launched
#echo launch_rom.sh  "$1" "$2" "$3" "$4" ${RUN} ${RUN2} "$CURDIR" "$EMU_EXE" $CPU_OC > $CURDIR/this.log
launch_rom.sh  "$1" "$2" "$3" "$4" ${RUN} ${RUN2} "$CURDIR" "$EMU_EXE" $CPU_OC
overclock.elf ${CPU_SPEED_MENU}
