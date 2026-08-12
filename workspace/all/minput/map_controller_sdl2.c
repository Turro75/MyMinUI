#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

const char* SDL_BUTTON_NAMES[] = {
    "a", "b", "x", "y", "back", "start", 
    "leftshoulder", "rightshoulder", "lefttrigger", "righttrigger",
    "guide", "leftstick", "rightstick"
};

const char* USER_LABELS[] = {
    "Button [ A ]", "Button [ B ]", "Button [ X ]", "Button [ Y ]",
    "Button [ SELECT ]", "Button [ START ]", 
    "Bumper [ L1 ]", "Bumper [ R1 ]", "Trigger [ L2 ] (Digital)", "Trigger [ R2 ] (Digital)",
    "Button [ MENU ]", "Left Stick Click [ L3 ]", "Right Stick Click [ R3 ]"
};
#define NUM_BUTTONS (sizeof(SDL_BUTTON_NAMES) / sizeof(SDL_BUTTON_NAMES[0]))

const char* DPAD_NAMES[] = { "dpup", "dpdown", "dpleft", "dpright" };
const char* DPAD_LABELS[] = { "D-PAD [ UP ]", "D-PAD [ DOWN ]", "D-PAD [ LEFT ]", "D-PAD [ RIGHT ]" };
#define NUM_DPAD 4

const char* AXIS_NAMES[] = { "leftx", "lefty", "rightx", "righty" };
const char* AXIS_LABELS[] = {
    "Left Stick -> Move firmly to the RIGHT",
    "Left Stick -> Move firmly DOWNWARDS",
    "Right Stick -> Move firmly to the RIGHT",
    "Right Stick -> Move firmly DOWNWARDS"
};
#define NUM_AXES 4

// Filter threshold set to 20000 (~60% travel) to completely ignore stick rest jitter
#define AXIS_TRIGGER_THRESHOLD 20000 

int main(int argc, char* argv[]) {
    // Initializing VIDEO, JOYSTICK, and GAMECONTROLLER for high-level rumble checking
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        printf("SDL Initialization Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Calibration & Rumble", 0, 0, 320, 240, SDL_WINDOW_HIDDEN);
    printf("=== INTERACTIVE CONTROLLER CALIBRATION & RUMBLE TOOL ===\n\n");

    SDL_Joystick* joystick = NULL;
    SDL_Event event;

    // Await device connection
    while (!joystick) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_JOYDEVICEADDED) {
                joystick = SDL_JoystickOpen(event.jdevice.which);
            }
        }
        SDL_Delay(100);
    }

    char guid_str[33];
    SDL_JoystickGUID guid = SDL_JoystickGetGUID(joystick);
    SDL_JoystickGetGUIDString(guid, guid_str, sizeof(guid_str));
    printf("[SYSTEM] GUID Detected: %s\n\n", guid_str);

    int button_results[NUM_BUTTONS];
    for (int i = 0; i < NUM_BUTTONS; i++) button_results[i] = -1;

    char dpad_results[NUM_DPAD][16];
    for (int i = 0; i < NUM_DPAD; i++) dpad_results[i][0] = '\0';

    int axis_results[NUM_AXES];
    for (int i = 0; i < NUM_AXES; i++) axis_results[i] = -1;

    // ==========================================
    // PHASE 1: D-PAD CALIBRATION
    // ==========================================
    printf(">>> PHASE 1: D-PAD DIRECTIONAL CALIBRATION <<<\n");
    for (int i = 0; i < NUM_DPAD; i++) {
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
        printf("Press: %s ... ", DPAD_LABELS[i]);
        fflush(stdout);

        uint32_t start_time = SDL_GetTicks();
        bool input_registered = false;

        while (SDL_GetTicks() - start_time < 3000 && !input_registered) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_JOYHATMOTION && event.jhat.value != SDL_HAT_CENTERED) {
                    sprintf(dpad_results[i], "h%d.%d", event.jhat.hat, event.jhat.value);
                    input_registered = true;
                    printf("[OK] Hat: %s\n", dpad_results[i]);
                    break;
                }
                if (event.type == SDL_JOYBUTTONDOWN) {
                    sprintf(dpad_results[i], "b%d", event.jbutton.button);
                    input_registered = true;
                    printf("[OK] Button: %s\n", dpad_results[i]);
                    while (SDL_JoystickGetButton(joystick, event.jbutton.button)) { SDL_PumpEvents(); SDL_Delay(10); }
                    break;
                }
            }
            SDL_Delay(10);
        }
        if (!input_registered) printf("[SKIPPED]\n");
        SDL_Delay(200);
    }

    // ==========================================
    // PHASE 2: ANALOG STICKS CALIBRATION
    // ==========================================
    printf("\n>>> PHASE 2: ANALOG STICKS AXES CALIBRATION <<<\n");
    for (int i = 0; i < NUM_AXES; i++) {
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
        printf("%s ... ", AXIS_LABELS[i]);
        fflush(stdout);

        uint32_t start_time = SDL_GetTicks();
        bool input_registered = false;
        int detected_axis = -1;

        while (SDL_GetTicks() - start_time < 4000 && !input_registered) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_JOYAXISMOTION) {
                    if (abs(event.jaxis.value) > AXIS_TRIGGER_THRESHOLD) {
                        detected_axis = event.jaxis.axis;
                        input_registered = true;
                        printf("[OK] Hardware Axis Detected: a%d\n", detected_axis);
                        break;
                    }
                }
            }
            SDL_Delay(10);
        }

        if (input_registered) {
            axis_results[i] = detected_axis;
            printf("Release stick to center position... ");
            fflush(stdout);
            // Critical loop: wait for current axis to center before asking next step
            while (abs(SDL_JoystickGetAxis(joystick, detected_axis)) > 8000) {
                SDL_PumpEvents();
                SDL_Delay(15);
            }
            printf("OK\n");
        } else {
            printf("[SKIPPED] Timeout reached.\n");
        }
        SDL_Delay(300);
    }
    // ==========================================
    // PHASE 3: BUTTONS & CLICKS CALIBRATION
    // ==========================================
    printf("\n>>> PHASE 3: FACE BUTTONS & STICK CLICKS CALIBRATION <<<\n");
    for (int i = 0; i < NUM_BUTTONS; i++) {
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
        printf("Press: %s ... ", USER_LABELS[i]);
        fflush(stdout);

        uint32_t start_time = SDL_GetTicks();
        bool input_registered = false;
        int detected_button = -1;

        while (SDL_GetTicks() - start_time < 3000 && !input_registered) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_JOYBUTTONDOWN) {
                    detected_button = event.jbutton.button;
                    input_registered = true;
                    break;
                }
            }
            SDL_Delay(10);
        }

        if (input_registered) {
            button_results[i] = detected_button;
            printf("[OK] Hardware ID: b%d\n", detected_button);
            while (SDL_JoystickGetButton(joystick, detected_button)) { SDL_PumpEvents(); SDL_Delay(10); }
        } else {
            printf("[SKIPPED]\n");
        }
        SDL_Delay(200);
    }

    // ==========================================
    // PHASE 4: HARDWARE ANALYSIS & RUMBLE TESTING
    // ==========================================
    printf("\n>>> PHASE 4: RUMBLE MOTORS ANALYSIS & FEEDBACK TEST <<<\n");
    
    SDL_GameController* controller = NULL;
    
    // Scan matching GameController indexes against standard joystick GUID data
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        SDL_JoystickGUID d_guid = SDL_JoystickGetDeviceGUID(i);
        if (memcmp(d_guid.data, guid.data, sizeof(guid.data)) == 0) {
            controller = SDL_GameControllerOpen(i);
            break;
        }
    }

    if (controller) {
        if (SDL_GameControllerHasRumble(controller) == SDL_TRUE) {
            printf("[REPORT] Sottosistema Rumble: AVAILABLE & SUPPORTED by driver layer.\n");
            
            printf("[TEST] Activating LOW FREQUENCY motor (Heavy Rumble - Left)... ");
            fflush(stdout);
            SDL_GameControllerRumble(controller, 0xFFFF, 0x0000, 600); // 100% Left, 0% Right for 600ms
            SDL_Delay(800);
            printf("OK\n");

            printf("[TEST] Activating HIGH FREQUENCY motor (Light Rumble - Right)... ");
            fflush(stdout);
            SDL_GameControllerRumble(controller, 0x0000, 0xFFFF, 600); // 0% Left, 100% Right for 600ms
            SDL_Delay(800);
            printf("OK\n");
            
            printf("[TEST] Activating dual synchronous rumble (100%% Low + 100%% High)... ");
            fflush(stdout);
            SDL_GameControllerRumble(controller, 0xFFFF, 0xFFFF, 500);
            SDL_Delay(600);
            printf("OK\n");
        } else {
            printf("[REPORT] Sottosistema Rumble: NOT AVAILABLE (Driver or hardware does not export motors).\n");
        }

        if (SDL_GameControllerHasRumbleTriggers(controller) == SDL_TRUE) {
            printf("[REPORT] Sottosistema Trigger Rumble: AVAILABLE (Independent L2/R2 motors).\n");
            printf("[TEST] Activating independent L2/R2 Trigger Rumble... ");
            fflush(stdout);
            SDL_GameControllerRumbleTriggers(controller, 0xFFFF, 0xFFFF, 500);
            SDL_Delay(600);
            printf("OK\n");
        } else {
            printf("[REPORT] Sottosistema Trigger Rumble: Not supported.\n");
        }
        
        SDL_GameControllerClose(controller);
    } else {
        printf("[ERROR] Failed to map joystick instance into GameController context for rumble verification.\n");
    }

    // ==========================================
    // PHASE 5: OUTPUT STRING GENERATION
    // ==========================================
    printf("\n-----------------------------------------------------------------\n");
    printf("=== CALIBRATION SUCCESSFUL! COPY THE FULL STRING BELOW ===\n\n");
    printf("%s,Anbernic RG35XXSP Mapped,platform:Linux,", guid_str);

    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (button_results[i] != -1) printf("%s:b%d,", SDL_BUTTON_NAMES[i], button_results[i]);
    }
    for (int i = 0; i < NUM_DPAD; i++) {
        if (dpad_results[i][0] != '\0') printf("%s:%s,", DPAD_NAMES[i], dpad_results[i]);
    }
    for (int i = 0; i < NUM_AXES; i++) {
        if (axis_results[i] != -1) {
            printf("%s:a%d", AXIS_NAMES[i], axis_results[i]);
            if (i < NUM_AXES - 1) printf(",");
        }
    }
    printf("\n-----------------------------------------------------------------\n");

    SDL_JoystickClose(joystick);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
