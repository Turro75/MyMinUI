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

void pan_display(int page){
	vid.vinfo.yoffset = (vid.vinfo.yres_virtual/2) * page;
	ioctl(vid.fdfb, FBIOPAN_DISPLAY, &vid.vinfo);
	ioctl(vid.fdfb, OWLFB_WAITFORVSYNC, &page);
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

/*int RG35XX_SDLFB_Flip(SDL_Surface *buffer, void * fbmmap, int linewidth) {
	//copy a surface to the screen and flip it
	//it must be the same resolution, the bpp16 is then converted to 32bpp
	//fprintf(stdout,"Buffer has %d bpp\n", buffer->format->BitsPerPixel);fflush(stdout);

	//the alpha channel must be set to 0xff
	int thispitch = buffer->pitch/buffer->format->BytesPerPixel;
	int x, y;
	if (buffer->format->BitsPerPixel == 16) {
		//ok start conversion assuming it is RGB565		
		for (y = 0; y < buffer->h; y++) {
			for (x = 0; x < buffer->w; x++) {
				uint16_t pixel = *((uint16_t *)buffer->pixels + x + y * thispitch);
				*((uint32_t *)fbmmap + x + y * linewidth) = (uint32_t)((0xFF000000) |  ((pixel & 0xF800) >> 8) | ((pixel & 0x7E0) << 5) | ((pixel & 0x1F) << 19));
			}
		}
	}
	if (buffer->format->BitsPerPixel == 32) {
		for (y = 0; y < buffer->h; y++) {
			for (x = 0; x < buffer->w; x++) {
				uint32_t pixel = *((uint32_t *)buffer->pixels + x + y * thispitch);
				*((uint32_t *)fbmmap + x + y * linewidth) = 
					0xFF000000 | ((pixel & 0xFF0000) >> 16) | (pixel & 0xFF00)  | ((pixel & 0xFF) << 16);
			}
		}
	}	
	//memcpy(fbmmap2, fbmmap, vid.screen_size);
	return 0;	
}
*/
int RG35XX_SDLFB_Flip(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
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
				*((uint32_t *)fbmmap + x + y * linewidth) = (uint32_t)((0xFF000000) |  ((pixel & 0xF800) >> 8) | ((pixel & 0x7E0) << 5) | ((pixel & 0x1F) << 19));
			}
		}
	}
	if (buffer->format->BitsPerPixel == 32) {
		for (y = targetarea.y; y < (targetarea.y + targetarea.h) ; y++) {
			for (x = targetarea.x; x < (targetarea.x + targetarea.w); x++) {
				uint32_t pixel = *((uint32_t *)buffer->pixels + x + y * thispitch);
				*((uint32_t *)fbmmap + x + y * linewidth) = 
					0xFF000000 | ((pixel & 0xFF0000) >> 16) | (pixel & 0xFF00)  | ((pixel & 0xFF) << 16);
			}
		}
	}	
	//memcpy(fbmmap2, fbmmap, vid.screen_size);
	return 0;	
}


int RG35XX_SDLFB_FlipRotate270(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
	//copy a surface to the screen and flip it
	//it must be the same resolution, the bpp16 is then converted to 32bpp
	//fprintf(stdout,"Buffer has %d bpp\n", buffer->format->BitsPerPixel);fflush(stdout);

	//the alpha channel must be set to 0xff
	int thispitch = buffer->pitch/buffer->format->BytesPerPixel;
	int x, y;
	if (buffer->format->BitsPerPixel == 16) {
		//ok start conversion assuming it is RGB565		
		for (y = 0; y < buffer->h; y++) {
			for (x = 0; x < buffer->w; x++) {
				uint16_t pixel = *((uint16_t *)buffer->pixels + x + y * thispitch);
				*((uint32_t *)fbmmap + y + (buffer->w - x - 1) * linewidth) = (uint32_t)((0xFF000000) |  ((pixel & 0xF800) >> 8) | ((pixel & 0x7E0) << 5) | ((pixel & 0x1F) << 19));
			}
		}
	}
	//memcpy(fbmmap2, fbmmap, vid.screen_size);
	return 0;	
}

int RG35XX_SDLFB_FlipRotate180(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
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
				*((uint32_t *)fbmmap + (buffer->w - x) + (buffer->h - y - 1) * linewidth) = (uint32_t)((0xFF000000) |  ((pixel & 0xF800) >> 8) | ((pixel & 0x7E0) << 5) | ((pixel & 0x1F) << 19));
			}
		}
	}
	//memcpy(fbmmap2, fbmmap, vid.screen_size);
	return 0;	
}

int RG35XX_SDLFB_FlipRotate90(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
	//copy a surface to the screen and flip it
	//it must be the same resolution, the bpp16 is then converted to 32bpp
	//fprintf(stdout,"Buffer has %d bpp\n", buffer->format->BitsPerPixel);fflush(stdout);

	//the alpha channel must be set to 0xff
	int thispitch = buffer->pitch/buffer->format->BytesPerPixel;
	int x, y;
	if (buffer->format->BitsPerPixel == 16) {
		//ok start conversion assuming it is RGB565		
		for (y = 0; y < buffer->h; y++) {
			for (x = 0; x < buffer->w; x++) {
				uint16_t pixel = *((uint16_t *)buffer->pixels + x + y * thispitch);
				*((uint32_t *)fbmmap + (buffer->h- y -1) + (x  * linewidth)) = (uint32_t)((0xFF000000) |  ((pixel & 0xF800) >> 8) | ((pixel & 0x7E0) << 5) | ((pixel & 0x1F) << 19));
			}
		}
	}
	//memcpy(fbmmap2, fbmmap, vid.screen_size);
	return 0;	
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

SDL_Surface* PLAT_initVideo(void) {	
	int w,p,h;
	if (GetHDMI()){
		SetHDMI(1);
		vid.fdfb = open("/dev/fb1", O_RDWR);
		w = HDMI_WIDTH_;
		h = HDMI_HEIGHT_;
		p = HDMI_PITCH_;		
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
	
	get_fbinfo();	

	if (exists(ROTATE_SYSTEM_PATH)) {
		vid.rotate = getInt(ROTATE_SYSTEM_PATH) &3;
	}

	if (vid.rotate % 2 == 1) {
		DEVICE_WIDTH = h;
		DEVICE_HEIGHT = w;
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
	
	struct owlfb_sync_info sinfo;
	sinfo.enabled = 1;
	sinfo.disp_id = 2;
	if (ioctl(vid.fdfb, OWLFB_VSYNC_EVENT_EN, &sinfo)<0) LOG_error("VSYNC_EVENT_EN failed %s\n",strerror(errno));
	
	vid.vinfo.xoffset=0;
	vid.vinfo.yoffset=0;
	for (int i = 0; i < 11; i++) {
		int cnow = SDL_GetTicks();
		usleep(5000*(i%2));
		int tmp = 0;
		vid.vinfo.yoffset = (i%2) * (vid.vinfo.yres_virtual/2);
		int error_code = ioctl(vid.fdfb, FBIOPAN_DISPLAY, &vid.vinfo); 
		ioctl(vid.fdfb, OWLFB_WAITFORVSYNC, &tmp);
		printf("FBIOPAN_DISPLAY Y MIXED DELAY took %i msec, error code is %i\n", SDL_GetTicks()-cnow, error_code);fflush(stdout);
	}

	vid.page = 0;
	pan_display(vid.page);
//	vid.numpages=2;

	vid.offset = vid.vinfo.yres_virtual/2 * vid.finfo.line_length;
	vid.screen_size = vid.offset*2;
	vid.linewidth = vid.finfo.line_length/(vid.vinfo.bits_per_pixel/8);

	vid.screen =  SDL_CreateRGBSurface(0, DEVICE_WIDTH, DEVICE_HEIGHT, FIXED_DEPTH, RGBA_MASK_565);
	vid.screen2 = SDL_CreateRGBSurface(0, DEVICE_WIDTH, DEVICE_HEIGHT, FIXED_DEPTH, RGBA_MASK_565); 

	//create a mmap with the maximum available memory, we avoid recreating it during the resize as it is useless and waste of time.
    vid.fbmmap = mmap(NULL, vid.screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, vid.fdfb, 0);
	vid.renderingGame = 0;
	
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
	}

	if (vid.rotate == 0) 
	{
		// No Rotation
		RG35XX_SDLFB_Flip(vid.screen, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
	}
	if (vid.rotate == 1)
	{
		// 90 Rotation
		RG35XX_SDLFB_FlipRotate90(vid.screen, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
	}
	if (vid.rotate == 2)
	{
		// 180 Rotation
		RG35XX_SDLFB_FlipRotate180(vid.screen, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
	}
	if (vid.rotate == 3)
	{
		// 270 Rotation
		RG35XX_SDLFB_FlipRotate270(vid.screen, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
	}
	vid.renderingGame = 0;
	pan_display(vid.page);
	vid.page = 1 - vid.page;
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
		case CPU_SPEED_MENU: 		freq =  504000; break;
		case CPU_SPEED_POWERSAVE:	freq = 1008000; break;
		case CPU_SPEED_NORMAL: 		freq = 1200000; break;
		case CPU_SPEED_PERFORMANCE: freq = 1392000; break;
		case CPU_SPEED_MAX: 		freq = 1488000; break;
	}
	
	char cmd[32];
	sprintf(cmd,"overclock.elf %d %d\n", freq, numCPU);
	system(cmd);
}

void PLAT_setRumble(int strength) {
	int val = MAX(0, MIN((100 * strength)>>16, 100));
	// LOG_info("strength: %8i (%3i/100)\n", strength, val);
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

int PLAT_getNumProcessors(void) {
	//the core can be deactivated by command line
	return sysconf(_SC_NPROCESSORS_ONLN);
}


uint32_t PLAT_screenMemSize(void) {

	int fdfb; // /dev/fb0 handler
	struct fb_fix_screeninfo finfo;  //fixed fb info

	fdfb = open("/dev/fb0", O_RDWR);
	ioctl(fdfb, FBIOGET_FSCREENINFO, &finfo);
	close(fdfb);
	return finfo.smem_len;
}