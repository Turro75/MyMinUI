# audioserver Makefile

ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

CC = $(CROSS_COMPILE)gcc
CFLAGS = -Os -Wl,--gc-sections -s

# Regola principale
all: audioserver.elf libredirect_dsp.so

# Compilazione dell'audioserver (eseguibile nativo per ALSA)
audioserver.elf: audioserver.c
	$(CC) $(CFLAGS) $< -o $@ $(LIBS) -lasound -lrt

# Compilazione del wrapper (libreria condivisa per LD_PRELOAD)
# Richiede -shared -fPIC e la libreria -ldl per risolvere dlsym
libredirect_dsp.so: wrapper.c
	$(CC) $(CFLAGS) -shared -fPIC $< -o $@ -ldl

# Pulizia dei file generati
clean:
	rm -f audioserver.elf libredirect_dsp.so
