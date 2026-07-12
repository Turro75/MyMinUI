in this folder I put some commandline utilities written together with gemini which helped me a lot during the port of myminui to rg35xxsp.

testsdl2drivers.c is able to automatically test which audio/video driver are compiled in the surrent libsdl2 then it also check which one can be used.
```
root@ANBERNIC:~# /mnt/sdcard/.system/h700/bin/audioserver.elf &
[1] 8050
root@ANBERNIC:~# Audio server started

root@ANBERNIC:~# LD_LIBRARY_PATH=/mnt/sdcard/.system/h700/lib:$LD_LIBRARY_PATH SDL_PATH_DSP=/tmp/dsp LD_PRELOAD=/mnt/sdcard/.system/h700/lib/libredirect_dsp.so /mnt/sdcard/.system/h700/bin/testioctl.elf
=== TEST COMPATIBILITÀ DRIVER VIDEO ===
  [OK]  mali -> ATTIVABILE

=== TEST COMPATIBILITÀ DRIVER AUDIO ===
  [OK]  alsa -> ATTIVABILE
  [OK]  dsp -> ATTIVABILE



root@ANBERNIC:/mnt/sdcard/Bios/P8# LD_LIBRARY_PATH=/mnt/sdcard/.system/h700/lib:$LD_LIBRARY_PATH SDL_PATH_DSP=/tmp/dsp LD_PRELOAD=/mnt/sdcard/.system/h700/lib/libredirect_dsp.so /mnt/sdcard/.system/h700/bin/testioctl.elf
=== TEST COMPATIBILITÀ DRIVER VIDEO ===
  [ERR] directfb -> NON ATTIVABILE (directfb not available)
  [OK]  offscreen -> ATTIVABILE
  [ERR] dummy -> NON ATTIVABILE (dummy not available)
  [ERR] evdev -> NON ATTIVABILE (evdev not available)

=== TEST COMPATIBILITÀ DRIVER AUDIO ===
  [OK]  alsa -> ATTIVABILE
  [OK]  dsp -> ATTIVABILE


```


testcontrollersdl2.c detects the current gamecontroller guid, then it starts a timed sequence to let you pressing buttons, at the end of the process it geenrates the working gamecontrollerdb string.

```
root@ANBERNIC:/mnt/sdcard# LD_LIBRARY_PATH=/mnt/sdcard/.system/h700/lib:$LD_LIBRARY_PATH  /mnt/sdcard/testioctl.elf
=== INTERACTIVE CONTROLLER CALIBRATION TOOL ===
Waiting for physical hardware detection...

[SYSTEM] Joystick attached! Detected GUID: 19000000010000000100000000010000
Starting calibration process. You have 3 seconds to press each requested input.
-----------------------------------------------------------------

>>> PHASE 1: DIRECTIONAL D-PAD CALIBRATION <<<
Press now: D-PAD [ UP ] ... [OK] Hat index: h0, Value: 1
Press now: D-PAD [ DOWN ] ... [OK] Hat index: h0, Value: 4
Press now: D-PAD [ LEFT ] ... [OK] Hat index: h0, Value: 8
Press now: D-PAD [ RIGHT ] ... [OK] Hat index: h0, Value: 2

>>> PHASE 2: FACE BUTTONS & TRIGGERS CALIBRATION <<<
Press now: Button [ A ] ... [OK] Registered Hardware ID: b3
Press now: Button [ B ] ... [OK] Registered Hardware ID: b4
Press now: Button [ X ] ... [OK] Registered Hardware ID: b6
Press now: Button [ Y ] ... [OK] Registered Hardware ID: b5
Press now: Button [ SELECT ] ... [OK] Registered Hardware ID: b9
Press now: Button [ START ] ... [OK] Registered Hardware ID: b10
Press now: Bumper [ L1 ] ... [OK] Registered Hardware ID: b7
Press now: Bumper [ R1 ] ... [OK] Registered Hardware ID: b8
Press now: Trigger [ L2 ] ... [OK] Registered Hardware ID: b12
Press now: Trigger [ R2 ] ... [OK] Registered Hardware ID: b13
Press now: Button [ MENU (Center) ] ... [OK] Registered Hardware ID: b11

-----------------------------------------------------------------
=== CALIBRATION SUCCESSFUL! COPY THE FULL STRING BELOW ===

19000000010000000100000000010000,Anbernic RG35XXSP Mapped,platform:Linux,a:b3,b:b4,x:b6,y:b5,back:b9,start:b10,leftshoulder:b7,rightshoulder:b8,lefttrigger:b12,righttrigger:b13,guide:b11,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2
```