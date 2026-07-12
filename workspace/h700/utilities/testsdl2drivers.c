#include <SDL2/SDL.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    // Il sistema principale deve essere inizializzato senza sottosistemi specifici
    if (SDL_Init(0) < 0) {
        printf("Errore critico di inizializzazione SDL: %s\n", SDL_GetError());
        return 1;
    }

    // 1. TEST DEI DRIVER VIDEO UNO A UNO
    printf("=== TEST COMPATIBILITÀ DRIVER VIDEO ===\n");
    int numVideoDrivers = SDL_GetNumVideoDrivers();
    
    for (int i = 0; i < numVideoDrivers; i++) {
        const char* driverName = SDL_GetVideoDriver(i);
        
        // Tenta l'inizializzazione del singolo driver
        if (SDL_VideoInit(driverName) == 0) {
            printf("  [OK]  %s -> ATTIVABILE\n", driverName);
            SDL_VideoQuit(); // Chiude il driver appena testato per liberare la risorsa
        } else {
            printf("  [ERR] %s -> NON ATTIVABILE (%s)\n", driverName, SDL_GetError());
        }
    }

    // 2. TEST DEI DRIVER AUDIO UNO A UNO
    printf("\n=== TEST COMPATIBILITÀ DRIVER AUDIO ===\n");
    int numAudioDrivers = SDL_GetNumAudioDrivers();
    
    for (int i = 0; i < numAudioDrivers; i++) {
        const char* driverName = SDL_GetAudioDriver(i);
        
        // Tenta l'inizializzazione del singolo driver
        if (SDL_AudioInit(driverName) == 0) {
            printf("  [OK]  %s -> ATTIVABILE\n", driverName);
            SDL_AudioQuit(); // Chiude il driver appena testato per liberare la risorsa
        } else {
            printf("  [ERR] %s -> NON ATTIVABILE (%s)\n", driverName, SDL_GetError());
        }
    }

    SDL_Quit();
    return 0;
}
