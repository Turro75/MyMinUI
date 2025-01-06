# MyMinUI

MyMinUI is a fork of the latest MinUI, I like MinUI but I also like playing old arcade coin up (thanks to FinUI) and DOOM which were missing so I added them. 
A missing feature when using MinUI with arcades is that while in the official MinUI's cores usually the name of the rom is enough to identify a game with arcade the rom naming is quit difficult to decode so boxarts are nearly mandatory to properly identifying a game, that's why I spent a lot of time adding boxarts.
I'm a player that uses a lot savestates, I really can't understand why almost all firmwares available in the retrogaming do not provide a way to select a specific slot with a graphical preview, when I saw for the first time the in game menu of MinUI I immediately felt that that was the way, then I added the minarch code to minui.   

You can find the latest release here: https://github.com/Turro75/MyMinUI/releases

## Features from FinUI:
- Add Favorites Collections (Press SELECT to toggle a rom)
- Clear "Recently Played"
- Additional emulators (MAME2003-PLUS)
- Base and Extras are merged into one Full release

# New features of MyMinUI:

## Release 06/01/2025

# All: 

- restored all currently supported devices Miyoo mini (plus), Miyoo A30, Rg35XX OG and GameConsole R36S. Since my SJGAM M21 is broken no more development on this device.

- new non gpu accelerated rendering engine ported to all devices, this engine allows a manual control of the whole render process allowing me splitting it to different threads and force them running on specific cores maximizing the performances.
  now the main thread (input + audio + menu) is always running on core 0, the core thread (frame generation at 60fps) is always running on core 1 and the rendering thread (game rotation, image scaling, apply effect, drawing to the display) is running on core 0 (if dualcore) or core 2 (if quadcore).  

- every platform has now the line and grid effect available. Both have an impact of roughly 20% of cpu load on rendering thread when activated 

- The aspect resize is now respectful of the native screen resolution, there are some devices wich would take benefit of the hardware scaling provided by the framebuffer kernel driver (such as the rg35xx) but the lost of performances due to the use of the old FullScreen1X scaler is compensated by the thread core engine. Native is integer scale, aspect expand the image to fit the screen preserving core provided aspect ratio, and fullscreen expand in both axes to fill the screen.
The new resize approach make easy adding new scaling options for future releases.

- Reviewed the thread video engine in minarch so all cores are now running in thread mode, non thread mode is hidden.

- Removed retroarch, since minarch is now able to do all of the needed things there are no more reason to keep retroarch.

- Fixed Fast forward which never worked in thread video mode, now it works with thread mode activated. I don't use it, during some test I got a steady 4x on doom, gba, nes and almost 2x in snes, neogeo.

- Debug HUD has now the same font size regardless the current image scale, since all are screens are now 640x480 the bottom right corner shows the current effect (0=none, 1=line, 2=grid) and the current game rotation. 

- All input handling are now manually and no more related to libsdl.

- a couple of new options in controls menu, now the user can set the abxy buttons to act as analog rigth stick (some games request it) and also making left analog stick acting as dpad.

- In the main menu it is now hidden the folder for hiddenroms, to make it visible create a file named .userdata/shared/enable-show-hidden-folder


# Miyoomini: 

- Ported to libSDL2, at the moment I'm using the libraries of pico8 provided by onionos, this is a temporarily solution while I understand how to make the toolchain generated libsdl2 working as expected (the audio forces everything running at half speed).

- To run native pico8 no need to add anything (except lexaloffle binaries in the bios folder of course) in "Tools/pico8 splore.pak" as all needed libraries are already in the .system/miyoomini/lib folder.

- the published toolchain does not reflect the one actually used for this release, once fixed the libSDL2 point I'll update the toolchain.

- Changed ps1 core to have the same as other platforms


# Miyoo A30: 

- Changed the install/update process.

- Changed ps1 core to have the same as other platforms.

## Release 15/12/2024

All: Regarding the support for non US chars (ZH, JP, etc...), it is enough replacing the system ttf font with one that includes the required charset. Added some notes in the readme.txt file as a quick howto.

SJGAM M21: I broke mine. No more releases on this. End of the story.

Closed one door opened another one: added support for the r36s running ArkOS.

This is an experimental release that provide a release file for the new part of the MyMinUI family, the GameConsole R36S.
This device is a reworked clone of the OdroidGoAdvance, which is also the base of the RG351 family.
Even though I always used Amberelec on my rg351p, I chose ArkOS as base system as ArkOS is much more flexible and don't need to build anything.

So in fact MyMinUI is an alternative launcher used in place of EmulationStation, everything under the hood is maintained so everyting that runs in ArkOS can potentially runs also in MyMinUI.

It is requested to have a genuine r36s running the latest r36s ArkOS image available here: https://aeolusux.github.io/ArkOS-R3XS/ probably it would be run also on other releases. I can't test others as I only have a v4 panel here.
Please carefully read the install.md in this repo or the Readme.txt file provided in the release file as the installation process requires specific steps to follow.
I created a tool named EnableMyMinUI (available in ArkOS Options/Tools) and a tool to DisableMyMinUI (available in MyMinUI/Tools) to switch to/from ArkOS/MyMinUI.
Maybe in a future release I'll edit BaRT mode to include MyMinUI but not for now.

Some additional notes:
Native Pico8 need pico8_64 and pico8.dat to run, copy them to the Bios/P8 folder.
I wanted to keep the ability to share the second sdcard across the other MyMinUi devices so I built the cores (32bits).
retroarch is ignored for now.
I've added a specific pak for NDS, PSP, N64 and Dreamcast that run the standalone emulator available in ArkOS. Those are also a good example for everyone who want extending the running systems, as said above everything running in ArkOS run in MyMinUI.
The PWR button is monitored by ArkOS so I decided to ignore it, to shutdown the handheld hold the menu button for at least 2 seconds, at release it will shutdown. 

For this I had to create a new (not accelerated) video driver, as the SDL2 rendering engine used in MinUI does not perform very well on many arcades and in some demanding PS1 games.
Taking profit of the experience done during the port to the SJGAM M21 I applied a similar solution that allows full speed in most systems/games tested (i.e. PS1 BR2 runs easily at 60fps).


## Release 01/12/2024
ALL: Added DosBox-pure, TyrQuake, Atari 2600 cores

Minarch: Removed Fullscreen1X, replaced by a more generic setting where it can be selected the maximum allowed upscale regardless the aspectratio, super useful for performances tuning.

M21: Add core auto screen rotation feature to minarch (only m21 for now) for vertical arcade games (i.e. 1943, karate champ, etc...)

Miyoo mini: revert lumon.elf to fix a drastic issue reported from user on github, thanks for the fix!


## Release 10/11/2024
ALL: 
- changed Pico8 native scripts so from now the pico8_dyn and pico8.dat must be copied under Bios/P8, in this way in case of shared sdcard You need to copy the files just once.

ALL:
- patched libretro fbneo core to back in the menu in case of load game failure, standard behavior is stucking on a screen (not well readable because in 32bit pixel format) and it was forced to reboot the device, now the error is handled and, in case, just read the log file, somewhere at the end of the file it is reported the error occoured, most of the time are missing file/bios in the romset.
- Have a look here: https://raw.githubusercontent.com/libretro/FBNeo/master/gamelist.txt to see all supported roms and related dependencies.

ALL (Multidisc games on Minarch PBP file):
- Enabled multidisc pbp files handling, auto reset core in case of manual disc change, auto load the disc requested by the state loaded.
It is strongly suggested to convert m3u/cue/bin folders for multidisc games to a single pbp file, I suggest psxpackager (https://github.com/RupertAvery/PSXPackager/releases/tag/v1.6.3) to do that. For apple silicon users use the intel version as the arm64 version is not working.
- m3u/cue/bin folders are still working but there are a couple of issues related to boxart and state preview in the minui menu. Everything work in the in game menu. This need much work to be fixed, I won't as I personally use only pbp files but patches are welcome.

MIYOOMINI(+):
- removed the audioserver.txt error file report in the sdcard root, I did to debug pico8 sound now useless as the bug has been fixed months ago.

SJGAM M21:
- Enabled the external controller (SJGAM proprietary only), only one external controller can be used at the moment as minarch does not support more than 1 player. 
- Enabled HDMI output including audio to HDMI, at the moment it stretches the image fullscreen, so in case of 16:9 monitors is not the best, I'll have a look soon to keep a better aspect ratio.
- Fixed audio not muted at minimum volume level.
- Swapped face buttons layout from the silly ABXY (clockwise) on the handheld and BAXY in the external controllers to the more common anbernic/miyoo ABYX, I know it doesn't reflect the printed letter on the buttons but I prefer that way.

## Release 04/11/2024

MIYOO A30:
Releases for Miyoo A30 are temporary suspended as it needs (much) more work to get it working properly with the latest minarch, basically it must be removed the SDL2 engine in place of direct framebuffer engine as rg35xx,miyoomini and m21 do.

CORE_THREAD_VIDEO:
I reviewed the thread core engine as based on time profiling while trying to optimize the m21 I realized the existing thread implementation can be improved.
Since most of the time is spent for upscaling I decided to split the current core thread is 2 threads, now:

the core thread generates the frame and copy it to a buffer then return to generate the next frame (which will come after 16.6msec to get 60fps)

The render thread upscales according to the selected scaler (up to 12msecs) the frame then draws the buffer content to the screen (< 2msecs) so it is easier for a multicore soc get everything upscaled keeping 60fps.
This allows the m21 performing very well. BR2 on PS1 can run at 56fps a 1x scale, ok no full speed but definitely playable. it is expected that most games are now able to run at 60fps upscaled 2x or 3x. it is just a matter to find the balance between scaler and cpu speed.

DEBUG HUD:
The debug info now shows also the bpp value (16 or 32), the rendered fps/generated fps, the number of cpu and load on each cpu and T0/T1 to inform if thread_video is enabled or not.

SCAN and GRID effects:
taken from MinUI as is, I'm not a fan of these artifacts on a 3.5" but here You are if You like.

32BPP Pixel Format support:
Enabled also 32bpp pixel format to let more arcades games to run. It can enabled in the in menu frontend options but You need to restart the game.

PRBOOM (DOOM):
thanks to the minarch rework now prboom is able to run also on minarch.

RETROARCH:
minarch is becoming featured and powerful, I'm considering to drop retroarch as there are no more reasons to keep it.

SHARING SAVES:
I aligned the cores of each devices and now the saves (made by minarch) can be shared across devices in case You have a single sdcard for all devices.
Shutting down doing a quicksave and putting that sdcard in another device let You recover the game at boot, nice.

## Release 08/10/2024

# New Features:

- All: Added Atari 5200 and Atari 7800 libretro cores, add bios and rom files to the provided folders. The best site to get bios/roms compatibility info is onion specifically the emulators page.
- Added support to SJGAM M21, it took more time than expected to bring a custom os overhaul to this device.
  Installation instructions are written in the Install.md file of this repo.
  Starting from the bad news, the so called N909 cpu is actually an allwinner h133 soc which is a dual core a7 @ max1.2GHz without any gpu, so the claimed "up to PS1" was absolutely right. 
  Performances wise this device is similar to a MM+ without overclock.
  What is not working: NDS, Amiga, retroarch, external controllers, HDMI out.
  What is working: all standard MyMinUI (including native pico8) features except the above. Special care put on the power handling as it has only a physical power switch. 
            double press on menu button put the device in sleep mode (the same happen after 30secs of inactivity)
            single menu press wake up the device (this is the standard behavior of MinUI on similar devices such as m17 and trimui smart) Thanks Shaun!
            keeping pressed the menu button for more than 3secs causes a poweroff command at button release time. If during the menu pressed time another button is pressed (i.e. plus button to adjust brightness) nothing will happen at button release.
            After 2 minutes of sleep the device will auto shutdown.

            There is no rtc, anyway the system time is saved at every poweroff command and restored at boot exactly as miyoo mini does, just be aware that switching off through the physical button don't do that.

            The input event handler doesn't work in sdl/sdl2 event, I patched both libraries to get the proper key events. I suspect most of the issues seen in some YT videos are caused by that.

            Specific setting for performances:
            all cores with a native.txt file in the pak folder are forced 1x fullscreen regardless the frontend setting, this is requested by all PS1 and some fbneo games to run fullspeed.
            SNES is running slow (50fps) if scaling is set to aspect/fullscreen, full speed is acheved only with scaling set to native.
            In almost all cores the thread set to ON gives a better overall fps result. 
            Some heavy arcade games can run full speed in fbneo with frameskip 1 of 2. 

  Some technical info for curious on this device:

            The display don't accept other than RGB8888 pixel format, moreover the MSB Byte of each pixel must be 0xff to get the pixel visible. (Still under investigation)
            That made impossibile using sdl/sdl2 flip/render functions, since MyMinUI works only with RGB565 I had to work direclty on framebuffer and creating my own flip functions.

            The Joysticks are digital (not L3/R3 buttons) and are hardwired to buttons so there is no way to remap them. 

            The device has a read only stock firmware accessible as mtd, it is based on musl libc which don't allow native pico8 and probably nds able to run. 
            The stock provided "cores" are actually all standalone tiny libretro frontend with core integrated.

            I created 2 toolchains, the native one (musleabihf) and a standard glibc (gnueabihf), after several test I decided to compile everything with gnueabihf and running everything in a chroot environment (thankfully sjgam provided fuse support) as MinUI already does on RG35XX OG.

            This allowed me to run native pico_dyn which is not working on musl environment. For those interested in experimenting with musl toolchain I already patched sdl, sdl2, directfb to work, in case someone wants to create some paks based on stock cores.

            In future I would like playing with mtd partitions to 1) change bootlogo and 2) edit the content of the /Games folder which is a collection of nes games that are accessible in case sdcard is not detected at boot. Since there are no way to connect to it from remote I don't see the point to invest time on custom kernel or system image.

# Fixes:

- fixed a bug in audio callback (api.c) that prevented to properly run prboom core, now Doom can run directly in minarch at full speed (Yeah!!!).
- fixed a bug in controller libretro event that caused segfaults on puae2021 core.


## Release 29/08/2024

RG35XX OG: fix audio speed bug in native pico8
Changed key mapping on pico8 to make it easier to use (refer to pdf file in the repo)

Miyoo A30: add analog stick support (from MinUI)
Changed key mapping on native pico8 to make it easier to use (refer to pdf file in the repo)

All: changed a bit the install/update/boot process so it can be shared a single sdcard among all 4 supported devices. At the moment save states are shared, some are working (i.e. doom) some others don't, I'm planning to split saves per device but this will eventually happen on future releases.

All: merged several functions implemented by MinUI during the summer. In a future release I'll merge also scanline/grid.

Updated Install.md file to reflect the current status.

IMPORTANT NOTE:
before updating from a previous release delete from sdcard the following folders (if present):

.tmp_update
miyoo
miyoo354
rg35xx
trimui



## Release 27/06/2024

# New features:

- minarch: add thread video setting (taken as is from MinUI)
- MiyooMini(+): added lumon.cfg to make screen settings adjustable, values are read at boot.
- Added toggle tool to switch dpad left-right and L2-R2 behavior, if the toggle is active:
   Dpad Left and Right select saved states while L2-R2 seek pages otherwise
   Dpad Left and Right seek pages as standard minui, L2-R2 select saved states
- little change of button mapping.
   SELECT previously were used to toggle a game in the favorite list, now it acts as alternate function for other buttons:
   START -> toggle currently selected rom Favorite. / SELECT+START -> hide currently selected rom
   A -> RUN current rom with default libretro frontend / SELECT+A -> RUN2 current rom with alternative libretro frontend
   X -> RUN selected state of current rom with default libretro frontend / SELECT+X -> RUN2 selected state of current rom with
   alternative libretro frontend
- Reworked launch.sh under Emus folders.
    moved most part of the shell script under bin so all launch.sh under Emus can behave the same way and are easier to maintain.
    launch.sh on each Emus folder now allows selecting RUN and RUN2 libretro frontend so the user can select which libretro frontend is
    launched when pressing A (RUN) or SELECT+A (RUN2).
Thanks to this feature the user can quickly run a game with minarch or retroarch. Not 100% of the cases but many times a saved state
created under minarch can be launched in retroarch and viceversa. Not made extensive tests but for instance a save state of a gb
game with colorization active does not run in retroarch if the same colorization is not set and so on.
At the moment all cores are set as RUN=minarch and RUN2=retroarch, only DOOM has minarch and retroarch swapped.
All standalone emulator kept their own launch.sh, both RUN and RUN2 do the same in this case.

For those who want adding cores not supported by MyMinUI can now use the precompiled cores available here:
miyoomini(+) https://buildbot.libretro.com/nightly/dingux/miyoo-arm32/latest/
rg35xx OG https://buildbot.libretro.com/nightly/linux/armv7-neon-hf/latest/
in case minarch is not able to run the new core they can easily move to retroarch.

# Fixes:

- Fix for romfile with ' in the name previously not running
- Fix: rg35xx install/update process by a fixed /misc/dmenu.bin
- Fix: miyoomini rtc not updated at boot
- Fix: overclock max 1.7GHz on MM and 1.8GHz on MM+
- Fix: saved state files now include parenthesys in the name to copy what retroarch does.
Special note if previous saved states are now missing:
Previously save state removed parts of the filename enclosed by parenthesys, now those part are preserved as retroarch does.
i.e. Tekken 3 (USA).zip create save state file named Tekken 3 (USA).stateX and Tekken 3 (USA).stateX.png with previous releases the saved state file were named Tekken 3.stateX and Tekken 3.stateX.png simply rename old states to recover them.


## release 02/06/2024
- RG35XX: not my work but it turned out that the work done by XQuader releasing drastic 1.2.1 (standalone Nintendo DS emulator) for Minui for RG35XX OG is working even on MyMinUI for RG35XX OG.  here https://boosty.to/xquader/posts/b9bfd9b4-5a37-48a6-8bc7-3d8aa48a5953?share=post_link You can download the minui file, just unpack it in the Emus directory.
- Miyoomini: here https://github.com/steward-fu/nds/releases/tag/v1.8 You can download the miyoomini version of drastic (made by Steward for OnionOS), unpack it in the Emus/miyoomini/NDS.pak/drastic folder. I created a launch.sh ready to run it.
It looks like the Miyoomini version is more effective on controls side, I've not tested very well but it seems the pen is working here while on rg35xx don't. Maybe XQuader will share the source so someone can improve it.
- The Drastic work inspired me to solve the SDL2 compilation/setup in the rg35xx toolchain, as a result I found the way to run native pico8, ok no wifi so splore is half working but it works.
- RG35XX and Miyoomini: thanks to the pico-8 wrapper for onionos I reworked it a bit so I added support for native Pico8, just copy Your licensed copy of pico8_dyn and pico8.dat (get them from raspberry pi download pack) in the folder /mnt/sdcard/Tools/(rg35xx or miyoomini)/Pico-8 Splore.pak/pico8-native 
  For the miyoomini only You need to download the pico8 wrapper for onionos from here https://github.com/XK9274/pico-8-wrapper-miyoo/releases/tag/0.8.1 , just unpack it under /mnt/SDCARD/Tools/miyoomini/Pico-8 Splore.pak/pico8-native, 
  This run pico8 splore (offline) as a tool, it loads the carts available in the Roms/Pico-8 (P8) folder, if the carts are saved in p8.png extension splore is able to show a game preview
  I created also the  Roms/Pico-8 Native (P8N) where You can store carts and launching them individually.
  Carts stored in Roms/Pico-8 (P8) are still launched with libretro fake-08 core (supports most of the game and is able to handle save states) when navigating in the menu as before.
  Carts stored in Roms/Pico-8 Native (P8N) are launched with native pico8 launcher
- a lot of rework in the way Minarch/Minui handle saved state, now minarch acts as retroarch so since now minui is able to show and start saved state for cores running on retroarch.
 This feature should help many users that want to add more cores to their own installation:
 Many cores available in OnionOS (get them in the file retroarch.7z present in the download release of OnionOS) are working even in rg35xx. Unfortunately most of them aren't working with minarch (Shauninman already said that missing cores are missing for good reasons), now it is easier to create a pak (please read pak.md for details) which run the core with retroarch without loosing the save state preview selection feature.
 the folder DOOM.pak is a working example just replace prboom with the name of the test core and set the cpu overclock You need at the beginning of the file, that's it. Atari cores are working in this way.
 - For users who want to use old states, they must be moved and renamed, for instance nbajam running on finalburnneo states were previously stored:

  preview) in /mnt/sdcard/.userdata/shared/.minui/FBN/nbajam.zip.X.bmp  (where X is the slot number 0 to 7)
 
  state) in /mnt/sdcard/.userdata/shared/FBN-fbneo/nbajam.zip.stX (where X is the slot number 0 to 7)
 
  now the files are stored in 
  
  /mnt/sdcard/FBN/states/nbajam.stateX 
  and 
  /mnt/sdcard/FBN/states/nbajam.stateX.png (where X is the slot number 1 to 8)
  
  please note that the slot 0 should not be used anymore be aware of this on retroarch as You can still select it. Also note that the file extension has been removed from state file, this is to copy what retroarch does so it will be easier updating it in the future myminui releases.   

  - added the file _map.txt under arcade and finalburnneo roms folders, rename them as map.txt to get the real name of the arcade name in place of the rom file name, please note that this takes a lot of time every time You enter the folder in case of big collection of roms (> than 4secs for 1000 rom files) because it is not cached so it's your choice, I don't use it but many asked for it. It also breaks the states preview logic, I'll fix this in future releases. 
  it is a plain txt file in case You want to edit it. it is based on the arcade-rom-name.txt available in onionos. 


## release 01/05/2024
- improved update/install process which allows to copy directly the release file in the sdcard root, be aware that this overwrites all existing files (but it keeps bios, roms, saves, etc..)
  the original install/update process is still working (just in case You edited some launch.sh file).
- minarch : added the ability to make a boxart using the current screenshot, it overwrites the current boxart if exists. 
- added the bmp2png utility for future boxart related tools (not needed for devices wich use SDL2 such as rgb30)
- Added HiddenRoms Collection (Press START to hide/unhide a rom)
- added Retroarch 1.15 as alternative libretro frontend for cores it has the same video filters available on garlicos (WIP).
- added NeoGeo CD core

## release 14/04/2024
- added prboom libretro core (Doom), it works only with retroarch as libretro frontend
- added puae2021 libretro core (Amiga), not really tested, expect issues, please report
- added FinalBurnNeo libretro core, it works surprisingly well on some arcade game which are slow on other OS, use an FBNeo romset as many 0.78 roms (mame2003+) don't work well.
- added Overclock Max profile (1.5GHz on rg35xx and 1.8GHz on miyoominiplus)
- replaced dinguxcommander with the garlicos version (source rg35xx.com) which has more features
- the power button now performs a shutdown as garlicos 1.4.9 does instead of standby, to restore the MinUI sleep mode behavior just create the file enable-sleep-mode in the folder /mnt/sdcard/.userdata/shared (action available under Tools->ToggleSleepMode)
- to use retroarch instead of minarch just copy to the ROMS/Extras/Emus/xxxx.pak the launch.sh from ROMS/Extras/Emus/Doom.pak then set the right core name and the cpu speed required.
- added retroarch as Tools
- added the main menu mode selector to version page, press up/down to change the mode then exit the page to get the new mode activated
  Three modes are implemented:
  1) STANDARD  -> this is the standard MinUI interface
  2) SIMPLE -> same as STANDARD with Tools and Settings items hidden
  3) FANCY -> it changes the look of the main menu showing boxarts and save state window selector if present.
      Boxarts must be png 640x480 (same garlic os format) and saved in the "Imgs" folder on each Roms folder. it is suggested to leave the left side of the image dark for better user experience
      it is allowed adding boxarts also for systems not only roms, just add the image named as the folder.
        i.e. if the rom folder is "Doom (DOOM)" then create a png called "Doom (DOOM).png" in the "Doom (DOOM)/Imgs" folder
      If saves are present in the selected rom the same state selector available in the in game menu (IMHO that is a MinUI awesome feature) is shown, the latest save is automatically selected, press X to load it, if X is pressed while an empty slot is selected the game starts without recalling state. save state selector is available ONLY for cores running Minarch.
      In fancy mode You can read the current folder in the top left corner of the display, while scrolling the recent or favorite lists it shows the folder of the currently selected game. 
      Under Tools there are 2 toggles to hide saved states and hide the boxart if a savestate is present.


Some changes under the hood:
- reduced footprint of the docker toolchain from 4.5GB down to 1.5GB.
- various improvements on makefiles
- the release files are separated and dedicated for each platform (WIP)
- all cores moved to the same system directory as BASE cores (which should be better for retroarch)
- all core launchers moved to SDCARD/Emus folders as EXTRAS
- enhanced install/update process


Install process on RG35XX:
flash an sdcard with the TF1.img (please follow official MinUI instructions) then unzip the rg35xx release file in the root.
Don't forget to copy the file dmenu.bin in the misc directory.
Starting from release 01/05/2024 it is available an enhanced install script that allows the user to copy directly the release file in the sdcard root, everything is now automated.

Install process on Miyoo Mini Plus:
format an sdcard in Fat32 then unzip the content of the miyoo mini release file in the root of the sdcard.
Starting from release 01/05/2024 it is available an enhanced install script that allows the user to copy directly the release file in the sdcard root, everything is now automated.

Upgrade process (both devices):
Even if theorically updating an existing MinUI or FinUI would be possible it is recommended to install MyMinUI from scratch.
To update a previous MyMinUI sdcard just copy the file MinUI.zip file in the root of the sdcard then reboot the device.
It is usually not needed unzipping the whole release file as if not made carefully You would lose existing roms/bios/saves, etc... in doubt choose merge instead of replace folders.
Starting from release 01/05/2024## it is available an enhanced updatel script that allows the user to copy directly the release file in the sdcard root, everything is now automated.

# MinUI

MinUI is a focused, custom launcher and libretro frontend for the RGB30, M17 (early revs), Trimui Smart (and Pro), Miyoo Mini (and Plus), and Anbernic RG35XX (and Plus).

<img src="github/minui-main.png" width=320 /> <img src="github/minui-menu-gbc.png" width=320 />  
See [more screenshots](github/).

## Features

- Simple launcher, simple SD card
- No settings or configuration
- No boxart, themes, or distractions
- Automatically hides hidden files
  and extension and region/version 
  cruft in display names
- Consistent in-emulator menu with
  quick access to save states, disc
  changing, and emulator options
- Automatically sleeps after 30 seconds 
  or press POWER to sleep (and wake)
- Automatically powers off while asleep
  after two minutes or hold POWER for
  one second
- Automatically resumes right where
  you left off if powered off while
  in-game, manually or while asleep
- Resume from manually created, last 
  used save state by pressing X in 
  the launcher instead of A
- Streamlined emulator frontend 
  (minarch + libretro cores)
- Single SD card compatible with
  multiple devices from different
  manufacturers

You can [grab the latest version here](https://github.com/shauninman/MinUI/releases).

> Devices with a physical power switch
> use MENU to sleep and wake instead of
> POWER. Once asleep the device can safely
> be powered off manually with the switch.

## Supported consoles

Base:

- Game Boy
- Game Boy Color
- Game Boy Advance
- Nintendo Entertainment System
- Super Nintendo Entertainment System
- Sega Genesis
- PlayStation

Extras:

- Neo Geo Pocket (and Color)
- Pico-8
- Pokémon mini
- Sega Game Gear
- Sega Master System
- Super Game Boy
- TurboGrafx-16 (and TurboGrafx-CD)
- Virtual Boy


## Legacy versions

The original Trimui Model S version of MinUI has been archived [here](https://github.com/shauninman/MinUI-Legacy-Trimui-Model-S).

The sequel, MiniUI for the Miyoo Mini, has been archived [here](https://github.com/shauninman/MiniUI-Legacy-Miyoo-Mini).

The return of MinUI for the Anbernic RG35XX has been archived [here](https://github.com/shauninman/MinUI-Legacy-RG35XX).
