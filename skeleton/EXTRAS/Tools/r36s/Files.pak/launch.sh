#!/bin/sh

cd $(dirname "$0")

HOME="$SDCARD_PATH"
MYDISK=$(df -P "$SDCARD_PATH" | tail -1 | cut -d' ' -f 1) ./MyCommander.elf 
