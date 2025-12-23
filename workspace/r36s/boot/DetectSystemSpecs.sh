#!/bin/bash

mkdir -p /roms/MyMinUI
TMPFILE1=/roms/MyMinUI/tmpfilespecs1.txt
#TMPFILE2=./tmpfilespecs2.txt
LOGFILE=/roms/MyMinUI/DetectSystemSpecs.log
echo "Starting system specs detection..." > $LOGFILE
echo "" > $TMPFILE1

#detect input events
echo "##############################" >> $LOGFILE
echo "Listing /dev/input/by-path/* devices:" > $LOGFILE
ls -l /dev/input/by-path/* >> $LOGFILE
#detect buttons 
echo "##############################" >> $LOGFILE
echo "Detecting input events for buttons..." >> $LOGFILE
evtest &> $TMPFILE1 & sleep 1 ; killall -9 evtest

EVENT=$(cat $TMPFILE1 | grep ^/dev | cut -d':' -f1)
rm -f $TMPFILE1
for i in $EVENT; do 
    echo "Testing $i" >> $LOGFILE    
    evtest $i >> $LOGFILE & 
    sleep 1
done 
killall -9 evtest


#detect sound specs
echo "##############################" >> $LOGFILE
echo "Detecting sound card info..." >> $LOGFILE
amixer >> $LOGFILE


#detect TFSlot
echo "##############################" >> $LOGFILE
echo "Checking TF2 slot presence" >> $LOGFILE
TF1DISKNUM=$(mount | grep "/ type ext4" | cut -d'p' -f1 | cut -d'k' -f2)
TF2DISKNUM=$((${TF1DISKNUM}+1))
echo "TF1 disk num is $TF1DISKNUM" >> $LOGFILE
echo "TF2 disk num is $TF2DISKNUM" >> $LOGFILE

#detect partitions
echo "##############################" >> $LOGFILE
echo "Listing partitions..." >> $LOGFILE
mount >> $LOGFILE

echo "##############################" >> $LOGFILE
echo "Listing /dev/mmcblk* devices:" >> $LOGFILE
ls -l /dev/mmcblk* >> $LOGFILE

#detect CPU info
echo "##############################" >> $LOGFILE
echo "Detecting CPU info..." >> $LOGFILE
cat /proc/cpuinfo >> $LOGFILE
echo "Governors available:" >> $LOGFILE
cat /sys/devices/system/cpu/cpufreq/policy0/scaling_available_governors >> $LOGFILE
echo "Frequencyies available:" >> $LOGFILE 
cat /sys/devices/system/cpu/cpufreq/policy0/scaling_available_frequencies >> $LOGFILE

#detect fb info
echo "##############################" >> $LOGFILE
echo "Detecting framebuffer info..." >> $LOGFILE
echo "fb0 info:" >> $LOGFILE
ls -l /dev/fb* >> $LOGFILE
echo "fb virtual_size:" >> $LOGFILE
cat /sys/class/graphics/fb0/virtual_size >> $LOGFILE
echo "fb bits_per_pixel:" >> $LOGFILE
cat /sys/class/graphics/fb0/bits_per_pixel >> $LOGFILE
echo "fb stride:" >> $LOGFILE
cat /sys/class/graphics/fb0/stride >> $LOGFILE
echo "fb modes:" >> $LOGFILE
cat /sys/class/graphics/fb0/modes >> $LOGFILE
echo "drm modes:" >> $LOGFILE
cat /sys/class/drm/card0-*/modes >> $LOGFILE

#detect modules loaded
echo "##############################" >> $LOGFILE
echo "Detecting loaded modules..." >> $LOGFILE
lsmod >> $LOGFILE

echo "System specs detection completed." >> $LOGFILE
