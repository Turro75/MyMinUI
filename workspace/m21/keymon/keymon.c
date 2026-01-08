#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <linux/input.h>

#include <msettings.h>

#define VOLUME_MIN 		0
#define VOLUME_MAX 		20
#define BRIGHTNESS_MIN 	0
#define BRIGHTNESS_MAX 	10

// uses different codes from SDL
#define CODE_MENU		27
#define CODE_SELECT 	296
#define CODE_START		297
#define CODE_PLUS		12
#define CODE_MINUS		11
#define CODE_PWR		-1


int USER_BTN_MENU = CODE_MENU;
int	USER_BTN_SELECT = CODE_SELECT;
int USER_BTN_START = CODE_START;
int USER_BTN_VOLUMEUP = CODE_PLUS;
int USER_BTN_VOLUMEDOWN = CODE_MINUS;
int USER_BTN_POWER = CODE_PWR;

//	for ev.value
#define RELEASED	0
#define PRESSED		1
#define REPEAT		2

#define INPUT_COUNT 1
static int inputs[INPUT_COUNT];
static struct input_event ev;
FILE *file_log;

void PAD_readCustomButtonMapping(void){
 	//check if there are any custom setting for system button mapping
 	char *env;
	printf("Default USER_BTN_SELECT = %d\n", USER_BTN_SELECT);
	env = getenv("USER_BTN_SELECT");
	if(env!=NULL) {
 		USER_BTN_SELECT = atoi(env);
 		printf("Override BTN_SELECT with value %d\n", USER_BTN_SELECT);
 	}

	printf("Default USER_BTN_START = %d\n", USER_BTN_START);
	env = getenv("USER_BTN_START");
    if(env!=NULL) {
 		USER_BTN_START= atoi(env);
 		printf("Override BTN_START with value %d\n", USER_BTN_START);
 	}

	printf("Default USER_BTN_MENU = %d\n", USER_BTN_MENU);
	env = getenv("USER_BTN_MENU");
    if(env!=NULL) {
 		USER_BTN_MENU= atoi(env);
 		printf("Override BTN_MENU with value %d\n", USER_BTN_MENU);
 	}

	printf("Default USER_BTN_VOLUMEUP = %d\n", USER_BTN_VOLUMEUP);
	env = getenv("USER_BTN_VOLUMEUP");
    if(env!=NULL) {
 		USER_BTN_VOLUMEUP= atoi(env);
 		printf("Override BTN_VOLUMEUP with value %d\n", USER_BTN_VOLUMEUP);
 	}

	printf("Default USER_BTN_VOLUMEDOWN = %d\n", USER_BTN_VOLUMEDOWN);
	env = getenv("USER_BTN_VOLUMEDOWN");
    if(env!=NULL) {
 		USER_BTN_VOLUMEDOWN= atoi(env);
 		printf("Override BTN_VOLUMEDOWN with value %d\n", USER_BTN_VOLUMEDOWN);
 	}

	printf("Default USER_BTN_POWER = %d\n", USER_BTN_POWER);
	env = getenv("USER_BTN_POWER");
    if(env!=NULL) {
 		USER_BTN_POWER= atoi(env);
 		printf("Override BTN_POWER with value %d\n", USER_BTN_POWER);
 	}
 }


int main (int argc, char *argv[]) {
	InitSettings();
	PAD_readCustomButtonMapping();
	// TODO: will require two inputs
	// input_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	inputs[0] = open("/dev/input/event1", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (inputs[0] < 0) {
		printf("KEYMON: failed to open /dev/input/event1 with error \n");system("sync");
		return -1;
	}
	//printf("opened /dev/input/event1\n");system("sync");

	uint32_t input;
	uint32_t val;
	uint32_t menu_pressed = 0;
	uint32_t select_pressed = 0;
	uint32_t start_pressed = 0;
	
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
	while (1) {
		gettimeofday(&tod, NULL);
		now = tod.tv_sec * 1000 + tod.tv_usec / 1000;
		if (now-then>1000) ignore = 1; // ignore input that arrived during sleep
		
		for (int i=0; i<INPUT_COUNT; i++) {
			int input_fd = inputs[i];
			while(read(input_fd, &ev, sizeof(ev))==sizeof(ev)) {
				if (ignore) continue;
				val = ev.value;

				if (( ev.type != EV_KEY ) || ( val > REPEAT )) continue;
	//			printf("Code: %i (%i)\n", ev.code, val); fflush(stdout);
				if (ev.code == USER_BTN_MENU) menu_pressed = val;
				else if (ev.code == USER_BTN_SELECT) select_pressed = val;
				else if (ev.code == USER_BTN_START) start_pressed = val;
				else if (ev.code == USER_BTN_VOLUMEUP) {
					up_pressed = up_just_pressed = val;
					if (val) up_repeat_at = now + 300;
				} 
				else if (ev.code == USER_BTN_VOLUMEDOWN) {
					down_pressed = down_just_pressed = val;
					if (val) down_repeat_at = now + 300;
				}
			}
		}
		
		if (ignore) {
			menu_pressed = 0;
			up_pressed = up_just_pressed = 0;
			down_pressed = down_just_pressed = 0;
			up_repeat_at = 0;
			down_repeat_at = 0;
		}
		
		if (up_just_pressed || (up_pressed && now>=up_repeat_at)) {
			if ((menu_pressed) || ((select_pressed) && (start_pressed))) {
				//printf("brightness up\n"); fflush(stdout);
				val = GetBrightness();
				if (val<BRIGHTNESS_MAX) SetBrightness(++val);
			}
			else {
				//printf("volume up\n"); fflush(stdout);
				val = GetVolume();
				if (val<VOLUME_MAX) SetVolume(++val);
			}
			
			if (up_just_pressed) up_just_pressed = 0;
			else up_repeat_at += 100;
		}
		
		if (down_just_pressed || (down_pressed && now>=down_repeat_at)) {
			if ((menu_pressed) || ((select_pressed) && (start_pressed))) {
				//printf("brightness down\n"); fflush(stdout);
				val = GetBrightness();
				if (val>BRIGHTNESS_MIN) SetBrightness(--val);
			}
			else {
				 //printf("volume down\n"); fflush(stdout);
				val = GetVolume();
				if (val>VOLUME_MIN) SetVolume(--val);
			}
			
			if (down_just_pressed) down_just_pressed = 0;
			else down_repeat_at += 100;
		}
		
		then = now;
		ignore = 0;
		
		usleep(16667); // 60fps
	}
	
}
