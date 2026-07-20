// trimuismart
#define _GNU_SOURCE  // 🎯 Crucial: Must be at the very top of your file to expose getprogname()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#include "sunxi_display2.h"
#include <time.h>
#include <sys/time.h>

#define ION_IOC_SUNXI_FREE           0xC0044901 
#define ION_IOC_SUNXI_ALLOC          0xC0144900 
#define ION_IOC_SUNXI_SHARE          0xC0084904 
extern char *__progname;

struct ion_allocation_data_v1 {
    size_t len;
    size_t align;
    unsigned int heap_id_mask;
    unsigned int flags;
    int handle;
};

struct ion_fd_data_v1 { 
    int handle; 
    int fd; 
};

struct disp_layer_config2_gdb {
    uint32_t dwords[55]; // 55 DWORD = 220 bytes (Dump GDB fbtest3)
};




#define RAW_A		304   //event1
#define RAW_B		305  //event1
#define RAW_X		307  //event1
#define RAW_Y		306  //event1
#define RAW_START	 311  //event1  
#define RAW_SELECT	 310  //event1
#define RAW_MENU	 312  //event1  //also 354
#define RAW_L1		 308  //event1
#define RAW_L2		 314  //event1
#define RAW_R1		 309  //event1
#define RAW_R2		 315  //event1
#define RAW_L3		 313  //event1  
#define RAW_R3		 316  //event1  
#define RAW_PLUS	 115  //event1
#define RAW_MINUS	 114  //event1
#define RAW_POWER	116  //event0
#define RAW_LSY		3 //event1
#define RAW_LSX		2 //event1
#define RAW_RSY		5   //event1
#define RAW_RSX		4  //event1

//lid 110? 111? su event0?

// from <linux/input.h> which has BTN_ constants that conflict with platform.h
struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
};
#define EV_KEY			0x01
#define EV_ABS			0x03
#define EV_FF			0x15

struct ff_replay {
	__u16 length;
	__u16 delay;
};

struct ff_rumble_effect {
	__u16 strong_magnitude;
	__u16 weak_magnitude;
};

struct ff_envelope {
	__u16 attack_length;
	__u16 attack_level;
	__u16 fade_length;
	__u16 fade_level;
};

struct ff_constant_effect {
	__s16 level;
	struct ff_envelope envelope;
};

struct ff_periodic_effect {
	__u16 waveform;
	__u16 period;
	__s16 magnitude;
	__s16 offset;
	__u16 phase;

	struct ff_envelope envelope;

	__u32 custom_len;
	__s16 *custom_data;
};
struct ff_trigger {
	__u16 button;
	__u16 interval;
};

struct ff_ramp_effect {
	__s16 start_level;
	__s16 end_level;
	struct ff_envelope envelope;
};

struct ff_condition_effect {
	__u16 right_saturation;
	__u16 left_saturation;

	__s16 right_coeff;
	__s16 left_coeff;

	__u16 deadband;
	__s16 center;
};

struct ff_effect {
	__u16 type;
	__s16 id;
	__u16 direction;
	struct ff_trigger trigger;
	struct ff_replay replay;

	union {
		struct ff_constant_effect constant;
		struct ff_ramp_effect ramp;
		struct ff_periodic_effect periodic;
		struct ff_condition_effect condition[2]; /* One for each axis */
		struct ff_rumble_effect rumble;
	} u;
};

#define FF_RUMBLE	0x50
#define EVIOCGBIT(ev,len)	_IOC(_IOC_READ, 'E', 0x20 + (ev), len)	/* get event bits */
#define EVIOCSFF	_IOC(_IOC_WRITE, 'E', 0x80, sizeof(struct ff_effect))	/* send a force effect to a force feedback device */

#define NOMENU_PATH SYSTEM_PATH "/menumissing.txt"
int menumissing = 0;

int is_cubexx = 0;
int is_rg34xx = 0;
int on_hdmi = 0;

static uint32_t PWR_Tick = 0;
#define PWR_TIMEOUT 2000
///////////////////////////////

#define INPUT_COUNT 2
static int inputs[INPUT_COUNT];

long map(int x, int in_min, int in_max, int out_min, int out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#define FF_MAX		0x7f
#define FF_CNT		(FF_MAX+1)

int testBit(int bit, const uint8_t arr[]) {
    return arr[bit / 8] & (1 << (bit % 8));
}
int rumblefd = -1;
int check_rumble(char *rumbledevice){
	char eventrumble[30];
	if (exists(rumbledevice)) {
		LOG_info("Checking %s for rumble support\n", rumbledevice);fflush(stdout);
		int fd = open(rumbledevice, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd >= 0) {
			uint8_t mFfBitmask[FF_CNT / 8];
			if (ioctl(fd, EVIOCGBIT(EV_FF,  sizeof(mFfBitmask)),  mFfBitmask) >= 0) {
			//	if (features > 0) {
				int x = testBit(FF_RUMBLE, mFfBitmask);
					if (x>0) {
						LOG_info("Rumble checked supported on %s\n", rumbledevice);fflush(stdout);
						rumblefd = open(rumbledevice, O_RDWR | O_NONBLOCK | O_CLOEXEC);
					}
			}
			close(fd);
		}
	}
}


void PLAT_initInput(void) {
	LOG_info("PLAT_initInput start\n");fflush(stdout);
	if (exists(NOMENU_PATH)) {
		menumissing = 1;
	}

	check_rumble("/dev/input/event1");
	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // power
	inputs[1] = open("/dev/input/event1", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // controller + volume
	LOG_info("Init Input:Detected RG35XXSP\n");fflush(stdout);
}

void PLAT_quitInput(void) {
	//Stick_quit();
	for (int i=0; i<INPUT_COUNT; i++) {
		close(inputs[i]);
	}
	close(rumblefd);
}

#define LID_PATH "/sys/class/power_supply/axp2202-battery/hallkey"
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


static int prev_button_pressed_vert, prev_button_pressed_horiz;

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
		while (read(inputs[i], &event, sizeof(event))==sizeof(event)) {
			if (event.type!=EV_KEY && event.type!=EV_ABS) continue;

			int btn = BTN_NONE;
			int pressed = 0; // 0=up,1=down
			int id = -1;
			int type = event.type;
			int code = event.code;
			int value = event.value;
		//	LOG_info("/dev/input/event%d: Type %d event: SCANCODE/AXIS=%i, PRESSED/AXIS_VALUE=%i\n", i,type, code, value);system("sync");
			// TODO: tmp, hardcoded, missing some buttons
			if (type==EV_KEY) {
				if (value>1) continue; // ignore repeats
			
				pressed = value;
				if 		(code==RAW_START)	{ btn = BTN_START; 		id = BTN_ID_START; } 
			    else if (code==RAW_SELECT)	{ btn = BTN_SELECT; 	id = BTN_ID_SELECT; }
				else if (code==RAW_A)		{ btn = BTN_A; 			id = BTN_ID_A; }
				else if (code==RAW_B)		{ btn = BTN_B; 			id = BTN_ID_B; }
				else if (code==RAW_X)		{ btn = BTN_X; 			id = BTN_ID_X; }
				else if (code==RAW_Y)		{ btn = BTN_Y; 			id = BTN_ID_Y; }
				 
				else if (code==RAW_L1)		{ btn = BTN_L1; 		id = BTN_ID_L1; }
				else if (code==RAW_L2)		{ btn = BTN_L2; 		id = BTN_ID_L2; }				
				else if (code==RAW_R1)		{ btn = BTN_R1; 		id = BTN_ID_R1; }
				else if (code==RAW_R2)		{ btn = BTN_R2; 		id = BTN_ID_R2; }
				else if (code==RAW_L3)		{ btn = BTN_L3; 		id = BTN_ID_L3; }
				else if (code==RAW_R3)		{ btn = BTN_R3; 		id = BTN_ID_R3; }

				else if (code==RAW_MENU)	{ btn = BTN_MENU; 		id = BTN_ID_MENU; } 				
				else if (code==RAW_PLUS)	{ btn = BTN_PLUS; 		id = BTN_ID_PLUS; }
				else if (code==RAW_MINUS)	{ btn = BTN_MINUS; 		id = BTN_ID_MINUS; }
				else if (code==RAW_POWER)	{ btn = BTN_POWER; 		id = BTN_ID_POWER; }
			}
			if (type==EV_ABS) {  // (range -1800 0 +1800)
				if (code==16) {  //same as rg351p dpad left = -1, right = 1
					if (value == 1) { pressed = 1 ; btn = BTN_DPAD_RIGHT; id = BTN_ID_DPAD_RIGHT; prev_button_pressed_horiz = BTN_DPAD_RIGHT;}
					if (value == -1) { pressed = 1 ; btn = BTN_DPAD_LEFT; id = BTN_ID_DPAD_LEFT; prev_button_pressed_horiz = BTN_DPAD_LEFT;}
					if (value == 0) { pressed = 0; btn = prev_button_pressed_horiz; }						
				}
				else if (code==17) {  // same as rg351p dpad up = -1, down = 1
					if (value == 1) { pressed = 1; btn = BTN_DPAD_DOWN; id = BTN_ID_DPAD_DOWN; prev_button_pressed_vert = BTN_DPAD_DOWN; }
					if (value == -1) { pressed = 1; btn = BTN_DPAD_UP; id = BTN_ID_DPAD_UP; prev_button_pressed_vert = BTN_DPAD_UP; }
					if (value == 0) { pressed = 0; btn = prev_button_pressed_vert; }
				}
    			else if (code==RAW_LSX) { pad.laxis.x =  map(value ,0,4096,0x7fff,-0x7fff);
					if (pad.map_leftstick_to_dpad)	PAD_setAnalog(BTN_ID_DPAD_LEFT, BTN_ID_DPAD_RIGHT, pad.laxis.x, tick+PAD_REPEAT_DELAY); } 
				else if (code==RAW_LSY) { pad.laxis.y =  map(value ,0,4096,0x7fff,-0x7fff);
					if (pad.map_leftstick_to_dpad)	PAD_setAnalog(BTN_ID_DPAD_UP,   BTN_ID_DPAD_DOWN,  pad.laxis.y, tick+PAD_REPEAT_DELAY); }
				else if (code==RAW_RSX) pad.raxis.x =  map(value ,0,4096,-0x7fff,0x7fff);
				else if (code==RAW_RSY) pad.raxis.y =  map(value ,0,4096,0x7fff,-0x7fff);
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
	if (lid.has_lid && PLAT_lidChanged(NULL) && !lid.is_open) {
		pad.just_released |= BTN_SLEEP;
	}
}
		

int PLAT_shouldWake(void) {
	int input;
	static struct input_event event;
	int lid_open = 1; // assume open by default
	if (lid.has_lid && PLAT_lidChanged(&lid_open) && lid_open) return 1;
	for (int i=0; i<INPUT_COUNT; i++) {
		input = inputs[i];
		while (read(input, &event, sizeof(event))==sizeof(event)) {
			if (event.value>1) continue; // ignore repeats
			if (event.type==EV_KEY && event.code==RAW_POWER && event.value==0) {
				return lid.has_lid ? lid.is_open: 1; // only wake if lid is open
			}
		}
	}
	return 0;
}

///////////////////////////////
#define NUM_PAGES 2

struct VID_Context {
	//int fdfb; // /dev/fb0 handler
	int dispfd; // /dev/disp handler
	int ionfd;

	//	struct fb_fix_screeninfo finfo;  //fixed fb info
//	struct fb_var_screeninfo vinfo;  //adjustable fb info
//	int32_t ion_handles[NUM_PAGES];
	int32_t ion_fds[NUM_PAGES];
	void *fbmmap[NUM_PAGES]; //mmap address of the framebuffer
	SDL_Surface* screen;  //swsurface to let sdl thinking it's the screen
	SDL_Surface* screen2;  //swsurface to let sdl thinking it's the screen
	SDL_Surface* screengame;  //swsurface to let sdl thinking it's the screen
	uint32_t vsync_refresh;
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
	struct disp_layer_config2 config_logica;
	struct disp_layer_config2_gdb * config_w;
	uint32_t offset;
	SDL_Rect targetRect;
	int renderingGame;
} vid;


int init_ion_allocator(void) {
    int i, ret;
    uint32_t check_arg = 0;
    uint64_t spurious_time_stack = 0;

    for (i = 0; i < NUM_PAGES; i++) {
        struct ion_allocation_data_v1 alloc_v1;
        struct ion_fd_data_v1 share_data;

        check_arg = 0;
        //ret = ioctl(vid.ionfd, ION_IOC_SUNXI_FREE, &check_arg);
        //if (ret < 0) print_sunxi_cedar_error(ret, errno);

        memset(&alloc_v1, 0, sizeof(alloc_v1));
        alloc_v1.len = vid.screen_size;
        alloc_v1.align = 4096;
        alloc_v1.heap_id_mask = 1;
        alloc_v1.flags = 3;

        if (ioctl(vid.ionfd, ION_IOC_SUNXI_ALLOC, &alloc_v1) < 0) {
            LOG_info("[ERROR] Failed ION Allocation index %d\n", i);
            return -2;
        }
        //vid.ion_handles[i] = alloc_v1.handle;

        check_arg = 0;
        ret = ioctl(vid.ionfd, ION_IOC_SUNXI_FREE, &check_arg);
        

        memset(&share_data, 0, sizeof(share_data));
        share_data.handle = alloc_v1.handle;
        share_data.fd = -1;
        if (ioctl(vid.ionfd, ION_IOC_SUNXI_SHARE, &share_data) < 0) {
            LOG_info("[ERROR] ION Share failed index %d\n", i);
            return -3;
        }
        vid.ion_fds[i] = share_data.fd;

        spurious_time_stack = (uint64_t)alloc_v1.handle | 0xDEADBEEF00000000ULL;
        int32_t handle_to_free = (int32_t)spurious_time_stack;
        ret = ioctl(vid.ionfd, ION_IOC_SUNXI_FREE, &handle_to_free);
        

        vid.fbmmap[i] = mmap(NULL, vid.screen_size, PROT_READ|PROT_WRITE, MAP_SHARED, vid.ion_fds[i], 0);
        if (vid.fbmmap[i] == MAP_FAILED) {
            LOG_info("[ERROR] ION Mmap failed index %d\n", i);
            return -4;
        }

        LOG_info("Success ion alloc fd= %d, size = %u, addr_vir = %p\n", 
               vid.ion_fds[i], vid.screen_size, vid.fbmmap[i]);
    }
    return 0;
}

void pack_gdb_structure(struct disp_layer_config2_gdb *dst, const struct disp_layer_config2 *src) {
    // 1. Azzeramento preventivo dell'intero blocco hardware da 220 byte
    memset(dst, 0, sizeof(struct disp_layer_config2_gdb));

    dst->dwords[0]  = (uint32_t)src->info.mode;
    
    // Compattiamo zorder (byte 0), alpha_mode (byte 1) e alpha_value (byte 2) nella dword 1
    dst->dwords[1]  = ((uint32_t)src->info.alpha_value << 16) | 
                      ((uint32_t)src->info.alpha_mode  << 8)  | 
                      ((uint32_t)src->info.zorder);

    dst->dwords[2]  = (uint32_t)src->info.screen_win.x;
    dst->dwords[3]  = (uint32_t)src->info.screen_win.y;
    dst->dwords[4]  = src->info.screen_win.width;
    dst->dwords[5]  = src->info.screen_win.height;
    
    // Dword 6 e 7 contengono b_trd_out e out_trd_mode (lasciati a 0 per modalità Mono standard)
    dst->dwords[6]  = 0; 
    dst->dwords[7]  = 0;

    // --- SEZIONE FB (Inizia a dword 8 / Offset 32) ---
    dst->dwords[8]  = (uint32_t)src->info.fb.fd;         // Iniezione dinamica dma_buf fd
    dst->dwords[9]  = src->info.fb.size[0].width;
    dst->dwords[10] = src->info.fb.size[0].height;
    
    // Dword da 11 a 17 contengono allineamenti e parametri degli altri piani della union
    // Vengono lasciate a zero per il formato compresso intero XRGB/RGB565
    dst->dwords[18] = (uint32_t)src->info.fb.format;      // Formato Pixel (10 o 0)
    dst->dwords[19] = (uint32_t)src->info.fb.color_space; // Spazio Colore (260 / 0x104)

    // --- SEZIONE CROP FIXED-POINT (Mappatura dword separate 32.32) ---
    // Estraiamo la parte intera a 32-bit (i bit alti della coordinata a 64-bit del Crop)
    dst->dwords[27] = (uint32_t)(src->info.fb.crop.width  >> 32); // Larghezza intera
    dst->dwords[28] = (uint32_t)(src->info.fb.crop.width  & 0xFFFFFFFFULL); // Frazione (0)
    dst->dwords[29] = (uint32_t)(src->info.fb.crop.height >> 32); // Altezza intera
    dst->dwords[30] = (uint32_t)(src->info.fb.crop.height & 0xFFFFFFFFULL); // Frazione (0)

    // Dword da 31 a 51: lasciate a zero (metadati estesi per ATW e SNR non usati)

    // --- CONTROLLI DI CODA FINALI (Dwords 52, 53, 54) ---
    dst->dwords[52] = (uint32_t)src->enable;   // Contiene il flag booleano nel byte basso
    dst->dwords[53] = src->channel;            // Canale hardware attivo (1)
    dst->dwords[54] = src->layer_id;           // Indice del livello (0)
}

void clean_layer(void){
	uint32_t args[4];
    struct disp_layer_config2_gdb clear_config;
	memset(&clear_config, 0, sizeof(clear_config));
    
    clear_config.dwords[52] = 0x00000000; // enable = 0 (Disattiva!)
    clear_config.dwords[53] = 0x00000001; // channel = 1
    clear_config.dwords[54] = 0x00000000; // layer_id = 0

    // Prepariamo l'argomento atomico per la ioctl 73
    args[0] = 0;                          // Screen 0
    args[1] = (uint32_t)&clear_config;    // Puntatore alla struttura di clear da 220 byte
    args[2] = 1;                          // Conteggio strutture (1)
    args[3] = 0;

    //printf("[HDMI] Invio configurazione di disattivazione layer (ioctl 0x49) per pulizia registri...\n");
    if (ioctl(vid.dispfd, DISP_LAYER_SET_CONFIG2, &args) < 0) {
        LOG_info("Unable to clean the layer: %s\n", strerror(errno));
    }

}

void swap_buffers_init(void){
	memset(&vid.config_logica, 0, sizeof(vid.config_logica));

    vid.config_logica.info.mode = LAYER_MODE_BUFFER;
    vid.config_logica.info.zorder = 20;
    vid.config_logica.info.alpha_mode = 1; 
    vid.config_logica.info.alpha_value = 255;

 	vid.config_logica.info.screen_win.x = 0;
    vid.config_logica.info.screen_win.y = 0;
    vid.config_logica.info.screen_win.width = vid.width;
    vid.config_logica.info.screen_win.height = vid.height;

 	//vid.config_logica.info.fb.fd = active_fd; // 🎯 Assegnazione logica dell'FD ION
    vid.config_logica.info.fb.size[0].width = vid.width;
    vid.config_logica.info.fb.size[0].height = vid.height;
    vid.config_logica.info.fb.format = DISP_FORMAT_XRGB_8888; // RGB565 (o 10 per XRGB8888)
    vid.config_logica.info.fb.color_space = DISP_BT601;

 // Geometria Crop usando lo shift a 32-bit richiesto dalle specifiche Sunxi
    vid.config_logica.info.fb.crop.width  = (uint64_t)vid.width << 32;
    vid.config_logica.info.fb.crop.height = (uint64_t)vid.height << 32;

 	vid.config_logica.enable = 1;
    vid.config_logica.channel = 1;
    vid.config_logica.layer_id = 0;
   // LOG_info ("struttura layer_config2 dimensione: %zu byte\n", sizeof(vid.config_logica));

 // 2. 🎯 TRADUZIONE AUTOMATICA NELL'ARRAY GREZZO DA 220 BYTE
	vid.config_w = malloc(sizeof(struct disp_layer_config2_gdb));
    pack_gdb_structure(vid.config_w, &vid.config_logica);
}

int swap_buffers(int page){
	vid.config_w->dwords[8]=vid.ion_fds[page];
	uint32_t args[4];
    args[0] = 0; args[1] = 1; args[2] = 0; args[3] = 0;
    if (ioctl(vid.dispfd, DISP_SHADOW_PROTECT, &args) < 0) return -1;

    args[0] = 0; args[1] = (uint32_t)vid.config_w; args[2] = 1; args[3] = 0;
    if (ioctl(vid.dispfd, DISP_LAYER_SET_CONFIG2, &args) < 0) {
        args[0] = 0; args[1] = 0; 
		ioctl(vid.dispfd, DISP_SHADOW_PROTECT, &args); // Emergenza
        return -2;
    }

    args[0] = 0; args[1] = 4; args[2] = 1; args[3] = 0;
    ioctl(vid.dispfd, DISP_HWC_COMMIT, &args);

    args[0] = 0; args[1] = 0; args[2] = 0; args[3] = 0;
    ioctl(vid.dispfd, DISP_SHADOW_PROTECT, &args);
}

// Struttura globale protetta con allineamento forzato a 8 byte per prevenire corruzioni
struct vsync_context_safe {
    uint64_t next_sec;         // Base tempi assoluta (Secondi)
    uint64_t next_nsec;        // Base tempi assoluta (Nanosecondi)
    uint32_t interval_ns;      // Intervallo fisso
    uint32_t pll_start_irq;
    uint32_t current_irq;
    uint32_t call_counter;     // Blindato contro sovrascritture indotte
} __attribute__((aligned(8)));

static struct vsync_context_safe vsync_ctx;

// Buffer statico isolato per la lettura del testo: azzera i rischi di stack overflow
static char sysfs_parsing_buffer[512];

uint32_t get_hardware_irq_atomic(void) {
    FILE *f = fopen("/sys/class/disp/disp/attr/sys", "r");
    if (!f) return 0;

    uint32_t irq_val = 0;
    memset(sysfs_parsing_buffer, 0, sizeof(sysfs_parsing_buffer));

    // Leggiamo in sicurezza usando il buffer statico isolato globalmente
    while (fgets(sysfs_parsing_buffer, sizeof(sysfs_parsing_buffer) - 1, f)) {
        char *match = strstr(sysfs_parsing_buffer, "irq:");
        if (match) {
            if (sscanf(match, "irq:%u", &irq_val) == 1) {
                break;
            }
        }
    }
    fclose(f);
    return irq_val;
}

#define NUM_SAMPLES 20
uint32_t measureAverageVsyncNs(void) {
    struct timespec t_start, t_end;
    uint32_t last_irq = get_hardware_irq_atomic();
    uint32_t current_irq_val = 0;
    int samples = 0;
    uint64_t elapsed_ns = 0;
	int num_samples = 1;
	if (__progname != NULL && strcmp(__progname, "minarch.elf") == 0) {
        num_samples=NUM_SAMPLES;
		LOG_info("minarch.elf detected!!!\n");
    }
    //LOG_info("[VSYNC] Avvio calibrazione. Campionamento di 20 cicli IRQ...\n");

    // Sincronizziamo il punto di partenza sullo scatto del prossimo interrupt
    while (get_hardware_irq_atomic() == last_irq) { 
        #if defined(__arm__) || defined(__aarch64__)
        asm volatile("yield" ::: "memory"); 
        #else
        asm volatile("nop");
        #endif
    }
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    last_irq = get_hardware_irq_atomic();

    // Raccogliamo i 20 campioni reali emessi dalla Display Engine
    while (samples < num_samples) {
        current_irq_val = get_hardware_irq_atomic();
        if (current_irq_val != last_irq && current_irq_val > 0) {
            samples++;
            last_irq = current_irq_val;
        }
        #if defined(__arm__) || defined(__aarch64__)
        asm volatile("yield" ::: "memory");
        #else
        asm volatile("nop");
        #endif
    }
    clock_gettime(CLOCK_MONOTONIC, &t_end);

    // Elaborazione matematica dell'intervallo medio
    elapsed_ns = (t_end.tv_sec - t_start.tv_sec) * 1000000000ULL + (t_end.tv_nsec - t_start.tv_nsec);
    vsync_ctx.interval_ns = (uint32_t)(elapsed_ns / num_samples);
    
    // Inizializzazione dei parametri di controllo del contesto statico
    vsync_ctx.call_counter = 0;
    
    // Ancoriamo il punto zero iniziale per far partire le scadenze assolute
   // while (get_hardware_irq_atomic() == last_irq) { asm volatile("yield"); }
   // clock_gettime(CLOCK_MONOTONIC, &vsync_ctx.next_deadline);
   // vsync_ctx.pll_start_irq = get_hardware_irq_atomic();
   // vsync_ctx.current_irq = vsync_ctx.pll_start_irq;

    LOG_info("[VSYNC] Calibration Vsync complete. Interval: %u ns (~%.2f Hz)\n", 
           vsync_ctx.interval_ns, 1000000000.0 / vsync_ctx.interval_ns);

    return vsync_ctx.interval_ns;
}

/**
 * Gestisce la temporizzazione real-time del frame. 
 * Esegue il clock_nanosleep assoluto e, ogni 60 chiamate, arresta l'esecuzione
 * per riallinearsi alla fase dell'IRQ hardware reale eliminando il drift.
 * Prima di uscire, incrementa e aggiorna il valore di vsync_ctx.current_irq.
 */

void waitVsync(void) {
    struct timespec t_now, t_sleep;
    static struct timespec t_last = {0, 0};
    static struct timespec next_target_time = {0, 0};
    static uint32_t counter = 0;
    uint32_t check_irq;
	if (counter==0){
		clock_gettime(CLOCK_MONOTONIC, &t_last);
	}
    // 1. HARD VSYNC: ATTIVO AL FRAME 0 E OGNI 60 FRAME
    if (counter % 60 == 0) {
        check_irq = get_hardware_irq_atomic();
        while (get_hardware_irq_atomic() == check_irq) {
            asm volatile("yield" ::: "memory"); 
        }
        clock_gettime(CLOCK_MONOTONIC, &t_now);
        next_target_time = t_now;
    } 
    // 2. SOFT VSYNC: PROIEZIONE AVANZAMENTO LINEARE COSTANTE 
    else {
        next_target_time.tv_nsec += vsync_ctx.interval_ns;
        if (next_target_time.tv_nsec >= 1000000000) {
            next_target_time.tv_sec += 1;
            next_target_time.tv_nsec -= 1000000000;
        } else if (next_target_time.tv_nsec < 0) {
            next_target_time.tv_sec -= 1;
            next_target_time.tv_nsec += 1000000000;
        }
    }

    // Iniezione e riallineamento millimetrico sul frame 1 per assorbire l'overhead iniziale
    if (counter == 1) {
        next_target_time = t_last;
        next_target_time.tv_nsec += vsync_ctx.interval_ns;
        if (next_target_time.tv_nsec >= 1000000000) {
            next_target_time.tv_sec += 1;
            next_target_time.tv_nsec -= 1000000000;
        }
    }

    // 3. FRENO INTERMEDIO IBRIDO: RIDUCE L'USO DELLA CPU AL MINIMO RILASCIANDO IL CORE
    if (counter > 0) {
        clock_gettime(CLOCK_MONOTONIC, &t_now);
        int64_t ns_rimanenti = (next_target_time.tv_sec - t_now.tv_sec) * 1000000000LL + 
                               (next_target_time.tv_nsec - t_now.tv_nsec);

        // Se mancano più di 1.5ms, addormentiamo il thread per un intervallo di sicurezza
        if (ns_rimanenti > 1500000LL) {
            t_sleep = next_target_time;
            t_sleep.tv_nsec -= 1000000L; // Svegliati con 1ms d'anticipo per finire a spin-lock
            if (t_sleep.tv_nsec < 0) {
                t_sleep.tv_sec -= 1;
                t_sleep.tv_nsec += 1000000000L;
            }
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t_sleep, NULL);
        }

        // SPIN-LOCK DI FINITURA CHIRURGICA: Spacca il microsecondo finale
        while (1) {
            clock_gettime(CLOCK_MONOTONIC, &t_now);
            if (t_now.tv_sec > next_target_time.tv_sec || 
               (t_now.tv_sec == next_target_time.tv_sec && t_now.tv_nsec >= next_target_time.tv_nsec)) {
                break; 
            }
            #if defined(__arm__) || defined(__aarch64__)
            asm volatile("yield" ::: "memory");
            #else
            asm volatile("nop");
            #endif
        }
    } else {
        clock_gettime(CLOCK_MONOTONIC, &t_now);
    }

    // 4. DIAGNOSTICA COMPLETA CON VISUALIZZAZIONE CORRETTA DEL FRAME 0
//    if (t_last.tv_sec > 0) {
//        uint64_t diff_ns = (t_now.tv_sec - t_last.tv_sec) * 1000000000ULL + (t_now.tv_nsec - t_last.tv_nsec);
//        printf("[INFO] %s Vsync | Elapsed = %5lluus | Target = %5dus | counter = %u\n", 
//               (counter % 60 == 0) ? "Hard" : "Soft", 
//               diff_ns / 1000, 
//               vsync_ctx.interval_ns / 1000, 
//               counter);
//    } else {
//        // La tua riga protetta per il counter 0
//        printf("[INFO] Hard Vsync | Elapsed =     0us | Target = %5dus | counter = 0\n",
//               vsync_ctx.interval_ns / 1000);
//    }

    t_last = t_now;
    counter++;
}



void trigger_audio_server_route(int hdmi_on) {
    FILE *pid_file = popen("pidof audioserver.elf", "r");
    if (pid_file) {
        pid_t server_pid = 0;
        if (fscanf(pid_file, "%d", &server_pid) == 1 && server_pid > 0) {
            if (hdmi_on == 1) { // HDMI output selected
                LOG_info("Signaling Audio Server to route over HDMI (SIGUSR1)...\n");
                kill(server_pid, SIGUSR1);
            } else {
                LOG_info("Signaling Audio Server to route over Speaker (SIGUSR2)...\n");
                kill(server_pid, SIGUSR2);
            }
        }
        pclose(pid_file);
    }
}


int switch_to_hdmi_output(int hdmi_on) {
    // Mappatura esatta dell'array a 32-bit vista su Ghidra
    uint32_t args[4];

    //LOG_info("[HDMI] Inizializzazione switch hardware del device su HDMI...\n");
	LOG_info("Switch Video output to %s\n", hdmi_on ==1 ? "HDMI": "LCD");
    args[0] = 0;                        // args: Screen ID (0 = Display Principale)
    args[1] = (hdmi_on==1) ? DISP_OUTPUT_TYPE_HDMI:DISP_OUTPUT_TYPE_LCD; // args: Tipo di Output desiderato (4 = HDMI)
    args[2] = (hdmi_on==1) ? DISP_TV_MOD_720P_60HZ : 0;   // args: Modalità video/Risoluzione (Es. 14 = 1080p 60Hz)
    args[3] = 0;                        // args: Safe padding / Inutilizzato

    // Questa ioctl spegne temporaneamente la pipeline dell'LCD, 
    // rialloca il modulo clock 'de_rate' e reindirizza il flusso sul chip HDMI
    if (ioctl(vid.dispfd, DISP_DEVICE_SWITCH, &args) < 0) {
        LOG_info("[ERROR] Switch HDMI failed: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}





#define HDMI_STATE_PATH "/sys/class/switch/hdmi/cable.0/state"

/*
Available frequencies userspace
480000
720000
936000
1008000
1104000
1200000
1320000
1416000
1512000
*/

int cpufreq_menu,cpufreq_game,cpufreq_perf,cpufreq_powersave,cpufreq_max,cpufreq_sleep;

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
		cpufreq_sleep = atoi(getenv("CPU_SPEED_SLEEP"));
		LOG_info("CPU_SPEED_SLEEP = %d\n", cpufreq_sleep);
	} 
	char* model = "Model Unknown";
	if (getenv("RGXX_MODEL") != NULL) {
		model = getenv("RGXX_MODEL");
	}
	
	is_cubexx = exactMatch("RGcubexx", model);
	is_rg34xx = prefixMatch("RG34xx", model);


	vid.ionfd = open("/dev/ion", O_RDWR);
    if (vid.ionfd < 0) {
        LOG_info("[ERROR] Unable to open /dev/ion: %s\n", strerror(errno));
        return NULL;
    }
	vid.dispfd = open("/dev/disp", O_RDWR);
    if (vid.dispfd < 0) {
        LOG_info("[ERROR] Unable to open /dev/disp: %s\n", strerror(errno));
    }
	on_hdmi=0;
	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;	
	int p = FIXED_PITCH;
	if (getInt(HDMI_STATE_PATH)) { // can't use getHDMI() from settings because it hasn't be initialized yet
		w = _HDMI_WIDTH;
		h = _HDMI_HEIGHT;
		p = _HDMI_PITCH;
		on_hdmi = 1;
		//switch to HDMI output
	} 
	trigger_audio_server_route(on_hdmi);
	switch_to_hdmi_output(on_hdmi); 
	clean_layer();
	vid.vsync_refresh = 16666700; //virtual 60Hz
	if (is_minarch!=0){
		vid.vsync_refresh=measureAverageVsyncNs(); //calibration of virtual vsync after hdmi/lcd out selected
	}
	DEVICE_WIDTH = w;
	DEVICE_HEIGHT = h;
	DEVICE_PITCH = p;

	//get_fbinfo();	

	if (exists(ROTATE_SYSTEM_PATH)) {
		vid.rotate = getInt(ROTATE_SYSTEM_PATH) &3;
	} else {
		//if the file does not exist, we create it with the default value.
		putInt(ROTATE_SYSTEM_PATH, vid.rotate);
	}
	
	DEVICE_WIDTH = w;
	DEVICE_HEIGHT = h;
	DEVICE_PITCH = p;
	vid.rotate = 0;

	if (exists(ROTATE_SYSTEM_PATH)) {
		vid.rotate = getInt(ROTATE_SYSTEM_PATH) &3;
	}
	
	GAME_WIDTH = DEVICE_WIDTH;
	GAME_HEIGHT = DEVICE_HEIGHT;

	if (vid.rotate % 2 == 1) {
		DEVICE_WIDTH = h;
		DEVICE_HEIGHT = w;
	}
	vid.width = DEVICE_WIDTH;
	vid.height = DEVICE_HEIGHT;
	vid.screen_size = vid.width * vid.height * 4;
	FIXED_SCALE = _FIXED_SCALE;
	InitAssetRects();


	vid.screen =  SDL_CreateRGBSurface(0, DEVICE_WIDTH, DEVICE_HEIGHT, FIXED_DEPTH, RGBA_MASK_565);
	vid.screengame =  SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, FIXED_DEPTH, RGBA_MASK_565);
	vid.screen2 = SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, FIXED_DEPTH, RGBA_MASK_565); 
	LOG_info("vid.screen: %ix%i\n", vid.screen->w, vid.screen->h);fflush(stdout);
	LOG_info("vid.screengame: %ix%i\n", vid.screengame->w, vid.screengame->h);fflush(stdout);
	LOG_info("vid.screen2: %ix%i\n", vid.screen2->w, vid.screen2->h);fflush(stdout);

	vid.page = 0;
	if (init_ion_allocator()<0) {
		return NULL;
	}
	
	vid.renderingGame = 0;

	vid.offset = vid.screen_size;
	vid.linewidth = vid.width;

    swap_buffers_init();
    //swap_buffers(vid.page);
	vid.sharpness = SHARPNESS_SOFT;
	return vid.screen;
}

void PLAT_quitVideo(void) {
	PLAT_clearVideo(vid.screen);
	PLAT_flip(vid.screen, 1);
	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.screen2);
	SDL_FreeSurface(vid.screengame);
	clean_layer();
	munmap(vid.fbmmap[0], 0);
	munmap(vid.fbmmap[1], 0);
	free(vid.config_w);
	close(vid.dispfd);	
	close(vid.ion_fds[0]);
	close(vid.ion_fds[1]);
    close(vid.ionfd);
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
	memset(vid.fbmmap[0], 0, vid.screen_size);
	memset(vid.fbmmap[1], 0, vid.screen_size);
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
	if (remaining>0) {
		usleep(remaining*1000);
	} else {
		waitVsync();
	}
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

int FlipRotate000_r36s(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
	//this is actually a no rotation conversion.
	
	//copy a surface to the screen and flip it
	//it must be the same resolution, the bpp16 is then converted to 32bpp
	//LOG_info("Buffer has %d bpp\n", buffer->format->BitsPerPixel);fflush(stdout);

	//the alpha channel must be set to 0xff
	int thispitch = buffer->pitch/buffer->format->BytesPerPixel;
	int x, y, widthminus_1, heightminus_1;
	widthminus_1 = buffer->w - 1;
	heightminus_1 = buffer->h - 1;
	uint32_t *dsttmp;
	uint16_t *srctmp;
	//ok start conversion assuming it is RGB565		
	for (y = targetarea.y; y < (targetarea.y + targetarea.h) ; y++) {
		dsttmp = (uint32_t *)fbmmap + y * linewidth;
		srctmp = (uint16_t *)buffer->pixels + y * thispitch;
		for (x = targetarea.x; x < (targetarea.x + targetarea.w); x++) {
			uint16_t pixel = *((uint16_t *)srctmp + x);
			uint32_t r = (pixel & 0xF800) << 8;
			uint32_t g = (pixel & 0x7E0) << 5;
			uint32_t ba = 0xFF000000 | (pixel & 0x1F) << 3;
			*((uint32_t *)dsttmp + x ) = (uint32_t)( r | g | ba);
		}
	}	
	return 0;	
}

void *memset32(void *m, uint32_t val, size_t count)
{
    uint32_t *buf = m;

    while(count--) *buf++ = val;
    return m;
}

void PLAT_flip(SDL_Surface* IGNORED, int sync) { //this rotates minarch menu + minui + tools
	vid.page ^= 1;
	
	if (!vid.renderingGame) {
		vid.targetRect.x = 0;
		vid.targetRect.y = 0;
		vid.targetRect.w = vid.screen->w;
		vid.targetRect.h = vid.screen->h;
		//vid.page = 0;	
		if (vid.rotate == 0) 
		{
			// No Rotation
			FlipRotate000_r36s(vid.screen, vid.fbmmap[vid.page],vid.linewidth, vid.targetRect);
		}
		if (vid.rotate == 1)
		{
			// 90 Rotation
			FlipRotate090bgr(vid.screen, vid.fbmmap[vid.page],vid.linewidth, vid.targetRect);
		}
		if (vid.rotate == 2)
		{
			// 180 Rotation
			FlipRotate180bgr(vid.screen, vid.fbmmap[vid.page],vid.linewidth, vid.targetRect);
		}
		if (vid.rotate == 3)
		{
			// 270 Rotation
			FlipRotate270bgr(vid.screen, vid.fbmmap[vid.page],vid.linewidth, vid.targetRect);
		}
		 //to avoid tearing/flickering in the menu
		
	//	swap_buffers(vid.page);		
	} else {
		if ((vid.targetRect.w == vid.screen->w) && (vid.targetRect.h == vid.screen->h)) {
			//fullscreen
		//	pixman_composite_src_0565_8888_asm_neon(vid.screengame->w, vid.screengame->h, vid.fbmmap[vid.page], vid.screengame->pitch/2, vid.screengame->pixels, vid.screengame->pitch/2);
			neon_convert_565_to_8888(vid.screengame->w, vid.screengame->h, vid.fbmmap[vid.page], vid.screengame->pitch/2, vid.screengame->pixels, vid.screengame->pitch/2);
		} else {
			//window
			convert_rgb565_to_argb8888_neon_rect(vid.screengame->pixels, vid.fbmmap[vid.page], vid.screengame->w, vid.screengame->w, vid.targetRect.x, vid.targetRect.y, vid.targetRect.w, vid.targetRect.h);
		}
		
	}
	vid.renderingGame = 0;	
	swap_buffers(vid.page);	
	if (sync) PLAT_vsync(0);
	//pan_display(vid.page);
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
	ovl.overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, SCALE1(OVERLAY_WIDTH), SCALE1(OVERLAY_HEIGHT),OVERLAY_DEPTH,OVERLAY_RGBA_MASK);
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
	*is_charging = getInt("/sys/class/power_supply/axp2202-usb/online");

	int i = getInt("/sys/class/power_supply/axp2202-battery/capacity");
	// worry less about battery and more about the game you're playing
	     if (i>80) *charge = 100;
	else if (i>60) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>20) *charge =  40;
	else if (i>10) *charge =  20;
	else           *charge =  10;

	char status[16];
	getFile("/sys/class/net/wlan0/operstate", status,16);
	online = prefixMatch("up", status);
	//LOG_info("Online: %d\n", online);fflush(stdout);
}

#define BLANK_PATH "/sys/class/graphics/fb0/blank"
#define LED_PATH "/sys/class/power_supply/axp2202-battery/work_led"
void PLAT_enableBacklight(int enable) {
	if (enable) {
		putInt(BLANK_PATH, FB_BLANK_UNBLANK); // wake
		SetBrightness(GetBrightness());
		putInt(LED_PATH,0);
	}
	else {
		putInt(BLANK_PATH, FB_BLANK_POWERDOWN); // sleep
		SetRawBrightness(0);
		putInt(LED_PATH,1);
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
	switch (speed) {
		case CPU_SPEED_MENU: 		freq = cpufreq_menu; break;
		case CPU_SPEED_POWERSAVE:	freq = cpufreq_powersave; break;
		case CPU_SPEED_NORMAL: 		freq = cpufreq_game ; break;
		case CPU_SPEED_PERFORMANCE: freq = cpufreq_perf ; break;
		case CPU_SPEED_MAX:			freq = cpufreq_max ; break;	
		case CPU_SPEED_SLEEP:		freq = cpufreq_sleep ; break;
	}
	char cmd[512];
	//sudo sh -c "echo -n 1512000 > /sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed"
	sprintf(cmd,"sudo sh -c \"echo -n userspace > %s \" ; sudo sh -c \"echo %i > %s\"", GOVERNOR_PATH, freq, GOVERNOR_CPUSPEED_PATH);
	if (freq) {
		system(cmd);
		LOG_info("Set CPU speed to %i\n", freq);
		cur_cpu_freq = freq/1000;
	}
}


enum retro_rumble_effect
{
   RETRO_RUMBLE_STRONG = 0,
   RETRO_RUMBLE_WEAK = 1,
   RETRO_RUMBLE_DUMMY = 10
};

static int rumblenum[2] = {-1,-1};
void PLAT_setRumble(int effect, int strength) {
	// buh
	/* Create new or update old playing state. */
    struct ff_effect e      = {0};
	struct input_event play;
    e.type   = FF_RUMBLE;
    e.id     = rumblenum[effect];
    e.replay.delay = 0;
	e.replay.length = 0xc000; // 1 second    e.u.rumble.strong_magnitude = strength;
	if (effect == RETRO_RUMBLE_STRONG) {
		e.u.rumble.strong_magnitude = strength;
	} else {
		e.u.rumble.weak_magnitude = strength;
	}

//	LOG_info("Setting rumble %d effect %d with strength %d\n", rumblenum[effect], effect, strength);fflush(stdout);
    //rumblenum++;
	if (ioctl(rumblefd, EVIOCSFF, &e) < 0)
	{
	//	LOG_info("Failed to set rumble effect num %d\n", rumblenum);
		return;
	} else {
		memset(&play,0,sizeof(play));
		play.type = EV_FF;
		play.code = e.id;
		rumblenum[effect] = e.id;
		play.value = 1;	
		if (write(rumblefd, (const void*) &play, sizeof(play)) < sizeof(play)) {
	//		LOG_info("Unable to Play rumble effect");fflush(stdout);
			;
		}
	}
}

int PLAT_pickSampleRate(int requested, int max) {
	return MAX(requested, max);
}

char* PLAT_getModel(void) {

	return "H700 SP";
}

int PLAT_isOnline(void) {
	return online;
}


char* PLAT_getIPAddress(void) {
    FILE *fp;
    char _buffer[256];
    char *outstr = NULL;

    // Esegue il comando e legge l'output
    fp = popen("ip route | cut -d' ' -f9 | uniq | grep \"\\.\"", "r");
/*
 ip route
default via 192.168.1.1 dev wlan0 proto dhcp metric 600 
192.168.1.0/24 dev wlan0 proto kernel scope link src 192.168.1.247 metric 600 
*/

    if (fp == NULL) {
        LOG_info("getIpAddress popen failed - %s\n", strerror(errno));
        return NULL; // Restituisce NULL in caso di errore
    }

    // Legge l'output riga per riga
    if (fgets(_buffer, sizeof(_buffer), fp) != NULL) {
        // Alloca memoria per la stringa di output
        size_t len = strlen(_buffer);
        if (len > 0 && _buffer[len - 1] == '\n') {
            _buffer[len - 1] = '\0'; // Rimuove il carattere di newline
        }
        outstr = malloc(len + 1); // Alloca memoria per la stringa
        if (outstr != NULL) {
            strcpy(outstr, _buffer); // Copia la stringa
        } else {
            LOG_info("Memory allocation failed for IP address\n");
        }
    }

    // Chiude lo stream
    int status = pclose(fp);
    if (status == -1) {
        LOG_info("pclose failed - %s\n", strerror(errno));
    }
	
    return outstr; // Restituisce la stringa allocata o NULL
}

int PLAT_getNumProcessors(void) {
	//the core can be deactivated by command line
	return sysconf(_SC_NPROCESSORS_ONLN);
}

int PLAT_getProcessorTemp(void) {
	int temp = getInt("/sys/class/thermal/thermal_zone0/temp");
	return temp / 1000;
}

uint32_t PLAT_screenMemSize(void) {
	return vid.screen_size;
}

void PLAT_getAudioOutput(void){
//buh!
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

uint32_t PLAT_getVsyncInterval(void){
	return vid.vsync_refresh;
}