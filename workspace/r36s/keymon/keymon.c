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
#define CODE_MENU		708 //event2
#define CODE_MENU_353 	316 //event4
#define CODE_PLUS		115 //event3
#define CODE_MINUS		114 //event3
#define CODE_PWR		116 //event0

//	for ev.value
#define RELEASED	0
#define PRESSED		1
#define REPEAT		2

#define INPUT_COUNT 3
static int inputs[INPUT_COUNT];
static struct input_event ev;
FILE *file_log;

int main (int argc, char *argv[]) {
	InitSettings();
	int is353v = 0;
	int isg350 = 0;
	int _MENU_RAW = CODE_MENU;
	// TODO: will require two inputs
	// input_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (access("/dev/input/by-path/platform-fe5b0000.i2c-event",F_OK)==0) {
		//is the rg353v
		is353v = 1;
		isg350 = 0;
	}
	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // power
	if (is353v) {
		inputs[1] = open("/dev/input/event4", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // controller
		_MENU_RAW = CODE_MENU_353;
	} else if (isg350) {
		inputs[1] = open("/dev/input/event2", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // controller
	} else {
		//r36s
		inputs[1] = open("/dev/input/event2", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // controller
	}
	
	inputs[2] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // volume +/-
	//Stick_init(); // analog

	//inputs[0] = open("/dev/input/event1", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	
	//printf("opened /dev/input/event1\n");system("sync");

	uint32_t input;
	uint32_t val;
	uint32_t menu_pressed = 0;
	
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
				//printf("Keymon: /dev/input/event%d: Type: %i Code: %i (Val: %i)\n", i ,ev.type, ev.code, ev.value); fflush(stdout);
				val = ev.value;

				if (( ev.type != EV_KEY ) || ( val > REPEAT )) continue;
				if (ev.code == _MENU_RAW) {
					menu_pressed = val;
				}
				if (ev.code == CODE_PLUS) {
					up_pressed = up_just_pressed = val;
					if (val) up_repeat_at = now + 300;
				}
				if (ev.code == CODE_MINUS) {
					down_pressed = down_just_pressed = val;
					if (val) down_repeat_at = now + 300;
				}
				/* switch (ev.code) {
					case _MENU_RAW:
						menu_pressed = val;
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
						//file_log = fopen("/mnt/SDCARD/.userdata/m21/logs/keymon.log", "a+"); 
						//fprintf(file_log, "Event BTN Code = %d\n",ev.code);
						//fclose(file_log); system("sync");
					break;
				} */
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
			if (menu_pressed) {
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
			if (menu_pressed) {
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
