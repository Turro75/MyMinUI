#include <stdio.h>
#include <stdlib.h>
#include <msettings.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

#include "defines.h"
#include "api.h"
#include "utils.h"

#if defined(USE_SDL2)
#include "SDL2_rotozoom.h"
#else
#include "SDL_rotozoom.h"
#endif
///////////////////////////////////////

typedef struct Array {
	int count;
	int capacity;
	void** items;
} Array;

static int selected_modifier;

static Array* Array_new(void) {
	Array* self = malloc(sizeof(Array));
	self->count = 0;
	self->capacity = 8;
	self->items = malloc(sizeof(void*) * self->capacity);
	return self;
}
static void Array_push(Array* self, void* item) {
	if (self->count>=self->capacity) {
		self->capacity *= 2;
		self->items = realloc(self->items, sizeof(void*) * self->capacity);
	}
	self->items[self->count++] = item;
}
static void Array_unshift(Array* self, void* item) {
	if (self->count==0) return Array_push(self, item);
	Array_push(self, NULL); // ensures we have enough capacity
	for (int i=self->count-2; i>=0; i--) {
		self->items[i+1] = self->items[i];
	}
	self->items[0] = item;
}
static void* Array_pop(Array* self) {
	if (self->count==0) return NULL;
	return self->items[--self->count];
}
static void Array_reverse(Array* self) {
	int end = self->count-1;
	int mid = self->count/2;
	for (int i=0; i<mid; i++) {
		void* item = self->items[i];
		self->items[i] = self->items[end-i];
		self->items[end-i] = item;
	}
}
static void Array_free(Array* self) {
	free(self->items); 
	free(self);
}

static int StringArray_indexOf(Array* self, char* str) {
	for (int i=0; i<self->count; i++) {
		if (exactMatch(self->items[i], str)) return i;
	}
	return -1;
}
static void StringArray_free(Array* self) {
	for (int i=0; i<self->count; i++) {
		free(self->items[i]);
	}
	Array_free(self);
}

///////////////////////////////////////

typedef struct Hash {
	Array* keys;
	Array* values;
} Hash; // not really a hash

static Hash* Hash_new(void) {
	Hash* self = malloc(sizeof(Hash));
	self->keys = Array_new();
	self->values = Array_new();
	return self;
}
static void Hash_free(Hash* self) {
	StringArray_free(self->keys);
	StringArray_free(self->values);
	free(self);
}
static void Hash_set(Hash* self, char* key, char* value) {
	Array_push(self->keys, strdup(key));
	Array_push(self->values, strdup(value));
}
static char* Hash_get(Hash* self, char* key) {
	int i = StringArray_indexOf(self->keys, key);
	if (i==-1) return NULL;
	return self->values->items[i];
}

///////////////////////////////////////

enum EntryType {
	ENTRY_DIR,
	ENTRY_PAK,
	ENTRY_ROM,
};
typedef struct Entry {
	char* path;
	char* name;
	char* fullname;
	char* unique;
	int type;
	int alpha; // index in parent Directory's alphas Array, which points to the index of an Entry in its entries Array :sweat_smile:
} Entry;

static int hasEmu(char* emu_name); 

static Entry* Entry_new(char* path, int type) {
	char display_name[256];
	getDisplayName(path, display_name);
	Entry* self = malloc(sizeof(Entry));
	self->path = strdup(path);
	self->name = strdup(display_name);
	self->unique = NULL;
	self->type = type;
	self->alpha = 0;
	return self;
}
static void Entry_free(Entry* self) {
	free(self->path);
	free(self->name);
	if (self->unique) free(self->unique);
	free(self);
}

static int EntryArray_indexOf(Array* self, char* path) {
	for (int i=0; i<self->count; i++) {
		Entry* entry = self->items[i];
		if (exactMatch(entry->path, path)) return i;
	}
	return -1;
}
static int EntryArray_sortEntry(const void* a, const void* b) {
	Entry* item1 = *(Entry**)a;
	Entry* item2 = *(Entry**)b;
	return strcasecmp(item1->name, item2->name);
}
static void EntryArray_sort(Array* self) {
	qsort(self->items, self->count, sizeof(void*), EntryArray_sortEntry);
}

static void EntryArray_free(Array* self) {
	for (int i=0; i<self->count; i++) {
		Entry_free(self->items[i]);
	}
	Array_free(self);
}

///////////////////////////////////////

#define INT_ARRAY_MAX 27
typedef struct IntArray {
	int count;
	int items[INT_ARRAY_MAX];
} IntArray;
static IntArray* IntArray_new(void) {
	IntArray* self = malloc(sizeof(IntArray));
	self->count = 0;
	memset(self->items, 0, sizeof(int) * INT_ARRAY_MAX);
	return self;
}
static void IntArray_push(IntArray* self, int i) {
	self->items[self->count++] = i;
}
static void IntArray_free(IntArray* self) {
	free(self);
}

///////////////////////////////////////

typedef struct Directory {
	char* path;
	char* name;
	Array* entries;
	IntArray* alphas;
	// rendering
	int selected;
	int start;
	int end;
} Directory;

static int getIndexChar(char* str) {
	char i = 0;
	char c = tolower(str[0]);
	if (c>='a' && c<='z') i = (c-'a')+1;
	return i;
}

static void getUniqueName(Entry* entry, char* out_name) {
	char* filename = strrchr(entry->path, '/')+1;
	char emu_tag[256];
	getEmuName(entry->path, emu_tag);
	
	char *tmp;
	strcpy(out_name, entry->name);
	tmp = out_name + strlen(out_name);
	strcpy(tmp, " (");
	tmp = out_name + strlen(out_name);
	strcpy(tmp, emu_tag);
	tmp = out_name + strlen(out_name);
	strcpy(tmp, ")");
}

static void Directory_index(Directory* self) {
	int skip_index = exactMatch(FAUX_HIDDEN_PATH, self->path) || exactMatch(FAUX_RECENT_PATH, self->path) || exactMatch(FAUX_FAVORITE_PATH, self->path) || prefixMatch(COLLECTIONS_PATH, self->path); // not alphabetized
	
	Hash* map = NULL;
	char map_path[256];
	sprintf(map_path, "%s/map.txt", self->path);
	if (exists(map_path)) {
		FILE* file = fopen(map_path, "r");
		if (file) {
			map = Hash_new();
			char line[256];
			int resort = 0;
			while (fgets(line,256,file)!=NULL) {
				normalizeNewline(line);
				trimTrailingNewlines(line);
				if (strlen(line)==0) continue; // skip empty lines

				char* tmp = strchr(line,'\t');
				if (tmp) {
					tmp[0] = '\0';
					char* key = line;
					char* value = tmp+1;
					Hash_set(map, key, value);
				}
			}
			fclose(file);
			
			// need to 
			for (int i=0; i<self->entries->count; i++) {
				Entry* entry = self->entries->items[i];
				char* filename = strrchr(entry->path, '/')+1;
				char* alias = Hash_get(map, filename);
				if (alias) {
					free(entry->name);
					entry->name = strdup(alias);
					resort = 1;
				}
			}
			
			// oof, double s
			
			// TODO: maybe map.txt logic should be moved to EntryArray_sort()
			// or another precursor?
			
			if (resort) EntryArray_sort(self->entries);
		}
	}
	
	Entry* prior = NULL;
	int alpha = -1;
	int index = 0;
	for (int i=0; i<self->entries->count; i++) {
		Entry* entry = self->entries->items[i];
		if (map) {
			char* filename = strrchr(entry->path, '/')+1;
			char* alias = Hash_get(map, filename);
			if (alias) {
				free(entry->name);
				entry->name = strdup(alias);
			}
		}
		
		if (prior!=NULL && exactMatch(prior->name, entry->name)) {
			if (prior->unique) free(prior->unique);
			if (entry->unique) free(entry->unique);
			
			char* prior_filename = strrchr(prior->path, '/')+1;
			char* entry_filename = strrchr(entry->path, '/')+1;
			if (exactMatch(prior_filename, entry_filename)) {
				char prior_unique[256];
				char entry_unique[256];
				getUniqueName(prior, prior_unique);
				getUniqueName(entry, entry_unique);
				
				prior->unique = strdup(prior_unique);
				entry->unique = strdup(entry_unique);
			}
			else {
				prior->unique = strdup(prior_filename);
				entry->unique = strdup(entry_filename);
			}
		}

		if (!skip_index) {
			int a = getIndexChar(entry->name);
			if (a!=alpha) {
				index = self->alphas->count;
				IntArray_push(self->alphas, i);
				alpha = a;
			}
			entry->alpha = index;
		}
		
		prior = entry;
	}
	
	if (map) Hash_free(map);
}

static Array* getRoot(void);
static Array* getRecents(void);
static Array* getCollection(char* path);
static Array* getDiscs(char* path);
static Array* getEntries(char* path);
static Array* getFavorites(void);

static Array* getHiddens(void);

static Directory* Directory_new(char* path, int selected) {
	char display_name[256];
	getDisplayName(path, display_name);
	
	Directory* self = malloc(sizeof(Directory));
	self->path = strdup(path);
	self->name = strdup(display_name);
	if (exactMatch(path, SDCARD_PATH)) {
		self->entries = getRoot();
	}
	else if (exactMatch(path, FAUX_RECENT_PATH)) {
		self->entries = getRecents();
	}
	else if (exactMatch(path, FAUX_FAVORITE_PATH)) {
		self->entries = getFavorites();
	}
	else if (exactMatch(path, FAUX_HIDDEN_PATH)) {
		self->entries = getHiddens();
	}
	else if (!exactMatch(path, COLLECTIONS_PATH) && prefixMatch(COLLECTIONS_PATH, path) && suffixMatch(".txt", path)) {
		self->entries = getCollection(path);
	}
	else if (suffixMatch(".m3u", path)) {
		self->entries = getDiscs(path);
	}
	else {
		self->entries = getEntries(path);
	}
	self->alphas = IntArray_new();
	self->selected = selected;
	Directory_index(self);
	return self;
}
static void Directory_free(Directory* self) {
	free(self->path);
	free(self->name);
	EntryArray_free(self->entries);
	IntArray_free(self->alphas);
	free(self);
}

static void DirectoryArray_pop(Array* self) {
	Directory_free(Array_pop(self));
}
static void DirectoryArray_free(Array* self) {
	for (int i=0; i<self->count; i++) {
		Directory_free(self->items[i]);
	}
	Array_free(self);
}

typedef struct Favorite {
	char* path; // NOTE: this is without the SDCARD_PATH prefix!
	int available;
} Favorite;

static Favorite* Favorite_new(char* path) {
	Favorite* self = malloc(sizeof(Favorite));

	char sd_path[256]; // only need to get emu name
	sprintf(sd_path, "%s%s", SDCARD_PATH, path);

	char emu_name[256];
	getEmuName(sd_path, emu_name);

	self->path = strdup(path);
	self->available = hasEmu(emu_name);
	return self;
}
static void Favorite_free(Favorite* self) {
	free(self->path);
	free(self);
}

static int FavoriteArray_indexOf(Array* self, char* str) {
	for (int i=0; i<self->count; i++) {
		Favorite* item = self->items[i];
		if (exactMatch(item->path, str)) return i;
	}
	return -1;
}
static void FavoriteArray_free(Array* self) {
	for (int i=0; i<self->count; i++) {
		Favorite_free(self->items[i]);
	}
	Array_free(self);
}

static int FavoriteArray_splice(Array* self, int index) {
	if (index != -1) {
		for(int i=index; i<self->count-1; i++) {
			self->items[i] = self->items[i+1];
		}
		--self->count;
	}
	return  index;
}

///////////////////////////////////////

typedef struct Hidden {
	char* path; // NOTE: this is without the SDCARD_PATH prefix!
	int available;
} Hidden;

static Hidden* Hidden_new(char* path) {
	Hidden* self = malloc(sizeof(Hidden));

	char sd_path[256]; // only need to get emu name
	sprintf(sd_path, "%s%s", SDCARD_PATH, path);

	char emu_name[256];
	getEmuName(sd_path, emu_name);

	self->path = strdup(path);
	self->available = hasEmu(emu_name);
	return self;
}
static void Hidden_free(Hidden* self) {
	free(self->path);
	free(self);
}

static int HiddenArray_indexOf(Array* self, char* str) {
	for (int i=0; i<self->count; i++) {
		Hidden* item = self->items[i];
		if (exactMatch(item->path, str)) return i;
	}
	return -1;
}
static void HiddenArray_free(Array* self) {
	for (int i=0; i<self->count; i++) {
		Hidden_free(self->items[i]);
	}
	Array_free(self);
}

static int HiddenArray_splice(Array* self, int index) {
	if (index != -1) {
		for(int i=index; i<self->count-1; i++) {
			self->items[i] = self->items[i+1];
		}
		--self->count;
	}
	return  index;
}

///////////////////////////////////////



typedef struct Recent {
	char* path; // NOTE: this is without the SDCARD_PATH prefix!
	char* alias;
	int available;
} Recent;
 // yiiikes
static char* recent_alias = NULL;


static Recent* Recent_new(char* path, char* alias) {
	Recent* self = malloc(sizeof(Recent));

	char sd_path[256]; // only need to get emu name
	sprintf(sd_path, "%s%s", SDCARD_PATH, path);

	char emu_name[256];
	getEmuName(sd_path, emu_name);
	
	self->path = strdup(path);
	self->alias = alias ? strdup(alias) : NULL;
	self->available = hasEmu(emu_name);
	return self;
}
static void Recent_free(Recent* self) {
	free(self->path);
	if (self->alias) free(self->alias);
	free(self);
}

static int RecentArray_indexOf(Array* self, char* str) {
	for (int i=0; i<self->count; i++) {
		Recent* item = self->items[i];
		if (exactMatch(item->path, str)) return i;
	}
	return -1;
}
static void RecentArray_free(Array* self) {
	for (int i=0; i<self->count; i++) {
		Recent_free(self->items[i]);
	}
	Array_free(self);
}

///////////////////////////////////////

static Directory* top;
static Array* stack; // DirectoryArray
static Array* recents; // RecentArray
static Array* favorites; // FavoriteArray
static Array* hiddens; // HiddenArray

static int quit = 0;
static int can_resume = 0;
static int should_resume = 0; // set to 1 on BTN_RESUME but only if can_resume==1
static int last_selected_slot = 0;
static int simple_mode = 0;
static char slot_path[256];
static char slot_path_rom[256];

static int restore_depth = -1;
static int restore_relative = -1;
static int restore_selected = 0;
static int restore_start = 0;
static int restore_end = 0;


#define STANDARD_MODE "Standard"
#define SIMPLE_MODE "Simple (no Setting)"
#define FANCY_MODE "Fancy (Boxart + State)"

int fancy_mode = 0;
static int hide_state = 0;
static int hide_boxartifstate = 0;


///////////////////////////////////////

#define MAX_RECENTS 24 // a multiple of all menu rows
static void saveRecents(void) {
	FILE* file = fopen(RECENT_PATH, "w");
	if (file) {
		for (int i=0; i<recents->count; i++) {
			Recent* recent = recents->items[i];
			fputs(recent->path, file);
			if (recent->alias) {
				fputs("\t", file);
				fputs(recent->alias, file);
			}
			putc('\n', file);
		}
		fclose(file);
	}
}
static void addRecent(char* path, char* alias) {
	path += strlen(SDCARD_PATH); // makes paths platform agnostic
	int id = RecentArray_indexOf(recents, path);
	if (id==-1) { // add
		while (recents->count>=MAX_RECENTS) {
			Recent_free(Array_pop(recents));
		}
		Array_unshift(recents, Recent_new(path, alias));
	}
	else if (id>0) { // bump to top
		for (int i=id; i>0; i--) {
			void* tmp = recents->items[i-1];
			recents->items[i-1] = recents->items[i];
			recents->items[i] = tmp;
		}
	}
	saveRecents();
}

static void saveFavorites(void) {
	FILE* file = fopen(FAVORITE_PATH, "w");
	if (file) {
		for (int i=0; i<favorites->count; i++) {
			Favorite* favorite = favorites->items[i];
			fputs(favorite->path, file);
			putc('\n', file);
		}
		fclose(file);
	}
}
static void toggleFavorite(char* path) {
	//printf("TEST FAV: %s\n",path);
	path += strlen(SDCARD_PATH); // makes paths platform agnostic
	//printf("TEST FAV: %s\n",path);
	int id = FavoriteArray_indexOf(favorites, path);
	//printf("TEST FAV: %s / ID = %d\n",path,id);
	if (id==-1) { // add
		Array_unshift(favorites, Favorite_new(path));
	}
	else { // remove
		FavoriteArray_splice(favorites, id);
	}
	saveFavorites();
}

static int isFavorite(char *path) {
	path += strlen(SDCARD_PATH); // makes paths platform agnostic
	int id = FavoriteArray_indexOf(favorites, path);
	return ++id;
}

static int hasEmu(char* emu_name) {
	char pak_path[256];
	sprintf(pak_path, "%s/Emus/%s.pak/launch.sh", PAKS_PATH, emu_name);
	if (exists(pak_path)) return 1;

	sprintf(pak_path, "%s/Emus/%s/%s.pak/launch.sh", SDCARD_PATH, PLATFORM, emu_name);
	return exists(pak_path);
}
static int hasCue(char* dir_path, char* cue_path) { // NOTE: dir_path not rom_path
	char* tmp = strrchr(dir_path, '/') + 1; // folder name
	sprintf(cue_path, "%s/%s.cue", dir_path, tmp);
	return exists(cue_path);
}
static int hasM3u(char* rom_path, char* m3u_path) { // NOTE: rom_path not dir_path
	char* tmp;
	
	strcpy(m3u_path, rom_path);
	tmp = strrchr(m3u_path, '/') + 1;
	tmp[0] = '\0';
	
	// path to parent directory
	char base_path[256];
	strcpy(base_path, m3u_path);
	
	tmp = strrchr(m3u_path, '/');
	tmp[0] = '\0';
	
	// get parent directory name
	char dir_name[256];
	tmp = strrchr(m3u_path, '/');
	strcpy(dir_name, tmp);
	
	// dir_name is also our m3u file name
	tmp = m3u_path + strlen(m3u_path); 
	strcpy(tmp, dir_name);

	// add extension
	tmp = m3u_path + strlen(m3u_path);
	strcpy(tmp, ".m3u");
	
	return exists(m3u_path);
}

static int hasRecents(void) {
	LOG_info("hasRecents %s\n", RECENT_PATH);
	int has = 0;
	
	Array* parent_paths = Array_new();
	if (exists(CHANGE_DISC_PATH)) {
		char sd_path[256];
		getFile(CHANGE_DISC_PATH, sd_path, 256);
		if (exists(sd_path)) {
			char* disc_path = sd_path + strlen(SDCARD_PATH); // makes path platform agnostic
			Recent* recent = Recent_new(disc_path, NULL);
			if (recent->available) has += 1;
			Array_push(recents, recent);
		
			char parent_path[256];
			strcpy(parent_path, disc_path);
			char* tmp = strrchr(parent_path, '/') + 1;
			tmp[0] = '\0';
			Array_push(parent_paths, strdup(parent_path));
		}
		unlink(CHANGE_DISC_PATH);
	}
	
	FILE* file = fopen(RECENT_PATH, "r"); // newest at top
	if (file) {
		char line[256];
		while (fgets(line,256,file)!=NULL) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line)==0) continue; // skip empty lines
			
			LOG_info("line: %s\n", line);
			
			char* path = line;
			char* alias = NULL;
			char* tmp = strchr(line,'\t');
			if (tmp) {
				tmp[0] = '\0';
				alias = tmp+1;
			}
			
			char sd_path[256];
			sprintf(sd_path, "%s%s", SDCARD_PATH, path);
			if (exists(sd_path)) {
				if (recents->count<MAX_RECENTS) {
					// this logic replaces an existing disc from a multi-disc game with the last used
					char m3u_path[256];
					if (hasM3u(sd_path, m3u_path)) { // TODO: this might tank launch speed
						char parent_path[256];
						strcpy(parent_path, path);
						char* tmp = strrchr(parent_path, '/') + 1;
						tmp[0] = '\0';
						
						int found = 0;
						for (int i=0; i<parent_paths->count; i++) {
							char* path = parent_paths->items[i];
							if (prefixMatch(path, parent_path)) {
								found = 1;
								break;
							}
						}
						if (found) continue;
						
						Array_push(parent_paths, strdup(parent_path));
					}
					
					LOG_info("path:%s alias:%s\n", path, alias);
					
					Recent* recent = Recent_new(path, alias);
					if (recent->available) has += 1;
					Array_push(recents, recent);
				}
			}
		}
		fclose(file);
	}
	
	saveRecents();
	
	StringArray_free(parent_paths);
	return has>0;
	//return 1;
}


static void saveHiddens(void) {
	FILE* file = fopen(HIDDEN_PATH, "w");
	if (file) {
		for (int i=0; i<hiddens->count; i++) {
			Hidden* hidden = hiddens->items[i];
			fputs(hidden->path, file);
			putc('\n', file);
		}
		fclose(file);
	}
}

static int hasHiddens(void) {
	int has = 0;

	FILE* file = fopen(HIDDEN_PATH, "r"); // newest at top
	if (file) {
		char line[256];
		while (fgets(line,256,file)!=NULL) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line)==0) continue; // skip empty lines

			char sd_path[256];
			sprintf(sd_path, "%s%s", SDCARD_PATH, line);
			if (exists(sd_path)) {
					Hidden* hidden = Hidden_new(line);
					if (hidden->available) has += 1;
					Array_push(hiddens, hidden);
			}
		}
		fclose(file);
	}

	saveHiddens();

	//return has>0;
	return 1; // Always show directory, even if empty.
}


static void toggleHidden(char* path) {
	//printf("TEST FAV: %s\n",path);
	path += strlen(SDCARD_PATH); // makes paths platform agnostic
	//printf("TEST FAV: %s\n",path);
	int id = HiddenArray_indexOf(hiddens, path);
	//printf("TEST FAV: %s / ID = %d\n",path,id);
	if (id==-1) { // add
		Array_unshift(hiddens, Hidden_new(path));
	}
	else { // remove
		HiddenArray_splice(hiddens, id);
	}
	saveHiddens();
}

static int isHidden(char * parentpath, char *path) {
	char fullpath[256];
	parentpath += strlen(SDCARD_PATH); // makes paths platform agnostic
	sprintf(fullpath,"%s/%s", parentpath, path);	
	int id = HiddenArray_indexOf(hiddens, fullpath);
	//printf("check if %s is hidden = %d\n", fullpath, id);
	return ++id;
}

static int hasFavorites(void) {
	int has = 0;

	FILE* file = fopen(FAVORITE_PATH, "r"); // newest at top
	if (file) {
		char line[256];
		while (fgets(line,256,file)!=NULL) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line)==0) continue; // skip empty lines

			char sd_path[256];
			sprintf(sd_path, "%s%s", SDCARD_PATH, line);
			if (exists(sd_path)) {
					Favorite* favorite = Favorite_new(line);
					if (favorite->available) has += 1;
					Array_push(favorites, favorite);
			}
		}
		fclose(file);
	}

	saveFavorites();

	//return has>0;
	return 1; // Always show directory, even if empty.
}

static int hasCollections(void) {
	int has = 0;
	if (!exists(COLLECTIONS_PATH)) return has;
	
	DIR *dh = opendir(COLLECTIONS_PATH);
	struct dirent *dp;
	while((dp = readdir(dh)) != NULL) {
		if (hide(dp->d_name)) continue;
		has = 1;
		break;
	}
	closedir(dh);
	return has;
}

static int hasRoms(char* dir_name) {
	int has = 0;
	char emu_name[256];
	char rom_path[256];

	getEmuName(dir_name, emu_name);
	
	// check for emu pak
	if (!hasEmu(emu_name)) return has;
	
	// check for at least one non-hidden file (we're going to assume it's a rom)
	sprintf(rom_path, "%s/%s/", ROMS_PATH, dir_name);
	DIR *dh = opendir(rom_path);
	if (dh!=NULL) {
		struct dirent *dp;
		while((dp = readdir(dh)) != NULL) {
			if (hide(dp->d_name)) continue;
			has = 1;
			break;
		}
		closedir(dh);
	}
	// if (!has) printf("No roms for %s!\n", dir_name);
	return has;
}
static Array* getRoot(void) {
	Array* root = Array_new();
	
	if (hasRecents()) Array_push(root, Entry_new(FAUX_RECENT_PATH, ENTRY_DIR));
	if (hasFavorites()) Array_push(root, Entry_new(FAUX_FAVORITE_PATH, ENTRY_DIR));
	
	
	Array* entries = Array_new();
	DIR* dh = opendir(ROMS_PATH);
	if (dh!=NULL) {
		struct dirent *dp;
		char* tmp;
		char full_path[256];
		sprintf(full_path, "%s/", ROMS_PATH);
		tmp = full_path + strlen(full_path);
		Array* emus = Array_new();
		while((dp = readdir(dh)) != NULL) {
			if (hide(dp->d_name)) continue;
			//if (isHidden(dp->d_name)) continue;
			if (hasRoms(dp->d_name)) {
				strcpy(tmp, dp->d_name);
				Array_push(emus, Entry_new(full_path, ENTRY_DIR));
			}
		}
		EntryArray_sort(emus);
		Entry* prev_entry = NULL;
		for (int i=0; i<emus->count; i++) {
			Entry* entry = emus->items[i];
			if (prev_entry!=NULL) {
				if (exactMatch(prev_entry->name, entry->name)) {
					Entry_free(entry);
					continue;
				}
			}
			Array_push(entries, entry);
			prev_entry = entry;
		}
		Array_free(emus); // just free the array part, entries now owns emus entries
		closedir(dh);
	}
	
	// copied/modded from Directory_index
	char map_path[256];
	sprintf(map_path, "%s/map.txt", ROMS_PATH);
	if (entries->count>0 && exists(map_path)) {
		FILE* file = fopen(map_path, "r");
		if (file) {
			Hash* map = Hash_new();
			char line[256];
			int resort = 0;
			while (fgets(line,256,file)!=NULL) {
				normalizeNewline(line);
				trimTrailingNewlines(line);
				if (strlen(line)==0) continue; // skip empty lines

				char* tmp = strchr(line,'\t');
				if (tmp) {
					tmp[0] = '\0';
					char* key = line;
					char* value = tmp+1;
					Hash_set(map, key, value);
				}
			}
			fclose(file);
			
			for (int i=0; i<entries->count; i++) {
				Entry* entry = entries->items[i];
				char* filename = strrchr(entry->path, '/')+1;
				char* alias = Hash_get(map, filename);
				if (alias) {
					free(entry->name);
					entry->name = strdup(alias);
					resort = 1;
				}
			} 
			if (resort) EntryArray_sort(entries);
			Hash_free(map);
		}
	}
	
	if (hasCollections()) {
		if (entries->count) Array_push(root, Entry_new(COLLECTIONS_PATH, ENTRY_DIR));
		else { // no visible systems, promote collections to root
			dh = opendir(COLLECTIONS_PATH);
			if (dh!=NULL) {
				struct dirent *dp;
				char* tmp;
				char full_path[256];
				sprintf(full_path, "%s/", COLLECTIONS_PATH);
				tmp = full_path + strlen(full_path);
				Array* collections = Array_new();
				while((dp = readdir(dh)) != NULL) {
					if (hide(dp->d_name)) continue;
					strcpy(tmp, dp->d_name);
					Array_push(collections, Entry_new(full_path, ENTRY_DIR)); // yes, collections are fake directories
				}
				EntryArray_sort(collections);
				for (int i=0; i<collections->count; i++) {
					Array_push(entries, collections->items[i]);
				}
				Array_free(collections); // just free the array part, entries now owns collections entries
				closedir(dh);
			}
		}
	}
	
	// add systems to root
	for (int i=0; i<entries->count; i++) {
		Array_push(root, entries->items[i]);
	}
	Array_free(entries); // root now owns entries' entries
	
	char* tools_path = SDCARD_PATH "/Tools/" PLATFORM;
	if (exists(tools_path) && !simple_mode) Array_push(root, Entry_new(tools_path, ENTRY_DIR));
	if (hasHiddens() && exists(SHOW_HIDDEN_FOLDER_PATH)) Array_push(root, Entry_new(FAUX_HIDDEN_PATH, ENTRY_DIR));	
	return root;
}
static Array* getRecents(void) {
	Array* entries = Array_new();
	for (int i=0; i<recents->count; i++) {
		Recent* recent = recents->items[i];
		if (!recent->available) continue;
		
		char sd_path[256];
		sprintf(sd_path, "%s%s", SDCARD_PATH, recent->path);
		int type = suffixMatch(".pak", sd_path) ? ENTRY_PAK : ENTRY_ROM; // ???
		Entry* entry = Entry_new(sd_path, type);
		if (recent->alias) {
			free(entry->name);
			entry->name = strdup(recent->alias);
		}
		Array_push(entries, entry);
	}
	return entries;
}

static Array* getHiddens(void) {
	Array* entries = Array_new();
	for (int i=0; i<hiddens->count; i++) {
		Hidden* hidden = hiddens->items[i];
		if (!hidden->available) continue;

		char sd_path[256];
		sprintf(sd_path, "%s%s", SDCARD_PATH, hidden->path);
		int type = suffixMatch(".pak", sd_path) ? ENTRY_PAK : ENTRY_ROM; // ???
		Array_push(entries, Entry_new(sd_path, type));
	}
	return entries;
}



static Array* getFavorites(void) {
	Array* entries = Array_new();
	for (int i=0; i<favorites->count; i++) {
		Favorite* favorite = favorites->items[i];
		if (!favorite->available) continue;

		char sd_path[256];
		sprintf(sd_path, "%s%s", SDCARD_PATH, favorite->path);
		int type = suffixMatch(".pak", sd_path) ? ENTRY_PAK : ENTRY_ROM; // ???
		Array_push(entries, Entry_new(sd_path, type));
	}
	return entries;
}


static Array* getCollection(char* path) {
	Array* entries = Array_new();
	FILE* file = fopen(path, "r");
	if (file) {
		char line[256];
		while (fgets(line,256,file)!=NULL) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line)==0) continue; // skip empty lines
			
			char sd_path[256];
			sprintf(sd_path, "%s%s", SDCARD_PATH, line);
			if (exists(sd_path)) {
				int type = suffixMatch(".pak", sd_path) ? ENTRY_PAK : ENTRY_ROM; // ???
				Array_push(entries, Entry_new(sd_path, type));
				
				// char emu_name[256];
				// getEmuName(sd_path, emu_name);
				// if (hasEmu(emu_name)) {
					// Array_push(entries, Entry_new(sd_path, ENTRY_ROM));
				// }
			}
		}
		fclose(file);
	}
	return entries;
}
static Array* getDiscs(char* path){
	
	// TODO: does path have SDCARD_PATH prefix?
	
	Array* entries = Array_new();
	
	char base_path[256];
	strcpy(base_path, path);
	char* tmp = strrchr(base_path, '/') + 1;
	tmp[0] = '\0';
	
	// TODO: limit number of discs supported (to 9?)
	FILE* file = fopen(path, "r");
	if (file) {
		char line[256];
		int disc = 0;
		while (fgets(line,256,file)!=NULL) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line)==0) continue; // skip empty lines
			
			char disc_path[256];
			sprintf(disc_path, "%s%s", base_path, line);
						
			if (exists(disc_path)) {
				disc += 1;
				Entry* entry = Entry_new(disc_path, ENTRY_ROM);
				free(entry->name);
				char name[16];
				sprintf(name, "Disc %i", disc);
				entry->name = strdup(name);
				Array_push(entries, entry);
			}
		}
		fclose(file);
	}
	return entries;
}
static int getFirstDisc(char* m3u_path, char* disc_path) { // based on getDiscs() natch
	int found = 0;

	char base_path[256];
	strcpy(base_path, m3u_path);
	char* tmp = strrchr(base_path, '/') + 1;
	tmp[0] = '\0';
	
	FILE* file = fopen(m3u_path, "r");
	if (file) {
		char line[256];
		while (fgets(line,256,file)!=NULL) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line)==0) continue; // skip empty lines
			
			sprintf(disc_path, "%s%s", base_path, line);
						
			if (exists(disc_path)) found = 1;
			break;
		}
		fclose(file);
	}
	return found;
}

static void addEntries(Array* entries, char* path) {
	DIR *dh = opendir(path);
	if (dh!=NULL) {
		struct dirent *dp;
		char* tmp;
		char full_path[256];
		sprintf(full_path, "%s/", path);
		tmp = full_path + strlen(full_path);
		while((dp = readdir(dh)) != NULL) {
			if (hide(dp->d_name)) continue;
			if (isHidden(path, dp->d_name)) continue;
			strcpy(tmp, dp->d_name);
			int is_dir = dp->d_type==DT_DIR;
			int type;
			if (is_dir) {
				// TODO: this should make sure launch.sh exists
				if (suffixMatch(".pak", dp->d_name)) {
					type = ENTRY_PAK;
				}
				else {
					type = ENTRY_DIR;
				}
			}
			else {
				if (prefixMatch(COLLECTIONS_PATH, full_path)) {
					type = ENTRY_DIR; // :shrug:
				}
				else {
					type = ENTRY_ROM;
				}
			}
			Array_push(entries, Entry_new(full_path, type));
		}
		closedir(dh);
	}
}

static int isConsoleDir(char* path) {
	char* tmp;
	char parent_dir[256];
	strcpy(parent_dir, path);
	tmp = strrchr(parent_dir, '/');
	tmp[0] = '\0';
	
	return exactMatch(parent_dir, ROMS_PATH);
}

static Array* getEntries(char* path){
	Array* entries = Array_new();

	if (isConsoleDir(path)) { // top-level console folder, might collate
		char collated_path[256];
		strcpy(collated_path, path);
		char* tmp = strrchr(collated_path, '(');
		// 1 because we want to keep the opening parenthesis to avoid collating "Game Boy Color" and "Game Boy Advance" into "Game Boy"
		// but conditional so we can continue to support a bare tag name as a folder name
		if (tmp) tmp[1] = '\0'; 
		
		DIR *dh = opendir(ROMS_PATH);
		if (dh!=NULL) {
			struct dirent *dp;
			char full_path[256];
			sprintf(full_path, "%s/", ROMS_PATH);
			tmp = full_path + strlen(full_path);
			// while loop so we can collate paths, see above
			while((dp = readdir(dh)) != NULL) {
				if (hide(dp->d_name)) continue;
				//if (isHidden(dp->d_name)) continue;
				if (dp->d_type!=DT_DIR) continue;
				strcpy(tmp, dp->d_name);
			
				if (!prefixMatch(collated_path, full_path)) continue;
				addEntries(entries, full_path);
			}
			closedir(dh);
		}
	}
	else addEntries(entries, path); // just a subfolder
	
	EntryArray_sort(entries);
	return entries;
}

///////////////////////////////////////

static void queueNext(char* cmd) {
	LOG_info("cmd: %s\n", cmd);
	putFile("/tmp/next", cmd);
	quit = 1;
}

// based on https://stackoverflow.com/a/31775567/145965
static int replaceString(char *line, const char *search, const char *replace) {
   char *sp; // start of pattern
   if ((sp = strstr(line, search)) == NULL) {
      return 0;
   }
   int count = 1;
   int sLen = strlen(search);
   int rLen = strlen(replace);
   if (sLen > rLen) {
      // move from right to left
      char *src = sp + sLen;
      char *dst = sp + rLen;
      while((*dst = *src) != '\0') { dst++; src++; }
   } else if (sLen < rLen) {
      // move from left to right
      int tLen = strlen(sp) - sLen;
      char *stop = sp + rLen;
      char *src = sp + sLen + tLen;
      char *dst = sp + rLen + tLen;
      while(dst >= stop) { *dst = *src; dst--; src--; }
   }
   memcpy(sp, replace, rLen);
   count += replaceString(sp + rLen, search, replace);
   return count;
}
static char* escapeSingleQuotes(char* str) {
	// why not call replaceString directly?
	// call points require the modified string be returned
	// but replaceString is recursive and depends on its
	// own return value (but does it need to?)
	replaceString(str, "'", "'\\''");
	return str;
}

///////////////////////////////////////

static void readyResumePath(char* rom_path, int type) {
	char* tmp;
	can_resume = 0;
	char path[256];
	strcpy(path, rom_path);
	
	if (!prefixMatch(ROMS_PATH, path)) return;
	
	char auto_path[256];
	if (type==ENTRY_DIR) {
		if (!hasCue(path, auto_path)) { // no cue?
			tmp = strrchr(auto_path, '.') + 1; // extension
			strcpy(tmp, "m3u"); // replace with m3u
			if (!exists(auto_path)) return; // no m3u
		}
		strcpy(path, auto_path); // cue or m3u if one exists
	}
	
	if (!suffixMatch(".m3u", path)) {
		char m3u_path[256];
		if (hasM3u(path, m3u_path)) {
			// change path to m3u path
			strcpy(path, m3u_path);
		}
	}
	
	char emu_name[256];
	getEmuName(path, emu_name);
	
	char rom_file[256];
	//tmp = strrchr(path, '/') + 1;
	//strcpy(rom_file, tmp);
	getDisplayNameParens(path,rom_file);
	sprintf(slot_path, "%s/.minui/%s/%s.txt", SHARED_USERDATA_PATH, emu_name, rom_file); // /.userdata/shared/.minui/<EMU>/<romname>
	//sprintf(slot_path_rom, "%s/%s/%s", MYSAVESTATE_PATH, emu_name, rom_file); // /.userdata/shared/.minui/<EMU>/<romname>.ext
	//sprintf(slot_path, "%s.txt", slot_path_rom); // /.userdata/.minui/<EMU>/<romname>.ext.txt
	
	//can_resume = exists(slot_path);
	last_selected_slot = canResume(path);
	if (last_selected_slot) can_resume = 1;

}
static void readyResume(Entry* entry) {
	readyResumePath(entry->path, entry->type);
}

static void saveLast(char* path);
static void loadLast(void);

static int autoResume(void) {
	// NOTE: bypasses recents
	if (!exists(AUTO_RESUME_PATH)) return 0;
	
	char path[256];
	getFile(AUTO_RESUME_PATH, path, 256);
	unlink(AUTO_RESUME_PATH);
	sync();
	if (exists(AUTO_RESUME_MODIFIER_PATH)){
		selected_modifier = 1;
		unlink(AUTO_RESUME_MODIFIER_PATH);
	}
	// make sure rom still exists
	char sd_path[256];
	sprintf(sd_path, "%s%s", SDCARD_PATH, path);
	if (!exists(sd_path)) return 0;
	
	// make sure emu still exists
	char emu_name[256];
	getEmuName(sd_path, emu_name);
	
	char emu_path[256];
	getEmuPath(emu_name, emu_path);
	
	if (!exists(emu_path)) return 0;
	
	// putFile(LAST_PATH, FAUX_RECENT_PATH); // saveLast() will crash here because top is NULL
	char _romname[256];
	getDisplayNameParens(path,_romname);
	char cmd[256];
	sprintf(cmd, "'%s' '%s' %d '%s/%s/States/%s.state' %d ", escapeSingleQuotes(emu_path), escapeSingleQuotes(sd_path), AUTO_RESUME_SLOT, MYSAVESTATE_PATH, escapeSingleQuotes(emu_name),escapeSingleQuotes(_romname), selected_modifier );
	putInt(RESUME_SLOT_PATH, AUTO_RESUME_SLOT);
	selected_modifier = 0;
	queueNext(cmd);
	return 1;
}

static void openPak(char* path) {
	// NOTE: escapeSingleQuotes() modifies the passed string 
	// so we need to save the path before we call that
	// if (prefixMatch(ROMS_PATH, path)) {
	// 	addRecent(path);
	// }
	saveLast(path);
	
	char cmd[256];
	sprintf(cmd, "'%s/launch.sh'", escapeSingleQuotes(path));
	queueNext(cmd);
}
static void openRom(char* path, char* last) {
	LOG_info("openRom(%s,%s)\nSELECT = %d\n", path, last, selected_modifier);
	char sd_path[256];
	strcpy(sd_path, path);
	int loadslot=-1;
	
	char m3u_path[256];
	int has_m3u = hasM3u(sd_path, m3u_path);
	
	char recent_path[256];
	strcpy(recent_path, has_m3u ? m3u_path : sd_path);
	
//	if (has_m3u && suffixMatch(".m3u", sd_path)) {
//		getFirstDisc(m3u_path, sd_path);
//	}

	char emu_name[256];
	getEmuName(sd_path, emu_name);

	if (should_resume) {
		char slot[16];
		getFile(slot_path, slot, 16);
		putFile(RESUME_SLOT_PATH, slot);
		should_resume = 0;
		loadslot=last_selected_slot;

/*		if (has_m3u) {
			char rom_file[256];
			strcpy(rom_file, strrchr(m3u_path, '/') + 1);
			
			// get disc for state
			char disc_path_path[256];
			sprintf(disc_path_path, "%s/.minui/%s/%s.%s.txt", SHARED_USERDATA_PATH, emu_name, rom_file, slot); // /.userdata/arm-480/.minui/<EMU>/<romname>.ext.0.txt

			if (exists(disc_path_path)) {
				// switch to disc path
				char disc_path[256];
				getFile(disc_path_path, disc_path, 256);
				if (disc_path[0]=='/') strcpy(sd_path, disc_path); // absolute
				else { // relative
					strcpy(sd_path, m3u_path);
					char* tmp = strrchr(sd_path, '/') + 1;
					strcpy(tmp, disc_path);
				}
			}
		}*/
	} 
	//else
	 //putInt(RESUME_SLOT_PATH,AUTO_RESUME_SLOT); // resume hidden default state
	// loadslot=-1;	
	char emu_path[256];
	getEmuPath(emu_name, emu_path);
	
	// NOTE: escapeSingleQuotes() modifies the passed string 
	// so we need to save the path before we call that
	addRecent(recent_path, recent_alias); // yiiikes
	saveLast(last==NULL ? sd_path : last);
	
	char statepath[256];
	char _romname[256];
	getDisplayNameParens(sd_path,_romname);
	getStatePath(sd_path, statepath);
	char cmd[512];
	sprintf(cmd, "'%s' '%s' %d '%s/%s.state%d' %d ", escapeSingleQuotes(emu_path), escapeSingleQuotes(sd_path), loadslot, escapeSingleQuotes(statepath), escapeSingleQuotes(_romname),loadslot, selected_modifier);
	selected_modifier = 0;
	queueNext(cmd);
}

static void openDirectory(char* path, int auto_launch) {
	char auto_path[256];
	if (hasCue(path, auto_path) && auto_launch) {
		openRom(auto_path, path);
		return;
	}

	char m3u_path[256];
	strcpy(m3u_path, auto_path);
	char* tmp = strrchr(m3u_path, '.') + 1; // extension
	strcpy(tmp, "m3u"); // replace with m3u
	if (exists(m3u_path) && auto_launch) {
		auto_path[0] = '\0';
		if (getFirstDisc(m3u_path, auto_path)) {
			openRom(auto_path, path);
			return;
		}
		// TODO: doesn't handle empty m3u files
	}
	
	int selected = 0;
	int start = selected;
	int end = 0;
	if (top && top->entries->count>0) {
		if (restore_depth==stack->count && top->selected==restore_relative) {
			selected = restore_selected;
			start = restore_start;
			end = restore_end;
		}
	}
	
	top = Directory_new(path, selected);
	top->start = start;
	top->end = end ? end : ((top->entries->count < ( MAIN_ROW_COUNT + fancy_mode )) ? top->entries->count : ( MAIN_ROW_COUNT + fancy_mode ));

	Array_push(stack, top);
}
static void closeDirectory(void) {
	restore_selected = top->selected;
	restore_start = top->start;
	restore_end = top->end;
	DirectoryArray_pop(stack);
	restore_depth = stack->count;
	top = stack->items[stack->count-1];
	restore_relative = top->selected;
}

static void Entry_open(Entry* self) {
	recent_alias = self->name;  // yiiikes
	if (self->type==ENTRY_ROM) {
		char *last = NULL;
		if (prefixMatch(COLLECTIONS_PATH, top->path)) {
			char* tmp;
			char filename[256];
			
			tmp = strrchr(self->path, '/');
			if (tmp) strcpy(filename, tmp+1);
			
			char last_path[256];
			sprintf(last_path, "%s/%s", top->path, filename);
			last = last_path;
		}
		openRom(self->path, last);
	}
	else if (self->type==ENTRY_PAK) {
		openPak(self->path);
	}
	else if (self->type==ENTRY_DIR) {
		openDirectory(self->path, 1);
	}
}

///////////////////////////////////////

static void saveLast(char* path) {
	// special case for recently played
	if (exactMatch(top->path, FAUX_RECENT_PATH)) {
		// NOTE: that we don't have to save the file because
		// your most recently played game will always be at
		// the top which is also the default selection
		path = FAUX_RECENT_PATH;
	}
	putFile(LAST_PATH, path);
}
static void loadLast(void) { // call after loading root directory
	if (!exists(LAST_PATH)) return;

	char last_path[256];
	getFile(LAST_PATH, last_path, 256);
	
	char full_path[256];
	strcpy(full_path, last_path);
	
	char* tmp;
	char filename[256];
	tmp = strrchr(last_path, '/');
	if (tmp) strcpy(filename, tmp);
	
	Array* last = Array_new();
	while (!exactMatch(last_path, SDCARD_PATH)) {
		Array_push(last, strdup(last_path));
		
		char* slash = strrchr(last_path, '/');
		last_path[(slash-last_path)] = '\0';
	}
	
	while (last->count>0) {
		char* path = Array_pop(last);
		if (!exactMatch(path, ROMS_PATH)) { // romsDir is effectively root as far as restoring state after a game
			char collated_path[256];
			collated_path[0] = '\0';
			if (suffixMatch(")", path) && isConsoleDir(path)) {
				strcpy(collated_path, path);
				tmp = strrchr(collated_path, '(');
				if (tmp) tmp[1] = '\0'; // 1 because we want to keep the opening parenthesis to avoid collating "Game Boy Color" and "Game Boy Advance" into "Game Boy"
			}
			
			for (int i=0; i<top->entries->count; i++) {
				Entry* entry = top->entries->items[i];
			
				// NOTE: strlen() is required for collated_path, '\0' wasn't reading as NULL for some reason
				if (exactMatch(entry->path, path) || (strlen(collated_path) && prefixMatch(collated_path, entry->path)) || (prefixMatch(COLLECTIONS_PATH, full_path) && suffixMatch(filename, entry->path))) {
					top->selected = i;
					if (i>=top->end) {
						top->start = i;
						top->end = top->start + ( MAIN_ROW_COUNT + fancy_mode );
						if (top->end>top->entries->count) {
							top->end = top->entries->count;
							top->start = top->end - ( MAIN_ROW_COUNT + fancy_mode );
						}
					}
					if (last->count==0 && !exactMatch(entry->path, FAUX_RECENT_PATH) && !exactMatch(entry->path, FAUX_HIDDEN_PATH) && !(!exactMatch(entry->path, COLLECTIONS_PATH) && prefixMatch(COLLECTIONS_PATH, entry->path))) break; // don't show contents of auto-launch dirs
				
					if (entry->type==ENTRY_DIR) {
						openDirectory(entry->path, 0);
						break;
					}
				}
			}
		}
		free(path); // we took ownership when we popped it
	}
	
	StringArray_free(last);
}

/* helper functions to draw boxart and savestate preview*/
int drawStatePreview(SDL_Surface* _screen, char* bmpPath, int stateIndex){
    #define WINDOW_RADIUS 4 // TODO: this logic belongs in blitRect?
	#define PAGINATION_HEIGHT 6
	// unscaled
	int hw = _screen->w * 3 / 5;
	int hh = _screen->h * 3 / 5;
	int pw = hw + SCALE1(WINDOW_RADIUS*2);
	int ph = hh + SCALE1(WINDOW_RADIUS*2 + PAGINATION_HEIGHT);
	int ox = _screen->w  - pw;
	int oy = (_screen->h - ph) / 2;
	GFX_blitRect(ASSET_STATE_BG, _screen, &(SDL_Rect){ox,oy,pw,ph});
	ox += SCALE1(WINDOW_RADIUS);
	oy += SCALE1(WINDOW_RADIUS);
	SDL_Surface* preview = NULL;
	SDL_Surface* unscaled_preview = IMG_Load(bmpPath);
	if (!unscaled_preview) {
        //printf("IMG_Load: %s\n", IMG_GetError());
        SDL_Rect preview = {ox,oy,hw,hh};
		SDL_FillRect(_screen, &preview, 0);
        GFX_blitMessage(font.large, "Empty Slot", _screen, &preview);
    } else {
		preview = zoomSurface(unscaled_preview, (1.0 * hw / unscaled_preview->w) , (1.0 * hh / unscaled_preview->h), 0);
	//	printf("SaveState BMP %s has size is %dx%d\n", bmpPath, unscaled_preview->w, unscaled_preview->h);
	}
	
    SDL_BlitSurface(preview, NULL, _screen, &(SDL_Rect){ox,oy});    
	SDL_FreeSurface(preview);
	SDL_FreeSurface(unscaled_preview);

   // pagination
   #define MENU_SLOT_COUNT 8
	ox += (pw-SCALE1(16*MENU_SLOT_COUNT))/2;
	oy += hh+SCALE1(WINDOW_RADIUS)-SCALE1(PAGINATION_HEIGHT / 2 - 1);
	for (int i=1; i<MENU_SLOT_COUNT+1; i++) {
		if (i==stateIndex) GFX_blitAsset(ASSET_PAGE, NULL, _screen, &(SDL_Rect){ox+SCALE1((i-1)*16),oy});
		else GFX_blitAsset(ASSET_DOT, NULL, _screen, &(SDL_Rect){ox+SCALE1((i-1)*16)+4,oy+SCALE1(2)});
	}
	if (stateIndex == 0){
		ox -= SCALE1(16);		
		GFX_blitAsset(ASSET_RED_PAGE, NULL, _screen, &(SDL_Rect){ox,oy});
	}
    return 1;
}

int drawBoxart(SDL_Surface* _screen, char* bmpPath){	
 #define WINDOW_RADIUS 4 
    int ox = 0;
	int	oy = 0;
	int hw = _screen->w ;
	int hh = _screen->h ;
// window
	GFX_blitRect(ASSET_STATE_BG, _screen, &(SDL_Rect){ox,oy,hw,hh});
	SDL_Surface* unscaled_boxart = IMG_Load(bmpPath);
	SDL_Surface* boxart = NULL;
	//printf("origimg %dx%d scaled to %dx%d\n", unscaled_boxart->w,unscaled_boxart->h, hw, hh);
	if (!unscaled_boxart) {
        printf("IMG_Load: %s\n", IMG_GetError());
        SDL_Rect boxart = {SCALE2(ox,oy),hw,hh};
		SDL_FillRect(_screen, &boxart, 0);
    } else {
		//resize image to fit current screen size 
		boxart = zoomSurface(unscaled_boxart, (1.0 * hw / unscaled_boxart->w) , (1.0 * hh / unscaled_boxart->h), 0);
	}
	SDL_BlitSurface(boxart, NULL, _screen, &(SDL_Rect){ox,oy});
	SDL_FreeSurface(unscaled_boxart);
	SDL_FreeSurface(boxart);
	return 1;
}


/* end helper functions*/



///////////////////////////////////////

static void Menu_init(void) {
	stack = Array_new(); // array of open Directories
	recents = Array_new();
	favorites = Array_new();
	hiddens = Array_new();

	openDirectory(SDCARD_PATH, 0);
	loadLast(); // restore state when available
}
static void Menu_quit(void) {
	RecentArray_free(recents);
	FavoriteArray_free(favorites);
	HiddenArray_free(hiddens);
	DirectoryArray_free(stack);
}

///////////////////////////////////////

int main (int argc, char *argv[]) {
	selected_modifier = 0;	
	if (autoResume()) return 0; // nothing to do	
	
	char modeStr[256]; 
	char tmpName[256];
	int itemnotchanged = 0;
	int page_seek_plus = BTN_RIGHT;
	int page_seek_minus = BTN_LEFT;
	int state_left = BTN_L2;
	int state_right = BTN_R2;

	if (exists(PAGE_SEEK_TRIGGERS)){
		page_seek_plus = BTN_R2;
		page_seek_minus = BTN_L2;
		state_left = BTN_LEFT;
		state_right = BTN_RIGHT;
	}
	
	sprintf(modeStr, STANDARD_MODE);
	simple_mode = exists(SIMPLE_MODE_PATH);
	fancy_mode = exists(FANCY_MODE_PATH);
	if (simple_mode){
		sprintf(modeStr, SIMPLE_MODE);
	}
	if (fancy_mode){
		simple_mode = 0;
		sprintf(modeStr, FANCY_MODE);
	}
	char pwractionstr[256];
	if (exists(PWR_SLEEP_PATH)){
		sprintf(pwractionstr,"SLP");
	} else {
		sprintf(pwractionstr,"OFF");
	}

	if (exists(HIDE_STATE_PATH)){
		hide_state = 1;
	}

	if (exists(HIDE_BOXART_IFSTATE_PATH)){
		hide_boxartifstate = 1;
	}
	LOG_info("MyMinUI\n");
	InitSettings();
	
	SDL_Surface* screen = GFX_init(MODE_MAIN);
	PAD_init();
	PWR_init();
	if (!HAS_POWER_BUTTON && !simple_mode) PWR_disableSleep();
	
	SDL_Surface* version = NULL;
	
	Menu_init();
	
	// now that (most of) the heavy lifting is done, take a load off
	PWR_setCPUSpeed(CPU_SPEED_MENU);
	GFX_setVsync(VSYNC_STRICT);

	PAD_reset();
	int dirty = 1;
	int mode_changed = 0;
	int show_version = 0;
	int show_setting = 0; // 1=brightness,2=volume
	int was_online = PLAT_isOnline();
	while (!quit) {
		GFX_startFrame();
		unsigned long now = SDL_GetTicks();
	//	LOG_info("START PAD_poll\n");
		PAD_poll();
	//	LOG_info("END PAD_poll\n");
			
		int selected = top->selected;
		int total = top->entries->count;
		
		PWR_update(&dirty, &show_setting, NULL, NULL);
		
		int is_online = PLAT_isOnline();
		if (was_online!=is_online) dirty = 1;
		was_online = is_online;
		
		if (show_version) {
			if (PAD_justPressed(BTN_B) || PAD_tappedMenu(now)) {
				show_version = 0;
				dirty = 1;
				if (!HAS_POWER_BUTTON && !simple_mode) PWR_disableSleep();
				if (mode_changed) { quit = 1;}
			}
			if (PAD_justPressed(BTN_UP) || PAD_justPressed(BTN_DOWN)){
				
				if (fancy_mode) {
					// go to standard mode
					remove(FANCY_MODE_PATH);
					fancy_mode=0;
					simple_mode=0;
					sprintf(modeStr, STANDARD_MODE);
				} else if (simple_mode) {
					// go to fancy_mode
					remove(SIMPLE_MODE_PATH);
					touch(FANCY_MODE_PATH);
					simple_mode = 0;
					fancy_mode = 1;
					sprintf(modeStr, FANCY_MODE);
				} else {
					//go to simple mode
					touch(SIMPLE_MODE_PATH);
					simple_mode = 1;
					fancy_mode = 0;
					sprintf(modeStr, SIMPLE_MODE);
				}
				mode_changed = 1;
				dirty = 1;
			}
		}
		else {
			if (PAD_tappedMenu(now)) {
				show_version = 1;
				dirty = 1;
				if (!HAS_POWER_BUTTON && !simple_mode) PWR_enableSleep();
			}
			else if (total>0) {
				if (PAD_justRepeated(BTN_UP)) {
					itemnotchanged = 0;
					if (selected==0 && !PAD_justPressed(BTN_UP)) {
						// stop at top
					}
					else {
						selected -= 1;
						if (selected<0) {
							selected = total-1;
							int start = total - ( MAIN_ROW_COUNT + fancy_mode );
							top->start = (start<0) ? 0 : start;
							top->end = total; 
						}
						else if (selected<top->start) {
							top->start -= 1;
							top->end -= 1;
						}
					}
				}
				else if (PAD_justRepeated(BTN_DOWN)) {
					itemnotchanged = 0;
					if (selected==total-1 && !PAD_justPressed(BTN_DOWN)) {
						// stop at bottom
					}
					else {
						selected += 1;
						if (selected>=total) {
							selected = 0;
							top->start = 0;
							top->end = (total<( MAIN_ROW_COUNT + fancy_mode )) ? total : ( MAIN_ROW_COUNT + fancy_mode );
						}
						else if (selected>=top->end) {
							top->start += 1;
							top->end += 1;
						}
					}
				}
				if (PAD_justPressed(state_left)) {
					Entry *myentry = top->entries->items[top->selected];
					if (myentry->type == ENTRY_ROM){
						if (can_resume) {
							//int curSaveIndex = getInt(slot_path);
							int curSaveIndex = last_selected_slot;
							curSaveIndex = curSaveIndex < 2  ? 8 : curSaveIndex-1;
							last_selected_slot = curSaveIndex;
							itemnotchanged = 1;
							//putInt(slot_path,curSaveIndex);
							dirty = 1;
						}
					}
					myentry=NULL;
				}
				if (PAD_justPressed(state_right)) {
					Entry *myentry = top->entries->items[top->selected];
					if (myentry->type == ENTRY_ROM){
						if (can_resume) {
							//int curSaveIndex = getInt(slot_path);
							int curSaveIndex = last_selected_slot;
							curSaveIndex = curSaveIndex > 7 ? 1 : curSaveIndex+1;
							//putInt(slot_path,curSaveIndex);
							last_selected_slot = curSaveIndex;
							itemnotchanged = 1;
							dirty = 1;
						}
					}
					myentry = NULL;
				}
				if (PAD_justRepeated(page_seek_minus)) {
					itemnotchanged = 0;
					selected -= ( MAIN_ROW_COUNT + fancy_mode );
					if (selected<0) {
						selected = 0;
						top->start = 0;
						top->end = (total<( MAIN_ROW_COUNT + fancy_mode )) ? total : ( MAIN_ROW_COUNT + fancy_mode );
					}
					else if (selected<top->start) {
						top->start -= ( MAIN_ROW_COUNT + fancy_mode );
						if (top->start<0) top->start = 0;
						top->end = top->start + ( MAIN_ROW_COUNT + fancy_mode );
					}
				}
				else if (PAD_justRepeated(page_seek_plus)) {
					itemnotchanged = 0;
					selected += ( MAIN_ROW_COUNT + fancy_mode );
					if (selected>=total) {
						selected = total-1;
						int start = total - ( MAIN_ROW_COUNT + fancy_mode );
						top->start = (start<0) ? 0 : start;
						top->end = total;
					}
					else if (selected>=top->end) {
						top->end += ( MAIN_ROW_COUNT + fancy_mode );
						if (top->end>total) top->end = total;
						top->start = top->end - ( MAIN_ROW_COUNT + fancy_mode );
					}
				}
			}
		
			if (PAD_justRepeated(BTN_L1) && !PAD_isPressed(BTN_R1) && !PWR_ignoreSettingInput(BTN_L1, show_setting)) { // previous alpha
				itemnotchanged = 0;
				Entry* entry = top->entries->items[selected];
				int i = entry->alpha-1;
				if (i>=0) {
					selected = top->alphas->items[i];
					if (total>( MAIN_ROW_COUNT + fancy_mode )) {
						top->start = selected;
						top->end = top->start + ( MAIN_ROW_COUNT + fancy_mode );
						if (top->end>total) top->end = total;
						top->start = top->end - ( MAIN_ROW_COUNT + fancy_mode );
					}
				}
			}
			else if (PAD_justRepeated(BTN_R1) && !PAD_isPressed(BTN_L1) && !PWR_ignoreSettingInput(BTN_R1, show_setting)) { // next alpha
				itemnotchanged = 0;
				Entry* entry = top->entries->items[selected];
				int i = entry->alpha+1;
				if (i<top->alphas->count) {
					selected = top->alphas->items[i];
					if (total>( MAIN_ROW_COUNT + fancy_mode )) {
						top->start = selected;
						top->end = top->start + ( MAIN_ROW_COUNT + fancy_mode );
						if (top->end>total) top->end = total;
						top->start = top->end - ( MAIN_ROW_COUNT + fancy_mode );
					}
				}
			}
	
			if (selected!=top->selected) {
				top->selected = selected;
				dirty = 1;
			}
	
			if (dirty && total>0 && (!itemnotchanged)) readyResume(top->entries->items[top->selected]);

			if (PAD_justPressed(BTN_SELECT)){
				selected_modifier = 1;
				dirty = 1;
				//printf("\nisPressed SELECT\n");
			}
			if (PAD_justReleased(BTN_SELECT)){
				selected_modifier = 0;
				dirty = 1;
				//printf("\nisReleased SELECT\n");
			}
			
			if (total>0 && can_resume && PAD_justReleased(BTN_RESUME)) {
				should_resume = 1;
				Entry_open(top->entries->items[top->selected]);
				dirty = 1;
			} else if (total>0 && PAD_justPressed(BTN_A)) {
				Entry_open(top->entries->items[top->selected]);
				total = top->entries->count;
				dirty = 1;

				if (total>0) readyResume(top->entries->items[top->selected]);
			}


			/*else if (total>0 && PAD_justPressed(BTN_SELECT)) {
				Entry* myentry = top->entries->items[top->selected];
				if (myentry->type == ENTRY_ROM) {
					toggleFavorite(myentry->path);
					quit = 1;
				}
			}*/
			else if (total>0 && PAD_justPressed(BTN_START)) {
				if (selected_modifier){
					//SELECT pressed so toggle Hidden
					Entry* myentry = top->entries->items[top->selected];
					if (myentry->type == ENTRY_ROM) {
						toggleHidden(myentry->path);
						if (selected>0) {
							selected--;
						} else {
							selected=0;
						}
						dirty = 1;
						//quit = 1;
					}
				} else { 
					//SELECT NOT PRESSED so toggle Favorites
					Entry* myentry = top->entries->items[top->selected];
					if (myentry->type == ENTRY_ROM) {
						toggleFavorite(myentry->path);
						quit = 1;
					}
				}				
			} else if (PAD_justPressed(BTN_B) && stack->count>1) {
				closeDirectory();
				total = top->entries->count;
				dirty = 1;
				// can_resume = 0;
				if (total>0) readyResume(top->entries->items[top->selected]);
			}
		}
		
		if (dirty) {
			GFX_clear(screen);
			
			int ox;
			int oy;
			int ow = GFX_blitHardwareGroup(screen, show_setting, fancy_mode);

			if (show_version) {
				//if (!version) {
					char release[256];
					getFile(ROOT_SYSTEM_PATH "/version.txt", release, 256);
					
					char *tmp,*commit;
					commit = strrchr(release, '\n');
					commit[0] = '\0';
					commit = strrchr(release, '\n')+1;
					tmp = strchr(release, '\n');
					tmp[0] = '\0';
					
					// TODO: not sure if I want bare PLAT_* calls here
					char* extra_key = "Model";
					char* extra_val = PLAT_getModel(); 
					
					SDL_Surface* release_txt = TTF_RenderUTF8_Blended(font.large, "Release", COLOR_DARK_TEXT);
					SDL_Surface* version_txt = TTF_RenderUTF8_Blended(font.large, release, COLOR_WHITE);
					SDL_Surface* commit_txt = TTF_RenderUTF8_Blended(font.large, "Commit", COLOR_DARK_TEXT);
					SDL_Surface* hash_txt = TTF_RenderUTF8_Blended(font.large, commit, COLOR_WHITE);
					
					SDL_Surface* key_txt = TTF_RenderUTF8_Blended(font.large, extra_key, COLOR_DARK_TEXT);
					SDL_Surface* val_txt = TTF_RenderUTF8_Blended(font.large, extra_val, COLOR_WHITE);
					
					int l_width = 0;
					int r_width = 0;
					
					if (release_txt->w>l_width) l_width = release_txt->w;
					if (commit_txt->w>l_width) l_width = commit_txt->w;
					if (key_txt->w>l_width) l_width = commit_txt->w;

					if (version_txt->w>r_width) r_width = version_txt->w;
					if (hash_txt->w>r_width) r_width = hash_txt->w;
					if (val_txt->w>r_width) r_width = val_txt->w;
					
					#define VERSION_LINE_HEIGHT 24
					int x = l_width + SCALE1(8);
					int w = x + r_width;
					int h = SCALE1(VERSION_LINE_HEIGHT*4);
					version = SDL_CreateRGBSurface(0,w,h,16,0,0,0,0);
					
					SDL_BlitSurface(release_txt, NULL, version, &(SDL_Rect){0, 0});
					SDL_BlitSurface(version_txt, NULL, version, &(SDL_Rect){x,0});
					SDL_BlitSurface(commit_txt, NULL, version, &(SDL_Rect){0,SCALE1(VERSION_LINE_HEIGHT)});
					SDL_BlitSurface(hash_txt, NULL, version, &(SDL_Rect){x,SCALE1(VERSION_LINE_HEIGHT)});
					
					SDL_BlitSurface(key_txt, NULL, version, &(SDL_Rect){0,SCALE1(VERSION_LINE_HEIGHT*2)});
					SDL_BlitSurface(val_txt, NULL, version, &(SDL_Rect){x,SCALE1(VERSION_LINE_HEIGHT*2)});
					
					SDL_FreeSurface(release_txt);
					SDL_FreeSurface(version_txt);
					SDL_FreeSurface(commit_txt);
					SDL_FreeSurface(hash_txt);
					SDL_FreeSurface(key_txt);
					SDL_FreeSurface(val_txt);
					SDL_Surface* fixedmode_txt = TTF_RenderUTF8_Blended(font.large, "Mode", COLOR_DARK_TEXT);
					SDL_Surface* mode_txt = TTF_RenderUTF8_Blended(font.large, modeStr, COLOR_WHITE);
					SDL_BlitSurface(fixedmode_txt, NULL, version, &(SDL_Rect){0,SCALE1(VERSION_LINE_HEIGHT*3)});
					SDL_BlitSurface(mode_txt, NULL, version, &(SDL_Rect){x,SCALE1(VERSION_LINE_HEIGHT*3)});
					SDL_FreeSurface(fixedmode_txt);
					SDL_FreeSurface(mode_txt);
				//}
				SDL_BlitSurface(version, NULL, screen, &(SDL_Rect){(screen->w-version->w)/2,(screen->h-version->h)/4});
				
				// buttons (duped and trimmed from below)
				if (show_setting && !GetHDMI()) GFX_blitHardwareHints(screen, show_setting, fancy_mode);
				else GFX_blitButtonGroup((char*[]){ BTN_SLEEP==BTN_POWER?"PWR":"MENU",pwractionstr,  NULL }, 0, screen, 0, fancy_mode);
				
				GFX_blitButtonGroup((char*[]){ "UP/DOWN", "MODE", "B","BACK",  NULL }, 0, screen, 1, fancy_mode);
			}
			else {
				// list
				if (total>0) {
					int selected_row = top->selected - top->start;



						/*  start entry for boxart and save state preview window*/
					if (fancy_mode) {  //only when fancy_mode is active
						Entry* myentry = top->entries->items[top->selected];
						//if (myentry1->type == ENTRY_ROM) {
						// current filename path is entry->path

						char myslot_path[256];
						char myRomName[256];
						char myBoxart_path[256];
						char myEmuName[256];
						char myslot_name[256];
						int myslotint;
						//readyResume(entry);
						if (myentry->type == ENTRY_ROM){
							getStatePath(myentry->path,slot_path_rom);
							//sprintf(slot_path_rom, MYSAVESTATE_PATH "/MAME/States");
							getDisplayNameParens(myentry->path,myslot_name);
							//myslotint = getInt(slot_path);
							myslotint = last_selected_slot;
							if (myslotint){
								sprintf(myslot_path, "%s/%s.state%d.png",slot_path_rom, myslot_name, myslotint);
							} else {
								sprintf(myslot_path, "%s/%s.state.png",slot_path_rom,myslot_name);
							}
							//LOG_info("CIAO\n\n\n\n\n\n%s\n\n\n\n\n\n\n\n",myslot_name);
						}
						
						
						//top->path;
						getParentFolderName(myentry->path, myEmuName);
						getDisplayNameParens(myentry->path, myRomName);
						if ( myentry->type == ENTRY_ROM) {
							sprintf(myBoxart_path, ROMS_PATH "/%s/Imgs/%s.png", myEmuName , myRomName);
						} else if ( myentry->type == ENTRY_DIR ) {
							sprintf(myBoxart_path,  "%s/Imgs/%s.png", myentry->path, myEmuName);
						} else { //pak
							sprintf(myBoxart_path, "%s/Imgs/%s.png", myentry->path, myentry->name);
						}
						/*
						printf("\n\nCurrent item name = %s\nCurrent Item path = %s\nCurrent Item Type = %d\nCurrent Item Save present = %d\nCurrent Item Last Save Slot = %d\nCurrent Item Slot bmp file = %s\nCurrent Item boxart Img = %s\n", 
										myentry->name, myentry->path, myentry->type, can_resume, (can_resume) ? getInt(slot_path) : -1, myslot_path, myBoxart_path);
						fflush(stdout);
						*/
						// the boxart should be entry->path ../Imgs/entry->name.png
						
						//check if a save state should be painted
						int showstate = 0;
						int showboxart = 1;
						if (can_resume && (myentry->type == ENTRY_ROM) && (!(hide_state))) {
							showstate = 1;
							if (hide_boxartifstate) {
								showboxart = 0;
							}
						} 

						if (exists(myBoxart_path) && (showboxart)){
						// print the boxart
							drawBoxart(screen,myBoxart_path);
						}
						// end print boxart
						//print the state slot preview if present
						if (showstate) {
							drawStatePreview(screen, myslot_path, myslotint);	
						}

						//}
						//myentry1 = NULL;
						GFX_blitHardwareGroup(screen, show_setting, fancy_mode);
						// the slot bmp should be in minui_path/EmuName/entry->name
						/* end for boxart and save state preview window, now print the text and the buttons */
				}

					for (int i=top->start,j=0; i<top->end; i++,j++) {
						Entry* entry = top->entries->items[i];
						char* entry_name = entry->name;
						char* entry_unique = entry->unique;

						int available_width = 0;
						TTF_Font *_font = font.large;
						SDL_Color text_color = COLOR_WHITE;
						available_width = screen->w  - SCALE1((PADDING - (PADDING*fancy_mode)) * 2);
						if (fancy_mode) {
							_font = font.medium;
							available_width = screen->w - ( screen->w  * 3 / 5);
							text_color = COLOR_GRAY;
						}
						
						if ((j==selected_row) && (fancy_mode)) {
							text_color = COLOR_WHITE;
							_font = font.large;
							available_width = screen->w  - SCALE1((PADDING - (PADDING*fancy_mode)) * 2);				
						} 
						if ((i==top->start) && !(fancy_mode) ) available_width -= ow;
					
						if (isFavorite(entry->path)) {
							text_color = COLOR_GOLD;
						}

						
						trimSortingMeta(&entry_name);
					
						char display_name[256];
						int text_width = GFX_truncateText(_font, entry_unique ? entry_unique : entry_name, display_name, available_width, SCALE1(BUTTON_PADDING*2));
						int max_width = MIN(available_width, text_width);
						if ((j==selected_row) && !(fancy_mode)) {
							GFX_blitPill(ASSET_WHITE_PILL, screen, &(SDL_Rect){
								SCALE1((PADDING - (PADDING*fancy_mode))),
								SCALE1((PADDING - (PADDING*fancy_mode))+(j*PILL_SIZE)),
								max_width,
								SCALE1(PILL_SIZE)
							});
							text_color = COLOR_BLACK;
						}	else if (entry->unique) {
							trimSortingMeta(&entry_unique);
							char unique_name[256];
							GFX_truncateText(_font, entry_unique, unique_name, available_width, SCALE1(BUTTON_PADDING*2));
						
							SDL_Surface* text = TTF_RenderUTF8_Blended(_font, unique_name, COLOR_DARK_TEXT);
							SDL_BlitSurface(text, &(SDL_Rect){
								0,
								0,
								max_width-SCALE1(BUTTON_PADDING*2),
								text->h
							}, screen, &(SDL_Rect){
								//SCALE1(PADDING+BUTTON_PADDING),
								//SCALE1(PADDING+(j*PILL_SIZE)+4)
								SCALE1((PADDING - (PADDING*fancy_mode))+BUTTON_PADDING-(PADDING*fancy_mode)),
								SCALE1((PADDING - (PADDING*fancy_mode))+(j*(PILL_SIZE-(5*fancy_mode)))+((PILL_SIZE-(5*fancy_mode))*fancy_mode)+4)
							});
						
							GFX_truncateText(_font, entry_name, display_name, available_width, SCALE1(BUTTON_PADDING*2));
						}
						SDL_Surface* text = TTF_RenderUTF8_Blended(_font, display_name, text_color);
						if ((j==selected_row) && (fancy_mode) && (entry->type == ENTRY_ROM)) {  //print the text twice, outline black and body white
							SDL_Surface* textout = TTF_RenderUTF8_Blended(font.largeoutline, display_name, COLOR_BLACK);
							SDL_BlitSurface(textout, &(SDL_Rect){
								0,
								0,
								max_width-SCALE1(BUTTON_PADDING*2),
								textout->h
							}, screen, &(SDL_Rect){
								SCALE1((PADDING - (PADDING*fancy_mode))+BUTTON_PADDING-(PADDING*fancy_mode)),
								SCALE1((PADDING - (PADDING*fancy_mode))+(j*(PILL_SIZE-(5*fancy_mode))+((PILL_SIZE-(5*fancy_mode))*fancy_mode)+4))
							});
							SDL_FreeSurface(textout);
						}
						SDL_BlitSurface(text, &(SDL_Rect){
							0,
							0,
							max_width-SCALE1(BUTTON_PADDING*2),
							text->h
						}, screen, &(SDL_Rect){
							SCALE1((PADDING - (PADDING*fancy_mode))+BUTTON_PADDING-(PADDING*fancy_mode)),
							SCALE1((PADDING - (PADDING*fancy_mode))+(j*(PILL_SIZE-(5*fancy_mode))+((PILL_SIZE-(5*fancy_mode))*fancy_mode)+4))
						});
						SDL_FreeSurface(text);
						}
						if (fancy_mode){		
								//sprintf(tmpName,"                 ");	
								Entry * entry =top->entries->items[top->selected];			
								if (entry->type == ENTRY_ROM) {								
									getDisplayParentFolderName(entry->path, tmpName);
									//getEmuName(entry->path, tmpName);
								} else if (entry->type == ENTRY_PAK){
									sprintf(tmpName, "Tools");
								} else {
									sprintf(tmpName, "Main Menu");
								}
								SDL_Surface* title_txt = TTF_RenderUTF8_Blended(font.small, tmpName, COLOR_WHITE);
								SDL_BlitSurface(title_txt, NULL, screen, &(SDL_Rect){15, 0});
								SDL_FreeSurface(title_txt);	
					}
				}
				else {
					// TODO: for some reason screen's dimensions end up being 0x0 in GFX_blitMessage...
					GFX_blitMessage(font.large, "Empty folder", screen, &(SDL_Rect){0,0,screen->w,screen->h}); //, NULL);
				}
			
				// buttons
				if (show_setting && !GetHDMI()) GFX_blitHardwareHints(screen, show_setting, fancy_mode);
				else if (can_resume) GFX_blitButtonGroup((char*[]){ "X","RSM","START",selected_modifier?"HIDE":"FAV",  NULL }, 0, screen, 0, fancy_mode);
				else {
					if (stack->count>1){
						GFX_blitButtonGroup((char*[]){ 
						BTN_SLEEP==BTN_POWER?"PWR":"MENU",
						BTN_SLEEP==BTN_POWER||simple_mode?pwractionstr:"INFO", "START", (selected_modifier?"HIDE":"FAV"), 
						NULL }, 0, screen, 0, fancy_mode);
					} else { 
						GFX_blitButtonGroup((char*[]){ 
						BTN_SLEEP==BTN_POWER?"PWR":"MENU",
						BTN_SLEEP==BTN_POWER||simple_mode?pwractionstr:"INFO",  
						NULL }, 0, screen, 0, fancy_mode);	
					}
				}
				if (total==0) {
					if (stack->count>1) {
						GFX_blitButtonGroup((char*[]){ "B","BACK",  NULL }, 0, screen, 1, fancy_mode);
					}
				}
				else {
					if (stack->count>1) {
						GFX_blitButtonGroup((char*[]){ "B","BACK", "A",selected_modifier?"RUN2":"RUN", NULL }, 1, screen, 1, fancy_mode);
					}
					else {
						GFX_blitButtonGroup((char*[]){ "A","OPEN", NULL }, 0, screen, 1, fancy_mode);
					}
				}
			}

			GFX_flip(screen);
			GFX_pan();
			dirty = 0;
		}
		else GFX_sync();
	}
	
	if (version) SDL_FreeSurface(version);

	Menu_quit();
	PWR_quit();
	PAD_quit();
	GFX_quit();
	QuitSettings();
}