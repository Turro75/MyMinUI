#!/bin/sh

RA_HOME = ${BIOS_PATH}/RETROARCH
cd ${RA_HOME}
retroarch.elf --features -v &> "$LOGS_PATH/RetroArchFeatures.txt"
retroarch.elf --help -v &> "$LOGS_PATH/RetroArchHelp.txt"
retroarch.elf --config ${BIOS_PATH}/RETROARCH/${PLATFORM}/retroarch.cfg --menu -v &> "$LOGS_PATH/RetroArch.txt"
