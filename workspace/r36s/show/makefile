ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

TARGET = show
PRODUCT = $(TARGET).elf

CC = $(CROSS_COMPILE)gcc 
include ../platform/makefile.env

INCDIR = -I. -I../../all/common/ -I../platform/ $(MY_INCDIR) -I../../all/minarch/libretro-common/include/
SOURCE = $(TARGET).c ../../all/common/utils.c ../../all/common/api.c ../platform/platform.c ../../all/common/$(SDL)_rotozoom.c
#SOURCE += ../../all/common/pixman-arm-neon-asm.S
INCDIR += -I../all/minarch/libretro-common/include/ 

CC = $(CROSS_COMPILE)gcc
CFLAGS   = $(MYARCH) -fomit-frame-pointer
CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -DUSE_$(SDL)  -Ofast $(MY_CFLAGS) -s
LDFLAGS	 = -ldl $(LIBS) -l$(SDL) -l$(SDL)_image -l$(SDL)_ttf -lpthread -lm -lz 
LDFLAGS +=  -lmsettings $(MY_LDFLAGS)

#PRODUCT= $(TARGET).elf

all: $(PREFIX)/include/msettings.h
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)
clean:
	rm -f $(PRODUCT)

$(PREFIX)/include/msettings.h:
	cd /root/workspace/$(PLATFORM)/libmsettings && make