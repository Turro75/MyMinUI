#!/bin/sh

#EMU_EXE=prboom
#RUN=minarch
#RUN2=retroarch

#set cpu clock, minarch will overwrite this value with the one stored in default.cfg
#CPU_OC=${CPU_SPEED_MAX} # 1.8 GHz MM+ / 1.7GHz MM
#CPU_OC=${CPU_SPEED_PERF}  # 1.6 GHz
#CPU_OC=${CPU_SPEED_GAME}  # 1.3 GHz
#CPU_OC=${CPU_SPEED_POWERSAVE}  # 1.1 GHz
#CPU_OC=${CPU_SPEED_MENU}  # 0.5 GHz


#$1 is the rom filename
#$2 is the save state slot number
#$3 is the save state file
#$4 is 1 if SELECT is pressed then use RUN2 instead of RUN
#$RUN is the deafult libretro frontend for this core.pak
#$RUN2 is the alternative libretro frontend
#"$(dirname "$0")" is the current directory
#$EMU_EXE is the name of the core to be launched
#$CPU_OC is the desired cpu freq 
#launch_rom.sh  "$1" "$2" "$3" "$4" ${RUN} ${RUN2} "$(dirname "$0")" "$EMU_EXE" $CPU_OC
#overclock.elf ${CPU_SPEED_MENU}

#at first check if it must run the deafult frontend or the alternative
if [ ${4} -eq 0 ]; then
    THISRUN=${5}
else
    THISRUN=${6}
fi

export SDL_AUDIODRIVER=mmiyoo

#audioserver must be killed to let sdl2 taking control of audio 

if $IS_PLUS; then
    killall -9 audioserver
else
    killall -9 audioserver.mod
fi

EMU_EXE=${8}

if [ "$THISRUN" = "minarch" ]; then
    EMU_TAG=$(basename "${7}" .pak)
    ROM="${1}"
    mkdir -p "$BIOS_PATH/$EMU_TAG"
    mkdir -p "$SAVES_PATH/$EMU_TAG"
    HOME="$USERDATA_PATH"
    cd "$HOME"
    minarch.elf "$CORES_PATH/${EMU_EXE}_libretro.so" "$ROM" $2 > "$LOGS_PATH/$EMU_TAG.txt"
fi

if [ "$THISRUN" = "retroarch" ]; then 

    EMU_TAG=$(basename "${7}" .pak)
    ROM="$1"
    mkdir -p "$BIOS_PATH/$EMU_TAG"
    mkdir -p "$SAVES_PATH/$EMU_TAG"
    mkdir -p "$SAVES_PATH/$EMU_TAG/States"
    mkdir -p "${USERDATA_PATH}/${EMU_TAG}-${EMU_EXE}"
    mkdir -p "${SHARED_USERDATA_PATH}/${EMU_TAG}-${EMU_EXE}"

    THIS_CORE_PATH="${7}"

    #create custom cfg for retroarch
    if [ ! -f "${THIS_CORE_PATH}/specialcfg.cfg" ]; then
        echo "system_directory = \"${BIOS_PATH}/$EMU_TAG\"" > "${THIS_CORE_PATH}/specialcfg.cfg"
        echo "rgui_config_directory = \"${SHARED_USERDATA_PATH}/${EMU_TAG}-${EMU_EXE}\"" >> "${THIS_CORE_PATH}/specialcfg.cfg"
        echo "cheat_database_path = \"${BIOS_PATH}/$EMU_TAG/cheats\"" >> "${THIS_CORE_PATH}/specialcfg.cfg"
        echo "input_remapping_directory = \"${BIOS_PATH}/$EMU_TAG/config/remaps\"" >> "${THIS_CORE_PATH}/specialcfg.cfg"
        echo "recording_output_directory = \"${SAVES_PATH}/$EMU_TAG\"" >> "${THIS_CORE_PATH}/specialcfg.cfg"
        echo "savefile_directory = \"${SAVES_PATH}/${EMU_TAG}\"" >> "${THIS_CORE_PATH}/specialcfg.cfg"
        echo "savestate_directory = \"${SAVES_PATH}/${EMU_TAG}/States\"" >> "${THIS_CORE_PATH}/specialcfg.cfg"
        echo "screenshot_directory = \"${SAVES_PATH}/${EMU_TAG}\"" >> "${THIS_CORE_PATH}/specialcfg.cfg"
        echo "system_directory = \"${BIOS_PATH}/${EMU_TAG}\"" >> "${THIS_CORE_PATH}/specialcfg.cfg"
    fi

    #echo "\"$2\" ---- \"$3\"" > ${THIS_CORE_PATH}/cmdline.txt

    LOADSLOT=" "
    #check if it is requested to load a slot
    if [ $2 != "-1" ]; then
        #there is a request to load a state, for retroarch it is better that minui copy the requested state file to a fixed slot (i.e. slot 10)
        #otherwise I have to retrieve the rom name from the $ROM parameter within this script, rename it by adding .entry suffix then load the slot 10 with the -e parameter on command line below.
        #may be useful that minui also gives the name of state file so it is easier to rename it then delete it when retroarch ends, minarch will ignore it as it is not requested.
        #ok let's say we have also the name opf the state file available as $3:
        #echo "sono nella if" >> ${THIS_CORE_PATH}/cmdline.txt
        cp "$3" "${3}.entry"
        LOADSLOT=" -e${2} " 
    fi

    RA_HOME="${BIOS_PATH}/RETROARCH"
    cd "$RA_HOME"

    overclock.elf ${9}
    nice -20 retroarch.elf -v --config "${BIOS_PATH}/RETROARCH/${PLATFORM}/retroarch.cfg" --appendconfig "${THIS_CORE_PATH}/specialcfg.cfg" -L "${CORES_PATH}/${EMU_EXE}_libretro.so" "$ROM" $LOADSLOT > "$LOGS_PATH/$EMU_TAG.txt"
    if [ $2 != "-1" ]; then
        rm -f "${3}.entry"
    fi
fi

