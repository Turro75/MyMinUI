## MyMinUI install instructions:

# Miyoo Mini:

Format as FAT32 a micro sdcard with enough space to contains all Your games, the volume name assigned to the partition doesn't matter.
in the release file (i.e. MyMinUI-YYYYMMDDb-0-miyoomini.zip) there is a folder called "miyoo" copy that folder as is in the FAT32 partition created above.
Copy also the whole release zip file (leave it zipped) in the FAT32 partition created above.

The sdcard file structure must be:
<pre>
├── MyMinUI-YYYYMMDDb-0-miyoomini.zip
└── miyoo
    └── app
        ├── miyoomini.sh
        ├── my282.sh
        ├── MainUI
        ├── keymon
        └── .tmp_update
            ├── miyoomini.sh
            ├── updater
            └── miyoomini
                ├── show.elf
                ├── updating.png
                └── installing.png
</pre>

Put the sdcard in the device and boot, a screen reporting "Installing MyMinUI..." is shown, after a while the device will reboot, again "Installing MyMinUI..." screen then after a while (be patient in this stage as the swap creation process takes time)
Once installation process is completed press the PWR button to shutdown the device, remove the sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the sdcard in the device and play Your games. 


# Miyoo Mini Plus:

Format as FAT32 a micro sdcard with enough space to contains all Your games, the volume name assigned to the partition doesn't matter.
in the release file (i.e. MyMinUI-YYYYMMDDb-0-miyoomini.zip) there is a folder called "miyoo354" copy that folder as is in the FAT32 partition created above.
Copy also the whole release zip file (leave it zipped) in the FAT32 partition created above.

The sdcard file structure must be:
<pre>
├── MyMinUI-YYYYMMDDb-0-miyoomini.zip
└── miyoo354
    └── app
        ├── miyoomini.sh
        ├── my282.sh
        ├── MainUI
        ├── keymon
        └── .tmp_update
            ├── miyoomini.sh
            ├── updater
            └── miyoomini
                ├── show.elf
                ├── updating.png
                └── installing.png
</pre>

Put the sdcard in the device and boot, a screen reporting "Installing MyMinUI..." is shown, after a while the device will reboot, again "Installing MyMinUI..." screen then after a while (be patient in this stage as the swap creation process takes time)
Once installation process is completed press the PWR button to shutdown the device, remove the sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the sdcard in the device and play Your games. 


# Miyoo A30:

Format as FAT32 a micro sdcard with enough space to contains all Your games, the volume name assigned to the partition doesn't matter.
in the release file (i.e. MyMinUI-YYYYMMDDb-0-my282.zip) there is a folder called "miyoo" copy that folder as is in the FAT32 partition created above.
Copy also the whole release zip file (leave it zipped) in the FAT32 partition created above.

The sdcard file structure must be:
<pre>
├── MyMinUI-YYYYMMDDb-0-my282.zip
└── miyoo
    └── app
        ├── miyoomini.sh
        ├── my282.sh
        ├── MainUI
        ├── keymon
        └── .tmp_update
            ├── my282.sh
            ├── updater
            └── my282
                ├── unzip
                ├── done.png
                ├── updating.png
                ├── installing.png
                └── show.elf
</pre>

Put the sdcard in the device and boot, a screen reporting "Installing MyMinUI..." is shown.
Once installation process is completed press the PWR button to shutdown the device, remove the sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the sdcard in the device and play Your games. 


# SJGAM M21/M22pro:

Format as FAT32 (it is also supported exFAT if You like) a micro sdcard with enough space to contains all Your games, the volume name assigned to the partition doesn't matter.
in the release file (i.e. MyMinUI-YYYYMMDDb-0-m21.zip) there is a folder called "m21" copy that folder as is in the FAT32 partition created above.
Move (or copy) the file m21/emulationstation and m21/tomato to the root of the FAT32 partition created above.
Copy also the whole release zip file (leave it zipped) in the FAT32 partition created above.

The sdcard file structure must be:
<pre>
├── MyMinUI-YYYYMMDDb-0-m21.zip
├── emulationstation
├── tomato
└── m21
    ├── updating.png
    ├── installing.png
    ├── emulationstation
    ├── tomato
    ├── libmusl
    │   ├── libSDL2-2.0.so.0
    │   ├── libasound.so.2
    │   ├── libext2fs.so.2
    │   ├── libblkid.so.1
    │   ├── libSDL-1.2.so.0
    │   ├── libpng16.so.16
    │   ├── libfuse.so.2
    │   ├── libz.so.1
    │   ├── libSDL2_image-2.0.so.0
    │   ├── libSDL2_ttf-2.0.so.0
    │   ├── libSDL_ttf-2.0.so.0
    │   ├── libSDL_image-1.2.so.0
    │   └── libcom_err.so.2
    └── binmusl
        ├── unzip
        ├── amixer
        ├── fuse2fs
        └── show.elf
</pre>

Put the sdcard in the device and boot, a screen reporting "Installing MyMinUI..." is shown, after a while the device will reboot, again "Installing MyMinUI..." screen then after a while (be patient in this stage as the swap creation process takes time)
Once installation process is completed switch off the PWR switch to shutdown the device, remove the sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the sdcard in the device and play Your games. 


# RG35XX OG (2022-2023) Single SDCard method:

Download the debloated stock ambernic image (TF1.img) from the legacy MinUI repo: https://github.com/shauninman/MinUI-Legacy-RG35XX/releases/download/stock-tf1-20230309/TF1.img.zip
Unzip then flash TF1.img to a micro sdcard with enough space to contains all Your games, use the flasher You like. 

This creates 4 partitions in the sdcard, 2 of them are visible only in Linux os while the "misc" and "ROMS" FAT32 partitions are both visible in Macos as well as Windows.
in the release file (i.e. MyMinUI-YYYYMMDDb-0-rg35xx.zip) there is a folder called "rg35xx" that contains a file called "dmenu.bin" copy this file to the "misc" partition.
The "ROMS" partition just created is limited to 3GB, use a partition manager to resize the partition to fill the remaining sdcard available space. 
Copy also the whole release zip file (leave it zipped) to the "ROMS" partition.

the misc partition now contains:
<pre>
├── uImage
├── ramdisk.img
├── recovery_logo.bmp.gz
├── kernel.dtb
├── dmenu.bin
├── def_config.bin
├── charging.png
├── boot_logo.bmp.gz
├── charger_logo.bmp.gz
├── battery_low.bmp.gz
├── 0_charger_frame.bmp.gz
├── 1_charger_frame.bmp.gz
├── 2_charger_frame.bmp.gz
├── 3_charger_frame.bmp.gz
├── 4_charger_frame.bmp.gz
├── 5_charger_frame.bmp.gz
├── uenv.txt
└── modules
   ├── pvrsrvkm.ko
   ├── ethernet.ko
   ├── atc260x_cap_gauge.ko
   ├── dc_owl.ko
   └── gpio_keys_polled.ko
</pre>

the ROMS partition must contains:
<pre>
├── MyMinUI-YYYYMMDDb-0-rg35xx.zip
├── bios
├── CPS
├── FBAHACK
├── FC
├── GB
├── GBA
├── GBC
├── GG
├── MAME
├── MD
├── NEOGEO
├── NGPC
├── PCENGINE
├── PS1
├── save
├── SFC
├── SMS
├── VERTICAL
└── WonderSwan
</pre>


Put the sdcard in the device and boot, a screen reporting "Installing MyMinUI..." is shown, after a while the device will reboot, again "Installing MyMinUI..." screen then after a while (be patient in this stage as the swap creation process takes time)
Once installation process is completed press the PWR button to shutdown the device, remove the sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the sdcard in the device and play Your games. 


# RG35XX OG (2022-2023) Two SDCard method:

Download the debloated stock ambernic image (TF1.img) from the legacy MinUI repo: https://github.com/shauninman/MinUI-Legacy-RG35XX/releases/download/stock-tf1-20230309/TF1.img.zip
Unzip then flash TF1.img to a micro sdcard (at least 8GB no need to be bigger than that), I personally use BalenaEtcher on macos, use the flasher You like. 

This creates 4 partitions in the FIRST sdcard, 2 of them are visible only in Linux os while the "misc" and "ROMS" FAT32 partitions are both visible in Macos as well as Windows.
in the release file (i.e. MyMinUI-YYYYMMDDb-0-rg35xx.zip) there is a folder called "rg35xx" that contains a file called "dmenu.bin" copy this file to the "misc" partition.

the misc partition on the FIRST sdcard now contains:
<pre>
├── uImage
├── ramdisk.img
├── recovery_logo.bmp.gz
├── kernel.dtb
├── dmenu.bin
├── def_config.bin
├── charging.png
├── boot_logo.bmp.gz
├── charger_logo.bmp.gz
├── battery_low.bmp.gz
├── 0_charger_frame.bmp.gz
├── 1_charger_frame.bmp.gz
├── 2_charger_frame.bmp.gz
├── 3_charger_frame.bmp.gz
├── 4_charger_frame.bmp.gz
├── 5_charger_frame.bmp.gz
├── uenv.txt
└── modules
   ├── pvrsrvkm.ko
   ├── ethernet.ko
   ├── atc260x_cap_gauge.ko
   ├── dc_owl.ko
   └── gpio_keys_polled.ko
</pre>

Format the SECOND sdcard as FAT32, it is also supported exFAT if You like.
Copy the whole release zip file (leave it zipped) to the partition of the SECOND sdcard.

the SECOND sdcard must contains:
<pre>
└── MyMinUI-YYYYMMDDb-0-rg35xx.zip
</pre>

Put both sdcards (sdcard flashed with TF1.img in the TF1 slot...) in the device and boot, a screen reporting "Installing MyMinUI..." is shown, after a while the device will reboot, again "Installing MyMinUI..." screen then after a while (be patient in this stage as the swap creation process takes time)
Once installation process is completed press the PWR button to shutdown the device, remove the second sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the second sdcard in the device and play Your games. 

The TF1 sdcard is used to boot MinUI while the second is used to store minui, all files are in TF2.


# R36S Single SDCard method:

The r36s platform supports: Anbernic rg353p(s), v(s) and m while arc is not tested, GameConsole r36s, r36sPlus (4" 1:1) and r40xx pro max (4" 4:3), Powkiddy rgb30 and v10.

MyMinUI for r36s is built over the latest ArkOS image for r36s available here: https://aeolusux.github.io/ArkOS-R3XS/ 
MyMinUI for rg353x is built over the latest official ArkOS image for available here at the end of the page: 
https://github.com/christianhaitian/arkos/wiki

At first You must install ArkOS, follow installation instructions provided by ArkOS support page. If You have a wifi dongle configure now the network to get is working even in MyMinUI.

Once ArkOS successfully installed create the folder <EASYROMS>/roms/MyMinUI in the sdcard, all Roms/Bios must be saved here. You don't have to use the rom/bios folder structure of ArkOS.

in the release file (i.e. MyMinUI-YYYYMMDDb-0-r36s.zip) there is a folder called "r36s" copy that folder as is in the <EASYROMS>/roms/MyMinUI/ created the step above.
Move (or just copy) the file r36s/EnableMyMinUI.sh to the folder <EASYROMS>/roms/tools/
Copy also the whole release zip file (leave it zipped) in the <EASYROMS>/roms/MyMinUI.

the partition EASYROMS must contains now: (+ all the arkos stuff)
<pre>
└── roms
    ├── MyMinUI
    │   ├── MyMinUI-YYYYMMDDb-0-r36s.zip
    │   └── r36s
    │       ├── unzip
    │       ├── show.elf
    │       ├── updating.png
    │       ├── installing.png
    │       ├── emulationstation.sh.ra
    │       ├── emulationstation.sh
    │       ├── emulationstation.sh.es
    │       ├── r36s.sh
    │       ├── r36s_stage2.sh
    │       └── EnableMyMinUI.sh
    └── tools
        └── EnableMyMinUI.sh
</pre>

Insert the sdcard in the TF1 slot and boot the device
While in EmulationStation go to options->Tools then run the tool EnableMyMinUI.sh and wait for the device to reboot
at reboot the installation process will start showing the screen "installing MyMinUI..."
Once installation process is completed press the PWR button to shutdown the device, remove the sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the sdcard in the device and play Your games. 




# R36S Two SDCard method:

The r36s platform supports: Anbernic rg353p(s), v(s) and m while arc is not tested, GameConsole r36s, r36sPlus (4" 1:1) and r40xx pro max (4" 4:3), Powkiddy rgb30 and v10.

MyMinUI for r36s is built over the latest ArkOS image for r36s available here: https://aeolusux.github.io/ArkOS-R3XS/ 
MyMinUI for rg353x is built over the latest official ArkOS image available at the end of the page: 
https://github.com/christianhaitian/arkos/wiki

On the FIRST sdcard You must install ArkOS, follow installation instructions provided by ArkOS support page. If You have a wifi dongle configure now the network to get is working even in MyMinUI.

Once ArkOS is successfully installed create the folder <EASYROMS>/roms/MyMinUI in the FIRST sdcard

in the release file (i.e. MyMinUI-YYYYMMDDb-0-r36s.zip) there is a folder called "r36s" copy that folder as is in the <EASYROMS>/roms/MyMinUI/ created the step above.
Move (or just copy) the file r36s/EnableMyMinUI.sh to the folder <EASYROMS>/roms/tools/

the partition EASYROMS on FIRST sdcard must contains now: (+ all the akos stuff)
<pre>
└── roms
    ├── MyMinUI
    │   └── r36s
    │       ├── unzip
    │       ├── show.elf
    │       ├── updating.png
    │       ├── installing.png
    │       ├── emulationstation.sh.ra
    │       ├── emulationstation.sh
    │       ├── emulationstation.sh.es
    │       ├── r36s.sh
    │       ├── r36s_stage2.sh
    │       └── EnableMyMinUI.sh
    └── tools
        └── EnableMyMinUI.sh
</pre>

Format the SECOND sdcard as FAT32, it is also supported exFAT if You like.
Copy the whole release zip file (leave it zipped) to the partition of the SECOND sdcard.
in the release file (i.e. MyMinUI-YYYYMMDDb-0-r36s.zip) there is a folder called "r36s" copy that folder as is in the partition of the SECOND sdcard (Yes You must copy that folder to both sdcards)

The SECOND sdcard file structure must be:
<pre>
├── MyMinUI-YYYYMMDDb-0-r36s.zip
└── r36s
    ├── unzip
    ├── show.elf
    ├── updating.png
    ├── installing.png
    ├── emulationstation.sh.ra
    ├── emulationstation.sh
    ├── emulationstation.sh.es
    ├── r36s.sh
    ├── r36s_stage2.sh
    └── EnableMyMinUI.sh
</pre>

While in EmulationStation go to options->Tools then run the tool EnableMyMinUI.sh 
at reboot the installation process will start showing the screen "installing MyMinUI..."
Once installation process is completed press the PWR button to shutdown the device, remove the sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the sdcard in the device and play Your games. 

# Share sdcard across other devices:

Well You can setup a single sdcard that can run on all 4 supported devices.

Only two requirements:

1)  The shared sdcard must be formatted as FAT32.
2)  In case one of the shared devices is an RG35XX OG or an ArkOS based device You must use Two sdcard method.

There is no specific sequence to follow, You can add a device at any time, just follow the instructions provided for each device.

The devices will share bios, roms and saves folders.
Some saved state files may work across devices (i.e. doom), but not all so don't expect support on that in case. If I'll move to a single setup file device independent I'll keep them separated per device.

The pico8 native raspberry binary files  pico8.dat, pico8_dyn and pico8_64 must be copied under the Bios/P8 folder it will be the same for all devices.
