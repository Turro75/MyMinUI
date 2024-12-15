## MyMinUI install instructions:

# Miyoo Mini:

Format as FAT32 a micro sdcard with enough space to contains all Your games, the volume name assigned to the partition doesn't matter.
in the release file (i.e. MyMinUI-YYYYMMDDb-0-miyoomini.zip) there is a folder called "miyoo" copy that folder as is in the FAT32 partition created above.
Copy also the whole release zip file (leave it zipped) in the FAT32 partition created above.

Put the sdcard in the device and boot, a screen reporting "Installing MyMinUI..." is shown, after a while the device will reboot, again "Installing MyMinUI..." screen then after a while (be patient in this stage as the swap creation process takes time)
Once installation process is completed press the PWR button to shutdown the device, remove the sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the sdcard in the device and play Your games. 


# Miyoo Mini Plus:

Format as FAT32 a micro sdcard with enough space to contains all Your games, the volume name assigned to the partition doesn't matter.
in the release file (i.e. MyMinUI-YYYYMMDDb-0-miyoomini.zip) there is a folder called "miyoo354" copy that folder as is in the FAT32 partition created above.
Copy also the whole release zip file (leave it zipped) in the FAT32 partition created above.

Put the sdcard in the device and boot, a screen reporting "Installing MyMinUI..." is shown, after a while the device will reboot, again "Installing MyMinUI..." screen then after a while (be patient in this stage as the swap creation process takes time)
Once installation process is completed press the PWR button to shutdown the device, remove the sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the sdcard in the device and play Your games. 


# Miyoo A30:

Format as FAT32 a micro sdcard with enough space to contains all Your games, the volume name assigned to the partition doesn't matter.
in the release file (i.e. MyMinUI-YYYYMMDDb-0-my282.zip) there is a folder called "miyoo" copy that folder as is in the FAT32 partition created above.
Copy also the whole release zip file (leave it zipped) in the FAT32 partition created above.

Put the sdcard in the device and boot, a screen reporting "Installing MyMinUI..." is shown.
Once installation process is completed press the PWR button to shutdown the device, remove the sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the sdcard in the device and play Your games. 


# SJGAM M21:

Format as FAT32 (it is also supported exFAT if You like) a micro sdcard with enough space to contains all Your games, the volume name assigned to the partition doesn't matter.
in the release file (i.e. MyMinUI-YYYYMMDDb-0-m21.zip) there is a folder called "m21" copy that folder as is in the FAT32 partition created above.
Move (or copy) the file m21/emulationstation to the root of the FAT32 partition created above.
Copy also the whole release zip file (leave it zipped) in the FAT32 partition created above.

Put the sdcard in the device and boot, a screen reporting "Installing MyMinUI..." is shown, after a while the device will reboot, again "Installing MyMinUI..." screen then after a while (be patient in this stage as the swap creation process takes time)
Once installation process is completed switch off the PWR switch to shutdown the device, remove the sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the sdcard in the device and play Your games. 


# RG35XX OG (2022-2023) Single SDCard method:

Download the debloated stock ambernic image (TF1.img) from the legacy MinUI repo: https://github.com/shauninman/MinUI-Legacy-RG35XX/releases/download/stock-tf1-20230309/TF1.img.zip
Unzip then flash TF1.img to a micro sdcard with enough space to contains all Your games, use the flasher You like. 

This creates 4 partitions in the sdcard, 2 of them are visible only in Linux os while the "misc" and "ROMS" FAT32 partitions are both visible in Macos as well as Windows.
in the release file (i.e. MyMinUI-YYYYMMDDb-0-rg35xx.zip) there is a folder called "rg35xx" that contains a file called "dmenu.bin" copy this file to the "misc" partition.
The "ROMS" partition just created is limited to 3GB, use a partition manager to resize the partition to fill the remaining sdcard available space. 
Copy also the whole release zip file (leave it zipped) to the "ROMS" partition.

Put the sdcard in the device and boot, a screen reporting "Installing MyMinUI..." is shown, after a while the device will reboot, again "Installing MyMinUI..." screen then after a while (be patient in this stage as the swap creation process takes time)
Once installation process is completed press the PWR button to shutdown the device, remove the sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the sdcard in the device and play Your games. 


# RG35XX OG (2022-2023) Two SDCard method:

Download the debloated stock ambernic image (TF1.img) from the legacy MinUI repo: https://github.com/shauninman/MinUI-Legacy-RG35XX/releases/download/stock-tf1-20230309/TF1.img.zip
Unzip then flash TF1.img to a micro sdcard (at least 8GB no need to be bigger than that), I personally use BalenaEtcher on macos, use the flasher You like. 

This creates 4 partitions in the FIRST sdcard, 2 of them are visible only in Linux os while the "misc" and "ROMS" FAT32 partitions are both visible in Macos as well as Windows.
in the release file (i.e. MyMinUI-YYYYMMDDb-0-rg35xx.zip) there is a folder called "rg35xx" that contains a file called "dmenu.bin" copy this file to the "misc" partition.

Format the SECOND sdcard as FAT32, it is also supported exFAT if You like.
Copy the whole release zip file (leave it zipped) to the partition of the SECOND sdcard.

Put both sdcards (sdcard flashed with TF1.img in the TF1 slot...) in the device and boot, a screen reporting "Installing MyMinUI..." is shown, after a while the device will reboot, again "Installing MyMinUI..." screen then after a while (be patient in this stage as the swap creation process takes time)
Once installation process is completed press the PWR button to shutdown the device, remove the second sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the second sdcard in the device and play Your games. 

The TF1 sdcard is used to boot MinUI while the second is used to store minui, all files are in TF2.


# R36S Single SDCard method:

MyMinUI is built over the latest ArkOS image for r36s available here: https://aeolusux.github.io/ArkOS-R3XS/ 

At first You must install ArkOS, follow installation instructions provided by ArkOS support page. If You have a wifi dongle configure now the network to get is working even in MyMinUI.

Once ArkOS successfully installed create the folder <EASYROMS>/roms/MyMinUI in the sdcard, all Roms/Bios must be saved here. You don't have to use the rom/bios folder structure of ArkOS.

in the release file (i.e. MyMinUI-YYYYMMDDb-0-r36s.zip) there is a folder called "r36s" copy that folder as is in the <EASYROMS>/roms/MyMinUI/ created the step above.
Move (or just copy) the file r36s/EnableMyMinUI.sh to the folder <EASYROMS>/roms/tools/
Copy also the whole release zip file (leave it zipped) in the <EASYROMS>/roms/MyMinUI.
Insert the sdcard in the TF1 slot and boot the device
While in EmulationStation go to options->Tools then run the tool EnableMyMinUI.sh and wait for the device to reboot
at reboot the installation process will start showing the screen "installing MyMinUI..."
Once installation process is completed press the PWR button to shutdown the device, remove the sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the sdcard in the device and play Your games. 


# R36S Two SDCard method:

MyMinUI is built over the latest ArkOS image for r36s available here: https://aeolusux.github.io/ArkOS-R3XS/ 

On the FIRST sdcard You must install ArkOS, follow installation instructions provided by ArkOS support page. If You have a wifi dongle configure now the network to get is working even in MyMinUI.

Once ArkOS is successfully installed create the folder <EASYROMS>/roms/MyMinUI in the FIRST sdcard

in the release file (i.e. MyMinUI-YYYYMMDDb-0-r36s.zip) there is a folder called "r36s" copy that folder as is in the <EASYROMS>/roms/MyMinUI/ created the step above.
Move (or just copy) the file r36s/EnableMyMinUI.sh to the folder <EASYROMS>/roms/tools/

Format the SECOND sdcard as FAT32, it is also supported exFAT if You like.
Copy the whole release zip file (leave it zipped) to the partition of the SECOND sdcard.
in the release file (i.e. MyMinUI-YYYYMMDDb-0-r36s.zip) there is a folder called "r36s" copy that folder as is in the partition of the SECOND sdcard (Yes You must copy that folder to both sdcards)

While in EmulationStation go to options->Tools then run the tool EnableMyMinUI.sh 
at reboot the installation process will start showing the screen "installing MyMinUI..."
Once installation process is completed press the PWR button to shutdown the device, remove the sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the sdcard in the device and play Your games. 


# Share sdcard across other devices:

Well You can setup a single sdcard that can run on all 4 supported devices.

Only two requirements:

1)  The shared sdcard must be formatted as FAT32.
2)  In case one of the shared devices is an RG35XX OG You must use Two sdcard method.

There is no specific sequence to follow, You can add a device at any time, just follow the instructions provided for each device.

The devices will share bios, roms and saves folders.
Some saved state files may work across devices (i.e. doom), but not all so don't expect support on that in case. If I'll move to a single setup file device independent I'll keep them separated per device.

The pico8 native binary files must be copied under the Tools/<devicename>/splore folder for each device. 
