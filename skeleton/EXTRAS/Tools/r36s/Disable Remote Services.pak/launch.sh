#!/bin/sh

cd $(dirname "$0")

#HOME="$SDCARD_PATH"
#export LD_LIBRARY_PATH="./lib:${LD_LIBRARY_PATH}"
if [ -f "/opt/system/Disable Remote Services.sh" ]; then
    "/opt/system/Disable Remote Services.sh" > ./log.txt
else
    sudo systemctl stop ssh
    sudo systemctl stop smbd
    sudo systemctl stop filebrowser
fi