#!/bin/sh

cd $(dirname "$0")

HOME="$SDCARD_PATH"
COMMANDER_SCREEN_FIX=1  MYDISK=$(df -P "$SDCARD_PATH" | tail -1 | cut -d' ' -f 1) ./MyCommander.elf > $LOGS_PATH/MyCommander.log 2>&1
