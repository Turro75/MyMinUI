#include "turbo.h"

// cycle period in frames at 60fps, half of it held down.
// 12 -> 5 shots/s, 6 -> 10 shots/s, 4 -> 15 shots/s.
// there is deliberately no period below 4: with fewer than 2 frames held down
// most games stop registering the input, and turbo would start dropping shots
// instead of speeding them up.
static const uint8_t turbo_period[TURBO_SPEED_COUNT] = { 12, 6, 4 };

int Turbo_fire(uint32_t frames_held, int speed) {
	if (speed < 0 || speed >= TURBO_SPEED_COUNT) speed = TURBO_NORMAL;
	uint32_t period = turbo_period[speed];
	// frames_held == 0 always lands here: a short tap still counts as one press.
	return (frames_held % period) < (period / 2);
}
