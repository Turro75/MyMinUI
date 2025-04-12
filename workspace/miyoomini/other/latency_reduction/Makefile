TARGET = as_preload.so

CC 	= $(CROSS_COMPILE)gcc

SOURCES = . 
CFILES = $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.c))

CFLAGS = -Os $(MYARCH)
CFLAGS += -fPIC -Wall
LDFLAGS	= -shared -ldl -s

$(TARGET): $(CFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) $(CFILES) -o $@

clean:
	rm -f $(TARGET)
