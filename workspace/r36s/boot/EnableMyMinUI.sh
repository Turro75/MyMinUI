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
sudo cp /roms/MyMinUI/r36s/emulationstation/emulationstation.sh /usr/bin/emulationstation/
sudo cp /roms/MyMinUI/r36s/emulationstation.sh.ra /usr/bin/emulationstation/
sudo cp /roms/MyMinUI/r36s/emulationstation.sh.es /usr/bin/emulationstation/
echo "Rebooting now..."
sleep 3
systemctl reboot