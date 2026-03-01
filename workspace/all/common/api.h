#ifndef __API_H__
#define __API_H__
#include "sdl.h"
#include "platform.h"

///////////////////////////////

enum {
	LOG_DEBUG = 0,
	LOG_INFO,
	LOG_WARN,
	LOG_ERROR,
};

#define LOG_debug(fmt, ...) LOG_note(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_info(fmt, ...) LOG_note(LOG_INFO, fmt, ##__VA_ARGS__)
#define LOG_warn(fmt, ...) LOG_note(LOG_WARN, fmt, ##__VA_ARGS__)
#define LOG_error(fmt, ...) LOG_note(LOG_ERROR, fmt, ##__VA_ARGS__)
void LOG_note(int level, const char* fmt, ...);

///////////////////////////////

#define PAGE_COUNT	2
#define PAGE_SCALE	3
#define PAGE_WIDTH	(FIXED_WIDTH * PAGE_SCALE)
#define PAGE_HEIGHT	(FIXED_HEIGHT * PAGE_SCALE)
#define PAGE_PITCH	(PAGE_WIDTH * FIXED_BPP)
#define PAGE_SIZE	(PAGE_PITCH * PAGE_HEIGHT)

///////////////////////////////

extern int USER_BTN_UP;
extern int USER_BTN_DOWN;
extern int USER_BTN_LEFT;
extern int USER_BTN_RIGHT;
extern int USER_BTN_A;
extern int USER_BTN_B;
extern int USER_BTN_X;
extern int USER_BTN_Y;
extern int USER_BTN_L1;
extern int USER_BTN_L2;
extern int USER_BTN_L3;
extern int USER_BTN_R1;
extern int USER_BTN_R2;
extern int USER_BTN_R3;
extern int USER_BTN_START;
extern int USER_BTN_SELECT;
extern int USER_BTN_MENU;
extern int USER_BTN_VOLUMEUP;
extern int USER_BTN_VOLUMEDOWN;
extern int USER_BTN_POWER;



///////////////////////////////

extern int DEVICE_WIDTH;
extern int DEVICE_HEIGHT;
extern int GAME_WIDTH;
extern int GAME_HEIGHT;
extern int GAME_PITCH;
extern int DEVICE_PITCH;
extern uint32_t frame_start;
extern int TARGET_FPS;
extern uint32_t cur_cpu_freq;

//////////////////////////////
// TODO: these only seem to be used by a tmp.pak in trimui (model s)
// used by minarch, optionally defined in platform.h
#ifndef PLAT_PAGE_BPP
#define PLAT_PAGE_BPP 	FIXED_BPP
#endif
#define PLAT_PAGE_DEPTH (PLAT_PAGE_BPP * 8)
#define PLAT_PAGE_PITCH (PAGE_WIDTH * PLAT_PAGE_BPP)
#define PLAT_PAGE_SIZE	(PLAT_PAGE_PITCH * PAGE_HEIGHT)

///////////////////////////////

#define RGBA_MASK_AUTO	0x0, 0x0, 0x0, 0x0
#define RGBA_MASK_565	0xF800, 0x07E0, 0x001F, 0x0000
#define RGBA_MASK_8888	0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000
#ifndef SCREEN_FPS
#define SCREEN_FPS 60.0
#endif
///////////////////////////////

extern uint32_t RGB_WHITE;
extern uint32_t RGB_BLACK;
extern uint32_t RGB_LIGHT_GRAY;
extern uint32_t RGB_GRAY;
extern uint32_t RGB_DARK_GRAY;
extern float currentratio;
extern int currentbufferfree;
extern int currentframecount;
extern double currentfps;
extern double currentreqfps;
extern float currentbufferms;
extern int currentbuffersize;
extern int currentsampleratein;
extern int currentsamplerateout;
extern int use_nofix;

extern int FIXED_SCALE;

enum {
	ASSET_WHITE_PILL,
	ASSET_BLACK_PILL,
	ASSET_DARK_GRAY_PILL,
	ASSET_OPTION,
	ASSET_BUTTON,
	ASSET_PAGE_BG,
	ASSET_STATE_BG,
	ASSET_PAGE,
	ASSET_BAR,
	ASSET_BAR_BG,
	ASSET_BAR_BG_MENU,
	ASSET_UNDERLINE,
	ASSET_DOT,
	ASSET_HOLE,
	
	ASSET_COLORS,
	
	ASSET_BRIGHTNESS,
	ASSET_VOLUME_MUTE,
	ASSET_VOLUME,
	ASSET_BATTERY,
	ASSET_BATTERY_LOW,
	ASSET_BATTERY_FILL,
	ASSET_BATTERY_FILL_LOW,
	ASSET_BATTERY_BOLT,
	
	ASSET_SCROLL_UP,
	ASSET_SCROLL_DOWN,
	
	ASSET_WIFI,
	ASSET_RED_DOT,
	ASSET_RED_PAGE,
};

typedef struct GFX_Fonts {
	TTF_Font* large; 	// menu
	TTF_Font* medium; 	// single char button label
	TTF_Font* small; 	// button hint
	TTF_Font* tiny; 	// multi char button label
	TTF_Font* largeoutline; // for fancy mode
} GFX_Fonts;
extern GFX_Fonts font;

enum {
	SHARPNESS_SHARP,
	SHARPNESS_CRISP,
	SHARPNESS_SOFT,
};

enum {
	EFFECT_NONE,
	EFFECT_LINE,
	EFFECT_GRID,
	EFFECT_COUNT,
};

/*
struct mybackbuffer {
	int size;
	int depth;
	int w;
	int h;
	int pitch;
	uint16_t* pixels;
};*/
// api.h - Aggiungi padding:
struct mybackbuffer {
    int size;
    int depth;
    int w;
    int h;
    int pitch;
    uint16_t* pixels;
    char __padding[8];  // ← Per 64-bit alignment
} __attribute__((aligned(64)));  // ← Evita false sharing

typedef struct GFX_Renderer {
	void* src;
	struct mybackbuffer* src_surface;
	void* dst;
	void* blit;
	double aspect; // 0 for integer, -1 for fullscreen, otherwise aspect ratio, used for SDL2 accelerated scaling
	double scale;
	int rotate; // 0=0, 1=90, 2=180, 3=270  value set by core and screen orientation
	int rotategame; // 0=0, 1=90, 2=180, 3=270  value set by core
	int resize;
	int screenscaling;
	// TODO: document this better
	int true_w;
	int true_h;
	int true_p;

	int src_x;
	int src_y;
	int src_w;
	int src_h;
	int src_p;
	
	// TODO: I think this is overscaled
	int dst_x;
	int dst_y;
	int dst_w;
	int dst_h;
	int dst_p;
} GFX_Renderer;

enum {
	MODE_MAIN,
	MODE_MENU,
};

//int fancy_mode;

void InitAssetRects(void);


//lid support, stolen as is from minui
typedef struct LID_Context {
	int has_lid;
	int is_open;
} LID_Context;
extern LID_Context lid;

void PLAT_initLid(void);
int PLAT_lidChanged(int* state);
//end of lid support

SDL_Surface* GFX_init(int mode);
#define GFX_resize PLAT_resizeVideo // (int w, int h, int pitch);
#define GFX_setScaleClip PLAT_setVideoScaleClip // (int x, int y, int width, int height)
#define GFX_setNearestNeighbor PLAT_setNearestNeighbor // (int enabled)
#define GFX_setSharpness PLAT_setSharpness // (int sharpness)
#define GFX_setEffect PLAT_setEffect // (int effect)
void GFX_setMode(int mode);
int GFX_hdmiChanged(void);

#define GFX_clear PLAT_clearVideo // (SDL_Surface* screen)
#define GFX_clearAll PLAT_clearAll // (void)

void GFX_startFrame(void);
void audioFPS(void);
void GFX_flip(SDL_Surface* screen);
void GFX_flipNoFix(SDL_Surface* screen);
void PLAT_flipHidden();
void GFX_flip_fixed_rate(SDL_Surface* screen, double target_fps); // if target_fps is 0, then use the native screen FPS
#define GFX_supportsOverscan PLAT_supportsOverscan // (void)
void GFX_sync(void); // call this to maintain 60fps when not calling GFX_flip() this frame
void GFX_sync_fixed_rate(double target_fps);
void GFX_delay(void); // gfx_sync() is only for everywhere where there is no audio buffer to rely on for delaying, stupid so doing gfx_delay() for like waiting for input loop in binding menu. Need to remove gfx_sync() everwhere eventually
void GFX_quit(void);
void GFX_pan(void);

/*
//used to convert the frame sent by the core ro the rgb565 format, the 0565_0565 acts as memcpy but faster.
void pixman_composite_src_8888_0565_asm_neon(int width, int height,
	uint16_t *dst, int dst_stride_pixels, const uint32_t *src, int src_stride_pixels);

void pixman_composite_src_1555_0565_asm_neon(int width, int height,
	uint16_t *dst, int dst_stride_pixels, const uint16_t *src, int src_stride_pixels);

void pixman_composite_src_0565_0565_asm_neon(int width, int height,
	uint16_t *dst, int dst_stride_pixels, const uint16_t *src, int src_stride_pixels);


//retroarch uses it in its sunxi implementation, quick way to convert and copy a buffer.
void pixman_composite_src_0565_8888_asm_neon(int width,
	int height,
	uint32_t *dst,
	int dst_stride_pixels,
	uint16_t *src,
	int src_stride_pixels);

// same as above, I added this version that swap r and b channels 
void pixman_composite_src_0565_8888_asm_neon_bgr(int width,
	int height,
	uint32_t *dst,
	int dst_stride_pixels,
	uint16_t *src,
	int src_stride_pixels);
*/

void convert_argb1555_to_rgb565_neon(int width, int height,
    uint16_t *dst, int dst_stride_pixels, const uint16_t *src, int src_stride_pixels);

// chatgpt contributed that, this is the faster and accuratereplacement of SDL_SoftStretch, 
//better because dedicated to rgb565 format only
int scale_mat_nearest_lut_rgb565_neon_fast_xy_pitch(
    const uint16_t *src_ptr, int src_w, int src_h, int src_pitch,
    uint16_t *dst_ptr, int dst_w, int dst_h, int dst_pitch,
    int dst_x, int dst_y, int out_w, int out_h);

enum {
	VSYNC_OFF = 0,
	VSYNC_LENIENT, // default
	VSYNC_STRICT,
};

int GFX_getVsync(void);
void GFX_setVsync(int vsync);

int GFX_truncateText(TTF_Font* font, const char* in_name, char* out_name, int max_width, int padding); // returns final width
int GFX_wrapText(TTF_Font* font, char* str, int max_width, int max_lines);

//#define GFX_getScaler PLAT_getScaler		// scaler_t:(GFX_Renderer* renderer)
#define GFX_blitRenderer PLAT_blitRenderer	// void:(GFX_Renderer* renderer)

//scaler_t GFX_getAAScaler(GFX_Renderer* renderer);
void GFX_freeAAScaler(void);

// NOTE: all dimensions should be pre-scaled
void GFX_blitAsset(int asset, SDL_Rect* src_rect, SDL_Surface* dst, SDL_Rect* dst_rect);
void GFX_blitPill(int asset, SDL_Surface* dst, SDL_Rect* dst_rect);
void GFX_blitRect(int asset, SDL_Surface* dst, SDL_Rect* dst_rect);
void GFX_blitBattery(SDL_Surface* dst, SDL_Rect* dst_rect);
int GFX_getButtonWidth(char* hint, char* button);
void GFX_blitButton(char* hint, char*button, SDL_Surface* dst, SDL_Rect* dst_rect);
void GFX_blitMessage(TTF_Font* font, char* msg, SDL_Surface* dst, SDL_Rect* dst_rect);

int GFX_blitHardwareGroup(SDL_Surface* dst, int show_setting, int _fancy_mode);
void GFX_blitHardwareHints(SDL_Surface* dst, int show_setting, int _fancy_mode);
int GFX_blitButtonGroup(char** hints, int primary, SDL_Surface* dst, int align_right, int _fancy_mode);

void GFX_sizeText(TTF_Font* font, char* str, int leading, int* w, int* h);
void GFX_blitText(TTF_Font* font, char* str, int leading, SDL_Color color, SDL_Surface* dst, SDL_Rect* dst_rect);

///////////////////////////////

typedef struct SND_Frame {
	int16_t left;
	int16_t right;
} SND_Frame;

typedef struct {
	SND_Frame* frames;
	int frame_count;
} ResampledFrames;

void SND_init(double sample_rate, double frame_rate);
size_t SND_batchSamplesNoFix(const SND_Frame* frames, size_t frame_count);
size_t SND_batchSamples(const SND_Frame* frames, size_t frame_count);
size_t SND_batchSamples_fixed_rate(const SND_Frame* frames, size_t frame_count);
void SND_quit(void);
void SND_resetAudio(double sample_rate, double frame_rate);
void SND_setQuality(int quality);

///////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PAD_Axis {
		int x;
		int y;
} PAD_Axis;
typedef struct PAD_Context {
	int is_pressed;
	int just_pressed;
	int just_released;
	int just_released_short;
	int just_repeated;
	uint32_t repeat_at[BTN_ID_COUNT];
	uint32_t begin_time[BTN_ID_COUNT];
	PAD_Axis laxis;
	PAD_Axis raxis;
	int map_leftstick_to_dpad;
} PAD_Context;
extern PAD_Context pad;

#define PAD_REPEAT_DELAY	300
#define PAD_REPEAT_INTERVAL 100

#define PAD_init PLAT_initInput
#define PAD_quit PLAT_quitInput

#ifndef PAD_poll
#define PAD_poll PAD_poll_SDL
#define PAD_wake PAD_wake_SDL
void PAD_poll_SDL(void);
int PAD_wake_SDL(void);
#endif

void PAD_setAnalog(int neg, int pos, int value, int repeat_at); // internal

void PAD_reset(void);
int PAD_anyJustPressed(void);
int PAD_anyPressed(void);
int PAD_anyJustReleased(void);
int PAD_anyJustReleasedShort(void);

int PAD_justPressed(int btn);
int PAD_isPressed(int btn);
int PAD_justReleased(int btn);
int PAD_justReleasedShort(int btn);
int PAD_justRepeated(int btn);

int PAD_tappedMenu(uint32_t now); // special case, returns 1 on release of BTN_MENU within 250ms if BTN_PLUS/BTN_MINUS haven't been pressed
void PAD_readCustomButtonMapping(void);

#ifdef __cplusplus
}
#endif

///////////////////////////////

void VIB_init(void);
void VIB_quit(void);
void VIB_setStrength(int port, int effect, int strength);
int VIB_getStrength(void);
	
///////////////////////////////

#define BRIGHTNESS_BUTTON_LABEL "+ -" // ew

typedef void (*PWR_callback_t)(void);
void PWR_init(void);
void PWR_quit(void);
void PWR_warn(int enable);

int PWR_ignoreSettingInput(int btn, int show_setting);
void PWR_update(int* dirty, int* show_setting, PWR_callback_t before_sleep, PWR_callback_t after_sleep);

void PWR_disablePowerOff(void);
void PWR_powerOff(void);
int PWR_isPoweringOff(void);

void PWR_fauxSleep(void);

void PWR_disableSleep(void);
void PWR_enableSleep(void);

void PWR_disableAutosleep(void);
void PWR_enableAutosleep(void);
int PWR_preventAutosleep(void);

int PWR_isCharging(void);
int PWR_getBattery(void);

enum {
	SCALE_NATIVE,
	SCALE_ASPECT,
	SCALE_EXTENDED,
	SCALE_FULLSCREEN,
	SCALE_4_3,
	SCALE_3_2,
	SCALE_COUNT,
};


enum {
	CPU_SPEED_MENU,
	CPU_SPEED_POWERSAVE,
	CPU_SPEED_NORMAL,
	CPU_SPEED_PERFORMANCE,
	CPU_SPEED_MAX,
};
#define PWR_setCPUSpeed PLAT_setCPUSpeed

///////////////////////////////

void PLAT_initInput(void);
void PLAT_quitInput(void);
void PLAT_pollInput(void);
int PLAT_shouldWake(void);

SDL_Surface* PLAT_initVideo(void);
void PLAT_quitVideo(void);
void PLAT_clearVideo(SDL_Surface* screen);
void PLAT_clearAll(void);
void PLAT_setVsync(int vsync);
SDL_Surface* PLAT_resizeVideo(int w, int h, int pitch);
void PLAT_setVideoScaleClip(int x, int y, int width, int height);
void PLAT_setNearestNeighbor(int enabled);
void PLAT_setSharpness(int sharpness);
void PLAT_setEffect(int effect);
void PLAT_vsync(int remaining);
//scaler_t PLAT_getScaler(GFX_Renderer* renderer);
void PLAT_blitRenderer(GFX_Renderer* renderer);
void PLAT_flip(SDL_Surface* screen, int sync);
void PLAT_pan(void);

SDL_Surface* PLAT_initOverlay(void);
void PLAT_quitOverlay(void);
void PLAT_enableOverlay(int enable);
	
#define PWR_LOW_CHARGE 10
void PLAT_getBatteryStatus(int* is_charging, int* charge); // 0,1 and 0,10,20,40,60,80,100
void PLAT_enableBacklight(int enable);
void PLAT_powerOff(void);
	
void PLAT_setCPUSpeed(int speed); // enum
void PLAT_setRumble(int effect, int strength);
int PLAT_pickSampleRate(int requested, int max);

char* PLAT_getModel(void);
int PLAT_isOnline(void);
char* PLAT_getIPAddress(void);

int PLAT_getNumProcessors(void);
void PLAT_getAudioOutput(void);
uint32_t PLAT_screenMemSize(void);
int PLAT_getProcessorTemp(void);

int PLAT_getScreenRotation(int game);
SDL_Surface* PLAT_getScreenGame(void);

int FlipRotate000(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea);
int FlipRotate090(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea);
int FlipRotate180(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea);
int FlipRotate270(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea);

int FlipRotate000bgr(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea);
int FlipRotate090bgr(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea);
int FlipRotate180bgr(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea);
int FlipRotate270bgr(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea);

int FlipRotate000_16(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea);
int FlipRotate090_16(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea);
int FlipRotate180_16(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea);
int FlipRotate270_16(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea);

void rotateIMG(void *src, void*dst, int rotation, int srcw, int srch, int srcp);
void convert_rgb565_to_argb8888_neon_rect(
    const uint16_t *src,
    uint32_t *dst,
    int src_width,
    int dst_pitch, // in pixels (not bytes)
    int start_x,
    int start_y,
    int rect_width,
    int rect_height
);

void convert_rgb565_to_abgr8888_neon_rect(
    const uint16_t *src,
    uint32_t *dst,
    int src_width,
    int dst_pitch, // in pixels (not bytes)
    int start_x,
    int start_y,
    int rect_width,
    int rect_height
); 

void neon_convert_565_to_8888(int width, int height, 
                              uint32_t *dst, int dst_pitch, 
                              const uint16_t *src, int src_pitch);

void neon_convert_565_to_8888_abgr(int width, int height, 
                              uint32_t *dst, int dst_pitch, 
                              const uint16_t *src, int src_pitch);

void neon_convert_8888_to_565(int width, int height, 
                              uint16_t *dst, int dst_pitch, 
                              const uint32_t *src, int src_pitch);

void neon_copy_rgb565(int width, int height, 
                      uint16_t *dst, int dst_pitch, 
                      const uint16_t *src, int src_pitch);

void scale1x_line(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp);
void scale1x_grid(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp);

#endif
