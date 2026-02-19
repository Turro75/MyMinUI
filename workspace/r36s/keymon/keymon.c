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
#define RAW_START	 	705 
#define RAW_START_353 	315 
#define RAW_SELECT	 	704
#define RAW_SELECT_353	314
#define CODE_PLUS		115 //event3
#define CODE_MINUS		114 //event3
#define CODE_PWR		116 //event0
#define CODE_R1			311
#define CODE_R2			313
#define CODE_L1			310
#define CODE_L2			312

//	for ev.value
#define RELEASED	0
#define PRESSED		1
#define REPEAT		2

//	for button_flag
#define SELECT_BIT	0
#define START_BIT	1
#define SELECT		(1<<SELECT_BIT)
#define START		(1<<START_BIT)

#define INPUT_COUNT 3
static int inputs[INPUT_COUNT];
static struct input_event ev;
FILE *file_log;

//add menu+select handling for standalone apps.
// once a standalone app is launched it writes its name to /tmp/killstandalone.txt -> system("echo blabla > /tmp/killstandalone.txt"); or putFile form utils.c
// at every cycle it is checked if the file exists, if yes, it is cheched if menu+select is pressed (or any other better combination) and 
//if yes it is performed a system("kill -9 $pidof $("cat /tmp/killstandalone.txt")"); the file must be deleted by the launch.sh script so in case the kill failed it can retry.

#define KILLSTANDALONE_PATH "/tmp/killstandalone.txt"

int main (int argc, char *argv[]) {
	InitSettings();
	int is353v = 0;
	int isrgb30 = 0;
	int isv10 = 0;
	int isrg351p = 0;
	int _MENU_RAW = CODE_MENU;
	int _START_RAW = RAW_START;
	int _SELECT_RAW = RAW_SELECT;
	int _PLUS_RAW = CODE_PLUS;
	int _MINUS_RAW = CODE_MINUS;
	int _R1_RAW = CODE_R1;
    int _R2_RAW = CODE_R2;
	int _L1_RAW = CODE_L1;
	int _L2_RAW = CODE_L2;
	int menumissing = 0;

	if (access("/dev/input/by-path/platform-fdd40000.i2c-platform-rk805-pwrkey-event",F_OK)==0) {
		//is the rk3566 based rg353v/353p/rgb30		
		if (access("/dev/input/by-path/platform-fe5b0000.i2c-event",F_OK)==0) {
			//is the rg353v/p
			is353v = 1;
		} else {
			//is the rgb30
			isrgb30 = 1;
		}
	} else {
		//is the rk3326 based devices mostly r36s/r40xx/r36s_plus/rg351p/v10
		if (access("/dev/input/by-path/platform-odroidgo2-joypad-event-joystick",F_OK)==0) {
			//is the powkiddy v10
			isv10 = 1;
		} else if (access("/dev/input/by-path/platform-ff300000.usb-usb-0:1.2:1.0-event-joystick",F_OK)==0) {
			//is the rg351p
			isrg351p = 1;
		}
	}

	if (is353v==1) {
		inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // power
		inputs[1] = open("/dev/input/event4", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // controller
		inputs[2] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // volume +/-
		_MENU_RAW = CODE_MENU_353;
		_START_RAW = RAW_START_353;
		_SELECT_RAW = RAW_SELECT_353;
	} else if (isrgb30==1) {
		inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // power
		inputs[1] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // controller
		inputs[2] = open("/dev/input/event2", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // volume +/-
		_START_RAW = RAW_START_353;
		_SELECT_RAW = RAW_SELECT_353;
		_MENU_RAW = 800; //no menu button on rgb30
		menumissing = 1;
	} else if (isv10==1) {
		inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // power
		inputs[1] = open("/dev/input/event2", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // controller
		inputs[2] = open("/dev/input/event1", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // volume +/-
		_START_RAW= 709;
		_SELECT_RAW= 704;
		_PLUS_RAW= 708;
		_MINUS_RAW= 705;
		_MENU_RAW = 800; //no menu button on v10
		menumissing = 1;
	} else if (isrg351p==1) {
		//check_rumble("/dev/input/event3"); no rumble support?
		inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // power
		inputs[1] = open("/dev/input/event2", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // controller
		inputs[2] = open("/dev/input/event1", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // volume +/-
		_R1_RAW = 309;
	    _R2_RAW = 315;
		_L1_RAW = 308;
		_L2_RAW = 314;
		_START_RAW = 310;
		_SELECT_RAW = 311;
		_MENU_RAW= 800; //no menu button on rg351p
		menumissing = 1;		
	} else {
		//r36s
		inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // power
		inputs[1] = open("/dev/input/event2", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // controller
		inputs[2] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // volume +/-
	}
		

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
				//printf("Keymon: /dev/input/event%d: Type: %i Code: %i (Val: %i)\n", i ,ev.type, ev.code, ev.value); fflush(stdout);
				val = ev.value;

				if (( ev.type != EV_KEY ) || ( val > REPEAT )) continue;
				if (ev.code == _MENU_RAW) {
					menu_pressed = val;
				}
				if (ev.code == _SELECT_RAW) {
					select_pressed = val;
				}
				if (ev.code == _START_RAW) {
					start_pressed = val;
				}
//				if (ev.code == _L3_RAW) {
//					start_pressed = val;
//				}
//				if (ev.code == _R3_RAW) {
//					select_pressed = val;
//				}
				if (ev.code == _PLUS_RAW) {
					up_pressed = up_just_pressed = val;
					if (val) up_repeat_at = now + 300;
				}
				if (ev.code == _MINUS_RAW) {
					down_pressed = down_just_pressed = val;
					if (val) down_repeat_at = now + 300;
				}
				if (((ev.code == _R1_RAW) || (ev.code == _R2_RAW)) && (isrg351p==1)) {
					up_pressed = up_just_pressed= val;
					if (val) up_repeat_at = now + 300;
				}
				if (((ev.code == _L1_RAW) || (ev.code == _L2_RAW)) && (isrg351p==1)) {
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

		if (access(KILLSTANDALONE_PATH,F_OK)==0) {
			if (start_pressed && select_pressed) { 
				char cmd[512];
				sprintf(cmd, "kill -9 $(pidof $(cat %s))", KILLSTANDALONE_PATH);
				if (system(cmd) == 0) {
					unlink(KILLSTANDALONE_PATH);
				}
			}
		}
		if (up_just_pressed || (up_pressed && now>=up_repeat_at)) {
			if (isrg351p==0) {
				if ((menu_pressed) || ((menumissing==1) && (select_pressed) && (start_pressed))) {
					//printf("brightness up\n"); fflush(stdout);
					val = GetBrightness();
					if (val<BRIGHTNESS_MAX) SetBrightness(++val);
				}
				else {
					//printf("volume up\n"); fflush(stdout);
					val = GetVolume();
					if (val<VOLUME_MAX) SetVolume(++val);
				}
			} else {
				//rg351p has no menu button, and no +/-  buttons so just use R1/R2 for volume/brightness up and L1/L2 for volume/brightness down
				if (select_pressed) {
					//printf("volume up\n"); fflush(stdout);
					val = GetVolume();
					if (val<VOLUME_MAX) SetVolume(++val);
				} else if (start_pressed) {
					//printf("brightness up\n"); fflush(stdout);
					val = GetBrightness();
					if (val<BRIGHTNESS_MAX) SetBrightness(++val);
				}
			}
			if (up_just_pressed) up_just_pressed = 0;
			else up_repeat_at += 100;
		}
		
		if (down_just_pressed || (down_pressed && now>=down_repeat_at)) {
			if (isrg351p==0) {
				if ((menu_pressed) || ((menumissing==1) && (select_pressed) && (start_pressed))) {
					//printf("brightness down\n"); fflush(stdout);
					val = GetBrightness();
					if (val>BRIGHTNESS_MIN) SetBrightness(--val);
				}
				else {
					//printf("volume down\n"); fflush(stdout);
					val = GetVolume();
					if (val>VOLUME_MIN) SetVolume(--val);
				}
			} else {
				//rg351p has no menu button, and no +/-  buttons so just use R1/R2 for volume/brightness up and L1/L2 for volume/brightness down
				if (select_pressed) {
					//printf("volume up\n"); fflush(stdout);
					val = GetVolume();
					if (val>VOLUME_MIN) SetVolume(--val);
				} else if (start_pressed) {
					//printf("brightness up\n"); fflush(stdout);
					val = GetBrightness();
					if (val>BRIGHTNESS_MIN) SetBrightness(--val);
				}
			}
			if (down_just_pressed) down_just_pressed = 0;
			else down_repeat_at += 100;
		}
		
		then = now;
		ignore = 0;
		
		usleep(16667); // 60fps
	}
	
}
