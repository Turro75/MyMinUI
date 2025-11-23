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

TARGET = convertboxart
INCDIR = -I. -I../common/ -I../../$(PLATFORM)/platform/ $(MY_INCDIR)
SOURCE =   $(TARGET).c ../common/utils.c ../common/api.c ../common/scaler.c ../common/$(SDL)_rotozoom.c ../../$(PLATFORM)/platform/platform.c
SOURCE += ../common/pixman-arm-neon-asm.S
INCDIR += -I../minarch/libretro-common/include/ 

CC = $(CROSS_COMPILE)gcc
CFLAGS   = $(MYARCH) -fomit-frame-pointer
CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -DUSE_$(SDL)  -O2 $(MY_CFLAGS)
LDFLAGS	 = -ldl $(LIBS) -l$(SDL) -l$(SDL)_image -l$(SDL)_ttf -lpthread -lm -lz -lpng
LDFLAGS +=  -lmsettings $(MY_LDFLAGS)

PRODUCT= build/$(PLATFORM)/$(TARGET).elf

all: $(PREFIX)/include/msettings.h
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)
clean:
	rm -f $(PRODUCT)

$(PREFIX)/include/msettings.h:
	cd /root/workspace/$(PLATFORM)/libmsettings && make