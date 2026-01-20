# MinUI

# NOTE: this runs on the host system (eg. macOS) not in a docker image
# it has to, otherwise we'd be running a docker in a docker and oof

ifeq (,$(PLATFORMS))
#PLATFORMS = tg5040 rgb30 miyoomini trimuismart m17 rg35xx rg35xxplus gkdpixel m21
PLATFORMS = rg35xx miyoomini r36s my282 m21
endif

###########################################################

BUILD_HASH:=$(shell git rev-parse --short HEAD)
RELEASE_TIME:=$(shell TZ=GMT date +%Y%m%d)
RELEASE_BETA=b
RELEASE_BASE=MyMinUI-$(RELEASE_TIME)$(RELEASE_BETA)
RELEASE_DOT:=$(shell find ./releases/. | grep -e ".*/${RELEASE_BASE}-[0-9]+-base\.zip" | wc -l | sed 's/ //g')

RELEASE_NAME=$(RELEASE_BASE)-$(RELEASE_DOT)


###########################################################
.PHONY: build

export MAKEFLAGS=--no-print-directory

all: setup $(PLATFORMS) done
	
shell:
	make -f makefile.toolchain PLATFORM=$(PLATFORM)

name: 
	echo $(RELEASE_NAME)

build:
	# ----------------------------------------------------
	make build -f makefile.toolchain PLATFORM=$(PLATFORM)
	# ----------------------------------------------------

system:
	make -f ./workspace/$(PLATFORM)/platform/makefile.copy PLATFORM=$(PLATFORM)
	
	# populate system
	cp ./workspace/$(PLATFORM)/keymon/keymon.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/$(PLATFORM)/libmsettings/libmsettings.so ./build/SYSTEM/$(PLATFORM)/lib
	cp ./workspace/all/minui/build/$(PLATFORM)/minui.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/minarch/build/$(PLATFORM)/minarch.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/clock/build/$(PLATFORM)/clock.elf ./build/EXTRAS/Tools/$(PLATFORM)/Clock.pak/
	cp ./workspace/all/minput/build/$(PLATFORM)/minput.elf ./build/EXTRAS/Tools/$(PLATFORM)/Input.pak/
	cp ./workspace/all/clear_recent/build/$(PLATFORM)/clear_recent.elf "./build/EXTRAS/Tools/$(PLATFORM)/Clear Recently Played.pak/"
	cp ./workspace/all/convertboxart/build/$(PLATFORM)/convertboxart.elf "./build/EXTRAS/Tools/$(PLATFORM)/Convert BoxArt.pak/"
	cp ./workspace/all/Commander/build/$(PLATFORM)/MyCommander.elf "./build/EXTRAS/Tools/$(PLATFORM)/Files.pak/"
	cp -r ./workspace/all/Commander/res "./build/EXTRAS/Tools/$(PLATFORM)/Files.pak/"

cores:
	cp ./workspace/$(PLATFORM)/cores/output/* ./build/SYSTEM/$(PLATFORM)/cores/
#    mv ./build/SYSTEM/$(PLATFORM)/cores/retroarch ./build/SYSTEM/$(PLATFORM)/bin/retroarch.elf
#	mv ./build/SYSTEM/$(PLATFORM)/cores/bmp2png ./build/SYSTEM/$(PLATFORM)/bin/bmp2png.elf

common: build system cores
	
clean:
	rm -rf ./build

setup:
	# ----------------------------------------------------
	# make sure we're running in an input device
	tty -s 
	
	# ready fresh build
	rm -rf ./build
	mkdir -p ./releases
	mkdir -p ./build/SYSTEM
	mkdir -p ./build/EXTRAS/Emus
	mkdir -p ./build/EXTRAS/Tools
	mkdir -p ./build/PAYLOAD

	cp -R ./skeleton/BASE ./build/BASE
	cp -R ./skeleton/BOOT ./build/BOOT
	cp -R ./skeleton/SYSTEM/res ./build/SYSTEM/res
	cp -R ./skeleton/SYSTEM/$(PLATFORM) ./build/SYSTEM/$(PLATFORM)
	cp -R ./skeleton/EXTRAS/Bios ./build/EXTRAS/Bios
	cp -R ./skeleton/EXTRAS/Roms ./build/EXTRAS/Roms
	cp -R ./skeleton/EXTRAS/Cheats ./build/EXTRAS/Cheats
#	cp -R ./skeleton/EXTRAS/Saves ./build/EXTRAS/Saves
	cp -R ./skeleton/EXTRAS/Emus/$(PLATFORM) ./build/EXTRAS/Emus/$(PLATFORM)
	cp -R ./skeleton/EXTRAS/Tools/$(PLATFORM) ./build/EXTRAS/Tools/$(PLATFORM)
	
	# remove authoring detritus
	cd ./build && find . -type f -name '.keep' -delete
	cd ./build && find . -type f -name '*.meta' -delete
	echo $(BUILD_HASH) > ./workspace/hash.txt
	
	# copy readmes to workspace so we can use Linux fmt instead of host's
	mkdir -p ./workspace/readmes
	cp ./skeleton/BASE/README.txt ./workspace/readmes/BASE-in.txt
#	cp ./skeleton/EXTRAS/README.txt ./workspace/readmes/EXTRAS-in.txt
	
done:
	command -v say >/dev/null 2>&1 && say "done" || echo "done"

special:
	# ----------------------------------------------------
	# setup miyoomini/trimui family .tmp_update in BOOT
	mv ./build/BOOT/common ./build/BOOT/.tmp_update
	mv ./build/BOOT/miyoo ./build/BASE/
#	mv ./build/BOOT/trimui ./build/BASE/
	cp -R ./build/BOOT/.tmp_update ./build/BASE/miyoo/app/
#	cp -R ./build/BOOT/.tmp_update ./build/BASE/trimui/app/
	cp -R ./build/BASE/miyoo ./build/BASE/miyoo354
	cp -R ./build/BASE/miyoo ./build/BASE/miyoo285

specialpackage: tidy

# ----------------------------------------------------
	# zip up build
		
	# move formatted readmes from workspace to build
	cp ./workspace/readmes/BASE-out.txt ./build/BASE/README.txt
	# cp ./workspace/readmes/EXTRAS-out.txt ./build/EXTRAS/README_EXTRAS.txt
	rm -rf ./workspace/readmes
	
	cd ./build/SYSTEM && echo "$(RELEASE_NAME)\n$(BUILD_HASH)" > version.txt
	./commits.sh > ./build/SYSTEM/commits.txt
	cd ./build && find . -type f -name '.DS_Store' -delete
	
	mv ./build/SYSTEM ./build/PAYLOAD/.system
	cp -R ./build/BOOT/.tmp_update ./build/PAYLOAD/
	cd ./build/PAYLOAD && zip -r ../BASE/trimui.zip .tmp_update
	
	cd ./build/PAYLOAD && zip -r MinUI.zip .system .tmp_update
	mv ./build/PAYLOAD/MinUI.zip ./build/BASE
	
	# TODO: can I just add everything in BASE to zip?
#	cd ./build/BASE && zip -r ../../releases/$(RELEASE_NAME)-base.zip Bios Roms Saves miyoo miyoo354 trimui rg35xx rg35xxplus gkdpixel em_ui.sh MinUI.zip README.txt
#	cd ./build/EXTRAS && zip -r ../../releases/$(RELEASE_NAME)-extras.zip Bios Emus Roms Saves Tools README.txt
	
	
	rm -fr ./build/FULL
	mkdir ./build/FULL
	cp -fR ./build/BASE/* ./build/FULL/
	cp -fR ./build/EXTRAS/* ./build/FULL/
	rm -rf ./build/BASE
	rm -rf ./build/EXTRAS
	rm -rf ./build/PAYLOAD
	rm -rf ./build/BOOT
	rm -rf ./releases/$(RELEASE_NAME)-$(PLATFORM).zip
	cd ./build/FULL && zip -r ../../releases/$(RELEASE_NAME)-$(PLATFORM).zip Bios Emus Roms Cheats Saves Tools miyoo miyoo354 miyoo285 trimui rg35xxplus r36s m21 gkdpixel em_ui.sh MinUI.zip README.txt
	
	
	echo "$(RELEASE_NAME)" > ./build/latest.txt
	


tidy:
	# ----------------------------------------------------
	# remove systems we're not ready to support yet
	
	# TODO: tmp, figure out a cleaner way to do this
	rm -rf ./build/SYSTEM/trimui
	rm -rf ./build/EXTRAS/Tools/trimui

package: tidy
	# ----------------------------------------------------
	# zip up build
		
	# move formatted readmes from workspace to build
	cp ./workspace/readmes/BASE-out.txt ./build/BASE/README.txt
	# cp ./workspace/readmes/EXTRAS-out.txt ./build/EXTRAS/README_EXTRAS.txt
	rm -rf ./workspace/readmes
	
	cd ./build/SYSTEM && echo "$(RELEASE_NAME)\n$(BUILD_HASH)" > version.txt
	./commits.sh > ./build/SYSTEM/commits.txt
	cd ./build && find . -type f -name '.DS_Store' -delete
	
	mv ./build/SYSTEM ./build/PAYLOAD/.system

	
	cd ./build/PAYLOAD && zip -r MinUI.zip .system 
	mv ./build/PAYLOAD/MinUI.zip ./build/BASE
	
	# TODO: can I just add everything in BASE to zip?
#	cd ./build/BASE && zip -r ../../releases/$(RELEASE_NAME)-base.zip Bios Roms Saves miyoo miyoo354 trimui rg35xx rg35xxplus gkdpixel em_ui.sh MinUI.zip README.txt
#	cd ./build/EXTRAS && zip -r ../../releases/$(RELEASE_NAME)-extras.zip Bios Emus Roms Saves Tools README.txt
	
	
	rm -fr ./build/FULL
	mkdir ./build/FULL
	cp -fR ./build/BASE/* ./build/FULL/
	cp -fR ./build/EXTRAS/* ./build/FULL/
	rm -rf ./build/BASE
	rm -rf ./build/EXTRAS
	rm -rf ./build/PAYLOAD
	rm -rf ./build/BOOT
	rm -rf ./releases/$(RELEASE_NAME)-$(PLATFORM).zip
	cd ./build/FULL && zip -r ../../releases/$(RELEASE_NAME)-$(PLATFORM).zip Bios Emus Roms Saves Cheats Tools miyoo miyoo354 m21 r36s rg35xx MinUI.zip README.txt
	
	
	echo "$(RELEASE_NAME)" > ./build/latest.txt
	

###########################################################

# TODO: make this a template like the cores makefile?

rg35xx:
	# ----------------------------------------------------
	make clean setup common package PLATFORM=$@
	# ----------------------------------------------------

rg35xxplus:
	# ----------------------------------------------------
	make clean setup common special package PLATFORM=$@
	# ----------------------------------------------------

miyoomini:
	# ----------------------------------------------------
	make clean setup common special specialpackage PLATFORM=$@
	# ----------------------------------------------------


my282:
	# ----------------------------------------------------
	make clean setup common special specialpackage PLATFORM=$@
	# ----------------------------------------------------


m21:
	# ----------------------------------------------------
	make clean setup common package PLATFORM=$@
	# ----------------------------------------------------


r36s:
	# ----------------------------------------------------
	make clean setup build system cores  package PLATFORM=$@
	# ----------------------------------------------------

trimuismart:
	# ----------------------------------------------------
	make clean setup common special package PLATFORM=$@
	# ----------------------------------------------------

trimui:
	# ----------------------------------------------------
	make clean setup common special package PLATFORM=$@
	# ----------------------------------------------------

rgb30:
	# ----------------------------------------------------
	make clean setup common special package PLATFORM=$@
	# ----------------------------------------------------

tg5040:
	# ----------------------------------------------------
	make clean setup common special package PLATFORM=$@
	# ----------------------------------------------------

m17:
	# ----------------------------------------------------
	make clean setup common special package PLATFORM=$@
	# ----------------------------------------------------

gkdpixel:
	# ----------------------------------------------------
	make clean setup common special package PLATFORM=$@
	# ----------------------------------------------------

