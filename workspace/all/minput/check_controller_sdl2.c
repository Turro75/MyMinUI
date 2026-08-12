#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

// Utility function to clear the terminal screen based on the OS
void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

int main(int argc, char* argv[]) {
    // Initialize VIDEO and GAMECONTROLLER subsystem to parse mappings properly
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
        printf("SDL Initialization Error: %s\n", SDL_GetError());
        return 1;
    }

    // Hidden window required to keep the SDL event queue pumped on Linux/ARM systems
    SDL_Window* window = SDL_CreateWindow("Controller Mapped Test", 0, 0, 320, 240, SDL_WINDOW_HIDDEN);

    printf("=== CUSTOM MAPPED CONTROLLER VERIFICATION TOOL ===\n");

    // =================================================================
    // LOADING CONFIGURATION FROM EXTERNAL DATABASE FILE
    // =================================================================
    const char* mapping_file = "gamecontrollerdb.txt";
    
    // SDL_GameControllerAddMappingsFromFile returns loaded entries count, or -1 on failure
    int mappings_loaded = SDL_GameControllerAddMappingsFromFile(mapping_file);
    if (mappings_loaded < 0) {
        printf("[WARNING] Unable to load '%s' (%s).\n", mapping_file, SDL_GetError());
        printf("Using default system layout configurations instead.\n\n");
    } else {
        printf("[SYSTEM] Successfully loaded %d custom definitions from '%s'!\n\n", mappings_loaded, mapping_file);
    }

    printf("Waiting for physical hardware detection...\n");

    SDL_GameController* controller = NULL;
    SDL_Event event;
    bool running = true;

    while (running) {
        while (SDL_PollEvent(&event)) {
            
            // Handles physical controller connection using the high-level API
            if (event.type == SDL_CONTROLLERDEVICEADDED && !controller) {
                controller = SDL_GameControllerOpen(event.cdevice.which);
                if (controller) {
                    clear_screen();
                    printf("[SYSTEM] Controller Attached and Successfully Mapped!\n");
                    printf("Logical Device Name: %s\n", SDL_GameControllerName(controller));
                    printf("Active Mapping Config:\n%s\n", SDL_GameControllerMapping(controller));
                    printf("-----------------------------------------------------------------\n");
                    printf("Press buttons and move sticks to verify if they match your custom layout.\n\n");
                }
            }

            // Handles controller removal gracefully
            if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
                printf("\n[SYSTEM] Controller disconnected or detached!\n");
                SDL_GameControllerClose(controller);
                controller = NULL;
            }

            // Capture standardized inputs mapped by your text file
            if (controller) {
                switch (event.type) {
                    
                    // 1. Mapped Digital Buttons (A, B, X, Y, Shoulders, L3, R3, etc.)
                    case SDL_CONTROLLERBUTTONDOWN: {
                        SDL_GameControllerButton button = (SDL_GameControllerButton)event.cbutton.button;
                        printf("[LOGICAL] Pressed: %s (Linked Hardware ID: b%d)\n", 
                               SDL_GameControllerGetStringForButton(button), event.cbutton.button);
                        break;
                    }
                    case SDL_CONTROLLERBUTTONUP: {
                        SDL_GameControllerButton button = (SDL_GameControllerButton)event.cbutton.button;
                        printf("[LOGICAL] Released: %s\n", SDL_GameControllerGetStringForButton(button));
                        break;
                    }

                    // 2. Mapped Analog Axes and Triggers
                    case SDL_CONTROLLERAXISMOTION: {
                        // Axis jitter filter threshold set to 5000 to cleanly capture tiny movements
                        if (abs(event.caxis.value) > 5000) { 
                            SDL_GameControllerAxis axis = (SDL_GameControllerAxis)event.caxis.axis;
                            printf("[LOGICAL] Axis: %-12s | Position Value: %6d\n", 
                                   SDL_GameControllerGetStringForAxis(axis), event.caxis.value);
                        }
                        break;
                    }

                    // Window closing or system-level shutdown signal
                    case SDL_QUIT:
                        running = false;
                        break;
                }
            }
        }
        SDL_Delay(5); // Protects system threads against 100% CPU core consumption
    }

    // Clean teardown safe procedures
    if (controller) SDL_GameControllerClose(controller);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
