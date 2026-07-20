#!/bin/sh

cd $(dirname "$0")

#HOME="$SDCARD_PATH"
#export LD_LIBRARY_PATH="./lib:${LD_LIBRARY_PATH}"
if [ -f "/opt/system/Enable Remote Services.sh" ]; then
    "/opt/system/Enable Remote Services.sh" > ./log.txt
else
    sudo systemctl start ssh
    sudo systemctl start smbd
    sudo systemctl start filebrowser
fi