# workspace (all)

###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

.PHONY: all


all:
		cd ./$(PLATFORM)/libmsettings && make
		cd ./$(PLATFORM) && make early # eg. other libs
		cd ./$(PLATFORM)/keymon && make
		cd ./$(PLATFORM)/cores && make
		cd ./$(PLATFORM) && make
		cd ./all/minui/ && make
		cd ./all/minarch/ && make
		cd ./all/clock/ && make
		cd ./all/minput/ && make
		cd ./all/clear_recent/ && make
		cd ./all/convertboxart/ && make
		cd ./all/Commander/ && make clean && make

clean:
	cd ./$(PLATFORM)/libmsettings && make clean
	cd ./$(PLATFORM)/keymon && make clean
	cd ./$(PLATFORM) && make clean
	cd ./$(PLATFORM)/cores && make clean
	cd ./all/minui/ && make clean
	cd ./all/minarch/ && make clean
	cd ./all/clock/ && make clean
	cd ./all/minput/ && make clean
	cd ./all/clear_recent/ && make clean
	cd ./all/convertboxart/ && make clean
	cd ./all/Commander/ && make clean

