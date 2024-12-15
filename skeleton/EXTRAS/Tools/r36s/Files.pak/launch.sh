#!/bin/sh

cd $(dirname "$0")

HOME="$SDCARD_PATH"
export LD_LIBRARY_PATH="./lib:${LD_LIBRARY_PATH}"
/opt/dingux/DinguxCommander > ./log.txt
