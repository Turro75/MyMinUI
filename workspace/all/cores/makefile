###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
	$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###############################

# overrides shared by all cores

# commits frozen to let compile process without issues, also good for save state compatibility across all devices.

a2600_HASH                   = 3cc89f0d  #libretro/stella2014-libretro
a5200_HASH                   = 52640407  #libretro/a5200
a7800_HASH                   = acae250d  #libretro/prosystem-libretro
beetle-pce-fast_HASH         = 71675959  #libretro/beetle-pce-fast-libretro
beetle-vb_HASH               = 8f837ebc  #libretro/beetle-vb-libretro
beetle_ngp_HASH              = 139fe34c  #libretro/beetle-ngp-libretro
dosbox_HASH                  = e8cb6683  #libretro/dosbox-pure
fake-08_HASH                 = 0d26fd59  #jtothebell/fake-08
fbneo_HASH                   = ffdacc6a  #libretro/fbneo
fceumm_HASH                  = 3544ff56  #libretro/libretro-fceumm
gambatte_HASH                = 9f591132  #libretro/gambatte-libretro
gpsp_HASH                    = b0d5d27a  #libretro/gpsp
mame2003-plus_HASH           = 04fb75e4  #libretro/mame2003-plus-libretro
mednafen_supafaust_HASH      = e25f6676  #libretro/supafaust
mgba_HASH                    = c9bbf28b  #libretro/mgba
neocd_HASH                   = 5eca2c8f  #libretro/neocd_libretro
pcsx_rearmed_HASH            = 6365a756  #libretro/pcsx_rearmed
picodrive_HASH               = aed5b649  #irixxxx/picodrive
pokemini_HASH                = 78656d46  #libretro/PokeMini
prboom_HASH                  = b3e5f8b2  #libretro/libretro-prboom
puae_HASH                    = c68404cf  #libretro/libretro-uae
race_HASH                    = 171950ea  #libretro/race
retroarch_HASH               = 6616b807  #libretro/RetroArch
snes9x2005_plus_HASH         = 74d871db  #libretro/snes9x2005
tyrquake_HASH                = dfdae65c  #libretro/tyrquake
wonderswan_HASH              = 2aeb47d3  #libretro/beetle-wswan-libretro

###############################

# this logic was broken out from picoarch's all-in-one makefile

PATCH = git apply
PROCS = -j8

###############################

BUILD_ROOT=../../$(PLATFORM)/cores

define TEMPLATE=

$1_REPO ?= https://github.com/libretro/$(1)
$1_MAKE ?= make $(and $($1_MAKEFILE),-f $($1_MAKEFILE)) platform=$(PLATFORM) $($(1)_FLAGS)
$1_BUILD_PATH ?= $(1)

src/$(1):
	mkdir -p src
	cd src && git clone $(if $($1_HASH),,--depth 1) --recursive $$($(1)_REPO) $(1)
	$(if $($1_HASH),cd src/$$($1_BUILD_PATH) && git checkout $($1_HASH) && echo $($1_HASH),)

src/$(1)/.patched: src/$(1)
	(test ! -f patches/$(1).patch) || (test -f src/$(1)/.patched) || (cd src/$(1) && $(PATCH) -p1 < ../../patches/$(1).patch && touch .patched && true)

src/$(1)/.patched-all:
	(test ! -d ../../all/cores/patches/$(1)) || (test -f src/$(1)/.patched-all) || (cd src/$(1) && $(foreach patch, $(sort $(wildcard ../../all/cores/patches/$(1)/*.patch)), $(PATCH) -p1 < ../../$(patch) &&) touch .patched-all && true)
	

output/$(1)_libretro.so: src/$(1)/.patched src/$(1)/.patched-all
	mkdir -p output
	cd src/$$($1_BUILD_PATH) && $$($1_MAKE) $(PROCS)
	$(CROSS_COMPILE)strip src/$$($1_BUILD_PATH)/$(if $($(1)_CORE),$($(1)_CORE),$(1)_libretro.so)
	cp src/$$($1_BUILD_PATH)/$(if $($(1)_CORE),$($(1)_CORE),$(1)_libretro.so) ./output

clone-$(1): src/$(1)

status-$(1): src/$(1)
	cd src/$(1) && git show --oneline -s

patch-$(1): src/$(1)/.patched

clean-$(1):
	test ! -d src/$(1) || cd src/$$($1_BUILD_PATH) && $$($1_MAKE) clean
	rm -rf output/$(1)_libretro.so

$(1): output/$(1)_libretro.so

endef

###############################

all: cores

$(foreach CORE,$(CORES),$(eval $(call TEMPLATE,$(CORE))))

cores: $(foreach CORE,$(CORES),$(CORE))
	
clean: $(foreach CORE,$(CORES),clean-$(CORE))
	
nuke:
	rm -rf ./output
	rm -rf ./src