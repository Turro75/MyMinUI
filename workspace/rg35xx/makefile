#rg35xx

###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_SDL2=other/sdl2
#REQUIRES_COMMANDER=other/DinguxCommander

all: readmes
	cd ./overclock && make
	cd ./boot && ./build.sh
	cd $(REQUIRES_SDL2) && ./autogen.sh && CFLAGS="-O2 $(MY_CFLAGS) -ffast-math -ftree-vectorize" \
		./configure CC=${CROSS_COMPILE}gcc --host=arm-buildroot-linux-gnueabihf \
		  --enable-arm-neon \
		  --enable-oss \
		  --enable-alsa \
		  --enable-video-directfb \
		  --disable-rpath \
		  --disable-arts \
		  --disable-esd \
		  --disable-dbus \
		  --disable-diskaudio \
		  --disable-dummyaudio \
		  --disable-pulseaudio \
		  --disable-video-kmsdrm \
		  --disable-video-vivante \
		  --disable-video-cocoa \
		  --disable-video-metal \
		  --disable-video-dummy \
		  --disable-video-offscreen \
		  --disable-video-x11 \
		  --disable-video-vulkan \
		  --disable-ime \
		  --disable-ibus \
		  --disable-fcitx \
		  --disable-directx \
		  --disable-xinput \
		  --disable-wasapi \
		  --enable-joystick-virtual \
		  --disable-libsamplerate && make -j4 V=1
	cd $(REQUIRES_SDL2) && cp ./build/.libs/libSDL2-2.0.so.0 ../libs/libSDL2-2.0.so.0


#early: $(REQUIRES_COMMANDER)
#	cd other/DinguxCommander && make -j
early: $(REQUIRES_SDL2)
	# early
		mkdir -p other/libs

clean:
	cd ./overclock && make clean
	cd ./boot && rm -rf ./output/*
#	cd other/DinguxCommander && make clean
	
#$(REQUIRES_COMMANDER):
#	git clone --depth 1 --branch minui-rg35xx https://github.com/shauninman/DinguxCommander.git $(REQUIRES_COMMANDER)

$(REQUIRES_SDL2):
	git clone https://github.com/libsdl-org/SDL.git -b release-2.32.x $(REQUIRES_SDL2)
	cd $(REQUIRES_SDL2) && patch -p1 < ../../sdl2_patch_controller.patch

include ../all/readmes/makefile
