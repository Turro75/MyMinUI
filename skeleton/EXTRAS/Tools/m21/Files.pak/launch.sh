#!/bin/sh

cd $(dirname "$0")

HOME="$SDCARD_PATH"
export LD_LIBRARY_PATH="./lib:${LD_LIBRARY_PATH}"
./DinguxCommander &> ./log.txt