// trimuismart
#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <msettings.h>

#include "defines.h"
//#include "platform.h"
#include "api.h"
#include "utils.h"
#include "sunxi_display2.h"

#define ISM22_PATH "/mnt/SDCARD/m21/thisism22"

///////////////////////////////

#define RAW_UP		103 //SDL_SCANCODE_UP
#define RAW_DOWN	108 //SDL_SCANCODE_DOWN
#define RAW_LEFT	105 //SDL_SCANCODE_LEFT
#define RAW_RIGHT	106 //SDL_SCANCODE_RIGHT
#define RAW_A		290 //SDL_SCANCODE_G
#define RAW_B		289 //SDL_SCANCODE_F
#define RAW_X		291 //SDL_SCANCODE_H
#define RAW_Y		288 //SDL_SCANCODE_D
#define RAW_START	297 //SDL_SCANCODE_GRAVE
#define RAW_SELECT	296 //SDL_SCANCODE_APOSTROPHE
#define RAW_MENU	27 //SDL_SCANCODE_RIGHTBRACKET
#define RAW_L1		292 //SDL_SCANCODE_J
#define RAW_L2		294 //SDL_SCANCODE_L
#define RAW_R1		293 //SDL_SCANCODE_K
#define RAW_R2		295 //SDL_SCANCODE_SEMICOLON
#define RAW_PLUS	12 //SDL_SCANCODE_MINUS
#define RAW_MINUS	11 //SDL_SCANCODE_0

#define NUM_INPUTS 2

static int inputs[NUM_INPUTS] = {-1};

void PLAT_initInput(void) {
	LOG_info("PLAT_initInput\n");
	char path[64];
	for (int i=0; i<NUM_INPUTS; i++) {
		if (inputs[i]>=0) {
			close(inputs[i]);
			inputs[i] = -1;
		}
	}
	for (int i=0; i<NUM_INPUTS; i++) {
		sprintf(path, "/dev/input/event%d", i+1);
		inputs[i] = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (inputs[i] < 0) {
			LOG_info("failed to open /dev/input/event%d with error \n", i+1);system("sync");
		}
	}
	LOG_info("PLAT_initInput success!\n");
	fflush(stdout);
}
void PLAT_quitInput(void) {
	LOG_info("PLAT_quitInput\n");
	for (int i=0; i<NUM_INPUTS; i++) {
		if (inputs[i]>=0) {
			close(inputs[i]);
			inputs[i] = -1;
		}
	}
	LOG_info("PLAT_quitInput Success!\n");
	fflush(stdout);
}

// from <linux/input.h> which has BTN_ constants that conflict with platform.h
struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
};
#define EV_KEY			0x01
#define EV_ABS			0x03

static int ism22 = 0;
static int PWR_Pressed = 0;
static int PWR_Actions = 0;
static uint32_t PWR_Tick = 0;
#define PWR_TIMEOUT 2000
int last_dpad_used[2];
int selectstartstatus[3] = {0}; 
int selectstartlaststatus[3] = {0}; 



void PLAT_pollInput(void) {

	if (inputs[0]<0) {
		LOG_info("ERROR as inputs<0\n");
		fflush(stdout);
		return;
	}

	// reset transient state
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;

	uint32_t tick = SDL_GetTicks();
	for (int i=0; i<BTN_ID_COUNT; i++) {
		int _btn = 1 << i;
		if ((pad.is_pressed & _btn) && (tick>=pad.repeat_at[i])) {
			pad.just_repeated |= _btn; // set
			pad.repeat_at[i] += PAD_REPEAT_INTERVAL;
		}
	}
	
	// the actual poll
	int input;
	static struct input_event event;
	for (int i=0; i<NUM_INPUTS; i++) {
		while (read(inputs[i], &event, sizeof(event))==sizeof(event)) {
			if (event.type!=EV_KEY && event.type!=EV_ABS) continue;

			int btn = BTN_NONE;
			int pressed = 0; // 0=up,1=down
			int id = -1;
			int type = event.type;
			int code = event.code;
			int value = event.value;
			//printf("/dev/input/event%d: Type %d event: SCANCODE/AXIS=%i, PRESSED/AXIS_VALUE=%i\n", i,type, code, value);system("sync");
			// TODO: tmp, hardcoded, missing some buttons
			if (type==EV_KEY) {
				if (value>1) continue; // ignore repeats
			
				pressed = value;

				if ((i > 0) || (ism22 == 1)) { //external controllers or is the m22 which doesn't have the menu button
				//special handling as the sjgam external controller does not provide menu button but only select+start, 
				//let's find a way to simulate menu button when select+start is detected
					if (code==RAW_START)	{ btn = BTN_START; 		id = BTN_ID_START; pressed ? selectstartstatus[i]++ : selectstartstatus[i]--; } 
				    if (code==RAW_SELECT)	{ btn = BTN_SELECT; 	id = BTN_ID_SELECT; pressed ? selectstartstatus[i]++ : selectstartstatus[i]--;}
					if (selectstartstatus[i] == 2) {
						if (selectstartlaststatus[i] != selectstartstatus[i]) {
							//LOG_info("SEL+START event detected, generating MENU event stause: %d\n", pressed);fflush(stdout);
							btn = BTN_MENU;  id = BTN_ID_MENU; 
							selectstartlaststatus[i]=1; 
							pad.is_pressed		&= ~BTN_SELECT; // unset
							pad.just_repeated	&= ~BTN_SELECT; // unset	
							pad.is_pressed		&= ~BTN_START; // unset
							pad.just_repeated	&= ~BTN_START; // unset	
							if (pressed){
								PWR_Pressed = 1;
								PWR_Tick = SDL_GetTicks();
								PWR_Actions = 0;		
								//LOG_info("BTN_MENU event detected, status: %d , start timer = %d , actions = %d\n", pressed, PWR_Tick, PWR_Actions);fflush(stdout);
								//printf("pwr pressed\n");				
							} 						
						}
					}
					if ((selectstartstatus[i] == 1) && (selectstartlaststatus[i] == 1)) {
						btn = BTN_MENU; 	
						id = BTN_ID_MENU; 
						selectstartlaststatus[i]=0;
						if ( (PWR_Pressed) && (!PWR_Actions) && (SDL_GetTicks() - PWR_Tick > PWR_TIMEOUT)) {
									//pwr button pressed for more than PWR_TIMEOUT ms (3s default)
									btn = BTN_POWEROFF; 		id = BTN_ID_POWEROFF;
									PWR_Pressed = 0;	
				//					LOG_info("BTN_MENU release event detected, status: %d , elapsed timer = %d , actions = %d\n", pressed, SDL_GetTicks() - PWR_Tick, PWR_Actions);fflush(stdout);
									//printf("pwr released and pwr button event generated\n");			
								} 
					}	
					if (code==RAW_A)		{ btn = BTN_B; 			id = BTN_ID_B; }
					if (code==RAW_B)		{ btn = BTN_A; 			id = BTN_ID_A; }
				} 
				else { //internal controls, standard behavior
					if 		(code==RAW_START)	{ btn = BTN_START; 		id = BTN_ID_START; } 
				    else if (code==RAW_SELECT)	{ btn = BTN_SELECT; 	id = BTN_ID_SELECT; }
					else if (code==RAW_A)		{ btn = BTN_A; 			id = BTN_ID_A; }
					else if (code==RAW_B)		{ btn = BTN_B; 			id = BTN_ID_B; }
				}

				//LOG_info("key event: %i (%i)\n", code,pressed);fflush(stdout);
				     if (code==RAW_UP) 		{ btn = BTN_DPAD_UP; 	id = BTN_ID_DPAD_UP; }
	 			else if (code==RAW_DOWN)	{ btn = BTN_DPAD_DOWN; 	id = BTN_ID_DPAD_DOWN; }
				else if (code==RAW_LEFT)	{ btn = BTN_DPAD_LEFT; 	id = BTN_ID_DPAD_LEFT; }
				else if (code==RAW_RIGHT)	{ btn = BTN_DPAD_RIGHT; id = BTN_ID_DPAD_RIGHT; }
				else if (code==RAW_X)		{ btn = BTN_Y; 			id = BTN_ID_Y; }
				else if (code==RAW_Y)		{ btn = BTN_X; 			id = BTN_ID_X; }
				
				else if (code==RAW_MENU)	{ 
							btn = BTN_MENU; 		id = BTN_ID_MENU; 
							// hack to generate a pwr button
							if (pressed){
								PWR_Pressed = 1;
								PWR_Tick = SDL_GetTicks();
								PWR_Actions = 0;		
								//printf("pwr pressed\n");				
							} else {						
								if ( (PWR_Pressed) && (!PWR_Actions) && (SDL_GetTicks() - PWR_Tick > PWR_TIMEOUT)) {
									//pwr button pressed for more than PWR_TIMEOUT ms (3s default)
									btn = BTN_POWEROFF; 		id = BTN_ID_POWEROFF;
									PWR_Pressed = 0;	
									//printf("pwr released and pwr button event generated\n");			
								} 
							}					
					}
				else if (code==RAW_L1)		{ btn = BTN_L1; 		id = BTN_ID_L1; }
				else if (code==RAW_L2)		{ btn = BTN_L2; 		id = BTN_ID_L2; }
				else if (code==RAW_R1)		{ btn = BTN_R1; 		id = BTN_ID_R1; }
				else if (code==RAW_R2)		{ btn = BTN_R2; 		id = BTN_ID_R2; }
				else if (code==RAW_PLUS)	{ btn = BTN_PLUS; 		id = BTN_ID_PLUS; }
				else if (code==RAW_MINUS)	{ btn = BTN_MINUS; 		id = BTN_ID_MINUS; }
			}
			if (type==EV_ABS) {
				if (code==1) {
					if (value==0)	{ last_dpad_used[1] = 0; pressed = 1; btn = BTN_DPAD_UP; 	id = BTN_ID_DPAD_UP; }
					if (value==255)	{ last_dpad_used[1] = 255; pressed = 1; btn = BTN_DPAD_DOWN; 	id = BTN_ID_DPAD_DOWN; }
					if (value==128)	{ pressed = 0; 
									  if (last_dpad_used[1] == 0) { btn = BTN_DPAD_UP; 	id = BTN_ID_DPAD_UP; }
									  else { btn = BTN_DPAD_DOWN; 	id = BTN_ID_DPAD_DOWN; }
					}
				}
				if (code==0) {
					if (value==0)	{ last_dpad_used[0] = 0; pressed = 1; btn = BTN_DPAD_LEFT; 	id = BTN_ID_DPAD_LEFT; }
					if (value==255)	{ last_dpad_used[0] = 255; pressed = 1; btn = BTN_DPAD_RIGHT; id = BTN_ID_DPAD_RIGHT; }
					if (value==128)	{ pressed = 0; 
									if (last_dpad_used[0] == 0) { btn = BTN_DPAD_LEFT; 	id = BTN_ID_DPAD_LEFT; }
									else { btn = BTN_DPAD_RIGHT; 	id = BTN_ID_DPAD_RIGHT; }
					}
				}
			}
			if ((btn!=BTN_NONE)&&(btn!=BTN_MENU)) {
				//LOG_info("Not BTN_MENU event detected at %d, btn = %d\n", SDL_GetTicks(),btn);fflush(stdout);
				PWR_Actions = 1;
			}
			if (btn==BTN_NONE) continue;

			if (!pressed) {
				pad.is_pressed		&= ~btn; // unset
				pad.just_repeated	&= ~btn; // unset
				pad.just_released	|= btn; // set
			}
			else if ((pad.is_pressed & btn)==BTN_NONE) {
				pad.just_pressed	|= btn; // set
				pad.just_repeated	|= btn; // set
				pad.is_pressed		|= btn; // set
				pad.repeat_at[id]	= tick + PAD_REPEAT_DELAY;
			}
		}
	}
}
		



int PLAT_shouldWake(void) {
	static struct input_event event;
	if (inputs[0] >= 0) {
		while (read(inputs[0], &event, sizeof(event))==sizeof(event)) {
		if (event.type==EV_KEY && (event.code==RAW_MENU || ( ism22 && event.code==RAW_SELECT)) && event.value==0) {
			return 1;
		}
	}	
	return 0;
	}
	
}

static struct VID_Context {
	int fdfb; // /dev/fb0 handler
	int dispfd; // /dev/fb1 handler
	struct fb_fix_screeninfo finfo;  //fixed fb info
	struct fb_var_screeninfo vinfo;  //adjustable fb info
	void *fbmmap[2]; //mmap address of the framebuffer
	SDL_Surface* screen;  //swsurface to let sdl thinking it's the screen
	SDL_Surface* screen2;  //used to apply screen_effect
	SDL_Surface *screengame;  //used to apply screen_effect
	int linewidth;
	int screen_size;
	int width;  //current width 
	int height; // current height
	int pitch;  //sdl bpp
	
	int sharpness; //let's see if it works
	int rotate;
	int rotategame;
	int page;
	int numpages;
	uint32_t offset;
	SDL_Rect targetRect;
	int renderingGame;
} vid;

void pan_display(int page){
	vid.vinfo.yoffset = vid.vinfo.yres_virtual/2 * page;
	//vid.vinfo.yoffset = 0;
	ioctl(vid.fdfb, FBIOPAN_DISPLAY, &vid.vinfo);
}

void get_fbinfo(void){
    ioctl(vid.fdfb, FBIOGET_FSCREENINFO, &vid.finfo);
    ioctl(vid.fdfb, FBIOGET_VSCREENINFO, &vid.vinfo);
	
    fprintf(stdout, "Fixed screen informations\n"
		"-------------------------\n"
		"Id string: %s\n"
		"FB start memory: %p\n"
		"FB memory size: %d\n"
		"FB LineLength: %d\n"
		"FB mmio_start: %p\n"
		"FB mmio_len: %d\n",
		vid.finfo.id, (void *)vid.finfo.smem_start, vid.finfo.smem_len,vid.finfo.line_length, (void *)vid.finfo.mmio_start, vid.finfo.mmio_len);

	fprintf(stdout, "Variable screen informations\n"
		"----------------------------\n"
		"xres: %d\n"
		"yres: %d\n"
		"xres_virtual: %d\n"
		"yres_virtual: %d\n"
		"bits_per_pixel: %d\n\n"
		"RED: L=%d, O=%d\n"
		"GREEN: L=%d, O=%d\n"
		"BLUE: L=%d, O=%d\n"            
		"ALPHA: L=%d, O=%d\n\n"

		"width: %d\n"
		"height: %d\n"
		"pixclock: %d\n"
		"left_margin: %d\n"
		"right_margin: %d\n"
		"upper_margin: %d\n"
		"lower_margin: %d\n"
		"hsync_len: %d\n"
		"vsync_len: %d\n"
		"sync: %d\n"
		"vmode: %d\n"
		"rotate: %d\n"
		"colorspace: %d\n",
		vid.vinfo.xres, vid.vinfo.yres, vid.vinfo.xres_virtual,
		vid.vinfo.yres_virtual, vid.vinfo.bits_per_pixel,
		vid.vinfo.red.length, vid.vinfo.red.offset,
		vid.vinfo.green.length,vid.vinfo.green.offset,
		vid.vinfo.blue.length,vid.vinfo.blue.offset,
		vid.vinfo.transp.length,vid.vinfo.transp.offset,
		vid.vinfo.width, vid.vinfo.height,
		vid.vinfo.pixclock,
		vid.vinfo.left_margin, vid.vinfo.right_margin,
		vid.vinfo.upper_margin, vid.vinfo.lower_margin,
		vid.vinfo.hsync_len, vid.vinfo.vsync_len
		,vid.vinfo.sync, vid.vinfo.vmode, vid.vinfo.rotate, vid.vinfo.colorspace
	);

    //fprintf(stdout, "PixelFormat is %d\n", vinfo.pixelformat);
    fflush(stdout);
}

void set_fbinfo(void){

	int i = ioctl(vid.fdfb, FBIOPUT_VSCREENINFO, &vid.vinfo);
	if (i<0) {
		fprintf(stdout, "FBIOPUT_VSCREENINFO failed with error %s\n", strerror(errno));
	}
}

int getHDMIStatus(void) {	
	int retvalue = 0;
	FILE * __stream = fopen("/sys/class/extcon/extcon0/state","r");
    if (__stream == (FILE *)0x0) {
      retvalue = 0;
    }
    else {
	  char acStack_128 [260];
      memset(acStack_128,0,0x100);
      fread(acStack_128,0x100,1,__stream);
	  
      char *pcVar10 = strstr(acStack_128,"HDMI=1");
      if (pcVar10 == (char *)0x0) {
        retvalue = 0;
      }
      else {
        retvalue = 1;
      }      
    }
	fclose(__stream);
	return retvalue;
}

disp_layer_config config;
static int platform_layer_id = -1;
int swap_buffers_init(void){
	memset(&config, 0, sizeof(config));
	config.layer_id = 0;
	config.channel = 0;
    config.enable = 1;
	config.info.fb.align[0] = 4;//bytes
    config.info.mode = LAYER_MODE_BUFFER;
    config.info.zorder = 0; // In primo piano
    config.info.alpha_mode = 1;
    config.info.alpha_value = 0xff;
    config.info.fb.format = DISP_FORMAT_ARGB_8888;
	config.info.fb.flags = DISP_BF_NORMAL;
	config.info.fb.scan = DISP_SCAN_PROGRESSIVE;
    config.info.fb.size[0].width = GAME_WIDTH;
    config.info.fb.size[0].height = GAME_HEIGHT;
    config.info.fb.crop.x = 0;
    config.info.fb.crop.y = 0;
    config.info.fb.crop.width = (unsigned long long)(GAME_WIDTH) << 32;
    config.info.fb.crop.height = (unsigned long long)(GAME_HEIGHT) << 32;
    config.info.screen_win.x = 0;
    config.info.screen_win.y = 0;
    config.info.screen_win.width = GAME_WIDTH;
    config.info.screen_win.height = GAME_HEIGHT;
	return 0;
}
// Funzione per swappare i buffer
int firstswap = 1;
void swap_buffers(int page)
{
	unsigned long arg[3];
 //   swap_buffers_init();
	// The Sunxi driver expects physical framebuffer addresses (smem_start),
	// not virtual mmap pointers. Use fb fixed info smem_start + page offset.
//	if (vid.fdfb >= 0) {
//		config.info.fb.addr[0] = (uintptr_t)vid.finfo.smem_start + ((uintptr_t)page * (uintptr_t)vid.offset);
//	} else {
		config.info.fb.addr[0] = (uintptr_t)vid.fbmmap[page];
//	}

	arg[0] = 0; // screen 0 (fb id)
	arg[1] = (unsigned long)&config;
	arg[2] = 1; // one layer
	int ret = -1;
	if (vid.dispfd >= 0) {
		ret = ioctl(vid.dispfd, DISP_LAYER_SET_CONFIG, (void*)arg);
		if (firstswap == 1) {
			firstswap = 0;
			LOG_info("First swap_buffers completed\n");
			LOG_info("swap_buffers: page=%d layer=%d platform_layer_id=%d ioctl_ret=%d\n", page, config.layer_id, platform_layer_id, ret);
			LOG_info("swap_buffers: fb_paddr=0x%08lx fb_mmap=%p finfo.smem_start=0x%08lx offset=%u\n", (unsigned long)config.info.fb.addr[0], vid.fbmmap[page], (unsigned long)vid.finfo.smem_start, (unsigned)vid.offset);
			fflush(stdout);
		}
	}
		
	//LOG_info("Swap_buffers retvalue = %d\n", ret);fflush(stdout);
}

int cpufreq_menu,cpufreq_game,cpufreq_perf,cpufreq_powersave,cpufreq_max;
SDL_Surface * page[2];
SDL_Surface* PLAT_initVideo(void) {

	//looks for environment cpu frequencies
	cpufreq_menu = atoi(getenv("CPU_SPEED_MENU"));
	LOG_info("CPU_SPEED_MENU = %d\n", cpufreq_menu);
	cpufreq_powersave= atoi(getenv("CPU_SPEED_POWERSAVE"));
	LOG_info("CPU_SPEED_POWERSAVE = %d\n", cpufreq_powersave);
	cpufreq_game = atoi(getenv("CPU_SPEED_GAME"));	
	LOG_info("CPU_SPEED_GAME = %d\n", cpufreq_game);
	cpufreq_perf = atoi(getenv("CPU_SPEED_PERF"));
	LOG_info("CPU_SPEED_PERF = %d\n", cpufreq_perf);
	cpufreq_max = atoi(getenv("CPU_SPEED_MAX"));
	LOG_info("CPU_SPEED_MAX = %d\n", cpufreq_max);

	ism22 = 0;

	if (exists(SYSTEM_PATH "/menumissing.txt")) {
		unlink(SYSTEM_PATH "/menumissing.txt");
	}

	if (exists(ISM22_PATH)) {
		ism22 = 1;
		//crrate the file menumissing.txt
		int tmpfd = open(SYSTEM_PATH "/menumissing.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
		if (tmpfd >= 0) {
			close(tmpfd);
		}
	}

	vid.fdfb = open("/dev/fb0", O_RDWR);
	LOG_info("Opened /dev/fb0 with fd %d\n", vid.fdfb);fflush(stdout);
	if (vid.fdfb < 0) {
		LOG_info("Error opening /dev/fb0\n");
		fflush(stdout);
		return NULL;
	}
	vid.dispfd = open("/dev/disp", O_RDWR);
	LOG_info("Opened /dev/disp with fd %d\n", vid.dispfd);fflush(stdout);
	if (vid.dispfd < 0) {
		LOG_info("Error opening /dev/disp\n");
		fflush(stdout);
		return NULL;
	}

	
	//system("cat /sys/class/disp/disp/attr/sys > /mnt/SDCARD/sysA.txt");
	int w,h,p,hz = 0;
	if (getHDMIStatus() || (ism22)) {
		char hdmimode[64];
		w = _HDMI_WIDTH;
		h = _HDMI_HEIGHT;
		p = _HDMI_PITCH;
		if (exists(CUSTOM_HDMI_SETTINGS_PATH)){
			getFile(CUSTOM_HDMI_SETTINGS_PATH,hdmimode,sizeof(hdmimode));
			if (getHdmiModeValues(hdmimode, &w, &h, &hz) == -1) {
				LOG_info("HDMI Custom Mode load failed\n");fflush(stdout);
				unlink(CUSTOM_HDMI_SETTINGS_PATH);
			} else {
				p = w * 2;
				LOG_info("HDMI Custom Mode detected %dx%dp%d\n", w, h, hz);fflush(stdout);
			}
		} else {
			//the first run write the default values to the custom file.
			sprintf(hdmimode, "%dx%dp%d", _HDMI_WIDTH, _HDMI_HEIGHT, _HDMI_HZ);
			putFile(CUSTOM_HDMI_SETTINGS_PATH,hdmimode);
			LOG_info("Writing default HDMI Mode %dx%dp%d\n", w, h, hz);fflush(stdout);
		}
		if (getenv("COMMANDER_SCREEN_FIX")!=NULL) {
			w = 1024;
			h = 576;
			p = w * 2;
		}
		system("sync");
	} else {
		//is an m21
		w = FIXED_WIDTH;
		h = FIXED_HEIGHT;
		p = FIXED_PITCH;		
	}

	DEVICE_WIDTH = w;
	DEVICE_HEIGHT = h;
	DEVICE_PITCH = p;
	GAME_WIDTH = w;
	GAME_HEIGHT = h;

	
	vid.rotate = ism22 * (1 - getHDMIStatus());   //m21 always 0, m22 is 0 on HDMI and 1 on display
	LOG_info("ism22 = %d, HDMI = %d, rotate = %d\n", ism22, getHDMIStatus(), vid.rotate);fflush(stdout);
	if (vid.rotate % 2 ==1) {
		//is the internal m22 display so the screen is rotated 90 degrees
		GAME_WIDTH = h;
		GAME_HEIGHT = w;
	}
	get_fbinfo();	

	if (exists( ROTATE_SYSTEM_PATH )) {
		vid.rotate = getInt( ROTATE_SYSTEM_PATH ) & 3;
		LOG_info("Detected custom screen orientation: Rotation = %d\n", vid.rotate);fflush(stdout);
	} else {
		//if the file does not exist, we create it with the default value.
		//putInt(ROTATE_SYSTEM_PATH, vid.rotate); wrong for m22
	}
	vid.rotategame = (4-vid.rotate) & 3;

	LOG_info("Use screen orientation: Rotation = %d, Game rotation = %d, ism22 = %d, HDMI = %d, rotate = %d\n", vid.rotate*90, vid.rotategame*90, ism22, getHDMIStatus(), vid.rotate);fflush(stdout);

	LOG_info("DEVICE_WIDTH = %d, DEVICE_HEIGHT = %d, DEVICE_PITCH = %d, GAME_WIDTH = %d, GAME_HEIGHT = %d\n", DEVICE_WIDTH, DEVICE_HEIGHT, DEVICE_PITCH, GAME_WIDTH, GAME_HEIGHT);fflush(stdout);

	/////for m22
	//HDMI ok
	//DEVICE_WIDTH=854;
	//DEVICE_HEIGHT=480;
	//DEVICE_PITCH=854*2;
	//vid.vinfo.xres=854;
    //vid.vinfo.yres=480;
	//vid.rotate=0;

	//display ok
	//DEVICE_WIDTH=854;
	//DEVICE_HEIGHT=480;
	//DEVICE_PITCH=854*2;
	//vid.rotate=1;
    //vid.vinfo.xres=480;
    //vid.vinfo.yres=854;
	
	
	vid.vinfo.xres=GAME_WIDTH;
	vid.vinfo.yres=GAME_HEIGHT;
	vid.vinfo.xoffset=0;
	vid.vinfo.yoffset=0;
	vid.vinfo.xres_virtual=vid.vinfo.xres;
	vid.vinfo.yres_virtual=vid.vinfo.yres*2;
	vid.vinfo.bits_per_pixel=32;	
	//at the beginning set the screen size to 640x480
    set_fbinfo();
	get_fbinfo();

	vid.screen =  SDL_CreateRGBSurface(0, DEVICE_WIDTH, DEVICE_HEIGHT, FIXED_DEPTH, RGBA_MASK_565);
	vid.screengame =  SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, FIXED_DEPTH, RGBA_MASK_565);
	vid.screen2 = SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, FIXED_DEPTH, RGBA_MASK_565); 
	LOG_info("vid.screen: %ix%i\n", vid.screen->w, vid.screen->h);fflush(stdout);
	LOG_info("vid.screengame: %ix%i\n", vid.screengame->w, vid.screengame->h);fflush(stdout);
	LOG_info("vid.screen2: %ix%i\n", vid.screen2->w, vid.screen2->h);fflush(stdout);

//	vid.page = 0;
//	pan_display(vid.page);
//	swap_buffers_init();
//	swap_buffers(vid.page);
	vid.renderingGame = 0;

	
	vid.screen_size = vid.vinfo.yres_virtual * vid.finfo.line_length;
	vid.offset = vid.screen_size/2;
	vid.linewidth = vid.finfo.line_length/(vid.vinfo.bits_per_pixel/8);
	//create a mmap with the maximum available memory, we avoid recreating it during the resize as it is useless and waste of time.
	vid.fbmmap[0] = mmap(NULL, vid.offset, PROT_READ | PROT_WRITE, MAP_SHARED, vid.fdfb, 0);
	if (vid.fbmmap[0] == MAP_FAILED) {
		LOG_info("Error mapping framebuffer device to memory: %s\n", strerror(errno));
		//return NULL;
	}
	/* Map the second page at an offset equal to one page size so the two
	   mappings refer to distinct framebuffer pages. */
	vid.fbmmap[1] = mmap(NULL, vid.offset, PROT_READ | PROT_WRITE, MAP_SHARED, vid.fdfb, 0);
	if (vid.fbmmap[1] == MAP_FAILED) {
		LOG_info("Error mapping framebuffer device to memory: %s\n", strerror(errno));
		//return NULL;
	}
	LOG_info("Address vid.fbmmap[0]: %p\n", vid.fbmmap[0]);fflush(stdout);
	LOG_info("Address vid.fbmmap[1]: %p\n", vid.fbmmap[1]);fflush(stdout);
	swap_buffers_init();
	vid.page = 0;
	swap_buffers(vid.page);
	vid.sharpness = SHARPNESS_SOFT;
//	page[0] = SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, FIXED_DEPTH, RGBA_MASK_565); 
//	page[1] = SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, FIXED_DEPTH, RGBA_MASK_565); 
//	SDL_FillRect(page[0], &(SDL_Rect){0, 0, GAME_WIDTH/2, GAME_HEIGHT}, 0xffff00ff); // TODO: revisit
//	SDL_FillRect(page[1], &(SDL_Rect){GAME_WIDTH/2, 0, GAME_WIDTH/2, GAME_HEIGHT}, 0xff00ffff); // TODO: revisit

	return vid.screen;
}

void PLAT_quitVideo(void) {
	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.screen2);
	SDL_FreeSurface(vid.screengame);

//	SDL_FreeSurface(page[0]);
//	SDL_FreeSurface(page[1]);
	if (vid.fbmmap[0] && vid.fbmmap[0] != MAP_FAILED) munmap(vid.fbmmap[0], vid.offset);
	if (vid.fbmmap[1] && vid.fbmmap[1] != MAP_FAILED) munmap(vid.fbmmap[1], vid.offset);
	if (platform_layer_id >= 0 && vid.dispfd >= 0) {
		uint32_t rel[2];
		rel[0] = 0; /* fb id */
		rel[1] = platform_layer_id;
		ioctl(vid.dispfd, DISP_CMD_LAYER_RELEASE, &rel);
		LOG_info("Released display layer %d\n", platform_layer_id);
		platform_layer_id = -1;
	}

	if (vid.fdfb >= 0) close(vid.fdfb);
	if (vid.dispfd >= 0) close(vid.dispfd);
}

void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_FillRect(vid.screen, NULL, 0); // TODO: revisit
	SDL_FillRect(vid.screen2, NULL, 0);
	SDL_FillRect(vid.screengame, NULL, 0);
}

void PLAT_clearAll(void) {
	SDL_FillRect(vid.screen, NULL, 0); // TODO: revisit
	SDL_FillRect(vid.screen2, NULL, 0);
	SDL_FillRect(vid.screengame, NULL, 0);
	memset(vid.fbmmap[0], 0, vid.offset);
	memset(vid.fbmmap[1], 0, vid.offset);
}

void PLAT_setVsync(int vsync) {
	// buh
}

static int next_effect = EFFECT_NONE;
static int effect_type = EFFECT_NONE;

SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
//	resizeVideo(w,h,p,0);
	return vid.screen;
}

SDL_Surface* PLAT_resizeVideoGame(int w, int h, int p) {
//	resizeVideo(w,h,p,1);
	return vid.screen;
}
void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// buh
}
void PLAT_setNearestNeighbor(int enabled) {
	// buh
}
void PLAT_setSharpness(int sharpness) {
	// force effect to reload
	// on scaling change
	if (effect_type>=EFFECT_NONE) next_effect = effect_type;
	effect_type = -1;
}

void PLAT_setEffect(int effect) {
	next_effect = effect;
}
void PLAT_vsync(int remaining) {
	if (remaining>0) {
		usleep(remaining*1000);
	} else {
		pan_display(0);
	}
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	return NULL;
}


void PLAT_blitRenderer(GFX_Renderer* renderer) {
	if (effect_type!=next_effect) {
		effect_type = next_effect;
	}

	if (effect_type==EFFECT_LINE) {
		scale_mat_nearest_lut_rgb565_neon_fast_xy_pitch(renderer->src_surface->pixels, renderer->src_surface->w, renderer->src_surface->h, renderer->src_surface->pitch, vid.screen2->pixels, vid.screen2->w, vid.screen2->h, vid.screen2->pitch, renderer->dst_x, renderer->dst_y,renderer->dst_w, renderer->dst_h);
		scale1x_line(vid.screen2->pixels, vid.screengame->pixels, vid.screen2->w, vid.screen2->h, vid.screen2->pitch, vid.screengame->w, vid.screengame->h, vid.screengame->pitch);
	}
	else if (effect_type==EFFECT_GRID) {
		scale_mat_nearest_lut_rgb565_neon_fast_xy_pitch(renderer->src_surface->pixels, renderer->src_surface->w, renderer->src_surface->h, renderer->src_surface->pitch, vid.screen2->pixels, vid.screen2->w, vid.screen2->h, vid.screen2->pitch, renderer->dst_x, renderer->dst_y,renderer->dst_w, renderer->dst_h);
		scale1x_grid(vid.screen2->pixels, vid.screengame->pixels, vid.screen2->w, vid.screen2->h, vid.screen2->pitch, vid.screengame->w, vid.screengame->h, vid.screengame->pitch);
	}
	else {
		scale_mat_nearest_lut_rgb565_neon_fast_xy_pitch(renderer->src_surface->pixels, renderer->src_surface->w, renderer->src_surface->h, renderer->src_surface->pitch, vid.screengame->pixels, vid.screengame->w, vid.screengame->h, vid.screengame->pitch, renderer->dst_x, renderer->dst_y,renderer->dst_w, renderer->dst_h);
		//SDL_SoftStretch(renderer->src_surface, NULL, vid.screen, &(SDL_Rect){renderer->dst_x,renderer->dst_y,renderer->dst_w,renderer->dst_h});
	}
	vid.targetRect.x = renderer->dst_x;
	vid.targetRect.y = renderer->dst_y;
	vid.targetRect.w = renderer->dst_w;
	vid.targetRect.h = renderer->dst_h;
	vid.renderingGame = 1;
}

void PLAT_pan(void) {
//	pan_display(vid.page);
//	if (vid.numpages == 2) {
//		vid.page ^= 1;
//	}
}


void PLAT_flip(SDL_Surface* IGNORED, int sync) { //this rotates minarch menu + minui + tools
//	uint32_t now = SDL_GetTicks();
//	vid.page=0;
	vid.page ^= 1;
	if (!vid.renderingGame) {
		vid.targetRect.x = 0;
		vid.targetRect.y = 0;
		vid.targetRect.w = vid.screen->w;
		vid.targetRect.h = vid.screen->h;
//		vid.page = 0;
		if (vid.rotate == 0)
		{
			// 90 Rotation
			FlipRotate000(vid.screen, vid.fbmmap[vid.page],vid.linewidth, vid.targetRect);
		}
		if (vid.rotate == 1)
		{
			// 90 Rotation
			FlipRotate090(vid.screen, vid.fbmmap[vid.page],vid.linewidth, vid.targetRect);
		}
		if (vid.rotate == 2)
		{
			// 180 Rotation
			FlipRotate180(vid.screen, vid.fbmmap[vid.page],vid.linewidth, vid.targetRect);
		}
		if (vid.rotate == 3)
		{
			// 270 Rotation
			FlipRotate270(vid.screen, vid.fbmmap[vid.page],vid.linewidth, vid.targetRect);
		}
		
	} else {
		//maybe one Day I'll find the time to investigate on why neon copy functions aren't working here
		// No Rotation
			FlipRotate000(vid.screengame, vid.fbmmap[vid.page],vid.linewidth, vid.targetRect);
	//		FlipRotate000(page[vid.page], vid.fbmmap[vid.page],vid.linewidth, vid.targetRect);
//		if (vid.targetRect.w == vid.screen->w) {
//			//fullscreen
//			pixman_composite_src_0565_8888_asm_neon(vid.screengame->w, vid.screengame->h, vid.fbmmap[vid.page], vid.screengame->pitch/2, vid.screengame->pixels, vid.screengame->pitch/2);
//		} else {
//			//window
//			convert_rgb565_to_argb8888_neon_rect(vid.screengame->pixels, vid.fbmmap[vid.page], vid.screengame->w, vid.screengame->w, vid.targetRect.x, vid.targetRect.y, vid.targetRect.w, vid.targetRect.h);
//		}
		
//		swap_buffers(vid.page);
//		vid.page ^= 1;
		
	}	
	vid.renderingGame = 0;
	swap_buffers(vid.page);
//	if (sync) pan_display(0);
	//LOG_info("Total Flip TOOK: %imsec, Draw TOOK: %imsec\n", SDL_GetTicks()-now, now2-now);fflush(stdout);
}
///////////////////////////////

// TODO: 
#define OVERLAY_WIDTH PILL_SIZE // unscaled
#define OVERLAY_HEIGHT PILL_SIZE // unscaled
#define OVERLAY_BPP 4
#define OVERLAY_DEPTH 16
#define OVERLAY_PITCH (OVERLAY_WIDTH * OVERLAY_BPP) // unscaled
#define OVERLAY_RGBA_MASK 0x00ff0000,0x0000ff00,0x000000ff,0xff000000 // ARGB
static struct OVL_Context {
	SDL_Surface* overlay;
} ovl;

SDL_Surface* PLAT_initOverlay(void) {
	ovl.overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, SCALE2(OVERLAY_WIDTH,OVERLAY_HEIGHT),OVERLAY_DEPTH,OVERLAY_RGBA_MASK);
	return ovl.overlay;
}
void PLAT_quitOverlay(void) {
	if (ovl.overlay) SDL_FreeSurface(ovl.overlay);
}
void PLAT_enableOverlay(int enable) {

}

///////////////////////////////

long map(int x, int in_min, int in_max, int out_min, int out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void PLAT_getBatteryStatus(int* is_charging, int* charge) {
/*	BATTERY info:
/sys/class/gpadc/charge -> 0 not charging
                        -> 1 charger connected

battery level info:
/sys/class/gpadc/data  min value 1030 (it is checked systemwise, whne below 1030 the /usr/bin/lowpower is launched)
need some work to get the maximum (full charge) value
after some test the max battery level seems 1215	*/
	int charge_fd = open("/sys/class/gpadc/charge", O_RDONLY);
	if (charge_fd == -1) {
		*is_charging = 0;
	} else {
		char buf[8];
		read(charge_fd, buf, 2);
		*is_charging = atoi(buf);
	}
	close(charge_fd);
	int i;
	charge_fd = open("/sys/class/gpadc/data", O_RDONLY);
	if (charge_fd == -1) {
		i = 1000;
	} else {
		char buf[8];//
		read(charge_fd, buf, 8);
		i = atoi(buf);
	}
	close(charge_fd);
	i = MIN(i,1200);
	*charge = map(i,1030,1200,0,100);
	//LOG_info("Raw battery: %i -> %d%%\n", i, *charge);
	// worry less about battery and more about the game you're playing
	/*
	     if (i>1200) *charge = 100;
	else if (i>1175) *charge =  80;
	else if (i>1150) *charge =  60;
	else if (i>1100) *charge =  40;
	else if (i>1050) *charge =  20;
	else           *charge =  10;
	*/
}

#define DISP_LCD_BACKLIGHT_ENABLE   0x104
#define DISP_LCD_BACKLIGHT_DISABLE  0x105

void rawBacklight(int value) {
	unsigned int args[4] = {0};
	args[1] = value;
	int disp_fd=open("/dev/disp",O_RDWR);
	if (value>0){
		ioctl(disp_fd, DISP_LCD_BACKLIGHT_ENABLE, args);
	} else {
		ioctl(disp_fd, DISP_LCD_BACKLIGHT_DISABLE, args);
	}	
	close(disp_fd);
}


void PLAT_enableBacklight(int enable) {
    if (enable>0){
		SetBrightness(GetBrightness());
        rawBacklight(1);		
    } else {
		rawBacklight(0);
	//	SetRawBrightness(254);
    }	
}

void PLAT_powerOff(void) {
	//system("leds_on");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(1);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();	
	
	touch("/tmp/poweroff");
	exit(0);
}

///////////////////////////////

#define GOVERNOR_CPUSPEED_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
#define GOVERNOR_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"

void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
/*
Available frequency
480000
720000
912000
1008000
1104000
1200000
*/	
	switch (speed) {
		case CPU_SPEED_MENU: 		freq = cpufreq_menu; break;
		case CPU_SPEED_POWERSAVE:	freq = cpufreq_powersave; break;
		case CPU_SPEED_NORMAL: 		freq = cpufreq_game ; break;
		case CPU_SPEED_PERFORMANCE: freq = cpufreq_perf ; break;
		case CPU_SPEED_MAX:			freq = cpufreq_max ; break;	
	}
	if (freq) {
		putFile(GOVERNOR_PATH, "userspace");
		putInt(GOVERNOR_CPUSPEED_PATH, freq);
		LOG_info("Set CPU speed to %i\n", freq);
		cur_cpu_freq = freq/1000;
	}
	
}

void PLAT_setRumble(int effect, int strength)
{
	// buh
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	return "SJGAM M21";
}

int PLAT_isOnline(void) {
	return 0;
}

int PLAT_getNumProcessors(void) {
	//the core can be deactivated by command line
	return sysconf(_SC_NPROCESSORS_ONLN);
}

uint32_t PLAT_screenMemSize(void) {
	return vid.screen_size;
}

void PLAT_getAudioOutput(void){
	LOG_info("Check for Videooutput\n");
	if (getHDMIStatus()) {
		setenv("AUDIODEV","audioHDMI",1);
		LOG_info("VideoOutput audioHDMI\n");
	} else {
		setenv("AUDIODEV","default",1);
		LOG_info("VideoOutput default\n");
	}
}


int PLAT_getScreenRotation(int game) {
	if (game) {
		return vid.rotategame;
	} else {
		return vid.rotate;
	}
}

SDL_Surface* PLAT_getScreenGame(void) {
	return vid.screengame;
}