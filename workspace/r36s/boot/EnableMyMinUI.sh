#!/bin/bash

#check if dArkOS
RET=$(sudo cat /etc/issue | grep -i -o Debian)
echo "Check for dArkOS -> ${RET}"
if [ -e /roms/MyMinUI/r36s/_r36s.sh ]; then
    mv /roms/MyMinUI/r36s/_r36s.sh /roms/MyMinUI/r36s/r36s.sh
fi
if [ ! -e /roms/MyMinUI/r36s/bak ]; then
    mkdir -p /roms/MyMinUI/r36s/bak
    echo "Back up original launcher files"
    cp /usr/bin/emulationstation/emulationstation.sh /roms/MyMinUI/r36s/bak/
    if [ -z "$RET" ]; then 
        cp /usr/bin/emulationstation/emulationstation.sh.ra /roms/MyMinUI/r36s/bak/
        cp /usr/bin/emulationstation/emulationstation.sh.es /roms/MyMinUI/r36s/bak/
    fi
fi
echo "Copy new launcher files"

if [ -e "/dev/input/by-path/platform-ff300000.usb-usb-0:1.2:1.0-event-joystick" ]; then
    # RG351P detected, use its specific launcher
	echo "RG351P detected"
	sudo cp -vf /roms/MyMinUI/r36s/emulationstation_351p.sh /usr/bin/emulationstation/emulationstation.sh
else
    if [ -z "$RET" ]; then 
        # R36S detected, use its specific launcher
	    echo "Detected ArkOS"
        sudo cp -vf /roms/MyMinUI/r36s/emulationstation.sh /usr/bin/emulationstation/
        sudo cp -vf /roms/MyMinUI/r36s/emulationstation.sh.ra /usr/bin/emulationstation/
        sudo cp -vf /roms/MyMinUI/r36s/emulationstation.sh.es /usr/bin/emulationstation/
    else 
	    echo "Detected dArkOS"
        # dArkOS detected, use its specific launcher
        sudo cp -vf /roms/MyMinUI/r36s/emulationstation_darkos.sh /usr/bin/emulationstation/emulationstation.sh
    fi    
fi

echo "Rebooting now..."
sleep 3
sudo systemctl reboot