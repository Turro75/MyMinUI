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
SDL?=SDL2

###########################################################

OBJDUMP = $(CROSS_COMPILE)objdump

TARGET = minarch
INCDIR = -I. -I./libretro-common/include/ -I../common/ -I../../$(PLATFORM)/platform/ $(MY_INCDIR)
SOURCE = $(TARGET).c ../common/scaler.c ../common/utils.c ../common/$(SDL)_rotozoom.c ../common/api.c ../../$(PLATFORM)/platform/platform.c 
SOURCE += ../common/pixman-arm-neon-asm.S


CC = $(CROSS_COMPILE)gcc
CFLAGS   = $(MYARCH) -fomit-frame-pointer -ftree-vectorize -funroll-loops -funsafe-math-optimizations
CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -DUSE_$(SDL) -O3 -std=gnu99
CFLAGS	+=  -flto -s $(MY_CFLAGS) 
LDFLAGS	 = -ldl $(LIBS) -lmsettings -l$(SDL) -l$(SDL)_image -l$(SDL)_ttf -lpthread -lm -lz $(MY_LDFLAGS)
# CFLAGS  += -Wall -Wno-unused-variable -Wno-unused-function -Wno-format-overflow
# CFLAGS  += -fsanitize=address -fno-common
# LDFLAGS += -lasan

BUILD_DATE!=date +%Y.%m.%d
BUILD_HASH!=cat ../../hash.txt
CFLAGS += -DBUILD_DATE=\"${BUILD_DATE}\" -DBUILD_HASH=\"${BUILD_HASH}\"

# CFLAGS += -DBUILD_DATE=\"20230326\" -DBUILD_HASH=\"d34db33f\"

PRODUCT= build/$(PLATFORM)/$(TARGET).elf

all: libretro-common $(PREFIX)/include/msettings.h
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE)  $(CFLAGS) $(LDFLAGS) -o $(PRODUCT)
#	$(OBJDUMP) -D $(PRODUCT) > $(PRODUCT).map
clean:
	rm -f $(PRODUCT)

libretro-common:
	git clone https://github.com/libretro/libretro-common
	
$(PREFIX)/include/msettings.h:
	cd /root/workspace/$(PLATFORM)/libmsettings && make