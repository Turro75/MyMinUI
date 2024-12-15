#!/bin/bash

directory="${SDCARD_PATH}/Saves/NDS"

if  [[ ! -d "${directory}/backup" ]]; then
  mkdir -p ${directory}/backup
fi
if  [[ ! -d "${directory}/cheats" ]]; then
  mkdir -p ${directory}/cheats
fi
if  [[ ! -d "${directory}/savestates" ]]; then
  mkdir -p ${directory}/savestates
fi
if  [[ ! -d "${directory}/slot2" ]]; then
  mkdir -p ${directory}/slot2
fi

ln -sfn ${directory}/backup /opt/drastic/backup
ln -sfn ${directory}/cheats /opt/drastic/cheats
ln -sfn ${directory}/savestates /opt/drastic/savestates
ln -sfn ${directory}/slot2 /opt/drastic/slot2


sudo /usr/local/bin/drastickeydemon.py &

cd /opt/drastic
if [ -f "/opt/system/Advanced/Switch to main SD for Roms.sh" ]; then
  LD_PRELOAD=./TF2/libSDL2-2.0.so.0.3000.2 ./drastic "$1"
  ln -sfn /roms2/nds/backup /opt/drastic/backup
  ln -sfn /roms2/nds/cheats /opt/drastic/cheats
  ln -sfn /roms2/nds/savestates /opt/drastic/savestates
  ln -sfn /roms2/nds/slot2 /opt/drastic/slot2

else
  LD_PRELOAD=./TF1/libSDL2-2.0.so.0.3000.2 ./drastic "$1"
  ln -sfn /roms/nds/backup /opt/drastic/backup
  ln -sfn /roms/nds/cheats /opt/drastic/cheats
  ln -sfn /roms/nds/savestates /opt/drastic/savestates
  ln -sfn /roms/nds/slot2 /opt/drastic/slot2
fi

sudo killall python3

sudo systemctl restart oga_events &
