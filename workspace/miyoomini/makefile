###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_SDL1=other/sdl1
REQUIRES_SDL2=other/sdl2
REQUIRES_NDS=other/nds

all: readmes
	cd blank && make
	cd batmon && make
	cd lumon && make
	cd show && make
	cd overclock && make
#	cd $(REQUIRES_NDS) && make -f Makefile.miyoo_mini cfg
#	cd $(REQUIRES_NDS) && make -f Makefile.miyoo_mini all
	cd $(REQUIRES_NDS) && make -f Makefile.mini cfg
	cd $(REQUIRES_NDS) && make -f Makefile.mini all
	cd $(REQUIRES_SDL1) && ./make.sh
	cd $(REQUIRES_SDL2) && ./run.sh config
	cp -L ${PREFIX}/lib/libjson-c.so.2 $(REQUIRES_NDS)/drastic/lib/

early:  $(REQUIRES_NDS) $(REQUIRES_SDL2) $(REQUIRES_SDL1)
	cd other/latency_reduction && make
	

clean:
	cd blank && make clean
	cd batmon && make clean
	cd lumon && make clean
	cd show && make clean
	cd other/latency_reduction && make clean
#	cd $(REQUIRES_NDS) && make -f Makefile.miyoo_mini clean
	cd $(REQUIRES_NDS) && make -f Makefile.mini clean
	cd $(REQUIRES_SDL2) && make clean
	cd $(REQUIRES_SDL1) && make distclean

###########################################################

$(REQUIRES_SDL1):
	git clone --depth 1 --branch miniui-miyoomini https://github.com/shauninman/SDL-1.2.git $(REQUIRES_SDL1)

$(REQUIRES_SDL2):
	git clone --depth 1 --branch pico8 https://github.com/XK9274/sdl2_miyoo.git $(REQUIRES_SDL2)
	cp other/libneon.so $(REQUIRES_SDL2)/
	cd $(REQUIRES_SDL2) && patch -p1 < ../sdl2pico8.patch

$(REQUIRES_NDS):
	rmdir -r $(REQUIRES_NDS) | true
	git clone https://github.com/Turro75/nds.git $(REQUIRES_NDS)
	cd $(REQUIRES_NDS) && git checkout 55d45ea
	cd $(REQUIRES_NDS) && patch -p1 < ../nds_19.patch

include ../all/readmes/makefile
