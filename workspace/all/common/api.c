#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <msettings.h>
#include <samplerate.h>
#include "defines.h"
#include "api.h"
#include "utils.h"

///////////////////////////////

extern int fancy_mode;
int DEVICE_WIDTH;
int DEVICE_HEIGHT;
int GAME_WIDTH;
int GAME_HEIGHT;
int DEVICE_PITCH;
int TARGET_FPS;
int IS_MINARCH = 0;

void LOG_note(int level, const char* fmt, ...) {
	char buf[1024] = {0};
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	switch(level) {
#ifdef DEBUG
	case LOG_DEBUG:
		printf("[DEBUG] %s", buf);
		break;
#endif
	case LOG_INFO:
		printf("[INFO] %s", buf);
		break;
	case LOG_WARN:
		fprintf(stderr, "[WARN] %s", buf);
		break;
	case LOG_ERROR:
		fprintf(stderr, "[ERROR] %s", buf);
		break;
	default:
		break;
	}
	fflush(stdout);
}

///////////////////////////////

uint32_t RGB_WHITE;
uint32_t RGB_RED;
uint32_t RGB_BLACK;
uint32_t RGB_LIGHT_GRAY;
uint32_t RGB_GRAY;
uint32_t RGB_DARK_GRAY;

float currentbufferms = 20.0;

static struct GFX_Context {
	SDL_Surface* screen;
	SDL_Surface* assets;

	int mode;
	int vsync;
} gfx;

static SDL_Rect asset_rects[] = {
	[ASSET_WHITE_PILL]		= {SCALE4( 1, 1,30,30)},
	[ASSET_BLACK_PILL]		= {SCALE4(33, 1,30,30)},
	[ASSET_DARK_GRAY_PILL]	= {SCALE4(65, 1,30,30)},
	[ASSET_OPTION]			= {SCALE4(97, 1,20,20)},
	[ASSET_BUTTON]			= {SCALE4( 1,33,20,20)},
	[ASSET_PAGE_BG]			= {SCALE4(64,33,15,15)},
	[ASSET_STATE_BG]		= {SCALE4(23,54, 8, 8)},
	[ASSET_PAGE]			= {SCALE4(39,54, 6, 6)},
	[ASSET_BAR]				= {SCALE4(33,58, 4, 4)},
	[ASSET_BAR_BG]			= {SCALE4(15,55, 4, 4)},
	[ASSET_BAR_BG_MENU]		= {SCALE4(85,56, 4, 4)},
	[ASSET_UNDERLINE]		= {SCALE4(85,51, 3, 3)},
	[ASSET_DOT]				= {SCALE4(33,54, 2, 2)},
	
	[ASSET_BRIGHTNESS]		= {SCALE4(23,33,19,19)},
	[ASSET_VOLUME_MUTE]		= {SCALE4(44,33,10,16)},
	[ASSET_VOLUME]			= {SCALE4(44,33,18,16)},
	[ASSET_BATTERY]			= {SCALE4(47,51,17,10)},
	[ASSET_BATTERY_LOW]		= {SCALE4(66,51,17,10)},
	[ASSET_BATTERY_FILL]	= {SCALE4(81,33,12, 6)},
	[ASSET_BATTERY_FILL_LOW]= {SCALE4( 1,55,12, 6)},
	[ASSET_BATTERY_BOLT]	= {SCALE4(81,41,12, 6)},
	
	[ASSET_SCROLL_UP]		= {SCALE4(97,23,24, 6)},
	[ASSET_SCROLL_DOWN]		= {SCALE4(97,31,24, 6)},

	[ASSET_WIFI]			= {SCALE4(95,39,14,10)},
	[ASSET_HOLE]			= {SCALE4( 1,63,20,20)},
	[ASSET_RED_DOT]			= {SCALE4( 33,54, 2, 2)},
	[ASSET_RED_PAGE] 		= {SCALE4( 92,64, 6, 6)},
};
static uint32_t asset_rgbs[ASSET_COLORS];
GFX_Fonts font;

///////////////////////////////
static int qualityLevels[] = {
	3,
	4,
	2,
	1
};
static struct PWR_Context {
	int initialized;
	
	int can_sleep;
	int can_poweroff;
	int can_autosleep;
	
	pthread_t battery_pt;
	int is_charging;
	int charge;
	int should_warn;

	SDL_Surface* overlay;
} pwr = {0};

#define BATCH_SIZE_NOFIX 400

typedef int (*SND_Resampler)(const SND_Frame frame);

static struct SND_Context {
	int initialized;
	double frame_rate;
	
	int sample_rate_in;
	int sample_rate_out;
	
	int buffer_seconds;     // current_audio_buffer_size
	SND_Frame* buffer;		// buf
	size_t frame_count; 	// buf_len
	
	int frame_in;     // buf_w
	int frame_out;    // buf_r
	int frame_filled; // max_buf_w
	
	SND_Resampler resample;
} snd = {0};

///////////////////////////////

static int _;
static double current_fps = SCREEN_FPS;
static int fps_counter = 0;
double currentfps = 0.0;
double currentreqfps = 0.0;

int currentbuffersize = 0;
int currentsampleratein = 0;
int currentsamplerateout = 0;
int should_rotate = 0;
int currentcputemp = 0;
int use_nofix = 0;

SDL_Surface* GFX_init(int mode) {
	gfx.screen = PLAT_initVideo();
	gfx.vsync = VSYNC_STRICT;
	gfx.mode = mode;
	
	RGB_WHITE		= SDL_MapRGB(gfx.screen->format, TRIAD_WHITE);
	RGB_RED			= SDL_MapRGB(gfx.screen->format, TRIAD_RED);
	RGB_BLACK		= SDL_MapRGB(gfx.screen->format, TRIAD_BLACK);
	RGB_LIGHT_GRAY	= SDL_MapRGB(gfx.screen->format, TRIAD_LIGHT_GRAY);
	RGB_GRAY		= SDL_MapRGB(gfx.screen->format, TRIAD_GRAY);
	RGB_DARK_GRAY	= SDL_MapRGB(gfx.screen->format, TRIAD_DARK_GRAY);
	
	asset_rgbs[ASSET_WHITE_PILL]	= RGB_WHITE;
	asset_rgbs[ASSET_BLACK_PILL]	= RGB_BLACK;
	asset_rgbs[ASSET_DARK_GRAY_PILL]= RGB_DARK_GRAY;
	asset_rgbs[ASSET_OPTION]		= RGB_DARK_GRAY;
	asset_rgbs[ASSET_BUTTON]		= RGB_WHITE;
	asset_rgbs[ASSET_PAGE_BG]		= RGB_WHITE;
	asset_rgbs[ASSET_STATE_BG]		= RGB_WHITE;
	asset_rgbs[ASSET_PAGE]			= RGB_BLACK;
	asset_rgbs[ASSET_BAR]			= RGB_WHITE;
	asset_rgbs[ASSET_BAR_BG]		= RGB_BLACK;
	asset_rgbs[ASSET_BAR_BG_MENU]	= RGB_DARK_GRAY;
	asset_rgbs[ASSET_UNDERLINE]		= RGB_GRAY;
	asset_rgbs[ASSET_DOT]			= RGB_LIGHT_GRAY;
	asset_rgbs[ASSET_HOLE]			= RGB_BLACK;
	asset_rgbs[ASSET_RED_DOT]		= RGB_RED;
	asset_rgbs[ASSET_RED_PAGE] 		= RGB_RED;
	
	char asset_path[MAX_PATH];
	sprintf(asset_path, RES_PATH "/assets@%ix.png", FIXED_SCALE);
	gfx.assets = IMG_Load(asset_path);
	
	TTF_Init();
	font.large 	= TTF_OpenFont(FONT_PATH, SCALE1(FONT_LARGE));
	font.largeoutline = TTF_OpenFont(FONT_PATH, SCALE1(FONT_LARGE));
	font.medium = TTF_OpenFont(FONT_PATH, SCALE1(FONT_MEDIUM));
	font.small 	= TTF_OpenFont(FONT_PATH, SCALE1(FONT_SMALL));
	font.tiny 	= TTF_OpenFont(FONT_PATH, SCALE1(FONT_TINY));
	
	TTF_SetFontStyle(font.large, TTF_STYLE_BOLD);
	TTF_SetFontStyle(font.medium, TTF_STYLE_BOLD);
	TTF_SetFontStyle(font.small, TTF_STYLE_BOLD);
	TTF_SetFontStyle(font.tiny, TTF_STYLE_BOLD);
	TTF_SetFontStyle(font.largeoutline, TTF_STYLE_BOLD);

	TTF_SetFontOutline(font.largeoutline, 1);
	
	
	return gfx.screen;
}
void GFX_quit(void) {
	TTF_CloseFont(font.large);
	TTF_CloseFont(font.largeoutline);
	TTF_CloseFont(font.medium);
	TTF_CloseFont(font.small);
	TTF_CloseFont(font.tiny);
	
	SDL_FreeSurface(gfx.assets);
	
	GFX_freeAAScaler();
	
	GFX_clearAll();

	PLAT_quitVideo();
}

void GFX_setMode(int mode) {
	gfx.mode = mode;
}
int GFX_getVsync(void) {
	return gfx.vsync;
}
void GFX_setVsync(int vsync) {
	PLAT_setVsync(vsync);
	gfx.vsync = vsync;
}

int GFX_hdmiChanged(void) {
	static int had_hdmi = -1;
	int has_hdmi = GetHDMI();
	if (had_hdmi==-1) had_hdmi = has_hdmi;
	if (had_hdmi==has_hdmi) return 0;
	had_hdmi = has_hdmi;
	return 1;
}

#define FRAME_BUDGET 17 // 60fps
uint32_t frame_start = 0;

static uint64_t per_frame_start = 0;
#define FPS_BUFFER_SIZE 50
// filling with  60.1 cause i'd rather underrun than overflow in start phase
static double fps_buffer[FPS_BUFFER_SIZE] = {60.1};
static int fps_buffer_index = 0;

void GFX_startFrame(void) {
	frame_start = SDL_GetTicks();
}

void GFX_pan(void) {
	PLAT_pan();
}

void GFX_flipNoFix(SDL_Surface* screen) {
	int should_vsync = (gfx.vsync!=VSYNC_OFF && (gfx.vsync==VSYNC_STRICT || frame_start==0 || SDL_GetTicks()-frame_start<FRAME_BUDGET));
	PLAT_flip(screen, should_vsync);
}


void GFX_flip(SDL_Surface* screen) {
	
	int should_vsync = (gfx.vsync!=VSYNC_OFF && (gfx.vsync==VSYNC_STRICT || frame_start==0 || SDL_GetTicks()-frame_start<FRAME_BUDGET));
	PLAT_flip(screen, should_vsync);

	currentfps = current_fps;
	fps_counter++;

	uint64_t performance_frequency = SDL_GetPerformanceFrequency();
	uint64_t frame_duration = SDL_GetPerformanceCounter() - per_frame_start;
	double elapsed_time_s = (double)frame_duration / performance_frequency;
	double tempfps = 1.0 / elapsed_time_s;

	if(tempfps < SCREEN_FPS * 0.9 || tempfps > SCREEN_FPS * 1.1) tempfps = SCREEN_FPS;
	
	fps_buffer[fps_buffer_index] = tempfps;
	fps_buffer_index = (fps_buffer_index + 1) % FPS_BUFFER_SIZE;
	// give it a little bit to stabilize and then use, meanwhile the buffer will
	// cover it
	if (fps_counter > 100) {
		double average_fps = 0.0;
		int fpsbuffersize = MIN(fps_counter, FPS_BUFFER_SIZE);
		for (int i = 0; i < fpsbuffersize; i++) {
			average_fps += fps_buffer[i];
		}
		average_fps /= fpsbuffersize;
		current_fps = average_fps;
	}
	
	per_frame_start = SDL_GetPerformanceCounter();
}
// eventually this function should be removed as its only here because of all the audio buffer based delay stuff
void GFX_sync(void) {
	uint32_t frame_duration = SDL_GetTicks() - frame_start;
	if (gfx.vsync!=VSYNC_OFF) {
		// this limiting condition helps SuperFX chip games
		if (gfx.vsync==VSYNC_STRICT || frame_start==0 || frame_duration<FRAME_BUDGET) { // only wait if we're under frame budget
			PLAT_vsync(FRAME_BUDGET-frame_duration);
		}
	}
	else {
		if (frame_duration<FRAME_BUDGET) SDL_Delay(FRAME_BUDGET-frame_duration);
	}
}

void GFX_flip_fixed_rate(SDL_Surface* screen, double target_fps) {
	if (target_fps == 0.0) target_fps = SCREEN_FPS;
	double frame_budget_ms = 1000.0 / target_fps;

	static int64_t frame_index = -1;
	static int64_t first_frame_start_time = 0;
	static double last_target_fps = 0.0;

	int64_t perf_freq = SDL_GetPerformanceFrequency();
	int64_t now = SDL_GetPerformanceCounter();

	if (++frame_index == 0 || target_fps != last_target_fps) {
		frame_index = 0;
		first_frame_start_time = now;
		last_target_fps = target_fps;
	}

	int64_t frame_duration = perf_freq / target_fps;
	int64_t time_of_frame = first_frame_start_time + frame_index * frame_duration;
	int64_t offset = now - time_of_frame;
	const int max_lost_frames = 2;

	// printf("%s: frame #%lld, time is %lld, scheduled at %lld, offset is %lld\n",
	// 	__FUNCTION__,
	// 	frame_index,
	// 	now,
	// 	time_of_frame,
	// 	now - time_of_frame);

	if (offset > 0) {
		if (offset > max_lost_frames * frame_duration) {
			frame_index = -1;
			last_target_fps = 0.0;
			LOG_debug("%s: lost sync by more than %d frames (late) @%llu -> reset\n\n", __FUNCTION__, max_lost_frames, SDL_GetPerformanceCounter());
		}
	}
	else {
		if (offset < -max_lost_frames * frame_duration) {
			frame_index = -1;
			last_target_fps = 0.0;
			LOG_debug("%s: lost sync by more than %d frames (early ?!) @%llu -> reset\n\n", __FUNCTION__, max_lost_frames, SDL_GetPerformanceCounter());
		}
		else if (offset < 0) {
			useconds_t time_to_sleep_us = (useconds_t) ((time_of_frame - now) * 1e6 / perf_freq);

			// The OS scheduling algorithm cannot guarantee that
			// the sleep will last the exact amount of requested time.
			// We sleep as much as we can using the OS primitive.
			const useconds_t min_waiting_time = 2000;
			if (time_to_sleep_us > min_waiting_time) {
				usleep(time_to_sleep_us - min_waiting_time);
			}

			while (SDL_GetPerformanceCounter() < time_of_frame) {
				// nothing...
			}
	}	
	}
	int should_vsync = (gfx.vsync!=VSYNC_OFF && (gfx.vsync==VSYNC_STRICT || frame_start==0 || SDL_GetTicks()-frame_start<FRAME_BUDGET));
	PLAT_flip(screen, should_vsync);

	double elapsed_time_s = (double)(SDL_GetPerformanceCounter() - per_frame_start) / perf_freq;
	double tempfps = 1.0 / elapsed_time_s;
	
	fps_buffer[fps_buffer_index] = tempfps;
	fps_buffer_index = (fps_buffer_index + 1) % FPS_BUFFER_SIZE;
	// give it a little bit to stabilize and then use, meanwhile the buffer will
	// cover it
	if (fps_counter++ > 100) {
		double average_fps = 0.0;
		int fpsbuffersize = MIN(fps_counter, FPS_BUFFER_SIZE);
		for (int i = 0; i < fpsbuffersize; i++) {
			average_fps += fps_buffer[i];
		}
		average_fps /= fpsbuffersize;
		currentfps = current_fps = average_fps;
	}
	else {
		currentfps = current_fps = target_fps;
	}
	per_frame_start = SDL_GetPerformanceCounter();
}

void GFX_sync_fixed_rate(double target_fps) {
	if (target_fps == 0.0) target_fps = SCREEN_FPS;
	int frame_budget = (int) lrint(1000.0 / target_fps);
	uint32_t frame_duration = SDL_GetTicks() - frame_start;
	if (gfx.vsync!=VSYNC_OFF) {
		// this limiting condition helps SuperFX chip games
		if (gfx.vsync==VSYNC_STRICT || frame_start==0 || frame_duration<frame_budget) { // only wait if we're under frame budget
			PLAT_vsync(frame_budget-frame_duration);
		}
	}
	else {
		if (frame_duration<frame_budget) SDL_Delay(frame_budget-frame_duration);
	}
}
// if a fake vsycn delay is really needed 
void GFX_delay(void) {
	uint32_t frame_duration = SDL_GetTicks() - frame_start;
	if (frame_duration<((1/SCREEN_FPS) * 1000)) SDL_Delay(((1/SCREEN_FPS) * 1000)-frame_duration);
}

int GFX_truncateText(TTF_Font* font, const char* in_name, char* out_name, int max_width, int padding) {
	int text_width;
	strcpy(out_name, in_name);
	TTF_SizeUTF8(font, out_name, &text_width, NULL);
	text_width += padding;
	
	while (text_width>max_width) {
		int len = strlen(out_name);
		strcpy(&out_name[len-4], "...\0");
		TTF_SizeUTF8(font, out_name, &text_width, NULL);
		text_width += padding;
	}
	
	return text_width;
}
int GFX_wrapText(TTF_Font* font, char* str, int max_width, int max_lines) {
	if (!str) return 0;
	
	int line_width;
	int max_line_width = 0;
	char* line = str;
	char buffer[MAX_PATH];
	
	TTF_SizeUTF8(font, line, &line_width, NULL);
	if (line_width<=max_width) {
		line_width = GFX_truncateText(font,line,buffer,max_width,0);
		strcpy(line,buffer);
		return line_width;
	}
	
	char* prev = NULL;
	char* tmp = line;
	int lines = 1;
	int i = 0;
	while (!max_lines || lines<max_lines) {
		tmp = strchr(tmp, ' ');
		if (!tmp) {
			if (prev) {
				TTF_SizeUTF8(font, line, &line_width, NULL);
				if (line_width>=max_width) {
					if (line_width>max_line_width) max_line_width = line_width;
					prev[0] = '\n';
					line = prev + 1;
				}
			}
			break;
		}
		tmp[0] = '\0';
		
		TTF_SizeUTF8(font, line, &line_width, NULL);

		if (line_width>=max_width) { // wrap
			if (line_width>max_line_width) max_line_width = line_width;
			tmp[0] = ' ';
			tmp += 1;
			prev[0] = '\n';
			prev += 1;
			line = prev;
			lines += 1;
		}
		else { // continue
			tmp[0] = ' ';
			prev = tmp;
			tmp += 1;
		}
		i += 1;
	}
	
	line_width = GFX_truncateText(font,line,buffer,max_width,0);
	strcpy(line,buffer);
	
	if (line_width>max_line_width) max_line_width = line_width;
	return max_line_width;
}

///////////////////////////////

// scale_blend (and supporting logic) from picoarch

struct blend_args {
	int w_ratio_in;
	int w_ratio_out;
	uint16_t w_bp[2];
	int h_ratio_in;
	int h_ratio_out;
	uint16_t h_bp[2];
	uint16_t *blend_line;
} blend_args;

#if __ARM_ARCH >= 5
static inline uint32_t average16(uint32_t c1, uint32_t c2) {
	uint32_t ret, lowbits = 0x0821;
	asm ("eor %0, %2, %3\r\n"
	     "and %0, %0, %1\r\n"
	     "add %0, %3, %0\r\n"
	     "add %0, %0, %2\r\n"
	     "lsr %0, %0, #1\r\n"
	     : "=&r" (ret) : "r" (lowbits), "r" (c1), "r" (c2) : );
	return ret;
}
static inline uint32_t average32(uint32_t c1, uint32_t c2) {
	uint32_t ret, lowbits = 0x08210821;

	asm ("eor %0, %3, %1\r\n"
	     "and %0, %0, %2\r\n"
	     "adds %0, %1, %0\r\n"
	     "and %1, %1, #0\r\n"
	     "movcs %1, #0x80000000\r\n"
	     "adds %0, %0, %3\r\n"
	     "rrx %0, %0\r\n"
	     "orr %0, %0, %1\r\n"
	     : "=&r" (ret), "+r" (c2) : "r" (lowbits), "r" (c1) : "cc" );

	return ret;
}

#define AVERAGE16_NOCHK(c1, c2) (average16((c1), (c2)))
#define AVERAGE32_NOCHK(c1, c2) (average32((c1), (c2)))

#else

static inline uint32_t average16(uint32_t c1, uint32_t c2) {
	return (c1 + c2 + ((c1 ^ c2) & 0x0821))>>1;
}
static inline uint32_t average32(uint32_t c1, uint32_t c2) {
	uint32_t sum = c1 + c2;
	uint32_t ret = sum + ((c1 ^ c2) & 0x08210821);
	uint32_t of = ((sum < c1) | (ret < sum)) << 31;

	return (ret >> 1) | of;
}

#define AVERAGE16_NOCHK(c1, c2) (average16((c1), (c2)))
#define AVERAGE32_NOCHK(c1, c2) (average32((c1), (c2)))

#endif

#define AVERAGE16(c1, c2) ((c1) == (c2) ? (c1) : AVERAGE16_NOCHK((c1), (c2)))
#define AVERAGE16_1_3(c1, c2) ((c1) == (c2) ? (c1) : (AVERAGE16_NOCHK(AVERAGE16_NOCHK((c1), (c2)), (c2))))

#define AVERAGE32(c1, c2) ((c1) == (c2) ? (c1) : AVERAGE32_NOCHK((c1), (c2)))
#define AVERAGE32_1_3(c1, c2) ((c1) == (c2) ? (c1) : (AVERAGE32_NOCHK(AVERAGE32_NOCHK((c1), (c2)), (c2))))

static inline int gcd(int a, int b) {
	return b ? gcd(b, a % b) : a;
}

static void scaleAA(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_p) {
	int dy = 0;
	int lines = h;

	int rat_w = blend_args.w_ratio_in;
	int rat_dst_w = blend_args.w_ratio_out;
	uint16_t *bw = blend_args.w_bp;

	int rat_h = blend_args.h_ratio_in;
	int rat_dst_h = blend_args.h_ratio_out;
	uint16_t *bh = blend_args.h_bp;

	while (lines--) {
		while (dy < rat_dst_h) {
			uint16_t *dst16 = (uint16_t *)dst;
			uint16_t *pblend = (uint16_t *)blend_args.blend_line;
			int col = w;
			int dx = 0;

			uint16_t *pnext = (uint16_t *)(src + pitch);
			if (!lines)
				pnext -= (pitch / sizeof(uint16_t));

			if (dy > rat_dst_h - bh[0]) {
				pblend = pnext;
			} else if (dy <= bh[0]) {
				/* Drops const, won't get touched later though */
				pblend = (uint16_t *)src;
			} else {
				const uint32_t *src32 = (const uint32_t *)src;
				const uint32_t *pnext32 = (const uint32_t *)pnext;
				uint32_t *pblend32 = (uint32_t *)pblend;
				int count = w / 2;

				if (dy <= bh[1]) {
					const uint32_t *tmp = pnext32;
					pnext32 = src32;
					src32 = tmp;
				}

				if (dy > rat_dst_h - bh[1] || dy <= bh[1]) {
					while(count--) {
						*pblend32++ = AVERAGE32_1_3(*src32, *pnext32);
						src32++;
						pnext32++;
					}
				} else {
					while(count--) {
						*pblend32++ = AVERAGE32(*src32, *pnext32);
						src32++;
						pnext32++;
					}
				}
			}

			while (col--) {
				uint16_t a, b, out;

				a = *pblend;
				b = *(pblend+1);

				while (dx < rat_dst_w) {
					if (a == b) {
						out = a;
					} else if (dx > rat_dst_w - bw[0]) { // top quintile, bbbb
						out = b;
					} else if (dx <= bw[0]) { // last quintile, aaaa
						out = a;
					} else {
						if (dx > rat_dst_w - bw[1]) { // 2nd quintile, abbb
							a = AVERAGE16_NOCHK(a,b);
						} else if (dx <= bw[1]) { // 4th quintile, aaab
							b = AVERAGE16_NOCHK(a,b);
						}

						out = AVERAGE16_NOCHK(a, b); // also 3rd quintile, aabb
					}
					*dst16++ = out;
					dx += rat_w;
				}

				dx -= rat_dst_w;
				pblend++;
			}

			dy += rat_h;
			dst += dst_p;
		}

		dy -= rat_dst_h;
		src += pitch;
	}
}

scaler_t GFX_getAAScaler(GFX_Renderer* renderer) {
	int gcd_w, div_w, gcd_h, div_h;
	blend_args.blend_line = calloc(renderer->src_w, sizeof(uint16_t));

	gcd_w = gcd(renderer->src_w, renderer->dst_w);
	blend_args.w_ratio_in = renderer->src_w / gcd_w;
	blend_args.w_ratio_out = renderer->dst_w / gcd_w;
	
	double blend_denominator = (renderer->src_w>renderer->dst_w) ? 5 : 2.5; // TODO: these values are really only good for the nano...
	// blend_denominator = 5.0; // better for trimui
	// LOG_info("blend_denominator: %f (%i && %i)\n", blend_denominator, HAS_SKINNY_SCREEN, renderer->dst_w>renderer->src_w);
	
	div_w = round(blend_args.w_ratio_out / blend_denominator);
	blend_args.w_bp[0] = div_w;
	blend_args.w_bp[1] = blend_args.w_ratio_out >> 1;

	gcd_h = gcd(renderer->src_h, renderer->dst_h);
	blend_args.h_ratio_in = renderer->src_h / gcd_h;
	blend_args.h_ratio_out = renderer->dst_h / gcd_h;

	div_h = round(blend_args.h_ratio_out / blend_denominator);
	blend_args.h_bp[0] = div_h;
	blend_args.h_bp[1] = blend_args.h_ratio_out >> 1;
	
	return scaleAA;
}
void GFX_freeAAScaler(void) {
	if (blend_args.blend_line != NULL) {
		free(blend_args.blend_line);
		blend_args.blend_line = NULL;
	}
}

///////////////////////////////

void GFX_blitAsset(int asset, SDL_Rect* src_rect, SDL_Surface* dst, SDL_Rect* dst_rect) {
	SDL_Rect* rect = &asset_rects[asset];
	SDL_Rect adj_rect = {
		.x = rect->x,
		.y = rect->y,
		.w = rect->w,
		.h = rect->h,
	};
	if (src_rect) {
		adj_rect.x += src_rect->x;
		adj_rect.y += src_rect->y;
		adj_rect.w  = src_rect->w;
		adj_rect.h  = src_rect->h;
	}
	SDL_BlitSurface(gfx.assets, &adj_rect, dst, dst_rect);
}
void GFX_blitPill(int asset, SDL_Surface* dst, SDL_Rect* dst_rect) {
	int x = dst_rect->x;
	int y = dst_rect->y;
	int w = dst_rect->w;
	int h = dst_rect->h;

	if (h==0) h = asset_rects[asset].h;
	
	int r = h / 2;
	if (w < h) w = h;
	w -= h;
	
	GFX_blitAsset(asset, &(SDL_Rect){0,0,r,h}, dst, &(SDL_Rect){x,y});
	x += r;
	if (w>0) {
		SDL_FillRect(dst, &(SDL_Rect){x,y,w,h}, asset_rgbs[asset]);
		x += w;
	}
	GFX_blitAsset(asset, &(SDL_Rect){r,0,r,h}, dst, &(SDL_Rect){x,y});
}
void GFX_blitRect(int asset, SDL_Surface* dst, SDL_Rect* dst_rect) {
	int x = dst_rect->x;
	int y = dst_rect->y;
	int w = dst_rect->w;
	int h = dst_rect->h;
	int c = asset_rgbs[asset];
	
	SDL_Rect* rect = &asset_rects[asset];
	int d = rect->w;
	int r = d / 2;

	GFX_blitAsset(asset, &(SDL_Rect){0,0,r,r}, dst, &(SDL_Rect){x,y});
	SDL_FillRect(dst, &(SDL_Rect){x+r,y,w-d,r}, c);
	GFX_blitAsset(asset, &(SDL_Rect){r,0,r,r}, dst, &(SDL_Rect){x+w-r,y});
	SDL_FillRect(dst, &(SDL_Rect){x,y+r,w,h-d}, c);
	GFX_blitAsset(asset, &(SDL_Rect){0,r,r,r}, dst, &(SDL_Rect){x,y+h-r});
	SDL_FillRect(dst, &(SDL_Rect){x+r,y+h-r,w-d,r}, c);
	GFX_blitAsset(asset, &(SDL_Rect){r,r,r,r}, dst, &(SDL_Rect){x+w-r,y+h-r});
}
void GFX_blitBattery(SDL_Surface* dst, SDL_Rect* dst_rect) {
	// LOG_info("dst: %p\n", dst);
	
	if (!dst_rect) dst_rect = &(SDL_Rect){0,0,0,0};
	
	SDL_Rect rect = asset_rects[ASSET_BATTERY];
	int x = dst_rect->x;
	int y = dst_rect->y;
	x += (SCALE1(PILL_SIZE) - (rect.w + FIXED_SCALE)) / 2;
	y += (SCALE1(PILL_SIZE) - rect.h) / 2;
	
	if (pwr.is_charging) {
		GFX_blitAsset(ASSET_BATTERY, NULL, dst, &(SDL_Rect){x,y});
		GFX_blitAsset(ASSET_BATTERY_BOLT, NULL, dst, &(SDL_Rect){x+SCALE1(3),y+SCALE1(2)});
	}
	else {
		int percent = pwr.charge;
		GFX_blitAsset(percent<=10?ASSET_BATTERY_LOW:ASSET_BATTERY, NULL, dst, &(SDL_Rect){x,y});
		
		rect = asset_rects[ASSET_BATTERY_FILL];
		SDL_Rect clip = rect;
		clip.w *= percent;
		clip.w /= 100;
		if (clip.w<=0) return;
		clip.x = rect.w - clip.w;
		clip.y = 0;
		
		GFX_blitAsset(percent<=20?ASSET_BATTERY_FILL_LOW:ASSET_BATTERY_FILL, &clip, dst, &(SDL_Rect){x+SCALE1(3)+clip.x,y+SCALE1(2)});
	}
}
int GFX_getButtonWidth(char* hint, char* button) {
	int button_width = 0;
	int width;
	
	int special_case = !strcmp(button,BRIGHTNESS_BUTTON_LABEL); // TODO: oof
	
	if (strlen(button)==1) {
		button_width += SCALE1(BUTTON_SIZE);
	}
	else {
		button_width += SCALE1(BUTTON_SIZE) / 2;
		TTF_SizeUTF8(special_case ? font.large : font.tiny, button, &width, NULL);
		button_width += width;
	}
	button_width += SCALE1(BUTTON_MARGIN);
	
	TTF_SizeUTF8(font.small, hint, &width, NULL);
	button_width += width + SCALE1(BUTTON_MARGIN);
	return button_width;
}
void GFX_blitButton(char* hint, char*button, SDL_Surface* dst, SDL_Rect* dst_rect) {
	SDL_Surface* text;
	int ox = 0;
	
	int special_case = !strcmp(button,BRIGHTNESS_BUTTON_LABEL); // TODO: oof
	
	// button
	if (strlen(button)==1) {
		GFX_blitAsset(ASSET_BUTTON, NULL, dst, dst_rect);

		// label
		text = TTF_RenderUTF8_Blended(font.medium, button, COLOR_BUTTON_TEXT);
		SDL_BlitSurface(text, NULL, dst, &(SDL_Rect){dst_rect->x+(SCALE1(BUTTON_SIZE)-text->w)/2,dst_rect->y+(SCALE1(BUTTON_SIZE)-text->h)/2});
		ox += SCALE1(BUTTON_SIZE);
		SDL_FreeSurface(text);
	}
	else {
		text = TTF_RenderUTF8_Blended(special_case ? font.large : font.tiny, button, COLOR_BUTTON_TEXT);
		GFX_blitPill(ASSET_BUTTON, dst, &(SDL_Rect){dst_rect->x,dst_rect->y,SCALE1(BUTTON_SIZE)/2+text->w,SCALE1(BUTTON_SIZE)});
		ox += SCALE1(BUTTON_SIZE)/4;
		
		int oy = special_case ? SCALE1(-2) : 0;
		SDL_BlitSurface(text, NULL, dst, &(SDL_Rect){ox+dst_rect->x,oy+dst_rect->y+(SCALE1(BUTTON_SIZE)-text->h)/2,text->w,text->h});
		ox += text->w;
		ox += SCALE1(BUTTON_SIZE)/4;
		SDL_FreeSurface(text);
	}
	
	ox += SCALE1(BUTTON_MARGIN);

	// hint text
	text = TTF_RenderUTF8_Blended(font.small, hint, COLOR_WHITE);
	SDL_BlitSurface(text, NULL, dst, &(SDL_Rect){ox+dst_rect->x,dst_rect->y+(SCALE1(BUTTON_SIZE)-text->h)/2,text->w,text->h});
	SDL_FreeSurface(text);
}
void GFX_blitMessage(TTF_Font* font, char* msg, SDL_Surface* dst, SDL_Rect* dst_rect) {
	if (!dst_rect) dst_rect = &(SDL_Rect){0,0,dst->w,dst->h};
	
	// LOG_info("GFX_blitMessage: %p (%ix%i)", dst, dst_rect->w,dst_rect->h);
	
	SDL_Surface* text;
#define TEXT_BOX_MAX_ROWS 16
#define LINE_HEIGHT 24
	char* rows[TEXT_BOX_MAX_ROWS];
	int row_count = 0;

	char* tmp;
	rows[row_count++] = msg;
	while ((tmp=strchr(rows[row_count-1], '\n'))!=NULL) {
		if (row_count+1>=TEXT_BOX_MAX_ROWS) return; // TODO: bail
		rows[row_count++] = tmp+1;
	}
	
	int rendered_height = SCALE1(LINE_HEIGHT) * row_count;
	int y = dst_rect->y;
	y += (dst_rect->h - rendered_height) / 2;
	
	char line[256];
	for (int i=0; i<row_count; i++) {
		int len;
		if (i+1<row_count) {
			len = rows[i+1]-rows[i]-1;
			if (len) strncpy(line, rows[i], len);
			line[len] = '\0';
		}
		else {
			len = strlen(rows[i]);
			strcpy(line, rows[i]);
		}
		
		
		if (len) {
			text = TTF_RenderUTF8_Blended(font, line, COLOR_WHITE);
			int x = dst_rect->x;
			x += (dst_rect->w - text->w) / 2;
			SDL_BlitSurface(text, NULL, dst, &(SDL_Rect){x,y});
			SDL_FreeSurface(text);
		}
		y += SCALE1(LINE_HEIGHT);
	}
}

int GFX_blitHardwareGroup(SDL_Surface* dst, int show_setting, int _fancy_mode) {
	int ox;
	int oy;
	int ow = 0;
	
	int setting_value;
	int setting_min;
	int setting_max;
	
	if (show_setting && !GetHDMI()) {
		ow = SCALE1(PILL_SIZE + SETTINGS_WIDTH + 10 + 4);
		ox = dst->w - SCALE1((PADDING - (PADDING*_fancy_mode))) - ow;
		oy = SCALE1((PADDING - (PADDING*_fancy_mode)));
		GFX_blitPill(gfx.mode==MODE_MAIN ? ASSET_DARK_GRAY_PILL : ASSET_BLACK_PILL, dst, &(SDL_Rect){
			ox,
			oy,
			ow,
			SCALE1(PILL_SIZE)
		});
		
		if (show_setting==1) {
			setting_value = GetBrightness();
			setting_min = BRIGHTNESS_MIN;
			setting_max = BRIGHTNESS_MAX;
		}
		else {
			setting_value = GetVolume();
			setting_min = VOLUME_MIN;
			setting_max = VOLUME_MAX;
		}
		
		int asset = show_setting==1?ASSET_BRIGHTNESS:(setting_value>0?ASSET_VOLUME:ASSET_VOLUME_MUTE);
		int ax = ox + (show_setting==1 ? SCALE1(6) : SCALE1(8));
		int ay = oy + (show_setting==1 ? SCALE1(5) : SCALE1(7));
		GFX_blitAsset(asset, NULL, dst, &(SDL_Rect){ax,ay});
		
		ox += SCALE1(PILL_SIZE);
		oy += SCALE1((PILL_SIZE - SETTINGS_SIZE) / 2);
		GFX_blitPill(gfx.mode==MODE_MAIN ? ASSET_BAR_BG : ASSET_BAR_BG_MENU, dst, &(SDL_Rect){
			ox,
			oy,
			SCALE1(SETTINGS_WIDTH),
			SCALE1(SETTINGS_SIZE)
		});
		
		float percent = ((float)(setting_value-setting_min) / (setting_max-setting_min));
		if (show_setting==1 || setting_value>0) {
			GFX_blitPill(ASSET_BAR, dst, &(SDL_Rect){
				ox,
				oy,
				SCALE1(SETTINGS_WIDTH) * percent,
				SCALE1(SETTINGS_SIZE)
			});
		}
	}
	else {
		// TODO: handle wifi
		int show_wifi = PLAT_isOnline(); // NOOOOO! not every frame!

		int ww = SCALE1(PILL_SIZE-3);
		ow = SCALE1(PILL_SIZE);
		if (show_wifi) ow += ww;

		ox = dst->w - SCALE1((PADDING - (PADDING*_fancy_mode))) - ow;
		oy = SCALE1((PADDING - (PADDING*_fancy_mode)));
		GFX_blitPill(gfx.mode==MODE_MAIN ? ASSET_DARK_GRAY_PILL : ASSET_BLACK_PILL, dst, &(SDL_Rect){
			ox,
			oy,
			ow,
			SCALE1(PILL_SIZE)
		});
		if (show_wifi) {
			SDL_Rect rect = asset_rects[ASSET_WIFI];
			int x = ox;
			int y = oy;
			x += (SCALE1(PILL_SIZE) - rect.w) / 2;
			y += (SCALE1(PILL_SIZE) - rect.h) / 2;
			
			GFX_blitAsset(ASSET_WIFI, NULL, dst, &(SDL_Rect){x,y});
			ox += ww;
		}
		GFX_blitBattery(dst, &(SDL_Rect){ox,oy});
	}
	
	return ow;
}
void GFX_blitHardwareHints(SDL_Surface* dst, int show_setting, int _fancy_mode) {
	if (BTN_MOD_VOLUME==BTN_SELECT && BTN_MOD_BRIGHTNESS==BTN_START) {
		if (show_setting==1) GFX_blitButtonGroup((char*[]){ "SELECT","VOLUME",  NULL }, 0, dst, 0, _fancy_mode);
		else GFX_blitButtonGroup((char*[]){ "START","BRIGHTNESS",  NULL }, 0, dst, 0, _fancy_mode);
	}
	else {
		if (show_setting==1) GFX_blitButtonGroup((char*[]){ BRIGHTNESS_BUTTON_LABEL,"BRIGHTNESS",  NULL }, 0, dst, 0, _fancy_mode);
		else GFX_blitButtonGroup((char*[]){ "MENU","BRIGHTNESS",  NULL }, 0, dst, 0, _fancy_mode);
	}
	
}

int GFX_blitButtonGroup(char** pairs, int primary, SDL_Surface* dst, int align_right, int _fancy_mode) {
	int ox;
	int oy;
	int ow;
	char* hint;
	char* button;

	struct Hint {
		char* hint;
		char* button;
		int ow;
	} hints[2]; 
	int w = 0; // individual button dimension
	int h = 0; // hints index
	ow = 0; // full pill width
	ox = align_right ? dst->w - SCALE1((PADDING - (PADDING*_fancy_mode))) : SCALE1((PADDING - (PADDING*_fancy_mode)));
	oy = dst->h - SCALE1((PADDING - (PADDING*_fancy_mode)) + PILL_SIZE);
	
	for (int i=0; i<2; i++) {
		if (!pairs[i*2]) break;
		if (HAS_SKINNY_SCREEN && i!=primary) continue; // space saving
		
		button = pairs[i * 2];
		hint = pairs[i * 2 + 1];
		w = GFX_getButtonWidth(hint, button);
		hints[h].hint = hint;
		hints[h].button = button;
		hints[h].ow = w;
		h += 1;
		ow += SCALE1(BUTTON_MARGIN) + w;
	}
	
	ow += SCALE1(BUTTON_MARGIN);
	if (align_right) ox -= ow;
	GFX_blitPill(gfx.mode==MODE_MAIN ? ASSET_DARK_GRAY_PILL : ASSET_BLACK_PILL, dst, &(SDL_Rect){
		ox,
		oy,
		ow,
		SCALE1(PILL_SIZE)
	});
	
	ox += SCALE1(BUTTON_MARGIN);
	oy += SCALE1(BUTTON_MARGIN);
	for (int i=0; i<h; i++) {
		GFX_blitButton(hints[i].hint, hints[i].button, dst, &(SDL_Rect){ox,oy});
		ox += hints[i].ow + SCALE1(BUTTON_MARGIN);
	}
	return ow;
}

#define MAX_TEXT_LINES 16
void GFX_sizeText(TTF_Font* font, char* str, int leading, int* w, int* h) {
	char* lines[MAX_TEXT_LINES];
	int count = 0;

	char* tmp;
	lines[count++] = str;
	while ((tmp=strchr(lines[count-1], '\n'))!=NULL) {
		if (count+1>MAX_TEXT_LINES) break; // TODO: bail?
		lines[count++] = tmp+1;
	}
	*h = count * leading;
	
	int mw = 0;
	char line[256];
	for (int i=0; i<count; i++) {
		int len;
		if (i+1<count) {
			len = lines[i+1]-lines[i]-1;
			if (len) strncpy(line, lines[i], len);
			line[len] = '\0';
		}
		else {
			len = strlen(lines[i]);
			strcpy(line, lines[i]);
		}
		
		if (len) {
			int lw;
			TTF_SizeUTF8(font, line, &lw, NULL);
			if (lw>mw) mw = lw;
		}
	}
	*w = mw;
}
void GFX_blitText(TTF_Font* font, char* str, int leading, SDL_Color color, SDL_Surface* dst, SDL_Rect* dst_rect) {
	if (dst_rect==NULL) dst_rect = &(SDL_Rect){0,0,dst->w,dst->h};
	
	char* lines[MAX_TEXT_LINES];
	int count = 0;

	char* tmp;
	lines[count++] = str;
	while ((tmp=strchr(lines[count-1], '\n'))!=NULL) {
		if (count+1>MAX_TEXT_LINES) break; // TODO: bail?
		lines[count++] = tmp+1;
	}
	int x = dst_rect->x;
	int y = dst_rect->y;
	
	SDL_Surface* text;
	char line[256];
	for (int i=0; i<count; i++) {
		int len;
		if (i+1<count) {
			len = lines[i+1]-lines[i]-1;
			if (len) strncpy(line, lines[i], len);
			line[len] = '\0';
		}
		else {
			len = strlen(lines[i]);
			strcpy(line, lines[i]);
		}
		
		if (len) {
			text = TTF_RenderUTF8_Blended(font, line, color);
			SDL_BlitSurface(text, NULL, dst, &(SDL_Rect){x+((dst_rect->w-text->w)/2),y+(i*leading)});
			SDL_FreeSurface(text);
		}
	}
}

///////////////////////////////

// based on picoarch's audio 
// implementation, rewritten 
// to (try to) understand it 
// better

#define MAX_SAMPLE_RATE 48000
#define BATCH_SIZE 100
#ifndef SAMPLES
	#define SAMPLES 512 // default
#endif

#define ms SDL_GetTicks



pthread_mutex_t audio_mutex = PTHREAD_MUTEX_INITIALIZER;

static void SND_audioCallback(void *userdata, uint8_t *stream, int len) {
	if (snd.frame_count == 0)
		return;

	int16_t *out = (int16_t *)stream;
	len /= (sizeof(int16_t) * 2);
	
	while (snd.frame_out!=snd.frame_in && len>0) {
		*out++ = snd.buffer[snd.frame_out].left;
		*out++ = snd.buffer[snd.frame_out].right;
		
		snd.frame_filled = snd.frame_out;
		
		snd.frame_out += 1;
		len -= 1;
		
		if (snd.frame_out>=snd.frame_count) snd.frame_out = 0;
	}
	
	while (len>0) {
		*out++ = 0;
		*out++ = 0;
		len -= 1;
	}
}

static void SND_resizeBuffer(void) { // plat_sound_resize_buffer
	//if nofix snd.buffer_seconds must be 0
	if (use_nofix == 1){
		snd.frame_count = snd.buffer_seconds * snd.sample_rate_in / snd.frame_rate;
	}
	if (snd.frame_count == 0)
		return;

	SDL_LockAudio();

	int buffer_bytes = snd.frame_count * sizeof(SND_Frame);
	snd.buffer = (SND_Frame*)realloc(snd.buffer, buffer_bytes);

	memset(snd.buffer, 0, buffer_bytes);

	snd.frame_in = 0;
	snd.frame_out = 0;

	if (use_nofix == 1) {
		snd.frame_filled = snd.frame_count - 1;
	}

	SDL_UnlockAudio();
}

static int SND_resampleNone(SND_Frame frame) { // audio_resample_passthrough
	snd.buffer[snd.frame_in++] = frame;
	if (snd.frame_in >= snd.frame_count) snd.frame_in = 0;
	return 1;
}
static int SND_resampleNear(SND_Frame frame) { // audio_resample_nearest
	static int diff = 0;
	int consumed = 0;

	if (diff < snd.sample_rate_out) {
		snd.buffer[snd.frame_in++] = frame;
		if (snd.frame_in >= snd.frame_count) snd.frame_in = 0;
		diff += snd.sample_rate_in;
	}

	if (diff >= snd.sample_rate_out) {
		consumed++;
		diff -= snd.sample_rate_out;
	}

	return consumed;
}

static void SND_selectResampler(void) { // plat_sound_select_resampler
	if (snd.sample_rate_in==snd.sample_rate_out) {
		snd.resample =  SND_resampleNone;
	}
	else {
		snd.resample = SND_resampleNear;
	}
}

static int soundQuality = 2;
static int resetSrcState = 0;
void SND_setQuality(int quality) {
	LOG_info("Set sound quality\n\n\n");
	soundQuality = qualityLevels[quality];
	resetSrcState = 1;
}
ResampledFrames resample_audio(const SND_Frame *input_frames,
	int input_frame_count, int input_sample_rate,
	int output_sample_rate, double ratio) {

	int error;
	static double previous_ratio = 1.0;
	static SRC_STATE *src_state = NULL;

	double final_ratio = ((double)output_sample_rate / input_sample_rate) * ratio;

	if (!src_state || resetSrcState) {
		resetSrcState = 0;
		src_state = src_new(soundQuality, 2, &error);
		if (src_state == NULL) {
			fprintf(stderr, "Error initializing SRC state: %s\n",
				src_strerror(error));
			exit(1);
		}
	}

	if (previous_ratio != final_ratio) {
		if (src_set_ratio(src_state, final_ratio) != 0) {
			fprintf(stderr, "Error setting resampling ratio: %s\n",
				src_strerror(src_error(src_state)));
			exit(1);
		}
		previous_ratio = final_ratio;
	}

	int max_output_frames = (int)(input_frame_count * final_ratio + 1);

	float *input_buffer = (float*)malloc(input_frame_count * 2 * sizeof(float));
	float *output_buffer = (float*)malloc(max_output_frames * 2 * sizeof(float));
	if (!input_buffer || !output_buffer) {
		fprintf(stderr, "Error allocating buffers\n");
		free(input_buffer);
		free(output_buffer);
		src_delete(src_state);
		exit(1);
	}

	for (int i = 0; i < input_frame_count; i++) {
		input_buffer[2 * i] = input_frames[i].left / 32768.0f;
		input_buffer[2 * i + 1] = input_frames[i].right / 32768.0f;
	}

	SRC_DATA src_data = {
		.data_in = input_buffer,
		.data_out = output_buffer,
		.input_frames = input_frame_count,
		.output_frames = max_output_frames,
		.src_ratio = final_ratio,
		.end_of_input = 0
	};

	if (src_process(src_state, &src_data) != 0) {
		fprintf(stderr, "Error resampling: %s\n",
			src_strerror(src_error(src_state)));
		free(input_buffer);
		free(output_buffer);
		exit(1);
	}

	int output_frame_count = src_data.output_frames_gen;

	SND_Frame *output_frames = (SND_Frame*)malloc(output_frame_count * sizeof(SND_Frame));
	if (!output_frames) {
		fprintf(stderr, "Error allocating output frames\n");
		free(input_buffer);
		free(output_buffer);
		exit(1);
	}

	for (int i = 0; i < output_frame_count; i++) {
		float left = output_buffer[2 * i];
		float right = output_buffer[2 * i + 1];

		left = fmaxf(-1.0f, fminf(1.0f, left));
		right = fmaxf(-1.0f, fminf(1.0f, right));

		output_frames[i].left = (int16_t)(left * 32767.0f);
		output_frames[i].right = (int16_t)(right * 32767.0f);
	}

	free(input_buffer);
	free(output_buffer);

	ResampledFrames resampled;
	resampled.frames = output_frames;
	resampled.frame_count = output_frame_count;

	return resampled;
}


#define ROLLING_AVERAGE_WINDOW_SIZE 5
static float adjustment_history[ROLLING_AVERAGE_WINDOW_SIZE] = {0.0f};
static int adjustment_index = 0;

float calculateBufferAdjustment(float remaining_space, float targetbuffer_over, float targetbuffer_under, int batchsize) {

    float midpoint = (targetbuffer_over + targetbuffer_under) / 2.0f;

    float normalizedDistance;
    if (remaining_space < midpoint) {
        normalizedDistance = (midpoint - remaining_space) / (midpoint - targetbuffer_over);
    } else {
        normalizedDistance = (remaining_space - midpoint) / (targetbuffer_under - midpoint);
    }
	// I make crazy small adjustments, mooore tiny is mooore stable :D But don't come neir the limits cuz imma hit ya with that 0.005 ratio adjustment, pow pow!
    // I wonder if staying in the middle of 0 to 4000 with 512 samples per batch playing at tiny different speeds each iteration is like the smallest I can get
	// lets say hovering around 2000 means 2000 samples queue, about 4 frames, so at 17ms(60fps) thats  68ms delay right?
	// Should have payed attention when my math teacher was talking dammit
	// Also I chose 3 for pow, but idk if that really the best nr, anyone good in maths looking at my code?
	float adjustment = 0.000001f + (0.005f - 0.000001f) * pow(normalizedDistance, 3);

    if (remaining_space < midpoint) {
        adjustment = -adjustment;
    }

    adjustment_history[adjustment_index] = adjustment;
    adjustment_index = (adjustment_index + 1) % ROLLING_AVERAGE_WINDOW_SIZE;

    // Calculate the rolling average
    float rolling_average = 0.0f;
    for (int i = 0; i < ROLLING_AVERAGE_WINDOW_SIZE; ++i) {
        rolling_average += adjustment_history[i];
    }
    rolling_average /= ROLLING_AVERAGE_WINDOW_SIZE;

    return rolling_average;
}




static SND_Frame tmpbuffer[BATCH_SIZE];
static SND_Frame *unwritten_frames = NULL;
static int unwritten_frame_count = 0;

float currentratio = 0.0;
int currentbufferfree = 0;
int currentframecount = 0;
static double ratio = 1.0;

size_t SND_batchSamplesNoFix(const SND_Frame* frames, size_t frame_count) { // plat_sound_write / plat_sound_write_resample
	if (snd.frame_count==0) return 0;
	
	SDL_LockAudio();
	int progress = 0;
	int consumed = 0;
	while (frame_count > 0) {
		int tries = 0;
		int amount = MIN(BATCH_SIZE_NOFIX, frame_count);

		while (tries < 10 && snd.frame_in==snd.frame_filled) {
			tries++;
			SDL_UnlockAudio();
			SDL_Delay(1);
			SDL_LockAudio();
		}

		while (amount && snd.frame_in != snd.frame_filled) {
			consumed = snd.resample(*frames);
			frames += consumed;
			amount -= consumed;
			frame_count -= consumed;
			progress += consumed;
		}
	}
	SDL_UnlockAudio();
	return progress;
}

size_t SND_batchSamples(const SND_Frame *frames, size_t frame_count) {
	
	int framecount = (int)frame_count;

	int consumed = 0;
	int total_consumed_frames = 0;

	float remaining_space=snd.frame_count;
	if (snd.frame_in >= snd.frame_out) {
		remaining_space = snd.frame_count - (snd.frame_in - snd.frame_out);
	}
	else {
		remaining_space = snd.frame_out - snd.frame_in;
	}
	currentbufferfree = remaining_space;

	float tempdelay = ((snd.frame_count - remaining_space) / snd.sample_rate_out) * 1000;

	currentbufferms = tempdelay;

	float tempratio = 1;
	// i use 0.4* as minimum free space because i want my algorithm to fight more for free buffer then full, cause you know free buffer is lower latency :D
	// My algorithm is fighting here with audio hardware. 
	// It's like a person is trying to balance on a rope (my algorithm) and another person (the audio hardware and screen) is wiggling the rope and the balancing person got to keep countering and try to stay stable
	float bufferadjustment = calculateBufferAdjustment(remaining_space, snd.frame_count*0.4, snd.frame_count,frame_count);
	ratio = (tempratio * (snd.frame_rate / current_fps)) + bufferadjustment;
	// printf("%s: ratio=%g, tempratio=%g, snd.frame_rate=%g, current_fps=%g, bufferadjustment=%g\n", __FUNCTION__, ratio, tempratio, snd.frame_rate, current_fps, bufferadjustment);

	currentratio = ratio;

	if(ratio > 1.5) 
		ratio = 1.5;
	if(ratio < 0.5)
		ratio = 0.5;

	while (framecount > 0) {
		
		int amount = MIN(BATCH_SIZE, framecount);

		for (int i = 0; i < amount; i++) {
			tmpbuffer[i] = frames[consumed + i];
		}
		consumed += amount;
		framecount -= amount;

		ResampledFrames resampled = resample_audio(
			tmpbuffer, amount, snd.sample_rate_in, snd.sample_rate_out, ratio);

		// Write resampled frames to the buffer
		int written_frames = 0;
		
		for (int i = 0; i < resampled.frame_count; i++) {
			if ((snd.frame_in + 1) % snd.frame_count == snd.frame_out) {
				// Buffer is full, break. This should never happen tho, but just to be save
				break;
			}
			pthread_mutex_lock(&audio_mutex);
			snd.buffer[snd.frame_in] = resampled.frames[i];
			snd.frame_in = (snd.frame_in + 1) % snd.frame_count;
			pthread_mutex_unlock(&audio_mutex);
			written_frames++;
			
		}
		
		total_consumed_frames += written_frames;
		free(resampled.frames);
	}

	return total_consumed_frames;
}

enum {
	SND_FF_ON_TIME,
	SND_FF_LATE,
	SND_FF_VERY_LATE
};

size_t SND_batchSamples_fixed_rate(const SND_Frame *frames, size_t frame_count) {
	static int current_mode = SND_FF_ON_TIME;

	int framecount = (int)frame_count;

	int consumed = 0;
	int total_consumed_frames = 0;

	//printf("received %d audio frames\n", frame_count);

	//int full = 0;

	float remaining_space=snd.frame_count;
	if (snd.frame_in >= snd.frame_out) {
		remaining_space = snd.frame_count - (snd.frame_in - snd.frame_out);
	}
	else {
		remaining_space = snd.frame_out - snd.frame_in;
	}
	//printf("    actual free: %g\n", remaining_space);
	currentbufferfree = remaining_space;
	float tempdelay = ((snd.frame_count - remaining_space) / snd.sample_rate_out) * 1000;
	currentbufferms = tempdelay;

	float occupancy = (float) (snd.frame_count - currentbufferfree) / snd.frame_count;
	switch(current_mode) {
		case SND_FF_ON_TIME:
			if (occupancy > 0.65) {
				current_mode = SND_FF_LATE;
			}
			break;
		case SND_FF_LATE:
			if (occupancy > 0.85) {
				current_mode = SND_FF_VERY_LATE;
			}
			else if (occupancy < 0.25) {
				current_mode = SND_FF_ON_TIME;
			}
			break;
		case SND_FF_VERY_LATE:
			if (occupancy < 0.50) {
				current_mode = SND_FF_LATE;
			}
			break;
	}

	switch(current_mode) {
		case SND_FF_ON_TIME:   ratio = 1.0; break;
		case SND_FF_LATE:      ratio = 0.995; break;
		case SND_FF_VERY_LATE: ratio = 0.980; break;
		default: ratio = 1.0;
	}
	currentratio = ratio;

	while (framecount > 0) {
		
		int amount = MIN(BATCH_SIZE, framecount);

		for (int i = 0; i < amount; i++) {
			tmpbuffer[i] = frames[consumed + i];
		}
		consumed += amount;
		framecount -= amount;

		ResampledFrames resampled = resample_audio(
			tmpbuffer, amount, snd.sample_rate_in, snd.sample_rate_out, ratio);

		// Write resampled frames to the buffer
		int written_frames = 0;
		
		for (int i = 0; i < resampled.frame_count; i++) {
			if ((snd.frame_in + 1) % snd.frame_count == snd.frame_out) {
				// Buffer is full, break. This should never happen tho, but just to be safe
				break;
			}
			pthread_mutex_lock(&audio_mutex);
			snd.buffer[snd.frame_in] = resampled.frames[i];
			snd.frame_in = (snd.frame_in + 1) % snd.frame_count;
			pthread_mutex_unlock(&audio_mutex);
			written_frames++;
			
		}
		
		total_consumed_frames += written_frames;
		free(resampled.frames);
	}

	return total_consumed_frames;
}

void SND_init(double sample_rate, double frame_rate) { // plat_sound_init
	LOG_info("SND_init\n");
	currentreqfps = frame_rate;
	if (SCREEN_FPS != frame_rate) {
		LOG_info("SND_init: target fps %g != %g\n", SCREEN_FPS, frame_rate);
		//SCREEN_FPS = frame_rate;
	}
	SDL_InitSubSystem(SDL_INIT_AUDIO);

	fps_counter = 0;
	fps_buffer_index = 0;

#if defined(USE_SDL2)
	LOG_info("Available audio drivers:\n");
	for (int i=0; i<SDL_GetNumAudioDrivers(); i++) {
		LOG_info("- %s\n", SDL_GetAudioDriver(i));
	}
	LOG_info("Current audio driver: %s\n", SDL_GetCurrentAudioDriver());
#endif	
	
	memset(&snd, 0, sizeof(struct SND_Context));
	snd.frame_rate = frame_rate;

	SDL_AudioSpec spec_in;
	SDL_AudioSpec spec_out;

	spec_in.freq = PLAT_pickSampleRate(sample_rate, MAX_SAMPLE_RATE);
	spec_in.format = AUDIO_S16;
	spec_in.channels = 2;
	spec_in.samples = SAMPLES;
	spec_in.callback = SND_audioCallback;
	
	if (SDL_OpenAudio(&spec_in, &spec_out)<0) LOG_info("SDL_OpenAudio error: %s\n", SDL_GetError());
	
	snd.frame_count = ((float)spec_out.freq/SCREEN_FPS)*6; // buffer size based on sample rate out (with 6 frames headroom), ideally you want to use actual FPS but don't know it at this point yet 
	currentbuffersize = snd.frame_count;
	snd.sample_rate_in  = sample_rate;
	snd.sample_rate_out = spec_out.freq;
	currentsampleratein = snd.sample_rate_in;
	currentsamplerateout = snd.sample_rate_out;
	
	snd.buffer_seconds = 5;
	SND_selectResampler();
	
	SND_resizeBuffer();
	SDL_PauseAudio(0);

	LOG_info("sample rate: %i (req) %i (rec) [samples %i]\n", snd.sample_rate_in, snd.sample_rate_out, SAMPLES);
	snd.initialized = 1;
}
void SND_quit(void) { // plat_sound_finish
	if (!snd.initialized) return;
	
	SDL_PauseAudio(1);
	SDL_CloseAudio();
	
	if (snd.buffer) {
		free(snd.buffer);
		snd.buffer = NULL;
	}
}

void SND_resetAudio(double sample_rate, double frame_rate) {
	LOG_info("SND_resetAudio\n");
	SND_quit();
	SND_init(sample_rate, frame_rate);
}


///////////////////////////////

PAD_Context pad;

#define AXIS_DEADZONE 0x4000
void PAD_setAnalog(int neg_id,int pos_id,int value,int repeat_at) {
	int neg = 1 << neg_id;
	int pos = 1 << pos_id;	
	if (value>AXIS_DEADZONE) { // pressing
		if (!(pad.is_pressed&pos)) { // not pressing
			pad.is_pressed 		|= pos; // set
			pad.just_pressed	|= pos; // set
			pad.just_repeated	|= pos; // set
			pad.repeat_at[pos_id]= repeat_at;

			if (pad.is_pressed&neg) { // was pressing opposite
				pad.is_pressed 		&= ~neg; // unset
				pad.just_repeated 	&= ~neg; // unset
				pad.just_released	|=  neg; // set
			}
		}
	}
	else if (value<-AXIS_DEADZONE) { // pressing
		if (!(pad.is_pressed&neg)) { // not pressing
			pad.is_pressed		|= neg; // set
			pad.just_pressed	|= neg; // set
			pad.just_repeated	|= neg; // set
			pad.repeat_at[neg_id]= repeat_at;

			if (pad.is_pressed&pos) { // was pressing opposite
				pad.is_pressed 		&= ~pos; // unset
				pad.just_repeated 	&= ~pos; // unset
				pad.just_released	|=  pos; // set
			}
		}
	}
	else { // not pressing
		if (pad.is_pressed&neg) { // was pressing
			pad.is_pressed 		&= ~neg; // unset
			pad.just_repeated	&=  neg; // unset
			pad.just_released	|=  neg; // set
		}
		if (pad.is_pressed&pos) { // was pressing
			pad.is_pressed 		&= ~pos; // unset
			pad.just_repeated	&=  pos; // unset
			pad.just_released	|=  pos; // set
		}
	}
}

void PAD_reset(void) {
	pad.just_pressed = BTN_NONE;
	pad.is_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;
}
void PAD_poll_SDL(void) {
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
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		int btn = BTN_NONE;
		int pressed = 0; // 0=up,1=down
		int id = -1;
		if (event.type==SDL_KEYDOWN || event.type==SDL_KEYUP) {
			uint8_t code = event.key.keysym.scancode;
			pressed = event.type==SDL_KEYDOWN;
			// LOG_info("key event: %i (%i)\n", code,pressed);
				 if (code==CODE_UP) 		{ btn = BTN_DPAD_UP; 		id = BTN_ID_DPAD_UP; }
 			else if (code==CODE_DOWN)		{ btn = BTN_DPAD_DOWN; 		id = BTN_ID_DPAD_DOWN; }
			else if (code==CODE_LEFT)		{ btn = BTN_DPAD_LEFT; 		id = BTN_ID_DPAD_LEFT; }
			else if (code==CODE_RIGHT)		{ btn = BTN_DPAD_RIGHT; 	id = BTN_ID_DPAD_RIGHT; }
			else if (code==CODE_A)			{ btn = BTN_A; 				id = BTN_ID_A; }
			else if (code==CODE_B)			{ btn = BTN_B; 				id = BTN_ID_B; }
			else if (code==CODE_X)			{ btn = BTN_X; 				id = BTN_ID_X; }
			else if (code==CODE_Y)			{ btn = BTN_Y; 				id = BTN_ID_Y; }
			else if (code==CODE_START)		{ btn = BTN_START; 			id = BTN_ID_START; }
			else if (code==CODE_SELECT)		{ btn = BTN_SELECT; 		id = BTN_ID_SELECT; }
			else if (code==CODE_MENU)		{ btn = BTN_MENU; 			id = BTN_ID_MENU; }
			else if (code==CODE_MENU_ALT)	{ btn = BTN_MENU; 			id = BTN_ID_MENU; }
			else if (code==CODE_L1)			{ btn = BTN_L1; 			id = BTN_ID_L1; }
			else if (code==CODE_L2)			{ btn = BTN_L2; 			id = BTN_ID_L2; }
			else if (code==CODE_R1)			{ btn = BTN_R1; 			id = BTN_ID_R1; }
			else if (code==CODE_R2)			{ btn = BTN_R2; 			id = BTN_ID_R2; }
			else if (code==CODE_PLUS)		{ btn = BTN_PLUS; 			id = BTN_ID_PLUS; }
			else if (code==CODE_MINUS)		{ btn = BTN_MINUS; 			id = BTN_ID_MINUS; }
			else if (code==CODE_POWER)		{ btn = BTN_POWER; 			id = BTN_ID_POWER; }
			else if (code==CODE_POWEROFF)	{ btn = BTN_POWEROFF;		id = BTN_ID_POWEROFF; } // nano-only
		}
		else if (event.type==SDL_JOYBUTTONDOWN || event.type==SDL_JOYBUTTONUP) {
			uint8_t joy = event.jbutton.button;
			pressed = event.type==SDL_JOYBUTTONDOWN;
			// LOG_info("joy event: %i (%i)\n", joy,pressed);
				 if (joy==JOY_UP) 		{ btn = BTN_DPAD_UP; 		id = BTN_ID_DPAD_UP; }
 			else if (joy==JOY_DOWN)		{ btn = BTN_DPAD_DOWN; 		id = BTN_ID_DPAD_DOWN; }
			else if (joy==JOY_LEFT)		{ btn = BTN_DPAD_LEFT; 		id = BTN_ID_DPAD_LEFT; }
			else if (joy==JOY_RIGHT)	{ btn = BTN_DPAD_RIGHT; 	id = BTN_ID_DPAD_RIGHT; }
			else if (joy==JOY_A)		{ btn = BTN_A; 				id = BTN_ID_A; }
			else if (joy==JOY_B)		{ btn = BTN_B; 				id = BTN_ID_B; }
			else if (joy==JOY_X)		{ btn = BTN_X; 				id = BTN_ID_X; }
			else if (joy==JOY_Y)		{ btn = BTN_Y; 				id = BTN_ID_Y; }
			else if (joy==JOY_START)	{ btn = BTN_START; 			id = BTN_ID_START; }
			else if (joy==JOY_SELECT)	{ btn = BTN_SELECT; 		id = BTN_ID_SELECT; }
			else if (joy==JOY_MENU)		{ btn = BTN_MENU; 			id = BTN_ID_MENU; }
			else if (joy==JOY_MENU_ALT) { btn = BTN_MENU; 			id = BTN_ID_MENU; }
			else if (joy==JOY_L1)		{ btn = BTN_L1; 			id = BTN_ID_L1; }
			else if (joy==JOY_L2)		{ btn = BTN_L2; 			id = BTN_ID_L2; }
			else if (joy==JOY_R1)		{ btn = BTN_R1; 			id = BTN_ID_R1; }
			else if (joy==JOY_R2)		{ btn = BTN_R2; 			id = BTN_ID_R2; }
			else if (joy==JOY_PLUS)		{ btn = BTN_PLUS; 			id = BTN_ID_PLUS; }
			else if (joy==JOY_MINUS)	{ btn = BTN_MINUS; 			id = BTN_ID_MINUS; }
			else if (joy==JOY_POWER)	{ btn = BTN_POWER; 			id = BTN_ID_POWER; }
		}
		else if (event.type==SDL_JOYHATMOTION) {
			int hats[4] = {-1,-1,-1,-1}; // -1=no change,0=up,1=down,2=left,3=right btn_ids
			int hat = event.jhat.value;
			// LOG_info("hat event: %i\n", hat);
			// TODO: safe to assume hats will always be the primary dpad?
			// TODO: this is literally a bitmask, make it one (oh, except there's 3 states...)
			switch (hat) {
				case SDL_HAT_UP:			hats[0]=1;	  hats[1]=0;	hats[2]=0;	  hats[3]=0;	break;
				case SDL_HAT_DOWN:			hats[0]=0;	  hats[1]=1;	hats[2]=0;	  hats[3]=0;	break;
				case SDL_HAT_LEFT:			hats[0]=0;	  hats[1]=0;	hats[2]=1;	  hats[3]=0;	break;
				case SDL_HAT_RIGHT:			hats[0]=0;	  hats[1]=0;	hats[2]=0;	  hats[3]=1;	break;
				case SDL_HAT_LEFTUP:		hats[0]=1;	  hats[1]=0;	hats[2]=1;	  hats[3]=0;	break;
				case SDL_HAT_LEFTDOWN:		hats[0]=0;	  hats[1]=1;	hats[2]=1;	  hats[3]=0;	break;
				case SDL_HAT_RIGHTUP:		hats[0]=1;	  hats[1]=0;	hats[2]=0;	  hats[3]=1;	break;
				case SDL_HAT_RIGHTDOWN:		hats[0]=0;	  hats[1]=1;	hats[2]=0;	  hats[3]=1;	break;
				case SDL_HAT_CENTERED:		hats[0]=0;	  hats[1]=0;	hats[2]=0;	  hats[3]=0;	break;
				default: break;
			}
			
			for (id=0; id<4; id++) {
				int state = hats[id];
				btn = 1 << id;
				if (state==0) {
					pad.is_pressed		&= ~btn; // unset
					pad.just_repeated	&= ~btn; // unset
					pad.just_released	|= btn; // set
				}
				else if (state==1 && (pad.is_pressed & btn)==BTN_NONE) {
					pad.just_pressed	|= btn; // set
					pad.just_repeated	|= btn; // set
					pad.is_pressed		|= btn; // set
					pad.repeat_at[id]	= tick + PAD_REPEAT_DELAY;
				}
			}
			btn = BTN_NONE; // already handled, force continue
		}
		else if (event.type==SDL_JOYAXISMOTION) {
			int axis = event.jaxis.axis;
			int val = event.jaxis.value;
			// LOG_info("axis: %i (%i)\n", axis,val);
			
			// triggers on tg5040
			if (axis==AXIS_L2) {
				btn = BTN_L2;
				id = BTN_ID_L2;
				pressed = val>0;
			}
			else if (axis==AXIS_R2) {
				btn = BTN_R2;
				id = BTN_ID_R2;
				pressed = val>0;
			}
			
			else if (axis==AXIS_LX) { pad.laxis.x = val; PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, val, tick+PAD_REPEAT_DELAY); }
			else if (axis==AXIS_LY) { pad.laxis.y = val; PAD_setAnalog(BTN_ID_ANALOG_UP,   BTN_ID_ANALOG_DOWN,  val, tick+PAD_REPEAT_DELAY); }
			else if (axis==AXIS_RX) pad.raxis.x = val;
			else if (axis==AXIS_RY) pad.raxis.y = val;

			// axis will fire off what looks like a release
			// before the first press but you can't release
			// a button that wasn't pressed
			if (!pressed && btn!=BTN_NONE && !(pad.is_pressed & btn)) {
				// LOG_info("cancel: %i\n", axis);
				btn = BTN_NONE;
			}
		}
		else if (event.type==SDL_QUIT) PWR_powerOff();
		
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
int PAD_wake_SDL(void) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type==SDL_KEYUP) {
			uint8_t code = event.key.keysym.scancode;
			if ((BTN_WAKE==BTN_POWER && code==CODE_POWER) || (BTN_WAKE==BTN_MENU && (code==CODE_MENU || code==CODE_MENU_ALT))) {
				return 1;
			}
		}
		else if (event.type==SDL_JOYBUTTONUP) {
			uint8_t joy = event.jbutton.button;
			if ((BTN_WAKE==BTN_POWER && joy==JOY_POWER) || (BTN_WAKE==BTN_MENU && (joy==JOY_MENU || joy==JOY_MENU_ALT))) {
				return 1;
			}
		} 
	}
	return 0;
}

int PAD_anyJustPressed(void)	{ return pad.just_pressed!=BTN_NONE; }
int PAD_anyPressed(void)		{ return pad.is_pressed!=BTN_NONE; }
int PAD_anyJustReleased(void)	{ return pad.just_released!=BTN_NONE; }

int PAD_justPressed(int btn)	{ return pad.just_pressed & btn; }
int PAD_isPressed(int btn)		{ return pad.is_pressed & btn; }
int PAD_justReleased(int btn)	{ return pad.just_released & btn; }
int PAD_justRepeated(int btn)	{ return pad.just_repeated & btn; }

int PAD_tappedMenu(uint32_t now) {
	#define MENU_DELAY 250 // also in PWR_update()
	static uint32_t menu_start = 0;
	static int ignore_menu = 0; 
	if (PAD_justPressed(BTN_MENU)) {
		ignore_menu = 0;
		menu_start = now;
	}
	else if (PAD_isPressed(BTN_MENU) && BTN_MOD_BRIGHTNESS==BTN_MENU && (PAD_justPressed(BTN_MOD_PLUS) || PAD_justPressed(BTN_MOD_MINUS))) {
		ignore_menu = 1;
	}
	return (!ignore_menu && PAD_justReleased(BTN_MENU) && now-menu_start<MENU_DELAY);
}

///////////////////////////////

static struct VIB_Context {
	int initialized;
	pthread_t pt;
	int queued_strength[2];
	int strength;
	int effect;
} vib = {0};
static void* VIB_thread(void *arg) {
#define DEFER_FRAMES 3
	static int defer = 0;
	while(1) {
		SDL_Delay(17);
		if (vib.queued_strength[0]!=vib.strength) {
			if (defer<DEFER_FRAMES && vib.queued_strength[0]==0) { // minimize vacillation between 0 and some number (which this motor doesn't like)
				defer += 1;
				continue;
			}
			vib.strength = vib.queued_strength[0];
			defer = 0;

			PLAT_setRumble(vib.effect,vib.strength);
			continue;
		}
		if (vib.queued_strength[1]!=vib.strength) {
			if (defer<DEFER_FRAMES && vib.queued_strength[1]==0) { // minimize vacillation between 0 and some number (which this motor doesn't like)
				defer += 1;
				continue;
			}
			vib.strength = vib.queued_strength[1];
			defer = 0;

			PLAT_setRumble(vib.effect,vib.strength);
		}
	}
	return 0;
}
void VIB_init(void) {
	vib.queued_strength[0] = vib.queued_strength[0] = vib.strength = 0;
	pthread_create(&vib.pt, NULL, &VIB_thread, NULL);
	vib.initialized = 1;
}
void VIB_quit(void) {
	if (!vib.initialized) return;
	
	VIB_setStrength(0,0,0);
	VIB_setStrength(0,1,0);
	pthread_cancel(vib.pt);
	pthread_join(vib.pt, NULL);
}
void VIB_setStrength(int port, int effect, int strength) {
	if (vib.queued_strength[effect]==strength) return;
	vib.queued_strength[effect] = strength;
	vib.effect = effect;
}
int VIB_getStrength(void) {
	return vib.strength;
}

///////////////////////////////

static void PWR_initOverlay(void) {
	// setup surface
	pwr.overlay = PLAT_initOverlay();

	// draw battery
	SDLX_SetAlpha(gfx.assets, 0,0);
	GFX_blitAsset(ASSET_BLACK_PILL, NULL, pwr.overlay, NULL);
	SDLX_SetAlpha(gfx.assets, SDL_SRCALPHA,0);
	GFX_blitBattery(pwr.overlay, NULL);
}

static void PWR_updateBatteryStatus(void) {
	PLAT_getBatteryStatus(&pwr.is_charging, &pwr.charge);
	PLAT_enableOverlay(pwr.should_warn && pwr.charge<=PWR_LOW_CHARGE);
}

static void* PWR_monitorBattery(void *arg) {
	while(1) {
		// TODO: the frequency of checking could depend on whether 
		// we're in game (less frequent) or menu (more frequent)
		sleep(5);
		PWR_updateBatteryStatus();
	}
	return NULL;
}

void PWR_init(void) {
	pwr.can_sleep = 1;
	pwr.can_poweroff = 1;
	pwr.can_autosleep = 1;
	pwr.should_warn = 0;
	pwr.charge = PWR_LOW_CHARGE;
	
	PWR_initOverlay();

	PWR_updateBatteryStatus();
	pthread_create(&pwr.battery_pt, NULL, &PWR_monitorBattery, NULL);
	pwr.initialized = 1;
}
void PWR_quit(void) {
	if (!pwr.initialized) return;
	
	PLAT_quitOverlay();
	
	// cancel battery thread
	pthread_cancel(pwr.battery_pt);
	pthread_join(pwr.battery_pt, NULL);
}
void PWR_warn(int enable) {
	pwr.should_warn = enable;
	PLAT_enableOverlay(pwr.should_warn && pwr.charge<=PWR_LOW_CHARGE);
}

int PWR_ignoreSettingInput(int btn, int show_setting) {
	return show_setting && (btn==BTN_MOD_PLUS || btn==BTN_MOD_MINUS);
}

void PWR_update(int* _dirty, int* _show_setting, PWR_callback_t before_sleep, PWR_callback_t after_sleep) {
	int dirty = _dirty ? *_dirty : 0;
	int show_setting = _show_setting ? *_show_setting : 0;
	
	static uint32_t last_input_at = 0; // timestamp of last input (autosleep)
	static uint32_t checked_charge_at = 0; // timestamp of last time checking charge
	static uint32_t setting_shown_at = 0; // timestamp when settings started being shown
	static uint32_t power_pressed_at = 0; // timestamp when power button was just pressed
	static uint32_t mod_unpressed_at = 0; // timestamp of last time settings modifier key was NOT down
	
	static int was_charging = -1;
	if (was_charging==-1) was_charging = pwr.is_charging;

	uint32_t now = SDL_GetTicks();
	if (was_charging || PAD_anyPressed() || last_input_at==0) last_input_at = now;
	
	#define CHARGE_DELAY 1000
	if (dirty || now-checked_charge_at>=CHARGE_DELAY) {
		int is_charging = pwr.is_charging;
		if (was_charging!=is_charging) {
			was_charging = is_charging;
			dirty = 1;
		}
		checked_charge_at = now;
	}
	int factor = 1;
	if (exists(PWR_SLEEP_PATH)){
		factor=20;
	}
	//if (PAD_justReleased(BTN_POWEROFF) || (power_pressed_at && now-power_pressed_at>=50)) {  //50= immediate shutdown, 1000 = shutdown after 1 sec or sleep on quick button press
	if (PAD_justReleased(BTN_POWEROFF) || (power_pressed_at && now-power_pressed_at>=(50*factor))) {
		if (before_sleep) {
			before_sleep();
		}
		PWR_powerOff();
	}
	
	if (PAD_justPressed(BTN_POWER)) {
		power_pressed_at = now;
	}
	
	#define SLEEP_DELAY 30000 // 30 seconds
	if (now-last_input_at>=SLEEP_DELAY && PWR_preventAutosleep()) last_input_at = now;
	
	if (
		now-last_input_at>=SLEEP_DELAY || // autosleep
		(pwr.can_sleep && PAD_justReleased(BTN_SLEEP)) // manual sleep
	) {
		if (before_sleep) before_sleep();
		PWR_fauxSleep();
		if (after_sleep) after_sleep();
		
		last_input_at = now = SDL_GetTicks();
		power_pressed_at = 0;
		dirty = 1;
	}
	
	int was_dirty = dirty; // dirty list (not including settings/battery)
	
	// TODO: only delay hiding setting changes if that setting didn't require a modifier button be held, otherwise release as soon as modifier is released
	
	int delay_settings = BTN_MOD_BRIGHTNESS==BTN_MENU; // when both volume and brighness require a modifier hide settings as soon as it is released
	#define SETTING_DELAY 500
	if (show_setting && (now-setting_shown_at>=SETTING_DELAY || !delay_settings) && !PAD_isPressed(BTN_MOD_VOLUME) && !PAD_isPressed(BTN_MOD_BRIGHTNESS)) {
		show_setting = 0;
		dirty = 1;
	}
	
	if (!show_setting && !PAD_isPressed(BTN_MOD_VOLUME) && !PAD_isPressed(BTN_MOD_BRIGHTNESS)) {
		mod_unpressed_at = now; // this feels backwards but is correct
	}
	
	#define MOD_DELAY 250
	if (
		(
			(PAD_isPressed(BTN_MOD_VOLUME) || PAD_isPressed(BTN_MOD_BRIGHTNESS)) && 
			(!delay_settings || now-mod_unpressed_at>=MOD_DELAY)
		) || 
		((!BTN_MOD_VOLUME || !BTN_MOD_BRIGHTNESS) && (PAD_justRepeated(BTN_MOD_PLUS) || PAD_justRepeated(BTN_MOD_MINUS)))
	) {
		setting_shown_at = now;
		if (PAD_isPressed(BTN_MOD_BRIGHTNESS)) {
			show_setting = 1;
		}
		else {
			show_setting = 2;
		}
	}
	
	if (show_setting) dirty = 1; // shm is slow or keymon is catching input on the next frame
	if (_dirty) *_dirty = dirty;
	if (_show_setting) *_show_setting = show_setting;
}

// TODO: this isn't whether it can sleep but more if it should sleep in response to the sleep button
void PWR_disableSleep(void) {
	pwr.can_sleep = 0;
}
void PWR_enableSleep(void) {
	pwr.can_sleep = 1;
}

void PWR_disablePowerOff(void) {
	pwr.can_poweroff = 0;
}
void PWR_powerOff(void) {
	if (pwr.can_poweroff) {
		
		int w = DEVICE_WIDTH;
		int h = DEVICE_HEIGHT;
		int p = DEVICE_PITCH;
		if (GetHDMI()) {
			w = HDMI_WIDTH;
			h = HDMI_HEIGHT;
			p = HDMI_PITCH;
		}
		gfx.screen = GFX_resize(w,h,p);
		
		char* msg;
		if (HAS_POWER_BUTTON || HAS_POWEROFF_BUTTON) msg = exists(AUTO_RESUME_PATH) ? "Quicksave created,\npowering off" : "Powering off";
		else msg = exists(AUTO_RESUME_PATH) ? "Quicksave created,\npower off now" : "Power off now";
		
		// LOG_info("PWR_powerOff %s (%ix%i)\n", gfx.screen, gfx.screen->w, gfx.screen->h);
		
		// TODO: for some reason screen's dimensions end up being 0x0 in GFX_blitMessage...
		PLAT_clearVideo(gfx.screen);
		GFX_blitMessage(font.large, msg, gfx.screen,&(SDL_Rect){0,0,gfx.screen->w,gfx.screen->h}); //, NULL);
		GFX_flip(gfx.screen);
		PLAT_powerOff();
	}
}

static void PWR_enterSleep(void) {
	SDL_PauseAudio(1);
	if (GetHDMI()) {
		PLAT_clearVideo(gfx.screen);
		PLAT_flip(gfx.screen, 0);
	}
	else {
		SetRawVolume(MUTE_VOLUME_RAW);
		PLAT_enableBacklight(0);
	}
	system("killall -STOP keymon.elf");
	
	sync();
}
static void PWR_exitSleep(void) {
	system("killall -CONT keymon.elf");
	if (GetHDMI()) {
		// buh
	}
	else {
		PLAT_enableBacklight(1);
		SetVolume(GetVolume());
	}
	SDL_PauseAudio(0);
	
	sync();
}

static void PWR_waitForWake(void) {
	uint32_t sleep_ticks = SDL_GetTicks();
	while (!PAD_wake()) {
		SDL_Delay(200);
		if (pwr.can_poweroff && SDL_GetTicks()-sleep_ticks>=120000) { // increased to two minutes
			if (pwr.is_charging) sleep_ticks += 60000; // check again in a minute
			else PWR_powerOff();
		}
	}
	
	return;
}
void PWR_fauxSleep(void) {
	GFX_clear(gfx.screen);
	PAD_reset();
	PWR_enterSleep();
	PWR_waitForWake();
	PWR_exitSleep();
	PAD_reset();
}

void PWR_disableAutosleep(void) {
	pwr.can_autosleep = 0;
}
void PWR_enableAutosleep(void) {
	pwr.can_autosleep = 1;
}
int PWR_preventAutosleep(void) {
	return pwr.is_charging || !pwr.can_autosleep;
}

// updated by PWR_updateBatteryStatus()
int PWR_isCharging(void) {
	return pwr.is_charging;
}
int PWR_getBattery(void) { // 10-100 in 10-20% fragments
	return pwr.charge;
}

#include <arm_neon.h>
#include <stdint.h>
#include <stdlib.h>

//this is the replacement of SDL_SoftStretch
int scale_mat_nearest_lut_rgb565_neon_fast_xy_pitch(
    const uint16_t *src_ptr, int src_w, int src_h, int src_pitch,
    uint16_t *dst_ptr, int dst_w, int dst_h, int dst_pitch,
    int dst_x, int dst_y, int out_w, int out_h)
{
    uint64_t incx = ((uint64_t)src_w << 16) / out_w;
    uint64_t incy = ((uint64_t)src_h << 16) / out_h;

    int *x_lut = (int *)malloc(out_w * sizeof(int));
    if (!x_lut) return -1;

    uint64_t posx = incx / 2;
    for (int x = 0; x < out_w; x++) {
        x_lut[x] = (int)(posx >> 16);
        posx += incx;
    }

    uint64_t posy = incy / 2;
    for (int y = 0; y < out_h; y++) {
        int src_y = (int)(posy >> 16);
        const uint16_t *src_row = (const uint16_t *)((const uint8_t *)src_ptr + src_y * src_pitch);
        uint16_t *dst_row = (uint16_t *)((uint8_t *)dst_ptr + (dst_y + y) * dst_pitch) + dst_x;

        int x = 0;
        for (; x + 8 <= out_w; x += 8) {
            uint16_t tmp[8] __attribute__((aligned(16)));
            tmp[0] = src_row[x_lut[x + 0]];
            tmp[1] = src_row[x_lut[x + 1]];
            tmp[2] = src_row[x_lut[x + 2]];
            tmp[3] = src_row[x_lut[x + 3]];
            tmp[4] = src_row[x_lut[x + 4]];
            tmp[5] = src_row[x_lut[x + 5]];
            tmp[6] = src_row[x_lut[x + 6]];
            tmp[7] = src_row[x_lut[x + 7]];
            vst1q_u16(dst_row + x, vld1q_u16(tmp));
        }

        for (; x < out_w; x++) {
            dst_row[x] = src_row[x_lut[x]];
        }

        posy += incy;
    }

    free(x_lut);
    return 0;
}


//this is used during flip frame, it converts the RGB565 to ABGR8888 then it copy it to the destination buffer for a limited rect
//RG35XX where Red and Blue are swapped
void convert_rgb565_to_abgr8888_neon_rect(
    const uint16_t *src,
    uint32_t *dst,
    int src_width,
    int dst_pitch, // in pixels (not bytes)
    int start_x,
    int start_y,
    int rect_width,
    int rect_height
) {
    uint8x8_t alpha = vdup_n_u8(0xFF); // Costante alpha
    int8x8_t shift_r = vdup_n_s8(3);   // Shift per R e B (5 bit -> 8 bit)
    int8x8_t shift_g = vdup_n_s8(2);   // Shift per G (6 bit -> 8 bit)

    uint16x8_t r_mask = vdupq_n_u16(0xF800); // Maschera per R
    uint16x8_t g_mask = vdupq_n_u16(0x07E0); // Maschera per G
    uint16x8_t b_mask = vdupq_n_u16(0x001F); // Maschera per B

    #pragma omp parallel for
    for (int y = 0; y < rect_height; y++) {
        const uint16_t *line_src = src + (start_y + y) * src_width + start_x;
        uint32_t *line_dst = dst + (start_y + y) * dst_pitch + start_x;

        for (int x = 0; x < rect_width; x += 16) {
            uint16x8_t pixels1 = vld1q_u16(line_src + x);
            uint16x8_t pixels2 = vld1q_u16(line_src + x + 8);

            uint16x8_t r1 = vandq_u16(pixels1, r_mask);
            uint16x8_t g1 = vandq_u16(pixels1, g_mask);
            uint16x8_t b1 = vandq_u16(pixels1, b_mask);

            uint8x8_t r5_1 = vmovn_u16(vshrq_n_u16(r1, 11));
            uint8x8_t g6_1 = vmovn_u16(vshrq_n_u16(g1, 5));
            uint8x8_t b5_1 = vmovn_u16(b1);

            uint8x8_t r8_1 = vshl_u8(r5_1, shift_r);
            uint8x8_t g8_1 = vshl_u8(g6_1, shift_g);
            uint8x8_t b8_1 = vshl_u8(b5_1, shift_r);

            uint8x8x4_t abgr1 = {r8_1, g8_1, b8_1, alpha};

            uint16x8_t r2 = vandq_u16(pixels2, r_mask);
            uint16x8_t g2 = vandq_u16(pixels2, g_mask);
            uint16x8_t b2 = vandq_u16(pixels2, b_mask);

            uint8x8_t r5_2 = vmovn_u16(vshrq_n_u16(r2, 11));
            uint8x8_t g6_2 = vmovn_u16(vshrq_n_u16(g2, 5));
            uint8x8_t b5_2 = vmovn_u16(b2);

            uint8x8_t r8_2 = vshl_u8(r5_2, shift_r);
            uint8x8_t g8_2 = vshl_u8(g6_2, shift_g);
            uint8x8_t b8_2 = vshl_u8(b5_2, shift_r);

            uint8x8x4_t abgr2 = {r8_2, g8_2, b8_2, alpha};

            vst4_u8((uint8_t *)(line_dst + x), abgr1);
            vst4_u8((uint8_t *)(line_dst + x + 8), abgr2);
        }
    }
}

//this is used during flip frame, it converts the RGB565 to ABGR8888 then it copy it to the destination buffer for a limited rect
void convert_rgb565_to_argb8888_neon_rect(
    const uint16_t *src,
    uint32_t *dst,
    int src_width,
    int dst_pitch, // in pixels (not bytes)
    int start_x,
    int start_y,
    int rect_width,
    int rect_height
) {
    uint8x8_t alpha = vdup_n_u8(0xFF); // Costante alpha
    int8x8_t shift_r = vdup_n_s8(3);   // Shift per R e B (5 bit -> 8 bit)
    int8x8_t shift_g = vdup_n_s8(2);   // Shift per G (6 bit -> 8 bit)

    uint16x8_t r_mask = vdupq_n_u16(0xF800); // Maschera per R
    uint16x8_t g_mask = vdupq_n_u16(0x07E0); // Maschera per G
    uint16x8_t b_mask = vdupq_n_u16(0x001F); // Maschera per B

    #pragma omp parallel for
    for (int y = 0; y < rect_height; y++) {
        const uint16_t *line_src = src + (start_y + y) * src_width + start_x;
        uint32_t *line_dst = dst + (start_y + y) * dst_pitch + start_x;

        for (int x = 0; x < rect_width; x += 16) {
            uint16x8_t pixels1 = vld1q_u16(line_src + x);
            uint16x8_t pixels2 = vld1q_u16(line_src + x + 8);

            uint16x8_t r1 = vandq_u16(pixels1, r_mask);
            uint16x8_t g1 = vandq_u16(pixels1, g_mask);
            uint16x8_t b1 = vandq_u16(pixels1, b_mask);

            uint8x8_t r5_1 = vmovn_u16(vshrq_n_u16(r1, 11));
            uint8x8_t g6_1 = vmovn_u16(vshrq_n_u16(g1, 5));
            uint8x8_t b5_1 = vmovn_u16(b1);

            uint8x8_t r8_1 = vshl_u8(r5_1, shift_r);
            uint8x8_t g8_1 = vshl_u8(g6_1, shift_g);
            uint8x8_t b8_1 = vshl_u8(b5_1, shift_r);

            uint8x8x4_t abgr1 = {b8_1, g8_1, r8_1, alpha};

            uint16x8_t r2 = vandq_u16(pixels2, r_mask);
            uint16x8_t g2 = vandq_u16(pixels2, g_mask);
            uint16x8_t b2 = vandq_u16(pixels2, b_mask);

            uint8x8_t r5_2 = vmovn_u16(vshrq_n_u16(r2, 11));
            uint8x8_t g6_2 = vmovn_u16(vshrq_n_u16(g2, 5));
            uint8x8_t b5_2 = vmovn_u16(b2);

            uint8x8_t r8_2 = vshl_u8(r5_2, shift_r);
            uint8x8_t g8_2 = vshl_u8(g6_2, shift_g);
            uint8x8_t b8_2 = vshl_u8(b5_2, shift_r);

            uint8x8x4_t abgr2 = {b8_2, g8_2, r8_2, alpha};

            vst4_u8((uint8_t *)(line_dst + x), abgr1);
            vst4_u8((uint8_t *)(line_dst + x + 8), abgr2);
        }
    }
}



void convert_argb1555_to_rgb565_neon(
    int width, int height,
    uint16_t *dst, int dst_stride_pixels,
    const uint16_t *src, int src_stride_pixels)
{
    for (int y = 0; y < height; ++y) {
        const uint16_t *src_row = src + y * src_stride_pixels;
        uint16_t *dst_row = dst + y * dst_stride_pixels;
        int x = 0;
        for (; x + 8 <= width; x += 8) {
            uint16x8_t vsrc = vld1q_u16(src_row + x);

            uint16x8_t r = vandq_u16(vshrq_n_u16(vsrc, 10), vdupq_n_u16(0x1F));
            uint16x8_t g = vandq_u16(vshrq_n_u16(vsrc, 5), vdupq_n_u16(0x1F));
            uint16x8_t b = vandq_u16(vsrc, vdupq_n_u16(0x1F));

            // Estendi il verde da 5 a 6 bit
            uint16x8_t g6 = vorrq_u16(vshlq_n_u16(g, 1), vshrq_n_u16(g, 4));

            uint16x8_t rgb = vorrq_u16(
                vorrq_u16(vshlq_n_u16(r, 11), vshlq_n_u16(g6, 5)),
                b
            );

            vst1q_u16(dst_row + x, rgb);
        }
        // pixel rimanenti
        for (; x < width; ++x) {
            uint16_t px = src_row[x];
            uint16_t r = (px >> 10) & 0x1F;
            uint16_t g = (px >> 5) & 0x1F;
            uint16_t b = px & 0x1F;
            uint16_t g6 = (g << 1) | (g >> 4);
            dst_row[x] = (r << 11) | (g6 << 5) | b;
        }
    }
}

int FlipRotate000(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
	//this is actually a no rotation conversion.
	
	//copy a surface to the screen and flip it
	//it must be the same resolution, the bpp16 is then converted to 32bpp
	//fprintf(stdout,"Buffer has %d bpp\n", buffer->format->BitsPerPixel);fflush(stdout);

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

int FlipRotate270(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {	
	//this is actually a 90deg rotation

	//the alpha channel must be set to 0xff
	int thispitch = buffer->pitch/buffer->format->BytesPerPixel;
	int x, y, widthminus_1, heightminus_1;
	widthminus_1 = buffer->w - 1;
	heightminus_1 = buffer->h - 1;
	uint32_t *dsttmp;
	uint16_t *srctmp;
		//ok start conversion assuming it is RGB565		
	for (y = targetarea.y; y < (targetarea.y + targetarea.h) ; y++) {
		dsttmp = (uint32_t *)fbmmap + y + widthminus_1 * linewidth;
		srctmp = (uint16_t *)buffer->pixels + y * thispitch;
		for (x = targetarea.x; x < (targetarea.x + targetarea.w); x++) {
			int tmp1 = x * linewidth;
			uint16_t pixel = *((uint16_t *)srctmp + x);
			uint32_t r = (pixel & 0xF800) << 8;
			uint32_t g = (pixel & 0x7E0) << 5;
			uint32_t ba = 0xFF000000 | (pixel & 0x1F) << 3;
			*((uint32_t *)dsttmp - tmp1) = (uint32_t)( r | g | ba);
		}
	}
	return 0;	
}

int FlipRotate180(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
	//this is actually a 180deg rotation
	
	//copy a surface to the screen and flip it
	//it must be the same resolution, the bpp16 is then converted to 32bpp
	//fprintf(stdout,"Buffer has %d bpp\n", buffer->format->BitsPerPixel);fflush(stdout);

	//the alpha channel must be set to 0xff
	int thispitch = buffer->pitch/buffer->format->BytesPerPixel;
	int x, y, widthminus_1, heightminus_1;
	widthminus_1 = buffer->w - 1;
	heightminus_1 = buffer->h - 1;
	uint32_t *dsttmp;
	uint16_t *srctmp;
	//ok start conversion assuming it is RGB565		
	for (y = targetarea.y; y < (targetarea.y + targetarea.h) ; y++) {
		dsttmp = (uint32_t *)fbmmap + (heightminus_1 - y) * linewidth + widthminus_1;
		srctmp = (uint16_t *)buffer->pixels + y * thispitch;
		for (x = targetarea.x; x < (targetarea.x + targetarea.w); x++) {
			uint16_t pixel = *((uint16_t *)srctmp + x);
			uint32_t r = (pixel & 0xF800) << 8;
			uint32_t g = (pixel & 0x7E0) << 5;
			uint32_t ba = 0xFF000000 | (pixel & 0x1F) << 3;
			*((uint32_t *)dsttmp - x) = (uint32_t)( r | g | ba);
		}
	}
	return 0;	
}

int FlipRotate090(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
	//this is actually a 270deg rotation

	//the alpha channel must be set to 0xff
	int thispitch = buffer->pitch/buffer->format->BytesPerPixel;
	int x, y, widthminus_1, heightminus_1;
	widthminus_1 = buffer->w - 1;
	heightminus_1 = buffer->h - 1;
	uint32_t *dsttmp;
	uint16_t *srctmp;
		//ok start conversion assuming it is RGB565		
	for (y = targetarea.y; y < (targetarea.y + targetarea.h) ; y++) {
		dsttmp = (uint32_t *)fbmmap + heightminus_1 - y;
		srctmp = (uint16_t *)buffer->pixels + y * thispitch;
		for (x = targetarea.x; x < (targetarea.x + targetarea.w); x++) {
			uint16_t pixel = *((uint16_t *)srctmp + x);
			uint32_t r = (pixel & 0xF800) << 8;
			uint32_t g = (pixel & 0x7E0) << 5;
			uint32_t ba = 0xFF000000 | (pixel & 0x1F) << 3;
			*((uint32_t *)dsttmp + x  * linewidth) = (uint32_t)( r | g | ba);
		}
	}	
	return 0;	
}

//rotating RGB565 to ABGR8888

int FlipRotate000bgr(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
	//this is actually a no rotation conversion.
	
	//copy a surface to the screen and flip it
	//it must be the same resolution, the bpp16 is then converted to 32bpp
	//fprintf(stdout,"Buffer has %d bpp\n", buffer->format->BitsPerPixel);fflush(stdout);

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
			uint32_t r = (pixel & 0xF800) >> 8;
			uint32_t g = (pixel & 0x7E0) << 5;
			uint32_t ba = 0xFF000000 | (pixel & 0x1F) << 19;
			*((uint32_t *)dsttmp + x ) = (uint32_t)( r | g | ba);
		}
	}	
	return 0;	
}

int FlipRotate270bgr(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {	
	//this is actually a 90deg rotation

	//the alpha channel must be set to 0xff
	int thispitch = buffer->pitch/buffer->format->BytesPerPixel;
	int x, y, widthminus_1, heightminus_1;
	widthminus_1 = buffer->w - 1;
	heightminus_1 = buffer->h - 1;
	uint32_t *dsttmp;
	uint16_t *srctmp;
		//ok start conversion assuming it is RGB565		
	for (y = targetarea.y; y < (targetarea.y + targetarea.h) ; y++) {
		dsttmp = (uint32_t *)fbmmap + y + widthminus_1 * linewidth;
		srctmp = (uint16_t *)buffer->pixels + y * thispitch;
		for (x = targetarea.x; x < (targetarea.x + targetarea.w); x++) {
			int tmp1 = x * linewidth;
			uint16_t pixel = *((uint16_t *)srctmp + x);
			uint32_t r = (pixel & 0xF800) >> 8;
			uint32_t g = (pixel & 0x7E0) << 5;
			uint32_t ba = 0xFF000000 | (pixel & 0x1F) << 19;
			*((uint32_t *)dsttmp - tmp1) = (uint32_t)( r | g | ba);
		}
	}
	return 0;	
}

int FlipRotate180bgr(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
	//this is actually a 180deg rotation
	
	//copy a surface to the screen and flip it
	//it must be the same resolution, the bpp16 is then converted to 32bpp
	//fprintf(stdout,"Buffer has %d bpp\n", buffer->format->BitsPerPixel);fflush(stdout);

	//the alpha channel must be set to 0xff
	int thispitch = buffer->pitch/buffer->format->BytesPerPixel;
	int x, y, widthminus_1, heightminus_1;
	widthminus_1 = buffer->w - 1;
	heightminus_1 = buffer->h - 1;
	uint32_t *dsttmp;
	uint16_t *srctmp;
	//ok start conversion assuming it is RGB565		
	for (y = targetarea.y; y < (targetarea.y + targetarea.h) ; y++) {
		dsttmp = (uint32_t *)fbmmap + (heightminus_1 - y) * linewidth + widthminus_1;
		srctmp = (uint16_t *)buffer->pixels + y * thispitch;
		for (x = targetarea.x; x < (targetarea.x + targetarea.w); x++) {
			uint16_t pixel = *((uint16_t *)srctmp + x);
			uint32_t r = (pixel & 0xF800) >> 8;
			uint32_t g = (pixel & 0x7E0) << 5;
			uint32_t ba = 0xFF000000 | (pixel & 0x1F) << 19;
			*((uint32_t *)dsttmp - x) = (uint32_t)( r | g | ba);
		}
	}
	return 0;	
}

int FlipRotate090bgr(SDL_Surface *buffer, void * fbmmap, int linewidth, SDL_Rect targetarea) {
	//this is actually a 270deg rotation

	//the alpha channel must be set to 0xff
	int thispitch = buffer->pitch/buffer->format->BytesPerPixel;
	int x, y, widthminus_1, heightminus_1;
	widthminus_1 = buffer->w - 1;
	heightminus_1 = buffer->h - 1;
	uint32_t *dsttmp;
	uint16_t *srctmp;
		//ok start conversion assuming it is RGB565		
	for (y = targetarea.y; y < (targetarea.y + targetarea.h) ; y++) {
		dsttmp = (uint32_t *)fbmmap + heightminus_1 - y;
		srctmp = (uint16_t *)buffer->pixels + y * thispitch;
		for (x = targetarea.x; x < (targetarea.x + targetarea.w); x++) {
			uint16_t pixel = *((uint16_t *)srctmp + x);
			uint32_t r = (pixel & 0xF800) >> 8;
			uint32_t g = (pixel & 0x7E0) << 5;
			uint32_t ba = 0xFF000000 | (pixel & 0x1F) << 19;
			*((uint32_t *)dsttmp + x  * linewidth) = (uint32_t)( r | g | ba);
		}
	}	
	return 0;	
}

void rotateIMG(void *src, void*dst, int rotation, int srcw, int srch, int srcp) {
	int x, y, thispitch, width_minus_1, height_minus_1;
	thispitch = srcp / 2;
	width_minus_1 = srcw - 1;
	height_minus_1 = srch - 1;
	uint16_t *dsttmp, *srctmp;
	if (rotation == 0) {	
//		gettimeofday(&now2,NULL);	
		for (y = 0; y < srch; y++) {
			dsttmp = (uint16_t *)dst + y * srcw;
			srctmp = (uint16_t *)src + y * thispitch;
			for (x = 0; x <  srcw; x++) {	
				*((uint16_t *)dsttmp + x) = *((uint16_t *)srctmp + x );
			}
		}
	} else
	if (rotation == 1) {
	//	gettimeofday(&now2,NULL);
		for (y = 0; y < srch; y++) {
			dsttmp = (uint16_t *)dst + y;
			srctmp = (uint16_t *)src + y * thispitch;
			for (x = 0; x < srcw; x++) {
				*((uint16_t *)dsttmp + (width_minus_1 - x) *  srch) = *((uint16_t *)srctmp + x);
			}
		}
	}
	else
	if (rotation == 2) {
	//	gettimeofday(&now2,NULL);		
		for (y = 0; y < srch; y++) {
			dsttmp = (uint16_t *)dst +(height_minus_1 - y) * srcw + width_minus_1;
			srctmp = (uint16_t *)src + y * thispitch;
			for (x = 0; x < srcw; x++) {
				*((uint16_t *)dsttmp - x) = *((uint16_t *)srctmp + x);
			}
		}	
	} else
	if (rotation == 3) {
	//	gettimeofday(&now2,NULL);
		for (y = 0; y < srch; y++) {
			dsttmp = (uint16_t *)dst + height_minus_1 - y;
			srctmp = (uint16_t *)src + y * thispitch;
			for (x = 0; x < srcw; x++) {
				*((uint16_t *)dsttmp  + x  *  srch) = *((uint16_t *)srctmp + x );
			}
		}
	}
}