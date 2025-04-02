#!/bin/sh

if [ -f /roms/MyMinUI/r36s/r36s.sh ]; then
    mv /roms/MyMinUI/r36s/r36s.sh /roms/MyMinUI/r36s/_r36s.sh
fi
sudo systemctl reboot
