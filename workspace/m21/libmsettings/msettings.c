#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <errno.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <sys/ioctl.h>
#include <string.h>

//#include "sunxi_display2.h"
#include "msettings.h"
#define ISM22_PATH "/mnt/SDCARD/m21/thisism22"

///////////////////////////////////////

#define SETTINGS_VERSION 1
typedef struct Settings {
	int version; // future proofing
	int brightness;
	int headphones; // available?
	int speaker;
	int unused[2]; // for future use
	// NOTE: doesn't really need to be persisted but still needs to be shared
	int jack; 
} Settings;
static Settings DefaultSettings = {
	.version = SETTINGS_VERSION,
	.brightness = 3,
	.headphones = 4,
	.speaker = 8,
	.jack = 0,
};
static Settings *settings;

#define SHM_KEY "/SharedSettings"
static char SettingsPath[256];
static int shm_fd = -1;
static int disp_fd = -1;
static int is_host = 0;
static int shm_size = sizeof(Settings);
static int preinitialized = 0;
static int this_ism22 = 0;

void preInitSettings(void) {
	//printf("InitSettings\n");system("sync");
	sprintf(SettingsPath, "%s/msettings.bin", getenv("USERDATA_PATH"));
	shm_fd = shm_open(SHM_KEY, O_RDWR | O_CREAT | O_EXCL, 0644); // see if it exists
	if (shm_fd==-1 && errno==EEXIST) { // already exists
		//puts("Settings client");
		shm_fd = shm_open(SHM_KEY, O_RDWR, 0644);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	}
	else { // host
		//puts("Settings host"); // should always be keymon
		is_host = 1;
		// we created it so set initial size and populate
		ftruncate(shm_fd, shm_size);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
		
		int fd = open(SettingsPath, O_RDONLY);
		if (fd>=0) {
			read(fd, settings, shm_size);
			// TODO: use settings->version for future proofing?
			close(fd);
		}
		else {
			// load defaults
			memcpy(settings, &DefaultSettings, shm_size);
		}
		
		// these shouldn't be persisted
		// settings->jack = 0;
		// settings->hdmi = 0;
	}
	
	if (access(ISM22_PATH, F_OK)==0) {
		this_ism22 = 1;
	}
	preinitialized = 1;
}
void InitSettings(void) {
	if (!preinitialized) preInitSettings();
	
	printf("brightness: %i \nspeaker: %i\n", settings->brightness, settings->speaker); fflush(stdout);
	
	SetVolume(GetVolume());
	SetBrightness(GetBrightness());
	// system("echo $(< " BRIGHTNESS_PATH ")");
}
static inline void SaveSettings(void) {
	shm_fd = open(SettingsPath, O_WRONLY, 0644);
	if (shm_fd>=0) {
		write(shm_fd, &settings, shm_size);
		
	}
}
void QuitSettings(void) {
	munmap(settings, shm_size);
	if (is_host) shm_unlink(SHM_KEY);
}

int GetBrightness(void) { // 0-10
	return settings->brightness;
}
void SetBrightness(int value) {
	settings->brightness = value;

	int raw;
	switch (value) {
		case  0: raw =  235; break;
		case  1: raw =  215; break;
		case  2: raw =  195; break;
		case  3: raw =  175; break;
		case  4: raw =  155; break;
		case  5: raw =  135; break;
		case  6: raw =  115; break;
		case  7: raw =  95; break;
		case  8: raw = 75; break;
		case  9: raw = 55; break;
		case 10: raw = 25; break;
	}
	if (this_ism22) {
		//ism22 is inverted
		raw = 255 - raw;
	}
	SetRawBrightness(raw);
	SaveSettings();
}

int GetVolume(void) { // 0-7
	return settings->jack ? settings->headphones : settings->speaker;
}
void SetVolume(int value) {
	if (settings->jack) settings->headphones = value;
	else settings->speaker = value;

	SetRawVolume(value);
	SaveSettings();
}

// from sunxi_display2.h
#define DISP_LCD_SET_BRIGHTNESS 0x102
#define DISP_LCD_GET_BRIGHTNESS 0x103

void SetRawBrightness(int val) { // 0 - 255
	unsigned int args[4] = {0};
	args[1] = val;
	disp_fd=open("/dev/disp",O_RDWR);
	if (ioctl(disp_fd, DISP_LCD_SET_BRIGHTNESS, args) <0)
	{
		printf("ioctl DISP_LCD_SET_BRIGHTNESS failed\n");
	} 
	close(disp_fd);
	// printf("SetRawBrightness(%i)\n", val); fflush(stdout);
	
}

long map(int x, int in_min, int in_max, int out_min, int out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
void SetRawVolume(int val) { // 0 - 20
	char cmd[256];	
	int rawval = map(val, 0, 20, 0, 7);
	//if (rawval == 0) {
		//sprintf(cmd, "amixer sset 'Headphone' mute && amixer sset 'Headphone volume' %d", 2);
	//	sprintf(cmd, "amixer sset 'Headphone volume' %d", 2);
	//} else	{
		//sprintf(cmd, "amixer sset 'Headphone' unmute && amixer sset 'Headphone volume' %d", rawval-1);
		sprintf(cmd, "amixer sset 'Headphone' unmute && amixer sset 'Headphone volume' %d", rawval);
	//}
	system(cmd);
	printf("SetRawVolume(%i->%i) \"%s\"\n", val,rawval,cmd); fflush(stdout);
}

// monitored and set by thread in keymon
int GetJack(void) {
	return settings->jack;
}
void SetJack(int value) {
	// printf("SetJack(%i)\n", value); fflush(stdout);
	settings->jack = value;
	SetVolume(GetVolume());
}

int GetHDMI(void) {
	return 0;
}
void SetHDMI(int value) {
	// buh
}