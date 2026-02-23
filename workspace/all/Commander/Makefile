#
#	DinguxCommander Makefile for RG35xx garlic
#


include ../../$(UNION_PLATFORM)/platform/makefile.env
SDL?=SDL2

CXX := $(CROSS_COMPILE)g++
CC := $(CROSS_COMPILE)gcc
CXXFLAGS := -Os $(MYARCH)
SDL_CONFIG := $(PREFIX)/bin/sdl-config
SDL2_CONFIG := $(PREFIX)/bin/sdl2-config
ifeq ($(SDL),SDL2)
  _SDL_CONFIG := $(SDL2_CONFIG)
else
  _SDL_CONFIG := $(SDL_CONFIG)
endif

CXXFLAGS += $(shell $(_SDL_CONFIG) --cflags) -I$(PREFIX)/include/$(SDL) -I./
CXXFLAGS += -I../common/ -I../../$(UNION_PLATFORM)/platform/ -I../../$(UNION_PLATFORM)/libmsettings/ -I../minarch/libretro-common/include/
CXXFLAGS += $(MY_INCDIR) $(MY_CFLAGS)

CXXFLAGS += -DOSK_KEY_SYSTEM_IS_BACKSPACE=ON
CXXFLAGS += -DPPU_X=2
CXXFLAGS += -DPPU_Y=2
CXXFLAGS += -DAUTOSCALE=1
CXXFLAGS += -DFONTS='{"FreeSans.ttf",20}'

CXXFLAGS += -DUSE_$(SDL)
CXXFLAGS += -DPLATFORM=\"$(UNION_PLATFORM)\"
#CXXFLAGS += -DFONTS='{"FreeSans.ttf",18},{"DroidSansFallback.ttf",15},{"/mnt/mmc/CFW/skin/font.ttf",14}'
#CXXFLAGS += -DLOW_DPI_FONTS='{"Fiery_Turk.ttf",8},{"/mnt/mmc/CFW/skin/font.ttf",9}'

RESDIR := res
CXXFLAGS += -DRESDIR="\"$(RESDIR)\""

LINKFLAGS += -s
LINKFLAGS += $(shell $(_SDL_CONFIG) --libs) -l$(SDL)_image -l$(SDL)_ttf -lmsettings -lpthread $(MY_LDFLAGS)

CMD := 
SUM := @echo

OUTDIR := ./build/$(UNION_PLATFORM)

EXECUTABLE := $(OUTDIR)/MyCommander.elf

OBJS :=	main.o commander.o config.o dialog.o fileLister.o fileutils.o keyboard.o panel.o resourceManager.o \
	screen.o sdl_ttf_multifont.o sdlutils.o text_edit.o utf8.o axis_direction.o controller_buttons.o text_viewer.o image_viewer.o  window.o \
	$(SDL)_rotozoom.o platform.o api.o utils.o scaler.o


#OBJS += ../../$(PLATFORM)/platform.o
#OBJS += ../common/api.o

DEPFILES := $(patsubst %.o,$(OUTDIR)/%.d,$(OBJS))

.PHONY: all clean

all: $(EXECUTABLE)

$(EXECUTABLE): $(addprefix $(OUTDIR)/,$(OBJS)) 
	mkdir -p $(OUTDIR)
	$(SUM) "  LINK    $@"
	$(CMD)$(CXX) $(LINKFLAGS) -o $@ $^

$(OUTDIR)/$(SDL)_rotozoom.o: ../common/$(SDL)_rotozoom.c
	$(SUM) "  CXX     $@"
	$(CMD)$(CC) $(CXXFLAGS) -MP -MMD -MF $(@:%.o=%.d) ../common/$(SDL)_rotozoom.c -c -o $@
	@touch $@ # Force .o file to be newer than .d file.

#$(OUTDIR)/myneon.o: ../common/pixman-arm-neon-asm.S
#	$(SUM) "  CXX     $@"
#	$(CMD)$(CC) $(CXXFLAGS) -MP -MMD -MF $(@:%.o=%.d) ../common/pixman-arm-neon-asm.S -c -o $@
#	@touch $@ # Force .o file to be newer than .d file.

$(OUTDIR)/api.o: ../common/api.c
	$(SUM) "  CXX     $@"
	$(CMD)$(CC) $(CXXFLAGS) -MP -MMD -MF $(@:%.o=%.d) ../common/api.c -c -o $@
	@touch $@ # Force .o file to be newer than .d file.

$(OUTDIR)/utils.o: ../common/utils.c
	$(SUM) "  CXX     $@"
	$(CMD)$(CC) $(CXXFLAGS) -MP -MMD -MF $(@:%.o=%.d) ../common/utils.c -c -o $@
	@touch $@ # Force .o file to be newer than .d file.

$(OUTDIR)/scaler.o: ../common/scaler.c
	$(SUM) "  CXX     $@"
	$(CMD)$(CC) $(CXXFLAGS) -MP -MMD -MF $(@:%.o=%.d) ../common/scaler.c -c -o $@
	@touch $@ # Force .o file to be newer than .d file.

$(OUTDIR)/platform.o: ../../$(UNION_PLATFORM)/platform/platform.c
	$(SUM) "  CXX     $@"
	$(CMD)$(CC) $(CXXFLAGS) -MP -MMD -MF $(@:%.o=%.d) ../../$(UNION_PLATFORM)/platform/platform.c -c -o $@
	@touch $@ # Force .o file to be newer than .d file.

$(OUTDIR)/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(SUM) "  CXX     $@"
	$(CMD)$(CXX) $(CXXFLAGS) -MP -MMD -MF $(@:%.o=%.d) -c $< -o $@
	@touch $@ # Force .o file to be newer than .d file.

$(OUTDIR)/%.o: src/%.c
	@mkdir -p $(@D)
	$(SUM) "  CXX     $@"
	$(CMD)$(CXX) $(CXXFLAGS) -MP -MMD -MF $(@:%.o=%.d) -c $< -o $@
	@touch $@ # Force .o file to be newer than .d file.

clean:
	$(SUM) "  RM      $(OUTDIR)"
	$(CMD)rm -rf $(OUTDIR)

# Load dependency files.
-include $(DEPFILES)

# Generate dependencies that do not exist yet.
# This is only in case some .d files have been deleted;
# in normal operation this rule is never triggered.
$(DEPFILES):
