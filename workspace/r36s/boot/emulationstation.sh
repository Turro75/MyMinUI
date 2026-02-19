#!/bin/bash

export SDL_ASSERT="always_ignore"


if [[ -e "/dev/input/by-path/platform-fe5b0000.i2c-event" ]]; then
  event_num="4"
else
  event_num="2"
fi
event_type="EV_KEY"
event_btn="BTN_SOUTH"
if [[ -e "/dev/input/by-path/platform-ff300000.usb-usb-0:1.2:1.0-event-joystick" ]]; then
  param_device="anbernic"
  if [[ -e "/dev/input/by-path/platform-fe5b0000.i2c-event" ]]; then
  event_num="4"
else
  event_num="2"
fi
  event_btn="BTN_EAST"
elif [[ -e "/dev/input/by-path/platform-odroidgo2-joypad-event-joystick" ]]; then
    if [[ ! -z $(cat /etc/emulationstation/es_input.cfg | grep "190000004b4800000010000001010000") ]]; then
      param_device="oga"
        else
          param_device="rk2020"
        fi
elif [[ -e "/dev/input/by-path/platform-odroidgo3-joypad-event-joystick" ]]; then
  param_device="ogs"
elif [[ -e "/dev/input/by-path/platform-singleadc-joypad-event-joystick" ]]; then
  param_device="rg552"
else
  param_device="chi"
fi

sudo chmod 666 /dev/tty1
export TERM=linux
export XDG_RUNTIME_DIR=/run/user/$UID/


isitrgui=$(cat ~/.config/retroarch/retroarch.cfg | grep 'menu_driver = "rgui"')

evtest --query /dev/input/event$event_num $event_type $event_btn
if [ "$?" -eq "10" ]; then
  #sudo systemctl start plymouth-quit
  sudo /bin/plymouth hide-splash
  printf "\033c" > /dev/tty1
  sudo setfont /usr/share/consolefonts/Lat7-Terminus20x10.psf.gz
  cd /usr/bin/emulationstation
  sudo ./boot_controls none $param_device &
  while true; do

          selection=(dialog \
          --backtitle "Boot and Recovery Tools" \
          --title "BaRT" \
          --no-collapse \
          --clear \
          --cancel-label "You must select one" \
          --menu "Distro: $(cat /usr/share/plymouth/themes/text.plymouth | grep ArkOS | cut -c 7-50)        Batt: $(cat /sys/class/power_supply/battery/capacity)%" 14 60 10)

          options=(
                  "1)" "Emulationstation"
                  "2)" "Retroarch"
                  "3)" "Wifi"
                  "4)" "Enable Remote Services"
                  "5)" "351Files"
                  "6)" "Backup ArkOS Settings"
                  "7)" "Restore ArkOS Settings"
                  "8)" "Reboot"
                  "9)" "Power Off"
          )

          choices=$("${selection[@]}" "${options[@]}" 2>&1 > /dev/tty1)

          for choice in $choices; do
                  case $choice in
                          "1)") sudo kill -9 $(pidof boot_controls)
                                printf "\033c" > /dev/tty1
                                sudo systemctl restart oga_events &
                                /usr/bin/emulationstation/emulationstation.sh
                                exit
                                ;;
                          "2)") if [ ! -z "$isitrgui" ]; then
                                  if [ ! -f "~/.config/.setxmb" ]; then
                                    sed -i '/menu_driver = \"rgui\"/s//menu_driver = \"xmb\"/g' /home/ark/.config/retroarch/retroarch.cfg
                                    sed -i '/xmb_menu_color_theme = \"4\"/s//xmb_menu_color_theme = \"8\"/g' /home/ark/.config/retroarch/retroarch.cfg
                                    touch ~/.config/.setxmb
                                  fi
                                fi
                                sudo kill -9 $(pidof boot_controls)
                                sudo cp -f /usr/bin/emulationstation/emulationstation.sh.ra /usr/bin/emulationstation/emulationstation.sh
                                sed -i '/input_exit_emulator_btn/s//;input_exit_emulator_btn/g' /home/ark/.config/retroarch/retroarch.cfg
                                printf "\033c" > /dev/tty1
                                sudo systemctl restart oga_events &
                                /usr/bin/emulationstation/emulationstation.sh
                                exit
                                ;;
                          "3)") /opt/system/Wifi.sh
                                ;;
                          "4)") /opt/system/Enable\ Remote\ Services.sh 2>&1 > /dev/tty1
                                ;;
                          "5)") /opt/system/351Files.sh 2>&1 > /dev/tty1
                                ;;
                          "6)") sudo kill -9 $(pidof boot_controls)
                                /opt/system/Advanced/"Backup ArkOS Settings.sh" 2>&1 > /dev/tty1
                                sudo ./boot_controls none $param_device &
                                ;;
                          "7)") sudo kill -9 $(pidof boot_controls)
                                /opt/system/Advanced/"Restore ArkOS Settings.sh" 2>&1 > /dev/tty1
                                sudo ./boot_controls none $param_device &
                                ;;
                          "8)") sudo reboot
                                ;;
                          "9)") sudo shutdown now
                                ;;
                  esac
          done
  done
else
  if [ -e /roms/MyMinUI/r36s/r36s.sh ]; then
    sudo /roms/MyMinUI/r36s/r36s.sh
    systemctl poweroff
  else
  esdir="$(dirname $0)"
  while true; do
      rm -f /tmp/es-restart /tmp/es-sysrestart /tmp/es-shutdown
      "$esdir/emulationstation" "$@"
      ret=$?
      [ -f /tmp/es-restart ] && continue
      if [ -e /tmp/es-sysrestart ]; then
          rm -f /tmp/es-sysrestart
          systemctl reboot
          break
      fi
      if [ -e /tmp/es-shutdown ]; then
          rm -f /tmp/es-shutdown
          systemctl poweroff
          break
      fi
      break
  done
  exit $ret
  fi
fi
