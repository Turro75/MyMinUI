// rgb30
#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include <msettings.h>
#include <mstick.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"

void Main_Flip(void);
///////////////////////////////

#define RAW_UP		103
#define RAW_DOWN	108
#define RAW_LEFT	105
#define RAW_RIGHT	106
#define RAW_A		 57
#define RAW_B		 29
#define RAW_X		 42
#define RAW_Y		 56
#define RAW_START	 28
#define RAW_SELECT	 97
#define RAW_MENU	  1
#define RAW_L1		 18
#define RAW_L2		 15
#define RAW_R1		 20
#define RAW_R2		 14
#define RAW_PLUS	115
#define RAW_MINUS	114
#define RAW_POWER	116

#define INPUT_COUNT 2
static int inputs[INPUT_COUNT];

void PLAT_initInput(void) {
	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // power
	inputs[1] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // controller
	Stick_init(); // analog
}
void PLAT_quitInput(void) {
	Stick_quit();
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
	pad.just_repeated = BTN_NONE;

	uint32_t tick = SDL_GetTicks();
	for (int i=0; i<BTN_ID_COUNT; i++) {
		int btn = 1 << i;
		if ((pad.is_pressed & btn) && (tick>=pad.repeat_at[i])) {
			pad.just_repeated |= btn; // set
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
	Stick_get(&(pad.laxis.x), &(pad.laxis.y));
	if (pad.map_leftstick_to_dpad){
		PAD_setAnalog(BTN_ID_DPAD_UP,   BTN_ID_DPAD_DOWN,  pad.laxis.y, tick+PAD_REPEAT_DELAY);
		PAD_setAnalog(BTN_ID_DPAD_LEFT,   BTN_ID_DPAD_RIGHT,  pad.laxis.x, tick+PAD_REPEAT_DELAY);
	} else {
		PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x, tick+PAD_REPEAT_DELAY);
		PAD_setAnalog(BTN_ID_ANALOG_UP,   BTN_ID_ANALOG_DOWN,  pad.laxis.y, tick+PAD_REPEAT_DELAY);
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

///////////////////////////////

static struct VID_Context {
	int fdfb; // /dev/fb0 handler
	struct fb_fix_screeninfo finfo;  //fixed fb info
	struct fb_var_screeninfo vinfo;  //adjustable fb info
	void *fbmmap; //mmap address of the framebuffer
	SDL_Surface* screen;  //swsurface to let sdl thinking it's the screen
	SDL_Surface* screen2;  //swsurface to let sdl thinking it's the screen
	int linewidth;
	int screen_size;
	int width;  //current width 
	int height; // current height
	int pitch;  //sdl bpp
	int sharpness; //let's see if it works
	int rotate;
	int page;
	int numpages;
	uint32_t offset;
	SDL_Rect targetRect;
	int renderingGame;
} vid;

static int device_width;
static int device_height;
static int device_pitch;

static int lastw=0;
static int lasth=0;
static int lastp=0;

static int finalrotate=0;

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

int MY_SDLFB_FlipRotate90(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
	//copy a surface to the screen and flip it
	//it must be the same resolution, the bpp16 is then converted to 32bpp
	//fprintf(stdout,"Buffer has %d bpp\n", buffer->format->BitsPerPixel);fflush(stdout);

	//the alpha channel must be set to 0xff
	int thispitch = buffer->pitch/buffer->format->BytesPerPixel;
	int x, y;
	if (buffer->format->BitsPerPixel == 16) {
		//ok start conversion assuming it is RGB565		
		for (y = targetarea.y; y < (targetarea.y + targetarea.h) ; y++) {
			for (x = targetarea.x; x < (targetarea.x + targetarea.w); x++) {
				uint16_t pixel = *((uint16_t *)buffer->pixels + x + y * thispitch);
				*((uint32_t *)fbmmap + x + y * linewidth) = (uint32_t)(0xFF000000 | ((pixel & 0xF800) << 8) | ((pixel & 0x7E0) << 5) | ((pixel & 0x1F) << 3));
			}
		}
	}
	if (buffer->format->BitsPerPixel == 32) {
		//ok start conversion assuming it is ABGR888
		for (y = targetarea.y; y < (targetarea.y + targetarea.h) ; y++) {
			for (x = targetarea.x; x < (targetarea.x + targetarea.w); x++) {
				uint32_t pixel = *((uint32_t *)buffer->pixels + x + y * thispitch);
				*((uint32_t *)fbmmap + x + y * linewidth) = 
					0xFF000000 | ((pixel & 0xFF0000) >> 16) | (pixel & 0xFF00)  | ((pixel & 0xFF) << 16);
			}
		}
	}
	return 0;	
}

int MY_SDLFB_FlipRotate270(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
	//copy a surface to the screen and flip it
	//it must be the same resolution, the bpp16 is then converted to 32bpp
	//fprintf(stdout,"Buffer has %d bpp\n", buffer->format->BitsPerPixel);fflush(stdout);

	//the alpha channel must be set to 0xff
	int thispitch = buffer->pitch/buffer->format->BytesPerPixel;
	int x, y;
	if (buffer->format->BitsPerPixel == 16) {
		//ok start conversion assuming it is RGB565		
		for (y = targetarea.y; y < (targetarea.y + targetarea.h) ; y++) {
			for (x = targetarea.x; x < (targetarea.x + targetarea.w); x++) {
				uint16_t pixel = *((uint16_t *)buffer->pixels + x + y * thispitch);
				*((uint32_t *)fbmmap + (buffer->w - x) + (buffer->h - y) * linewidth) = (uint32_t)(0xFF000000 | ((pixel & 0xF800) << 8) | ((pixel & 0x7E0) << 5) | ((pixel & 0x1F) << 3));
			}
		}
	}
	if (buffer->format->BitsPerPixel == 32) {
		//ok start conversion assuming it is ABGR888
		for (y = targetarea.y; y < (targetarea.y + targetarea.h) ; y++) {
			for (x = targetarea.x; x < (targetarea.x + targetarea.w); x++) {
				uint32_t pixel = *((uint32_t *)buffer->pixels + x + y * thispitch);
				*((uint32_t *)fbmmap + (buffer->w - x) + (buffer->h - y) * linewidth) = 
					0xFF000000 | ((pixel & 0xFF0000) >> 16) | (pixel & 0xFF00)  | ((pixel & 0xFF) << 16);
			}
		}
	}
	return 0;	
}

int MY_SDLFB_Flip(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
	//copy a surface to the screen and flip it
	//it must be the same resolution, the bpp16 is then converted to 32bpp
	//fprintf(stdout,"Buffer has %d bpp\n", buffer->format->BitsPerPixel);fflush(stdout);

	//the alpha channel must be set to 0xff
	int thispitch = buffer->pitch/buffer->format->BytesPerPixel;
	int x, y;
	if (buffer->format->BitsPerPixel == 16) {
		//ok start conversion assuming it is RGB565		
		for (y = targetarea.y; y < (targetarea.y + targetarea.h) ; y++) {
			for (x = targetarea.x; x < (targetarea.x + targetarea.w); x++) {
				uint16_t pixel = *((uint16_t *)buffer->pixels + x + y * thispitch);
				*((uint32_t *)fbmmap + y + (buffer->w - x - 1) * linewidth) = (uint32_t)(0xFF000000 | ((pixel & 0xF800) << 8) | ((pixel & 0x7E0) << 5) | ((pixel & 0x1F) << 3));
			}
		}
	}
	if (buffer->format->BitsPerPixel == 32) {
		//ok start conversion assuming it is ABGR888
		for (y = targetarea.y; y < (targetarea.y + targetarea.h) ; y++) {
			for (x = targetarea.x; x < (targetarea.x + targetarea.w); x++) {
				uint32_t pixel = *((uint32_t *)buffer->pixels + x + y * thispitch);
				*((uint32_t *)fbmmap + y + (buffer->w - x - 1) * linewidth) = 
					0xFF000000 | ((pixel & 0xFF0000) >> 16) | (pixel & 0xFF00)  | ((pixel & 0xFF) << 16);
			}
		}
	}
	return 0;	
}
int MY_SDLFB_FlipRotate180(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
	//copy a surface to the screen and flip it
	//it must be the same resolution, the bpp16 is then converted to 32bpp
	//fprintf(stdout,"Buffer has %d bpp\n", buffer->format->BitsPerPixel);fflush(stdout);

	//the alpha channel must be set to 0xff
	int thispitch = buffer->pitch/buffer->format->BytesPerPixel;
	int x, y;
	if (buffer->format->BitsPerPixel == 16) {
		//ok start conversion assuming it is RGB565		
		for (y = targetarea.y; y < (targetarea.y + targetarea.h) ; y++) {
			for (x = targetarea.x; x < (targetarea.x + targetarea.w); x++) {
				uint16_t pixel = *((uint16_t *)buffer->pixels + x + y * thispitch);
				*((uint32_t *)fbmmap + (buffer->h- y -1) + (x  * linewidth)) = (uint32_t)(0xFF000000 | ((pixel & 0xF800) << 8) | ((pixel & 0x7E0) << 5) | ((pixel & 0x1F) << 3));
			}
		}
	}
	if (buffer->format->BitsPerPixel == 32) {
		//ok start conversion assuming it is ABGR888
		for (y = targetarea.y; y < (targetarea.y + targetarea.h) ; y++) {
			for (x = targetarea.x; x < (targetarea.x + targetarea.w); x++) {
				uint32_t pixel = *((uint32_t *)buffer->pixels + x + y * thispitch);
				*((uint32_t *)fbmmap + (buffer->h- y -1) + (x  * linewidth)) = 
					0xFF000000 | ((pixel & 0xFF0000) >> 16) | (pixel & 0xFF00)  | ((pixel & 0xFF) << 16);
			}
		}
	}
	return 0;	
}

void pan_display(int page){
	vid.vinfo.yoffset = (vid.vinfo.yres_virtual/2) * page;
	ioctl(vid.fdfb, FBIOPAN_DISPLAY, &vid.vinfo);
}

SDL_Surface* PLAT_initVideo(void) {	
	vid.fdfb = open("/dev/fb0", O_RDWR);
	int w = FIXED_HEIGHT;
	int h = FIXED_WIDTH;
	int p = FIXED_PITCH;
	DEVICE_WIDTH = w;
	DEVICE_HEIGHT = h;
	DEVICE_PITCH = p;
	vid.rotate = 0;
	
	get_fbinfo();	

	if (exists(ROTATE_SYSTEM_PATH)) {
		vid.rotate = getInt(ROTATE_SYSTEM_PATH) &3;
	}

	if (vid.rotate % 2 == 0) {
		DEVICE_WIDTH = h;
		DEVICE_HEIGHT = w;
	}
	
    vid.vinfo.xres=w;
    vid.vinfo.yres=h;
	vid.vinfo.xres_virtual=vid.vinfo.xres;
	vid.vinfo.yres_virtual=vid.vinfo.yres*2;
	vid.vinfo.bits_per_pixel=32;	
	//at the beginning set the screen size to 640x480
    set_fbinfo();
	get_fbinfo();

	vid.screen =  SDL_CreateRGBSurface(0, DEVICE_WIDTH, DEVICE_HEIGHT, FIXED_DEPTH, RGBA_MASK_565);
	vid.screen2 = SDL_CreateRGBSurface(0, DEVICE_WIDTH, DEVICE_HEIGHT, FIXED_DEPTH, RGBA_MASK_565); 

	vid.page = 0;
	pan_display(vid.page);
	vid.renderingGame = 0;

	if(vid.vinfo.yres_virtual >= vid.vinfo.yres*2) {
		vid.numpages=2;
	} else {
		vid.numpages=1;
	}
	vid.offset = vid.vinfo.yres_virtual/2 * vid.finfo.line_length;
	vid.screen_size = vid.offset*2;
	vid.linewidth = vid.finfo.line_length/(vid.vinfo.bits_per_pixel/8);
    vid.fbmmap = mmap(NULL, vid.screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, vid.fdfb, 0);
	
	vid.sharpness = SHARPNESS_SOFT;
	return vid.screen;
}

void PLAT_quitVideo(void) {
	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.screen2);
	munmap(vid.fbmmap, 0);	
    close(vid.fdfb);
}

void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_FillRect(vid.screen, NULL, 0); // TODO: revisit
	SDL_FillRect(vid.screen2, NULL, 0);
}

void PLAT_clearAll(void) {
	SDL_FillRect(vid.screen, NULL, 0); // TODO: revisit
	SDL_FillRect(vid.screen2, NULL, 0);
	memset(vid.fbmmap, 0, vid.screen_size);
}

void PLAT_setVsync(int vsync) {
	// buh
}

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
	if (remaining>0) usleep(remaining*1000);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	return NULL;
}

void Main_Flip(void);

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	if (effect_type!=next_effect) {
		effect_type = next_effect;
	}
//	scale1x_line(renderer->src_surface->pixels, vid.screen2->pixels, renderer->dst_w, renderer->dst_h, renderer->src_surface->pitch, renderer->dst_w, renderer->dst_h, renderer->src_surface->pitch);
	if (effect_type==EFFECT_LINE) {
		SDL_SoftStretch(renderer->src_surface, NULL, vid.screen2, &(SDL_Rect){renderer->dst_x,renderer->dst_y,renderer->dst_w,renderer->dst_h});
		scale1x_line(vid.screen2->pixels, vid.screen->pixels, vid.screen2->w, vid.screen2->h, vid.screen2->pitch, vid.screen->w, vid.screen->h, vid.screen->pitch);
	}
	else if (effect_type==EFFECT_GRID) {
		SDL_SoftStretch(renderer->src_surface, NULL, vid.screen2, &(SDL_Rect){renderer->dst_x,renderer->dst_y,renderer->dst_w,renderer->dst_h});
		scale1x_grid(vid.screen2->pixels, vid.screen->pixels, vid.screen2->w, vid.screen2->h, vid.screen2->pitch, vid.screen->w, vid.screen->h, vid.screen->pitch);
	}
	else {
		SDL_SoftStretch(renderer->src_surface, NULL, vid.screen, &(SDL_Rect){renderer->dst_x,renderer->dst_y,renderer->dst_w,renderer->dst_h});
	}
	vid.targetRect.x = renderer->dst_x;
	vid.targetRect.y = renderer->dst_y;
	vid.targetRect.w = renderer->dst_w;
	vid.targetRect.h = renderer->dst_h;
	vid.renderingGame = 1;
}

void PLAT_flip(SDL_Surface* IGNORED, int sync) { //this rotates minarch menu + minui + tools
//	uint32_t now = SDL_GetTicks();

	if (!vid.renderingGame) {
		vid.targetRect.x = 0;
		vid.targetRect.y = 0;
		vid.targetRect.w = vid.screen->w;
		vid.targetRect.h = vid.screen->h;
		vid.page = 0;
	}	
	if (vid.rotate == 0) 
	{
		// No Rotation
		MY_SDLFB_Flip(vid.screen, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
	}
	if (vid.rotate == 1)
	{
		// 90 Rotation
		MY_SDLFB_FlipRotate90(vid.screen, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
	}
	if (vid.rotate == 2)
	{
		// 180 Rotation
		MY_SDLFB_FlipRotate180(vid.screen, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
	}
	if (vid.rotate == 3)
	{
		// 270 Rotation
		MY_SDLFB_FlipRotate270(vid.screen, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
	}
	vid.renderingGame = 0;
	pan_display(vid.page);
	if (vid.numpages == 2) {
		vid.page ^= 1;
	}
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

static int online = 0;
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	// *is_charging = 0;
	// *charge = PWR_LOW_CHARGE;
	// return;
	
	*is_charging = getInt("/sys/class/power_supply/usb/online");

	int i = getInt("/sys/class/power_supply/battery/capacity");
	// worry less about battery and more about the game you're playing
	     if (i>80) *charge = 100;
	else if (i>60) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>20) *charge =  40;
	else if (i>10) *charge =  20;
	else           *charge =  10;

	// wifi status, just hooking into the regular PWR polling
	// char status[16];
	// getFile("/sys/class/net/wlan0/operstate", status,16);
	// online = prefixMatch("up", status);
}

#define LED_PATH "/sys/class/leds/led1/brightness"
void PLAT_enableBacklight(int enable) {
	if (enable) {
		SetBrightness(GetBrightness());
		putInt(LED_PATH,0);
	}
	else {
		SetRawBrightness(0);
		putInt(LED_PATH,255);
	}
}

void PLAT_powerOff(void) {
	system("rm -f /tmp/minui_exec && sync");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	putInt(LED_PATH,255);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();
	exit(0);
}

///////////////////////////////

void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
		case CPU_SPEED_MENU: 		freq =  576; break;
		case CPU_SPEED_POWERSAVE:	freq = 816; break;
		case CPU_SPEED_NORMAL: 		freq = 1200; break;
		case CPU_SPEED_PERFORMANCE: freq = 1344; break;
		case CPU_SPEED_MAX: 		freq = 1512; break;
	}

	char cmd[128];
	sprintf(cmd,"overclock.elf userspace 4 %d 384 1080 0", freq);
	system(cmd);
}

#define RUMBLE_PATH "/sys/devices/virtual/timed_output/vibrator/enable"

void PLAT_setRumble(int strength) {
	putInt(RUMBLE_PATH, strength?1000:0);
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	return "Miyoo A30";
}

int PLAT_getNumProcessors(void) {
	//the core can be deactivated by command line
	return sysconf(_SC_NPROCESSORS_ONLN);
}

int PLAT_isOnline(void) {
	return online;
}