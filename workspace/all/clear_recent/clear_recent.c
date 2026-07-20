#include <stdio.h>
#include <time.h>
#include <unistd.h>
#if defined(USE_SDL2)
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#else
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#endif

#include <msettings.h>

#include "defines.h"
#include "utils.h"
#include "api.h"


int main(int argc, char* argv[]) {
	PWR_setCPUSpeed(CPU_SPEED_MENU);
	
	SDL_Surface* screen = GFX_init(MODE_MAIN);
	PAD_init();
	PWR_init();
	InitSettings();

    SDL_Event event;
    int quit = 0;
    int save_changes = 0;
    GFX_clear(screen);
    // Show confirmation message
    // GFX_blitHardwareGroup(screen, show_setting);
    GFX_blitMessage(font.large, "Are you sure you want to clear\nRecently Played?", screen, &(SDL_Rect){0,0,screen->w,screen->h});
    GFX_blitButtonGroup((char*[]){ "B","CANCEL", "A","CLEAR", NULL },0, screen, 1, 0);

    GFX_flip(screen);

    // Wait for user's input
    while (!quit) {
        GFX_startFrame();
        PAD_poll();
        if (PAD_justPressed(BTN_A)) {
            save_changes = 1;
            quit = 1;
        } else if (PAD_justPressed(BTN_B)) {
            quit = 1;
        } else {
            GFX_sync();
        }
    }

    // Execute main program based on user's input
    if (save_changes) {
         fclose(fopen(RECENT_PATH, "w"));
    }

    QuitSettings();
    PAD_quit();
    PWR_quit();
    GFX_quit();

    return EXIT_SUCCESS;
}
