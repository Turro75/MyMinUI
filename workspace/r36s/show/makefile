ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

TARGET = show
PRODUCT = $(TARGET).elf

CC = $(CROSS_COMPILE)gcc 
FLAGS	= -Os -lSDL -lSDL_image -lm -lrt -ldl -lz -Wl,--gc-sections 
FLAGSSDL2	= -DUSE_SDL2 -Os -lSDL2 -lSDL2_image -lm -lrt -ldl -lz -Wl,--gc-sections 
FLAGSFB = -Os -lm -lrt -ldl -lz -Wl,--gc-sections 

all:
	$(CC) $(TARGET).c SDL2_rotozoom.c  -o $(PRODUCT) $(FLAGSSDL2)
clean:
	rm -rf $(PRODUCT)