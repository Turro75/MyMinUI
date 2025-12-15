#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/stat.h>
#include "defines.h"
#include "utils.h"
#include "api.h"

///////////////////////////////////////

int prefixMatch(char* pre, char* str) {
	return (strncasecmp(pre,str,strlen(pre))==0);
}

int suffixMatch(char* suf, char* str) {
	int len = strlen(suf);
	int offset = strlen(str)-len;
	return (offset>=0 && strncasecmp(suf, str+offset, len)==0);
}
int exactMatch(char* str1, char* str2) {
	int len1 = strlen(str1);
	if (len1!=strlen(str2)) return 0;
	return (strncmp(str1,str2,len1)==0);
}
int hide(char* file_name) {
	int result = 0;
	if (file_name[0]=='.' || suffixMatch(".disabled", file_name) || exactMatch("map.txt", file_name)) result = 1;
	if (file_name[0] == 'I' && file_name[1] == 'm' && file_name[2] == 'g' && file_name[3] == 's' && file_name[4] == 0) result = 1; 
	return result;
}

void bmp2png(char * filename){
	char cmd[512];
	sprintf(cmd,"bmp2png.elf -X \"%s\" && rm \"%s.bak\"", filename, filename);
	system(cmd);
}

void getStatePath(char * gamepath, char* statepath){
	//problem, given the full rom path how to retrieve the core used at the time of state creation? minarch knows the core but minui don't.
	//solution 1 -> parse the lauch.sh to get the core name from the line EMU_TAG=xxxxx 
	//solution 2 -> actually it is pointless knowing also the used core as the emuname (i.e. MAME instead of MAME-mame2003plus) is unique and always uses that core.

	//go for solution2 by changing also the minarch behavior
	char emuname[256];
	getEmuName(gamepath, emuname);
	sprintf(statepath, MYSAVESTATE_PATH "/%s/States",emuname);
}

int canResume(char * gamepath, int last_known_slot){
	//check if exists at least one saved state for the currently selected items then return the slot number
	// the target is removing the need of the file slot_path
	char statepath[256];
	char checkpath[256];
	//char slot_path[256];
	struct stat buffer;
	//char checkpath2[256];
	char romname[256];
	getStatePath(gamepath,statepath);
	getDisplayNameParens(gamepath,romname);
	//sprintf(slot_path, "%s/%s.txt",  gamepath);
	
	//ok now checkpath contains the full path of the state file to check if exists regardless of the slotnumber.
	int result = 0;
	uint64_t latest = 0;
	for (int i=1;i<9;i++){
		sprintf(checkpath,"%s/%s.state%d",statepath,romname,i);
		if (exists(checkpath)){
			if (i == last_known_slot) {
				//found the last known slot, return it
				result = i;
				break;
			}
			result = i;
			//ok the saved state exists, check how old it is.
			//stat(checkpath,&buffer);
			//	if (buffer.st_mtime > latest){
			//		latest=buffer.st_mtime;
			//		result = i;
			//	}  not all devices have rtc so we can't do this
			//instead let's check if the last stored slot number is present 
		}
	} 
	return result;
}

void getDisplayName(const char* in_name, char* out_name) {
	char* tmp;
	char work_name[256];
	strcpy(work_name, in_name);
	strcpy(out_name, in_name);
	
	if (suffixMatch("/" PLATFORM, work_name)) { // hide platform from Tools path...
		tmp = strrchr(work_name, '/');
		tmp[0] = '\0';
	}
	
	// extract just the filename if necessary
	tmp = strrchr(work_name, '/');
	if (tmp) strcpy(out_name, tmp+1);
	
	// remove extension(s), eg. .p8.png
	while ((tmp = strrchr(out_name, '.'))!=NULL) {
		int len = strlen(tmp);
		if (len>2 && len<=4) tmp[0] = '\0'; // 3 letter extension plus dot
		else break;
	}
	
	// remove trailing parens (round and square)
	strcpy(work_name, out_name);
	while ((tmp=strrchr(out_name, '('))!=NULL || (tmp=strrchr(out_name, '['))!=NULL) {
		if (tmp==out_name) break;
		tmp[0] = '\0';
		tmp = out_name;
	}
	
	// make sure we haven't nuked the entire name
	if (out_name[0]=='\0') strcpy(out_name, work_name);
	
	// remove trailing whitespace
	tmp = out_name + strlen(out_name) - 1;
    while(tmp>out_name && isspace((unsigned char)*tmp)) tmp--;
    tmp[1] = '\0';
}

void getEmuName(const char* in_name, char* out_name) { // NOTE: both char arrays need to be MAX_PATH length!
	char* tmp;
	strcpy(out_name, in_name);
	tmp = out_name;
	
	// printf("--------\n  in_name: %s\n",in_name); fflush(stdout);
	
	// extract just the Roms folder name if necessary
	if (prefixMatch(ROMS_PATH, tmp)) {
		tmp += strlen(ROMS_PATH) + 1;
		char* tmp2 = strchr(tmp, '/');
		if (tmp2) tmp2[0] = '\0';
		// printf("    tmp1: %s\n", tmp);
		strcpy(out_name, tmp);
		tmp = out_name;
	}

	// finally extract pak name from parenths if present
	tmp = strrchr(tmp, '(');
	if (tmp) {
		tmp += 1;
		// printf("    tmp2: %s\n", tmp);
		strcpy(out_name, tmp);
		tmp = strchr(out_name,')');
		tmp[0] = '\0';
	}
	
	// printf(" out_name: %s\n", out_name); fflush(stdout);
}
void getEmuPath(char* emu_name, char* pak_path) {
	sprintf(pak_path, "%s/Emus/%s/%s.pak/launch.sh", SDCARD_PATH, PLATFORM, emu_name);
	if (exists(pak_path)) return;
	sprintf(pak_path, "%s/Emus/%s.pak/launch.sh", PAKS_PATH, emu_name);
}

//boxarts
void getDisplayNameParens(const char* in_name, char* out_name) {
	char* tmp;
	char work_name[256];
	strcpy(work_name, in_name);
	strcpy(out_name, in_name);

	if (suffixMatch("/" PLATFORM, work_name)) { // hide platform from Tools path...
		tmp = strrchr(work_name, '/');
		tmp[0] = '\0';
	}

	// extract just the filename if necessary
	tmp = strrchr(work_name, '/');
	if (tmp) strcpy(out_name, tmp+1);

	// remove extension(s), eg. .p8.png
	while ((tmp = strrchr(out_name, '.'))!=NULL) {
		int len = strlen(tmp);
		if (len>2 && len<=4) tmp[0] = '\0'; // 3 letter extension plus dot
		else break;
	}


	// make sure we haven't nuked the entire name
	if (out_name[0]=='\0') strcpy(out_name, work_name);

	// remove trailing whitespace
	tmp = out_name + strlen(out_name) - 1;
    while(tmp>out_name && isspace((unsigned char)*tmp)) tmp--;
    tmp[1] = '\0';
}

//get the name of the parent directory with parentesys
void getParentFolderName(const char* in_name, char* out_name) { // NOTE: both char arrays need to be MAX_PATH length!
	char* tmp;
	strcpy(out_name, in_name);
	tmp = out_name;

	// printf("--------\n  in_name: %s\n",in_name); fflush(stdout);

	// extract just the Roms folder name if necessary
	if (prefixMatch(ROMS_PATH, tmp)) {
		tmp += strlen(ROMS_PATH) + 1;
		char* tmp2 = strchr(tmp, '/');
		if (tmp2) tmp2[0] = '\0';
		// printf("    tmp1: %s\n", tmp);
		strcpy(out_name, tmp);
		tmp = out_name;
	}

}


//get the name of the parent directory removing parentesys
void getDisplayParentFolderName(const char* in_name, char* out_name) { // NOTE: both char arrays need to be MAX_PATH length!
	char* tmp, *tmp3;
	strcpy(out_name, in_name);
	tmp = out_name;

	// printf("--------\n  in_name: %s\n",in_name); fflush(stdout);
	
	// extract just the Roms folder name if necessary
	if (prefixMatch(ROMS_PATH, tmp)) {
		tmp += strlen(ROMS_PATH) + 1;
		char* tmp2 = strchr(tmp, '/');
		if (tmp2) tmp2[0] = '\0';
		// printf("    tmp1: %s\n", tmp);
		strcpy(out_name, tmp);
		tmp = out_name;
	}
	//remove parentesys as we need only the system name
	tmp3 = strrchr(tmp, '(');
	if (tmp3) {
		tmp = strtok(tmp,"(");
		tmp = out_name;
	}
}

char *ltrim(char *s)
{
    while(isspace(*s)) s++;
    return s;
}

char *rtrim(char *s)
{
    char* back = s + strlen(s);
    while(isspace(*--back));
    *(back+1) = '\0';
    return s;
}

char *trim(char *s)
{
    return rtrim(ltrim(s)); 
}


int readBoxartcfg(char * filecfg, myBoxartData * boxartdata){
	//read BOXART_CFGFILE then fill the struct woth all read data
	FILE *config = fopen(filecfg,"r");
    //LOG_info("Opening CFG file %s\n", BOXART_CFGFILE);
    char *mygradient;
    if (config) {
		char * pch;
		char parametri[512];
        //LOG_info("Reading CFG file %s\n", BOXART_CFGFILE);
        while (fgets(parametri, sizeof(parametri), config)) {
            char tmpstr[20];
                if (strncmp(parametri,"BX =",4) == 0){
                pch = strtok (parametri,"=");
                pch = strtok (NULL, "=");
                boxartdata->bX = strtol(pch,NULL,10);
                //LOG_info("BX = %d\n", boxartdata.bX);
                }
            if (strncmp(parametri,"BY =",4) == 0){
                pch = strtok (parametri,"=");
                pch = strtok (NULL, "=");
                boxartdata->bY = strtol(pch,NULL,10);
                //LOG_info("BY = %d\n", boxartdata.bY);
                }
            if (strncmp(parametri,"BW =",4) == 0){
                pch = strtok (parametri,"=");
                pch = strtok (NULL, "=");
                boxartdata->bW = strtol(pch,NULL,10);
                //LOG_info("BW = %d\n", boxartdata.bW);
                }
            if (strncmp(parametri,"BH =",4) == 0){
                pch = strtok (parametri,"=");
                pch = strtok (NULL, "=");
                boxartdata->bH = strtol(pch,NULL,10);
                //LOG_info("BH = %d\n", boxartdata.bH);
                }
            if (strncmp(parametri,"SW =",4) == 0){
                pch = strtok (parametri,"=");
                pch = strtok (NULL, "=");
                boxartdata->sW = strtol(pch,NULL,10);
                //LOG_info("SW = %d\n", boxartdata.sW);
                }
            if (strncmp(parametri,"SH =",4) == 0){
                pch = strtok (parametri,"=");
                pch = strtok (NULL, "=");
                boxartdata->sH = strtol(pch,NULL,10);
                //LOG_info("SH = %d\n", boxartdata.sH);
                }
            if (strncmp(parametri,"ASPECT =",8) == 0){
                pch = strtok (parametri,"=");
                pch = strtok (NULL, "=");
                boxartdata->aspect = strtol(pch,NULL,10);
                //LOG_info("ASPECT = %d\n", boxartdata.aspect);
                }
            if (strncmp(parametri,"GRADIENT = ",11) == 0){
                pch = strtok (parametri,"=");
                pch = strtok (NULL, "=");
				mygradient = trim(pch);
                sprintf(boxartdata->gradient,"%s", mygradient);
                //LOG_info("GRADIENT = #%s#\n",(boxartdata.gradient);
                }            
        }
        fclose(config);
		return 1;
    }
	return -1;
}


// end boxarts



void normalizeNewline(char* line) {
	int len = strlen(line);
	if (len>1 && line[len-1]=='\n' && line[len-2]=='\r') { // windows!
		line[len-2] = '\n';
		line[len-1] = '\0';
	}
}
void trimTrailingNewlines(char* line) {
	int len = strlen(line);
	while (len>0 && line[len-1]=='\n') {
		line[len-1] = '\0'; // trim newline
		len -= 1;
	}
}
void trimSortingMeta(char** str) { // eg. `001) `
	// TODO: this code is suss
	char* safe = *str;
	while(isdigit(**str)) *str += 1; // ignore leading numbers

	if (*str[0]==')') { // then match a closing parenthesis
		*str += 1;
	}
	else { //  or bail, restoring the string to its original value
		*str = safe;
		return;
	}
	
	while(isblank(**str)) *str += 1; // ignore leading space
}

///////////////////////////////////////

int exists(char* path) {
	return access(path, F_OK)==0;
}
void touch(char* path) {
	close(open(path, O_RDWR|O_CREAT, 0777));
}
void putFile(char* path, char* contents) {
	FILE* file = fopen(path, "w");
	if (file) {
		fputs(contents, file);
		fclose(file);
	}
}
void getFile(char* path, char* buffer, size_t buffer_size) {
	FILE *file = fopen(path, "r");
	if (file) {
		fseek(file, 0L, SEEK_END);
		size_t size = ftell(file);
		if (size>buffer_size-1) size = buffer_size - 1;
		rewind(file);
		fread(buffer, sizeof(char), size, file);
		fclose(file);
		buffer[size] = '\0';
	}
}
char* allocFile(char* path) { // caller must free!
	char* contents = NULL;
	FILE *file = fopen(path, "r");
	if (file) {
		fseek(file, 0L, SEEK_END);
		size_t size = ftell(file);
		contents = calloc(size+1, sizeof(char));
		fseek(file, 0L, SEEK_SET);
		fread(contents, sizeof(char), size, file);
		fclose(file);
		contents[size] = '\0';
	}
	return contents;
}
int getInt(char* path) {
	int i = 0;
	FILE *file = fopen(path, "r");
	if (file!=NULL) {
		fscanf(file, "%i", &i);
		fclose(file);
	}
	return i;
}
void putInt(char* path, int value) {
	char buffer[8];
	sprintf(buffer, "%d", value);
	putFile(path, buffer);
}

uint64_t getMicroseconds(void) {
    uint64_t ret;
    struct timeval tv;

    gettimeofday(&tv, NULL);

    ret = (uint64_t)tv.tv_sec * 1000000;
    ret += (uint64_t)tv.tv_usec;

    return ret;
}


int getHdmiModeValues(char * hdmimode, int *w, int *h, int *hz){
	// Inizializza i valori a 0
	int localw = *w;
	int localh = *h;
	int localhz = *hz;
    *w = 0;
    *h = 0;
    *hz = 0;

    // Trova la posizione dei separatori 'x' e 'p'
    char *x_pos = strchr(hdmimode, 'x');
    char *p_pos = strchr(hdmimode, 'p');

    if (x_pos && p_pos) {
        // Estrai la larghezza (w)
        *x_pos = '\0'; // Termina la stringa temporaneamente
        *w = atoi(hdmimode);

        // Estrai l'altezza (h)
        char *height_start = x_pos + 1;
        *p_pos = '\0'; // Termina la stringa temporaneamente
        *h = atoi(height_start);

        // Estrai la frequenza (hz)
        char *hz_start = p_pos + 1;
        *hz = atoi(hz_start);
		return 0;
    } else {
		// Se il formato non è corretto, ripristina i valori originali
		*w = localw;
		*h = localh;
		*hz = localhz;
		return -1;
	}
}

//add cheat support
void truncateString(char *string, size_t max_len) {
	size_t len = strlen(string) + 1;
	if (len <= max_len) return;

	strncpy(&string[max_len - 4], "...\0", 4);
}
void wrapString(char *string, size_t max_len, size_t max_lines) {
	char *line = string;

	for (size_t i = 1; i < max_lines; i++) {
		char *p = line;
		char *prev;
		do {
			prev = p;
			p = strchr(prev+1, ' ');
		} while (p && p - line < (int)max_len);

		if (!p && strlen(line) < max_len) break;

		if (prev && prev != line) {
			line = prev + 1;
			*prev = '\n';
		}
	}
	truncateString(line, max_len);
}
//end cheat support