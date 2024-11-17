#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>

#include <msettings.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "defines.h"

//	for ev.value
#define RELEASED	0
#define PRESSED		1
#define REPEAT		2

#define INPUT_COUNT 4
#define JS_COUNT 3
static int inputs[INPUT_COUNT];
static int jsinputs[JS_COUNT];
static struct input_event ev;

struct js_event {
		uint32_t time;     /* event timestamp in milliseconds */
		int16_t value;    /* value */
		uint8_t type;      /* event type */
		uint8_t number;    /* axis/button number */
	};
#define JS_EVENT_BUTTON         0x01    /* button pressed/released */
#define JS_EVENT_AXIS           0x02    /* joystick moved */
#define JS_EVENT_INIT           0x80    /* initial state of device */

struct js_event jsev;

static pthread_t ports_pt;

#define JACK_STATE_PATH "/sys/class/switch/h2w/state"
#define BACKLIGHT_PATH "/sys/class/backlight/backlight.2/bl_power"

int getInt(char* path) {
	int i = 0;
	FILE *file = fopen(path, "r");
	if (file!=NULL) {
		fscanf(file, "%i", &i);
		fclose(file);
	}
	return i;
}

static void* watchPorts(void *arg) {
	int has_headphones,had_headphones;
	
	has_headphones = had_headphones = getInt(JACK_STATE_PATH);
	SetJack(has_headphones);
	
	while(1) {
		sleep(1);
		
		has_headphones = getInt(JACK_STATE_PATH);
		if (had_headphones!=has_headphones) {
			had_headphones = has_headphones;
			SetJack(has_headphones);
		}
	}
	
	return 0;
}

int fd_is_valid(int fd)
{
    return fcntl(fd, F_GETFD) != -1;
}

int main (int argc, char *argv[]) {
	InitSettings();
	pthread_create(&ports_pt, NULL, &watchPorts, NULL);
	
	char path[INPUT_COUNT][32];
	char jspath[JS_COUNT][32];
	for (int i=0; i<INPUT_COUNT; i++) {
		sprintf(path[i], "/dev/input/event%i", i);
		inputs[i] = open(path[i], O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	}
	
	for (int i=0; i<JS_COUNT; i++) {
		sprintf(jspath[i], "/dev/input/js%i", i);
		jsinputs[i] = open(jspath[i], O_RDONLY | O_NONBLOCK| O_CLOEXEC);
	}

	uint32_t input;
	uint32_t val;
	uint32_t menu_pressed = 0;
	uint32_t power_pressed = 0;
	
	uint32_t up_pressed = 0;
	uint32_t up_just_pressed = 0;
	uint32_t up_repeat_at = 0;
	
	uint32_t down_pressed = 0;
	uint32_t down_just_pressed = 0;
	uint32_t down_repeat_at = 0;
	
	uint8_t ignore;
	uint32_t then;
	uint32_t now;
	struct timeval tod;
	
	gettimeofday(&tod, NULL);
	then = tod.tv_sec * 1000 + tod.tv_usec / 1000; // essential SDL_GetTicks()
	ignore = 0;
	printf("\n");fflush(stdout);
	while (1) {
		gettimeofday(&tod, NULL);
		now = tod.tv_sec * 1000 + tod.tv_usec / 1000;
		if (now-then>100) ignore = 1; // ignore input that arrived during sleep
		
		for (int x=0; x<JS_COUNT; x++) {
		//	if (fcntl(jsinputs[x], F_GETFD) <	0) {
		//		printf("Reopening /dev/input/js%i\n", x);fflush(stdout);
		//		jsinputs[x] = open(jspath[x], O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		//	}
			while (read (jsinputs[x], &jsev, sizeof(jsev)) == sizeof(jsev)) {
				if (jsev.type > 0) {
					printf("/dev/input/js%i: type:%i number:%i value:%i\n", x, jsev.type, jsev.number, jsev.value); fflush(stdout);
				}
			}			
		}


		for (int i=0; i<INPUT_COUNT; i++) {
			
		//	if (fcntl(inputs[i], F_GETFD) <	0) {
		//		printf("Reopening /dev/input/event%i\n", i);fflush(stdout);
		//		inputs[i] = open(path[i], O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		//	}
			input = inputs[i];
			while(read(input, &ev, sizeof(ev))==sizeof(ev)) {
				if (ignore) continue;
				
				val = ev.value;
				if ((( ev.type == EV_KEY )  && ( val <= REPEAT )) || (ev.type == EV_ABS)) {
					printf("/dev/input/event%i: type:%i code:%i value:%i\n\n", i, ev.type,ev.code,ev.value); fflush(stdout);
					switch (ev.code) {
						case 316:
							system("/mnt/sdcard/Tools/rg35xx/Info.pak/launch.sh");fflush(stdout);
							break;
						case CODE_MENU:
							menu_pressed = val;
						break;
						case CODE_POWER:
							power_pressed = val;
						break;
						case CODE_PLUS:
							up_pressed = up_just_pressed = val;
							if (val) up_repeat_at = now + 300;
						break;
						case CODE_MINUS:
							down_pressed = down_just_pressed = val;
							if (val) down_repeat_at = now + 300;
						break;
						default:
						break;
					}
				}
			}
		}
		
		if (up_just_pressed || (up_pressed && now>=up_repeat_at)) {
			if (menu_pressed) {
				val = GetBrightness();
				if (val<BRIGHTNESS_MAX) SetBrightness(++val);
			}
			else {
				val = GetVolume();
				if (val<VOLUME_MAX) SetVolume(++val);
			}
			
			if (up_just_pressed) up_just_pressed = 0;
			else up_repeat_at += 100;
		}
		
		if (down_just_pressed || (down_pressed && now>=down_repeat_at)) {
			if (menu_pressed) {
				val = GetBrightness();
				if (val>BRIGHTNESS_MIN) SetBrightness(--val);
			}
			else {
				val = GetVolume();
				if (val>VOLUME_MIN) SetVolume(--val);
			}
			
			if (down_just_pressed) down_just_pressed = 0;
			else down_repeat_at += 100;
		}
		
		then = now;
		ignore = 0;
		
		usleep(16666); // 60fps
	}
}
