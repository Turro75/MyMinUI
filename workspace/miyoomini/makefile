###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_SDL=other/sdl
REQUIRES_COMMANDER=other/DinguxCommander
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

	cp -L ${PREFIX}/lib/libjson-c.so.2 $(REQUIRES_NDS)/drastic/lib/

early:  $(REQUIRES_NDS) #$(REQUIRES_SDL)
	cd other/latency_reduction && make

clean:
	cd blank && make clean
	cd batmon && make clean
	cd lumon && make clean
	cd show && make clean
	cd other/latency_reduction && make clean
#	cd $(REQUIRES_NDS) && make -f Makefile.miyoo_mini clean
	cd $(REQUIRES_NDS) && make -f Makefile.mini clean

###########################################################

$(REQUIRES_SDL):
	git clone --depth 1 --branch miniui-miyoomini https://github.com/shauninman/SDL-1.2.git $(REQUIRES_SDL)
$(REQUIRES_COMMANDER):
	git clone --depth 1 --branch miniui-miyoomini https://github.com/shauninman/DinguxCommander.git $(REQUIRES_COMMANDER)

$(REQUIRES_NDS):
	rmdir -r $(REQUIRES_NDS) | true
	git clone https://github.com/Turro75/nds.git $(REQUIRES_NDS)
	cd $(REQUIRES_NDS) && git checkout 55d45ea
	cd $(REQUIRES_NDS) && patch -p1 < ../nds_19.patch

include ../all/readmes/makefile
