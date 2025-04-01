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
#include "platform.h"
#include "api.h"
#include "utils.h"

//almost all event2
#define RAW_UP		544
#define RAW_DOWN	545
#define RAW_LEFT	546
#define RAW_RIGHT	547
#define RAW_A		305
#define RAW_B		304
#define RAW_X		307
#define RAW_Y		308
#define RAW_START	 705
#define RAW_SELECT	 704
#define RAW_MENU	 708
#define RAW_L1		 310
#define RAW_L2		 312
#define RAW_R1		 311
#define RAW_R2		 313
#define RAW_L3		 706
#define RAW_R3		 707
#define RAW_PLUS	 115  //event3
#define RAW_MINUS	 114  //event3
#define RAW_POWER	116  //event0

// from <linux/input.h> which has BTN_ constants that conflict with platform.h
struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
};
#define EV_KEY			0x01
#define EV_ABS			0x03
///////////////////////////////


#define INPUT_COUNT 3
static int inputs[INPUT_COUNT];

long map(int x, int in_min, int in_max, int out_min, int out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void PLAT_initInput(void) {
	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // power
	inputs[1] = open("/dev/input/event2", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // controller
	inputs[2] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // volume +/-
	//Stick_init(); // analog
}

void PLAT_quitInput(void) {
	//Stick_quit();
	for (int i=0; i<INPUT_COUNT; i++) {
		close(inputs[i]);
	}
}

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
	for (int i=0; i<INPUT_COUNT; i++) {
		while (read(inputs[i], &event, sizeof(event))==sizeof(event)) {
			if (event.type!=EV_KEY && event.type!=EV_ABS) continue;

			int btn = BTN_NONE;
			int pressed = 0; // 0=up,1=down
			int id = -1;
			int type = event.type;
			int code = event.code;
			int value = event.value;
		//	printf("/dev/input/event%d: Type %d event: SCANCODE/AXIS=%i, PRESSED/AXIS_VALUE=%i\n", i,type, code, value);system("sync");
			// TODO: tmp, hardcoded, missing some buttons
			if (type==EV_KEY) {
				if (value>1) continue; // ignore repeats
			
				pressed = value;

				if 		(code==RAW_START)	{ btn = BTN_START; 		id = BTN_ID_START; } 
			    else if (code==RAW_SELECT)	{ btn = BTN_SELECT; 	id = BTN_ID_SELECT; }
				else if (code==RAW_A)		{ btn = BTN_A; 			id = BTN_ID_A; }
				else if (code==RAW_B)		{ btn = BTN_B; 			id = BTN_ID_B; }

				//LOG_info("key event: %i (%i)\n", code,pressed);fflush(stdout);
				else if (code==RAW_UP) 		{ btn = BTN_DPAD_UP; 	id = BTN_ID_DPAD_UP; }
	 			else if (code==RAW_DOWN)	{ btn = BTN_DPAD_DOWN; 	id = BTN_ID_DPAD_DOWN; }
				else if (code==RAW_LEFT)	{ btn = BTN_DPAD_LEFT; 	id = BTN_ID_DPAD_LEFT; }
				else if (code==RAW_RIGHT)	{ btn = BTN_DPAD_RIGHT; id = BTN_ID_DPAD_RIGHT; }
				else if (code==RAW_X)		{ btn = BTN_X; 			id = BTN_ID_X; }
				else if (code==RAW_Y)		{ btn = BTN_Y; 			id = BTN_ID_Y; }
				
				else if (code==RAW_MENU)	{ btn = BTN_MENU; 		id = BTN_ID_MENU; }
				else if (code==RAW_L1)		{ btn = BTN_L1; 		id = BTN_ID_L1; }
				else if (code==RAW_L2)		{ btn = BTN_L2; 		id = BTN_ID_L2; }
				
				else if (code==RAW_R1)		{ btn = BTN_R1; 		id = BTN_ID_R1; }
				else if (code==RAW_R2)		{ btn = BTN_R2; 		id = BTN_ID_R2; }
				else if (code==RAW_L3)		{ btn = BTN_L3; 		id = BTN_ID_L3; }
				else if (code==RAW_R3)		{ btn = BTN_R3; 		id = BTN_ID_R3; }
				else if (code==RAW_PLUS)	{ btn = BTN_PLUS; 		id = BTN_ID_PLUS; }
				else if (code==RAW_MINUS)	{ btn = BTN_MINUS; 		id = BTN_ID_MINUS; }
			}
			if (type==EV_ABS) {  // (range -1800 0 +1800)

				if (code==0) {  //left stick horizontal analog (-1800 left / +1800 right)
					pad.laxis.x =  map(value ,-1800,1800,-0x7fff,0x7fff);
				}
				if (code==1) {  //left stick vertical analog (-1800 up / +1800 down)
					pad.laxis.y =  map(value ,-1800,1800,-0x7fff,0x7fff);
				}
				if (code==3) {  //right stick horizontal analog (-1800 left / +1800 right)
					pad.raxis.x =  map(value ,-1800,1800,-0x7fff,0x7fff);
				}
				if (code==4) {  //right stick vertical analog (-1800 up / +1800 down)
					pad.raxis.y =  map(value ,-1800,1800,-0x7fff,0x7fff);
				}
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
	//int input;
	static struct input_event event;
//	for (int i=0; i<INPUT_COUNT; i++) {
		//input = inputs[i];
		while (read(inputs[0], &event, sizeof(event))==sizeof(event)) {
			if (event.type==EV_KEY && event.code==RAW_POWER && event.value==0) {
				return 1;
			}
		}
//	}
	return 0;
}

static struct VID_Context {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;
	SDL_Texture* target;
	SDL_Surface* buffer;
	SDL_Surface* screen;
	GFX_Renderer* blit; // yeesh
	
	int width;
	int height;
	int pitch;
} vid;

SDL_Surface* PLAT_initVideo(void) {
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	
	LOG_info("Available video drivers:\n");
	for (int i=0; i<SDL_GetNumVideoDrivers(); i++) {
		LOG_info("- %s\n", SDL_GetVideoDriver(i));
	}
	LOG_info("Current video driver: %s\n", SDL_GetCurrentVideoDriver());
	
	LOG_info("Available render drivers:\n");
	for (int i=0; i<SDL_GetNumRenderDrivers(); i++) {
		SDL_RendererInfo info;
		SDL_GetRenderDriverInfo(i,&info);
		LOG_info("- %s\n", info.name);
	}
	
	LOG_info("Available display modes:\n");
	SDL_DisplayMode mode;
	for (int i=0; i<SDL_GetNumDisplayModes(0); i++) {
		SDL_GetDisplayMode(0, i, &mode);
		LOG_info("- %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	}
	SDL_GetCurrentDisplayMode(0, &mode);
	LOG_info("Current display mode: %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));fflush(stdout);
	
	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;
	vid.window   = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w,h, SDL_WINDOW_SHOWN);
	vid.renderer = SDL_CreateRenderer(vid.window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
	
	SDL_RendererInfo info;
	SDL_GetRendererInfo(vid.renderer, &info);
	LOG_info("Current render driver: %s\n", info.name);fflush(stdout);
	
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	
	// SDL_SetTextureScaleMode(vid.texture, SDL_ScaleModeNearest);
	LOG_info("PLAT_initVideo almost success!\n");fflush(stdout);
	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);
	vid.screen	= SDL_CreateRGBSurface(SDL_SWSURFACE, w,h, FIXED_DEPTH, RGBA_MASK_565);
	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
	LOG_info("PLAT_initVideo success!\n");fflush(stdout);
	return vid.screen;
}

static void clearVideo(void) {
	for (int i=0; i<3; i++) {
		SDL_RenderClear(vid.renderer);
		SDL_FillRect(vid.screen, NULL, 0);
		
		SDL_LockTexture(vid.texture,NULL,&vid.buffer->pixels,&vid.buffer->pitch);
		SDL_FillRect(vid.buffer, NULL, 0);
		SDL_UnlockTexture(vid.texture);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL, NULL);
		
		SDL_RenderPresent(vid.renderer);
	}
}

void PLAT_quitVideo(void) {
	clearVideo();

	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.buffer);
	SDL_DestroyTexture(vid.texture);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);

	SDL_Quit();
	system("dd if=/dev/zero of=/dev/fb0 bs=1228800 count=1"); //system("cat /dev/zero > /dev/fb0");
}

void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_FillRect(screen, NULL, 0); // TODO: revisit
}
void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen); // TODO: revist
	SDL_RenderClear(vid.renderer);
}

void PLAT_setVsync(int vsync) {
	
}

static void resizeVideo(int w, int h, int p) {
	if (w==vid.width && h==vid.height && p==vid.pitch) return;
	
	LOG_info("resizeVideo(%i,%i,%i)\n",w,h,p);

	SDL_FreeSurface(vid.buffer);
	SDL_DestroyTexture(vid.texture);
	// PLAT_clearVideo(vid.screen);
	
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	// SDL_SetTextureScaleMode(vid.texture, SDL_ScaleModeNearest);
	
	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);

	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
	resizeVideo(w,h,p);
	return vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	
}
void PLAT_setNearestNeighbor(int enabled) {
	// always enabled?
}
void PLAT_setSharpness(int sharpness) {
	// buh
}
void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	// LOG_info("getScaler for scale: %i\n", renderer->scale);
//	effect.next_scale = renderer->scale;
	return scale1x1_c16;
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.blit = renderer;
	SDL_RenderClear(vid.renderer);
	resizeVideo(vid.blit->true_w,vid.blit->true_h,vid.blit->src_p);
	PLAT_flip(NULL, 0);
}

void PLAT_flip(SDL_Surface* IGNORED, int ignored) {
	if (!vid.blit) {
		resizeVideo(FIXED_WIDTH,FIXED_HEIGHT,FIXED_PITCH); // !!!???
		SDL_UpdateTexture(vid.texture,NULL,vid.screen->pixels,vid.screen->pitch);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL,NULL);
		SDL_RenderPresent(vid.renderer);
		return;
	}
	
		SDL_UpdateTexture(vid.texture,NULL,vid.blit->src,vid.blit->src_p);
	// LOG_info("blit blocked for %ims (%i,%i)\n", SDL_GetTicks()-then,vid.buffer->w,vid.buffer->h);
	
	SDL_Texture* target = vid.texture;
	int x = vid.blit->src_x;
	int y = vid.blit->src_y;
	int w = vid.blit->src_w;
	int h = vid.blit->src_h;
/*	if (vid.sharpness==SHARPNESS_CRISP) {
		SDL_SetRenderTarget(vid.renderer,vid.target);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL,NULL);
		SDL_SetRenderTarget(vid.renderer,NULL);
		x *= hard_scale;
		y *= hard_scale;
		w *= hard_scale;
		h *= hard_scale;
		target = vid.target;
	} */
	
	SDL_Rect* src_rect = &(SDL_Rect){x,y,w,h};
	SDL_Rect* dst_rect = &(SDL_Rect){0,0,FIXED_WIDTH,FIXED_HEIGHT};
	if (vid.blit->aspect==0) { // native or cropped
		// LOG_info("src_rect %i,%i %ix%i\n",src_rect->x,src_rect->y,src_rect->w,src_rect->h);

		int w = vid.blit->src_w * vid.blit->scale;
		int h = vid.blit->src_h * vid.blit->scale;
		int x = (FIXED_WIDTH - w) / 2;
		int y = (FIXED_HEIGHT - h) / 2;
		dst_rect->x = x;
		dst_rect->y = y;
		dst_rect->w = w;
		dst_rect->h = h;
		
		// LOG_info("dst_rect %i,%i %ix%i\n",dst_rect->x,dst_rect->y,dst_rect->w,dst_rect->h);
	}
	else if (vid.blit->aspect>0) { // aspect
		int h = FIXED_HEIGHT;
		int w = h * vid.blit->aspect;
		if (w>FIXED_WIDTH) {
			double ratio = 1 / vid.blit->aspect;
			w = FIXED_WIDTH;
			h = w * ratio;
		}
		int x = (FIXED_WIDTH - w) / 2;
		int y = (FIXED_HEIGHT - h) / 2;
		// dst_rect = &(SDL_Rect){x,y,w,h};
		dst_rect->x = x;
		dst_rect->y = y;
		dst_rect->w = w;
		dst_rect->h = h;
	}
	
	SDL_RenderCopy(vid.renderer, target, src_rect, dst_rect);
	
//	updateEffect();
//	if (vid.blit && effect.type!=EFFECT_NONE && vid.effect) {
//		SDL_RenderCopy(vid.renderer, vid.effect, &(SDL_Rect){0,0,dst_rect->w,dst_rect->h}, dst_rect);
//	}	
	// uint32_t then = SDL_GetTicks();
	SDL_RenderPresent(vid.renderer);
	// LOG_info("SDL_RenderPresent blocked for %ims\n", SDL_GetTicks()-then);
	vid.blit = NULL;
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

void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	int charge_fd = open("/sys/class/power_supply/battery/current_now", O_RDONLY);
	if (charge_fd < 0) {
		*is_charging = 0;
	} else {
		char buf[8];
		read(charge_fd, buf, 2);
		*is_charging = (atoi(buf)>0)?1:0;
	}
	close(charge_fd);
	int i;
	charge_fd = open("/sys/class/power_supply/battery/voltage_now", O_RDONLY);
	if (charge_fd < 0) {
		i = 3380000;
	} else {
		char buf[10];//
		read(charge_fd, buf, 8);
		i = atoi(buf);
	}
	close(charge_fd);
	i = MIN(i,4100000);
	*charge = map(i,3380000,4100000,0,100);
	LOG_info("Charging: %d, Raw battery: %i -> %d%%\n", *is_charging, i, *charge);
}


void rawBacklight(int value) {
	unsigned int args[4] = {0};
	args[1] = value;
	int disp_fd=open("/dev/disp",O_RDWR);
	if (value){
//		ioctl(disp_fd, DISP_LCD_BACKLIGHT_ENABLE, args);
	} else {
//		ioctl(disp_fd, DISP_LCD_BACKLIGHT_DISABLE, args);
	}	
	close(disp_fd);
}


void PLAT_enableBacklight(int enable) {
    if (enable){
		SetBrightness(GetBrightness());
        rawBacklight(140);		
    } else {
		//rawBacklight(0);
		SetRawBrightness(0);
    }	
}

void PLAT_setEffect(int effect) {
	
}

SDL_Surface* PLAT_resizeVideoGame(int w, int h, int p) {
	resizeVideo(w,h,p);
	return vid.screen;
}

void PLAT_powerOff(void) {
	//system("leds_on");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();	
	
	touch("/tmp/poweroff");
	exit(0);
}

///////////////////////////////

#define GOVERNOR_CPUSPEED_PATH "/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed"
#define GOVERNOR_PATH          "/sys/devices/system/cpu/cpufreq/policy0/scaling_governor"

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
		case CPU_SPEED_MENU: 		freq = 1008000; break;
		case CPU_SPEED_POWERSAVE:	freq = 1008000; break;
		case CPU_SPEED_NORMAL: 		freq = 1200000; break;
		case CPU_SPEED_PERFORMANCE: freq = 1296000; break;
		case CPU_SPEED_MAX:			freq = 1512000; break;
	}
	char cmd[512];
	//sudo sh -c "echo -n 1512000 > /sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed"
	sprintf(cmd,"sudo sh -c \"echo -n userspace > %s \" ; sudo sh -c \"echo %i > %s\"", GOVERNOR_PATH, freq, GOVERNOR_CPUSPEED_PATH);
	system(cmd);
}

void PLAT_setRumble(int strength) {
	// buh
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	return "R36S ArkOS";
}

int PLAT_isOnline(void) {
	return 0;
}

int PLAT_getNumProcessors(void) {
	//the core can be deactivated by command line
	return sysconf(_SC_NPROCESSORS_ONLN);
}

uint32_t PLAT_screenMemSize(void) {
	return 0;
}

void PLAT_getAudioOutput(void){
//buh!
}