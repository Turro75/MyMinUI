#m21

###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_SDL2=other/sdl2

all: readmes
	cd show && make
	cd overclock && make
	cd audioserver && make
	cd $(REQUIRES_SDL2) && CFLAGS="-O2 -march=armv8-a+crc -mtune=cortex-a53 -mcpu=cortex-a53 -ffast-math -ftree-vectorize" \
		./configure CC=${CROSS_COMPILE}gcc --host=arm-buildroot-linux-gnueabihf \
		  --enable-arm-neon \
		  --enable-oss \
		  --enable-alsa \
		  --disable-video-directfb \
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
		  --disable-joystick-mfi \
		  --disable-directx \
		  --disable-xinput \
		  --disable-wasapi \
		  --disable-hidapi-joystick \
		  --disable-hidapi-libusb \
		  --enable-joystick-virtual \
		  --enable-video-mali \
		  --disable-libsamplerate && make -j4 V=99
	cd $(REQUIRES_SDL2) && cp ./build/.libs/libSDL2-2.0.so.0 ../libs/libSDL2-2.0.so.0
	cd $(REQUIRES_SDL2) && cp $(PREFIX)/lib/libSDL2_image-2.0.so.0 ../libs/libSDL2_image-2.0.so.0
	cd $(REQUIRES_SDL2) && cp $(PREFIX)/lib/libsamplerate.so.0 ../libs/libsamplerate.so.0


early: $(REQUIRES_SDL2)
	mkdir -p other
	mkdir -p other/libs
	
clean:
	cd show && make clean
	cd overclock && make clean
	cd audioserver && make clean
	cd sdl2 && make clean


$(REQUIRES_SDL2):
	git clone https://github.com/JohnnyonFlame/SDL-malifbdev-rot $(REQUIRES_SDL2)

	
include ../all/readmes/makefile