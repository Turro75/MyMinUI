# sdl2 utilitiesMakefile

ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

CC = $(CROSS_COMPILE)gcc
CFLAGS =  -Wl,--gc-sections -g

# Regola principale
all: testcontrollersdl2.elf testsdl2drivers.elf show.elf

show.elf: testlayers.c sunxi_display2.h
	$(CC) $(CFLAGS) $< -o $@ $(LIBS) 

testcontrollersdl2.elf: testcontrollersdl2.c
	$(CC) $(CFLAGS) $< -o $@ $(LIBS) -lSDL2

testsdl2drivers.elf: testsdl2drivers.c
	$(CC) $(CFLAGS) $< -o $@ $(LIBS) -lSDL2


# Pulizia dei file generati
clean:
	rm -f testcontrollersdl2.elf testsdl2drivers.elf
