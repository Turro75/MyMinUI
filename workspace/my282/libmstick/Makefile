ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif
ifeq (,$(PREFIX))
$(error missing PREFIX for this toolchain)
endif

include ../platform/makefile.env

TARGET=mstick

.PHONY: build
.PHONY: clean

CC = $(CROSS_COMPILE)gcc

SYSROOT     := $(shell $(CC) --print-sysroot)

INCLUDEDIR = $(SYSROOT)/usr/include
CFLAGS = -I$(INCLUDEDIR) $(MYARCH)
LDFLAGS = -ldl -lpthread -lrt -s

OPTM=-Ofast

build: 
	$(CC) -c -Werror -fpic "$(TARGET).c" $(CFLAGS) -Wl,--no-as-needed $(LDFLAGS)
	$(CC) -shared -o "lib$(TARGET).so" "$(TARGET).o" $(CFLAGS) $(LDFLAGS)
	cp "$(TARGET).h" "$(PREFIX)/include"
	cp "lib$(TARGET).so" "$(PREFIX)/lib"
clean:
	rm -f *.o
	rm -f "lib$(TARGET).so"
	rm -f $(PREFIX)/include/$(TARGET).h
	rm -f $(PREFIX)/lib/lib$(TARGET).so
