#include <stdio.h>
#include "turbo.h"

static int failures = 0;

static void check(int cond, const char* what) {
	if (cond) { printf("  ok   %s\n", what); }
	else      { printf("  FAIL %s\n", what); failures++; }
}

// a "shot" is a rising edge: a frame returning 1 after returning 0 before it.
// frame -1 counts as 0, so frames_held==0 returning 1 already is an edge.
static int count_edges(int speed, int frames) {
	int edges = 0;
	int prev = 0;
	for (int f=0; f<frames; f++) {
		int now = Turbo_fire(f, speed);
		if (now && !prev) edges += 1;
		prev = now;
	}
	return edges;
}

static int count_on(int speed, int frames) {
	int on = 0;
	for (int f=0; f<frames; f++) if (Turbo_fire(f, speed)) on += 1;
	return on;
}

// shortest contiguous run of pressed frames
static int min_run(int speed, int frames) {
	int min = frames + 1;
	int run = 0;
	for (int f=0; f<frames; f++) {
		if (Turbo_fire(f, speed)) run += 1;
		else if (run) { if (run<min) min = run; run = 0; }
	}
	if (run && run<min) min = run;
	return min;
}

static void test_first_frame_always_fires(void) {
	printf("primeiro frame\n");
	check(Turbo_fire(0, TURBO_SLOW)   == 1, "Slow dispara no frame 0");
	check(Turbo_fire(0, TURBO_NORMAL) == 1, "Normal dispara no frame 0");
	check(Turbo_fire(0, TURBO_FAST)   == 1, "Fast dispara no frame 0");
}

static void test_duty_cycle(void) {
	printf("duty cycle em 60 frames (1 segundo a 60fps)\n");
	check(count_edges(TURBO_SLOW,   60) ==  5, "Slow da 5 disparos por segundo");
	check(count_edges(TURBO_NORMAL, 60) == 10, "Normal da 10 disparos por segundo");
	check(count_edges(TURBO_FAST,   60) == 15, "Fast da 15 disparos por segundo");

	check(count_on(TURBO_SLOW,   60) == 30, "Slow fica metade do tempo pressionado");
	check(count_on(TURBO_NORMAL, 60) == 30, "Normal fica metade do tempo pressionado");
	check(count_on(TURBO_FAST,   60) == 30, "Fast fica metade do tempo pressionado");
}

static void test_minimum_press_length(void) {
	printf("piso de 2 frames pressionados\n");
	check(min_run(TURBO_SLOW,   60) >= 2, "Slow nunca pressiona por menos de 2 frames");
	check(min_run(TURBO_NORMAL, 60) >= 2, "Normal nunca pressiona por menos de 2 frames");
	check(min_run(TURBO_FAST,   60) >= 2, "Fast nunca pressiona por menos de 2 frames");
}

static void test_invalid_speed_falls_back_to_normal(void) {
	printf("velocidade invalida\n");
	int negative_matches = 1;
	int too_big_matches = 1;
	for (int f=0; f<60; f++) {
		if (Turbo_fire(f, -1)  != Turbo_fire(f, TURBO_NORMAL)) negative_matches = 0;
		if (Turbo_fire(f, 99)  != Turbo_fire(f, TURBO_NORMAL)) too_big_matches  = 0;
	}
	check(negative_matches, "speed negativo se comporta como Normal");
	check(too_big_matches,  "speed acima da faixa se comporta como Normal");
	check(TURBO_SPEED_COUNT == 3, "existem exatamente tres velocidades");
}

static void test_reset_restarts_the_cycle(void) {
	printf("reset na soltura\n");
	// releasing and pressing again zeroes frames_held; the sequence must match
	int same = 1;
	for (int f=0; f<24; f++) {
		if (Turbo_fire(f, TURBO_NORMAL) != Turbo_fire(f % 6, TURBO_NORMAL)) same = 0;
	}
	check(same, "a fase depende so de frames_held, sem estado escondido");
	check(Turbo_fire(0, TURBO_NORMAL) == 1, "reapertar dispara na hora");
}

int main(void) {
	test_first_frame_always_fires();
	test_duty_cycle();
	test_minimum_press_length();
	test_invalid_speed_falls_back_to_normal();
	test_reset_restarts_the_cycle();
	printf("\n%s (%d failures)\n", failures ? "FAILED" : "PASSED", failures);
	return failures ? 1 : 0;
}
