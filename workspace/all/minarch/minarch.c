#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <msettings.h>

#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <libgen.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <zlib.h>
#include <pthread.h>
#include <sched.h>

#include "libretro.h"
#include "defines.h"
#include "utils.h"
#include "api.h"

#include "scaler.h"

#include <sys/sysinfo.h>
#include <sys/time.h>

#if defined(USE_SDL2)
#include "SDL2_rotozoom.h"
#include <SDL2/SDL_image.h>
#else
#include "SDL_rotozoom.h"
#endif

///////////////////////////////////////

static SDL_Surface* screen;
static SDL_Surface* screengame;

static int quit = 0;
static int show_menu = 0;
static int simple_mode = 0;
static int thread_video = 0;
static int should_run_core = 0; // used by threaded video
static int should_run_flip = 0;
static int flipThreadStarted = 0;
static int coreThreadStarted = 0;
static int flipThreadPaused = 0;
static int coreThreadPaused = 0;
static int render = 0;
static int rendering = 0;
static int firstmenu = 0;
static int processors = 0;
static int Founddiskcontrol = 0;
static int NumDiscsDetected = 0;
static int coreDiscManaged = 0;
static int config_load_done = 0;
static int waitforthread = 0;
static int loadgamesuccess = 0;
static int can_dupe = false;
uint32_t *mutedaudiodata;

static pthread_t		core_pt, flip_pt;
static pthread_mutex_t	core_mx, flip_mx;
static pthread_cond_t	core_rq, flip_rq; 
static struct mybackbuffer	backbuffer;
static void* coreThread(void *arg);
static void* flipThread(void *arg);


char pwractionstr[256];

int fancy_mode;




// default frontend options
static int screen_scaling = SCALE_ASPECT;
//static int screen_max_scale = 5; //6x
static int screen_effect = EFFECT_NONE;
static int screen_sharpness = SHARPNESS_SOFT;
static int prevent_tearing = 1; // lenient
static int sync_ref = 0;
static int show_debug = 0;
static int max_ff_speed = 3; // 4x
static int fast_forward = 0;
static int overclock = 1; // normal
static int has_custom_controllers = 0;
static int gamepad_type = 0; // index in gamepad_labels/gamepad_values
static int downsample = 0; // set to 1 to convert from 8888 to 565, set to 2 to convert from 1555 to 565

GFX_Renderer renderer;

static int coreloaded = 0;

///////////////////////////////////////

static struct Core {
	int initialized;
	int need_fullpath;
	
	const char tag[8]; // eg. GBC
	const char name[128]; // eg. gambatte
	const char version[128]; // eg. Gambatte (v0.5.0-netlink 7e02df6)
	const char extensions[128]; // eg. gb|gbc|dmg
	
	const char config_dir[MAX_PATH]; // eg. /mnt/sdcard/.userdata/rg35xx/GB-gambatte
	const char states_dir[MAX_PATH]; // eg. /mnt/sdcard/.userdata/arm-480/GB-gambatte
	const char saves_dir[MAX_PATH]; // eg. /mnt/sdcard/Saves/GB
	const char bios_dir[MAX_PATH]; // eg. /mnt/sdcard/Bios/GB
	
	double fps;
	double sample_rate;
	double aspect_ratio;
	
	void* handle;
	void (*init)(void);
	void (*deinit)(void);
	
	void (*get_system_info)(struct retro_system_info *info);
	void (*get_system_av_info)(struct retro_system_av_info *info);
	void (*set_controller_port_device)(unsigned port, unsigned device);
	
	void (*reset)(void);
	void (*run)(void);
	size_t (*serialize_size)(void);
	bool (*serialize)(void *data, size_t size);
	bool (*unserialize)(const void *data, size_t size);
	bool (*load_game)(const struct retro_game_info *game);
	bool (*load_game_special)(unsigned game_type, const struct retro_game_info *info, size_t num_info);
	void (*unload_game)(void);
	unsigned (*get_region)(void);
	void *(*get_memory_data)(unsigned id);
	size_t (*get_memory_size)(unsigned id);
	
	// retro_audio_buffer_status_callback_t audio_buffer_status;
} core;

///////////////////////////////////////
// based on picoarch/unzip.c

#define ZIP_HEADER_SIZE 30
#define ZIP_CHUNK_SIZE 65536
#define ZIP_LE_READ16(buf) ((uint16_t)(((uint8_t *)(buf))[1] << 8 | ((uint8_t *)(buf))[0]))
#define ZIP_LE_READ32(buf) ((uint32_t)(((uint8_t *)(buf))[3] << 24 | ((uint8_t *)(buf))[2] << 16 | ((uint8_t *)(buf))[1] << 8 | ((uint8_t *)(buf))[0]))
typedef int (*Zip_extract_t)(FILE* zip, FILE* dst, size_t size);

static int Zip_copy(FILE* zip, FILE* dst, size_t size) { // uncompressed?
	uint8_t buffer[ZIP_CHUNK_SIZE];
	while (size) {
		size_t sz = MIN(size, ZIP_CHUNK_SIZE);
		if (sz!= fread(buffer, 1, sz, zip)) return -1;
		if (sz!=fwrite(buffer, 1, sz, dst)) return -1;
		size -= sz;
	}
	return 0;
}
static int Zip_inflate(FILE* zip, FILE* dst, size_t size) { // compressed
	z_stream stream = {0};
	size_t have = 0;
	uint8_t  in[ZIP_CHUNK_SIZE];
	uint8_t out[ZIP_CHUNK_SIZE];
	int ret = -1;

	ret = inflateInit2(&stream, -MAX_WBITS);
	if (ret != Z_OK)
		return ret;
	
	do {
		size_t insize = MIN(size, ZIP_CHUNK_SIZE);

		stream.avail_in = fread(in, 1, insize, zip);
		if (ferror(zip)) {
			(void)inflateEnd(&stream);
			return Z_ERRNO;
		}

		if (!stream.avail_in)
			break;
		stream.next_in = in;

		do {
			stream.avail_out = ZIP_CHUNK_SIZE;
			stream.next_out = out;

			ret = inflate(&stream, Z_NO_FLUSH);
			switch(ret) {
				case Z_NEED_DICT:
					ret = Z_DATA_ERROR;
				case Z_DATA_ERROR:
				case Z_MEM_ERROR:
					(void)inflateEnd(&stream);
					return ret;
			}

			have = ZIP_CHUNK_SIZE - stream.avail_out;
			if (fwrite(out, 1, have, dst) != have || ferror(dst)) {
				(void)inflateEnd(&stream);
				return Z_ERRNO;
			}
		} while (stream.avail_out == 0);

		size -= insize;
	} while (size && ret != Z_STREAM_END);

	(void)inflateEnd(&stream);

	if (!size || ret == Z_STREAM_END) {
		return Z_OK;
	} else {
		return Z_DATA_ERROR;
	}
}

///////////////////////////////////////

static struct Game {
	char path[MAX_PATH];
	char name[MAX_PATH]; // TODO: rename to basename?
	char fullname[MAX_PATH];
	char basename[MAX_PATH];
	char m3u_path[MAX_PATH];
	char tmp_path[MAX_PATH]; // location of unzipped file
	void* data;
	size_t size;
	int is_open;
} game;
static void Game_open(char* path) {
	LOG_info("Game_open\n");
	memset(&game, 0, sizeof(game));
	
	strcpy((char*)game.path, path);
	//LOG_info("Game.path = %s\n", game.path);
	strcpy((char*)game.name, strrchr(path, '/')+1);
	//LOG_info("Game.name = %s\n", game.name);
	getDisplayName(game.name,game.basename);
	getDisplayNameParens(game.name,game.fullname);
	//LOG_info("Game.basename = %s\n", game.basename);
	// if we have a zip file
	if (suffixMatch(".zip", game.path)) {
		LOG_info("is zip file\n");
		int supports_zip = 0;
		int i = 0;
		char* ext;
		char exts[128];
		char* extensions[32];
		strcpy(exts,core.extensions);
		while ((ext=strtok(i?NULL:exts,"|"))) {
			extensions[i++] = ext;
			if (!strcmp("zip", ext)) {
				supports_zip = 1;
				break;
			}
		}
		extensions[i] = NULL;
	
		// if the core doesn't support zip files natively
		if (!supports_zip) {
			FILE *zip = fopen(game.path, "r");
			if (zip==NULL) {
				LOG_error("Error opening archive: %s\n\t%s\n", game.path, strerror(errno));
				return;
			}
			
			// extract a known file format
			uint8_t header[ZIP_HEADER_SIZE];
			uint32_t next = 0;
			uint16_t len = 0;
			char filename[MAX_PATH];
			uint32_t compressed_size = 0;
			char extension[8];
			while (1) {
				if (next) fseek(zip, next, SEEK_CUR);
				
				if (ZIP_HEADER_SIZE!=fread(header, 1, ZIP_HEADER_SIZE, zip)) break;
				
				if ((uint16_t)(header[6]) & 0x0008) break;
				
				len = ZIP_LE_READ16(&header[26]);
				if (len>=MAX_PATH) break;
				
				if (len!=fread(filename,1,len,zip)) break;
				filename[len] = '\0';
				LOG_info("filename: %s\n", filename);
				
				compressed_size = ZIP_LE_READ32(&header[18]);
				
				fseek(zip, ZIP_LE_READ16(&header[28]), SEEK_CUR);
				next = compressed_size;
				
				int found = 0;
				for (i=0; extensions[i]; i++) {
					sprintf(extension, ".%s", extensions[i]);
					if (suffixMatch(extension, filename)) {
						
						found = 1;
						break;
					}
				}
				if (!found) continue;
				
				char tmp_template[MAX_PATH];
				strcpy(tmp_template, "/tmp/minarch-XXXXXX");
				char* tmp_dirname = mkdtemp(tmp_template);
				// LOG_info("tmp_dirname: %s\n", tmp_dirname);
				sprintf(game.tmp_path, "%s/%s", tmp_dirname, basename(filename));
				
				// TODO: we need to clear game.tmp_path if anything below this point fails!
				
				FILE* dst = fopen(game.tmp_path, "w");
				if (dst==NULL) {
					game.tmp_path[0] = '\0';
					LOG_error("Error extracting file: %s\n\t%s\n", filename, strerror(errno));
					return;
				}
				
				Zip_extract_t extract = NULL;
				switch (ZIP_LE_READ16(&header[8])) {
					case 0: extract = Zip_copy; break;
					case 8: extract = Zip_inflate; break;
				}
				
				if (!extract || extract(zip,dst,compressed_size)) {
					game.tmp_path[0] = '\0';
					LOG_error("Error extracting file: %s\n\t%s\n", filename, strerror(errno));
					return;
				}
				
				fclose(dst);
				
				break;
			}
			
			fclose(zip);
		}
	}
		
	// some cores handle opening files themselves, eg. pcsx_rearmed
	// if the frontend tries to load a 500MB file itself bad things happen
	if (!core.need_fullpath) {
		path = game.tmp_path[0]=='\0'?game.path:game.tmp_path;
		
		FILE *file = fopen(path, "r");
		if (file==NULL) {
			LOG_error("Error opening game: %s\n\t%s\n", path, strerror(errno));
			return;
		}
	
		fseek(file, 0, SEEK_END);
		game.size = ftell(file);
	
		rewind(file);
		game.data = malloc(game.size);
		if (game.data==NULL) {
			LOG_error("Couldn't allocate memory for file: %s\n", path);
			return;
		}
	
		fread(game.data, sizeof(uint8_t), game.size, file);
	
		fclose(file);
	}
	
	// m3u-based?
	char* tmp;
	char m3u_path[256];
	char base_path[256];
	char dir_name[256];

	strcpy(m3u_path, game.path);
	tmp = strrchr(m3u_path, '/') + 1;
	tmp[0] = '\0';
	
	strcpy(base_path, m3u_path);
	
	tmp = strrchr(m3u_path, '/');
	tmp[0] = '\0';

	tmp = strrchr(m3u_path, '/');
	strcpy(dir_name, tmp);
	
	tmp = m3u_path + strlen(m3u_path); 
	strcpy(tmp, dir_name);
	
	tmp = m3u_path + strlen(m3u_path);
	strcpy(tmp, ".m3u");
	
	if (exists(m3u_path)) {
		strcpy(game.m3u_path, m3u_path);
		strcpy((char*)game.name, strrchr(m3u_path, '/')+1);
	}
	
	game.is_open = 1;
}
static void Game_close(void) {
	if (game.data) free(game.data);
	if (game.tmp_path[0]) remove(game.tmp_path);
	game.is_open = 0;
	VIB_setStrength(0); // just in case
}

static struct retro_disk_control_callback disk_control;
static struct retro_disk_control_ext_callback disk_control_ext;
static void Game_changeDisc(int index,  char* path) {
	LOG_info("Game_changeDisc start gamepath=%s -> path=%s\n", game.path, path);
	if (!coreDiscManaged){
		if (exactMatch(game.path, path)) return;
	}
	if (!exists(path)) return;
	
	if (!coreDiscManaged) {
		LOG_info("Game_changeDisc 2a\n");
		Game_close();
		LOG_info("Game_changeDisc 2b\n");
		Game_open(path);
	}	

		struct retro_game_info game_info = {};
		game_info.path = game.path;
		game_info.data = game.data;
		game_info.size = game.size;
	//int _index = coreDiscManaged ? index : 0;
	if (Founddiskcontrol == 2) {
		if (coreDiscManaged) {
			//LOG_info("1ChangeDisc: founddiskcontrol=%d, coreDiscManaged=%d with index=%d, path=%s\n", Founddiskcontrol, coreDiscManaged, index, path);
			disk_control.set_eject_state(true);
			disk_control.set_image_index(index);
			disk_control.set_eject_state(false);
			
		} else {
			//LOG_info("2ChangeDisc: founddiskcontrol=%d, coreDiscManaged=%d with index=%d, path=%s\n", Founddiskcontrol, coreDiscManaged, index, path);
			disk_control.replace_image_index(index, &game_info);
		
		}
	} else {
		if (coreDiscManaged) {
			//LOG_info("3ChangeDisc: founddiskcontrol=%d, coreDiscManaged=%d with index=%d, current_index=%d, path=%s\n", Founddiskcontrol, coreDiscManaged, index,disk_control_ext.get_image_index() ,path);
			
		
		} else {
			//LOG_info("4ChangeDisc: founddiskcontrol=%d, coreDiscManaged=%d with index=%d, path=%s\n", Founddiskcontrol, coreDiscManaged, index, path);
			disk_control_ext.replace_image_index(index, &game_info);		
		}
		if (disk_control_ext.set_eject_state(true)) {
			//	LOG_info("3ChangeDisc: disk_control_ext.set_eject_state(true) SUCCESS!!!!\n");
			};
		disk_control_ext.set_image_index(index);
		if (disk_control_ext.set_eject_state(false)){
			//LOG_info("3ChangeDisc: disk_control_ext.set_eject_state(false) SUCCESS!!!!\n");
			}
	}	
	putFile(CHANGE_DISC_PATH, path); // MinUI still needs to know this to update recents.txt
	//putInt(CHANGE_DISC_INDEX_PATH, _index);
	//Game_close();
	//core.reset();
	//status = STATUS_RESET;
	//show_menu = 0;
	//Game_open(path);
	
}

///////////////////////////////////////

static void SRAM_getPath(char* filename) {
	sprintf(filename, "%s/%s.sav", core.saves_dir, game.name);
}
static void SRAM_read(void) {
	size_t sram_size = core.get_memory_size(RETRO_MEMORY_SAVE_RAM);
	if (!sram_size) return;
	
	char filename[MAX_PATH];
	SRAM_getPath(filename);
	printf("sav path (read): %s\n", filename);
	
	FILE *sram_file = fopen(filename, "r");
	if (!sram_file) return;

	void* sram = core.get_memory_data(RETRO_MEMORY_SAVE_RAM);

	if (!sram || !fread(sram, 1, sram_size, sram_file)) {
		LOG_error("Error reading SRAM data\n");
	}

	fclose(sram_file);
}
static void SRAM_write(void) {
	size_t sram_size = core.get_memory_size(RETRO_MEMORY_SAVE_RAM);
	if (!sram_size) return;
	
	char filename[MAX_PATH];
	SRAM_getPath(filename);
	printf("sav path (write): %s\n", filename);
		
	FILE *sram_file = fopen(filename, "w");
	if (!sram_file) {
		LOG_error("Error opening SRAM file: %s\n", strerror(errno));
		return;
	}

	void *sram = core.get_memory_data(RETRO_MEMORY_SAVE_RAM);

	if (!sram || sram_size != fwrite(sram, 1, sram_size, sram_file)) {
		LOG_error("Error writing SRAM data to file\n");
	}

	fclose(sram_file);

	sync();
}

///////////////////////////////////////

static void RTC_getPath(char* filename) {
	sprintf(filename, "%s/%s.rtc", core.saves_dir, game.name);
}
static void RTC_read(void) {
	size_t rtc_size = core.get_memory_size(RETRO_MEMORY_RTC);
	if (!rtc_size) return;
	
	char filename[MAX_PATH];
	RTC_getPath(filename);
	printf("rtc path (read): %s\n", filename);
	
	FILE *rtc_file = fopen(filename, "r");
	if (!rtc_file) return;

	void* rtc = core.get_memory_data(RETRO_MEMORY_RTC);

	if (!rtc || !fread(rtc, 1, rtc_size, rtc_file)) {
		LOG_error("Error reading RTC data\n");
	}

	fclose(rtc_file);
}
static void RTC_write(void) {
	size_t rtc_size = core.get_memory_size(RETRO_MEMORY_RTC);
	if (!rtc_size) return;
	
	char filename[MAX_PATH];
	RTC_getPath(filename);
	printf("rtc path (write) size(%u): %s\n", rtc_size, filename);
		
	FILE *rtc_file = fopen(filename, "w");
	if (!rtc_file) {
		LOG_error("Error opening RTC file: %s\n", strerror(errno));
		return;
	}

	void *rtc = core.get_memory_data(RETRO_MEMORY_RTC);

	if (!rtc || rtc_size != fwrite(rtc, 1, rtc_size, rtc_file)) {
		LOG_error("Error writing RTC data to file\n");
	}

	fclose(rtc_file);

	sync();
}

///////////////////////////////////////

static int state_slot = 0;
static void State_getPath(char* filename) {
	if (state_slot == 0){
		sprintf(filename, "%s/%s.state", core.states_dir, game.fullname);
	} else {
	sprintf(filename, "%s/%s.state%i", core.states_dir, game.fullname, state_slot);
	}
}
static void State_read(void) { // from picoarch
	size_t state_size = core.serialize_size();
	LOG_info("Begin state read - size(%u) on slot %d\n", state_size, state_slot);fflush(stdout);
	if (!state_size) return;

	int was_ff = fast_forward;
	fast_forward = 0;

	void *state = calloc(1, state_size);
	if (!state) {
		LOG_error("Couldn't allocate memory for state\n");
		goto error;
	}

	char filename[MAX_PATH];
	State_getPath(filename);
	
	FILE *state_file = fopen(filename, "r");
	if (!state_file) {
		if (state_slot!=8) { // st8 is a default state in MiniUI and may not exist, that's okay
			LOG_error("Error opening state file: %s (%s)\n", filename, strerror(errno));
		}
		goto error;
	}
	
	// some cores report the wrong serialize size initially for some games, eg. mgba: Wario Land 4
	// so we allow a size mismatch as long as the actual size fits in the buffer we've allocated
	if (state_size < fread(state, 1, state_size, state_file)) {
		LOG_error("Error reading state data from file: %s (%s)\n", filename, strerror(errno));
		goto error;
	}

	if (!core.unserialize(state, state_size)) {
		LOG_error("Error restoring save state: %s (%s)\n", filename, strerror(errno));
		goto error;
	}
	LOG_info("Successfully state read - size(%u) on slot %d\n", state_size, state_slot);fflush(stdout);
error:
	if (state) free(state);
	if (state_file) fclose(state_file);
	
	fast_forward = was_ff;
}
static void State_write(void) { // from picoarch
	size_t state_size = core.serialize_size();
	LOG_info("Begin state write - size(%u) on slot %d\n", state_size, state_slot);fflush(stdout);
	if (!state_size) return;
	
	int was_ff = fast_forward;
	fast_forward = 0;
	
	void *state = calloc(1, state_size);
	if (!state) {
		LOG_error("Couldn't allocate memory for state\n");
		goto error;
	}

	char filename[MAX_PATH];
	State_getPath(filename);
	
	FILE *state_file = fopen(filename, "w");
	if (!state_file) {
		LOG_error("Error opening state file: %s (%s)\n", filename, strerror(errno));
		goto error;
	}

	if (!core.serialize(state, state_size)) {
		LOG_error("Error creating save state: %s (%s)\n", filename, strerror(errno));
		goto error;
	}

	if (state_size != fwrite(state, 1, state_size, state_file)) {
		LOG_error("Error writing state data to file: %s (%s)\n", filename, strerror(errno));
		goto error;
	}
	LOG_info("Successfully state write - size(%u) on slot %d\n", state_size, state_slot);fflush(stdout);
error:
	if (state) free(state);
	if (state_file) fclose(state_file);

	sync();
	
	fast_forward = was_ff;
}
static void State_autosave(void) {
	int last_state_slot = state_slot;
	state_slot = AUTO_RESUME_SLOT;
	State_write();
	state_slot = last_state_slot;
}
static void State_resume(int _resume_slot) {
	//if (!exists(RESUME_SLOT_PATH)) return;
	if (_resume_slot==-1) return;
	int last_state_slot = state_slot;
	state_slot = _resume_slot;
	//state_slot = getInt(RESUME_SLOT_PATH);
	//unlink(RESUME_SLOT_PATH);
	State_read();
	state_slot = last_state_slot;
}

///////////////////////////////

typedef struct Option {
	char* key;
	char* name; // desc
	char* desc; // info, truncated
	char* full; // info, longer but possibly still truncated
	char* var;
	int default_value;
	int value;
	int count; // TODO: drop this?
	int lock;
	char** values;
	char** labels;
} Option;
typedef struct OptionList {
	int count;
	int changed;
	Option* options;
	
	int enabled_count;
	Option** enabled_options;
	// OptionList_callback_t on_set;
} OptionList;

static char* onoff_labels[] = {
	"Off",
	"On",
	NULL
};
static char* scaling_labels[] = {
	"Native",
	"Aspect",
	"Extended",
	"Fullscreen",
	NULL
};
static char* max_scaling_labels[] = {
	"1x",
	"2x",
	"3x",
	"4x",
	"5x",
	"6x",
	NULL,
};
static char* effect_labels[] = {
	"None",
	"Line",
	"Grid",
	NULL
};
static char* sharpness_labels[] = {
	"Sharp",
	"Crisp",
	"Soft",
	NULL
};
static char* tearing_labels[] = {
	"Off",
	"Lenient",
	"Strict",
	NULL
};
static char* sync_ref_labels[] = {
	"Auto",
	"Screen",
	"Native",
	NULL
};
static char* max_ff_labels[] = {
	"None",
	"2x",
	"3x",
	"4x",
	"5x",
	"6x",
	"7x",
	"8x",
	NULL,
};

///////////////////////////////

enum {
	FE_OPT_SCALING,
//	FE_OPT_MAX_SCALE,
	FE_OPT_EFFECT,
	FE_OPT_SHARPNESS,
	FE_OPT_TEARING,
	FE_OPT_SYNC_REFERENCE,
	FE_OPT_OVERCLOCK,
	FE_OPT_THREAD,
	FE_OPT_DEBUG,
	FE_OPT_MAXFF,
	FE_OPT_COUNT,
};

enum {
	SYNC_SRC_AUTO,
	SYNC_SRC_SCREEN,
	SYNC_SRC_CORE
};

enum {
	SHORTCUT_SAVE_STATE,
	SHORTCUT_LOAD_STATE,
	SHORTCUT_RESET_GAME,
	SHORTCUT_SAVE_QUIT,
	SHORTCUT_CYCLE_SCALE,
	SHORTCUT_CYCLE_EFFECT,
	SHORTCUT_TOGGLE_FF,
	SHORTCUT_HOLD_FF,
	SHORTCUT_COUNT,
};

#define LOCAL_BUTTON_COUNT 16 // depends on device
#define RETRO_BUTTON_COUNT 16 // allow L3/R3 to be remapped by user if desired, eg. Virtual Boy uses extra buttons for right d-pad

typedef struct ButtonMapping { 
	char* name;
	int retro;
	int local; // TODO: dislike this name...
	int mod;
	int default_;
	int ignore;
} ButtonMapping;

static ButtonMapping default_button_mapping[] = { // used if pak.cfg doesn't exist or doesn't have bindings
	{"Up",			RETRO_DEVICE_ID_JOYPAD_UP,		BTN_ID_DPAD_UP},
	{"Down",		RETRO_DEVICE_ID_JOYPAD_DOWN,	BTN_ID_DPAD_DOWN},
	{"Left",		RETRO_DEVICE_ID_JOYPAD_LEFT,	BTN_ID_DPAD_LEFT},
	{"Right",		RETRO_DEVICE_ID_JOYPAD_RIGHT,	BTN_ID_DPAD_RIGHT},	
	{"A Button",	RETRO_DEVICE_ID_JOYPAD_A,		BTN_ID_A},
	{"B Button",	RETRO_DEVICE_ID_JOYPAD_B,		BTN_ID_B},
	{"X Button",	RETRO_DEVICE_ID_JOYPAD_X,		BTN_ID_X},
	{"Y Button",	RETRO_DEVICE_ID_JOYPAD_Y,		BTN_ID_Y},
	{"Start",		RETRO_DEVICE_ID_JOYPAD_START,	BTN_ID_START},
	{"Select",		RETRO_DEVICE_ID_JOYPAD_SELECT,	BTN_ID_SELECT},
	{"L1 Button",	RETRO_DEVICE_ID_JOYPAD_L,		BTN_ID_L1},
	{"R1 Button",	RETRO_DEVICE_ID_JOYPAD_R,		BTN_ID_R1},
	{"L2 Button",	RETRO_DEVICE_ID_JOYPAD_L2,		BTN_ID_L2},
	{"R2 Button",	RETRO_DEVICE_ID_JOYPAD_R2,		BTN_ID_R2},
	{"L3 Button",	RETRO_DEVICE_ID_JOYPAD_L3,		BTN_ID_L3},
	{"R3 Button",	RETRO_DEVICE_ID_JOYPAD_R3,		BTN_ID_R3},
	{NULL,0,0}
};
static ButtonMapping button_label_mapping[] = { // used to lookup the retro_id and local btn_id from button name
	{"NONE",	-1,								BTN_ID_NONE},
	{"UP",		RETRO_DEVICE_ID_JOYPAD_UP,		BTN_ID_DPAD_UP},
	{"DOWN",	RETRO_DEVICE_ID_JOYPAD_DOWN,	BTN_ID_DPAD_DOWN},
	{"LEFT",	RETRO_DEVICE_ID_JOYPAD_LEFT,	BTN_ID_DPAD_LEFT},
	{"RIGHT",	RETRO_DEVICE_ID_JOYPAD_RIGHT,	BTN_ID_DPAD_RIGHT},
	{"A",		RETRO_DEVICE_ID_JOYPAD_A,		BTN_ID_A},
	{"B",		RETRO_DEVICE_ID_JOYPAD_B,		BTN_ID_B},
	{"X",		RETRO_DEVICE_ID_JOYPAD_X,		BTN_ID_X},
	{"Y",		RETRO_DEVICE_ID_JOYPAD_Y,		BTN_ID_Y},
	{"START",	RETRO_DEVICE_ID_JOYPAD_START,	BTN_ID_START},
	{"SELECT",	RETRO_DEVICE_ID_JOYPAD_SELECT,	BTN_ID_SELECT},
	{"L1",		RETRO_DEVICE_ID_JOYPAD_L,		BTN_ID_L1},
	{"R1",		RETRO_DEVICE_ID_JOYPAD_R,		BTN_ID_R1},
	{"L2",		RETRO_DEVICE_ID_JOYPAD_L2,		BTN_ID_L2},
	{"R2",		RETRO_DEVICE_ID_JOYPAD_R2,		BTN_ID_R2},
	{"L3",		RETRO_DEVICE_ID_JOYPAD_L3,		BTN_ID_L3},
	{"R3",		RETRO_DEVICE_ID_JOYPAD_R3,		BTN_ID_R3},
	{NULL,0,0}
};
static ButtonMapping core_button_mapping[RETRO_BUTTON_COUNT+1] = {0};

static const char* device_button_names[LOCAL_BUTTON_COUNT] = {
	[BTN_ID_DPAD_UP]	= "UP",
	[BTN_ID_DPAD_DOWN]	= "DOWN",
	[BTN_ID_DPAD_LEFT]	= "LEFT",
	[BTN_ID_DPAD_RIGHT]	= "RIGHT",
	[BTN_ID_SELECT]		= "SELECT",
	[BTN_ID_START]		= "START",
	[BTN_ID_Y]			= "Y",
	[BTN_ID_X]			= "X",
	[BTN_ID_B]			= "B",
	[BTN_ID_A]			= "A",
	[BTN_ID_L1]			= "L1",
	[BTN_ID_R1]			= "R1",
	[BTN_ID_L2]			= "L2",
	[BTN_ID_R2]			= "R2",
	[BTN_ID_L3]			= "L3",
	[BTN_ID_R3]			= "R3",	
};


// NOTE: these must be in BTN_ID_ order also off by 1 because of NONE (which is -1 in BTN_ID_ land)
static char* button_labels[] = {
	"NONE", // displayed by default
	"UP",
	"DOWN",
	"LEFT",
	"RIGHT",
	"A",
	"B",
	"X",
	"Y",
	"START",
	"SELECT",
	"L1",
	"R1",
	"L2",
	"R2",
	"L3",
	"R3",
	"MENU+UP",
	"MENU+DOWN",
	"MENU+LEFT",
	"MENU+RIGHT",
	"MENU+A",
	"MENU+B",
	"MENU+X",
	"MENU+Y",
	"MENU+START",
	"MENU+SELECT",
	"MENU+L1",
	"MENU+R1",
	"MENU+L2",
	"MENU+R2",
	"MENU+L3",
	"MENU+R3",
	NULL,
};
static char* overclock_labels[] = {
	"Powersave",
	"Normal",
	"Performance",
	"Max",
	NULL,
};

// TODO: this should be provided by the core
static char* gamepad_labels[] = {
	"Standard",
	"DualShock",
	NULL,
};
static char* gamepad_values[] = {
	"1",
	"517",
	NULL,
};


enum {
	CONFIG_NONE,
	CONFIG_CONSOLE,
	CONFIG_GAME,
};

static struct Config {
	char* system_cfg; // system.cfg based on system limitations
	char* default_cfg; // pak.cfg based on platform limitations
	char* user_cfg; // minarch.cfg or game.cfg based on user preference
	OptionList frontend;
	OptionList core;
	ButtonMapping* controls;
	ButtonMapping* shortcuts;
	int loaded;
	int initialized;
	int controller_map_abxy_to_rstick;
	int controller_map_lstick_to_dpad;
} config = {
	.frontend = { // (OptionList)
		.count = FE_OPT_COUNT,
		.options = (Option[]){
			[FE_OPT_SCALING] = {
				.key	= "minarch_screen_scaling", 
				.name	= "Screen Scaling",
				.desc	= "Native uses integer scaling.\nAspect uses core reported aspect ratio.\nExtended is like aspect but the it extends the image to fit 3/4 of screen.\nFullscreen has non-squarepixels.",
				.default_value = 1,
				.value = 1,
				.count = SCALE_COUNT,
				.values = scaling_labels,
				.labels = scaling_labels,
			},
		/*	[FE_OPT_MAX_SCALE] = {
				.key	= "minarch_screen_max_scale",
				.name	= "Max Upscale",
				.desc	= "Select the maximum upscale factor.",
				.default_value = 5, // 6x
				.value = 5, // 6x
				.count = 6,
				.values = max_scaling_labels,
				.labels = max_scaling_labels,
			},*/
			[FE_OPT_EFFECT] = {
				.key	= "minarch_screen_effect",
				.name	= "Screen Effect",
				.desc	= "Grid simulates an LCD grid.\nLine simulates CRT scanlines.\nEffects usually look best at native scaling.",
				.default_value = 0,
				.value = 0,
				.count = 3,
				.values = effect_labels,
				.labels = effect_labels,
			},
			[FE_OPT_SHARPNESS] = {
				.key	= "minarch_screen_sharpness",
				.name	= "Screen Sharpness",
				.desc	= "Sharp uses nearest neighbor sampling.\nCrisp integer upscales before linear sampling.\nSoft uses linear sampling.",
				.default_value = 2,
				.value = 2,
				.count = 3,
				.values = sharpness_labels,
				.labels = sharpness_labels,
			},
			[FE_OPT_TEARING] = {
				.key	= "minarch_prevent_tearing",
				.name	= "Prevent Tearing",
				.desc	= "Wait for vsync before drawing the next frame.\nLenient only waits when within frame budget.\nStrict always waits.",
				.default_value = VSYNC_LENIENT,
				.value = VSYNC_LENIENT,
				.count = 3,
				.values = tearing_labels,
				.labels = tearing_labels,
			},
			[FE_OPT_SYNC_REFERENCE] = {
				.key	= "minarch_sync_reference",
				.name	= "Core Sync",
				.desc	= "Choose what should be used as a\nreference for the frame rate.\n\"Native\" uses the emulator frame rate,\n\"Screen\" uses the frame rate of the screen.",
				.default_value = SYNC_SRC_AUTO,
				.value = SYNC_SRC_AUTO,
				.count = 3,
				.values = sync_ref_labels,
				.labels = sync_ref_labels,
			},
			[FE_OPT_OVERCLOCK] = {
				.key	= "minarch_cpu_speed",
				.name	= "CPU Speed",
				.desc	= "Over- or underclock the CPU to prioritize\npure performance or power savings.",
				.default_value = 1,
				.value = 1,
				.count = 4,
				.values = overclock_labels,
				.labels = overclock_labels,
			},
			[FE_OPT_THREAD] = {
				.key	= "minarch_thread_video",
				.name	= "Thread Core",
				.desc	= "Move emulation to a thread.\nPrevents audio crackle but may\ncause dropped frames.",
				.default_value = 0,
				.value = 0,
				.count = 2,
				.values = onoff_labels,
				.labels = onoff_labels,
			},
			[FE_OPT_DEBUG] = {
				.key	= "minarch_debug_hud",
				.name	= "Debug HUD",
				.desc	= "Show frames per second, cpu load,\nresolution, and scaler information.",
				.default_value = 0,
				.value = 0,
				.count = 2,
				.values = onoff_labels,
				.labels = onoff_labels,
			},
			[FE_OPT_MAXFF] = {
				.key	= "minarch_max_ff_speed",
				.name	= "Max FF Speed",
				.desc	= "Fast forward will not exceed the\nselected speed (but may be less\ndepending on game and emulator).",
				.default_value = 3, // 4x
				.value = 3, // 4x
				.count = 8,
				.values = max_ff_labels,
				.labels = max_ff_labels,
			},
			[FE_OPT_COUNT] = {NULL}
		}
	},
	.core = { // (OptionList)
		.count = 0,
		.options = (Option[]){
			{NULL},
		},
	},
	.controls = default_button_mapping,
	.shortcuts = (ButtonMapping[]){
		[SHORTCUT_SAVE_STATE]			= {"Save State",		-1, BTN_ID_NONE, 0},
		[SHORTCUT_LOAD_STATE]			= {"Load State",		-1, BTN_ID_NONE, 0},
		[SHORTCUT_RESET_GAME]			= {"Reset Game",		-1, BTN_ID_NONE, 0},
		[SHORTCUT_SAVE_QUIT]			= {"Save & Quit",		-1, BTN_ID_NONE, 0},
		[SHORTCUT_CYCLE_SCALE]			= {"Cycle Scaling",		-1, BTN_ID_NONE, 0},
		[SHORTCUT_CYCLE_EFFECT]			= {"Cycle Effect",		-1, BTN_ID_NONE, 0},
		[SHORTCUT_TOGGLE_FF]			= {"Toggle FF",			-1, BTN_ID_NONE, 0},
		[SHORTCUT_HOLD_FF]				= {"Hold FF",			-1, BTN_ID_NONE, 0},
		{NULL}
	},
	.controller_map_abxy_to_rstick = 0,
	.controller_map_lstick_to_dpad = 0,
};
static int Config_getValue(char* cfg, const char* key, char* out_value, int* lock) {
	char* tmp = cfg;
	while ((tmp = strstr(tmp, key))) {
		if (lock!=NULL && tmp>cfg && *(tmp-1)=='-') *lock = 1; // prefixed with a `-` means lock
		tmp += strlen(key);
		if (!strncmp(tmp, " = ", 3)) break;
	};
	if (!tmp) return 0;
	tmp += 3;
	
	strncpy(out_value, tmp, 256);
	out_value[256 - 1] = '\0';
	tmp = strchr(out_value, '\n');
	if (!tmp) tmp = strchr(out_value, '\r');
	if (tmp) *tmp = '\0';

	// LOG_info("\t%s = %s (%s)\n", key, out_value, (lock && *lock) ? "hidden":"shown");
	return 1;
}

static void setOverclock(int i) {
	overclock = i;
	switch (i) {
		case 0: PWR_setCPUSpeed(CPU_SPEED_POWERSAVE); break;
		case 1: PWR_setCPUSpeed(CPU_SPEED_NORMAL); break;
		case 2: PWR_setCPUSpeed(CPU_SPEED_PERFORMANCE); break;
		case 3: PWR_setCPUSpeed(CPU_SPEED_MAX); break;
	}
	processors = PLAT_getNumProcessors();
}
static int toggle_thread = 0;
char effect_str[5];
static void Config_syncFrontend(char* key, int value) {
	int i = -1;
	if (exactMatch(key,config.frontend.options[FE_OPT_SCALING].key)) {
		screen_scaling 	= value;
		
		if (screen_scaling==SCALE_NATIVE) GFX_setSharpness(SHARPNESS_SHARP);
		else GFX_setSharpness(screen_sharpness);
		
		renderer.dst_p = 0;
		i = FE_OPT_SCALING;
	}
/*else if (exactMatch(key,config.frontend.options[FE_OPT_MAX_SCALE].key)) {
		screen_max_scale = value;
		renderer.dst_p = 0;
		i = FE_OPT_MAX_SCALE;
	}*/
	else if (exactMatch(key,config.frontend.options[FE_OPT_EFFECT].key)) {
		screen_effect = value;
		GFX_setEffect(value);
		if (screen_effect==EFFECT_GRID) {
			sprintf(effect_str, "2");
		} else if (screen_effect==EFFECT_LINE) {
			sprintf(effect_str, "1");
		} else { 
			sprintf(effect_str, "0"); 
		}

		renderer.dst_p = 0;
		i = FE_OPT_EFFECT;
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_SHARPNESS].key)) {
		screen_sharpness = value;
		GFX_setSharpness(value);
		renderer.dst_p = 0;
		i = FE_OPT_SHARPNESS;
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_TEARING].key)) {
		prevent_tearing = value;
		i = FE_OPT_TEARING;
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_SYNC_REFERENCE].key)) {
		sync_ref = value;
		i = FE_OPT_SYNC_REFERENCE;
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_THREAD].key)) {
		int old_value = thread_video;
		toggle_thread = old_value!=value;
		waitforthread = value;
		i = FE_OPT_THREAD;
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_OVERCLOCK].key)) {
		overclock = value;
		i = FE_OPT_OVERCLOCK;
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_DEBUG].key)) {
		show_debug = value;
		i = FE_OPT_DEBUG;
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_MAXFF].key)) {
		max_ff_speed = value;
		i = FE_OPT_MAXFF;
	}
	if (i==-1) return;
	Option* option = &config.frontend.options[i];
	option->value = value;
}
static void OptionList_setOptionValue(OptionList* list, const char* key, const char* value);
enum {
	CONFIG_WRITE_ALL,
	CONFIG_WRITE_GAME,
};
static void Config_getPath(char* filename, int override) {
	if (override) sprintf(filename, "%s/%s.cfg", core.config_dir, game.name);
	else sprintf(filename, "%s/minarch.cfg", core.config_dir);
}
static void Config_init(void) {
	if (!config.default_cfg || config.initialized) return;
	
	LOG_info("Config_init\n");
	char* tmp = config.default_cfg;
	char* tmp2;
	char* key;
	
	char button_name[128];
	char button_id[128];
	int i = 0;
	while ((tmp = strstr(tmp, "bind "))) {
		tmp += 5; // tmp now points to the button name (plus the rest of the line)
		key = tmp;
		tmp = strstr(tmp, " = ");
		if (!tmp) break;
		
		int len = tmp-key;
		strncpy(button_name, key, len);
		button_name[len] = '\0';
		
		tmp += 3;
		strncpy(button_id, tmp, 128);
		tmp2 = strchr(button_id, '\n');
		if (!tmp2) tmp2 = strchr(button_id, '\r');
		if (tmp2) *tmp2 = '\0';
		
		int retro_id = -1;
		int local_id = -1;
		
		tmp2 = strrchr(button_id, ':');
		int remap = 0;
		if (tmp2) {
			for (int j=0; button_label_mapping[j].name; j++) {
				ButtonMapping* button = &button_label_mapping[j];
				if (!strcmp(tmp2+1,button->name)) {
					retro_id = button->retro;
					break;
				}
			}
			*tmp2 = '\0';
		}
		for (int j=0; button_label_mapping[j].name; j++) {
			ButtonMapping* button = &button_label_mapping[j];
			if (!strcmp(button_id,button->name)) {
				local_id = button->local;
				if (retro_id==-1) retro_id = button->retro;
				break;
			}
		}
		
		tmp += strlen(button_id); // prepare to continue search
		
		LOG_info("\tbind %s (%s) %i:%i\n", button_name, button_id, local_id, retro_id);
		
		// TODO: test this without a final line return
		tmp2 = calloc(strlen(button_name)+1, sizeof(char));
		strcpy(tmp2, button_name);
		ButtonMapping* button = &core_button_mapping[i++];
		button->name = tmp2;
		button->retro = retro_id;
		button->local = local_id;
	};
	
	config.initialized = 1;
}
static void Config_quit(void) {
	if (!config.initialized) return;
	for (int i=0; core_button_mapping[i].name; i++) {
		free(core_button_mapping[i].name);
	}
}
static void Config_readOptionsString(char* cfg) {
	if (!cfg) return;

	LOG_info("Config_readOptions\n");
	char key[256];
	char value[256];
	for (int i=0; config.frontend.options[i].key; i++) {
		Option* option = &config.frontend.options[i];
		if (!Config_getValue(cfg, option->key, value, &option->lock)) continue;
		OptionList_setOptionValue(&config.frontend, option->key, value);
		Config_syncFrontend(option->key, option->value);
	}
	
	if (has_custom_controllers && Config_getValue(cfg,"minarch_gamepad_type",value,NULL)) {
		gamepad_type = strtol(value, NULL, 0);
		int device = strtol(gamepad_values[gamepad_type], NULL, 0);
		core.set_controller_port_device(0, device);
	}

	if (Config_getValue(cfg,"MapABXYtoRightStick",value,NULL)) {
		if (strstr(value, "On")){
				config.controller_map_abxy_to_rstick = 1;
			} else {
				config.controller_map_abxy_to_rstick = 0;	
			}
	}

	if (Config_getValue(cfg,"MapLeftSticktoDPad",value,NULL)) {
		if (strstr(value, "On")){
				config.controller_map_lstick_to_dpad = 1;
			} else {
				config.controller_map_lstick_to_dpad = 0;
			}
		pad.map_leftstick_to_dpad = config.controller_map_lstick_to_dpad;
	}
	 
	for (int i=0; config.core.options[i].key; i++) {
		Option* option = &config.core.options[i];
		if (!Config_getValue(cfg, option->key, value, &option->lock)) continue;
		OptionList_setOptionValue(&config.core, option->key, value);
	}
}
static void Config_readControlsString(char* cfg) {
	if (!cfg) return;

	LOG_info("Config_readControlsString\n");
	
	char key[256];
	char value[256];
	char* tmp;
	for (int i=0; config.controls[i].name; i++) {
		ButtonMapping* mapping = &config.controls[i];
		sprintf(key, "bind %s", mapping->name);
		sprintf(value, "NONE");
		
		if (!Config_getValue(cfg, key, value, NULL)) continue;
		if ((tmp = strrchr(value, ':'))) *tmp = '\0'; // this is a binding artifact in default.cfg, ignore
		
		int id = -1;
		for (int j=0; button_labels[j]; j++) {
			if (!strcmp(button_labels[j],value)) {
				id = j - 1;
				break;
			}
		}
		// LOG_info("\t%s (%i)\n", value, id);
		
		int mod = 0;
		if (id>=LOCAL_BUTTON_COUNT) {
			id -= LOCAL_BUTTON_COUNT;
			mod = 1;
		}
		
		mapping->local = id;
		mapping->mod = mod;
	}
	
	for (int i=0; config.shortcuts[i].name; i++) {
		ButtonMapping* mapping = &config.shortcuts[i];
		sprintf(key, "bind %s", mapping->name);
		sprintf(value, "NONE");

		if (!Config_getValue(cfg, key, value, NULL)) continue;
		
		int id = -1;
		for (int j=0; button_labels[j]; j++) {
			if (!strcmp(button_labels[j],value)) {
				id = j - 1;
				break;
			}
		}
		
		int mod = 0;
		if (id>=LOCAL_BUTTON_COUNT) {
			id -= LOCAL_BUTTON_COUNT;
			mod = 1;
		}
		// LOG_info("shortcut %s:%s (%i:%i)\n", key,value, id, mod);

		mapping->local = id;
		mapping->mod = mod;
	}
}
static void Config_load(void) {
	LOG_info("Config_load\n");
	
	char* system_path = SYSTEM_PATH "/system.cfg";
	if (exists(system_path)) config.system_cfg = allocFile(system_path);
	else config.system_cfg = NULL;
	
	char default_path[MAX_PATH];
	getEmuPath((char *)core.tag, default_path);
	char* tmp = strrchr(default_path, '/');
	strcpy(tmp,"/default.cfg");
	
	if (exists(default_path)) config.default_cfg = allocFile(default_path);
	else config.default_cfg = NULL;
	
	char path[MAX_PATH];
	config.loaded = CONFIG_NONE;
	int override = 0;
	Config_getPath(path, CONFIG_WRITE_GAME);
	if (exists(path)) override = 1; 
	if (!override) Config_getPath(path, CONFIG_WRITE_ALL);
	
	config.user_cfg = allocFile(path);
	if (!config.user_cfg) return;
	
	config.loaded = override ? CONFIG_GAME : CONFIG_CONSOLE;
}
static void Config_free(void) {
	if (config.system_cfg) free(config.system_cfg);
	if (config.default_cfg) free(config.default_cfg);
	if (config.user_cfg) free(config.user_cfg);	
	LOG_info("Config_free: waitforthread=%d thread_video=%d, toggle_thread=%d\n", waitforthread, thread_video, toggle_thread);fflush(stdout);
}
static void Config_readOptions(void) {
	Config_readOptionsString(config.system_cfg);
	Config_readOptionsString(config.default_cfg);
	Config_readOptionsString(config.user_cfg);

	// screen_scaling = SCALE_NATIVE; // TODO: tmp
}
static void Config_readControls(void) {
	Config_readControlsString(config.default_cfg);
	Config_readControlsString(config.user_cfg);
}
static void Config_write(int override) {
	char path[MAX_PATH];
	// sprintf(path, "%s/%s.cfg", core.config_dir, game.name);
	Config_getPath(path, CONFIG_WRITE_GAME);
	
	if (!override) {
		if (config.loaded==CONFIG_GAME) unlink(path);
		Config_getPath(path, CONFIG_WRITE_ALL);
	}
	config.loaded = override ? CONFIG_GAME : CONFIG_CONSOLE;
	
	FILE *file = fopen(path, "wb");
	if (!file) return;
	
	for (int i=0; config.frontend.options[i].key; i++) {
		Option* option = &config.frontend.options[i];
		fprintf(file, "%s = %s\n", option->key, option->values[option->value]);
	}
	for (int i=0; config.core.options[i].key; i++) {
		Option* option = &config.core.options[i];
		fprintf(file, "%s = %s\n", option->key, option->values[option->value]);
	}

	if (has_custom_controllers) fprintf(file, "%s = %i\n", "minarch_gamepad_type", gamepad_type);

	fprintf(file,"%s = %s\n", "MapABXYtoRightStick", config.controller_map_abxy_to_rstick ? "On" : "Off");
	fprintf(file,"%s = %s\n", "MapLeftSticktoDPad", config.controller_map_lstick_to_dpad ? "On" : "Off");

	for (int i=0; config.controls[i].name; i++) {
		ButtonMapping* mapping = &config.controls[i];
		int j = mapping->local + 1;
		if (mapping->mod) j += LOCAL_BUTTON_COUNT;
		fprintf(file, "bind %s = %s\n", mapping->name, button_labels[j]);
	}
	for (int i=0; config.shortcuts[i].name; i++) {
		ButtonMapping* mapping = &config.shortcuts[i];
		int j = mapping->local + 1;
		if (mapping->mod) j += LOCAL_BUTTON_COUNT;
		fprintf(file, "bind %s = %s\n", mapping->name, button_labels[j]);
	}
	
	fclose(file);
	sync();
}
static void Config_restore(void) {
	char path[MAX_PATH];
	if (config.loaded==CONFIG_GAME) {
		sprintf(path, "%s/%s.cfg", core.config_dir, game.name);
		unlink(path);
	}
	else if (config.loaded==CONFIG_CONSOLE) {
		sprintf(path, "%s/minarch.cfg", core.config_dir);
		unlink(path);
	}
	config.loaded = CONFIG_NONE;
	
	for (int i=0; config.frontend.options[i].key; i++) {
		Option* option = &config.frontend.options[i];
		option->value = option->default_value;
		Config_syncFrontend(option->key, option->value);
	}
	for (int i=0; config.core.options[i].key; i++) {
		Option* option = &config.core.options[i];
		option->value = option->default_value;
	}
	config.core.changed = 1; // let the core know

	if (has_custom_controllers) {
		gamepad_type = 0;
		core.set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
	}

	config.controller_map_abxy_to_rstick = 0;
	config.controller_map_lstick_to_dpad = 0;
	pad.map_leftstick_to_dpad = config.controller_map_lstick_to_dpad;

	for (int i=0; config.controls[i].name; i++) {
		ButtonMapping* mapping = &config.controls[i];
		mapping->local = mapping->default_;
		mapping->mod = 0;
	}
	for (int i=0; config.shortcuts[i].name; i++) {
		ButtonMapping* mapping = &config.shortcuts[i];
		mapping->local = BTN_ID_NONE;
		mapping->mod = 0;
	}
	
	Config_load();
	Config_readOptions();
	Config_readControls();
	Config_free();
	
	renderer.dst_p = 0;
}

///////////////////////////////

static  int Option_getValueIndex(Option* item, const char* value) {
	if (!value) return 0;
	for (int i=0; i<item->count; i++) {
		if (!strcmp(item->values[i], value)) return i;
	}
	return 0;
}
static void Option_setValue(Option* item, const char* value) {
	// TODO: store previous value?
	item->value = Option_getValueIndex(item, value);
}

// the following 3 functions always touch config.core, the rest can operate on arbitrary OptionLists
static void OptionList_init(const struct retro_core_option_definition *defs) {
	LOG_info("OptionList_init\n");
	int count;
	for (count=0; defs[count].key; count++);
	
	// LOG_info("count: %i\n", count);
	
	// TODO: add frontend options to this? so the can use the same override method? eg. minarch_*
	
	config.core.count = count;
	if (count) {
		config.core.options = calloc(count+1, sizeof(Option));
		
		for (int i=0; i<config.core.count; i++) {
			int len;
			const struct retro_core_option_definition *def = &defs[i];
			Option* item = &config.core.options[i];
			len = strlen(def->key) + 1;
		
			item->key = calloc(len, sizeof(char));
			strcpy(item->key, def->key);
			
			len = strlen(def->desc) + 1;
			item->name = calloc(len, sizeof(char));
			strcpy(item->name, def->desc);
			
			if (def->info) {
				len = strlen(def->info) + 1;
				item->desc = calloc(len, sizeof(char));
				strncpy(item->desc, def->info, len);

				item->full = calloc(len, sizeof(char));
				strncpy(item->full, item->desc, len);
				// item->desc[len-1] = '\0';
				
				// these magic numbers are more about chars per line than pixel width 
				// so it's not going to be relative to the screen size, only the scale
				// what does that even mean?
				GFX_wrapText(font.tiny, item->desc, SCALE1(240), 2); // TODO magic number!
				GFX_wrapText(font.medium, item->full, SCALE1(240), 7); // TODO: magic number!
			}
		
			for (count=0; def->values[count].value; count++);
		
			item->count = count;
			item->values = calloc(count+1, sizeof(char*));
			item->labels = calloc(count+1, sizeof(char*));
	
			for (int j=0; j<count; j++) {
				const char* value = def->values[j].value;
				const char* label = def->values[j].label;
		
				len = strlen(value) + 1;
				item->values[j] = calloc(len, sizeof(char));
				strcpy(item->values[j], value);
		
				if (label) {
					len = strlen(label) + 1;
					item->labels[j] = calloc(len, sizeof(char));
					strcpy(item->labels[j], label);
				}
				else {
					item->labels[j] = item->values[j];
				}
				// printf("\t%s\n", item->labels[j]);
			}
			
			item->value = Option_getValueIndex(item, def->default_value);
			item->default_value = item->value;
			
			LOG_info("\tINIT %s (%s) TO %s (%s)\n", item->name, item->key, item->labels[item->value], item->values[item->value]);
		}
	}
	// fflush(stdout);
}
static void OptionList_vars(const struct retro_variable *vars) {
	LOG_info("OptionList_vars\n");
	int count;
	for (count=0; vars[count].key; count++);
	
	config.core.count = count;
	if (count) {
		config.core.options = calloc(count+1, sizeof(Option));
	
		for (int i=0; i<config.core.count; i++) {
			int len;
			const struct retro_variable *var = &vars[i];
			Option* item = &config.core.options[i];

			len = strlen(var->key) + 1;
			item->key = calloc(len, sizeof(char));
			strcpy(item->key, var->key);
			
			len = strlen(var->value) + 1;
			item->var = calloc(len, sizeof(char));
			strcpy(item->var, var->value);
			
			char* tmp = strchr(item->var, ';');
			if (tmp && *(tmp+1)==' ') {
				*tmp = '\0';
				item->name = item->var;
				tmp += 2;
			}
			
			char* opt = tmp;
			for (count=0; (tmp=strchr(tmp, '|')); tmp++, count++);
			count += 1; // last entry after final '|'
		
			item->count = count;
			item->values = calloc(count+1, sizeof(char*));
			item->labels = calloc(count+1, sizeof(char*));

			tmp = opt;
			int j;
			for (j=0; (tmp=strchr(tmp, '|')); j++) {
				item->values[j] = opt;
				item->labels[j] = opt;
				*tmp = '\0';
				tmp += 1;
				opt = tmp; 
			}
			item->values[j] = opt;
			item->labels[j] = opt;
			
			// no native default_value support for retro vars
			item->value = 0;
			item->default_value = item->value;
			// printf("SET %s to %s (%i)\n", item->key, default_value, item->value); fflush(stdout);
		}
	}
	// fflush(stdout);
}
static void OptionList_reset(void) {
	if (!config.core.count) return;
	
	for (int i=0; i<config.core.count; i++) {
		Option* item = &config.core.options[i];
		if (item->var) {
			// values/labels are all points to var
			// so no need to free individually
			free(item->var);
		}
		else {
			if (item->desc) free(item->desc);
			if (item->full) free(item->full);
			for (int j=0; j<item->count; j++) {
				char* value = item->values[j];
				char* label = item->labels[j];
				if (label!=value) free(label);
				free(value);
			}
		}
		free(item->values);
		free(item->labels);
		free(item->key);
		free(item->name);
	}
	if (config.core.enabled_options) free(config.core.enabled_options);
	config.core.enabled_count = 0;
	free(config.core.options);
}

static Option* OptionList_getOption(OptionList* list, const char* key) {
	for (int i=0; i<list->count; i++) {
		Option* item = &list->options[i];
		if (!strcmp(item->key, key)) return item;
	}
	return NULL;
}
static char* OptionList_getOptionValue(OptionList* list, const char* key) {
	Option* item = OptionList_getOption(list, key);
	if (item) LOG_info("\tGET %s (%s) = %s (%s)\n", item->name, item->key, item->labels[item->value], item->values[item->value]);
	
	if (item) return item->values[item->value];
	else LOG_warn("unknown option %s \n", key);
	return NULL;
}
static void OptionList_setOptionRawValue(OptionList* list, const char* key, int value) {
	Option* item = OptionList_getOption(list, key);
	if (item) {
		item->value = value;
		list->changed = 1;
		LOG_info("\tRAW SET %s (%s) TO %s (%s)\n", item->name, item->key, item->labels[item->value], item->values[item->value]);
		// if (list->on_set) list->on_set(list, key);
	}
	else LOG_info("unknown option %s \n", key);
}
static void OptionList_setOptionValue(OptionList* list, const char* key, const char* value) {
	Option* item = OptionList_getOption(list, key);
	if (item) {
		Option_setValue(item, value);
		list->changed = 1;
		LOG_info("\tSET %s (%s) TO %s (%s)\n", item->name, item->key, item->labels[item->value], item->values[item->value]);
		// if (list->on_set) list->on_set(list, key);
	}
	else LOG_info("unknown option %s \n", key);
}
// static void OptionList_setOptionVisibility(OptionList* list, const char* key, int visible) {
// 	Option* item = OptionList_getOption(list, key);
// 	if (item) item->visible = visible;
// 	else printf("unknown option %s \n", key); fflush(stdout);
// }

///////////////////////////////

static void Menu_beforeSleep(void);
static void Menu_afterSleep(void);

static void Menu_saveState(void);
static void Menu_loadState(void);
static void Menu_makeboxart(void);
int waiting_for_thread_stop = 0;

static void wait_For_Thread(void) {
	//at first stop running core.run
	waiting_for_thread_stop = 1;
//	LOG_info("wait_For_Thread IN0: render = %i - rendering = %i\n",render,rendering);fflush(stdout);
	pthread_mutex_lock(&core_mx);
	should_run_core = 0;
	pthread_mutex_unlock(&core_mx);
	//wait 50msecs to be sure the core has stopped 
//	LOG_info("wait_For_Thread IN1: render = %i - rendering = %i\n",render,rendering);fflush(stdout);
	usleep(25000);
//	LOG_info("wait_For_Thread IN2: render = %i - rendering = %i\n",render,rendering);fflush(stdout);
	//rendering = 1;
	//signal the core thread to unlock the main thread and render the last frame
	pthread_mutex_lock(&core_mx);
	pthread_cond_signal(&core_rq);
	pthread_mutex_unlock(&core_mx);
//	LOG_info("wait_For_Thread IN3: render = %i - rendering = %i\n",render,rendering);fflush(stdout);
	//wait 25msecs to be sure the flip thread has renderer the last frame
	usleep(25000);
//	LOG_info("wait_For_Thread IN4: render = %i - rendering = %i\n",render,rendering);fflush(stdout);
	int rendering2 = 1000;
	//check if render and renderer are both 0, wait at least 1000msecs the go forward
	while (((render!=0) || (rendering!=0)) && (rendering2>0)) { 
		//LOG_info("rendering in Menu_loop render = %i - rendering = %i\n",render,rendering);fflush(stdout);
		usleep(1000);
		rendering2--; //waiting a bit ensure that menu won't crash even on some cores (i.e. dosbox)
	}
//	LOG_info("wait_For_Thread IN5: render = %i - rendering = %i\n",render,rendering);fflush(stdout);
	rendering = 0;
	render = 0;
	waiting_for_thread_stop = 0;
}


static int setFastForward(int enable) {
	fast_forward = enable;
	return enable;
}

uint32_t buttons = 0; // RETRO_DEVICE_ID_JOYPAD_* buttons
int16_t analogs[16] = {0};
static int ignore_menu = 0;
static void input_poll_callback(void) {
	PAD_poll();

	int show_setting = 0;
	PWR_update(NULL, &show_setting, Menu_beforeSleep, Menu_afterSleep);

	// I _think_ this can stay as is...
	if (PAD_justPressed(BTN_MENU)) {
		ignore_menu = 0;
	}
	if (PAD_isPressed(BTN_MENU) && (PAD_isPressed(BTN_PLUS) || PAD_isPressed(BTN_MINUS))) {
		ignore_menu = 1;
	}
	
	if (PAD_justPressed(BTN_POWER)) {
		if (thread_video) {

		}
	}
	else if (PAD_justReleased(BTN_POWER)) {
		if (thread_video) {
			wait_For_Thread();
		}
		Menu_beforeSleep();
		PWR_powerOff();
	}

	static int toggled_ff_on = 0; // this logic only works because TOGGLE_FF is before HOLD_FF in the menu...
	for (int i=0; i<SHORTCUT_COUNT; i++) {
		ButtonMapping* mapping = &config.shortcuts[i];
		int btn = 1 << mapping->local;
		if (btn==BTN_NONE) continue; // not bound
		if (gamepad_type==0) {
			switch(btn) {
				case BTN_DPAD_UP: 		btn = BTN_UP; break;
				case BTN_DPAD_DOWN: 	btn = BTN_DOWN; break;
				case BTN_DPAD_LEFT: 	btn = BTN_LEFT; break;
				case BTN_DPAD_RIGHT: 	btn = BTN_RIGHT; break;
			}
		}
		if (!mapping->mod || PAD_isPressed(BTN_MENU)) {
			if (i==SHORTCUT_TOGGLE_FF) {
				if (PAD_justPressed(btn)) {
					toggled_ff_on = setFastForward(!fast_forward);
					if (mapping->mod) ignore_menu = 1;
					break;
				}
				else if (PAD_justReleased(btn)) {
					if (mapping->mod) ignore_menu = 1;
					break;
				}
			}
			else if (i==SHORTCUT_HOLD_FF) {
				// don't allow turn off fast_forward with a release of the hold button 
				// if it was initially turned on with the toggle button
				if (PAD_justPressed(btn) || (!toggled_ff_on && PAD_justReleased(btn))) {
					fast_forward = setFastForward(PAD_isPressed(btn));
					if (mapping->mod) ignore_menu = 1; // very unlikely but just in case
				}
			}
			else if (PAD_justPressed(btn)) {
				switch (i) {
					case SHORTCUT_SAVE_STATE: Menu_saveState(); break;
					case SHORTCUT_LOAD_STATE: Menu_loadState(); break;
					case SHORTCUT_RESET_GAME: core.reset(); break;
					case SHORTCUT_SAVE_QUIT:
						Menu_saveState();
						quit = 1;
						break;
					case SHORTCUT_CYCLE_SCALE:
						screen_scaling += 1;
						if (screen_scaling>=SCALE_COUNT) screen_scaling -= SCALE_COUNT;
						Config_syncFrontend(config.frontend.options[FE_OPT_SCALING].key, screen_scaling);
						break;
					case SHORTCUT_CYCLE_EFFECT:
						screen_effect += 1;
						if (screen_effect>=EFFECT_COUNT) screen_effect -= EFFECT_COUNT;
						Config_syncFrontend(config.frontend.options[FE_OPT_EFFECT].key, screen_effect);
						break;
					default: break;
				}
				
				if (mapping->mod) ignore_menu = 1;
			}
		}
	}
	
	if (!ignore_menu && PAD_justReleased(BTN_MENU)) {
		show_menu = 1;
		//firstmenu = 1;

		if (thread_video) {	
			wait_For_Thread();
		}
	}
	
	// TODO: figure out how to ignore button when MENU+button is handled first
	// TODO: array size of LOCAL_ whatever that macro is
	// TODO: then split it into two loops
	// TODO: first check for MENU+button
	// TODO: when found mark button the array
	// TODO: then check for button
	// TODO: only modify if absent from array
	// TODO: the shortcuts loop above should also contribute to the array
	
	buttons = 0;
	analogs[0] = pad.laxis.x;
	analogs[1] = pad.laxis.y; //pad.laxis.y;
	analogs[2] = pad.raxis.x; //pad.raxis.x;
	analogs[3] = pad.raxis.y; //pad.raxis.y;
	analogs[12] =  0;
	analogs[13] =  0;
	analogs[14] =  0;
	analogs[15] =  0;
	for (int i=0; config.controls[i].name; i++) {
		ButtonMapping* mapping = &config.controls[i];
		int btn = 1 << mapping->local;
		int btn_retro = 1 << mapping->retro;
		if (btn==BTN_NONE) continue; // present buttons can still be unbound
		if (PAD_isPressed(btn) && (!mapping->mod || PAD_isPressed(BTN_MENU))) {
			buttons |= 1 << mapping->retro;
			if (mapping->mod) ignore_menu = 1;
			analogs[12] = btn_retro==BTN_L2 ? 0x7fff : 0;
			analogs[13] = btn_retro==BTN_R2 ? 0x7fff : 0;
			analogs[14] = btn_retro==BTN_L3 ? 0x7fff : 0;
			analogs[15] = btn_retro==BTN_R3 ? 0x7fff : 0;
			if (config.controller_map_abxy_to_rstick == 1) {
				//	 if (btn==BTN_LEFT) 	analogs[0]= 0x7fff; 
				//else if (btn==BTN_RIGHT) 	analogs[0]= -0x7fff;
				//else if (btn==BTN_DOWN) 	analogs[1]= 0x7fff;
				//else if (btn==BTN_UP) 		analogs[1]= -0x7fff;
				/*else*/ if (btn==BTN_A)	analogs[2]= 0x7fff; 
				else if (btn==BTN_Y) 		analogs[2]= -0x7fff;
				else if (btn==BTN_B) 		analogs[3]= 0x7fff;
				else if (btn==BTN_X) 		analogs[3]= -0x7fff;
			}
		}
		//  && !PWR_ignoreSettingInput(btn, show_setting)
	}	
	// if (buttons) LOG_info("buttons: %i\n", buttons);
}
static int16_t input_state_callback(unsigned port, unsigned device, unsigned index, unsigned id) {
	// id == RETRO_DEVICE_ID_JOYPAD_MASK or RETRO_DEVICE_ID_JOYPAD_*
	if (port == 0 && device == RETRO_DEVICE_JOYPAD && index == 0) {
		if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return buttons;
		return (buttons >> id) & 1;
	}
	if (port == 0 && device == RETRO_DEVICE_ANALOG && index<2){
	//	LOG_info("Returned analog <2 %d\n", analogs[index*2+id]);
		return analogs[index*2+id];
	}	
	if (port == 0 && device == RETRO_DEVICE_ANALOG && index>1){
	//	LOG_info("Returned analog %d=%d\n", id, analogs[id]);
		return analogs[id];
	}	
	return 0;
}
///////////////////////////////

static void Input_init(const struct retro_input_descriptor *vars) {
	static int input_initialized = 0;
	if (input_initialized) return;

	LOG_info("Input_init\n");
	
	config.controls = core_button_mapping[0].name ? core_button_mapping : default_button_mapping;
	
	puts("---------------------------------");

	const char* core_button_names[RETRO_BUTTON_COUNT] = {0};
	int present[RETRO_BUTTON_COUNT];
	int core_mapped = 0;
	if (vars) {
		core_mapped = 1;
		// identify buttons available in this core
		for (int i=0; vars[i].description; i++) {
			const struct retro_input_descriptor* var = &vars[i];
			if (var->port!=0 || var->device!=RETRO_DEVICE_JOYPAD || var->index!=0) continue;

			// TODO: don't ignore unavailable buttons, just override them to BTN_ID_NONE!
			if (var->id>=RETRO_BUTTON_COUNT) {
				printf("UNAVAILABLE: %s\n", var->description); fflush(stdout);
				continue;
			}
			else {
				printf("PRESENT    : %s\n", var->description); fflush(stdout);
			}
			present[var->id] = 1;
			core_button_names[var->id] = var->description;
		}
	} else {
		LOG_info("Input data actually not received\n");
	}
	
	puts("---------------------------------");

	for (int i=0;default_button_mapping[i].name; i++) {
		ButtonMapping* mapping = &default_button_mapping[i];
		LOG_info("DEFAULT %s (%s): <%s>\n", core_button_names[mapping->retro], mapping->name, (mapping->local==BTN_ID_NONE ? "NONE" : device_button_names[mapping->local]));
		if (core_button_names[mapping->retro]) mapping->name = (char*)core_button_names[mapping->retro];
	}
	
	puts("---------------------------------");

	for (int i=0; config.controls[i].name; i++) {
		ButtonMapping* mapping = &config.controls[i];
		mapping->default_ = mapping->local;

		// ignore mappings that aren't available in this core
		if (core_mapped && !present[mapping->retro]) {
			mapping->ignore = 1;
			continue;
		}
		LOG_info("%s: <%s> (%i:%i)\n", mapping->name, (mapping->local==BTN_ID_NONE ? "NONE" : device_button_names[mapping->local]), mapping->local, mapping->retro);
	}
	
	puts("---------------------------------");
	input_initialized = 1;
}

static bool set_rumble_state(unsigned port, enum retro_rumble_effect effect, uint16_t strength) {
	// TODO: handle other args? not sure I can
	VIB_setStrength(strength);
	return 1;
}


static bool environment_callback(unsigned cmd, void *data) { // copied from picoarch initially
//	LOG_info("Received environment_callback: %i\n", cmd);
	
	switch(cmd) {
	case RETRO_ENVIRONMENT_SET_ROTATION: { // 1 wait to activate until rotation render is working
		int *out = (int *)data;
		if (out)
			renderer.rotategame = *out;
			LOG_info("RETRO_ENVIRONMENT_SET_ROTATION set to %i\n", renderer.rotate);
			//faster for devices with rotated screen?
			renderer.rotate = (renderer.rotategame + PLAT_getScreenRotation(1)) & 3;
		return true;
		break;
	}
	case RETRO_ENVIRONMENT_GET_OVERSCAN: { /* 2 */
		bool *out = (bool *)data;
		if (out)
			*out = true;
		break;
	}
	case RETRO_ENVIRONMENT_GET_CAN_DUPE: { /* 3 */
		bool *out = (bool *)data;
		if (out)
			*out = can_dupe;
		break;
	}
	case RETRO_ENVIRONMENT_SET_MESSAGE: { /* 6 */
		const struct retro_message *message = (const struct retro_message*)data;
		if (message) LOG_info("%s\n", message->msg);
		break;
	}
	case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL: { /* 8 */
		LOG_info("RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL does nothing\n");
		// TODO: used by fceumm at least
	}
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: { /* 9 */
		const char **out = (const char **)data;
		if (out) {
			*out = core.bios_dir;
		}
		
		break;
	}
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: { /* 10 */
		const enum retro_pixel_format *format = (enum retro_pixel_format *)data;
		LOG_info("SET RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: %i\n", *format);
		if (*format == RETRO_PIXEL_FORMAT_0RGB1555) { // TODO: pull from platform.h?		
			downsample = 2; 
			backbuffer.depth = 15;
			return true;
		}		

		if (*format == RETRO_PIXEL_FORMAT_RGB565) { // TODO: pull from platform.h?
			/* 565 is only supported format */
			downsample = 0;
			backbuffer.depth = 16;
			return true;
		} 

		if (*format == RETRO_PIXEL_FORMAT_XRGB8888) { // TODO: pull from platform.h?
			downsample = 1; 
			backbuffer.depth = 32;
			return true;
		}
		return false;
		break;
	}
	case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: { /* 11 */
		//puts("RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS\n");
		Input_init((const struct retro_input_descriptor *)data);
		return false;
	} break;
	case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE: { /* 13 */
		const struct retro_disk_control_callback *var =
			(const struct retro_disk_control_callback *)data;

		if (var) {
			memset(&disk_control, 0, sizeof(struct retro_disk_control_callback));
			memcpy(&disk_control, var, sizeof(struct retro_disk_control_callback));
			Founddiskcontrol = 2;
		}
		break;
	}

	// TODO: this is called whether using variables or options
	case RETRO_ENVIRONMENT_GET_VARIABLE: { /* 15 */
		// puts("RETRO_ENVIRONMENT_GET_VARIABLE");
		struct retro_variable *var = (struct retro_variable *)data;
		if (var && var->key) {
			var->value = OptionList_getOptionValue(&config.core, var->key);
			// printf("\t%s = %s\n", var->key, var->value);
		}
		break;
	}
	// TODO: I think this is where the core reports its variables (the precursor to options)
	// TODO: this is called if RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION sets out to 0
	// TODO: not used by anything yet
	case RETRO_ENVIRONMENT_SET_VARIABLES: { /* 16 */
		// puts("RETRO_ENVIRONMENT_SET_VARIABLES");
		const struct retro_variable *vars = (const struct retro_variable *)data;
		if (vars) {
			OptionList_reset();
			OptionList_vars(vars);
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME: { /* 18 */
		bool flag = *(bool*)data;
		// LOG_info("%i: RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME: %i\n", cmd, flag);
		break;
	}
	case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: { /* 17 */
		bool *out = (bool *)data;
		if (out) {
			*out = config.core.changed;
			config.core.changed = 0;
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK: { /* 21 */
		// LOG_info("%i: RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK\n", cmd);
		break;
	}
	case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK: { /* 22 */
		// LOG_info("%i: RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK\n", cmd);
		break;
	}
	case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE: { /* 23 */
	        struct retro_rumble_interface *iface = (struct retro_rumble_interface*)data;

	        // LOG_info("Setup rumble interface.\n");
	        iface->set_rumble_state = set_rumble_state;
		break;
	}
case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES: {
		unsigned *out = (unsigned *)data;
		if (out)
			*out = (1 << RETRO_DEVICE_JOYPAD) | (1 << RETRO_DEVICE_ANALOG);
		break;
	}
	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: { /* 27 */
		struct retro_log_callback *log_cb = (struct retro_log_callback *)data;
		if (log_cb)
			log_cb->log = (void (*)(enum retro_log_level, const char*, ...))LOG_note; // same difference
		break;
	}
	case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: { /* 31 */
		const char **out = (const char **)data;
		if (out)
			*out = core.saves_dir; // save_dir;
		break;
	}
	case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: { /* 32 */
		struct retro_system_av_info *info = (struct retro_system_av_info *)data;
		if (info) {
		/*	info->timing.fps = core.fps;
			info->timing.sample_rate = core.sample_rate;
			info->geometry.base_width = core.width;
			info->geometry.base_height = core.height;
			info->geometry.max_width = core.width;
			info->geometry.max_height = core.height;
			info->geometry.aspect_ratio = 1.0f; // TODO: aspect ratio*/
			LOG_info("RETRO_ENVIRONMENT_GET_SYSTEM_AV_INFO fps:%f samplerate:%f width:%f height:%f aspectratio:%f\n", info->timing.fps, info->timing.sample_rate, info->geometry.base_width, info->geometry.base_height, info->geometry.aspect_ratio);
		}
		break;
	}

	case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO: { /* 35 */
		// LOG_info("RETRO_ENVIRONMENT_SET_CONTROLLER_INFO\n");
		const struct retro_controller_info *infos = (const struct retro_controller_info *)data;
		if (infos) {
			// TODO: store to gamepad_values/gamepad_labels for gamepad_device
			const struct retro_controller_info *info = &infos[0];
			for (int i=0; i<info->num_types-1; i++) {
				const struct retro_controller_description *type = &info->types[i];
				if (exactMatch((char*)type->desc,"dualshock")) { // currently only enabled for PlayStation
					has_custom_controllers = 1;
					break;
				}
				// printf("\t%i: %s\n", type->id, type->desc);
			}
		}
		fflush(stdout);
		return false; // TODO: tmp
		break;
	}
	// RETRO_ENVIRONMENT_SET_MEMORY_MAPS (36 | RETRO_ENVIRONMENT_EXPERIMENTAL)
	// RETRO_ENVIRONMENT_GET_LANGUAGE 39
	case RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER: { /* (40 | RETRO_ENVIRONMENT_EXPERIMENTAL) */
		// puts("RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER");
		break;
	}
	// RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS (42 | RETRO_ENVIRONMENT_EXPERIMENTAL)
	// RETRO_ENVIRONMENT_GET_VFS_INTERFACE (45 | RETRO_ENVIRONMENT_EXPERIMENTAL)
	// RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE (47 | RETRO_ENVIRONMENT_EXPERIMENTAL)
	// RETRO_ENVIRONMENT_GET_INPUT_BITMASKS (51 | RETRO_ENVIRONMENT_EXPERIMENTAL)
	case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS: { /* 51 | RETRO_ENVIRONMENT_EXPERIMENTAL */
		bool *out = (bool *)data;
		if (out)
			*out = true;
		break;
	}
	case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: { /* 52 */
		// puts("RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION");
		unsigned *out = (unsigned *)data;
		if (out)
			*out = 1;
		break;
	}
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS: { /* 53 */
		// puts("RETRO_ENVIRONMENT_SET_CORE_OPTIONS");
		if (data) {
			OptionList_reset();
			OptionList_init((const struct retro_core_option_definition *)data); 
			Config_readOptions();
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL: { /* 54 */
		// puts("RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL");
		const struct retro_core_options_intl *options = (const struct retro_core_options_intl *)data;
		if (options && options->us) {
			OptionList_reset();
			OptionList_init(options->us);
			Config_readOptions();
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY: { /* 55 */
		// puts("RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY");
		// const struct retro_core_option_display *display = (const struct retro_core_option_display *)data;
	// 	if (display) OptionList_setOptionVisibility(&config.core, display->key, display->visible);
		break;
	}
	case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION: { /* 57 */
		unsigned *out =	(unsigned *)data;
		if (out) *out = 1;
		break;
	}
	case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE: { /* 58 */
		const struct retro_disk_control_ext_callback *var =
			(const struct retro_disk_control_ext_callback *)data;

		if (var) {
			memset(&disk_control_ext, 0, sizeof(struct retro_disk_control_ext_callback));
			memcpy(&disk_control_ext, var, sizeof(struct retro_disk_control_ext_callback));
			Founddiskcontrol = 1;
		}
		break;
	}
	// TODO: RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION 59
	// TODO: used by mgba, (but only during frameskip?)
	
	case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK: { /* 62 */
	 	LOG_info("RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK does nothing\n");
	// 	const struct retro_audio_buffer_status_callback *cb = (const struct retro_audio_buffer_status_callback *)data;
	// 	if (cb) {
	// 		LOG_info("has audo_buffer_status callback\n");
	// 		core.audio_buffer_status = cb->callback;
	// 	} else {
	// 		LOG_info("no audo_buffer_status callback\n");
	// 		core.audio_buffer_status = NULL;
	// 	}
	 	break;
	}
	// TODO: used by mgba, (but only during frameskip?)
	case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY: { /* 63 */
	 	LOG_info("RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY does nothing\n");
	//
	// 	const unsigned *latency_ms = (const unsigned *)data;
	// 	if (latency_ms) {
	// 		unsigned frames = *latency_ms * core.fps / 1000;
	// 		if (frames < 30)
	// 			// audio_buffer_size_override = frames;
	// 			LOG_info("audio_buffer_size_override = %i (unused?)\n", frames);
	// 		else
	// 			LOG_info("Audio buffer change out of range (%d), ignored\n", frames);
	// 	}
	 	break;
	 }

	// TODO: RETRO_ENVIRONMENT_SET_FASTFORWARDING_OVERRIDE 64
	case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE: { /* 65 */
		// const struct retro_system_content_info_override* info = (const struct retro_system_content_info_override* )data;
		// if (info) LOG_info("has overrides");
		break;
	}
	// RETRO_ENVIRONMENT_GET_GAME_INFO_EXT 66
	// TODO: RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK 69
	// used by fceumm
	// TODO: used by gambatte for L/R palette switching (seems like it needs to return true even if data is NULL to indicate support)
	case RETRO_ENVIRONMENT_SET_VARIABLE: {
		// puts("RETRO_ENVIRONMENT_SET_VARIABLE");
		const struct retro_variable *var = (const struct retro_variable *)data;
		if (var && var->key) {
			// printf("\t%s = %s\n", var->key, var->value);
			OptionList_setOptionValue(&config.core, var->key, var->value);
			break;
		}

		int *out = (int *)data;
		if (out) *out = 1;
		
		break;
	}
	case RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT: {
		//puts("RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT"); fflush(stdout);
		int result = RETRO_SAVESTATE_CONTEXT_NORMAL;
		if (data)
            *(int*)data = result;
			break;
	}
	
	// unused
	// case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK: {
	// 	puts("RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK"); fflush(stdout);
	// 	break;
	// }
	// case RETRO_ENVIRONMENT_GET_THROTTLE_STATE: {
	// 	puts("RETRO_ENVIRONMENT_GET_THROTTLE_STATE"); fflush(stdout);
	// 	break;
	// }
	// case RETRO_ENVIRONMENT_GET_FASTFORWARDING: {
	// 	puts("RETRO_ENVIRONMENT_GET_FASTFORWARDING"); fflush(stdout);
	// 	break;
	// };
	
	default:
		LOG_debug("Unsupported environment cmd: %u\n", cmd);fflush(stdout);
		return false;
	}
	return true;
}

///////////////////////////////

// TODO: this is a dumb API
SDL_Surface* digits;
#define DIGIT_WIDTH 9
#define DIGIT_HEIGHT 8
#define DIGIT_TRACKING -2
enum {
	DIGIT_SLASH = 10,
	DIGIT_DOT,
	DIGIT_PERCENT,
	DIGIT_X,
	DIGIT_OP, // (
	DIGIT_CP, // )
	DIGIT_COUNT,
};
#define DIGIT_SPACE DIGIT_COUNT
static void MSG_init(void) {
	digits = SDL_CreateRGBSurface(SDL_SWSURFACE,SCALE2(DIGIT_WIDTH*DIGIT_COUNT,DIGIT_HEIGHT),FIXED_DEPTH, 0,0,0,0);
	SDL_FillRect(digits, NULL, RGB_BLACK);
	
	SDL_Surface* digit;
	char* chars[] = { "0","1","2","3","4","5","6","7","8","9","/",".","%","x","(",")", NULL };
	char* c;
	int i = 0;
	while ((c = chars[i])) {
		digit = TTF_RenderUTF8_Blended(font.tiny, c, COLOR_WHITE);
		SDL_BlitSurface(digit, NULL, digits, &(SDL_Rect){ (i * SCALE1(DIGIT_WIDTH)) + (SCALE1(DIGIT_WIDTH) - digit->w)/2, (SCALE1(DIGIT_HEIGHT) - digit->h)/2});
		SDL_FreeSurface(digit);
		i += 1;
	}
}
static int MSG_blitChar(int n, int x, int y) {
	if (n!=DIGIT_SPACE) SDL_BlitSurface(digits, &(SDL_Rect){n*SCALE1(DIGIT_WIDTH),0,SCALE2(DIGIT_WIDTH,DIGIT_HEIGHT)}, screen, &(SDL_Rect){x,y});
	return x + SCALE1(DIGIT_WIDTH + DIGIT_TRACKING);
}
static int MSG_blitInt(int num, int x, int y) {
	int i = num;
	int n;
	
	if (i > 999) {
		n = i / 1000;
		i -= n * 1000;
		x = MSG_blitChar(n,x,y);
	}
	if (i > 99) {
		n = i / 100;
		i -= n * 100;
		x = MSG_blitChar(n,x,y);
	}
	else if (num>99) {
		x = MSG_blitChar(0,x,y);
	}
	if (i > 9) {
		n = i / 10;
		i -= n * 10;
		x = MSG_blitChar(n,x,y);
	}
	else if (num>9) {
		x = MSG_blitChar(0,x,y);
	}
	
	n = i;
	x = MSG_blitChar(n,x,y);
	
	return x;
}
static int MSG_blitDouble(double num, int x, int y) {
	int i = num;
	int r = (num-i) * 10;
	int n;
	
	x = MSG_blitInt(i, x,y);

	n = DIGIT_DOT;
	x = MSG_blitChar(n,x,y);
	
	n = r;
	x = MSG_blitChar(n,x,y);
	return x;
}
static void MSG_quit(void) {
	SDL_FreeSurface(digits);
}

///////////////////////////////

/*
static const char* bitmap_font[] = {
	['0'] = 
		" 111 "
		"1   1"
		"1   1"
		"1  11"
		"1 1 1"
		"11  1"
		"1   1"
		"1   1"
		" 111 ",
	['1'] =
		"   1 "
		" 111 "
		"   1 "
		"   1 "
		"   1 "
		"   1 "
		"   1 "
		"   1 "
		"   1 ",
	['2'] =
		" 111 "
		"1   1"
		"    1"
		"   1 "
		"  1  "
		" 1   "
		"1    "
		"1    "
		"11111",
	['3'] =
		" 111 "
		"1   1"
		"    1"
		"    1"
		" 111 "
		"    1"
		"    1"
		"1   1"
		" 111 ",
	['4'] =
		"1   1"
		"1   1"
		"1   1"
		"1   1"
		"1   1"
		"1   1"
		"11111"
		"    1"
		"    1",
	['5'] =
		"11111"
		"1    "
		"1    "
		"1111 "
		"    1"
		"    1"
		"    1"
		"1   1"
		" 111 ",
	['6'] =
		" 111 "
		"1    "
		"1    "
		"1111 "
		"1   1"
		"1   1"
		"1   1"
		"1   1"
		" 111 ",
	['7'] =
		"11111"
		"    1"
		"    1"
		"   1 "
		"  1  "
		"  1  "
		"  1  "
		"  1  "
		"  1  ",
	['8'] =
		" 111 "
		"1   1"
		"1   1"
		"1   1"
		" 111 "
		"1   1"
		"1   1"
		"1   1"
		" 111 ",
	['9'] =
		" 111 "
		"1   1"
		"1   1"
		"1   1"
		"1   1"
		" 1111"
		"    1"
		"    1"
		" 111 ",
	['.'] = 
		"     "
		"     "
		"     "
		"     "
		"     "
		"     "
		"     "
		" 11  "
		" 11  ",
	[','] = 
		"     "
		"     "
		"     "
		"     "
		"     "
		"     "
		"  1  "
		"  1  "
		" 1   ",
	[' '] = 
		"     "
		"     "
		"     "
		"     "
		"     "
		"     "
		"     "
		"     "
		"     ",
	['('] = 
		"   1 "
		"  1  "
		" 1   "
		" 1   "
		" 1   "
		" 1   "
		" 1   "
		"  1  "
		"   1 ",
	[')'] = 
		" 1   "
		"  1  "
		"   1 "
		"   1 "
		"   1 "
		"   1 "
		"   1 "
		"  1  "
		" 1   ",
	['/'] = 
		"   1 "
		"   1 "
		"   1 "
		"  1  "
		"  1  "
		"  1  "
		" 1   "
		" 1   "
		" 1   ",
	['x'] = 
		"     "
		"     "
		"1   1"
		"1   1"
		" 1 1 "
		"  1  "
		" 1 1 "
		"1   1"
		"1   1",
	['%'] = 
		" 1   "
		"1 1  "
		"1 1 1"
		" 1 1 "
		"  1  "
		" 1 1 "
		"1 1 1"
		"  1 1"
		"   1 ",
	['T'] = 
		"11111"
		"  1  "
		"  1  "
		"  1  "
		"  1  "
		"  1  "
		"  1  "
		"  1  "
		"  1  ",	
};

static void blitBitmapText(char* text, int ox, int oy, uint16_t* data, int stride, int width, int height) {
	#define CHAR_WIDTH 5
	#define CHAR_HEIGHT 9
	#define LETTERSPACING 1
*/
///////////////////////////////

static const char* bitmap_font[] = {
	['0'] = 
		"  111111  "
		" 11111111 "
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		" 11111111 "
		"  111111  ",
	['1'] =
		"      11  "
		"    1111  "
		"  111111  "
		" 1111111  "
		"      11  "
		"      11  "
		"      11  "
		"      11  "
		"      11  "
		"      11  "
		"      11  "
		"      11  "
		"      11  "
		"      11  "
		"      11  "
		"      11  ",		
	['2'] =
		"  111111  "
		" 11111111 "
		"11      11"
		"11      11"
		"       11 "
		"      11  "
		"     11   "
		"    11    "
		"   11     "
		"  11      "
		" 11       "
		"11        "
		"11        "
		"11        "
		"1111111111"
		"1111111111",
	['3'] =
		"  111111  "
		" 11111111 "
		"11      11"
		"        11"
		"        11"
		"        11"
		"        11"
		" 11111111 "
		" 11111111 "
		"        11"
		"        11"
		"        11"
		"        11"
		"11      11"
		" 11111111 "
		"  111111  ",
	['4'] =
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"1111111111"
		" 111111111"
		"        11"
		"        11"
		"        11"
		"        11"
		"        11"
		"        11",
	['5'] =
		"1111111111"
		"1111111111"
		"11        "
		"11        "
		"11        "
		"11        "
		"111111111 "
		"1111111111"
		"        11"
		"        11"
		"        11"
		"        11"
		"        11"
		"11      11"
		"1111111111"
		" 11111111 ",
	['6'] =
		" 11111111 "
		"1111111111"
		"11        "
		"11        "
		"11        "
		"11        "
		"11        "
		"111111111 "
		"1111111111"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"1111111111"
		" 11111111 ",
	['7'] =
		"1111111111"
		"1111111111"
		"        11"
		"        11"
		"        11"
		"        11"
		"       11 "
		"      11  "
		"     11   "
		"    11    "
		"    11    "
		"    11    "
		"    11    "
		"    11    "
		"    11    "
		"    11    ",
	['8'] =
		"  111111  "
		" 11111111 "
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		" 11111111 "
		" 11111111 "
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		" 11111111 "
		"  111111  ",
	['9'] =
		"  111111  "
		" 11111111 "
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		" 11111111 "
		"  1111111 "
		"        11"
		"        11"
		"        11"
		"        11"
		"        11"
		" 11111111 "
		"  111111  ",
	['.'] = 
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"  1111	   "
		"  1111    "
		"  1111    ",
	[','] = 
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"    111   "
		"   111	   "
		"  111     "
		" 111      ",
	[' '] = 
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          "
		"          ",
	['('] = 
		"       11 "
		"      11  "
		"     11   "
		"    11    "
		"   11     "
		"  11      "
		" 11       "
		" 11       "
		" 11       "
		" 11       "
		"  11      "
		"   11     "
		"    11    "
		"     11   "
		"      11  "
		"       11 ",
	[')'] = 
		" 11       "
		"  11      "
		"   11     "
		"    11    "
		"     11   "
		"      11  "
		"       11 "
		"       11 "
		"       11 "
		"       11 "
		"      11  "
		"     11   "
		"    11    "
		"   11     "
		"  11      "
		" 11       ",
	['/'] = 
		"       11 "
		"       11 "
		"      11  "
		"      11  "
		"     11   "
		"     11   "
		"    11    "
		"    11    "
		"   11     "
		"   11     "
		"  11      "
		"  11      "
		" 11       "
		" 11       "
		"11        "
		"11        ",
	['x'] = 
		"          "
		"          "
		"          "
		"          "
		"11      11"
		"11      11"
		" 11    11 "
		"  11  11  "
		"   1111   "
		"    11    "
		"    11    "
		"   1111   "
		"  11  11  "
		" 11    11 "
		"11      11"
		"11      11",
	['%'] = 
	    "          "
		"  11      "
		" 1111     "
		"11  11 11 "
		" 1111  11 "
		"  11   11 "
		"      11  "
		"     11   "
		"    11    "
		"    11    "
		"   11     "
		"  11  11  "
		" 11  1111 "
		" 11 11  11"
		"     1111 "
		"      11  ",
	['T'] = 
		"1111111111"
		"1111111111"
		"    11    "
		"    11    "
		"    11    "
		"    11    "
		"    11    "
		"    11    "
		"    11    "
		"    11    "
		"    11    "
		"    11    "
		"    11    "
		"    11    "
		"    11    "
		"    11    ",	
	['F'] = 
		"1111111111"
		"1111111111"
		"11        "
		"11        "
		"11        "
		"11        "
		"11        "
		"1111111   "
		"1111111   "
		"11        "
		"11        "
		"11        "
		"11        "
		"11        "
		"11        "
		"11        ",	
	['E'] = 
		"1111111111"
		"1111111111"
		"11        "
		"11        "
		"11        "
		"11        "
		"11        "
		"1111111   "
		"1111111   "
		"11        "
		"11        "
		"11        "
		"11        "
		"11        "
		"1111111111"
		"1111111111",
	['A'] = 
		"   1111   "
		"  111111  "
		"111    111"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"1111111111"
		"1111111111"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11",
	['N'] = 
		"11      11"
		"11      11"
		"111     11"
		"1111    11"
		"11 11   11"
		"11  11  11"
		"11   11 11"
		"11    1111"
		"11     111"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11"
		"11      11",		
};

static void blitBitmapText(char* text, int ox, int oy, uint16_t* data, int stride, int width, int height) {
	#define _CHAR_WIDTH 10
	#define _CHAR_HEIGHT 16
	#define _LETTERSPACING 2


	int len = strlen(text);
	int w = ((_CHAR_WIDTH+_LETTERSPACING)*len)-1;
	int h = _CHAR_HEIGHT;

	if (ox<0) ox = width-w+ox;
	if (oy<0) oy = height-h+oy;


	data += oy * stride + ox;
	for (int y=0; y<_CHAR_HEIGHT; y++) {
		uint16_t* row = data + y * stride;
		memset(row, 0, w*2);
		for (int i=0; i<len; i++) {
			const char* c = bitmap_font[text[i]];
			for (int x=0; x<_CHAR_WIDTH; x++) {
				int j = y * _CHAR_WIDTH + x;
				if (c[j]=='1') *row = 0xffff;
				row++;
			}
			row += _LETTERSPACING;
		}
	}
}

///////////////////////////////

//static int cpu_ticks = 0;
static int fps_ticks = 0;
static int fps2_ticks = 0;
static int use_ticks = 0;
static double fps_double = 0;
static double fps2_double = 0;
//static double cpu_double = 0;
static double use_double = 0;
static char cpuload[128];
		
static uint32_t sec_start = 0;

#ifdef USES_SWSCALER
	static int fit = 1;
#else
	static int fit = 0;
#endif	

void video_refresh_callback_rotate(int rotation, void *data, unsigned width, unsigned height, size_t pitch) {
	//int x, y, thispitch;
	//struct timeval now, now1, now2, now3, now4, now5, now6;
//	gettimeofday(&now,NULL);
	rotateIMG(data, renderer.src_surface->pixels, rotation, width, height, pitch);
	//gettimeofday(&now1,NULL);	
	if (renderer.true_w != width || renderer.true_h != height || renderer.true_p != pitch){
		LOG_info("Game Rotation of %ddeg calls RESIZE %ix%i_%i\n", rotation*90, width, height, pitch);fflush(stdout);
		renderer.resize = 1;
		renderer.true_w = width;
		renderer.true_h = height;
		renderer.true_p = pitch;	
		renderer.src_surface->w=width * ((3 - renderer.rotate) % 2) + height * (renderer.rotate % 2);
		renderer.src_surface->h=width * (renderer.rotate % 2) + height * ((3-renderer.rotate) % 2);
		renderer.src_surface->pitch=renderer.src_surface->w*2;	
	} 	
//	LOG_info("Game Rotation of %d %dx%d_%d - rotate %dusec - freesurface %dusec - createsurface %dusec - pixman %dusec\n", rotation*90, width, height, pitch, now1.tv_usec - now.tv_usec, now1.tv_usec - now.tv_usec ,now2.tv_usec - now1.tv_usec, now3.tv_usec - now2.tv_usec);fflush(stdout);
}

char resizemode;
SDL_Rect video_refresh_callback_resize_native(void) {
	LOG_info("RESIZE NATIVE\n");fflush(stdout);
	//integer scaling
	SDL_Rect retvalue;
	int scale = 6;
	int max_xscale = 6;
	while (max_xscale * renderer.src_surface->w > GAME_WIDTH) max_xscale--;
	int max_yscale = 6;
	while (max_yscale * renderer.src_surface->h > GAME_HEIGHT) max_yscale--;
	scale = MIN(max_xscale, max_yscale);
	//scale = MIN(scale, screen_max_scale+1);
	LOG_info("width %d height %d / screenwidth %d screenheight %d -> scale %d max_xscale %d max_yscale %d\n", renderer.src_surface->w, renderer.src_surface->h, GAME_WIDTH, GAME_HEIGHT, scale, max_xscale, max_yscale);fflush(stdout);
	//calculate offsets
	
	int dst_w = renderer.src_surface->w * scale;
	int dst_h = renderer.src_surface->h * scale;
	int dst_x = (GAME_WIDTH - dst_w) / 2;
	int dst_y = (GAME_HEIGHT - dst_h) / 2;
	retvalue.x = dst_x;
	retvalue.y = dst_y;
	retvalue.w = dst_w;
	retvalue.h = dst_h;
	LOG_info("Dst_w %d dst_h %d dst_x %d dst_y %d\n", dst_w, dst_h, dst_x, dst_y);fflush(stdout);
	return retvalue;
}

SDL_Rect video_refresh_callback_resize_aspect(void) {
	LOG_info("RESIZE ASPECT\n");fflush(stdout);
	//maximum scaling keeping original aspect ratio 
	SDL_Rect retvalue;
	int dst_x,dst_y,dst_w,dst_h;
	double sysaspect = 1.0 * GAME_WIDTH / GAME_HEIGHT;
	double aspect = renderer.src_surface->w / (double)renderer.src_surface->h;
	if (aspect >= sysaspect) {
		//landscape or 1:1 -> width limited
		dst_w = GAME_WIDTH;
		double _dst_h = 1.0 * dst_w / aspect;
		dst_h = (int)_dst_h;
		dst_x = 0;
		dst_y = (GAME_HEIGHT - dst_h) / 2;
	} else {
		//portrait -> height limited		
		dst_h = GAME_HEIGHT;
		double _dst_w = 1.0 * dst_h * aspect;
		dst_w = (int)_dst_w;
		dst_x = (GAME_WIDTH - dst_w) / 2;
		dst_y = 0;		
	}
	retvalue.x = dst_x;
	retvalue.y = dst_y;
	retvalue.w = dst_w;
	retvalue.h = dst_h;
	LOG_info("Sysaspect %f aspect %f dst_w %d dst_h %d dst_x %d dst_y %d\n", sysaspect, aspect,dst_w, dst_h, dst_x, dst_y);fflush(stdout);
	return retvalue;
	//calculate offsets
}

SDL_Rect video_refresh_callback_resize_extended(void) {
	LOG_info("RESIZE EXTENDED\n");fflush(stdout);
	//maximum scaling keeping original aspect ratio then expand the shortest dimensione to fill better the screen
	// for games that have a too narrow or too low aspect ratio (i.e. 1941 arcade) 
	SDL_Rect retvalue;
	int dst_x,dst_y,dst_w,dst_h;
	double sysaspect = 1.0 * GAME_WIDTH / GAME_HEIGHT;
	double aspect = renderer.src_surface->w / (double)renderer.src_surface->h;
	if (aspect >= sysaspect) {
		//landscape or 1:1 -> width limited
		dst_w = GAME_WIDTH;
		double _dst_h = 1.0 * dst_w / aspect;
		dst_h = (int)_dst_h;
		dst_x = 0;
		dst_y = (GAME_HEIGHT - dst_h) / 2;
		if (dst_y > 2){
			dst_h = dst_h + dst_y;
			dst_y = (GAME_HEIGHT - dst_h) / 2;
		}
	} else {
		//portrait -> height limited		
		dst_h = GAME_HEIGHT;
		double _dst_w = 1.0 * dst_h * aspect;
		dst_w = (int)_dst_w;
		dst_x = (GAME_WIDTH - dst_w) / 2;
		if (dst_x > 2){
			dst_w = dst_w + dst_x;
			dst_x = (GAME_WIDTH - dst_w) / 2;
		}
		dst_y = 0;		
	}
	retvalue.x = dst_x;
	retvalue.y = dst_y;
	retvalue.w = dst_w;
	retvalue.h = dst_h;
	LOG_info("Sysaspect %f aspect %f dst_w %d dst_h %d dst_x %d dst_y %d\n", sysaspect, aspect,dst_w, dst_h, dst_x, dst_y);fflush(stdout);
	return retvalue;
	//calculate offsets
}

void video_refresh_callback_resize(void) {
	LOG_info("RESIZE IN\n");fflush(stdout);
	uint32_t now = SDL_GetTicks();
	LOG_info(" VideoResize IN %d %d %d ABS:%d\n", renderer.src_surface->w, renderer.src_surface->h, renderer.src_surface->pitch, now);fflush(stdout);
	SDL_Rect targetarea = {0,0,GAME_WIDTH,GAME_HEIGHT};
	switch(screen_scaling) {
		case SCALE_ASPECT: { 
							targetarea = video_refresh_callback_resize_aspect();
							resizemode = 'A';
							break;
							};
		case SCALE_EXTENDED: { 
							targetarea = video_refresh_callback_resize_extended();
							resizemode = 'E';
							break;
							};
		case SCALE_NATIVE: { 
							targetarea = video_refresh_callback_resize_native();
							resizemode = 'N';
							break;
							};
		case SCALE_FULLSCREEN: { 
								LOG_info("RESIZE FULLSCREEN\n");fflush(stdout);
								resizemode = 'F';
								//targetarea = video_refresh_callback_resize_fullscreen(data);
								break;
							};
		default: { 
					LOG_info("RESIZE FULLSCREEN UNDEF\n");fflush(stdout);
					resizemode = 'F';
								//targetarea = video_refresh_callback_resize_fullscreen(data);
					break;
				}	
	}
	renderer.scale=1.0 * targetarea.w / (double)renderer.src_surface->w;
	renderer.aspect=1.0 * targetarea.w / (double)targetarea.h;
	renderer.src_w = renderer.src_surface->w;
	renderer.src_h = renderer.src_surface->h;
	renderer.src_p = renderer.src_surface->pitch;
	renderer.dst_w = targetarea.w;
	renderer.dst_h = targetarea.h;
	renderer.dst_x = targetarea.x;
	renderer.dst_y = targetarea.y;	
	renderer.dst_p = renderer.dst_w * FIXED_DEPTH;	
	renderer.resize = 0;
	if (renderer.screenscaling != screen_scaling) {
		renderer.resize = 1;
		renderer.screenscaling = screen_scaling;
	} 
	
	LOG_info("VideoResize OUT %d %d %d %d TOOK %dms ABS:%d\n", renderer.dst_w , renderer.dst_h, renderer.dst_x , renderer.dst_y ,  SDL_GetTicks()-now, SDL_GetTicks());fflush(stdout);
}


static int use_core_fps = 1;
static uint32_t last_flip_time = 0;
static void video_refresh_callback_main(const void *data, unsigned width, unsigned height, size_t pitch) {
//	uint32_t now = SDL_GetTicks();
 	if (!data) return;
	if (!thread_video) rendering = 0;
	fps_ticks += 1;
//	LOG_info("Video_refresh_callback_main IN ABS:%d\n", SDL_GetTicks());fflush(stdout);
	// 10 seems to be the sweet spot that allows 2x in NES and SNES and 8x in GB at 60fps
	// 14 will let GB hit 10x but NES and SNES will drop to 1.5x at 30fps (not sure why)
	// but 10 hurts PS...
	// TODO: 10 was based on rg35xx, probably different results on other supported platforms
	if (fast_forward && SDL_GetTicks()-last_flip_time<17) return;
	
	// FFVII menus 
	// 16: 30/200
	// 15: 30/180
	// 14: 45/180
	// 12: 30/150
	// 10: 30/120 (optimize text off has no effect)
	//  8: 60/210 (with optimize text off)
	// you can squeeze more out of every console by turning prevent tearing off
	// eg. PS@10 60/240
	
	//fps_ticks += 1;
	//LOG_info("%05d: fps_ticks incremented!\n", fps_ticks);fflush(stdout);
	//if (downsample) pitch /= 2; // everything expects 16 but we're downsampling from 32

	renderer.src = (void*)data;
	struct timeval now, now1, now2, now3, now4, now5, now6;
	gettimeofday(&now,NULL);
	
	//ok here I have renderer.src that points to the frame data to display, it is in RGB565 format
	//quite sure it is the right moment to rotate it, before calling the selected scaler.
	//video_refresh_callback_rotate((renderer.rotate + PLAT_getScreenRotation()) & 3, renderer.src, width, height, pitch);
	video_refresh_callback_rotate(renderer.rotate, (uint16_t*)data, width, height, pitch);
	gettimeofday(&now1,NULL);
	if (renderer.dst_p==0 || renderer.resize ==1) {
		video_refresh_callback_resize();
		GFX_clearAll();	
	}
	gettimeofday(&now2,NULL);
	GFX_blitRenderer(&renderer);
	gettimeofday(&now3,NULL);
	// debug
	if (show_debug) {
		char debug_text[128];
		sprintf(debug_text, "%ix%i %c%.1fx %d", renderer.src_w,renderer.src_h, resizemode, renderer.scale, backbuffer.depth);
		blitBitmapText(debug_text,renderer.dst_x+2,renderer.dst_y+2,screengame->pixels,screengame->pitch/2, renderer.dst_w+renderer.dst_x,renderer.dst_h+renderer.dst_y);

		sprintf(debug_text, "%i,%i %ix%i", renderer.dst_x,renderer.dst_y, renderer.dst_w,renderer.dst_h);
		blitBitmapText(debug_text,-2,renderer.dst_y+2,screengame->pixels,screengame->pitch/2, renderer.dst_w+renderer.dst_x,renderer.dst_h+renderer.dst_y);

		sprintf(debug_text, "%.0f/%.0f", fps_double,fps2_double);
//#ifdef M21
#if 1
		blitBitmapText(debug_text,renderer.dst_x+2,-2,screengame->pixels,screengame->pitch/2, renderer.dst_w+renderer.dst_x,renderer.dst_h+renderer.dst_y);
#else
		blitBitmapText(debug_text,2,-21,screen->pixels,screen->pitch/2, screen->w,screen->h);

		sprintf(debug_text, "%s", cpuload);
		blitBitmapText(debug_text,2,-2,screen->pixels,screen->pitch/2, screen->w,screen->h);
#endif
		sprintf(debug_text, "%s %i", effect_str,renderer.rotategame*90);
		blitBitmapText(debug_text,-2,-2,screengame->pixels,screengame->pitch/2, renderer.dst_w+renderer.dst_x,renderer.dst_h+renderer.dst_y);
	}

	gettimeofday(&now4,NULL);
	if (use_core_fps) {
		GFX_flip_fixed_rate(screen, core.fps);
	}
	else {
	  	GFX_flip(screen);
	}
	gettimeofday(&now5,NULL);
	GFX_pan();
	gettimeofday(&now6,NULL);
	last_flip_time = SDL_GetTicks();
	if (!thread_video) render = 0;

	//LOG_info("videorefreshcallbackmain took %dusec - rotate took %dusec - resize took %dusec - flip took %dusec - wait pan for %dusec\n", now6.tv_usec - now.tv_usec, now1.tv_usec - now.tv_usec, now3.tv_usec - now2.tv_usec, now5.tv_usec - now4.tv_usec, now6.tv_usec - now5.tv_usec);

}

//int storage_audio_timing[60*60*10][4] = {-1};
//static uint32_t _x = 0;
static uint32_t currentframenum = 0;
static uint32_t firstframe = 1;
static uint32_t last_callback_time = 0;
static unsigned long long last_video_time = 0;
static void video_refresh_callback(const void *data, unsigned width, unsigned height, size_t pitch) {

//	struct timeval now;
//	gettimeofday(&now, NULL);
//	unsigned long long now_usec = now.tv_sec * 1000000 + now.tv_usec;
//	LOG_info("FRAME: %d (Video)             %05lluusec elasped, %lluusec absolute\n\n", currentframenum++,now_usec - last_video_time, now_usec);fflush(stdout); //LOG_info("Audio sample batch callback %d frames\n", frames);fflush(stdout);
//	last_video_time = now_usec;

	int callback_time = SDL_GetTicks();
//	storage_audio_timing[_x][2] = callback_time;
	//LOG_info("video_refresh_callback IN elapsed %lums width:%i height:%i pitch:%i ABS:%i\n", callback_time-last_callback_time ,width,height,pitch, callback_time);fflush(stdout);
	if (!data || show_menu) {
		//LOG_info("Empty Frame\n");fflush(stdout);
		return; //frameskip activated?
	}
//	LOG_info("Video_refresh_callback A\n");fflush(stdout);
	if (!backbuffer.pixels) {
		//LOG_info("backbuffer.pixels not yet ready\n");fflush(stdout);	
		return;
	}

//	if (backbuffer.size != pitch * height) {
//		realloc(backbuffer.pixels, pitch * height);	
//	}

	uint16_t *output = backbuffer.pixels;
	//struct timeval now, now2;
	//gettimeofday(&now,NULL);
	if (downsample == 0) {
		//as is
		//this is quite faster than memcpy, in the worst case of 720x720 it takes 0.6msec while memcpy takes 1.5msec.
		pixman_composite_src_0565_0565_asm_neon(pitch/2, height, output, pitch/2, (uint16_t *)data, pitch/2);
	}

	if (downsample == 1) {
		// from 8888 to 565
		pitch /= 2;	
		// using pixman_composite allows to save up to 3msec per frame (pixman requires 2.7msec@720x720, while cycle above takes 5.8msecs)
		pixman_composite_src_8888_0565_asm_neon(pitch/2, height, output, pitch/2, (uint32_t *)data, pitch/2);
	} 

	if (downsample == 2) {
		//from 1555 to 565
	//	uint32_t counter = pitch*height/2;
	//	const uint16_t *input16 = (uint16_t *)data;		
	//	while (counter--) {
	//		uint16_t pixel = *input16++;
	//		uint16_t r = (pixel & 0x7C00) << 1;  // da bit 10-14 → bit 11-15
	//		uint16_t g = (pixel & 0x03E0) << 1;  // da bit 5-9 → bit 5-10
	//		uint16_t b = (pixel & 0x001F);       // da bit 0-4 → bit 0-4
	//		*output++ = r  | g  | b;
	//	}
		pixman_composite_src_1555_0565_asm_neon(pitch/2, height, output,pitch/2, (uint16_t *)data, pitch/2);
	}
	//gettimeofday(&now2,NULL);
	//LOG_info("video_refresh_callback copy to backbuffer %d took %dusec %dx%d_%d\n", downsample, now2.tv_usec - now.tv_usec, width, height, pitch);fflush(stdout);
	backbuffer.w = width;
	backbuffer.h = height;
	backbuffer.pitch = pitch;
	backbuffer.size = pitch * height;
	//LOG_info("Video_refresh_callback MID\n");fflush(stdout);
	if (thread_video) {		
		pthread_mutex_lock(&flip_mx);
		fps2_ticks++;
		pthread_mutex_unlock(&flip_mx);
		
		pthread_mutex_lock(&core_mx);
		rendering = 1;
		pthread_cond_signal(&core_rq);		
		pthread_mutex_unlock(&core_mx);
		//PLAT_vsync(0);
		if (firstframe) {
			firstframe = 0;
			PLAT_vsync(0);
		}
			//LOG_info("Video_refresh_callback first frame\n");fflush(stdout);
	}
	else {
		fps2_ticks++;
		rendering = 1;
		video_refresh_callback_main(backbuffer.pixels,width,height,pitch);	
	}
	//uint32_t cur = SDL_GetTicks() - last_callback_time;
	//if ( cur < 16) SDL_Delay(16 - cur);
	//LOG_info("Video_refresh_callback OUT: waitforthread:%i thread_video:%i config_done = %i %ix%i_%i took:%lums ABS %lu tmptime=%d\n",waitforthread,thread_video,config_load_done,width,height,pitch, SDL_GetTicks()-callback_time, callback_time,cur);fflush(stdout);
	

//	LOG_info("videorefreshcallback took %dmsec elapsed %dmsec\n", SDL_GetTicks() - callback_time, callback_time - last_callback_time);fflush(stdout);
	last_callback_time = callback_time;
}
///////////////////////////////
//array to store the audio timing profile, each second is 60frames, so I'll record the first 10 minutes of every game 
// each record contains 2 values, the first is the entry time in msec, the second is the exit time in msec

// NOTE: sound must be disabled for fast forward to work...
static void audio_sample_callback(int16_t left, int16_t right) {
	if (fast_forward) return;
	SND_Frame frame;
	frame.left = left;
	frame.right = right;
#ifdef M21
	if (!GetVolume()) {frame.left = 0; frame.right = 0; } 
#endif
	if (use_core_fps) {
		SND_batchSamples_fixed_rate(&frame, 1);
	}
	else {
		SND_batchSamples(&frame, 1);
	}
}

struct timeval tv;
static uint64_t last_audio_time = 0;
static uint64_t microsvalue = 0;
static uint32_t frame_period_usecs = 0;
static size_t audio_sample_batch_callback(const int16_t *data, size_t frames) { 
	if (fast_forward) return frames;
//	struct timeval now;
//	gettimeofday(&now, NULL);
//	unsigned long long now_usec = now.tv_sec * 1000000 + now.tv_usec;
//	LOG_info("FRAME: %d (Audio )%d frames, %05lluusec elasped, %lluusec absolute\n", currentframenum, frames, now_usec - last_audio_time, now_usec);fflush(stdout); //LOG_info("Audio sample batch callback %d frames\n", frames);fflush(stdout);
//	last_audio_time = now_usec;

//	storage_audio_timing[_x][0] = SDL_GetTicks();
//	storage_audio_timing[_x][3] = frames;
	int retvalue;
	SND_Frame *tmpdata = (SND_Frame *)data;
#ifdef M21	
	if (!GetVolume()) { tmpdata = (SND_Frame*)mutedaudiodata; }
#endif

if (use_core_fps) {
	retvalue = SND_batchSamples_fixed_rate((const SND_Frame*)tmpdata, frames);
}
else {
	retvalue = SND_batchSamples((const SND_Frame*)tmpdata, frames);
}

//	storage_audio_timing[_x++][1] = SDL_GetTicks();
//	if (_x == 60*60*10) _x=0;
//	gettimeofday(&tv,NULL);
//	microsvalue = 1000000 * tv.tv_sec + tv.tv_usec;
//	if (last_audio_time == 0) last_audio_time = microsvalue;
	//LOG_info("audio_sample_batch_callback %jd/%jd/%i usec\n",(intmax_t)last_audio_time,microsvalue,frame_period_usecs);fflush(stdout);

//	while ((microsvalue - last_audio_time) < frame_period_usecs) 
//		{
//			gettimeofday(&tv,NULL);
//			microsvalue = 1000000 * tv.tv_sec + tv.tv_usec;
//		}

//	last_audio_time = microsvalue;

//	gettimeofday(&now, NULL);
//	now_usec = now.tv_sec * 1000000 + now.tv_usec;
//	LOG_info("FRAME: %d (Audio) %d frames, %05lluusec elasped, %lluusec absolute RETURN\n", currentframenum, frames, now_usec - last_audio_time, now_usec);fflush(stdout);
	return retvalue;
};

//store the array to a file at exit of the game to avoid interference 
/*void save_storage_audio_timing(void) {

	LOG_info("save_storage_audio_timing writing to file START _x = %i\n",_x);
	int i = 0;
	FILE* file = fopen( SDCARD_PATH "/storage_audio_timing.txt", "w");
	for (i = 1; i < _x; i++) {
		fprintf(file, "%05d VideoFrame IN = %d Cycle = %dmsec AudioFrames%d: TimeIn=%dmsec TimeOut=%dmsec Cycle = %d\n", 
			i, 
			storage_audio_timing[i][2], 
			storage_audio_timing[i][2] - storage_audio_timing[i-1][2], 
			storage_audio_timing[i][3],
			storage_audio_timing[i][0], 
			storage_audio_timing[i][1], 
			storage_audio_timing[i][0] - storage_audio_timing[i-1][0]
		);
	}
	fclose(file);
	LOG_info("save_storage_audio_timing writing to file DONE\n");
	fflush(stdout);
}*/

///////////////////////////////////////

void Core_getName(char* in_name, char* out_name) {
	strcpy(out_name, basename(in_name));
	char* tmp = strrchr(out_name, '_');
	tmp[0] = '\0';
}
void Core_open(const char* core_path, const char* tag_name) {
	LOG_info("Core_open\n");
	core.handle = dlopen(core_path, RTLD_LAZY);
	
	if (!core.handle) LOG_error("%s\n", dlerror());
	
	core.init = dlsym(core.handle, "retro_init");
	core.deinit = dlsym(core.handle, "retro_deinit");
	core.get_system_info = dlsym(core.handle, "retro_get_system_info");
	core.get_system_av_info = dlsym(core.handle, "retro_get_system_av_info");
	core.set_controller_port_device = dlsym(core.handle, "retro_set_controller_port_device");
	core.reset = dlsym(core.handle, "retro_reset");
	core.run = dlsym(core.handle, "retro_run");
	core.serialize_size = dlsym(core.handle, "retro_serialize_size");
	core.serialize = dlsym(core.handle, "retro_serialize");
	core.unserialize = dlsym(core.handle, "retro_unserialize");
	core.load_game = dlsym(core.handle, "retro_load_game");
	core.load_game_special = dlsym(core.handle, "retro_load_game_special");
	core.unload_game = dlsym(core.handle, "retro_unload_game");
	core.get_region = dlsym(core.handle, "retro_get_region");
	core.get_memory_data = dlsym(core.handle, "retro_get_memory_data");
	core.get_memory_size = dlsym(core.handle, "retro_get_memory_size");
	
	void (*set_environment_callback)(retro_environment_t);
	void (*set_video_refresh_callback)(retro_video_refresh_t);
	void (*set_audio_sample_callback)(retro_audio_sample_t);
	void (*set_audio_sample_batch_callback)(retro_audio_sample_batch_t);
	void (*set_input_poll_callback)(retro_input_poll_t);
	void (*set_input_state_callback)(retro_input_state_t);
	
	set_environment_callback = dlsym(core.handle, "retro_set_environment");
	set_video_refresh_callback = dlsym(core.handle, "retro_set_video_refresh");
	set_audio_sample_callback = dlsym(core.handle, "retro_set_audio_sample");
	set_audio_sample_batch_callback = dlsym(core.handle, "retro_set_audio_sample_batch");
	set_input_poll_callback = dlsym(core.handle, "retro_set_input_poll");
	set_input_state_callback = dlsym(core.handle, "retro_set_input_state");
	
	struct retro_system_info info = {};
	core.get_system_info(&info);
	
	Core_getName((char*)core_path, (char*)core.name);
	if (strcmp(core.name, "gambatte") == 0) {
		//detected core gambatte, it needs a workaround to enable can_dupe otherwise don't start.
		can_dupe = true;
	}
	sprintf((char*)core.version, "%s (%s)", info.library_name, info.library_version);
	strcpy((char*)core.tag, tag_name);
	strcpy((char*)core.extensions, info.valid_extensions);
	
	core.need_fullpath = info.need_fullpath;
	
	LOG_info("core: %s version: %s tag: %s (valid_extensions: %s need_fullpath: %i)\n", core.name, core.version, core.tag, info.valid_extensions, info.need_fullpath);
	
	sprintf((char*)core.config_dir, USERDATA_PATH "/%s-%s", core.tag, core.name);
	//sprintf((char*)core.states_dir, SHARED_USERDATA_PATH "/%s-%s", core.tag, core.name);
	sprintf((char*)core.states_dir, MYSAVESTATE_PATH "/%s/States", core.tag);
	sprintf((char*)core.saves_dir, SDCARD_PATH "/Saves/%s", core.tag);
	sprintf((char*)core.bios_dir, SDCARD_PATH "/Bios/%s", core.tag);
	
	char cmd[512];
	sprintf(cmd, "mkdir -p \"%s\"; mkdir -p \"%s\"", core.config_dir, core.states_dir);
	system(cmd);

	set_environment_callback(environment_callback);
	set_video_refresh_callback(video_refresh_callback);
	set_audio_sample_callback(audio_sample_callback);
	set_audio_sample_batch_callback(audio_sample_batch_callback);
	set_input_poll_callback(input_poll_callback);
	set_input_state_callback(input_state_callback);
}

int Core_updateAVInfo(void) {
	struct retro_system_av_info av_info = {};
	core.get_system_av_info(&av_info);

	double a = av_info.geometry.aspect_ratio;
	if (a<=0) a = (double)av_info.geometry.base_width / av_info.geometry.base_height;

	int changed = (core.fps != av_info.timing.fps || core.sample_rate != av_info.timing.sample_rate || core.aspect_ratio != a);

	core.fps = av_info.timing.fps;
	core.sample_rate = av_info.timing.sample_rate;
	core.aspect_ratio = a;

	if (changed) LOG_info("aspect_ratio: %f (%ix%i) fps: %f\n", a, av_info.geometry.base_width,av_info.geometry.base_height, core.fps);

	return changed;
}


void Core_init(void) {
	LOG_info("Core_init\n");
	core.init();
	core.initialized = 1;
}
void Core_load(void) {
	LOG_info("Core_load\n");
	struct retro_game_info game_info;
	game_info.path = game.tmp_path[0]?game.tmp_path:game.path;
	game_info.data = game.data;
	game_info.size = game.size;
	
	loadgamesuccess = core.load_game(&game_info);
	
	SRAM_read();
	RTC_read();
	
	// NOTE: must be called after core.load_game!
	struct retro_system_av_info av_info = {};
	core.get_system_av_info(&av_info);

	// FIX: some cores need configure a default controller.
	core.set_controller_port_device(0, RETRO_DEVICE_JOYPAD); // set a default, may update after loading configs

	core.fps = av_info.timing.fps;
	core.sample_rate = av_info.timing.sample_rate;
	double frameperiod = 1000000.0/core.fps;
	frame_period_usecs = (uint32_t)(frameperiod);
	LOG_info("Expected fps: %f sample_rate: %f Frame_Period: %i\n", core.fps, core.sample_rate, frame_period_usecs);
	double a = av_info.geometry.aspect_ratio;
	if (a<=0) a = (double)av_info.geometry.base_width / av_info.geometry.base_height;
	core.aspect_ratio = a;
	coreloaded = 1;
	if (Founddiskcontrol==1) {
		NumDiscsDetected = disk_control_ext.get_num_images();	
		LOG_info("\n\nRETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE - NumDiscs = %i\n", NumDiscsDetected);
	} else if (Founddiskcontrol==2) {
		NumDiscsDetected = disk_control.get_num_images();
		LOG_info("\n\nRETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE - NumDiscs = %i\n", NumDiscsDetected);
	}
	if (NumDiscsDetected>1){
		coreDiscManaged = 1;
		LOG_info("NumDisksDetected = %i\nMultidisk detected by core - switch to libretro disk control\n\n", NumDiscsDetected);
	}

	LOG_info("aspect_ratio: %f (%ix%i) fps: %f\n", a, av_info.geometry.base_width,av_info.geometry.base_height, core.fps);fflush(stdout);
	Core_updateAVInfo();
}
void Core_reset(void) {
	core.reset();
}
void Core_unload(void) {
	SND_quit();
}
void Core_quit(void) {
	if (core.initialized) {
		SRAM_write();
		RTC_write();
		core.unload_game();
		core.deinit();
		core.initialized = 0;
	}
}
void Core_close(void) {
	if (core.handle) dlclose(core.handle);
}

///////////////////////////////////////

#define MENU_ITEM_COUNT 5
#define MENU_SLOT_COUNT 8

enum {
	ITEM_CONT,
	ITEM_SAVE,
	ITEM_LOAD,
	ITEM_OPTS,
	//ITEM_BOXART,
	ITEM_QUIT,
};

enum {
	STATUS_CONT =  0,
	STATUS_SAVE =  1,
	STATUS_LOAD = 11,
	STATUS_OPTS = 23,
	STATUS_DISC = 24,
	//STATUS_BOXART = 25,
	STATUS_QUIT = 30,
	STATUS_RESET= 31,
};

// TODO: I don't love how overloaded this has become
static struct {
	SDL_Surface* bitmap;
	SDL_Surface* overlay;
	char* items[MENU_ITEM_COUNT];
	char* disc_paths[9]; // up to 9 paths, Arc the Lad Collection is 7 discs
	char minui_dir[256];
	char slot_path[256];
	char base_path[256];
	char bmp_path[256];
	char txt_path[256];
	char txt_path_slot[256];
	int disc;
	int total_discs;
	int slot;
	int save_exists;
	int preview_exists;
} menu = {
	.bitmap = NULL,
	.disc = -1,
	.total_discs = 0,
	.save_exists = 0,
	.preview_exists = 0,
	
	.items = {
		[ITEM_CONT] = "Continue",
		[ITEM_SAVE] = "Save",
		[ITEM_LOAD] = "Load",
		[ITEM_OPTS] = "Options",
		//[ITEM_BOXART] = "Make Boxart",
		[ITEM_QUIT] = "Quit",
	}
};

int makeBoxart(SDL_Surface *image, char *filename) {
	myBoxartData boxartdata;
	readBoxartcfg(GAMEBOXART_CFGFILE, &boxartdata);
	char dirpath[256];
	char tmp[256];
	char cmd1[512];
	getParentFolderName(game.path,tmp);
	sprintf(dirpath, ROMS_PATH "/%s/Imgs",tmp);
	LOG_info("#####baxart2 name###### = %s\n", dirpath);
    sprintf(cmd1,"mkdir -p \"%s\"",dirpath); 
	system(cmd1);
	cmd1[0]='\0';
    SDL_Surface *mysurface = SDL_CreateRGBSurface(0,boxartdata.sW,boxartdata.sH,16,0,0,0,0);
    //SDL_Surface *unscaled_myimg = IMG_Load(BACKGROUND);
    if (image == NULL){
        LOG_info("Background image loading failed");
        return -1;
    }
	double xfactr, yfactr;
    double myaspect = 1.0 * image->w / image->h; 
    int localX = boxartdata.bX;
    int localY = boxartdata.bY; 
    switch (boxartdata.aspect) {
        case ASPECT: 
            LOG_info("ASPECT = ASPECT\n");
            //first calculate othe original aspectratio
            if (myaspect > 1.0){
                //the W is bigger than H
                    //resize to fit W
                    xfactr = 1.0 * boxartdata.bW / image->w;
                    yfactr = xfactr; 
                    localY += (boxartdata.bH - (image->h * yfactr))/2;
            } else { //image is higher than wider
                    //resize to fit H
                    yfactr = 1.0 * boxartdata.bH / image->h;
                    xfactr = yfactr; 
                    localX += (boxartdata.bW - (image->w * xfactr))/2;
            }
            break;
        case NATIVE: 
            LOG_info("ASPECT = NATIVE\n");
            if ((boxartdata.bW > image->w) && (boxartdata.bH > image->h)){
                //image is smaller than target box
                xfactr = 1.0;
                yfactr = 1.0; 
                //change x y to place the image in the center of the target box
                localX += (boxartdata.bW - image->w)/2;
                localY += (boxartdata.bH - image->h)/2;
            } else {
                //image is bigger than target box so apply ASPECT rule
                if (myaspect > 1.0){
                    //the W is bigger than H
                        //resize to fit W
                        xfactr = 1.0 * boxartdata.sW / image->w;
                        yfactr = xfactr; 
                        localY += (boxartdata.bH - (image->h * yfactr))/2;
                } else { //image is higher than wider
                        //resize to fit H
                        yfactr = 1.0 * boxartdata.bH / image->h;
                        xfactr = yfactr; 
                        localX += (boxartdata.bW - (image->w * xfactr))/2;
                }
            }            
            break;
        case FULL: 
            //fill target box by shrinking/streching the original image
            LOG_info("ASPECT = FULL\n");
            xfactr = 1.0 * boxartdata.bW / image->w;
            yfactr = 1.0 * boxartdata.bH / image->h; 
            break;
    }
    SDL_Surface *scaled_myimg = zoomSurface(image, xfactr , yfactr, 0);
    SDL_BlitSurface(scaled_myimg,NULL,mysurface,&(SDL_Rect){localX,localY});

	if (strncmp(boxartdata.gradient,"NONE",4) != 0){
        SDL_Surface *blackgradient = IMG_Load(boxartdata.gradient);
        if (blackgradient == NULL){
            LOG_info("Failed loading Gradient: %s\n", IMG_GetError());
            return -1;  
        } 
		LOG_info("Applying Gradient %s\n",boxartdata.gradient);
#if defined (USE_SDL2)
		SDL_SetSurfaceBlendMode(blackgradient,SDL_BLENDMODE_BLEND);
#else
		SDL_SetColorKey(blackgradient, SDL_TRUE, SDL_MapRGB(blackgradient->format, 0, 0, 0));
#endif
		if (SDL_BlitSurface(blackgradient,NULL,mysurface,NULL) == 0) {
			LOG_info("Applying Gradient Success\n");
		}   			
		SDL_FreeSurface(blackgradient);
    }


    SDL_RWops* out = SDL_RWFromFile(filename, "wb");
#if defined (USE_SDL2)
	IMG_SavePNG_RW(mysurface, out, 1);
#else
	SDL_SaveBMP_RW(mysurface, out, 1);
	bmp2png(menu.bmp_path);	
#endif
    //SDL_BlitSurface(mysurface,NULL,screen,NULL); 
    SDL_FreeSurface(scaled_myimg); 
    //SDL_FreeSurface(unscaled_myimg);
    SDL_FreeSurface(mysurface);
    return 1;
}




void Menu_init(void) {
	menu.overlay = SDL_CreateRGBSurface(SDL_SWSURFACE,DEVICE_WIDTH,DEVICE_HEIGHT,FIXED_DEPTH,RGBA_MASK_AUTO);
	SDLX_SetAlpha(menu.overlay, SDL_SRCALPHA, 0x80);
	SDL_FillRect(menu.overlay, NULL, 0);
	
	char emu_name[256];
	getEmuName(game.path, emu_name);
	sprintf(menu.minui_dir, SHARED_USERDATA_PATH "/.minui/%s", emu_name);
	mkdir(menu.minui_dir, 0755);

	sprintf(menu.slot_path, "%s/%s.txt", menu.minui_dir, game.fullname);
	
	if (simple_mode) menu.items[ITEM_OPTS] = "Reset";

	if (coreDiscManaged){  //the core has detected multidisc pbp game
		menu.total_discs = NumDiscsDetected;	//set the num of discs detected
		for (int i = 0; i < menu.total_discs; i++) {
			menu.disc_paths[i] = strdup(game.path); //copy the game path for each disc path, in this case all are the same
		}
		menu.disc = disk_control_ext.get_image_index();  //get the current disc index
	}

	if (game.m3u_path[0]) {  //in case of m3u file the core doesn't detects it so coreDiscManaged is not set
		char* tmp;
		strcpy(menu.base_path, game.m3u_path);
		tmp = strrchr(menu.base_path, '/') + 1;
		tmp[0] = '\0';
		
		//read m3u file
		FILE* file = fopen(game.m3u_path, "r");
		if (file) {
			char line[256];
			while (fgets(line,256,file)!=NULL) {
				normalizeNewline(line);
				trimTrailingNewlines(line);
				if (strlen(line)==0) continue; // skip empty lines
		
				char disc_path[256];
				strcpy(disc_path, menu.base_path);
				tmp = disc_path + strlen(disc_path);
				strcpy(tmp, line);
				
				// for every valid line in the m3u fill the disc_paths array
				// found a valid disc path
				if (exists(disc_path)) {
					menu.disc_paths[menu.total_discs] = strdup(disc_path);
					// matched our current disc					
					if (exactMatch(disc_path, game.path)) {
						menu.disc = menu.total_discs;
					}
					menu.total_discs += 1;
				}
			}
			fclose(file);
			if (menu.total_discs > 1) {
				struct retro_game_info game_info = {};
				coreDiscManaged = 1;
				NumDiscsDetected = menu.total_discs;
				for (int i = 0; i < menu.total_discs; i++) {
					LOG_info("Disc %d: %s\n", i+1, menu.disc_paths[i]);
					game_info.path = menu.disc_paths[i];
					if (i>0) disk_control_ext.replace_image_index(i, &game_info);
				}
			}
		}
	}
}

void Menu_quit(void) {
	SDL_FreeSurface(menu.overlay);
}
void Menu_beforeSleep(void) {
	SRAM_write();
	RTC_write();
	State_autosave();
	putFile(AUTO_RESUME_PATH, game.path + strlen(SDCARD_PATH));
	PWR_setCPUSpeed(CPU_SPEED_MENU);
}
void Menu_afterSleep(void) {
	unlink(AUTO_RESUME_PATH);
	setOverclock(overclock);
}

typedef struct MenuList MenuList;
typedef struct MenuItem MenuItem;
enum {
	MENU_CALLBACK_NOP,
	MENU_CALLBACK_EXIT,
	MENU_CALLBACK_NEXT_ITEM,
};
typedef int (*MenuList_callback_t)(MenuList* list, int i);
typedef struct MenuItem {
	char* name;
	char* desc;
	char** values;
	char* key; // optional, used by options
	int id; // optional, used by bindings
	int value;
	MenuList* submenu;
	MenuList_callback_t on_confirm;
	MenuList_callback_t on_change;
} MenuItem;

enum {
	MENU_LIST, // eg. save and main menu
	MENU_VAR, // eg. frontend
	MENU_FIXED, // eg. emulator
	MENU_INPUT, // eg. renders like but MENU_VAR but handles input differently
};
typedef struct MenuList {
	int type;
	int max_width; // cached on first draw
	char* desc;
	MenuItem* items;
	MenuList_callback_t on_confirm;
	MenuList_callback_t on_change;
} MenuList;

static int Menu_message(char* message, char** pairs) {
	GFX_setMode(MODE_MAIN);
	int dirty = 1;
	while (1) {
		GFX_startFrame();
		PAD_poll();

		if (PAD_justPressed(BTN_A) || PAD_justPressed(BTN_B)) break;
		
		PWR_update(&dirty, NULL, Menu_beforeSleep, Menu_afterSleep);
		
		if (dirty) {
			GFX_clear(screen);
			GFX_blitMessage(font.medium, message, screen, &(SDL_Rect){0,SCALE1((PADDING - (PADDING*fancy_mode))),screen->w,screen->h-SCALE1(PILL_SIZE+(PADDING - (PADDING*fancy_mode)))});
			GFX_blitButtonGroup(pairs, 0, screen, 1, fancy_mode);
			GFX_flip(screen);
			GFX_pan();
			dirty = 0;
		}
		else GFX_sync();
	}
	GFX_setMode(MODE_MENU);
	return MENU_CALLBACK_NOP; // TODO: this should probably be an arg
}

static int Menu_options(MenuList* list);

static int MenuList_freeItems(MenuList* list, int i) {
	// TODO: what calls this? do menu's register for needing it? then call it on quit for each?
	if (list->items) free(list->items);
	return MENU_CALLBACK_NOP;
}

static int OptionFrontend_optionChanged(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	Config_syncFrontend(item->key, item->value);
	return MENU_CALLBACK_NOP;
}
static MenuList OptionFrontend_menu = {
	.type = MENU_VAR,
	.on_change = OptionFrontend_optionChanged,
	.items = NULL,
};
static int OptionFrontend_openMenu(MenuList* list, int i) {
	if (OptionFrontend_menu.items==NULL) {
		// TODO: where do I free this? I guess I don't :sweat_smile:
		if (!config.frontend.enabled_count) {
			int enabled_count = 0;
			for (int i=0; i<config.frontend.count; i++) {
				if (!config.frontend.options[i].lock) enabled_count += 1;
			}
			config.frontend.enabled_count = enabled_count;
			config.frontend.enabled_options = calloc(enabled_count+1, sizeof(Option*));
			int j = 0;
			for (int i=0; i<config.frontend.count; i++) {
				Option* item = &config.frontend.options[i];
				if (item->lock) continue;
				config.frontend.enabled_options[j] = item;
				j += 1;
			}
		}

		OptionFrontend_menu.items = calloc(config.frontend.enabled_count+1, sizeof(MenuItem));
		for (int j=0; j<config.frontend.enabled_count; j++) {
			Option* option = config.frontend.enabled_options[j];
			MenuItem* item = &OptionFrontend_menu.items[j];
			item->key = option->key;
			item->name = option->name;
			item->desc = option->desc;
			item->value = option->value;
			item->values = option->labels;
		}
	}
	else {
		// update values
		for (int j=0; j<config.frontend.enabled_count; j++) {
			Option* option = config.frontend.enabled_options[j];
			MenuItem* item = &OptionFrontend_menu.items[j];
			item->value = option->value;
		}
	}
	
	Menu_options(&OptionFrontend_menu);
	return MENU_CALLBACK_NOP;
}

static int OptionEmulator_optionChanged(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	Option* option = OptionList_getOption(&config.core, item->key);
	LOG_info("%s (%s) changed from `%s` (%s) to `%s` (%s)\n", item->name, item->key, 
		item->values[option->value], option->values[option->value], 
		item->values[item->value], option->values[item->value]
	);
	OptionList_setOptionRawValue(&config.core, item->key, item->value);
	return MENU_CALLBACK_NOP;
}
static int OptionEmulator_optionDetail(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	Option* option = OptionList_getOption(&config.core, item->key);
	if (option->full) return Menu_message(option->full, (char*[]){ "B","BACK", NULL });
	else return MENU_CALLBACK_NOP;
}
static MenuList OptionEmulator_menu = {
	.type = MENU_FIXED,
	.on_confirm = OptionEmulator_optionDetail, // TODO: this needs pagination to be truly useful
	.on_change = OptionEmulator_optionChanged,
	.items = NULL,
};
static int OptionEmulator_openMenu(MenuList* list, int i) {
	if (OptionEmulator_menu.items==NULL) {
		// TODO: where do I free this? I guess I don't :sweat_smile:
		if (!config.core.enabled_count) {
			int enabled_count = 0;
			for (int i=0; i<config.core.count; i++) {
				if (!config.core.options[i].lock) enabled_count += 1;
			}
			config.core.enabled_count = enabled_count;
			config.core.enabled_options = calloc(enabled_count+1, sizeof(Option*));
			int j = 0;
			for (int i=0; i<config.core.count; i++) {
				Option* item = &config.core.options[i];
				if (item->lock) continue;
				config.core.enabled_options[j] = item;
				j += 1;
			}
		}
		
		OptionEmulator_menu.items = calloc(config.core.enabled_count+1, sizeof(MenuItem));
		for (int j=0; j<config.core.enabled_count; j++) {
			Option* option = config.core.enabled_options[j];
			MenuItem* item = &OptionEmulator_menu.items[j];
			item->key = option->key;
			item->name = option->name;
			item->desc = option->desc;
			item->value = option->value;
			item->values = option->labels;
		}
	}
	else {
		// update values
		for (int j=0; j<config.core.enabled_count; j++) {
			Option* option = config.core.enabled_options[j];
			MenuItem* item = &OptionEmulator_menu.items[j];
			item->value = option->value;
		}
	}
	
	if (OptionEmulator_menu.items[0].name) { // TODO: why doesn't this just use (enabled_)count?
		Menu_options(&OptionEmulator_menu);
	}
	else {
		Menu_message("This core has no options.", (char*[]){ "B","BACK", NULL });
	}
	
	return MENU_CALLBACK_NOP;
}

int OptionControls_bind(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	if (item->values!=button_labels) {
		// LOG_info("changed gamepad_type\n");
		return MENU_CALLBACK_NOP;
	}
	
	ButtonMapping* button = &config.controls[item->id];	
	
	int bound = 0;
	while (!bound) {
		GFX_startFrame();
		PAD_poll();
		
		// NOTE: off by one because of the initial NONE value
		for (int id=0; id<=LOCAL_BUTTON_COUNT; id++) {
			if (PAD_justPressed(1 << (id-1))) {
				item->value = id;
				button->local = id - 1;
				if (PAD_isPressed(BTN_MENU)) {
					item->value += LOCAL_BUTTON_COUNT;
					button->mod = 1;
				}
				else {
					button->mod = 0;
				}
				bound = 1;
				break;
			}
		}
		GFX_sync();
	}
	return MENU_CALLBACK_NEXT_ITEM;
}
static int OptionControls_unbind(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	if (item->values!=button_labels) return MENU_CALLBACK_NOP;
	
	ButtonMapping* button = &config.controls[item->id];
	button->local = -1;
	button->mod = 0;
	return MENU_CALLBACK_NOP;
}
static int OptionControls_optionChanged(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	if (item->values!=gamepad_labels) return MENU_CALLBACK_NOP;

	if (has_custom_controllers) {
		gamepad_type = item->value;
		int device = strtol(gamepad_values[item->value], NULL, 0);
		core.set_controller_port_device(0, device);
	}
	return MENU_CALLBACK_NOP;
}

static int OptionControls_mapABXYChanged(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	if (item->values!=onoff_labels) return MENU_CALLBACK_NOP;
	config.controller_map_abxy_to_rstick = item->value;

	return MENU_CALLBACK_NOP;
}
 
 static int OptionControls_mapLStickChanged(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	if (item->values!=onoff_labels) return MENU_CALLBACK_NOP;
	config.controller_map_lstick_to_dpad = item->value;
	pad.map_leftstick_to_dpad = config.controller_map_lstick_to_dpad;

	return MENU_CALLBACK_NOP;
}



static MenuList OptionControls_menu = {
	.type = MENU_INPUT,
	.desc = "Press A to set and X to clear."
		"\nSupports single button and MENU+button." // TODO: not supported on nano because POWER doubles as MENU
	,
	.on_confirm = OptionControls_bind,
	.on_change = OptionControls_unbind,
	.items = NULL
};

static int OptionControls_openMenu(MenuList* list, int i) {
	LOG_info("OptionControls_openMenu\n");

	if (OptionControls_menu.items==NULL) {
	//	LOG_info("OptionControls_openMenu Create\n");
		// TODO: where do I free this?
		OptionControls_menu.items = calloc(RETRO_BUTTON_COUNT+2+has_custom_controllers, sizeof(MenuItem));
		int k = 0;
		
		MenuItem* item2 = &OptionControls_menu.items[k++];
		item2->name = "MapABXYtoRightStick";
		item2->desc = "Map ABXY buttons to right stick.";
		item2->value = config.controller_map_abxy_to_rstick;
		item2->values = onoff_labels;
		item2->on_change = OptionControls_mapABXYChanged;

		MenuItem* item3 = &OptionControls_menu.items[k++];
		item3->name = "MapLeftSticktoDPad";
		item3->desc = "Map left stick to DPad buttons.";
		item3->value = config.controller_map_lstick_to_dpad;
		item3->values = onoff_labels;
		item3->on_change = OptionControls_mapLStickChanged;
	
		if (has_custom_controllers) {
			MenuItem* item = &OptionControls_menu.items[k++];
			item->name = "Controller";
			item->desc = "Select the type of controller.";
			item->value = gamepad_type;
			item->values = gamepad_labels;
			item->on_change = OptionControls_optionChanged;
		}
		
		for (int j=0; config.controls[j].name; j++) {
			ButtonMapping* button = &config.controls[j];
			if (button->ignore) continue;
						
			MenuItem* item = &OptionControls_menu.items[k++];
			item->id = j;
			item->name = button->name;
			item->desc = NULL;
			item->value = button->local + 1;
			if (button->mod) item->value += LOCAL_BUTTON_COUNT;
			item->values = button_labels;
			LOG_info("\t%s (%i:%i)\n", button->name, button->local, button->retro);
		}
	}
	else {
		// update values
		//LOG_info("OptionControls_openMenu Refresh\n");
		int k = 0;
		
		MenuItem* item2 = &OptionControls_menu.items[k++];
		item2->value = config.controller_map_abxy_to_rstick;

		MenuItem* item3 = &OptionControls_menu.items[k++];
		item3->value = config.controller_map_lstick_to_dpad;
		pad.map_leftstick_to_dpad = config.controller_map_lstick_to_dpad;
		

		if (has_custom_controllers) {
			MenuItem* item = &OptionControls_menu.items[k++];
			item->value = gamepad_type;
		}
		
		for (int j=0; config.controls[j].name; j++) {
			ButtonMapping* button = &config.controls[j];
			if (button->ignore) continue;
			
			MenuItem* item = &OptionControls_menu.items[k++];
			item->value = button->local + 1;
			if (button->mod) item->value += LOCAL_BUTTON_COUNT;
		}
	}
	Menu_options(&OptionControls_menu);
	return MENU_CALLBACK_NOP;
}

static int OptionShortcuts_bind(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	ButtonMapping* button = &config.shortcuts[item->id];
	int bound = 0;
	while (!bound) {
		GFX_startFrame();
		PAD_poll();
		
		// NOTE: off by one because of the initial NONE value
		for (int id=0; id<=LOCAL_BUTTON_COUNT; id++) {
			if (PAD_justPressed(1 << (id-1))) {
				item->value = id;
				button->local = id - 1;
				if (PAD_isPressed(BTN_MENU)) {
					item->value += LOCAL_BUTTON_COUNT;
					button->mod = 1;
				}
				else {
					button->mod = 0;
				}
				bound = 1;
				break;
			}
		}
		GFX_sync();
	}
	return MENU_CALLBACK_NEXT_ITEM;
}
static int OptionShortcuts_unbind(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	ButtonMapping* button = &config.shortcuts[item->id];
	button->local = -1;
	button->mod = 0;
	return MENU_CALLBACK_NOP;
}
static MenuList OptionShortcuts_menu = {
	.type = MENU_INPUT,
	.desc = "Press A to set and X to clear." 
		"\nSupports single button and MENU+button." // TODO: not supported on nano because POWER doubles as MENU
	,
	.on_confirm = OptionShortcuts_bind,
	.on_change = OptionShortcuts_unbind,
	.items = NULL
};
static char* getSaveDesc(void) {
	switch (config.loaded) {
		case CONFIG_NONE:		return "Using defaults."; break;
		case CONFIG_CONSOLE:	return "Using console config."; break;
		case CONFIG_GAME:		return "Using game config."; break;
	}
	return NULL;
}
static int OptionShortcuts_openMenu(MenuList* list, int i) {
	if (OptionShortcuts_menu.items==NULL) {
		// TODO: where do I free this? I guess I don't :sweat_smile:
		OptionShortcuts_menu.items = calloc(SHORTCUT_COUNT+1, sizeof(MenuItem));
		for (int j=0; config.shortcuts[j].name; j++) {
			ButtonMapping* button = &config.shortcuts[j];
			MenuItem* item = &OptionShortcuts_menu.items[j];
			item->id = j;
			item->name = button->name;
			item->desc = NULL;
			item->value = button->local + 1;
			if (button->mod) item->value += LOCAL_BUTTON_COUNT;
			item->values = button_labels;
		}
	}
	else {
		// update values
		for (int j=0; config.shortcuts[j].name; j++) {
			ButtonMapping* button = &config.shortcuts[j];
			MenuItem* item = &OptionShortcuts_menu.items[j];
			item->value = button->local + 1;
			if (button->mod) item->value += LOCAL_BUTTON_COUNT;
		}
	}
	Menu_options(&OptionShortcuts_menu);
	return MENU_CALLBACK_NOP;
}

static void OptionSaveChanges_updateDesc(void);
static int OptionSaveChanges_onConfirm(MenuList* list, int i) {
	char* message;
	switch (i) {
		case 0: {
			Config_write(CONFIG_WRITE_ALL);
			message = "Saved for console.";
			break;
		}
		case 1: {
			Config_write(CONFIG_WRITE_GAME);
			message = "Saved for game.";
			break;
		}
		default: {
			Config_restore();
			if (config.loaded) message = "Restored console defaults.";
			else message = "Restored defaults.";
			break;
		}
	}
	Menu_message(message, (char*[]){ "A","OKAY", NULL });
	OptionSaveChanges_updateDesc();
	return MENU_CALLBACK_EXIT;
}
static MenuList OptionSaveChanges_menu = {
	.type = MENU_LIST,
	.on_confirm = OptionSaveChanges_onConfirm,
	.items = (MenuItem[]){
		{"Save for console"},
		{"Save for game"},
		{"Restore defaults"},
		{NULL},
	}
};
static int OptionSaveChanges_openMenu(MenuList* list, int i) {
	OptionSaveChanges_updateDesc();
	OptionSaveChanges_menu.desc = getSaveDesc();
	Menu_options(&OptionSaveChanges_menu);
	return MENU_CALLBACK_NOP;
}

static int OptionQuicksave_onConfirm(MenuList* list, int i) {
	Menu_beforeSleep();
	PWR_powerOff();
}

static MenuList options_menu = {
	.type = MENU_LIST,
	.items = (MenuItem[]) {
		{"Frontend", "MyMinUI (" BUILD_DATE " " BUILD_HASH ")",.on_confirm=OptionFrontend_openMenu},
		{"Emulator",.on_confirm=OptionEmulator_openMenu},
		{"Controls",.on_confirm=OptionControls_openMenu},
		{"Shortcuts",.on_confirm=OptionShortcuts_openMenu}, 
		{"Save Changes",.on_confirm=OptionSaveChanges_openMenu},
		{NULL},
		{NULL},
		{NULL},
	}
};

static void OptionSaveChanges_updateDesc(void) {
	options_menu.items[4].desc = getSaveDesc();
}

#define OPTION_PADDING 8

static int Menu_options(MenuList* list) {
	MenuItem* items = list->items;
	int type = list->type;

	int dirty = 1;
	int show_options = 1;
	int show_settings = 0;
	int await_input = 0;
	
	// dependent on option list offset top and bottom, eg. the gray triangles
	int max_visible_options = (screen->h - ((SCALE1(PADDING + PILL_SIZE) * 2) + SCALE1(BUTTON_SIZE))) / SCALE1(BUTTON_SIZE); // 7 for 480, 10 for 720


	int count;
	for (count=0; items[count].name; count++);
	int selected = 0;
	int start = 0;
	int end = MIN(count,max_visible_options);
	int visible_rows = end;
	
	OptionSaveChanges_updateDesc();
	
	int defer_menu = false;
	while (show_options) {
		if (await_input) {
			defer_menu = true;
			list->on_confirm(list, selected);
			
			selected += 1;
			if (selected>=count) {
				selected = 0;
				start = 0;
				end = visible_rows;
			}
			else if (selected>=end) {
				start += 1;
				end += 1;
			}
			dirty = 1;
			await_input = false;
		}
		
		GFX_startFrame();
		PAD_poll();
		
		if (PAD_justRepeated(BTN_UP)) {
			selected -= 1;
			if (selected<0) {
				selected = count - 1;
				start = MAX(0,count - max_visible_options);
				end = count;
			}
			else if (selected<start) {
				start -= 1;
				end -= 1;
			}
			dirty = 1;
		}
		else if (PAD_justRepeated(BTN_DOWN)) {
			selected += 1;
			if (selected>=count) {
				selected = 0;
				start = 0;
				end = visible_rows;
			}
			else if (selected>=end) {
				start += 1;
				end += 1;
			}
			dirty = 1;
		}
		else {
			MenuItem* item = &items[selected];
			if (item->values && item->values!=button_labels) { // not an input binding
				if (PAD_justRepeated(BTN_LEFT)) {
					if (item->value>0) item->value -= 1;
					else {
						int j;
						for (j=0; item->values[j]; j++);
						item->value = j - 1;
					}
				
						if (item->on_change) item->on_change(list, selected);
						else if (list->on_change) list->on_change(list, selected);
				
					dirty = 1;
				}
				else if (PAD_justRepeated(BTN_RIGHT)) {
					if (item->values[item->value+1]) item->value += 1;
					else item->value = 0;
				
				if (item->on_change) item->on_change(list, selected);
				else if (list->on_change) list->on_change(list, selected);
				
				dirty = 1;
				}
			}
		}
		
		// uint32_t now = SDL_GetTicks();
		if (PAD_justPressed(BTN_B)) { // || PAD_tappedMenu(now)
			show_options = 0;
		}
		else if (PAD_justPressed(BTN_A)) {
			MenuItem* item = &items[selected];
			int result = MENU_CALLBACK_NOP;
			if (item->on_confirm) result = item->on_confirm(list, selected); // item-specific action, eg. Save for all games
			else if (item->submenu) result = Menu_options(item->submenu); // drill down, eg. main options menu
			// TODO: is there a way to defer on_confirm for MENU_INPUT so we can clear the currently set value to indicate it is awaiting input? 
			// eg. set a flag to call on_confirm at the beginning of the next frame?
			else if (list->on_confirm) {
				if (item->values==button_labels) await_input = 1; // button binding
				else result = list->on_confirm(list, selected); // list-specific action, eg. show item detail view or input binding
			}
			if (result==MENU_CALLBACK_EXIT) show_options = 0;
			else {
				if (result==MENU_CALLBACK_NEXT_ITEM) {
					// copied from PAD_justRepeated(BTN_DOWN) above
					selected += 1;
					if (selected>=count) {
						selected = 0;
						start = 0;
						end = visible_rows;
					}
					else if (selected>=end) {
						start += 1;
						end += 1;
					}
				}
				dirty = 1;
			}
		}
		else if (type==MENU_INPUT) {
			if (PAD_justPressed(BTN_X)) {
				MenuItem* item = &items[selected];
				item->value = 0;
				
				if (item->on_change) item->on_change(list, selected);
				else if (list->on_change) list->on_change(list, selected);
				
				// copied from PAD_justRepeated(BTN_DOWN) above
				selected += 1;
				if (selected>=count) {
					selected = 0;
					start = 0;
					end = visible_rows;
				}
				else if (selected>=end) {
					start += 1;
					end += 1;
				}
				dirty = 1;
			}
		}
		
		if (!defer_menu) PWR_update(&dirty, &show_settings, Menu_beforeSleep, Menu_afterSleep);
		
		if (defer_menu && PAD_justReleased(BTN_MENU)) defer_menu = false;
		
		if (dirty) {
			GFX_clear(screen);
			GFX_blitHardwareGroup(screen, show_settings, fancy_mode);
			
			char* desc = NULL;
			SDL_Surface* text;

			if (type==MENU_LIST) {
				int mw = list->max_width;
				if (!mw) {
					// get the width of the widest item
					for (int i=0; i<count; i++) {
						MenuItem* item = &items[i];
						int w = 0;
						TTF_SizeUTF8(font.small, item->name, &w, NULL);
						w += SCALE1(OPTION_PADDING*2);
						if (w>mw) mw = w;
					}
					// cache the result
					list->max_width = mw = MIN(mw, screen->w - SCALE1((PADDING - (PADDING*fancy_mode)) *2));
				}
				
				int ox = (screen->w - mw) / 2;
				int oy = SCALE1(PADDING  + PILL_SIZE);
				int selected_row = selected - start;
				for (int i=start,j=0; i<end; i++,j++) {
					MenuItem* item = &items[i];
					SDL_Color text_color = COLOR_WHITE;

					// int ox = (screen->w - w) / 2; // if we're centering these (but I don't think we should after seeing it)
					if (j==selected_row) {
						// move out of conditional if centering
						int w = 0;
						TTF_SizeUTF8(font.small, item->name, &w, NULL);
						w += SCALE1(OPTION_PADDING*2);
						
						GFX_blitPill(ASSET_BUTTON, screen, &(SDL_Rect){
							ox,
							oy+SCALE1(j*BUTTON_SIZE),
							w,
							SCALE1(BUTTON_SIZE)
						});
						text_color = COLOR_BLACK;
						
						if (item->desc) desc = item->desc;
					}
					text = TTF_RenderUTF8_Blended(font.small, item->name, text_color);
					SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
						ox+SCALE1(OPTION_PADDING),
						oy+SCALE1((j*BUTTON_SIZE)+1)
					});
					SDL_FreeSurface(text);
				}
			}
			else if (type==MENU_FIXED) {
				// NOTE: no need to calculate max width
				int mw = screen->w - SCALE1((PADDING - (PADDING*fancy_mode))*2);
				// int lw,rw;
				// lw = rw = mw / 2;
				int ox,oy;
				ox = oy = SCALE1((PADDING - (PADDING*fancy_mode)));
				oy += SCALE1(PILL_SIZE);
				
				int selected_row = selected - start;
				for (int i=start,j=0; i<end; i++,j++) {
					MenuItem* item = &items[i];
					SDL_Color text_color = COLOR_WHITE;

					if (j==selected_row) {
						// gray pill
						GFX_blitPill(ASSET_OPTION, screen, &(SDL_Rect){
							ox,
							oy+SCALE1(j*BUTTON_SIZE),
							mw,
							SCALE1(BUTTON_SIZE)
						});
					}
					
					if (item->value>=0) {
						text = TTF_RenderUTF8_Blended(font.tiny, item->values[item->value], COLOR_WHITE); // always white
						SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
							ox + mw - text->w - SCALE1(OPTION_PADDING),
							oy+SCALE1((j*BUTTON_SIZE)+3)
						});
						SDL_FreeSurface(text);
					}
					
					// TODO: blit a black pill on unselected rows (to cover longer item->values?) or truncate longer item->values?
					if (j==selected_row) {
						// white pill
						int w = 0;
						TTF_SizeUTF8(font.small, item->name, &w, NULL);
						w += SCALE1(OPTION_PADDING*2);
						GFX_blitPill(ASSET_BUTTON, screen, &(SDL_Rect){
							ox,
							oy+SCALE1(j*BUTTON_SIZE),
							w,
							SCALE1(BUTTON_SIZE)
						});
						text_color = COLOR_BLACK;
						
						if (item->desc) desc = item->desc;
					}
					text = TTF_RenderUTF8_Blended(font.small, item->name, text_color);
					SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
						ox+SCALE1(OPTION_PADDING),
						oy+SCALE1((j*BUTTON_SIZE)+1)
					});
					SDL_FreeSurface(text);
				}
			}
			else if (type==MENU_VAR || type==MENU_INPUT) {
				int mw = list->max_width;
				if (!mw) {
					// get the width of the widest row
					int mrw = 0;
					for (int i=0; i<count; i++) {
						MenuItem* item = &items[i];
						int w = 0;
						int lw = 0;
						int rw = 0;
						TTF_SizeUTF8(font.small, item->name, &lw, NULL);
						
						// every value list in an input table is the same
						// so only calculate rw for the first item...
						if (!mrw || type!=MENU_INPUT) {
							for (int j=0; item->values[j]; j++) {
								TTF_SizeUTF8(font.tiny, item->values[j], &rw, NULL);
								if (lw+rw>w) w = lw+rw;
								if (rw>mrw) mrw = rw;
							}
						}
						else {
							w = lw + mrw;
						}
						w += SCALE1(OPTION_PADDING*4);
						if (w>mw) mw = w;
					}
					fflush(stdout);
					// cache the result
					list->max_width = mw = MIN(mw, screen->w - SCALE1((PADDING - (PADDING*fancy_mode)) *2));
				}
				
				int ox = (screen->w - mw) / 2;
				int oy = SCALE1((PADDING - (PADDING*fancy_mode)) + PILL_SIZE);
				int selected_row = selected - start;
				for (int i=start,j=0; i<end; i++,j++) {
					MenuItem* item = &items[i];
					SDL_Color text_color = COLOR_WHITE;

					if (j==selected_row) {
						// gray pill
						GFX_blitPill(ASSET_OPTION, screen, &(SDL_Rect){
							ox,
							oy+SCALE1(j*BUTTON_SIZE),
							mw,
							SCALE1(BUTTON_SIZE)
						});
						
						// white pill
						int w = 0;
						TTF_SizeUTF8(font.small, item->name, &w, NULL);
						w += SCALE1(OPTION_PADDING*2);
						GFX_blitPill(ASSET_BUTTON, screen, &(SDL_Rect){
							ox,
							oy+SCALE1(j*BUTTON_SIZE),
							w,
							SCALE1(BUTTON_SIZE)
						});
						text_color = COLOR_BLACK;
						
						if (item->desc) desc = item->desc;
					}
					text = TTF_RenderUTF8_Blended(font.small, item->name, text_color);
					SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
						ox+SCALE1(OPTION_PADDING),
						oy+SCALE1((j*BUTTON_SIZE)+1)
					});
					SDL_FreeSurface(text);
					
					if (await_input && j==selected_row) {
						// buh
					}
					else if (item->value>=0) {
						text = TTF_RenderUTF8_Blended(font.tiny, item->values[item->value], COLOR_WHITE); // always white
						SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
							ox + mw - text->w - SCALE1(OPTION_PADDING),
							oy+SCALE1((j*BUTTON_SIZE)+3)
						});
						SDL_FreeSurface(text);
					}
				}
			}
			
			if (count>max_visible_options) {
				#define SCROLL_WIDTH 24
				#define SCROLL_HEIGHT 4
				int ox = (screen->w - SCALE1(SCROLL_WIDTH))/2;
				int oy = SCALE1((PILL_SIZE - SCROLL_HEIGHT) / 2);
				if (start>0) GFX_blitAsset(ASSET_SCROLL_UP,   NULL, screen, &(SDL_Rect){ox, SCALE1((PADDING - (PADDING*fancy_mode))) + oy});
				if (end<count) GFX_blitAsset(ASSET_SCROLL_DOWN, NULL, screen, &(SDL_Rect){ox, screen->h - SCALE1((PADDING - (PADDING*fancy_mode)) + PILL_SIZE + BUTTON_SIZE) + oy});
			}
			
			if (!desc && list->desc) desc = list->desc;
			
			if (desc) {
				int w,h;
				GFX_sizeText(font.tiny, desc, SCALE1(12), &w,&h);
				GFX_blitText(font.tiny, desc, SCALE1(12), COLOR_WHITE, screen, &(SDL_Rect){
					(screen->w - w) / 2,
					screen->h - SCALE1((PADDING - (PADDING*fancy_mode))) - h,
					w,h
				});
			}
			
			GFX_flip(screen);
			GFX_pan();
			dirty = 0;
		}
		else GFX_sync();
	}
	
	// GFX_clearAll();
	// GFX_flip(screen);
	
	return 0;
}

static void Menu_scale(SDL_Surface* src, SDL_Surface* dst) {
	// LOG_info("Menu_scale src: %ix%i dst: %ix%i\n", src->w,src->h,dst->w,dst->h);
	
	uint16_t* s = src->pixels;
	uint16_t* d = dst->pixels;
	
	int sw = src->w;
	int sh = src->h;
	int sp = src->pitch / FIXED_BPP;
	
	int dw = dst->w;
	int dh = dst->h;
	int dp = dst->pitch / FIXED_BPP;
	
	int rx = 0;
	int ry = 0;
	int rw = dw;
	int rh = dh;
	
	int scaling = screen_scaling;
	if (scaling==SCALE_NATIVE) {
		// LOG_info("native\n");
		
		rx = renderer.dst_x;
		ry = renderer.dst_y;
		rw = renderer.src_w;
		rh = renderer.src_h;
		if (renderer.scale) {
			// LOG_info("scale: %i\n", renderer.scale);
			rw *= renderer.scale;
			rh *= renderer.scale;
		}
		else {
			// LOG_info("forced crop\n"); // eg. fc on nano, vb on smart
			rw -= renderer.src_x * 2;
			rh -= renderer.src_y * 2;
			sw = rw;
			sh = rh;
		}
		
		if (dw==DEVICE_WIDTH/2) {
			// LOG_info("halve\n");
			rx /= 2;
			ry /= 2;
			rw /= 2;
			rh /= 2;
		}
	}
	
	if (scaling==SCALE_ASPECT || rw>dw || rh>dh) {
		// LOG_info("aspect\n");
		double fixed_aspect_ratio = ((double)DEVICE_WIDTH) / DEVICE_HEIGHT;
		int core_aspect = core.aspect_ratio * 1000;
		int fixed_aspect = fixed_aspect_ratio * 1000;
		
		if (core_aspect>fixed_aspect) {
			// LOG_info("letterbox\n");
			rw = dw;
			rh = rw / core.aspect_ratio;
			rh += rh%2;
		}
		else if (core_aspect<fixed_aspect) {
			// LOG_info("pillarbox\n");
			rh = dh;
			rw = rh * core.aspect_ratio;
			rw += rw%2;
			rw = (rw/8)*8; // probably necessary here since we're not scaling by an integer
		}
		else {
			// LOG_info("perfect match\n");
			rw = dw;
			rh = dh;
		}
		
		rx = (dw - rw) / 2;
		ry = (dh - rh) / 2;
	}
	
	// LOG_info("Menu_scale (r): %i,%i %ix%i\n",rx,ry,rw,rh);
	// LOG_info("offset: %i,%i\n", renderer.src_x, renderer.src_y);

	// dumb nearest neighbor scaling
	int mx = (sw << 16) / rw;
	int my = (sh << 16) / rh;
	int ox = (renderer.src_x << 16);
	int sx = ox;
	int sy = (renderer.src_y << 16);
	int lr = -1;
	int sr = 0;
	int dr = ry * dp;
	int cp = dp * FIXED_BPP;
	
	// LOG_info("Menu_scale (s): %i,%i %ix%i\n",sx,sy,sw,sh);
	// LOG_info("mx:%i my:%i sx>>16:%i sy>>16:%i\n",mx,my,((sx+mx) >> 16),((sy+my) >> 16));

	for (int dy=0; dy<rh; dy++) {
		sx = ox;
		sr = (sy >> 16) * sp;
		if (sr==lr) {
			memcpy(d+dr,d+dr-dp,cp);
		}
		else {
	        for (int dx=0; dx<rw; dx++) {
	            d[dr + rx + dx] = s[sr + (sx >> 16)];
				sx += mx;
	        }
		}
		lr = sr;
		sy += my;
		dr += dp;
    }
	
	// LOG_info("successful\n");
}

static void Menu_initState(void) {
	if (exists(menu.slot_path)) menu.slot = getInt(menu.slot_path);
	if (menu.slot==0) menu.slot = 1;
	
	menu.save_exists = 0;
	menu.preview_exists = 0;
}
static void Menu_updateState(void) {
	// LOG_info("Menu_updateState\n");

	int last_slot = state_slot;
	state_slot = menu.slot;

	char save_path[256];
	State_getPath(save_path);

	state_slot = last_slot;
	char slotstr[5];
	if (menu.slot==0){
		sprintf(slotstr,".");
	} else {
		sprintf(slotstr,"%d.",menu.slot);
	}
	char *tmpname = game.fullname;
	if (game.m3u_path[0]) {
		getDisplayNameParens(game.m3u_path, tmpname);	
	} 
	//sprintf(menu.bmp_path, "%s/%s.state%spng", menu.minui_dir, game.basename, slotstr);
	sprintf(menu.bmp_path, "%s/%s.state%spng", core.states_dir, game.fullname, slotstr);
	sprintf(menu.txt_path, "%s/%s%stxt", menu.minui_dir, game.fullname, slotstr);
	
	menu.save_exists = exists(save_path);
	menu.preview_exists = menu.save_exists && exists(menu.bmp_path);

	// LOG_info("save_path: %s (%i)\n", save_path, menu.save_exists);
	// LOG_info("bmp_path: %s txt_path: %s (%i)\n", menu.bmp_path, menu.txt_path, menu.preview_exists);
}
static void Menu_saveState(void) {
	// LOG_info("Menu_saveState\n");

	Menu_updateState();
	
	if (menu.total_discs) {
		char* disc_path = menu.disc_paths[menu.disc];
		putFile(menu.txt_path, disc_path + strlen(menu.base_path));
		putInt(menu.txt_path_slot, menu.disc);
	}
	
	//SDL_Surface* bitmap = SDL_CreateRGBSurfaceFrom(renderer.src_surface->pixels, renderer.src_surface->w, renderer.src_surface->h, FIXED_DEPTH, renderer.src_surface->pitch, RGBA_MASK_565);
	SDL_Surface *tmpbitmap = SDL_CreateRGBSurfaceFrom(renderer.src_surface->pixels, renderer.src_surface->w, renderer.src_surface->h, FIXED_DEPTH, renderer.src_surface->pitch, RGBA_MASK_565);
	SDL_Surface *bitmap = rotozoomSurface(tmpbitmap, (4-PLAT_getScreenRotation(1))*90.0, 1.0, 1);
	SDL_FreeSurface(tmpbitmap);
	SDL_RWops* out =     SDL_RWFromFile(menu.bmp_path, "wb");
#if defined (USE_SDL2)
	IMG_SavePNG_RW(bitmap, out, 1);
#else
	SDL_SaveBMP_RW(bitmap, out, 1);
	bmp2png(menu.bmp_path);	
#endif
	
	// LOG_info("%s %ix%i\n", menu.bmp_path, bitmap->w,bitmap->h);
	
	SDL_FreeSurface(bitmap);
	state_slot = menu.slot;
	putInt(menu.slot_path, menu.slot);
	State_write();
}
static void Menu_loadState(void) {
	// LOG_info("Menu_loadState\n");

	Menu_updateState();
	
	//now useless as state loads right disk on its own?

	if (menu.save_exists && menu.total_discs) {  
		char slot_disc_name[256];
		getFile(menu.txt_path, slot_disc_name, 256);
		
		char slot_disc_path[256];
		if (slot_disc_name[0]=='/') strcpy(slot_disc_path, slot_disc_name);
		else sprintf(slot_disc_path, "%s%s", menu.base_path, slot_disc_name);
		int next_index=0;
		char* disc_path = menu.disc_paths[menu.disc];
		for (int i=0; i<menu.total_discs; i++) {
			if (exactMatch(slot_disc_path, menu.disc_paths[i])) {
				next_index = i;
				break;
			}
		}
		if (!exactMatch(slot_disc_path, disc_path)) {
			Game_changeDisc(next_index,slot_disc_path);
		}
	}
	
	state_slot = menu.slot;
	putInt(menu.slot_path, menu.slot);
	State_read();
}

static void Menu_makeboxart(void) {
	SDL_Surface* bitmap = menu.bitmap;
	if (!bitmap) bitmap = SDL_CreateRGBSurfaceFrom(renderer.src, renderer.true_w, renderer.true_h, FIXED_DEPTH, renderer.src_p, RGBA_MASK_565);
	char myEmuName[256];
	char myRomName[256];
	char bmppath[512];
	//strcpy(dirname1,game.path);
	//getParentFolderName(dirname1,dirname2);
	//LOG_info("#####game.path###### = %s/Imgs\n", game.path);
	getParentFolderName(game.path, myEmuName);
	getDisplayNameParens(game.path, myRomName);
	sprintf(bmppath, ROMS_PATH "/%s/Imgs/%s.png", myEmuName , myRomName);
	LOG_info("#####boxart name###### = %s\n", bmppath);
	makeBoxart(bitmap,bmppath);

	if (bitmap!=menu.bitmap) SDL_FreeSurface(bitmap);
}

static int getAlias(char* path, char* alias) {
	LOG_info("alias path: %s\n", path);
	char* tmp;
	char map_path[256];
	strcpy(map_path, path);
	tmp = strrchr(map_path, '/');
	if (tmp) {
		tmp += 1;
		strcpy(tmp, "map.txt");
		LOG_info("map_path: %s\n", map_path);
	}
	char* file_name = strrchr(path,'/');
	if (file_name) file_name += 1;
	LOG_info("file_name: %s\n", file_name);
	
	if (exists(map_path)) {
		FILE* file = fopen(map_path, "r");
		if (file) {
			char line[256];
			while (fgets(line,256,file)!=NULL) {
				normalizeNewline(line);
				trimTrailingNewlines(line);
				if (strlen(line)==0) continue; // skip empty lines
			
				tmp = strchr(line,'\t');
				if (tmp) {
					tmp[0] = '\0';
					char* key = line;
					char* value = tmp+1;
					if (exactMatch(file_name,key)) {
						strcpy(alias, value);
						return 1;
					}
				}
			}
			fclose(file);
		}
	}
	return 0;
}

static void Menu_loop(void) {

	LOG_info("Entering Menu render= %i - rendering= %i\n",render,rendering);fflush(stdout);
	//wait_For_Thread();
/*	int rendering2 = 1500;
	while (((render!=0) || (rendering!=0)) && (rendering2>0)) { 
		//LOG_info("rendering in Menu_loop render = %i - rendering = %i\n",render,rendering);fflush(stdout);
		usleep(1000);
		rendering2--; //waiting a bit ensure that menu won't crash even on some cores (i.e. dosbox)
	}
	rendering = 0;
	render = 0;*/
	if (firstmenu) PLAT_clearAll();
	firstmenu = 0;
	//SDL_FillRect(screengame, NULL, 0xFFFF0000);
//	menu.bitmap = SDL_CreateRGBSurfaceFrom(screengame->pixels, DEVICE_WIDTH, DEVICE_HEIGHT, FIXED_DEPTH, DEVICE_PITCH, RGBA_MASK_565);
//	LOG_info("Create tmpbitmap\n");fflush(stdout);
	//check if the bitmap must be rotated:
//	int gamerot, screenrot;
	//gamerot = PLAT_getScreenRotation(1);
	//screenrot = PLAT_getScreenRotation(0);
	//if (gamerot!=screenrot) {
		//menu.bitmap must be rotated
	//}
	//menu.bitmap = rotateSurface90Degrees(screengame, 3);
	menu.bitmap = rotozoomSurface(screengame, (4-PLAT_getScreenRotation(1))*90.0, 1.0, 1);
	SDL_Surface* backing = SDL_CreateRGBSurface(SDL_SWSURFACE,DEVICE_WIDTH,DEVICE_HEIGHT,FIXED_DEPTH,RGBA_MASK_565); 
	SDL_BlitSurface(menu.bitmap, NULL, backing, NULL);
	int restore_w = screen->w;
	int restore_h = screen->h;
	int restore_p = screen->pitch;
	//if (restore_w!=DEVICE_WIDTH || restore_h!=DEVICE_HEIGHT || restore_p!=DEVICE_PITCH) {
	
	LOG_info("Menu_loop begin call GFX_resize %i %i %i\n",DEVICE_WIDTH,DEVICE_HEIGHT,DEVICE_PITCH);fflush(stdout);
	screen = GFX_resize(DEVICE_WIDTH,DEVICE_HEIGHT,DEVICE_PITCH);
	//}
	
	SRAM_write();
	RTC_write();
	PWR_warn(0);
	if (!HAS_POWER_BUTTON) PWR_enableSleep();
	PWR_setCPUSpeed(CPU_SPEED_MENU); // set Hz directly
	GFX_setVsync(VSYNC_STRICT);
	GFX_setEffect(EFFECT_NONE);
	
	int rumble_strength = VIB_getStrength();
	VIB_setStrength(0);
	
	PWR_enableAutosleep();
	PAD_reset();
	
	// if (!HAS_POWER_BUTTON && !HAS_POWEROFF_BUTTON) {
	// 	MenuItem* item = &options_menu.items[5];
	// 	item->name = "Quicksave";
	// 	item->desc = "Automatically resume current state next power on.";
	// 	item->on_confirm = OptionQuicksave_onConfirm;
	// }
	
	// path and string things
	char* tmp;
	char rom_name[256]; // without extension or cruft
	char rom_name2[256]; // without extension or cruft
	
	if (getAlias(game.path, rom_name2)){
		// remove trailing parens (round and square)
		char *tmp;
		while ((tmp=strrchr(rom_name2, '('))!=NULL || (tmp=strrchr(rom_name2, '['))!=NULL) {
			if (tmp==rom_name2) 
				break;
			tmp[0] = '\0';
			tmp = rom_name2;
		}
		strcpy(rom_name, rom_name2);
	} else {
		getDisplayName(game.name, rom_name);
	}
	int rom_disc = -1;
	char disc_name[16];
	if (menu.total_discs) {
		if (coreDiscManaged) {
			menu.disc = disk_control_ext.get_image_index();
		}		
		rom_disc = menu.disc;
		sprintf(disc_name, "Disc %i", menu.disc+1);
	}
		
	int selected = 0; // resets every launch
	Menu_initState();
	
	int status = STATUS_CONT; // TODO: no longer used?
	int show_setting = 0;
	int dirty = 1;
	int ignore_menu = 0;
	int menu_start = 0;
	
	SDL_Surface* preview = SDL_CreateRGBSurface(SDL_SWSURFACE,DEVICE_WIDTH/2,DEVICE_HEIGHT/2,FIXED_DEPTH,RGBA_MASK_565); // TODO: retain until changed?
	
	while (show_menu) {
		GFX_startFrame();
		uint32_t now = SDL_GetTicks();

		PAD_poll();
		
		if (PAD_justPressed(BTN_UP)) {
			selected -= 1;
			if (selected<0) selected += MENU_ITEM_COUNT;
			dirty = 1;
		}
		else if (PAD_justPressed(BTN_DOWN)) {
			selected += 1;
			if (selected>=MENU_ITEM_COUNT) selected -= MENU_ITEM_COUNT;
			dirty = 1;
		}
		else if (PAD_justPressed(BTN_LEFT)) {
			if (menu.total_discs>1 && selected==ITEM_CONT) {
				menu.disc -= 1;
				if (menu.disc<0) menu.disc += menu.total_discs;
				dirty = 1;
				sprintf(disc_name, "Disc %i", menu.disc+1);
			}
			else if (selected==ITEM_SAVE || selected==ITEM_LOAD) {
				menu.slot -= 1;
				if (menu.slot<1) menu.slot += MENU_SLOT_COUNT;
				dirty = 1;
			}
		}
		else if (PAD_justPressed(BTN_RIGHT)) {
			if (menu.total_discs>1 && selected==ITEM_CONT) {
				menu.disc += 1;
				if (menu.disc==menu.total_discs) menu.disc -= menu.total_discs;
				dirty = 1;
				sprintf(disc_name, "Disc %i", menu.disc+1);
			}
			else if (selected==ITEM_SAVE || selected==ITEM_LOAD) {
				menu.slot += 1;
				if (menu.slot>MENU_SLOT_COUNT) menu.slot -= MENU_SLOT_COUNT;
				dirty = 1;
			}
		}
		
		if (dirty && (selected==ITEM_SAVE || selected==ITEM_LOAD)) {
			Menu_updateState();
		}
		
		if (PAD_justPressed(BTN_Y)) {
			Menu_makeboxart();		
		}

		if (PAD_justPressed(BTN_B) || (BTN_WAKE!=BTN_MENU && PAD_tappedMenu(now))) {
			status = STATUS_CONT;
			show_menu = 0;
		}
		else if (PAD_justPressed(BTN_A)) {
			switch(selected) {
				case ITEM_CONT:
				LOG_info("MENU: Num discs=%d Changing disc from %d to %d - %s\n", menu.total_discs,rom_disc, menu.disc, menu.disc_paths[menu.disc]);
				if (menu.total_discs && rom_disc!=menu.disc) {
						status = STATUS_DISC;
						char* disc_path = menu.disc_paths[menu.disc];
						LOG_info("MENU: Changing disc from %d to %d - %s\n", rom_disc, menu.disc, disc_path);
						Game_changeDisc(menu.disc,disc_path);
						sleep(2);
						core.reset();
					}
					else {
						status = STATUS_CONT;
					}
					show_menu = 0;
				break;
				
				case ITEM_SAVE: {
					Menu_saveState();
					status = STATUS_SAVE;
					show_menu = 0;
				}
				break;
				case ITEM_LOAD: {
					Menu_loadState();
					status = STATUS_LOAD;
					show_menu = 0;
				}
				break;
				/*case ITEM_BOXART: {
					Menu_makeboxart();
					status = STATUS_BOXART;
					show_menu = 1;
				}
				break;*/
				case ITEM_OPTS: {
					if (simple_mode) {
						core.reset();
						status = STATUS_RESET;
						show_menu = 0;
					}
					else {
						int old_scaling = screen_scaling;
						Menu_options(&options_menu);
						if (screen_scaling!=old_scaling) {
							restore_w = renderer.dst_w;
							restore_h = renderer.dst_h;
							restore_p = renderer.dst_p; // screen->pitch;
							LOG_info("Menu_loop change aspect call GFX_resize %i %i %i\n", DEVICE_WIDTH, DEVICE_HEIGHT, DEVICE_PITCH);fflush(stdout);
							screen = GFX_resize(DEVICE_WIDTH,DEVICE_HEIGHT,DEVICE_PITCH);
							SDL_FillRect(backing, NULL, 0);
							SDL_BlitSurface(menu.bitmap, NULL, backing, NULL);
						}
						dirty = 1;
					}
				}
				break;
				case ITEM_QUIT:
					status = STATUS_QUIT;
					show_menu = 0;
					quit = 1; // TODO: tmp?
				break;
			}
			if (!show_menu) break;
		}

		PWR_update(&dirty, &show_setting, Menu_beforeSleep, Menu_afterSleep);
		
		if (dirty) {
			GFX_clear(screen);
			
			SDL_BlitSurface(backing, NULL, screen, NULL);
			SDL_BlitSurface(menu.overlay, NULL, screen, NULL);

			int ox, oy;
			int ow = GFX_blitHardwareGroup(screen, show_setting, fancy_mode);
			int max_width = screen->w - SCALE1((PADDING - (PADDING*fancy_mode)) * 2) - ow;
			
			char display_name[256];
			int text_width = GFX_truncateText(font.large, rom_name, display_name, max_width, SCALE1(BUTTON_PADDING*2));
			max_width = MIN(max_width, text_width);

			SDL_Surface* text;
			text = TTF_RenderUTF8_Blended(font.large, display_name, COLOR_WHITE);
			GFX_blitPill(ASSET_BLACK_PILL, screen, &(SDL_Rect){
				SCALE1((PADDING - (PADDING*fancy_mode))),
				SCALE1((PADDING - (PADDING*fancy_mode))),
				max_width,
				SCALE1(PILL_SIZE)
			});
			SDL_BlitSurface(text, &(SDL_Rect){
				0,
				0,
				max_width-SCALE1(BUTTON_PADDING*2),
				text->h
			}, screen, &(SDL_Rect){
				SCALE1((PADDING - (PADDING*fancy_mode))+BUTTON_PADDING),
				SCALE1((PADDING - (PADDING*fancy_mode))+4)
			});
			SDL_FreeSurface(text);
			if (show_setting && !GetHDMI()) {
				 GFX_blitHardwareHints(screen, show_setting, fancy_mode);
			} else {
				GFX_blitButtonGroup((char*[]){ BTN_SLEEP==BTN_POWER?"PWR":"MENU",pwractionstr, "Y","BOXART", NULL }, 0, screen, 0, fancy_mode);
			}
			
			GFX_blitButtonGroup((char*[]){ "B","BACK", "A","OKAY", NULL }, 1, screen, 1, fancy_mode);
			
			// list
			oy = (((DEVICE_HEIGHT / FIXED_SCALE) - ((PADDING - (PADDING*fancy_mode))* 2)) - (MENU_ITEM_COUNT * PILL_SIZE)) / 2;
			for (int i=0; i<MENU_ITEM_COUNT; i++) {
				char* item = menu.items[i];
				SDL_Color text_color = COLOR_WHITE;
				
				if (i==selected) {
					// disc change
					if (menu.total_discs>1 && i==ITEM_CONT) {				
						GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){
							SCALE1((PADDING - (PADDING*fancy_mode))),
							SCALE1(oy + (PADDING - (PADDING*fancy_mode))),
							screen->w - SCALE1((PADDING - (PADDING*fancy_mode)) * 2),
							SCALE1(PILL_SIZE)
						});
						text = TTF_RenderUTF8_Blended(font.large, disc_name, COLOR_WHITE);
						SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
							screen->w - SCALE1((PADDING - (PADDING*fancy_mode)) + BUTTON_PADDING) - text->w,
							SCALE1(oy + (PADDING - (PADDING*fancy_mode)) + 4)
						});
						SDL_FreeSurface(text);
					}
					
					TTF_SizeUTF8(font.large, item, &ow, NULL);
					ow += SCALE1(BUTTON_PADDING*2);
					
					// pill
					GFX_blitPill(ASSET_WHITE_PILL, screen, &(SDL_Rect){
						SCALE1((PADDING - (PADDING*fancy_mode))),
						SCALE1(oy + (PADDING - (PADDING*fancy_mode)) + (i * PILL_SIZE)),
						ow,
						SCALE1(PILL_SIZE)
					});
					text_color = COLOR_BLACK;
				}
				else {
					// shadow
					text = TTF_RenderUTF8_Blended(font.large, item, COLOR_BLACK);
					SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
						SCALE1(2 + (PADDING - (PADDING*fancy_mode)) + BUTTON_PADDING),
						SCALE1(1 + (PADDING - (PADDING*fancy_mode)) + oy + (i * PILL_SIZE) + 4)
					});
					SDL_FreeSurface(text);
				}
				
				// text
				text = TTF_RenderUTF8_Blended(font.large, item, text_color);
				SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
					SCALE1((PADDING - (PADDING*fancy_mode)) + BUTTON_PADDING),
					SCALE1(oy + (PADDING - (PADDING*fancy_mode)) + (i * PILL_SIZE) + 4)
				});
				SDL_FreeSurface(text);
			}
			/*
			// slot preview
			if (selected==ITEM_SAVE || selected==ITEM_LOAD) {
				#define WINDOW_RADIUS 4 // TODO: this logic belongs in blitRect?
				#define PAGINATION_HEIGHT 6
				// unscaled
				int hw = DEVICE_WIDTH / 2;
				int hh = DEVICE_HEIGHT / 2;
				int pw = hw + SCALE1(WINDOW_RADIUS*2);
				int ph = hh + SCALE1(WINDOW_RADIUS*2 + PAGINATION_HEIGHT + WINDOW_RADIUS);
				ox = DEVICE_WIDTH - pw - SCALE1(PADDING);
				oy = (DEVICE_HEIGHT - ph) / 2;
				
				// window
				GFX_blitRect(ASSET_STATE_BG, screen, &(SDL_Rect){ox,oy,pw,ph});
				ox += SCALE1(WINDOW_RADIUS);
				oy += SCALE1(WINDOW_RADIUS);
				
				if (menu.preview_exists) { // has save, has preview
					// lotta memory churn here
					SDL_Surface* bmp = IMG_Load(menu.bmp_path);
					SDL_Surface* raw_preview = SDL_ConvertSurface(bmp, screen->format, SDL_SWSURFACE);
					
					// LOG_info("raw_preview %ix%i\n", raw_preview->w,raw_preview->h);
					
					SDL_FillRect(preview, NULL, 0);
					Menu_scale(raw_preview, preview);
					SDL_BlitSurface(preview, NULL, screen, &(SDL_Rect){ox,oy});
					SDL_FreeSurface(raw_preview);
					SDL_FreeSurface(bmp);
				}
				else {
					SDL_Rect preview_rect = {ox,oy,hw,hh};
					SDL_FillRect(screen, &preview_rect, 0);
					if (menu.save_exists) GFX_blitMessage(font.large, "No Preview", screen, &preview_rect);
					else GFX_blitMessage(font.large, "Empty Slot", screen, &preview_rect);
				}
				
				// pagination
				ox += (pw-SCALE1(15*MENU_SLOT_COUNT))/2;
				oy += hh+SCALE1(WINDOW_RADIUS);
				for (int i=0; i<MENU_SLOT_COUNT; i++) {
					if (i==menu.slot)GFX_blitAsset(ASSET_PAGE, NULL, screen, &(SDL_Rect){ox+SCALE1(i*15),oy});
					else GFX_blitAsset(ASSET_DOT, NULL, screen, &(SDL_Rect){ox+SCALE1(i*15)+4,oy+SCALE1(2)});
				}
			} */

			// my slot preview enlarged to 384x288 for better see 
			if (selected==ITEM_SAVE || selected==ITEM_LOAD) {
				#define WINDOW_RADIUS 4 // TODO: this logic belongs in blitRect?
				#define PAGINATION_HEIGHT 6
				// unscaled
				int hw = DEVICE_WIDTH * 3 / 5;
				int hh = DEVICE_HEIGHT * 3 / 5;
				int pw = hw + SCALE1(WINDOW_RADIUS*2);
				int ph = hh + SCALE1(WINDOW_RADIUS*2 + PAGINATION_HEIGHT);
				ox = DEVICE_WIDTH - pw;
				oy = (DEVICE_HEIGHT - ph) / 2;
							
				// window
				GFX_blitRect(ASSET_STATE_BG, screen, &(SDL_Rect){ox,oy,pw,ph});
				ox += SCALE1(WINDOW_RADIUS);
				oy += SCALE1(WINDOW_RADIUS);
				
				if (menu.preview_exists) { // has save, has preview
					// lotta memory churn here
					SDL_Surface* bmp = IMG_Load(menu.bmp_path);
					if (!bmp) {
						SDL_Rect preview_rect = {ox,oy,hw,hh};
						SDL_FillRect(screen, &preview_rect, 0);
						GFX_blitMessage(font.large, "No Preview", screen, &preview_rect);
					} else {
						SDL_Surface* preview = zoomSurface(bmp, (1.0 * hw / bmp->w) , (1.0 * hh / bmp->h), 0);					
						SDL_BlitSurface(preview, NULL, screen, &(SDL_Rect){ox,oy}); 
						if (bmp) SDL_FreeSurface(bmp);   
						if (preview) SDL_FreeSurface(preview);					
					}
					
				}
				else {
					SDL_Rect preview_rect = {ox,oy,hw,hh};
					SDL_FillRect(screen, &preview_rect, 0);
					if (menu.save_exists) GFX_blitMessage(font.large, "No Preview", screen, &preview_rect);
					else GFX_blitMessage(font.large, "Empty Slot", screen, &preview_rect);
				}
				
				// pagination
				ox += (pw-SCALE1(16*MENU_SLOT_COUNT))/2;
				oy += hh+SCALE1(WINDOW_RADIUS)-SCALE1(PAGINATION_HEIGHT / 2 - 1);
				for (int i=1; i<MENU_SLOT_COUNT+1; i++) {
					if (i==menu.slot)GFX_blitAsset(ASSET_PAGE, NULL, screen, &(SDL_Rect){ox+SCALE1((i-1)*16),oy});
					else GFX_blitAsset(ASSET_DOT, NULL, screen, &(SDL_Rect){ox+SCALE1((i-1)*16)+4,oy+SCALE1(2)});
				}
				if (menu.slot == 0){
					ox -= SCALE1(16);		
					GFX_blitAsset(ASSET_RED_PAGE, NULL, screen, &(SDL_Rect){ox,oy});
				}
			}
	
			GFX_flip(screen);
			GFX_pan();
			dirty = 0;
		}
		else GFX_sync();
	}
	
	SDL_FreeSurface(preview);
	
	PAD_reset();

	GFX_clearAll();
	PWR_warn(1);
	
	if (!quit) {

		LOG_info("Menu_loop exit from menu call GFX_resize %i %i %i\n", restore_w, restore_h, restore_p);fflush(stdout);
		screen = GFX_resize(restore_w,restore_h,restore_p);
		GFX_setEffect(screen_effect);
		GFX_clear(screen);
		

		setOverclock(overclock); // restore overclock value
		if (rumble_strength) VIB_setStrength(rumble_strength);
		
		GFX_setVsync(prevent_tearing);
		if (!HAS_POWER_BUTTON) PWR_disableSleep();


		if (thread_video) {
			LOG_info("Exiting menu, returning to video_thread\n");fflush(stdout);
			pthread_mutex_lock(&core_mx);
			should_run_core = 1;
			pthread_mutex_unlock(&core_mx);		
		} else {
			video_refresh_callback(renderer.src, renderer.true_w, renderer.true_h, renderer.src_p);
		}
	}
	
	SDL_FreeSurface(menu.bitmap);
	menu.bitmap = NULL;
	SDL_FreeSurface(backing);
	PWR_disableAutosleep();
	firstframe = 1;
}

// TODO: move to PWR_*?
static unsigned getUsage(void) { // from picoarch
	long unsigned ticks = 0;
	long ticksps = 0;
	FILE *file = NULL;

	file = fopen("/proc/self/stat", "r");
	if (!file)
		goto finish;

	if (!fscanf(file, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu", &ticks))
		goto finish;

	ticksps = sysconf(_SC_CLK_TCK);

	if (ticksps)
		ticks = ticks * 100 / ticksps;
	
finish:
	if (file)
		fclose(file);

	return ticks;
}


static long a[9];
static long up[5], up1[5], idle[5], idle1[5];
void getCPUusage(char * data) {
	int counter;
  	int i;
  	float load;
  	FILE* fp;
  	char *scrap;
	data[0] = '\0';
	scrap=(char *)malloc(15);
    fp = fopen ("/proc/stat", "r");
	for (counter = 0; counter <= processors; counter++)
    {
       up1[counter]=0;
       fscanf (fp, "%s %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",scrap,&a[0],&a[1],&a[2],&a[3],&a[4],&a[5],&a[6],&a[7],&a[8],&a[9]);
       if (strncmp(scrap,"cpu",3) != 0) break;
	   if (counter < 1) continue;
       for (i =0; i < 8; i++) up1[counter]+= a[i];
       idle1[counter] = a[3]+a[4];
       up[counter]=up1[counter]-up[counter];
       idle[counter]=idle1[counter]-idle[counter];
       load=((float)(up[counter]-idle[counter])/(float)up[counter])*100.0;
// now you have the load..
        sprintf(data,"%s%03.0f%%", data, load);
        //if (counter == processors) printf("\n");
// update staic variables for next loop
       up[counter]=up1[counter];
       idle[counter]=idle1[counter];
     }
	sprintf(data,"%s%s", data, (thread_video == 1) ? "T": " ");
	fclose(fp);
}


static void chooseSyncRef(void) {
#if defined(NO_VSYNC)
	use_core_fps = 1;
#else
	switch (sync_ref) {
		case SYNC_SRC_AUTO:   use_core_fps = (core.get_region() == RETRO_REGION_PAL); break;
		case SYNC_SRC_SCREEN: use_core_fps = 0; break;
		case SYNC_SRC_CORE:   use_core_fps = 1; break;
	}
#endif
	LOG_info("%s: sync_ref is set to %s, game region is %s, use core fps = %s\n",
		  __FUNCTION__,
		  sync_ref_labels[sync_ref],
		  core.get_region() == RETRO_REGION_NTSC ? "NTSC" : "PAL",
		  use_core_fps ? "yes" : "no");

}


static void trackFPS(void) {
	if (!show_debug) return;

	uint32_t now = SDL_GetTicks();
	if (now - sec_start>=1000) {
		double last_time = (double)(now - sec_start) / 1000;
		fps_double = fps_ticks / last_time;
		fps2_double = fps2_ticks / last_time;
#ifndef M21
//		getCPUusage(cpuload);
#endif
		sec_start = now;
		fps_ticks = 0;
		fps2_ticks = 0;		
		// LOG_info("fps: %f cpu: %f\n", fps_double, cpu_double);
	}
}

static void limitFF(void) {
	if (! fast_forward) return;
	static uint64_t ff_frame_time = 0;
	static uint64_t last_time = 0;
	static int last_max_speed = -1;
	if (last_max_speed!=max_ff_speed) {
		last_max_speed = max_ff_speed;
		ff_frame_time = 1000000 / (core.fps * (max_ff_speed + 1));
	}
	
	uint64_t now = getMicroseconds();
	if (fast_forward && max_ff_speed) {
		if (last_time == 0) last_time = now;
		int elapsed = now - last_time;
		if (elapsed>0 && elapsed<0x80000) {
			if (elapsed<ff_frame_time) {
				int delay = (ff_frame_time - elapsed) / 1000;
				if (delay>0 && delay<17) { // don't allow a delay any greater than a frame
					SDL_Delay(delay);
				}
			}
			last_time += ff_frame_time;
			return;
		}
	}
	last_time = now;
}
cpu_set_t coret;
cpu_set_t flipt;

static void* flipThread(void *arg) {
	int run = 0;
	int render_ = 0;
	LOG_info("flipThread started now on cpu %i\n", sched_getcpu());fflush(stdout);
	int moved = pthread_setaffinity_np(flip_pt, sizeof(cpu_set_t), &flipt);
	LOG_info("flipThread moved to cpu %i with result %i\n", sched_getcpu(), moved);fflush(stdout);
	flipThreadStarted = 1;
	while (!quit) {
		pthread_mutex_lock(&flip_mx);
		run = should_run_flip;
		render_ = render;
		pthread_mutex_unlock(&flip_mx);
#ifdef M210
		if (run && render_) {
			trackFPS();
			video_refresh_callback_main(backbuffer.pixels,backbuffer.w,backbuffer.h,backbuffer.pitch);
			pthread_mutex_lock(&flip_mx);
#else
		if (run) {
			trackFPS();
			pthread_mutex_lock(&flip_mx);
			pthread_cond_wait(&flip_rq, &flip_mx);	
			video_refresh_callback_main(backbuffer.pixels,backbuffer.w,backbuffer.h,backbuffer.pitch);
#endif
			render = 0;
			pthread_mutex_unlock(&flip_mx);
		} else { //should never happen
			sleep(0);
		}	
	}
	pthread_exit(NULL);
}
static void* coreThread(void *arg) {
	LOG_info("coreThread started now on cpu %i\n", sched_getcpu());fflush(stdout);
	int moved = pthread_setaffinity_np(core_pt, sizeof(cpu_set_t), &coret);
	LOG_info("coreThread moved to cpu %i with result %i\n", sched_getcpu(),moved);fflush(stdout);
	coreThreadStarted = 1;
	while (!quit) {
		int run = 0;
		pthread_mutex_lock(&core_mx);
		run = should_run_core;
		pthread_mutex_unlock(&core_mx);
		if (run) {
			core.run();
			limitFF();
			//trackFPS();
		} else { //to keep cpu low while in the menu
			sleep(0);
		}
	}
	pthread_exit(NULL);
}


static void resetFPSCounter() {
	sec_start = SDL_GetTicks();
	fps_ticks = 0;
	fps2_ticks = 0;
}


int main(int argc , char* argv[]) {
	LOG_info("MinArch Date:%s Commit:%s\n", BUILD_DATE, BUILD_HASH);
	cpu_set_t maint;
	IS_MINARCH=1;

	setOverclock(overclock); // default to normal
	// force a stack overflow to ensure asan is linked and actually working
	// char tmp[2];
	// tmp[2] = 'a';
	processors = PLAT_getNumProcessors();
	CPU_ZERO(&maint);
	CPU_ZERO(&flipt);
	CPU_ZERO(&coret);
	CPU_SET(0, &maint);
	if (processors > 2){
		//this is a quadcore cpu, set main thread to cpu 2, flip thread to cpu 3 and corethread to cpu 4
		LOG_info("Quadcore CPU detected\n");
		CPU_SET(2, &flipt);
		CPU_SET(1, &coret);
	} else {
		//this is a dual core cpu, set main thread to cpu 0, flip thread to cpu 0 and corethread to cpu 1
		LOG_info("Dualcore CPU detected\n");
		CPU_SET(0, &flipt);
		CPU_SET(1, &coret);
	}
	char core_path[MAX_PATH];
	char rom_path[MAX_PATH]; 
	char tag_name[MAX_PATH];
	int resume_slot;

	mutedaudiodata = calloc(2000,sizeof(uint32_t));
	int has_pending_opt_change = 0;

	//backbuffer.pixels = (uint16_t*)malloc(MAX_WIDTH*MAX_HEIGHT*sizeof(uint16_t));
	
	backbuffer.w = MAX_WIDTH;
	backbuffer.h = MAX_HEIGHT;
	backbuffer.pitch = backbuffer.w*sizeof(uint16_t);
	backbuffer.size = backbuffer.pitch*backbuffer.h;
	//backbuffer.pixels = (uint16_t*)malloc(backbuffer.size);
	posix_memalign((void **)&backbuffer.pixels,16,backbuffer.size);
	backbuffer.depth = -1;

	renderer.src_surface = malloc(sizeof(struct mybackbuffer));//backbuffer.size);
	posix_memalign((void **)&renderer.src_surface->pixels,16,backbuffer.size);

	renderer.rotate = 0; //set default rotation to 0 deg
#ifdef M21 //if hdmi cable is detected the audio is routed to hdmi instead of speakers, specific for SJGAM M21.
	PLAT_getAudioOutput();
#endif
	fancy_mode = exists(FANCY_MODE_PATH);
	
	strcpy(core_path, argv[1]);
	strcpy(rom_path, argv[2]);
	resume_slot=strtol(argv[3],NULL,10);
	getEmuName(rom_path, tag_name);
	
	LOG_info("rom_path: %s\n", rom_path);

#ifdef M21
	preInitSettings();
#endif	
	if (exists(PWR_SLEEP_PATH)){
		sprintf(pwractionstr,"SLP");
	} else {
		sprintf(pwractionstr,"OFF");
	}

	screen = GFX_init(MODE_MENU);
	screengame = PLAT_getScreenGame();
	renderer.rotate = (renderer.rotate + PLAT_getScreenRotation(1)) & 3;
	setOverclock(overclock); // default to normal
	PAD_init();
	//DEVICE_WIDTH = screen->w; // yea or nay?
	//DEVICE_HEIGHT = screen->h; // yea or nay?
	//DEVICE_PITCH = screen->pitch; // yea or nay?
	// LOG_info("DEVICE_SIZE: %ix%i (%i)\n", DEVICE_WIDTH,DEVICE_HEIGHT,DEVICE_PITCH);
	
	VIB_init();
	PWR_init();
	if (!HAS_POWER_BUTTON) PWR_disableSleep();
	MSG_init();

	// Overrides_init();

	
	Core_open(core_path, tag_name);
	Game_open(rom_path);
	if (!game.is_open) goto finish;
	
	simple_mode = exists(SIMPLE_MODE_PATH);
	
	// restore options
	Config_load();
	Config_init();
	Config_readOptions(); // cores with boot logo option (eg. gb) need to load options early
	setOverclock(overclock);
	GFX_setVsync(prevent_tearing);
	
	Core_init();
	
	// TODO: find a better place to do this
	// mixing static and loaded data is messy
	// why not move to Core_init()?
	// ah, because it's defined before options_menu...
	options_menu.items[1].desc = (char*)core.version;
	
	Core_load();
	Input_init(NULL);
	Config_readOptions(); // but others load and report options later (eg. nes)
	Config_readControls(); // restore controls (after the core has reported its defaults)
	Config_free();
		
	SND_init(core.sample_rate, core.fps);
	InitSettings(); // after we initialize audio
	Menu_init();
	State_resume(resume_slot);
	resume_slot=-1;
	Menu_initState(); // make ready for state shortcuts
	
	
	core_mx = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	core_rq = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
	pthread_create(&core_pt, NULL, &coreThread, NULL);
	flip_mx = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	flip_rq = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
	pthread_create(&flip_pt, NULL, &flipThread, NULL);	
	
	PWR_warn(1);
	PWR_disableAutosleep();

	// force a vsync immediately before loop
	// for better frame pacing?
	GFX_clearAll();
	GFX_flip(screen);
	GFX_pan();

	sec_start = SDL_GetTicks();

	if ((renderer.rotate % 2) == 1 ) {
		core.aspect_ratio = 1.0 / core.aspect_ratio;
	}
	quit = !loadgamesuccess;
	waiting_for_thread_stop = 0;
	int currentmaincpu = sched_getcpu();
	LOG_info("DETECTED NUM %i CPUS\n", processors);
	LOG_info("MAIN THREAD RUNNING ON CPU %i\n", currentmaincpu);
	sched_setaffinity(0, sizeof(cpu_set_t), &maint);
	currentmaincpu = sched_getcpu();
	LOG_info("MAIN THREAD MOVED TO CPU %i\n", currentmaincpu);
	while (!quit) {
		GFX_startFrame();
		if (has_pending_opt_change) {
			has_pending_opt_change = 0;
			if (Core_updateAVInfo()) {
				LOG_info("AV info changed, reset sound system");
				SND_resetAudio(core.sample_rate, core.fps);
			}
			resetFPSCounter();
			chooseSyncRef();
		}
		if ((!thread_video)&&(config_load_done)&&(!waitforthread)) {
			core.run();
			limitFF();
			trackFPS();
		}

		if (thread_video && !quit) {
			pthread_mutex_lock(&core_mx);
			pthread_cond_wait(&core_rq,&core_mx);
			rendering = 0;
			pthread_mutex_unlock(&core_mx);
			pthread_mutex_lock(&flip_mx);
			render = 1;
#ifndef M210
			pthread_cond_signal(&flip_rq);
#endif
			pthread_mutex_unlock(&flip_mx);	
#ifndef MIYOOMINI	//don't know why this makes Miyoomini cry :-)				
			while (waiting_for_thread_stop) {
				asm("nop");
			//		usleep(0);
			}
#endif
		}
		
		if (show_menu) { 
			Menu_loop();
			has_pending_opt_change = config.core.changed;
			resetFPSCounter();
			chooseSyncRef();
		}
		
		//trackFPS();
		if (toggle_thread) {
			toggle_thread = 0;
			// LOG_info("toggling thread from %i to %i\n", thread_video, !thread_video);
			thread_video = !thread_video;
			if (thread_video) {
				// enable
				pthread_mutex_lock(&flip_mx);
				should_run_flip = 1;
				pthread_mutex_unlock(&flip_mx);
				pthread_mutex_lock(&core_mx);
				should_run_core = 1;
				pthread_mutex_unlock(&core_mx);
				LOG_info("Here I started the threads 1\n");fflush(stdout);
				config_load_done = 1;
			}
			else {
				// disable
				pthread_mutex_lock(&core_mx);
				should_run_core = 0;
				pthread_mutex_unlock(&core_mx);
				pthread_mutex_lock(&flip_mx);
				should_run_flip = 0;
				pthread_mutex_unlock(&flip_mx);	

				// force a vsync immediately before loop
				// for better frame pacing?
				GFX_clearAll();
				GFX_flip(screen);
				GFX_pan();
			}
		}
		if ((!waitforthread)&&(!config_load_done)) {
			config_load_done = 1;
		}
	}
	
	Menu_quit();
	QuitSettings();
	
finish:

	Game_close();
	Core_unload();
	Core_quit();
	Core_close();
	
	Config_quit();
	
	MSG_quit();
	PWR_quit();
	VIB_quit();
	SND_quit();
	PAD_quit();
	GFX_quit();
//	save_storage_audio_timing();
	free(backbuffer.pixels);
	free(renderer.src_surface->pixels);
	free(renderer.src_surface);
	free(mutedaudiodata);
	
	return EXIT_SUCCESS;
}
