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

#define ISM22_PATH "/mnt/SDCARD/m21/thisism22"

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
							btn = BTN_MENU;  id = BTN_ID_MENU; 
							selectstartlaststatus[i]=1; 
							pad.is_pressed		&= ~BTN_SELECT; // unset
							pad.just_repeated	&= ~BTN_SELECT; // unset	
							pad.is_pressed		&= ~BTN_START; // unset
							pad.just_repeated	&= ~BTN_START; // unset						
							}
					if ((selectstartstatus[i] == 1) && (selectstartlaststatus[i] == 1)) {btn = BTN_MENU; 	id = BTN_ID_MENU; selectstartlaststatus[i]=0;}	
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
		if (event.type==EV_KEY && (event.code==RAW_MENU || ( ism22 && event.code==RAW_SELECT)) && event.value==0) {
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

int M21_SDLFB_Flip(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
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

int M21_SDLFB_FlipRotate180(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
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

int M21_SDLFB_FlipRotate90(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
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
int M21_SDLFB_FlipRotate270(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
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

SDL_Surface* PLAT_initVideo(void) {
	ism22 = 0;
	if (exists(ISM22_PATH)) {
		ism22 = 1;
	}
//	system("cat /sys/class/disp/disp/attr/sys > /mnt/SDCARD/sys0.txt");
//	system("cat /sys/class/graphics/fb0/mode > /mnt/SDCARD/mode.txt");
//	system("cat /sys/class/graphics/fb0/modes > /mnt/SDCARD/modes.txt");
//	system ("/usr/sbin/fbset -i > /mnt/SDCARD/fbset.txt");
//	system(" /usr/sbin/fbset -g 35242 64 96 35 12 112 2 ");
//	system ("/usr/sbin/fbset -i >> /mnt/SDCARD/fbset.txt");


	vid.fdfb = open("/dev/fb0", O_RDWR);
	
	//system("cat /sys/class/disp/disp/attr/sys > /mnt/SDCARD/sysA.txt");
	int w,h,p;
	int behavior = 0;
	if (getHDMIStatus() || (ism22)) {
		w = _HDMI_WIDTH;
		h = _HDMI_HEIGHT;
		p = _HDMI_PITCH;
	} else {
		//is an m21
		w = FIXED_WIDTH;
		h = FIXED_HEIGHT;
		p = FIXED_PITCH;		
	}

	DEVICE_WIDTH = w;
	DEVICE_HEIGHT = h;
	DEVICE_PITCH = p;
	vid.rotate = ism22 * (1 - getHDMIStatus());   //m21 always 0, m22 is 0 on HDMI and 1 on display

	get_fbinfo();	

	if (exists( ROTATE_SYSTEM_PATH )) {
		vid.rotate = getInt( ROTATE_SYSTEM_PATH ) & 3;
	}
	
	vid.vinfo.xres=w;
	vid.vinfo.yres=h;
	

	if (vid.rotate % 2 == 1) {
		vid.vinfo.xres=h;
		vid.vinfo.yres=w;
	}
	vid.vinfo.xoffset=0;
	vid.vinfo.yoffset=0;
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
	
	
	vid.vinfo.bits_per_pixel=32;

//	int m = vid.vinfo.xres>vid.vinfo.yres?vid.vinfo.xres:vid.vinfo.yres;
	//at the beginning set the screen size to 640x480
	vid.vinfo.xres_virtual=vid.vinfo.xres;
	vid.vinfo.yres_virtual=vid.vinfo.yres*2;
//	vid.vinfo.pixclock = 20250;
//	vid.vinfo.right_margin = 0;
    set_fbinfo();
	get_fbinfo();

	

	unsigned int arg[3];
	arg[0] = 0;
	arg[1] = 1;
	int zero = 0;
	
#define DISP_VSYNC_EVENT_EN 0x0B
	printf("ABILITAZIONE VSYNC RESULT = %d\n",ioctl(vid.fdfb, DISP_VSYNC_EVENT_EN, (void*)arg)); fflush (stdout);	

//	system("cat /sys/class/disp/disp/attr/sys > /mnt/SDCARD/sys1.txt");
//	if (getInt(HDMI_STATE_PATH)) {
//		w = HDMI_WIDTH;
//		h = HDMI_HEIGHT;
//		p = HDMI_PITCH;
//	}
	vid.vinfo.xoffset=0;
	vid.vinfo.yoffset=0;
	int error_code;
	for (int i = 0; i < 11; i++) {
		int cnow = SDL_GetTicks();
		//usleep(5000);
		vid.vinfo.yoffset = (i%2) * (vid.vinfo.yres_virtual/2);
		error_code = ioctl(vid.fdfb, FBIOPAN_DISPLAY, &vid.vinfo); 
		printf("FBIOPAN_DISPLAY Y delay took %i msec, error code is %i\n", SDL_GetTicks()-cnow, error_code);fflush(stdout);
	}
	vid.vinfo.xoffset=0;
	vid.vinfo.yoffset=0;
	for (int i = 0; i < 11; i++) {
		int cnow = SDL_GetTicks();
		//usleep(5000);
		vid.vinfo.xoffset = (i%2) * (vid.vinfo.xres_virtual/2);
		error_code = ioctl(vid.fdfb, FBIOPAN_DISPLAY, &vid.vinfo); 
		printf("FBIOPAN_DISPLAY X took %i msec, error code is %i\n", SDL_GetTicks()-cnow, error_code);fflush(stdout);
	}
	vid.vinfo.xoffset=0;
	vid.vinfo.yoffset=0;
	for (int i = 0; i < 11; i++) {
		int cnow = SDL_GetTicks();		
		vid.vinfo.yoffset = (i%2) * (vid.vinfo.yres_virtual/2);
		error_code = ioctl(vid.fdfb, FBIOPAN_DISPLAY, &vid.vinfo); 
		printf("FBIOPAN_DISPLAY Y NO DELAY took %i msec, error code is %i\n", SDL_GetTicks()-cnow, error_code);fflush(stdout);
	}
	vid.vinfo.xoffset=0;
	vid.vinfo.yoffset=0;
	for (int i = 0; i < 11; i++) {
		int cnow = SDL_GetTicks();
		//usleep(5000*(i%2));
		vid.vinfo.xoffset = (i%2) * (vid.vinfo.xres_virtual/2);
		error_code = ioctl(vid.fdfb, FBIOPAN_DISPLAY, &vid.vinfo); 
		printf("FBIOPAN_DISPLAY X NO DELAY took %i msec, error code is %i\n", SDL_GetTicks()-cnow, error_code);fflush(stdout);
	}
	vid.vinfo.xoffset=0;
	vid.vinfo.yoffset=0;
	for (int i = 0; i < 11; i++) {
		int cnow = SDL_GetTicks();
	//	usleep(5000);
		vid.vinfo.yoffset = (i%2) * (vid.vinfo.yres_virtual/2);
		error_code = ioctl(vid.fdfb, FBIOPUT_VSCREENINFO, &vid.vinfo);
		printf("FBIOPUT_VSCREENINFO took %i msec, error code is %i\n", SDL_GetTicks()-cnow, error_code);fflush(stdout);
	}

	for (int i = 0; i < 11; i++) {
		int cnow = SDL_GetTicks();
	//	usleep(5000);
		int _;
		error_code = ioctl(vid.fdfb, FBIOBLANK, &_); 
		printf("FBIOBLANK took %i msec, error code is %i\n", SDL_GetTicks()-cnow, error_code);fflush(stdout);
	}

	for (int i = 0; i < 11; i++) {
		int cnow = SDL_GetTicks();
	//	usleep(5000);
		int _;
		error_code = ioctl(vid.fdfb, FBIO_WAITFORVSYNC, &_); 
		printf("FBIO_WAITFORVSYNC took %i msec, error code is %i\n", SDL_GetTicks()-cnow, error_code);fflush(stdout);
	}

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
	vid.screen2 = SDL_CreateRGBSurface(0, DEVICE_WIDTH, DEVICE_HEIGHT, FIXED_DEPTH, RGBA_MASK_565); 
	//create a mmap with the maximum available memory, we avoid recreating it during the resize as it is useless and waste of time.
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
	}
	
	if (vid.rotate == 0) 
	{
		// No Rotation
		M21_SDLFB_Flip(vid.screen, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
	}
	if (vid.rotate == 1)
	{
		// 90 Rotation
		M21_SDLFB_FlipRotate90(vid.screen, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
	}
	if (vid.rotate == 2)
	{
		// 180 Rotation
		M21_SDLFB_FlipRotate180(vid.screen, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
	}
	if (vid.rotate == 3)
	{
		// 270 Rotation
		M21_SDLFB_FlipRotate270(vid.screen, vid.fbmmap+vid.page*vid.offset,vid.linewidth, vid.targetRect);
	}
	vid.renderingGame = 0;
//	pan_display(vid.page);
	if (vid.numpages == 2) {
//		vid.page = 1 - vid.page;
	}
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
	if (getHDMIStatus()) {
		setenv("AUDIODEV","audioHDMI",1);
		LOG_info("VideoOutput audioHDMI\n");
	} else {
		setenv("AUDIODEV","default",1);
		LOG_info("VideoOutput default\n");
	}
}
