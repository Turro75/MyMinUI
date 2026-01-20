// miyoomini
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <msettings.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"
#include "scaler.h"

///////////////////////////////
// based on eggs GFXSample_rev15

//#include <mi_sys.h>
//#include <mi_gfx.h>

int is_plus = 0;
int is_miniv4 = 0;


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

#define INPUT_COUNT 1
static int inputs[INPUT_COUNT];

void PLAT_initInput(void) {
	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // controller
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
	if (lid.has_lid && PLAT_lidChanged(NULL)) pad.just_released |= BTN_SLEEP;
}

int PLAT_shouldWake(void) {
	int lid_open = 1; // assume open by default
	if (lid.has_lid && PLAT_lidChanged(&lid_open) && lid_open) return 1;
	
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
	void *fbmmap; //mmap address of the framebuffer, up to 2 pages
	SDL_Surface* screen;  //swsurface to let sdl thinking it's the screen
	SDL_Surface* screen2;
	SDL_Surface* screen3;
	SDL_Surface *screengame;
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

int cpufreq_menu,cpufreq_game,cpufreq_perf,cpufreq_powersave,cpufreq_max;

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


	is_plus = exists("/customer/app/axp_test");
	char *tmpvar = getenv("IS_MMV4");
	is_miniv4 = (strncmp(tmpvar, "true", 4) == 0);
  
	vid.fdfb = open("/dev/fb0", O_RDWR);
	int p = FIXED_PITCH;
	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	vid.width = w;
	vid.height = h;
	DEVICE_WIDTH = w;
	DEVICE_HEIGHT = h;

	if (exists(ROTATE_SYSTEM_PATH)) {
		vid.rotate = getInt(ROTATE_SYSTEM_PATH) &3;
	}

	if (is_miniv4) {
		LOG_info("Miyoomini 2.8 v4 Found!, set the game screen to 752x560\n");
		w = 752;
		h = 560;
		vid.width = w;
		vid.height = h;
		DEVICE_WIDTH = FIXED_WIDTH;
		DEVICE_HEIGHT = FIXED_HEIGHT;
		p = w * FIXED_BPP;
	}	

	if (vid.rotate % 2 == 1) {
		DEVICE_WIDTH = FIXED_HEIGHT;
		DEVICE_HEIGHT = FIXED_WIDTH;
	}

	DEVICE_PITCH = p;
	vid.rotate = 2;	//no reason to change it.
	get_fbinfo();	

	GAME_WIDTH = w;
	GAME_HEIGHT = h;	
	
	vid.rotategame = (4-vid.rotate)&3;
    vid.vinfo.xres=w;
    vid.vinfo.yres=h;
	vid.vinfo.xres_virtual=vid.vinfo.xres;
	vid.vinfo.yres_virtual=vid.vinfo.yres*2;
	vid.vinfo.bits_per_pixel=32;

	//at the beginning set the screen size to 640x480
    set_fbinfo();
	get_fbinfo();

	LOG_info("DEVICE_WIDTH=%d, DEVICE_HEIGHT=%d\n", DEVICE_WIDTH, DEVICE_HEIGHT);fflush(stdout);

/*	for (int c = 0; c < 10; c++) {
		int cnow=SDL_GetTicks();
		pan_display(c % 2);
		LOG_info("\tPAN_display took %imsec\n", SDL_GetTicks()-cnow);
	} */
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
	vid.screen =  SDL_CreateRGBSurface(0, DEVICE_WIDTH, DEVICE_HEIGHT, FIXED_DEPTH, RGBA_MASK_565);
	vid.screen2 = SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, FIXED_DEPTH, RGBA_MASK_565); 
	vid.screen3 = SDL_CreateRGBSurface(0, vid.width, vid.height, FIXED_DEPTH, RGBA_MASK_565);
	vid.screengame = SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, FIXED_DEPTH, RGBA_MASK_565); 
	//create a mmap with the maximum available memory, we avoid recreating it during the resize as it is useless and waste of time.

    vid.fbmmap = mmap(NULL, vid.screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, vid.fdfb, 0);
	
	vid.sharpness = SHARPNESS_SOFT;
	return vid.screen;
}

void PLAT_quitVideo(void) {
	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.screen2);
	SDL_FreeSurface(vid.screen3);
	SDL_FreeSurface(vid.screengame);
	vid.vinfo.xres = FIXED_WIDTH;
	vid.vinfo.yres = FIXED_HEIGHT;
	set_fbinfo();
	munmap(vid.fbmmap, 0);
    close(vid.fdfb);
}

void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_FillRect(vid.screen, NULL, 0); // TODO: revisit
	SDL_FillRect(vid.screen2, NULL, 0);
	SDL_FillRect(vid.screen3, NULL, 0);
	SDL_FillRect(vid.screengame, NULL, 0);
}

void PLAT_clearAll(void) {
	SDL_FillRect(vid.screen, NULL, 0); // TODO: revisit
	SDL_FillRect(vid.screen2, NULL, 0);
	SDL_FillRect(vid.screen3, NULL, 0);
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
		ioctl(vid.fdfb, FBIO_WAITFORVSYNC, &res);
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
	vid.page ^= 1;
	if (!vid.renderingGame) {
		vid.targetRect.x = 0;
		vid.targetRect.y = 0;
		vid.targetRect.w = vid.width;
		vid.targetRect.h = vid.height;
		scale_mat_nearest_lut_rgb565_neon_fast_xy_pitch(vid.screen->pixels, vid.screen->w, vid.screen->h, vid.screen->pitch, vid.screen3->pixels, vid.screen3->w, vid.screen3->h, vid.screen3->pitch, 0,0, vid.width, vid.height);
		if (vid.rotate == 0) 
		{
			// No Rotation
			FlipRotate000(vid.screen3, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);			
		}
		if (vid.rotate == 1)
		{
			// 90 Rotation
			FlipRotate090(vid.screen3, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
		}
		if (vid.rotate == 2)
		{
			// 180 Rotation
			FlipRotate180(vid.screen3, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
		}
		if (vid.rotate == 3)
		{
			// 270 Rotation
			FlipRotate270(vid.screen3, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
		}
		//to avoid flickering/tearing in menu.
	} else {
		//the image must be rotated by 180° 
		pixman_composite_src_0565_8888_asm_neon(vid.screengame->w, vid.screengame->h, vid.fbmmap+vid.page*vid.offset*sync, vid.screengame->pitch/2, vid.screengame->pixels, vid.screengame->pitch/2);
		//FlipRotate000(vid.screengame, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
		
	}
	vid.renderingGame = 0;	
	pan_display(vid.page); 	
}

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

//	mmplus axp223 (via eggs)
#define	AXPDEV	"/dev/i2c-1"
#define	AXPID	(0x34)

//
//	AXP223 write (plus)
//		32 .. bit7: Shutdown Control
//
int axp_write(unsigned char address, unsigned char val) {
	struct i2c_msg msg[1];
	struct i2c_rdwr_ioctl_data packets;
	unsigned char buf[2];
	int ret;
	int fd = open(AXPDEV, O_RDWR);
	ioctl(fd, I2C_TIMEOUT, 5);
	ioctl(fd, I2C_RETRIES, 1);

	buf[0] = address;
	buf[1] = val;
	msg[0].addr = AXPID;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = buf;

	packets.nmsgs = 1;
	packets.msgs = &msg[0];
	ret = ioctl(fd, I2C_RDWR, &packets);

	close(fd);
	if (ret < 0) return -1;
	return 0;
}

//
//	AXP223 read (plus)
//		00 .. C4/C5(USBDC connected) 00(discharging)
//			bit7: ACIN presence indication 0:ACIN not exist, 1:ACIN exists
//			bit6: Indicating whether ACIN is usable (used by axp_test)
//			bit4: Indicating whether VBUS is usable (used by axp_test)
//			bit2: Indicating the Battery current direction 0: discharging, 1: charging
//			bit0: Indicating whether the boot source is ACIN or VBUS
//		01 .. 70(charging) 30(non-charging)
//			bit6: Charge indication 0:not charge or charge finished, 1: in charging
//		B9 .. (& 0x7F) battery percentage
//
int axp_read(unsigned char address) {
	struct i2c_msg msg[2];
	struct i2c_rdwr_ioctl_data packets;
	unsigned char val;
	int ret;
	int fd = open(AXPDEV, O_RDWR);
	ioctl(fd, I2C_TIMEOUT, 5);
	ioctl(fd, I2C_RETRIES, 1);

	msg[0].addr = AXPID;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &address;
	msg[1].addr = AXPID;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &val;

	packets.nmsgs = 2;
	packets.msgs = &msg[0];
	ret = ioctl(fd, I2C_RDWR, &packets);

	close(fd);
	if(ret < 0) return -1;
	return val;
}

///////////////////////////////


#define LID_PATH "/sys/devices/soc0/soc/soc:hall-mh248/hallvalue"
void PLAT_initLid(void) {
	lid.has_lid = exists(LID_PATH);
}
int PLAT_lidChanged(int* state) {
	if (lid.has_lid) {
		int lid_open = getInt(LID_PATH);
		if (lid_open!=lid.is_open) {
			lid.is_open = lid_open;
			if (state) *state = lid_open;
			return 1;
		}
	}
	return 0;
}


//////////////////////////////////

void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	*is_charging = is_plus ? (axp_read(0x00) & 0x4) > 0 : getInt("/sys/devices/gpiochip0/gpio/gpio59/value");
	
	int i = getInt("/tmp/battery"); // 0-100?

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
	if (enable) {
		putInt("/sys/class/gpio/gpio4/value", 1);
		putInt("/sys/class/gpio/unexport", 4);
		putInt("/sys/class/pwm/pwmchip0/export", 0);
		putInt("/sys/class/pwm/pwmchip0/pwm0/enable",0);
		putInt("/sys/class/pwm/pwmchip0/pwm0/enable",1);
	}
	else {
		putInt("/sys/class/gpio/export", 4);
		putFile("/sys/class/gpio/gpio4/direction", "out");
		putInt("/sys/class/gpio/gpio4/value", 0);
	}
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
	while (1) pause(); // lolwat
}

///////////////////////////////

// #define GOVERNOR_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
// void PLAT_setCPUSpeed(int speed) {
// 	// TODO: this isn't quite right
// 	if (speed==CPU_SPEED_MENU) putFile(GOVERNOR_PATH, "powersave");
// 	else if (speed==CPU_SPEED_POWERSAVE) putFile(GOVERNOR_PATH, "ondemand");
// 	else putFile(GOVERNOR_PATH, "performance");
// }

// copy/paste of 35XX version now that we have our own overclock.elf
void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
		case CPU_SPEED_MENU: 		freq = cpufreq_menu; break;
		case CPU_SPEED_POWERSAVE:	freq = cpufreq_powersave; break;
		case CPU_SPEED_NORMAL: 		freq = cpufreq_game ; break;
		case CPU_SPEED_PERFORMANCE: freq = cpufreq_perf ; break;
		case CPU_SPEED_MAX:			freq = cpufreq_max ; break;	
	}
	
	char cmd[32];
	sprintf(cmd,"overclock.elf %d\n", freq);
	if (freq) {
		system(cmd);
		LOG_info("Set CPU speed to %i\n", freq);
		cur_cpu_freq = freq/1000;
	}
}

void rumble_start(int strength){
	static char lastvalue = 0;
    const char str_export[2] = "48";
    const char str_direction[3] = "out";
    char value[1];
    int fd;

    value[0] = (strength == 0 ? 0x31 : 0x30); // '0' : '1'
    if (lastvalue != value[0]) {
       fd = open("/sys/class/gpio/export", O_WRONLY);
       if (fd > 0) { write(fd, str_export, 2); close(fd); }
       fd = open("/sys/class/gpio/gpio48/direction", O_WRONLY);
       if (fd > 0) { write(fd, str_direction, 3); close(fd); }
       fd = open("/sys/class/gpio/gpio48/value", O_WRONLY);
       if (fd > 0) { write(fd, value, 1); close(fd); }
       lastvalue = value[0];
    }
}

uint32_t rumble_stop(uint32_t interval, void *param) {
	rumble_start(0);
	return 0; // return 0 to stop the timer
}

void PLAT_setRumble(int effect, int strength) {
	rumble_start(strength);
	if (strength > 0)
		SDL_AddTimer(((70-(1-effect)*60) * strength)>>16, rumble_stop, NULL);
}

int PLAT_pickSampleRate(int requested, int max) {
	return max;
}

char* PLAT_getModel(void) {
	if (lid.has_lid) return "Miyoo Mini Flip";
	return is_plus ? "Miyoo Mini Plus" : "Miyoo Mini";
}

int PLAT_isOnline(void) {
	return 0;
}

int PLAT_getNumProcessors(void) {
	//the core can be deactivated by command line
	return 2;
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
