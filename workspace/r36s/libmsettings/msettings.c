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


void putFile(char* path, char* contents) {
	FILE* file = fopen(path, "w");
	if (file) {
		fputs(contents, file);
		fclose(file);
	}
}


void putInt(char* path, int value) {
	char buffer[8];
	sprintf(buffer, "%d", value);
	putFile(path, buffer);
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

long map(int x, int in_min, int in_max, int out_min, int out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

///////////////////////////////////////
#define BRIGHTNESS_MAX_PATH "/sys/devices/platform/backlight/backlight/backlight/max_brightness"
#define SETTINGS_VERSION 1
typedef struct Settings {
	int version; // future proofing
	int brightness;
	int headphones; // available?
	int speaker;
	int jack;
	int unused[3]; // for future use
} Settings;
static Settings DefaultSettings = {
	.version = SETTINGS_VERSION,
	.brightness = 3,
	.headphones = 4,
	.speaker = 8,
	.jack = 0,
	.unused = {0,0,0}
};
static Settings *settings;

#define SHM_KEY "/SharedSettings"
static char SettingsPath[256];
static int shm_fd = -1;
static int disp_fd = -1;
static int is_host = 0;
static int shm_size = sizeof(Settings);
static int preinitialized = 0;

static int max_brightness = 0;
static int max_volume = 0;
static int min_volume = 0;

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
			close(fd);
		}
		else {
			// load defaults
			memcpy(settings, &DefaultSettings, shm_size);
		}
	}
	preinitialized = 1;
}
#define MAX_VOLUME_PATH "/MyMinUI/.userdata/r36s/max_volume.txt"
#define MIN_VOLUME_PATH "/MyMinUI/.userdata/r36s/min_volume.txt"
char channel[50];

void InitSettings(void) {
	if (!preinitialized) preInitSettings();
	
	max_brightness = getInt(BRIGHTNESS_MAX_PATH);

	char cmd[256];	
	
	if (access("/dev/input/by-path/platform-fdd40000.i2c-platform-rk805-pwrkey-event",F_OK)==0) {
		//is the rk3566 based rg353v/353p/rgb30
		sprintf(channel, "Master");
	} else {
		//is the r36s
		sprintf(channel, "Playback");
	}

	sprintf(cmd, "amixer sget %s | grep Limits | cut -d'-' -f2 > %s", channel, MAX_VOLUME_PATH);
	system(cmd);
	char *tmpminval = getenv("MIN_VOLUME_VALUE");
	if (tmpminval!=NULL) {
		sprintf(cmd, "echo %s > %s", tmpminval, MIN_VOLUME_PATH);
	} else { //use the minimal raw value set at boot by the system
		sprintf(cmd, "amixer sget %s | grep Limits | cut -d':' -f2 | cut -d'-' -f1 > %s", channel,  MIN_VOLUME_PATH);	
	}
	system(cmd);
	system("sync");
	min_volume = getInt(MIN_VOLUME_PATH);
	max_volume = getInt(MAX_VOLUME_PATH);
	

	printf("brightness: %i \nspeaker: %i\n", settings->brightness, settings->speaker); fflush(stdout);
	
	SetVolume(GetVolume());
	SetBrightness(GetBrightness());
	// system("echo $(< " BRIGHTNESS_PATH ")");
}
static inline void SaveSettings(void) {
	int fd = open(SettingsPath, O_CREAT|O_WRONLY, 0644);
	if (fd>=0) {
		write(fd, settings, shm_size);
		close(fd);
		//sync();
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

	int raw = map(value,0,10,3,max_brightness);
	SetRawBrightness(raw);
	SaveSettings();
}

int GetVolume(void) { // 0-237
	return settings->jack ? settings->headphones : settings->speaker;
}
void SetVolume(int value) {
	if (settings->jack) settings->headphones = value;
	else settings->speaker = value;

	SetRawVolume(value);
	SaveSettings();
}

#define BRIGHTNESS_PATH "/sys/devices/platform/backlight/backlight/backlight/brightness"
void SetRawBrightness(int val) { // 0 - 120

	putInt(BRIGHTNESS_PATH, val);
}

void SetRawVolume(int val) { // 0 - 20
	char cmd[256];
	int rawval = 0;	
	if (val > 0) {
		rawval = map(val, 0, 20, min_volume, max_volume);
	}
	sprintf(cmd, "amixer sset \"%s\"  %d", channel, rawval);
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
	int retvalue = 0;
	char cmd[256];
	if (access("/dev/input/by-path/platform-fdd40000.i2c-platform-rk805-pwrkey-event",F_OK)==0) { 
		retvalue = getInt("/sys/class/extcon/hdmi/cable.0/state");
		int _retvalue = retvalue;
		if (access("/dev/input/by-path/platform-fe5b0000.i2c-event",F_OK)!=0) {
			//is not the rg353v
			_retvalue ^= 1; //invert for some reason, as it seems reversed on rgb30
		}
		if (_retvalue == 0) {		
			sprintf(cmd, "amixer set \"Playback Path\" SPK"); //no HDMI connected
		} else {
			sprintf(cmd, "amixer set \"Playback Path\" HP"); //HDMI connected	
		}
		system(cmd);
	}
	return retvalue;
}
void SetHDMI(int value) {
	// buh
}