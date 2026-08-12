###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
	$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

ifeq (,$(CROSS_COMPILE))
	$(error missing CROSS_COMPILE for this toolchain)
endif

###########################################################

include ../../$(PLATFORM)/platform/makefile.env
SDL?=SDL

###########################################################

TARGET = minput
TARGET_MAP = minput_1
INCDIR = -I. -I../common/ -I../../$(PLATFORM)/platform/ $(MY_INCDIR)
SOURCE = $(TARGET).c ../common/utils.c ../common/api.c ../../$(PLATFORM)/platform/platform.c
SOURCE_MAP = $(TARGET_MAP).c ../common/utils.c ../common/api.c ../../$(PLATFORM)/platform/platform.c
INCDIR += -I../minarch/libretro-common/include/ 

CC = $(CROSS_COMPILE)gcc
CFLAGS   = $(MYARCH) -fomit-frame-pointer
CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -DUSE_$(SDL)  -Ofast $(MY_CFLAGS)
LDFLAGS	 = -ldl $(LIBS) -l$(SDL) -l$(SDL)_image -l$(SDL)_ttf -lpthread -lm -lz 
LDFLAGS +=  -lmsettings $(MY_LDFLAGS)

PRODUCT= build/$(PLATFORM)/$(TARGET).elf
PRODUCT_MAP= build/$(PLATFORM)/$(TARGET_MAP).elf

all: $(PREFIX)/include/msettings.h map_controller_sdl2.c check_controller_sdl2.c map_minput.c check_minput.c minput_1.c
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)
	$(CC) $(SOURCE_MAP) -o $(PRODUCT_MAP) $(CFLAGS) $(LDFLAGS)
	$(CC) $(CFLAGS) map_controller_sdl2.c -o build/$(PLATFORM)/map_controller_sdl2.elf $(LIBS) -lSDL2
	$(CC) $(CFLAGS) check_controller_sdl2.c -o build/$(PLATFORM)/check_controller_sdl2.elf $(LIBS) -lSDL2

clean:
	rm -f $(PRODUCT)
	rm -f build/$(PLATFORM)/map_controller_sdl2.elf
	rm -f build/$(PLATFORM)/check_controller_sdl2.elf
	rm -f build/$(PLATFORM)/map_minput.elf

$(PREFIX)/include/msettings.h:
	cd /root/workspace/$(PLATFORM)/libmsettings && make