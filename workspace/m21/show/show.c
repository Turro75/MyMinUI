#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <linux/kd.h>
#include <signal.h>
#include <sys/ioctl.h>
#if defined(USE_SDL2)
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "SDL2_rotozoom.h"
#else
#include <SDL/SDL.h>
#include "SDL_rotozoom.h"
#endif
#include "platform.h"

SDL_Surface* bmp = NULL;
SDL_Surface* screen = NULL;
SDL_Surface* preview = NULL;
void killHandler(int retvalue)
{
    if (bmp) SDL_FreeSurface(bmp);
	if (screen) {
		PLAT_quitVideo();
		//SDL_FreeSurface(screen);
	}	
	if (preview) SDL_FreeSurface(preview);
	exit(retvalue);
}

int main(int argc , char* argv[]) {
	if (argc<2) {
		fprintf(stdout,"Usage: show image.png delay");
		return 0;
	}
	signal(SIGKILL, killHandler);
	signal(SIGTERM, killHandler);
	signal(SIGINT, killHandler);
	signal(SIGQUIT, killHandler);
	char path[256];
	strncpy(path,argv[1],256);
	if (access(path, F_OK)!=0) return 0; // nothing to show :(
	int delay = argc>2 ? atoi(argv[2]) : 2;

	bmp = IMG_Load(path);
	if (!bmp) {
		fprintf(stdout,"Failed to load image %s\n",path);fflush(stdout);
		killHandler(-1);
	}
	screen = PLAT_initVideo();
	preview = zoomSurface(bmp, (1.0 * screen->w / bmp->w) , (1.0 * screen->h / bmp->h), 0);
	if (!preview) {
		fprintf(stdout,"Failed to scale image %s\n",path);fflush(stdout);
		killHandler(-1);
	}				
	SDL_BlitSurface(preview, NULL, screen, NULL); 					
	PLAT_flip(screen, 0);
	sleep(delay);
	killHandler(0);
}
