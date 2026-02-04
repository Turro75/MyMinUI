#!/bin/bash

systemctl stop emulationstation.service
sleep 2
if [ -f /roms/MyMinUI/r36s/_r36s.sh ]; then
    mv /roms/MyMinUI/r36s/_r36s.sh /roms/MyMinUI/r36s/r36s.sh
fi
if [ ! -e /roms/MyMinUI/r36s/bak ]; then
    mkdir -p /roms/MyMinUI/r36s/bak
    echo "Back up original launcher files"
    cp /usr/bin/emulationstation/emulationstation.sh /roms/MyMinUI/r36s/bak/
    cp /usr/bin/emulationstation/emulationstation.sh.ra /roms/MyMinUI/r36s/bak/
    cp /usr/bin/emulationstation/emulationstation.sh.es /roms/MyMinUI/r36s/bak/
fi
echo "Copy new launcher files"

if [ -f "/dev/input/by-path/platform-ff300000.usb-usb-0:1.2:1.0-event-joystick" ]; then
    # RG351P detected, use its specific launcher
    sudo cp -vf /roms/MyMinUI/r36s/emulationstation_rg351p.sh /usr/bin/emulationstation/emulationstation.sh
else
    # R36S detected, use its specific launcher
    sudo cp -vf /roms/MyMinUI/r36s/emulationstation.sh /usr/bin/emulationstation/
    sudo cp -vf /roms/MyMinUI/r36s/emulationstation.sh.ra /usr/bin/emulationstation/
    sudo cp -vf /roms/MyMinUI/r36s/emulationstation.sh.es /usr/bin/emulationstation/
fi

echo "Rebooting now..."
sleep 3
systemctl reboot