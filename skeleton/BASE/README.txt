MyMinUI is a minimal launcher for the Miyoo Mini (and Plus), RG35XX Original, SJGAM M21 and GameConsole R36s based on MinUI ()

Source:
https://github.com/Turro75/MyMinUI

----------------------------------------
Installing

# Miyoo Mini:

Format as FAT32 a micro sdcard with enough space to contains all Your games, the volume name assigned to the partition doesn't matter.
in the release file (i.e. MyMinUI-YYYYMMDDb-0-miyoomini.zip) there is a folder called "miyoo" copy that folder as is in the FAT32 partition created above.
Copy also the whole release zip file (leave it zipped) in the FAT32 partition created above.

The sdcard file structure must be:

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


Put the sdcard in the device and boot, a screen reporting "Installing MyMinUI..." is shown, after a while the device will reboot, again "Installing MyMinUI..." screen then after a while (be patient in this stage as the swap creation process takes time)
Once installation process is completed press the PWR button to shutdown the device, remove the sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the sdcard in the device and play Your games. 

If you have a later device with RTC support, you can enable it by creating an empty file named "enable-rtc" (no extension) in "/.userdata/shared/".

# Miyoo Mini Plus:

Format as FAT32 a micro sdcard with enough space to contains all Your games, the volume name assigned to the partition doesn't matter.
in the release file (i.e. MyMinUI-YYYYMMDDb-0-miyoomini.zip) there is a folder called "miyoo354" copy that folder as is in the FAT32 partition created above.
Copy also the whole release zip file (leave it zipped) in the FAT32 partition created above.

The sdcard file structure must be:

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


Put the sdcard in the device and boot, a screen reporting "Installing MyMinUI..." is shown, after a while the device will reboot, again "Installing MyMinUI..." screen then after a while (be patient in this stage as the swap creation process takes time)
Once installation process is completed press the PWR button to shutdown the device, remove the sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the sdcard in the device and play Your games. 

If you have a later device with RTC support, you can enable it by creating an empty file named "enable-rtc" (no extension) in "/.userdata/shared/".

# Miyoo A30:

Format as FAT32 a micro sdcard with enough space to contains all Your games, the volume name assigned to the partition doesn't matter.
in the release file (i.e. MyMinUI-YYYYMMDDb-0-my282.zip) there is a folder called "miyoo" copy that folder as is in the FAT32 partition created above.
Copy also the whole release zip file (leave it zipped) in the FAT32 partition created above.

The sdcard file structure must be:

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

Put the sdcard in the device and boot, a screen reporting "Installing MyMinUI..." is shown.
Once installation process is completed press the PWR button to shutdown the device, remove the sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the sdcard in the device and play Your games. 


# SJGAM M21/M22pro:

Format as FAT32 (it is also supported exFAT if You like) a micro sdcard with enough space to contains all Your games, the volume name assigned to the partition doesn't matter.
in the release file (i.e. MyMinUI-YYYYMMDDb-0-m21.zip) there is a folder called "m21" copy that folder as is in the FAT32 partition created above.
Move (or copy) the file m21/emulationstation and m21/tomato to the root of the FAT32 partition created above.
Copy also the whole release zip file (leave it zipped) in the FAT32 partition created above.


The sdcard file structure must be:

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


the ROMS partition must contains:

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

Put the sdcard in the device and boot, a screen reporting "Installing MyMinUI..." is shown, after a while the device will reboot, again "Installing MyMinUI..." screen then after a while (be patient in this stage as the swap creation process takes time)
Once installation process is completed press the PWR button to shutdown the device, remove the sdcard and insert it in the pc, now You can fill the bios and roms folders with Your files. Put the sdcard in the device and play Your games. 


# RG35XX OG (2022-2023) Two SDCard method:

Download the debloated stock ambernic image (TF1.img) from the legacy MinUI repo: https://github.com/shauninman/MinUI-Legacy-RG35XX/releases/download/stock-tf1-20230309/TF1.img.zip
Unzip then flash TF1.img to a micro sdcard (at least 8GB no need to be bigger than that), I personally use BalenaEtcher on macos, use the flasher You like. 

This creates 4 partitions in the FIRST sdcard, 2 of them are visible only in Linux os while the "misc" and "ROMS" FAT32 partitions are both visible in Macos as well as Windows.
in the release file (i.e. MyMinUI-YYYYMMDDb-0-rg35xx.zip) there is a folder called "rg35xx" that contains a file called "dmenu.bin" copy this file to the "misc" partition.

the misc partition on the FIRST sdcard now contains:

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


Format the SECOND sdcard as FAT32, it is also supported exFAT if You like.
Copy the whole release zip file (leave it zipped) to the partition of the SECOND sdcard.

the SECOND sdcard must contains:

└── MyMinUI-YYYYMMDDb-0-rg35xx.zip

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

the partition EASYROMS on FIRST sdcard must contains now: (+ the whole arkos stuff)
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

The pico8 native binary files must be copied in Bios/P8 folder. 

----------------------------------------
Updating

ALL

Copy "MyMinUI-YYYYMMDDb-0-<PLATFORM>.zip" (without unzipping) to the root of the SD card containing your Roms.

----------------------------------------
Shortcuts


 MIYOO MINI PLUS / RG35XX / M21
  
  Brightness: MENU + VOLUME UP
                  or VOLUME DOWN
  
MIYOO MINI 

  Volume: SELECT + L or R
  Brightness: START + L or R1

MIYOO MINI (PLUS) / RG35XX 
  
  Sleep: POWER
  Wake: POWER
  
M21 / R36S
  
  Sleep: 2 mins timeout
  Wake: MENU
  Power Off: keep pressed MENU for more than 2 secs, then when screen is dark switch off Power switch.

M22pro

  since menu button is missing press Select + Start to act as a menu button.

----------------------------------------
Quicksave & auto-resume

MyMinUI will create a quicksave when powering off in-game. The next time you power on the device it will automatically resume from where you left off. A quicksave is created when powering off manually or automatically after a short sleep. On devices without a POWER button (M21) press the MENU button twice to put the device to sleep before flipping the POWER switch.

----------------------------------------
Roms

Included in this zip is a "Roms" folder containing folders for each console MinUI currently supports. You can rename these folders but you must keep the uppercase tag name in parentheses in order to retain the mapping to the correct emulator (eg. "Nintendo Entertainment System (FC)" could be renamed to "Nintendo (FC)", "NES (FC)", or "Famicom (FC)"). 

When one or more folder share the same display name (eg. "Game Boy Advance (GBA)" and "Game Boy Advance (MGBA)") they will be combined into a single menu item containing the roms from both folders (continuing the previous example, "Game Boy Advance"). This allows opening specific roms with an alternate pak.

Recommended Rom version for Arcade:
  FBN: it requires the FinalBurn Neo - Arcade ROM Set (Full Non-Merged)
       support hiscore.dat when placed in the Bios/FBN folder
  MAME: it requires MAME 2003-Plus Reference Full Non-Merged Romsets
       support hiscore.dat when placed in the Bios/MAME folder

----------------------------------------
Bios

Some emulators require or perform much better with official bios. MyMinUI is strictly BYOB. Place the bios for each system in a folder that matches the tag in the corresponding "Roms" folder name (eg. bios for "Sony PlayStation (PS)" roms goes in "/Bios/PS/"),

Bios file names are case-sensitive:

├── A2600
├── A5200
│   ├── 5200.rom
│   └── ATARIBAS.ROM
├── A7800
│   └── 7800 BIOS (U).rom
├── DC
├── DOOM
│   └── prboom.wad
├── DOS
├── FBN
│   └── fbneo
│       └── hiscore.dat
├── FC
│   └── disksys.rom
├── GB
│   └── gb_bios.bin
├── GBA
│   └── gba_bios.bin
├── GBC
│   └── gbc_bios.bin
├── GG
│   └── bios.gg
├── MAME
│   └── mame2003-plus
│       └── hiscore.dat
├── MD
│   └── bios_MD.bin
├── MGBA
│   └── gba_bios.bin
├── N64
├── NG
│   └── neogeo.zip
├── NGCD
│   └── neocd
│       └── neocd.zip
├── NGPC
├── P8
│   └── put-pico8.dat-pico8_dyn-pico8_64-here
├── PCE
│   └── syscard3.pce
├── PKM
│   └── bios.min
├── PS
│   └── PSXONPSP660.BIN
├── PSP
├── PUAE
│   ├── kick34005.A500
│   ├── kick40063.A600
│   └── kick40068.A1200
├── QUAKE
├── SFC
├── SGB
│   └── sgb_bios.bin
├── SMS
│   ├── bios_E.sms
│   ├── bios_J.sms
│   └── bios_U.sms
└── VB

----------------------------------------
Disc-based games

To streamline launching multi-file disc-based games with MyMinUI place your bin/cue (and/or iso/wav files) in a folder with the same name as the cue file. MyMinUI will automatically launch the cue file instead of navigating into the folder when selected, eg. 

  Harmful Park (English v1.0)/
    Harmful Park (English v1.0).bin
    Harmful Park (English v1.0).cue

For multi-disc games, put all the files for all the discs in a single folder. Then create an m3u file in that folder (just a text file containing the relative path to each disc's cue file on a separate line) with the same name as the folder. Instead of showing the entire messy contents of the folder, MyMinUI will launch the appropriate cue file, eg. For a "Policenauts" folder structured like this:

  Policenauts (English v1.0)/
    Policenauts (English v1.0).m3u
    Policenauts (Japan) (Disc 1).bin
    Policenauts (Japan) (Disc 1).cue
    Policenauts (Japan) (Disc 2).bin
    Policenauts (Japan) (Disc 2).cue

The m3u file would contain just:

  Policenauts (Japan) (Disc 1).cue
  Policenauts (Japan) (Disc 2).cue

MyMinUI also supports chd files and pbp files (recommended). Regardless of the multi-disc file format used, every disc of the same game share the same memory card and save state slots.

----------------------------------------
Collections

A collection is just a text file containing an ordered list of full paths to rom, cue, or m3u files. These text files live in the "Collections" folder at the root of your SD card, eg. "/Collections/Metroid series.txt" might look like this:

  /Roms/GBA/Metroid Zero Mission.gba
  /Roms/GB/Metroid II.gb
  /Roms/SNES (SFC)/Super Metroid.sfc
  /Roms/GBA/Metroid Fusion.gba

----------------------------------------
There are 3 working modes Standard, Simple and Fancy, to select the mode press menu then up-down to switch the current mode.

Standard mode (Default)
 
Same look & feel of MinUI keeping additional MyMinUI features.

----------------------------------------
Fancy mode
 
a reworked layout that allows to show boxart and saved state previews.

----------------------------------------
Simple mode

Not simple enough for you (or maybe your kids)? MinUI has a simple mode that hides the Tools folder and replaces Options in the in-game menu with Reset. Perfect for handing off to littles (and olds too I guess). Just create an empty file named "enable-simple-mode" (no extension) in "/.userdata/shared/".


----------------------------------------
Tools

Several tools are provided:
Files -> Dingux commander with editing mode enabled
Input -> quick control input checker, to see analog activity keep any button pressed
Clear Recent -> empty the recent list
Splore -> starts splore the P8 native cart manager (only offline)
Clock -> set current system Clock
ToggleSleepMode -> change PWR button behavior from sleep to power off (Default) and viceversa
ToggleSeekPageTriggers -> allow page scrolling by Left/Right (Default) or L2/R2
ToggleHideState -> Hide saved state preview (Default OFF)
ToggleHideBOXARTifState -> If state is present the BOXART below is not shown (Default OFF) 
Convert BoxArt -> Utility to convert images in the format You like, refer to BOXART.md for details

----------------------------------------
Support for non US fonts:

It is not intended to support non US characters, in case You need there is an easy workaround as the system actually support utf8.
Take any otf/ttf font (the one You like) that supports the charset You need then overwrite the system font which are
- .system/<PLATFORM>/res/BPreplayBold-unhinted.otf (for minui, minarch and other tools)
- Tools/<PLATFORM>/Files/Res/FreeSans.ttf (specific for the tool Files DinguxCommander)
It is important to rename the new font as the system is looking for fonts called as the files provided in the releases.
Of course the look&feel will change.

Be aware that the font files will be overwritten at any update.
----------------------------------------
Thanks

BIG BIG Thank to ShaunInman for sharing his amazing work on MinUI from which I based MyMinUI.

To eggs, for his NEON scalers, years of top-notch example code, and patience in the face of my endless questions.

Check out eggs' releases (includes source code): 

  RG35XX https://www.dropbox.com/sh/3av70t99ffdgzk1/AAAKPQ4y0kBtTsO3e_Xlrhqha
  Miyoo Mini https://www.dropbox.com/sh/hqcsr1h1d7f8nr3/AABtSOygIX_e4mio3rkLetWTa
  Trimui Model S https://www.dropbox.com/sh/5e9xwvp672vt8cr/AAAkfmYQeqdAalPiTramOz9Ma

To neonloop, for putting together the original Trimui toolchain from which I learned everything I know about docker and buildroot and is the basis for every toolchain I've put together since, and for picoarch, the inspiration for minarch.

Check out neonloop's repos: 

  https://git.crowdedwood.com

To fewt and the entire JELOS community, for JELOS (without which MinUI would not exist on the RGB30) and for sharing their knowledge with this perpetual Linux kernel novice.

Check out JELOS:

  https://github.com/JustEnoughLinuxOS/distribution

To BlackSeraph, for introducing me to chroot.

Check out the GarlicOS repos:

	https://github.com/GarlicOS

And to Jim Gray, for commiserating during development, for early alpha testing, and for providing the soundtrack for much of MinUI's development.

Check out Jim's music: 

  https://ourghosts.bandcamp.com/music
  https://www.patreon.com/ourghosts/

----------------------------------------
