#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

int prefixMatch(char* pre, char* str);
int suffixMatch(char* suf, char* str);
int exactMatch(char* str1, char* str2);
int hide(char* file_name);

void getDisplayName(const char* in_name, char* out_name);
void getEmuName(const char* in_name, char* out_name);
void getEmuPath(char* emu_name, char* pak_path);

//boxarts
enum Aspect { ASPECT, NATIVE, FULL};

typedef struct _BoxartData {
    int sW;
    int sH;
    int bX;
    int bY;
    int bW;
    int bH;
    enum Aspect aspect;
    char gradient[256];
} myBoxartData;

void getDisplayNameParens(const char* in_name, char* out_name);
void getParentFolderName(const char* in_name, char* out_name);
void getDisplayParentFolderName(const char* in_name, char* out_name);
int readBoxartcfg(char *, myBoxartData *);
void getStatePath(char * gamepath, char* statepath);
int canResume(char * rompath, int last_known_slot);
// end boxarts

void normalizeNewline(char* line);
void trimTrailingNewlines(char* line);
void trimSortingMeta(char** str);
char *trim(char *s);
int exists(char* path);
void touch(char* path);
void putFile(char* path, char* contents);
char* allocFile(char* path); // caller must free
void getFile(char* path, char* buffer, size_t buffer_size);
void putInt(char* path, int value);
int getInt(char* path);
void bmp2png(char * filename);
int getHdmiModeValues(char * hdmimode, int *w, int *h, int *hz);

uint64_t getMicroseconds(void);

#endif
