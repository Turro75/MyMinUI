#ifndef turbo_h__
#define turbo_h__

#include <stdint.h>

enum {
	TURBO_SLOW,
	TURBO_NORMAL,
	TURBO_FAST,
	TURBO_SPEED_COUNT,
};

// frames_held: how many frames the button has been held (0 on the first frame).
// speed: TURBO_*. Out of range falls back to TURBO_NORMAL.
// Returns 1 if the button should read as pressed on this frame.
int Turbo_fire(uint32_t frames_held, int speed);

#endif
