#!/bin/bash

sudo chmod 666 /dev/tty1

if  [[ $1 == "retroarch" ]]; then
/usr/local/bin/"$1" -L /home/ark/.config/"$1"/cores/"$2"_libretro.so "$3"
elif [[ $1 == "retroarch32" ]]; then
/usr/local/bin/"$1" -L /home/ark/.config/"$1"/cores/"$2"_libretro.so "$3"
elif [[ $1 == "retrorun" ]]; then
evtest --query /dev/input/event2 EV_KEY BTN_EAST
   if [ "$?" -eq "10" ]; then
      echo "640x480 Enabled!" >> /dev/tty1
      /usr/bin/retrorun-640 --triggers -n -s "${SDCARD_PATH}/Roms/Dreamcast (DC)" -d $BIOS_PATH/DC /home/ark/.config/retroarch/cores/"$2"_libretro.so "$3"
   else
      /usr/bin/retrorun --triggers -n -s "${SDCARD_PATH}/Roms/Dreamcast (DC)"  -d $BIOS_PATH/DC /home/ark/.config/retroarch/cores/"$2"_libretro.so "$3"
   fi
printf "\033c" >> /dev/tty1
else
evtest --query /dev/input/event2 EV_KEY BTN_EAST
   if [ "$?" -eq "10" ]; then
      echo "640x480 Enabled!" >> /dev/tty1
      /usr/bin/retrorun32-640 --triggers -n -s "${SDCARD_PATH}/Roms/Dreamcast (DC)" -d $BIOS_PATH/DC /home/ark/.config/retroarch32/cores/"$2"_libretro.so "$3"
   else
      /usr/bin/retrorun32 --triggers -n -s "${SDCARD_PATH}/Roms/Dreamcast (DC)" -d $BIOS_PATH/DC /home/ark/.config/retroarch32/cores/"$2"_libretro.so "$3"
   fi
printf "\033c" >> /dev/tty1
fi
