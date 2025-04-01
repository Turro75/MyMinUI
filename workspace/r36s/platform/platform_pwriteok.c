// trimuismart
#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <linux/kd.h>
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
#include "sdl.h"

//almost all event2 on r36s, event4 on rg353v
// /dewv/input/event1 is for the touchscreen
#define RAW_UP		544
#define RAW_DOWN	545
#define RAW_LEFT	546
#define RAW_RIGHT	547
#define RAW_A		305
#define RAW_B		304
#define RAW_X		307
#define RAW_Y		308
#define RAW_START	 705 
#define RAW_START_353 315 
#define RAW_SELECT	 704
#define RAW_SELECT_353	314
#define RAW_MENU	 708
#define RAW_MENU_353 316
#define RAW_L1		 310
#define RAW_L2		 312
#define RAW_R1		 311
#define RAW_R2		 313
#define RAW_L3		 706  
#define RAW_L3_353	 317
#define RAW_R3		 707  
#define RAW_R3_353	 318
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

static int PWR_Pressed = 0;
static int PWR_Actions = 0;
static uint32_t PWR_Tick = 0;
#define PWR_TIMEOUT 2000
///////////////////////////////
static int is353v = 0;
static int isg350 = 0;

static int _L3_RAW, _R3_RAW, _START_RAW, _SELECT_RAW, _MENU_RAW;

#define INPUT_COUNT 3
static int inputs[INPUT_COUNT];

long map(int x, int in_min, int in_max, int out_min, int out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void PLAT_initInput(void) {
	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // power
	if (inputs[0]<0) {
		LOG_info("/dev/input/event0 open failed"); // volume +/-
	}
	if (is353v) {
		inputs[1] = open("/dev/input/event4", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // controller
		_L3_RAW = RAW_L3_353;
		_R3_RAW = RAW_R3_353;
		_START_RAW = RAW_START_353;
		_SELECT_RAW = RAW_SELECT_353;
		_MENU_RAW = RAW_MENU_353;
	} else if (isg350) {
		inputs[1] = open("/dev/input/event2", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // controller
		_L3_RAW = RAW_L3_353;
		_R3_RAW = RAW_R3_353;
		_START_RAW = RAW_START_353;
		_SELECT_RAW = RAW_SELECT_353;
		_MENU_RAW = RAW_MENU_353;
	} else {
		//r36s
		inputs[1] = open("/dev/input/event2", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // controller
		_L3_RAW = RAW_L3;
		_R3_RAW = RAW_R3;
		_START_RAW = RAW_START;
		_SELECT_RAW = RAW_SELECT;
		_MENU_RAW = RAW_MENU;
	}
	if (inputs[1]<=0) {
		LOG_info("/dev/input/event2-4 open failed: is353v=%d - isg350=%d - r36s = %d", is353v, isg350, !is353v && !isg350); // volume +/-
	}
	inputs[2] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // volume +/-
	if (inputs[2]<=0) {
		LOG_info("/dev/input/event3 open failed"); // volume +/-
	}
	//Stick_init(); // analog

	// test to simulate a volume down pressure to let rg353v working...
	struct input_event event;
	event.type = EV_KEY;
	event.code = RAW_MINUS;
	event.value = 1;
	write(inputs[2], &event, sizeof(event));
	usleep(20000);
	event.value = 0;
	write(inputs[2], &event, sizeof(event));
}

void PLAT_quitInput(void) {
	//Stick_quit();
	for (int i=0; i<INPUT_COUNT; i++) {
		close(inputs[i]);
	}
}

void PLAT_pollInput(void) {

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

				if 		(code==_START_RAW)	{ btn = BTN_START; 		id = BTN_ID_START; } 
			    else if (code==_SELECT_RAW)	{ btn = BTN_SELECT; 	id = BTN_ID_SELECT; }
				else if (code==RAW_A)		{ btn = BTN_A; 			id = BTN_ID_A; }
				else if (code==RAW_B)		{ btn = BTN_B; 			id = BTN_ID_B; }

				//LOG_info("key event: %i (%i)\n", code,pressed);fflush(stdout);
				else if (code==RAW_UP) 		{ btn = BTN_DPAD_UP; 	id = BTN_ID_DPAD_UP; }
	 			else if (code==RAW_DOWN)	{ btn = BTN_DPAD_DOWN; 	id = BTN_ID_DPAD_DOWN; }
				else if (code==RAW_LEFT)	{ btn = BTN_DPAD_LEFT; 	id = BTN_ID_DPAD_LEFT; }
				else if (code==RAW_RIGHT)	{ btn = BTN_DPAD_RIGHT; id = BTN_ID_DPAD_RIGHT; }
				else if (code==RAW_X)		{ btn = BTN_X; 			id = BTN_ID_X; }
				else if (code==RAW_Y)		{ btn = BTN_Y; 			id = BTN_ID_Y; }
				 
				else if (code==RAW_L1)		{ btn = BTN_L1; 		id = BTN_ID_L1; }
				else if (code==RAW_L2)		{ btn = BTN_L2; 		id = BTN_ID_L2; }				
				else if (code==RAW_R1)		{ btn = BTN_R1; 		id = BTN_ID_R1; }
				else if (code==RAW_R2)		{ btn = BTN_R2; 		id = BTN_ID_R2; }
				else if (code==_L3_RAW)		{ btn = BTN_L3; 		id = BTN_ID_L3; }
				else if (code==_R3_RAW)		{ btn = BTN_R3; 		id = BTN_ID_R3; }

				else if (code==_MENU_RAW)	{ 
							btn = BTN_MENU; 		id = BTN_ID_MENU; 
							// hack to generate a pwr button
				/* 			if (pressed){
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
							}					 */
					}
				else if (code==RAW_PLUS)	{ btn = BTN_PLUS; 		id = BTN_ID_PLUS; }
				else if (code==RAW_MINUS)	{ btn = BTN_MINUS; 		id = BTN_ID_MINUS; }
				else if (code==RAW_POWER)	{ btn = BTN_POWER; 		id = BTN_ID_POWER; }
			}
			if (type==EV_ABS) {  // (range -1800 0 +1800)

				if (code==0) {  //left stick horizontal analog (-1800 left / +1800 right)
					pad.laxis.x =  map(value ,-1800,1800,-0x7fff,0x7fff);
					if (pad.map_leftstick_to_dpad)
						PAD_setAnalog(BTN_ID_DPAD_LEFT, BTN_ID_DPAD_RIGHT, pad.laxis.x, tick+PAD_REPEAT_DELAY);
				}
				if (code==1) {  //left stick vertical analog (-1800 up / +1800 down)
					pad.laxis.y =  map(value ,-1800,1800,-0x7fff,0x7fff);
					if (pad.map_leftstick_to_dpad)
						PAD_setAnalog(BTN_ID_DPAD_UP,   BTN_ID_DPAD_DOWN,  pad.laxis.y, tick+PAD_REPEAT_DELAY);
				}
				if (code==3) {  //right stick horizontal analog (-1800 left / +1800 right)
					pad.raxis.x =  map(value ,-1800,1800,-0x7fff,0x7fff);
				}
				if (code==4) {  //right stick vertical analog (-1800 up / +1800 down)
					pad.raxis.y =  map(value ,-1800,1800,-0x7fff,0x7fff);
				}
			}
			//if ((btn!=BTN_NONE)&&(btn!=BTN_MENU)) PWR_Actions = 1;
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
	void *fbmmap[2]; //mmap address of the framebuffer
	SDL_Surface* screen;  //swsurface to let sdl thinking it's the screen
	SDL_Surface* screen2; //stretched SDL2 surface
	int linewidth;
	int screen_size;
	int width;  //current width 
	int height; // current height
	int pitch;  //sdl bpp
	int sharpness; //let's see if it works
	uint16_t* pixels;
	int rotate;
	int page;
	int numpages;
	int offset;
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
		"ALPHA: L=%d, O=%d\n",
		vid.vinfo.xres, vid.vinfo.yres, vid.vinfo.xres_virtual,
		vid.vinfo.yres_virtual, vid.vinfo.bits_per_pixel,
		vid.vinfo.red.length, vid.vinfo.red.offset,
		vid.vinfo.blue.length,vid.vinfo.blue.offset,
		vid.vinfo.green.length,vid.vinfo.green.offset,
		vid.vinfo.transp.length,vid.vinfo.transp.offset);

    //fprintf(stdout, "PixelFormat is %d\n", vinfo.pixelformat);
    fflush(stdout);
}

void set_fbinfo(void){
    ioctl(vid.fdfb, FBIOPUT_VSCREENINFO, &vid.vinfo);
}

void pan_display(int page){
	vid.vinfo.yoffset = (vid.vinfo.yres_virtual/2) * page;
	ioctl(vid.fdfb, FBIOPAN_DISPLAY, &vid.vinfo);
}

int R36_SDLFB_Flip(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
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
				*((uint32_t *)fbmmap + x + y * linewidth) = (uint32_t)(0x00000000 | ((pixel & 0xF800) << 8) | ((pixel & 0x7E0) << 5) | ((pixel & 0x1F) << 3));
			}
		}
	}
	if (buffer->format->BitsPerPixel == 32) {
		//ok start conversion assuming it is ABGR888
		for (y = targetarea.y; y < (targetarea.y + targetarea.h) ; y++) {
			for (x = targetarea.x; x < (targetarea.x + targetarea.w); x++) {
				uint32_t pixel = *((uint32_t *)buffer->pixels + x + y * thispitch);
				*((uint32_t *)fbmmap + x + y * linewidth) = 
					0x00000000 | ((pixel & 0xFF0000) >> 16) | (pixel & 0xFF00)  | ((pixel & 0xFF) << 16);
			}
		}
	}
	return 0;	
}

int R36_SDLFB_FlipRotate180(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
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
				*((uint32_t *)fbmmap + (buffer->w - x) + (buffer->h - y -1) * linewidth) = (uint32_t)(0xFF000000 | ((pixel & 0xF800) << 8) | ((pixel & 0x7E0) << 5) | ((pixel & 0x1F) << 3));
			}
		}
	}
	if (buffer->format->BitsPerPixel == 32) {
		//ok start conversion assuming it is ABGR888
		for (y = targetarea.y; y < (targetarea.y + targetarea.h) ; y++) {
			for (x = targetarea.x; x < (targetarea.x + targetarea.w); x++) {
				uint32_t pixel = *((uint32_t *)buffer->pixels + x + y * thispitch);
				*((uint32_t *)fbmmap + (buffer->w - x) + (buffer->h - y -1) * linewidth) = 
					0xFF000000 | ((pixel & 0xFF0000) >> 16) | (pixel & 0xFF00)  | ((pixel & 0xFF) << 16);
			}
		}
	}
	return 0;	
}

int R36_SDLFB_FlipRotate90(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
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
int R36_SDLFB_FlipRotate270(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
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

void IOCTLttyON(void){
	int mode = -1;
	int fdtty = -1;
	fdtty = open("/dev/tty1", O_RDWR);
	int res = ioctl(fdtty, KDGETMODE, &mode);
	LOG_info("Console mode = %d Result = %d\n", mode,res);
	if (mode == 0){
		LOG_info("Console mode = %d Result = %d\n", mode,res);
		mode = 1;
		res = -1;
		res = ioctl(fdtty, KDSETMODE, mode);
		if (res < 0) {
			LOG_info("Console mode now = %d Result = %d errno = %s \n",mode, res,strerror(errno));//\n", mode,res);
		} else {
			LOG_info("Console mode now = %d Result = %d\n",mode, res);//\n", mode,res);
		}	
	}
	close(fdtty);
}

SDL_Surface* PLAT_initVideo(void) {
	IOCTLttyON();
	vid.fdfb = open("/dev/fb0", O_RDWR);
	int w,p,h;
	get_fbinfo();
	w = FIXED_WIDTH;
	h = FIXED_HEIGHT;
	p = FIXED_PITCH;	
	vid.numpages = 1;
	if (exists("/dev/input/by-path/platform-fe5b0000.i2c-event")) {
		//is the rg353v
		is353v = 1;
		isg350 = 0;
		if (GetHDMI()) {
			w = _HDMI_WIDTH;
			h = _HDMI_HEIGHT;
			p = _HDMI_PITCH;
		} 
	}
	
	DEVICE_WIDTH = w;
	DEVICE_HEIGHT = h;
	DEVICE_PITCH = p;
	vid.rotate = 0;


	if (exists(ROTATE_SYSTEM_PATH)) {
		vid.rotate = getInt(ROTATE_SYSTEM_PATH) &3;
	}

	if (vid.rotate % 2 == 1) {
		DEVICE_WIDTH = h;
		DEVICE_HEIGHT = w;
	}
	
    vid.vinfo.xres=w;
    vid.vinfo.yres=h;
	vid.vinfo.bits_per_pixel=32;
	//at the beginning set the screen size to 640x480
    set_fbinfo();
	get_fbinfo();
	LOG_info("DEVICE_WIDTH=%d, DEVICE_HEIGHT=%d\n", DEVICE_WIDTH, DEVICE_HEIGHT);fflush(stdout);
//	if (getInt(HDMI_STATE_PATH)) {
//		w = HDMI_WIDTH;
//		h = HDMI_HEIGHT;
//		p = HDMI_PITCH;
//	}

	int error_code;
	for (int i = 0; i < 11; i++) {
		int cnow = SDL_GetTicks();
		usleep(7000);
		pan_display(0);
		printf("FBIOPAN_DISPLAY took %i msec, error code is %i\n", SDL_GetTicks()-cnow, error_code);fflush(stdout);
	}
	
	for (int i = 0; i < 11; i++) {
		int cnow = SDL_GetTicks();
		usleep(7000);
		set_fbinfo();
		printf("FBIOPUT_VSCREENINFO took %i msec, error code is %i\n", SDL_GetTicks()-cnow, error_code);fflush(stdout);
	}

	for (int i = 0; i < 11; i++) {
		int cnow = SDL_GetTicks();
		usleep(7000);
		int _;
		error_code = ioctl(vid.fdfb, FBIOBLANK, &_); 
		printf("FBIOBLANK took %i msec, error code is %i\n", SDL_GetTicks()-cnow, error_code);fflush(stdout);
	}
	
	for (int i = 0; i < 11; i++) {
		int cnow = SDL_GetTicks();
		usleep(7000);
		int _;
		error_code = ioctl(vid.fdfb, FBIO_WAITFORVSYNC, &_); 
		printf("FBIO_WAITFORVSYNC took %i msec, error code is %i\n", SDL_GetTicks()-cnow, error_code);fflush(stdout);
	}


	if (vid.vinfo.yres_virtual >= vid.vinfo.yres*2) {
		vid.numpages = 2;
	}
	vid.page = 0;
	//pan_display(vid.page);
	vid.renderingGame = 0;
	
	vid.offset = vid.vinfo.xres*vid.vinfo.yres*(vid.vinfo.bits_per_pixel/8);
	vid.screen_size = vid.offset*vid.numpages;
	vid.screen =  SDL_CreateRGBSurface(0, DEVICE_WIDTH, DEVICE_HEIGHT, FIXED_DEPTH, RGBA_MASK_565);
	vid.screen2 = SDL_CreateRGBSurface(0, DEVICE_WIDTH, DEVICE_HEIGHT, FIXED_DEPTH, RGBA_MASK_565); 

	vid.linewidth = vid.finfo.line_length/(vid.vinfo.bits_per_pixel/8);
	//create a mmap with the maximum available memory, we avoid recreating it during the resize as it is useless and waste of time.	
	
	vid.fbmmap[0] = malloc(vid.screen_size/vid.numpages *(sizeof(uint8_t)));
	vid.fbmmap[1] = malloc(vid.screen_size/vid.numpages *(sizeof(uint8_t)));
	vid.sharpness = SHARPNESS_SOFT;
	return vid.screen;
}

void PLAT_quitVideo(void) {
	close(vid.fdfb);
	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.screen2);
	free(vid.fbmmap[0]);
	free(vid.fbmmap[1]);
}

void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_FillRect(vid.screen, NULL, 0); // TODO: revisit
	SDL_FillRect(vid.screen2, NULL, 0);
}

void  PLAT_clearAll (void) {
	SDL_FillRect(vid.screen, NULL, 0); // TODO: revisit
	SDL_FillRect(vid.screen2, NULL, 0);
	memset(vid.fbmmap[0], 0, vid.screen_size/vid.numpages);
	memset(vid.fbmmap[1], 0, vid.screen_size/vid.numpages);
	pwrite(vid.fdfb, vid.fbmmap[0], vid.screen_size/vid.numpages,0);
	pwrite(vid.fdfb, vid.fbmmap[1], vid.screen_size/vid.numpages,vid.offset);
	//lseek(vid.fdfb,vid.offset*vid.page,0);
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
//	if (remaining>0) usleep(remaining*1000);
	int res = 0;
	ioctl(vid.fdfb, FBIOBLANK, &res);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	return NULL;
}

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
		//vid.page = 0;
	}
	if (vid.rotate == 0) 
	{
		// No Rotation
		R36_SDLFB_Flip(vid.screen, vid.fbmmap[vid.page],vid.linewidth, vid.targetRect);
	}
	if (vid.rotate == 1)
	{
		// 90 Rotation
		R36_SDLFB_FlipRotate90(vid.screen, vid.fbmmap[vid.page],vid.linewidth, vid.targetRect);
	}
	if (vid.rotate == 2)
	{
		// 180 Rotation
		R36_SDLFB_FlipRotate180(vid.screen, vid.fbmmap[vid.page],vid.linewidth, vid.targetRect);
	}
	if (vid.rotate == 3)
	{
		// 270 Rotation
		R36_SDLFB_FlipRotate270(vid.screen, vid.fbmmap[vid.page],vid.linewidth, vid.targetRect);
	}
//	int now2 = SDL_GetTicks();

	if (vid.numpages == 1) {
		vid.page = 0;
		pan_display(vid.page);
		pwrite(vid.fdfb, vid.fbmmap[vid.page], vid.offset, vid.offset * vid.page);
	} else {
		pwrite(vid.fdfb, vid.fbmmap[vid.page], vid.offset, vid.offset * vid.page);
		pan_display(vid.page);
		vid.page = 1-vid.page;
	}
	vid.renderingGame = 0;	
//	LOG_info("Total Flip TOOK: %imsec, Draw TOOK: %imsec\n", SDL_GetTicks()-now, now2-now);
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
	return "R36S/353 ArkOS";
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
//buh!
}