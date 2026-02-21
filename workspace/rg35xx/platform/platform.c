// rg35xx
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <msettings.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"
////////////////////////////////
//test for vsync
struct owlfb_sync_info {
	uint8_t enabled;
	uint8_t disp_id;
	uint16_t reserved2;
};

#define OWL_IOW(num, dtype) _IOW('O', num, dtype)
#define OWLFB_WAITFORVSYNC      OWL_IOW(57, long long)
#define OWLFB_VSYNC_EVENT_EN	OWL_IOW(67, struct owlfb_sync_info)
// end test for VSYNC

///////////////////////////////
#define RAW_UP			0x5A
#define RAW_DOWN		0x5B
#define RAW_LEFT		0x5C
#define RAW_RIGHT		0x5D
#define RAW_SELECT		0x63
#define RAW_START		0x62
#define RAW_A			0x5E
#define RAW_B			0x5F
#define RAW_X			0x60
#define RAW_Y			0x61
#define RAW_L1			0x64
#define RAW_R1			0x65
#define RAW_L2			0x66
#define RAW_R2			0x67
#define RAW_MENU		0x68
#define RAW_POWER		0x74
#define RAW_PLUS		0x6C
#define RAW_MINUS		0x6D

#define INPUT_COUNT 2
static int inputs[INPUT_COUNT];

void PLAT_initInput(void) {
	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // power
	inputs[1] = open("/dev/input/event1", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // controller
}
void PLAT_quitInput(void) {
	for (int i=0; i<INPUT_COUNT; i++) {
		close(inputs[i]);
	}
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

void PLAT_pollInput(void) {
		// reset transient state
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_released_short = BTN_NONE;
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
	for (int i=0; i<INPUT_COUNT; i++) {
		input = inputs[i];
		while (read(input, &event, sizeof(event))==sizeof(event)) {
			if (event.type!=EV_KEY && event.type!=EV_ABS) continue;

			int btn = BTN_NONE;
			int pressed = 0; // 0=up,1=down
			int id = -1;
			int type = event.type;
			int code = event.code;
			int value = event.value;
			
			// TODO: tmp, hardcoded, missing some buttons
			if (type==EV_KEY) {
				if (value>1) continue; // ignore repeats
			
				pressed = value;
				// LOG_info("key event: %i (%i)\n", code,pressed);
				     if (code==RAW_UP) 		{ btn = BTN_DPAD_UP; 	id = BTN_ID_DPAD_UP; }
	 			else if (code==RAW_DOWN)	{ btn = BTN_DPAD_DOWN; 	id = BTN_ID_DPAD_DOWN; }
				else if (code==RAW_LEFT)	{ btn = BTN_DPAD_LEFT; 	id = BTN_ID_DPAD_LEFT; }
				else if (code==RAW_RIGHT)	{ btn = BTN_DPAD_RIGHT; id = BTN_ID_DPAD_RIGHT; }
				else if (code==RAW_A)		{ btn = BTN_A; 			id = BTN_ID_A; }
				else if (code==RAW_B)		{ btn = BTN_B; 			id = BTN_ID_B; }
				else if (code==RAW_X)		{ btn = BTN_X; 			id = BTN_ID_X; }
				else if (code==RAW_Y)		{ btn = BTN_Y; 			id = BTN_ID_Y; }
				else if (code==RAW_START)	{ btn = BTN_START; 		id = BTN_ID_START; }
				else if (code==RAW_SELECT)	{ btn = BTN_SELECT; 	id = BTN_ID_SELECT; }
				else if (code==RAW_MENU)	{ btn = BTN_MENU; 		id = BTN_ID_MENU; }
				else if (code==RAW_L1)		{ btn = BTN_L1; 		id = BTN_ID_L1; }
				else if (code==RAW_L2)		{ btn = BTN_L2; 		id = BTN_ID_L2; }
				else if (code==RAW_R1)		{ btn = BTN_R1; 		id = BTN_ID_R1; }
				else if (code==RAW_R2)		{ btn = BTN_R2; 		id = BTN_ID_R2; }
				else if (code==RAW_PLUS)	{ btn = BTN_PLUS; 		id = BTN_ID_PLUS; }
				else if (code==RAW_MINUS)	{ btn = BTN_MINUS; 		id = BTN_ID_MINUS; }
				else if (code==RAW_POWER)	{ btn = BTN_POWER; 		id = BTN_ID_POWER; }
			}
			
			if (btn==BTN_NONE) continue;
		
			if (!pressed) {
				if (pad.is_pressed & btn) {
					if ((tick - pad.begin_time[id]) > (PAD_REPEAT_DELAY + PAD_REPEAT_INTERVAL)) {
						pad.just_released	|= btn; // set
					} else {
						pad.just_released_short	|= btn; // set
					}
				}
				pad.is_pressed		&= ~btn; // unset
				pad.just_repeated	&= ~btn; // unset
				pad.begin_time[id] = 0;
			}
			else if ((pad.is_pressed & btn)==BTN_NONE) {
				pad.just_pressed	|= btn; // set
				pad.is_pressed		|= btn; // set
				pad.begin_time[id]  = tick;
				pad.repeat_at[id]	= tick + PAD_REPEAT_DELAY;
			}
		}
	}
}

int PLAT_shouldWake(void) {
	int input;
	static struct input_event event;
	for (int i=0; i<INPUT_COUNT; i++) {
		input = inputs[i];
		while (read(input, &event, sizeof(event))==sizeof(event)) {
			if (event.type==EV_KEY && event.code==RAW_POWER && event.value==0) {
				return 1;
			}
		}
	}
	return 0;
}

static struct VID_Context {
	int fdfb; // /dev/fb0 handler
	struct fb_fix_screeninfo finfo;  //fixed fb info
	struct fb_var_screeninfo vinfo;  //adjustable fb info
	void *fbmmap; //mmap address of the framebuffer
	SDL_Surface* screen;  //swsurface to let sdl thinking it's the screen
	SDL_Surface* screen2;  //used to apply screen_effect
	SDL_Surface* screengame;  //used to apply screen_effect
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
	vid.vinfo.yoffset = (vid.vinfo.yres_virtual/2) * page;
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
		vid.vinfo.blue.length,vid.vinfo.blue.offset,
		vid.vinfo.green.length,vid.vinfo.green.offset,
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


void SetHDMI(int value) {
	putInt("/sys/class/backlight/backlight.2/bl_power", value);
	setenv("AUDIODEV", "default", 1);
	//putInt("sys/class/graphics/fb0/mirror_to_hdmi", value);
	if (value == 1) {
		setenv("AUDIODEV", "hdmi", 1);
		//system("cp -f /mnt/sdcard/hdmi.conf /usr/share/alsa/alsa.conf.d/display.conf");system("sync");
	}
	else {
		setenv("AUDIODEV", "default", 1);
		//system("cp -f /mnt/sdcard/display.conf /usr/share/alsa/alsa.conf.d/display.conf");system("sync");
	}
	usleep(500000);
}


int cpufreq_menu,cpufreq_game,cpufreq_perf,cpufreq_powersave,cpufreq_max;

SDL_Surface* PLAT_initVideo(void) {

	//set default cpu frequencies only if defined, useful for cases where the initvideo is reqquested out of the system.
	if (getenv("CPU_SPEED_MENU") != NULL && getenv("CPU_SPEED_POWERSAVE") != NULL && getenv("CPU_SPEED_GAME") != NULL && getenv("CPU_SPEED_PERF") != NULL && getenv("CPU_SPEED_MAX") != NULL) {
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
	} 

	int w,p,h,hz = 0;
	if (GetHDMI()){
		SetHDMI(1);
		vid.fdfb = open("/dev/fb1", O_RDWR);
		char hdmimode[64];
		w = HDMI_WIDTH_;
		h = HDMI_HEIGHT_;
		p = HDMI_PITCH_;
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
			sprintf(hdmimode, "%dx%dp%d", HDMI_WIDTH_, HDMI_HEIGHT_, HDMI_HZ_);
			putFile(CUSTOM_HDMI_SETTINGS_PATH,hdmimode);
		}
	} else {
		SetHDMI(0);
		vid.fdfb = open("/dev/fb0", O_RDWR);
		w = FIXED_WIDTH;
		h = FIXED_HEIGHT;
		p = FIXED_PITCH;
	}

	DEVICE_WIDTH = w;
	DEVICE_HEIGHT = h;
	DEVICE_PITCH = p;
	vid.rotate = 0;
	vid.rotategame = vid.rotate;
	get_fbinfo();	

	if (exists(ROTATE_SYSTEM_PATH)) {
		vid.rotate = getInt(ROTATE_SYSTEM_PATH) &3;
	}
	GAME_WIDTH = DEVICE_WIDTH;
	GAME_HEIGHT = DEVICE_HEIGHT;
	if (vid.rotate % 2 == 1) {
		DEVICE_WIDTH = h;
		DEVICE_HEIGHT = w;
		vid.rotategame = (4-vid.rotate)&3;
	}
	

    vid.vinfo.xres=w;
    vid.vinfo.yres=h;
	vid.vinfo.xres_virtual = vid.vinfo.xres;
	vid.vinfo.yres_virtual = vid.vinfo.yres * 2;
	vid.vinfo.bits_per_pixel=32;
	//at the beginning set the screen size to 640x480
    set_fbinfo();
	get_fbinfo();
	LOG_info("DEVICE_WIDTH=%d, DEVICE_HEIGHT=%d, DEVICE_PITCH=%d Virtual %dx%d\n", DEVICE_WIDTH, DEVICE_HEIGHT, DEVICE_PITCH, vid.vinfo.xres_virtual, vid.vinfo.yres_virtual);fflush(stdout);
	FIXED_SCALE = _FIXED_SCALE;
	InitAssetRects();
	struct owlfb_sync_info sinfo;
	sinfo.enabled = 1;
	sinfo.disp_id = 2;
	if (ioctl(vid.fdfb, OWLFB_VSYNC_EVENT_EN, &sinfo)<0) LOG_error("VSYNC_EVENT_EN failed %s\n",strerror(errno));
	
	vid.vinfo.xoffset=0;
	vid.vinfo.yoffset=0;
	vid.page = 0;
	pan_display(vid.page);

	vid.offset = vid.vinfo.yres_virtual/2 * vid.finfo.line_length;
	vid.screen_size = vid.offset*2;
	vid.linewidth = vid.finfo.line_length/(vid.vinfo.bits_per_pixel/8);

	vid.screen =  SDL_CreateRGBSurface(0, DEVICE_WIDTH, DEVICE_HEIGHT, FIXED_DEPTH, RGBA_MASK_565);
	vid.screen2 = SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, FIXED_DEPTH, RGBA_MASK_565); 
	vid.screengame =  SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, FIXED_DEPTH, RGBA_MASK_565);

	//create a mmap with the maximum available memory, we avoid recreating it during the resize as it is useless and waste of time.
    vid.fbmmap = mmap(NULL, vid.screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, vid.fdfb, 0);
	vid.renderingGame = 0;
	
	vid.sharpness = SHARPNESS_SOFT;
	return vid.screen;
}

void PLAT_quitVideo(void) {
	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.screen2);
	SDL_FreeSurface(vid.screengame);
	munmap(vid.fbmmap, 0);	
    close(vid.fdfb);
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
	memset(vid.fbmmap, 0, vid.screen_size);
}

void PLAT_setVsync(int vsync) {
	// buh
}

//static int hard_scale = 4; // TODO: base src size, eg. 160x144 can be 4
static void resizeVideo(int w, int h, int p, int game) {
	// buh
}

static int next_effect = EFFECT_NONE;
static int effect_type = EFFECT_NONE;

SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
//	resizeVideo(w,h,p,0);
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
	if (remaining > 0) {
		usleep(remaining*1000);
	} else {
		int res = 0;
		ioctl(vid.fdfb, OWLFB_WAITFORVSYNC, &res);
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

	}
void PLAT_flip(SDL_Surface* IGNORED, int sync) { //this rotates minarch menu + minui + tools
//	uint32_t now = SDL_GetTicks();
	vid.page ^= 1;
	if (!vid.renderingGame) {
		vid.targetRect.x = 0;
		vid.targetRect.y = 0;
		vid.targetRect.w = vid.screen->w;
		vid.targetRect.h = vid.screen->h;
		//vid.page = 0;
		if (vid.rotate == 0)
		{
			// 90 Rotation
			FlipRotate000bgr(vid.screen, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
		}
		if (vid.rotate == 1)
		{
			// 90 Rotation
			FlipRotate090bgr(vid.screen, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
		}
		if (vid.rotate == 2)
		{
			// 180 Rotation
			FlipRotate180bgr(vid.screen, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
		}
		if (vid.rotate == 3)
		{
			// 270 Rotation
			FlipRotate270bgr(vid.screen, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
		}
		pan_display(vid.page);
	} else {
		// No Rotation
	//	FlipRotate000bgr(vid.screengame, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
		//orientation ok but red and blue must be swapped
		if (vid.targetRect.w == vid.screen->w) {
			//fullscreen
			pixman_composite_src_0565_8888_asm_neon_bgr(vid.screengame->w,vid.screengame->h, vid.fbmmap+vid.page*vid.offset, vid.screengame->w, vid.screengame->pixels, vid.screengame->w);
		} else {
			//window
			convert_rgb565_to_abgr8888_neon_rect(vid.screengame->pixels, vid.fbmmap+vid.page*vid.offset, vid.screengame->w, vid.screengame->w, vid.targetRect.x, vid.targetRect.y, vid.targetRect.w, vid.targetRect.h);
		}
		vid.renderingGame = 0;
		if (sync) {
			PLAT_vsync(0);
		}
		pan_display(vid.page);
	}	
//	LOG_info("FLIP_VIDEO took %imsec\n", SDL_GetTicks()-now);fflush(stdout);
}

///////////////////////////////

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
	// setup surface
	ovl.overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, SCALE1(OVERLAY_WIDTH), SCALE1(OVERLAY_HEIGHT),OVERLAY_DEPTH,OVERLAY_RGBA_MASK);
	return ovl.overlay;
}
void PLAT_quitOverlay(void) {
	if (ovl.overlay) SDL_FreeSurface(ovl.overlay);
}
void PLAT_enableOverlay(int enable) {

}
///////////////////////////////

void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	*is_charging = getInt("/sys/class/power_supply/battery/charger_online");
	
	int i = getInt("/sys/class/power_supply/battery/voltage_now") / 10000; // 310-410
	i -= 310; 	// ~0-100

	// worry less about battery and more about the game you're playing
	     if (i>80) *charge = 100;
	else if (i>60) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>20) *charge =  40;
	else if (i>10) *charge =  20;
	else           *charge =  10;

	// TODO: tmp
	// *is_charging = 0;
	// *charge = PWR_LOW_CHARGE;
}

void PLAT_enableBacklight(int enable) {
	putInt("/sys/class/backlight/backlight.2/bl_power", enable ? FB_BLANK_UNBLANK : FB_BLANK_POWERDOWN);
}
void PLAT_powerOff(void) {
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();
	
	system("shutdown");
}

///////////////////////////////

void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	int numCPU = 4;
	char * buf = SDL_getenv("NUM_CPU");
	if (buf) {
		numCPU = atoi(buf);
		if ((numCPU < 1) || (numCPU > 4)) {
			numCPU = 4;
		}
	}
	switch (speed) {
		case CPU_SPEED_MENU: 		freq = cpufreq_menu; break;
		case CPU_SPEED_POWERSAVE:	freq = cpufreq_powersave; break;
		case CPU_SPEED_NORMAL: 		freq = cpufreq_game ; break;
		case CPU_SPEED_PERFORMANCE: freq = cpufreq_perf ; break;
		case CPU_SPEED_MAX:			freq = cpufreq_max ; break;	
	}
	
	char cmd[32];
	sprintf(cmd,"overclock.elf %d %d\n", freq, numCPU);
	if (freq) {
		system(cmd);
		LOG_info("Set CPU speed to %i\n", freq);
		cur_cpu_freq = freq/1000;
	}
}

void PLAT_setRumble(int effect, int strength) {	
		int val = ((70-(1-effect)*60) * strength)>>16;
		//LOG_info("Rumble effect: %i, strength: %i, val: %i\n", effect, strength, val);fflush(stdout);
		int fd = open("/sys/class/power_supply/battery/moto", O_WRONLY);
		if (fd>0) {
			dprintf(fd, "%d", val);
			close(fd);
		}
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	return "Anbernic RG35XX";
}

int PLAT_isOnline(void) {
	return 0;
}

char* PLAT_getIPAddress(void){
	char *outstr = NULL;
	outstr = malloc(8); // Alloca memoria per la stringa
	strcpy(outstr,"Offline");
	return outstr;
}

int PLAT_getNumProcessors(void) {
	//the core can be deactivated by command line
	return sysconf(_SC_NPROCESSORS_ONLN);
}

int PLAT_getProcessorTemp(void) {
	int temp = getInt("/sys/class/thermal/thermal_zone1/temp");
	return temp / 1000;
}

uint32_t PLAT_screenMemSize(void) {

	int fdfb; // /dev/fb0 handler
	struct fb_fix_screeninfo finfo;  //fixed fb info

	fdfb = open("/dev/fb0", O_RDWR);
	ioctl(fdfb, FBIOGET_FSCREENINFO, &finfo);
	close(fdfb);
	return finfo.smem_len;
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