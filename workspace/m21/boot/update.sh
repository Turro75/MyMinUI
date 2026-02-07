#!/bin/sh

# NOTE: becomes .system/m21/bin/install.h
# NOTE2: this runs in the system env so only stock musl commands
if [ -f /mnt/SDCARD/.system/m21/custombuttonmapping.env ]; then
    rm -f /mnt/SDCARD/.system/m21/_custombuttonmapping.env 
else
    mv  /mnt/SDCARD/.system/m21/_custombuttonmapping.env /mnt/SDCARD/.system/m21/custombuttonmapping.env
fi
exit 0