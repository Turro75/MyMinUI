# Turbo test -- builds and runs on the host (macOS/Linux), no SDL and no Docker.
CFLAGS = -std=gnu99 -Wall -Wextra -Wno-unused-parameter -I..

test: turbo_test
	./turbo_test

turbo_test: turbo_test.c ../turbo.c ../turbo.h
	$(CC) $(CFLAGS) turbo_test.c ../turbo.c -o turbo_test

clean:
	rm -f turbo_test
