#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>

// SDL GameController standard mapping names
const char* SDL_BUTTON_NAMES[] = {
    "a", "b", "x", "y", 
    "back", "start", 
    "leftshoulder", "rightshoulder", 
    "lefttrigger", "righttrigger",
    "guide" // MENU TICKET AT THE VERY END
};

const char* USER_LABELS[] = {
    "Button [ A ]", "Button [ B ]", "Button [ X ]", "Button [ Y ]",
    "Button [ SELECT ]", "Button [ START ]", 
    "Bumper [ L1 ]", "Bumper [ R1 ]", "Trigger [ L2 ]", "Trigger [ R2 ]",
    "Button [ MENU (Center) ]"
};
#define NUM_BUTTONS (sizeof(SDL_BUTTON_NAMES) / sizeof(SDL_BUTTON_NAMES[0]))

// D-Pad standard directions for the calibration phase
const char* DPAD_NAMES[] = { "dpup", "dpdown", "dpleft", "dpright" };
const char* DPAD_LABELS[] = { "D-PAD [ UP ]", "D-PAD [ DOWN ]", "D-PAD [ LEFT ]", "D-PAD [ RIGHT ]" };
#define NUM_DPAD 4

int main(int argc, char* argv[]) {
    // Initialize VIDEO (for core events) and JOYSTICK to capture raw data accurately
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        printf("SDL Initialization Error: %s\n", SDL_GetError());
        return 1;
    }

    // Dummy window required on Linux ARM systems to keep the event queue alive
    SDL_Window* window = SDL_CreateWindow("Calibration", 0, 0, 320, 240, SDL_WINDOW_HIDDEN);

    printf("=== INTERACTIVE CONTROLLER CALIBRATION TOOL ===\n");
    printf("Waiting for physical hardware detection...\n\n");

    SDL_Joystick* joystick = NULL;
    SDL_Event event;

    // Loop until the system registers a valid hardware index
    while (!joystick) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_JOYDEVICEADDED) {
                joystick = SDL_JoystickOpen(event.jdevice.which);
            }
        }
        SDL_Delay(100);
    }

    // Extract the specific 32-character hardware GUID
    char guid_str[33];
    SDL_JoystickGUID guid = SDL_JoystickGetGUID(joystick);
    SDL_JoystickGetGUIDString(guid, guid_str, sizeof(guid_str));

    printf("[SYSTEM] Joystick attached! Detected GUID: %s\n", guid_str);
    printf("Starting calibration process. You have 3 seconds to press each requested input.\n");
    printf("-----------------------------------------------------------------\n\n");

    // Arrays to store calibrated mapping targets (-1 = skipped/timeout)
    int button_results[NUM_BUTTONS];
    for (int i = 0; i < NUM_BUTTONS; i++) button_results[i] = -1;

    int dpad_hat_index = -1;
    int dpad_hat_values[NUM_DPAD] = { -1, -1, -1, -1 };

    // ==========================================
    // PHASE 1: D-PAD CALIBRATION
    // ==========================================
    printf(">>> PHASE 1: DIRECTIONAL D-PAD CALIBRATION <<<\n");
    for (int i = 0; i < NUM_DPAD; i++) {
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
        printf("Press now: %s ... ", DPAD_LABELS[i]);
        fflush(stdout);

        uint32_t start_time = SDL_GetTicks();
        bool input_registered = false;

        while (SDL_GetTicks() - start_time < 3000 && !input_registered) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_JOYHATMOTION && event.jhat.value != SDL_HAT_CENTERED) {
                    dpad_hat_index = event.jhat.hat;
                    dpad_hat_values[i] = event.jhat.value;
                    input_registered = true;
                    printf("[OK] Hat index: h%d, Value: %d\n", dpad_hat_index, event.jhat.value);
                    break;
                }
            }
            SDL_Delay(10);
        }

        if (!input_registered) {
            printf("[SKIPPED] Timeout reached.\n");
        } else {
            // Wait for the user to completely release the D-Pad direction
            while (SDL_JoystickGetHat(joystick, dpad_hat_index) != SDL_HAT_CENTERED) {
                SDL_PumpEvents();
                SDL_Delay(10);
            }
        }
        SDL_Delay(250);
    }
    printf("\n");

    // ==========================================
    // PHASE 2: BUTTONS & TRIGGERS CALIBRATION
    // ==========================================
    printf(">>> PHASE 2: FACE BUTTONS & TRIGGERS CALIBRATION <<<\n");
    for (int i = 0; i < NUM_BUTTONS; i++) {
        // Clear lingering queue entries before starting the next ticket
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
        
        printf("Press now: %s ... ", USER_LABELS[i]);
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
            printf("[OK] Registered Hardware ID: b%d\n", detected_button);
            
            // Wait for total key release to protect against double impulses
            while (SDL_JoystickGetButton(joystick, detected_button)) {
                SDL_PumpEvents();
                SDL_Delay(10);
            }
        } else {
            printf("[SKIPPED] Timeout reached.\n");
        }
        SDL_Delay(250); // Small safety cooldown delay
    }

    // ==========================================
    // PHASE 3: OUTPUT GENERATION
    // ==========================================
    printf("\n-----------------------------------------------------------------\n");
    printf("=== CALIBRATION SUCCESSFUL! COPY THE FULL STRING BELOW ===\n\n");
    
    // Print the global prefix header block
    printf("%s,Anbernic RG35XXSP Mapped,platform:Linux,", guid_str);

    // Print all dynamically recorded button map blocks
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (button_results[i] != -1) {
            printf("%s:b%d,", SDL_BUTTON_NAMES[i], button_results[i]);
        }
    }

    // Print dynamically recorded D-Pad directional entries
    if (dpad_hat_index != -1) {
        for (int i = 0; i < NUM_DPAD; i++) {
            if (dpad_hat_values[i] != -1) {
                printf("%s:h%d.%d", DPAD_NAMES[i], dpad_hat_index, dpad_hat_values[i]);
                // Add a comma for separation, except for the very last option string character
                if (i < NUM_DPAD - 1) printf(",");
            }
        }
    }
    printf("\n\n-----------------------------------------------------------------\n");

    // Safe teardown clean up procedures
    SDL_JoystickClose(joystick);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
