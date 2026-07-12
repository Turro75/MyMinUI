#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>
#include <pthread.h>
#include <linux/fb.h>

#include <msettings.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

// #include "defines.h"

#define VOLUME_MIN 		0
#define VOLUME_MAX 		20
#define BRIGHTNESS_MIN 	0
#define BRIGHTNESS_MAX 	10

// uses different codes from SDL
#define CODE_MENU		312 // but also 354
#define CODE_PLUS		115
#define CODE_MINUS		114

//	for ev.value
#define RELEASED	0
#define PRESSED		1
#define REPEAT		2

static int	input_fd = 0;
static struct input_event ev;
int loopdelay = 16667; // 60fps

static pthread_t hdmi_pt;
#define HDMI_STATE_PATH "/sys/class/extcon/hdmi/cable.0/state"


void putFile(char* path, char* contents) {
	FILE* file = fopen(path, "w");
	if (file) {
		fputs(contents, file);
		fclose(file);
	}
}
int getInt(char* path) {
	int i = 0;
	FILE *file = fopen(path, "r");
	if (file!=NULL) {
		fscanf(file, "%i", &i);
		fclose(file);
	}
	return i;
}
void putInt(char* path, int value) {
	char buffer[8];
	sprintf(buffer, "%d", value);
	putFile(path, buffer);
}

static void* watchHDMI(void *arg) {
	int has_hdmi,had_hdmi;
	
	has_hdmi = had_hdmi = getInt(HDMI_STATE_PATH);
	SetHDMI(has_hdmi);
	
	while(1) {
		sleep(1);
		
		has_hdmi = getInt(HDMI_STATE_PATH);
		if (had_hdmi!=has_hdmi) {
			had_hdmi = has_hdmi;
			SetHDMI(has_hdmi);
		}
	}
	
	return 0;
}

#define BLANK_PATH "/sys/class/graphics/fb0/blank"
#define LED_PATH "/sys/class/power_supply/axp2202-battery/work_led"
void enableBacklight(int enable) {
	if (enable) {
		putInt(BLANK_PATH, FB_BLANK_UNBLANK); // wake
		SetBrightness(GetBrightness());
		putInt(LED_PATH,0);
	}
	else {
		putInt(BLANK_PATH, FB_BLANK_POWERDOWN); // sleep
		SetRawBrightness(0);
		putInt(LED_PATH,1);
	}
}


#define LID_PATH "/sys/class/power_supply/axp2202-battery/hallkey"
static int lid_open_last=0;
int checkLidChanged(void) {
	int lid_open=0;
	if (access(LID_PATH, F_OK)==0) {
		lid_open = getInt(LID_PATH);
		if (lid_open!=lid_open_last) {
			lid_open_last = lid_open;
			return 1; //lid status changed, lid_open_last contains current status
		}
	}
	return 0;
}

//string array that contains alll possible standalone app names that must be paused when lid is closed
#define NUM_PROCESSSNAME 5
char * sa_process_name[NUM_PROCESSSNAME] = {
	"pico8_dyn",
	"drastic",
	"MyCommander.elf",
	"minput.elf",
	"clock.elf",
};

	int last_brightness;
	int last_volume;
	int last_cpufreq;

int main (int argc, char *argv[]) {
	InitSettings();
	pthread_create(&hdmi_pt, NULL, &watchHDMI, NULL);
	
	input_fd = open("/dev/input/event1", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	
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

		if (checkLidChanged()==1) {	//lid changed status
			int found = 0;
			char sa_running_name[256];
			for (int i=0; i<NUM_PROCESSSNAME; i++){
				char tmpstr[256];
				sprintf(tmpstr, "/bin/pidof %s > /dev/null", sa_process_name[i]);
				if(0 == system(tmpstr)){
					//read name of standalone app					
					sprintf(sa_running_name, "%s", sa_process_name[i]);
					found = 1;
					break;
				}
			}
			if (found==1){
		//		printf("Standalone app name: %s\n", sa_running_name); fflush(stdout);
				//check if lid is open/close
				char sa_cmd[256];
				//int blankfd = -1;
				if (lid_open_last==0){ //primo keymon di test a lemonzest
		//			printf("lid_is_CLOSED\n"); fflush(stdout);
					// Lid now closed, turn off backlight and pause the standalone process
					last_brightness = GetBrightness();
					last_volume = GetVolume();
					SetBrightness(0);
					SetVolume(0);
					loopdelay = 100000; // 10fps
					//blankfd = open("/proc/mi_modules/fb/mi_fb0", O_WRONLY);
					//write(blankfd, "GUI_SHOW 0 off", 14); //blank screen
					//close(blankfd);
					//blankfd = -1;
					enableBacklight(0);
					sprintf(sa_cmd, "/usr/bin/killall -SIGSTOP %s", sa_running_name);
					system(sa_cmd);
					//underclock to minimum cpu clock to save battery when lid closed 
					last_cpufreq = getInt("/tmp/new_cpu_freq.txt");	
					sprintf(sa_cmd, "/mnt/SDCARD/.system/h700/bin/overclock.elf 480000");
					system(sa_cmd);
					//paused=1;
				} else {
		//			printf("lid_is_OPEN\n"); fflush(stdout);
		            //restore cpu clock
					sprintf(sa_cmd, "/mnt/sdcard/.system/h700/bin/overclock.elf %d", last_cpufreq);
					system(sa_cmd);
					// Lid now opened, restore brightness and resume the standalone process
					SetBrightness(last_brightness);
					SetVolume(last_volume);					
					//blankfd = open("/proc/mi_modules/fb/mi_fb0", O_WRONLY);
					//write(blankfd, "GUI_SHOW 0 on", 13); //unblank screen
					//close(blankfd);
					//blankfd = -1;
					loopdelay = 16667; // 60fps
					enableBacklight(1);
					//sleep(1); //give some time to the system to stabilize before resuming the app
					sprintf(sa_cmd, "/usr/bin/killall -SIGCONT %s", sa_running_name);
					system(sa_cmd);
					//paused=0;
				}
			}
		}

		gettimeofday(&tod, NULL);
		now = tod.tv_sec * 1000 + tod.tv_usec / 1000;
		// TODO: check if if necessary
		if (now-then>1000) ignore = 1; // ignore input that arrived during sleep
		
		while(read(input_fd, &ev, sizeof(ev))==sizeof(ev)) {
			if (ignore) continue;
			val = ev.value;

			if (( ev.type != EV_KEY ) || ( val > REPEAT )) continue;
			// printf("code: %i (%i)\n", ev.code, val); fflush(stdout);
			switch (ev.code) {
				case CODE_MENU:
					menu_pressed = val;
				break;
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
		
		if (ignore) {
			menu_pressed = 0;
			up_pressed = up_just_pressed = 0;
			down_pressed = down_just_pressed = 0;
			up_repeat_at = 0;
			down_repeat_at = 0;
		}
		
		if (up_just_pressed || (up_pressed && now>=up_repeat_at)) {
			if (menu_pressed) {
				// printf("brightness up\n"); fflush(stdout);
				val = GetBrightness();
				if (val<BRIGHTNESS_MAX) SetBrightness(++val);
			}
			else {
				// printf("volume up\n"); fflush(stdout);
				val = GetVolume();
				if (val<VOLUME_MAX) SetVolume(++val);
			}
			
			if (up_just_pressed) up_just_pressed = 0;
			else up_repeat_at += 100;
		}
		
		if (down_just_pressed || (down_pressed && now>=down_repeat_at)) {
			if (menu_pressed) {
				// printf("brightness down\n"); fflush(stdout);
				val = GetBrightness();
				if (val>BRIGHTNESS_MIN) SetBrightness(--val);
			}
			else {
				// printf("volume down\n"); fflush(stdout);
				val = GetVolume();
				if (val>VOLUME_MIN) SetVolume(--val);
			}
			
			if (down_just_pressed) down_just_pressed = 0;
			else down_repeat_at += 100;
		}
		
		then = now;
		ignore = 0;
		
		usleep(loopdelay);
	}
}
