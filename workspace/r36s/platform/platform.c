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
#include <xf86drm.h>
#include <xf86drmMode.h>

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
#define RAW_R1_353P	 RAW_R2
#define RAW_R2_353P	 RAW_R1
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
static int is353p = 0;
static int isg350 = 0;

static int _R1_RAW, _R2_RAW, _L3_RAW, _R3_RAW, _START_RAW, _SELECT_RAW, _MENU_RAW;

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
	_R1_RAW = RAW_R1;
	_R2_RAW = RAW_R2;
	if (is353v) {
		inputs[1] = open("/dev/input/event4", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // controller
		_L3_RAW = RAW_L3_353;
		_R3_RAW = RAW_R3_353;

		// check if is set as rg353p then swap R1/R2 to meet rg353p
		if (is353p){
			_R1_RAW = RAW_R2;
	    	_R2_RAW = RAW_R1;
		}

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
	if (inputs[1]<0) {
		LOG_info("/dev/input/event2-4 open failed: is353v=%d - isg350=%d - r36s = %d", is353v, isg350, !is353v && !isg350); // volume +/-
	}
	inputs[2] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // volume +/-
	if (inputs[2]<0) {
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
				else if (code==_R1_RAW)		{ btn = BTN_R1; 		id = BTN_ID_R1; }
				else if (code==_R2_RAW)		{ btn = BTN_R2; 		id = BTN_ID_R2; }
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
	void *fbmmap[2]; //mmap address of the framebuffer
	int fb[2];
	int crtc[2];
	int conn[2];
	int handle[2];
	struct _drmModeModeInfo mode[2];
	SDL_Surface* screen;  //swsurface to let sdl thinking it's the screen
	SDL_Surface* screen2; //stretched SDL2 surface
	int linewidth[2];
	int screen_size[2];
	int width;  //current width 
	int height; // current height
	int pitch;  //sdl bpp
	int sharpness; //let's see if it works
	int rotate;
	int page;
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
					0x00000000 | (pixel & 0xFF0000)  | (pixel & 0xFF00)  | (pixel & 0xFF) ;
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


struct modeset_buf {
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t size;
	uint32_t handle;
	uint8_t *map;
	uint32_t fb;
};

struct drm_mode_destroy_dumb dreq;


SDL_Surface* PLAT_initVideo(void) {
	IOCTLttyON();
	vid.fdfb = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	
	drmModeRes *res;
	drmModeConnector *conn;
	
	int w,p,h;
	//get_fbinfo();
	w = FIXED_WIDTH;
	h = FIXED_HEIGHT;
	p = FIXED_PITCH;	
//	vid.numpages = 1;
	if (exists("/dev/input/by-path/platform-fe5b0000.i2c-event")) {
		//is the rg353v
		is353v = 1;
		isg350 = 0;
		// check here if the file exists to meet rg353p
		if (exists(IS_RG353P)){
			is353p = 1;
		}
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
	

	/* retrieve resources */
	res = drmModeGetResources(vid.fdfb);
	if (!res) {
		fprintf(stdout, "cannot retrieve DRM resources (%d): %m\n",	errno);fflush(stdout);
	}
	/* iterate all connectors */
	LOG_info("Connectors = %d\n", res->count_connectors);fflush(stdout);
	for (int i = 0; i < res->count_connectors; ++i) {
		/* get information for each connector */
		conn = drmModeGetConnector(vid.fdfb, res->connectors[i]);
		if (!conn) {
			fprintf(stdout, "cannot retrieve DRM connector %u:%u (%d): %m\n",
				i, res->connectors[i], errno);fflush(stdout);
			continue;
		}
		//trovato un connettore
		LOG_info("ConnectorID %u\n", conn->connector_id);fflush(stdout);
		if (conn->connection != DRM_MODE_CONNECTED) {
			fprintf(stderr, "ignoring unused connector %u\n",conn->connector_id);fflush(stdout);
		}
		for (int c = 0; c < conn->count_modes; c++) {
			LOG_info("ConnectorID %i : mode %ux%u@%dHz\n", conn->connector_id,conn->modes[c].hdisplay ,conn->modes[c].vdisplay,conn->modes[c].vrefresh);fflush(stdout);
			//found a mode
			
			if (conn->modes[c].vdisplay == DEVICE_HEIGHT && conn->modes[c].hdisplay == DEVICE_WIDTH && conn->modes[c].vrefresh == _HDMI_HZ) {
				LOG_info("ConnectorID %i : mode %ux%u@%dHz found\n", conn->connector_id,conn->modes[c].hdisplay ,conn->modes[c].vdisplay,conn->modes[c].vrefresh);fflush(stdout);
				drmModeEncoder *enc;
				vid.conn[0] = conn->connector_id;
				vid.conn[1] = conn->connector_id;
				enc = drmModeGetEncoder(vid.fdfb, conn->encoder_id);
				if (enc){
					if (enc->crtc_id) {
						memcpy(&vid.mode[0], &conn->modes[c], sizeof(drmModeModeInfo));
						memcpy(&vid.mode[1], &conn->modes[c], sizeof(drmModeModeInfo));
						vid.crtc[0] = enc->crtc_id;
						vid.crtc[1] = enc->crtc_id;
						LOG_info("Found CRTC %u on Connector %u\n",enc->crtc_id,conn->connector_id);						
					}
					else {
						LOG_info("No CRTC found on Connector %u with error %s\n",conn->connector_id, strerror(errno));
					}
				}				
				drmModeFreeEncoder(enc);				
			}
		}
		drmModeFreeConnector(conn);
	}	
	
	for (int thispage = 0; thispage < 2; thispage++) {
		struct drm_mode_create_dumb creq;
		struct drm_mode_map_dumb mreq;
		int ret;

		 /* create dumb buffer */
		memset(&creq, 0, sizeof(creq));
		creq.width = w;
		creq.height = h;
		creq.bpp = 32;
		ret = drmIoctl(vid.fdfb, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
		if (ret < 0) {
			fprintf(stdout, "cannot create dumb buffer $d (%d): %m\n", thispage, errno);fflush(stdout);
		}	
		struct modeset_buf buf;
		vid.screen_size[thispage] = creq.size;
		vid.fb[thispage] = 0;
		vid.handle[thispage] = creq.handle;
		LOG_info("Screen[%d] %dx%d bpp=%d pitch=%d, size=%u\n", thispage, creq.width, creq.height, creq.bpp, creq.pitch, creq.size);fflush(stdout);
		ret = drmModeAddFB(vid.fdfb, creq.width, creq.height, 24, creq.bpp, creq.pitch, creq.handle, &vid.fb[thispage]);
		if (ret) {
			fprintf(stdout, "cannot create framebuffer %d (%d): %m\n", thispage, errno);fflush(stdout);
		}
			/* prepare buffer for memory mapping */
		memset(&mreq, 0, sizeof(mreq));
		mreq.handle = creq.handle;
		ret = drmIoctl(vid.fdfb, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
		if (ret < 0) {
			fprintf(stdout, "cannot map dumb buffer %d (%d): %m\n", thispage, errno);fflush(stdout);
		}
		vid.fbmmap[thispage] = mmap(0, vid.screen_size[thispage], PROT_READ | PROT_WRITE, MAP_SHARED,	vid.fdfb, mreq.offset);
		if (vid.fbmmap[thispage] == MAP_FAILED) {
			fprintf(stdout, "cannot mmap dumb buffer %d (%d): %m\n", thispage,	errno);fflush(stdout);
		}
		memset(vid.fbmmap[thispage], 0, vid.screen_size[thispage]);
		ret = drmModeSetCrtc(vid.fdfb, vid.crtc[thispage], vid.fb[thispage], 0, 0, &vid.conn[thispage], 1, &vid.mode[thispage]);
		if (ret)
			fprintf(stderr, "BUF%d cannot set CRTC for connector %u (%d): %m\n",thispage, vid.conn[thispage], errno);
		vid.linewidth[thispage] = creq.pitch / (creq.bpp/8);
	}

	vid.page = 0;
	vid.renderingGame = 0;
	vid.screen =  SDL_CreateRGBSurface(0, DEVICE_WIDTH, DEVICE_HEIGHT, FIXED_DEPTH, RGBA_MASK_565);
	vid.screen2 = SDL_CreateRGBSurface(0, DEVICE_WIDTH, DEVICE_HEIGHT, FIXED_DEPTH, RGBA_MASK_565);	
	vid.sharpness = SHARPNESS_SOFT;
	return vid.screen;
}

void PLAT_quitVideo(void) {
	close(vid.fdfb);
	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.screen2);
	munmap(vid.fbmmap[0],vid.screen_size[0]);
	munmap(vid.fbmmap[1],vid.screen_size[1]);

	drmModeRmFB(vid.fdfb, vid.fb[0]);
	memset(&dreq, 0, sizeof(dreq));
	dreq.handle = vid.handle[0];
	drmIoctl(vid.fdfb, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);

	drmModeRmFB(vid.fdfb, vid.fb[1]);
	memset(&dreq, 0, sizeof(dreq));
	dreq.handle = vid.handle[1];
	drmIoctl(vid.fdfb, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
}

void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_FillRect(vid.screen, NULL, 0); // TODO: revisit
	SDL_FillRect(vid.screen2, NULL, 0);
}

void  PLAT_clearAll (void) {
	SDL_FillRect(vid.screen, NULL, 0); // TODO: revisit
	SDL_FillRect(vid.screen2, NULL, 0);
	memset(vid.fbmmap[0], 0, vid.screen_size[0]);
	memset(vid.fbmmap[1], 0, vid.screen_size[1]);
//	pwrite(vid.fdfb, vid.fbmmap[0], vid.screen_size/vid.numpages,0);
//	pwrite(vid.fdfb, vid.fbmmap[1], vid.screen_size/vid.numpages,vid.offset);
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
//	int a,b,c,d,e,f,g,h;
//	a = SDL_GetTicks();
//	SDL_Surface *tmp32_1 = SDL_ConvertSurfaceFormat(renderer->src_surface, SDL_PIXELFORMAT_ARGB8888, 0);
//	b = SDL_GetTicks();
//	SDL_Surface *tmp32_2 = SDL_CreateRGBSurfaceWithFormat(0, DEVICE_WIDTH, DEVICE_HEIGHT, 32, SDL_PIXELFORMAT_ARGB8888);
//	c = SDL_GetTicks();
//	SDL_SoftStretch(tmp32_1, NULL, tmp32_2, &(SDL_Rect){renderer->dst_x,renderer->dst_y,renderer->dst_w,renderer->dst_h});
//	d = SDL_GetTicks();
//	SDL_SoftStretchLinear(tmp32_1, NULL, tmp32_2, &(SDL_Rect){renderer->dst_x,renderer->dst_y,renderer->dst_w,renderer->dst_h});
//	e = SDL_GetTicks();
//	SDL_FreeSurface(tmp32_1);
//	SDL_FreeSurface(tmp32_2);
//	scale1x_line(renderer->src_surface->pixels, vid.screen2->pixels, renderer->dst_w, renderer->dst_h, renderer->src_surface->pitch, renderer->dst_w, renderer->dst_h, renderer->src_surface->pitch);	
	if ( renderer->screenscaling != SCALE_FULLSCREEN){
		//not fullscreen
		if ( DEVICE_WIDTH == _HDMI_WIDTH ) {
			//ok the real resolution is set to 720x480, I need to stretch and compress the image 
			vid.width = (renderer->dst_w * 64) / 72;						
		}
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
	//	f = SDL_GetTicks();
	//	SDL_SoftStretch(renderer->src_surface, NULL, vid.screen, &(SDL_Rect){renderer->dst_x,renderer->dst_y,renderer->dst_w,renderer->dst_h});
	//	g = SDL_GetTicks();
	//	SDL_SoftStretchLinear(tmp32_1, NULL, vid.screen32, &(SDL_Rect){renderer->dst_x,renderer->dst_y,renderer->dst_w,renderer->dst_h});
	}
	//LOG_info("convert 16to32 =%ims, softs=%ims, softslinear =%ims,  softsreal=%ims\n", b-a, d-c, e-d, g-f);fflush(stdout);
	vid.targetRect.x = renderer->dst_x;
	vid.targetRect.y = renderer->dst_y;
	vid.targetRect.w = renderer->dst_w;
	vid.targetRect.h = renderer->dst_h;
	vid.renderingGame = 1;
}
int flipflag;
static void modeset_page_flip_event(int fd, unsigned int frame,
	unsigned int sec, unsigned int usec,
	void *data)
{
	flipflag = 0;
}

void PLAT_flip(SDL_Surface* IGNORED, int sync) { //this rotates minarch menu + minui + tools
//	uint32_t now = SDL_GetTicks();

	drmEventContext ev;

	/* init variables */
	memset(&ev, 0, sizeof(ev));
	ev.version = DRM_EVENT_CONTEXT_VERSION;
	ev.page_flip_handler = modeset_page_flip_event;

	if (!vid.renderingGame) {
		vid.targetRect.x = 0;
		vid.targetRect.y = 0;
		vid.targetRect.w = vid.screen->w;
		vid.targetRect.h = vid.screen->h;
		//vid.page = 0;
	}
	if (vid.rotate == 0) 
	{
		//No Rotation
		R36_SDLFB_Flip(vid.screen, vid.fbmmap[vid.page],vid.linewidth[vid.page], vid.targetRect);		
	}
	if (vid.rotate == 1)
	{
		// 90 Rotation
		R36_SDLFB_FlipRotate90(vid.screen, vid.fbmmap[vid.page],vid.linewidth[vid.page], vid.targetRect);
	}
	if (vid.rotate == 2)
	{
		// 180 Rotation
		R36_SDLFB_FlipRotate180(vid.screen, vid.fbmmap[vid.page],vid.linewidth[vid.page], vid.targetRect);
	}
	if (vid.rotate == 3)
	{
		// 270 Rotation
		R36_SDLFB_FlipRotate270(vid.screen, vid.fbmmap[vid.page],vid.linewidth[vid.page], vid.targetRect);
	}
//	int now2 = SDL_GetTicks();
	
	
	flipflag=1;	
	int ret = drmModePageFlip(vid.fdfb	, vid.crtc[vid.page], vid.fb[vid.page], DRM_MODE_PAGE_FLIP_EVENT, NULL);
	while(flipflag){
		drmHandleEvent(vid.fdfb, &ev);
	}
	vid.page = 1 - vid.page;
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
int online;

void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	int charge_fd = open("/sys/class/power_supply/battery/current_now", O_RDONLY);
	if (charge_fd < 0) {
		*is_charging = 0;
	} else {
		char buf[8];
		read(charge_fd, buf, 8);
		*is_charging = (atoi(buf)>0)?1:0;
	}
	close(charge_fd);
	int i;
	charge_fd = open("/sys/class/power_supply/battery/voltage_now", O_RDONLY);
	if (charge_fd < 0) {
		i = 3380000;
	} else {
		char buf[10];//
		read(charge_fd, buf, 10);
		i = atoi(buf);
	}
	close(charge_fd);
	i = MIN(i,4100000);
	*charge = map(i,3380000,4100000,0,100);
	LOG_info("Charging: %d, Raw battery: %i -> %d%%\n", *is_charging, i, *charge);

	char status[16];
	getFile("/sys/class/net/wlan0/operstate", status,16);
	online = prefixMatch("up", status);
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
	return online;
}

int PLAT_getNumProcessors(void) {
	//the core can be deactivated by command line
	return sysconf(_SC_NPROCESSORS_ONLN);
}

uint32_t PLAT_screenMemSize(void) {
	return vid.screen_size[0];
}

void PLAT_getAudioOutput(void){
//buh!
}