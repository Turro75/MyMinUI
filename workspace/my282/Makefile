#my282

###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_UNZIP60=other/unzip60
REQUIRES_NDS=other/nds

all: readmes
	cd show && make
	cd overclock && make
#	cd other/DinguxCommander && make
#   cd other/squashfs && make
#	cp -L $(PREFIX)/lib/libsamplerate.so.0 $(REQUIRES_LIBSAMPLERATE)/
early: $(REQUIRES_UNZIP60) $(REQUIRES_NDS)
	mkdir -p other
	cd $(REQUIRES_UNZIP60) && make -f unix/Makefile.trimuismart unzip
#	cd $(REQUIRES_LIBSAMPLERATE) && make && make install
	cd libmstick && make
	
clean:
	cd show && make clean
	cd overclock && make clean
#	cd other/squashfs && make clean
#	cd other/DinguxCommander && make clean
	cd other/unzip60 && make -f unix/Makefile.trimuismart clean
	cd $(REQUIRES_NDS) && make -f Makefile.a30 clean
#	cd other/libsamplerate && make clean
	
$(REQUIRES_UNZIP60):
	git clone --depth 1 https://github.com/shauninman/unzip60.git $(REQUIRES_UNZIP60)

$(REQUIRES_NDS):
	rm -r $(REQUIRES_NDS) | true
	git clone https://github.com/Turro75/nds.git $(REQUIRES_NDS)
	cd $(REQUIRES_NDS) && git checkout 55d45ea
	cd $(REQUIRES_NDS) && patch -p1 < ../nds_19.patch
	cd $(REQUIRES_NDS) && cp -r ../nds_add_libs/a30 ./drastic/lib/
	cd $(REQUIRES_NDS) && make -f Makefile.a30 cfg
	cd $(REQUIRES_NDS) && make -f Makefile.a30

include ../all/readmes/makefile