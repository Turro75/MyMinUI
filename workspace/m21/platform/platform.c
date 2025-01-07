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


///////////////////////////////
void Main_Flip(void);

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

static int PWR_Pressed = 0;
static int PWR_Actions = 0;
static uint32_t PWR_Tick = 0;
#define PWR_TIMEOUT 2000
int last_dpad_used[2];
int selectstartstatus[2] = {0}; 
int selectstartlaststatus[2] = {0}; 



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

				if (i > 0) { //external controllers
				//special handling as the sjgam external controller does not provide menu button but only select+start, 
				//let's find a way to simulate menu button when select+start is detected
					if (code==RAW_START)	{ btn = BTN_START; 		id = BTN_ID_START; pressed ? selectstartstatus[i-1]++ : selectstartstatus[i-1]--; } 
				    if (code==RAW_SELECT)	{ btn = BTN_SELECT; 	id = BTN_ID_SELECT; pressed ? selectstartstatus[i-1]++ : selectstartstatus[i-1]--;}
					if (selectstartstatus[i-1] == 2) {
							btn = BTN_MENU;  id = BTN_ID_MENU; 
							selectstartlaststatus[i-1]=1; 
							pad.is_pressed		&= ~BTN_SELECT; // unset
							pad.just_repeated	&= ~BTN_SELECT; // unset	
							pad.is_pressed		&= ~BTN_START; // unset
							pad.just_repeated	&= ~BTN_START; // unset						
							}
					if ((selectstartstatus[i-1] == 1) && (selectstartlaststatus[i-1] == 1)) {btn = BTN_MENU; 	id = BTN_ID_MENU; selectstartlaststatus[i-1]=0;}	
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
			if ((btn!=BTN_NONE)&&(btn!=BTN_MENU)) PWR_Actions = 1;
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
		if (event.type==EV_KEY && event.code==RAW_MENU && event.value==0) {
			return 1;
		}
	}	
	return 0;
	}
	
}

static struct VID_Context {
	int fdfb; // /dev/fb0 handler
	struct fb_fix_screeninfo finfo;  //fixed fb info
	struct fb_var_screeninfo vinfo;  //adjustable fb info
	void *fbmmap; //mmap address of the framebuffer
	SDL_Surface* screen;  //swsurface to let sdl thinking it's the screen
	SDL_Surface* screen2;  //used to apply screen_effect
	GFX_Renderer* blit; // yeesh, let's see if useful
	int linewidth;
	int screen_size;
	int width;  //current width 
	int height; // current height
	int pitch;  //sdl bpp
	int sharpness; //let's see if it works
	uint16_t* pixels;
	int rotate;
} vid;

static int device_width;
static int device_height;
static int device_pitch;

static int lastw=0;
static int lasth=0;
static int lastp=0;


void get_fbinfo(void){
    ioctl(vid.fdfb, FBIOGET_FSCREENINFO, &vid.finfo);
    ioctl(vid.fdfb, FBIOGET_VSCREENINFO, &vid.vinfo);
/*	
    fprintf(stdout, "Fixed screen informations\n"
                    "-------------------------\n"
                    "Id string: %s\n"
                    "FB start memory: %p\n"
                    "FB LineLength: %d\n",
                    finfo.id, (void *)finfo.smem_start, finfo.line_length);

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
                    vinfo.xres, vinfo.yres, vinfo.xres_virtual,
                    vinfo.yres_virtual, vinfo.bits_per_pixel,
                    vinfo.red.length, vinfo.red.offset,
                    vinfo.blue.length,vinfo.blue.offset,
                    vinfo.green.length,vinfo.green.offset,
                    vinfo.transp.length,vinfo.transp.offset);

    //fprintf(stdout, "PixelFormat is %d\n", vinfo.pixelformat);
    fflush(stdout);*/
}

void set_fbinfo(void){
    ioctl(vid.fdfb, FBIOPUT_VSCREENINFO, &vid.vinfo);
}

int M21_SDLFB_Flip(SDL_Surface *buffer, void * fbmmap, int linewidth) {
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
				*((uint32_t *)fbmmap + x + y * linewidth) = (uint32_t)(0xFF000000 | ((pixel & 0xF800) << 8) | ((pixel & 0x7E0) << 5) | ((pixel & 0x1F) << 3));
			}
		}
	}
	if (buffer->format->BitsPerPixel == 32) {
		//ok start conversion assuming it is ABGR888
		for (y = 0; y < buffer->h; y++) {
			for (x = 0; x < buffer->w; x++) {
				uint32_t pixel = *((uint32_t *)buffer->pixels + x + y * thispitch);
				*((uint32_t *)fbmmap + x + y * linewidth) = 
					0xFF000000 | ((pixel & 0xFF0000) >> 16) | (pixel & 0xFF00)  | ((pixel & 0xFF) << 16);
			}
		}
	}
	return 0;	
}

int M21_SDLFB_FlipRotate180(SDL_Surface *buffer, void * fbmmap, int linewidth) {
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
				*((uint32_t *)fbmmap + (buffer->w - x) + (buffer->h - y -1) * linewidth) = (uint32_t)(0xFF000000 | ((pixel & 0xF800) << 8) | ((pixel & 0x7E0) << 5) | ((pixel & 0x1F) << 3));
			}
		}
	}
	if (buffer->format->BitsPerPixel == 32) {
		//ok start conversion assuming it is ABGR888
		for (y = 0; y < buffer->h; y++) {
			for (x = 0; x < buffer->w; x++) {
				uint32_t pixel = *((uint32_t *)buffer->pixels + x + y * thispitch);
				*((uint32_t *)fbmmap + (buffer->w - x) + (buffer->h - y -1) * linewidth) = 
					0xFF000000 | ((pixel & 0xFF0000) >> 16) | (pixel & 0xFF00)  | ((pixel & 0xFF) << 16);
			}
		}
	}
	return 0;	
}

int M21_SDLFB_FlipRotate90(SDL_Surface *buffer, void * fbmmap, int linewidth) {
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
				*((uint32_t *)fbmmap + (buffer->h- y -1) + (x  * linewidth)) = (uint32_t)(0xFF000000 | ((pixel & 0xF800) << 8) | ((pixel & 0x7E0) << 5) | ((pixel & 0x1F) << 3));
			}
		}
	}
	if (buffer->format->BitsPerPixel == 32) {
		//ok start conversion assuming it is ABGR888
		for (y = 0; y < buffer->h; y++) {
			for (x = 0; x < buffer->w; x++) {
				uint32_t pixel = *((uint32_t *)buffer->pixels + x + y * thispitch);
				*((uint32_t *)fbmmap + (buffer->h- y -1) + (x  * linewidth)) = 
					0xFF000000 | ((pixel & 0xFF0000) >> 16) | (pixel & 0xFF00)  | ((pixel & 0xFF) << 16);
			}
		}
	}
	return 0;	
}
int M21_SDLFB_FlipRotate270(SDL_Surface *buffer, void * fbmmap, int linewidth) {
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
				*((uint32_t *)fbmmap + y + (buffer->w - x - 1) * linewidth) = (uint32_t)(0xFF000000 | ((pixel & 0xF800) << 8) | ((pixel & 0x7E0) << 5) | ((pixel & 0x1F) << 3));
			}
		}
	}
	if (buffer->format->BitsPerPixel == 32) {
		//ok start conversion assuming it is ABGR888
		for (y = 0; y < buffer->h; y++) {
			for (x = 0; x < buffer->w; x++) {
				uint32_t pixel = *((uint32_t *)buffer->pixels + x + y * thispitch);
				*((uint32_t *)fbmmap + y + (buffer->w - x - 1) * linewidth) = 
					0xFF000000 | ((pixel & 0xFF0000) >> 16) | (pixel & 0xFF00)  | ((pixel & 0xFF) << 16);
			}
		}
	}
	return 0;	
}


SDL_Surface* PLAT_initVideo(void) {
	vid.fdfb = open("/dev/fb0", O_RDWR);

#ifndef M22
	//M21
	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;
#else
	//M22
	int w = FIXED_HEIGHT;
	int h = FIXED_WIDTH;
	int p = FIXED_PITCH;
#endif
	DEVICE_WIDTH = w;
	DEVICE_HEIGHT = h;
	vid.rotate = 0;	
	get_fbinfo();	

	if (exists(ROTATE_SYSTEM_PATH)) {
		vid.rotate = getInt(ROTATE_SYSTEM_PATH) &3;
	}
#ifndef M22
	//M21
	if (vid.rotate % 2 == 1) {
		DEVICE_WIDTH = FIXED_HEIGHT;
		DEVICE_HEIGHT = FIXED_WIDTH;
	}
#else
	//M22
	if (vid.rotate % 2 == 0) {
		DEVICE_WIDTH = FIXED_WIDTH;
		DEVICE_HEIGHT = FIXED_HEIGHT;
	}
#endif
    vid.vinfo.xres=w;
    vid.vinfo.yres=h;
	vid.vinfo.bits_per_pixel=32;

	int m = vid.vinfo.xres>vid.vinfo.yres?vid.vinfo.xres:vid.vinfo.yres;
	//at the beginning set the screen size to 640x480
    set_fbinfo();
	get_fbinfo();
	
//	if (getInt(HDMI_STATE_PATH)) {
//		w = HDMI_WIDTH;
//		h = HDMI_HEIGHT;
//		p = HDMI_PITCH;
//	}
	vid.pixels = malloc(m*p);
	vid.screen = SDL_CreateRGBSurfaceFrom(vid.pixels, DEVICE_WIDTH, DEVICE_HEIGHT, FIXED_DEPTH, p, RGBA_MASK_565);
	vid.screen2 = SDL_CreateRGBSurface(0, DEVICE_WIDTH, DEVICE_HEIGHT, FIXED_DEPTH, RGBA_MASK_565); 

	vid.linewidth = vid.finfo.line_length/(vid.vinfo.bits_per_pixel/8);

	//create a mmap with the maximum available memory, we avoid recreating it during the resize as it is useless and waste of time.
	vid.screen_size = vid.finfo.line_length * m;
	vid.fbmmap = malloc(vid.screen_size *(sizeof(uint8_t)));
	vid.sharpness = SHARPNESS_SOFT;
	return vid.screen;
}

void PLAT_quitVideo(void) {
		// clearVideo();
	PLAT_clearAll();
	if (vid.pixels) free(vid.pixels);
	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.screen2);
	vid.pixels=NULL;
	munmap(vid.fbmmap, 0);	
    close(vid.fdfb);
	SDL_Quit();
}

static void clearVideo(void) {
	SDL_FillRect(vid.screen, NULL, 0);
	SDL_FillRect(vid.screen2, NULL, 0);
	memset(vid.fbmmap, 0, vid.screen_size);
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
	if (w==vid.width && h==vid.height && p==vid.pitch) return;
	int targetw = w;
	int targeth = h;
	if (game == 1){  //check final rotation only if in game
	//	if (finalrotate % 2 == 1) {
	//	targetw = h;
	//	targeth = w;
	//	}
	}
	// TODO: minarch disables crisp (and nn upscale before linear downscale) when native
	if (targetw < 0) targetw = targetw* -1;
	if (targeth < 0) targeth = targeth* -1;
	
	if (vid.screen) {
		SDL_FreeSurface(vid.screen);
	}
	
	get_fbinfo();
    vid.vinfo.xres=targetw;
    vid.vinfo.yres=targeth;
	
	vid.vinfo.yres_virtual=targeth;
	vid.vinfo.xres_virtual=vid.vinfo.xres;
	vid.vinfo.bits_per_pixel=32;
    set_fbinfo();
	get_fbinfo();

	//vid.screen	= SDL_CreateRGBSurface(SDL_SWSURFACE,w,h, FIXED_DEPTH, RGBA_MASK_565);
	//if (pixels) free(pixels);
	vid.pixels = (uint16_t *)realloc(vid.pixels,h*p);
	vid.screen = SDL_CreateRGBSurfaceFrom(vid.pixels, w, h, FIXED_DEPTH, p, RGBA_MASK_565);
	
	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
	//LOG_info("resizeVideo(%i,%i,%i) hard_scale: %i crisp: %i\n",w,h,p, hard_scale,vid.sharpness==SHARPNESS_CRISP);
	LOG_info("resizeVideo(%i,%i,%i)\n",w,h,p);
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
	if (remaining>0) usleep(remaining*1000);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
/*	if (effect_type==EFFECT_LINE) {
		switch (renderer->scale) {
			case 4:  return scale4x_line;
			case 3:  return scale3x_line;
			case 2:  return scale2x_line;
			default: return scale1x_line;
		}
	}
	else if (effect_type==EFFECT_GRID) {
		switch (renderer->scale) {
			case 3:  return scale3x_grid;
			case 2:  return scale2x_grid;
		}
	}
	
	switch (renderer->scale) {
		case 6:  return scale6x6_n16;
		case 5:  return scale5x5_n16;
		case 4:  return scale4x4_n16;
		case 3:  return scale3x3_n16;
		case 2:  return scale2x2_n16;
		default: return scale1x1_n16;
	}*/
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
}


void PLAT_flip(SDL_Surface* IGNORED, int ignored) { //this rotates minarch menu + minui + tools

#ifndef M22
	if (vid.rotate == 0) 
	{
		// No Rotation
		M21_SDLFB_Flip(vid.screen, vid.fbmmap,vid.linewidth);
	}
	if (vid.rotate == 1)
	{
		// 90 Rotation
		M21_SDLFB_FlipRotate90(vid.screen, vid.fbmmap,vid.linewidth);
	}
	if (vid.rotate == 2)
	{
		// 180 Rotation
		M21_SDLFB_FlipRotate180(vid.screen, vid.fbmmap,vid.linewidth);
	}
	if (vid.rotate == 3)
	{
		// 270 Rotation
		M21_SDLFB_FlipRotate270(vid.screen, vid.fbmmap,vid.linewidth);
	}
#else
	if (vid.rotate == 0) 
	{
		// No Rotation
		M21_SDLFB_FlipRotate270(vid.screen, vid.fbmmap,vid.linewidth);
	}
	if (vid.rotate == 1)
	{
		// 90 Rotation
		M21_SDLFB_Flip(vid.screen, vid.fbmmap,vid.linewidth);
	}
	if (vid.rotate == 2)
	{
		// 180 Rotation
		M21_SDLFB_FlipRotate90(vid.screen, vid.fbmmap,vid.linewidth);
	}
	if (vid.rotate == 3)
	{
		// 270 Rotation
		M21_SDLFB_FlipRotate180(vid.screen, vid.fbmmap,vid.linewidth);
	}
#endif
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
	if (value){
		ioctl(disp_fd, DISP_LCD_BACKLIGHT_ENABLE, args);
	} else {
		ioctl(disp_fd, DISP_LCD_BACKLIGHT_DISABLE, args);
	}	
	close(disp_fd);
}


void PLAT_enableBacklight(int enable) {
    if (enable){
		SetBrightness(GetBrightness());
        rawBacklight(1);		
    } else {
		//rawBacklight(0);
		SetRawBrightness(255);
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
		case CPU_SPEED_MENU: 		freq = 480000; break;
		case CPU_SPEED_POWERSAVE:	freq = 912000; break;
		case CPU_SPEED_NORMAL: 		freq = 1080000; break;
		case CPU_SPEED_PERFORMANCE: freq = 1104000; break;
		case CPU_SPEED_MAX:			freq = 1200000; break;
	}
	putFile(GOVERNOR_PATH, "userspace");
	putInt(GOVERNOR_CPUSPEED_PATH, freq);
}

void PLAT_setRumble(int strength) {
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
	FILE * __stream = fopen("/sys/class/extcon/extcon0/state","r");
    if (__stream == (FILE *)0x0) {
      setenv("AUDIODEV","default",1);
    }
    else {
	  char acStack_128 [260];
      memset(acStack_128,0,0x100);
      fread(acStack_128,0x100,1,__stream);
      char *pcVar10 = strstr(acStack_128,"HDMI=1");
      if (pcVar10 == (char *)0x0) {
        setenv("AUDIODEV","default",1);
		LOG_info("VideoOutput default\n");
      }
      else {
        setenv("AUDIODEV","audioHDMI",1);
		LOG_info("VideoOutput audioHDMI\n");
      }
      fclose(__stream);
    }
}