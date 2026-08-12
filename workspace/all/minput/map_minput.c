#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <msettings.h>

#include "sdl.h"
#include "defines.h"
#include "api.h"
#include "utils.h"

#define AXIS_TRIGGER_THRESHOLD 20000
#define AXIS_RELEASE_THRESHOLD 8000
#define STICK_BOX_SIZE 104  

#define TOTAL_STEPS  (sizeof(SDL_MAP_NAMES) / sizeof(SDL_MAP_NAMES[0]))

const char* SDL_MAP_NAMES[] = {
    "dpup", "dpdown", "dpleft", "dpright",
    "leftx", "lefty", "rightx", "righty",
    "a", "b", "x", "y", "back", "start", 
    "leftshoulder", "rightshoulder", "lefttrigger", "righttrigger",
    "guide", "leftstick", "rightstick"
};

const char* STEP_PROMPTS[] = {
    "D-PAD: Press UP", "D-PAD: Press DOWN", "D-PAD: Press LEFT", "D-PAD: Press RIGHT",
    "LEFT STICK: Push RIGHT", "LEFT STICK: Push DOWN", "RIGHT STICK: Push RIGHT", "RIGHT STICK: Push DOWN",
    "Button: Press A", "Button: Press B", "Button: Press X", "Button: Press Y",
    "Button: Press SELECT", "Button: Press START",
    "Bumper: Press L1", "Bumper: Press R1", "Trigger: Press L2", "Trigger: Press R2",
    "Button: Press MENU (Guide)", "Click: Press LEFT STICK (L3)", "Click: Press RIGHT STICK (R3)"
};

typedef struct {
    int buttons[TOTAL_STEPS]; 
    int axes[TOTAL_STEPS];    
    int hat_id;               
    int hat_values[4];        
} LocalControllerMap;

void SurfaceDrawBox(SDL_Surface* dest, int x, int y, int w, int h, Uint32 color_hex) {
    SDL_Rect rect = { x, y, w, h };
    SDL_FillRect(dest, &rect, color_hex);
}

void SurfaceDrawText(SDL_Surface* dest, const char* text, int x, int y, SDL_Color color) {
    if (!text || strlen(text) == 0 || !font.tiny) return;
    SDL_Surface* text_surf = TTF_RenderUTF8_Blended(font.tiny, text, color);
    if (!text_surf) return;
    SDL_Rect dest_rect = { x, y, 0, 0 };
    SDL_BlitSurface(text_surf, NULL, dest, &dest_rect);
    SDL_FreeSurface(text_surf);
}

void SurfaceDrawCircle(SDL_Surface* dest, int centerX, int centerY, int radius, Uint32 color) {
    for (int w = 0; w < radius * 2; w++) {
        for (int h = 0; h < radius * 2; h++) {
            int dx = radius - w; 
            int dy = radius - h; 
            if ((dx*dx + dy*dy) <= (radius * radius)) {
                SurfaceDrawBox(dest, centerX + dx, centerY + dy, 1, 1, color);
            }
        }
    }
}
void SurfaceDrawBoxOutline(SDL_Surface* dest, int x, int y, int size, Uint32 color) {
    SurfaceDrawBox(dest, x, y, size, 2, color);             
    SurfaceDrawBox(dest, x, y + size - 2, size, 2, color);  
    SurfaceDrawBox(dest, x, y, 2, size, color);             
    SurfaceDrawBox(dest, x + size - 2, y, 2, size, color);  
    SurfaceDrawBox(dest, x + (size / 2) - 1, y + 4, 2, size - 8, SDL_MapRGB(dest->format, 35, 45, 75));
    SurfaceDrawBox(dest, x + 4, y + (size / 2) - 1, size - 8, 2, SDL_MapRGB(dest->format, 35, 45, 75));
}

void SurfaceDrawTargetPoint(SDL_Surface* dest, int box_x, int box_y, const char* axis_target, Uint32 color_hex) {
    int target_x = box_x + (STICK_BOX_SIZE / 2);
    int target_y = box_y + (STICK_BOX_SIZE / 2);

    if (strstr(axis_target, "x")) {
        target_x = box_x + STICK_BOX_SIZE - 12;
    } else if (strstr(axis_target, "y")) {
        target_y = box_y + STICK_BOX_SIZE - 12;
    }
    SurfaceDrawCircle(dest, target_x, target_y, 6, color_hex);
}

// FIXED: Receives clean string pointers, writes mapping row, and flushes cache natively
void SaveMappingToFile(const char* filename, const char* guid, const char* name_str, char lines[TOTAL_STEPS][16]) {
    FILE* file = fopen(filename, "a");
    if (!file) return;

    fprintf(file, "%s,%s,platform:Linux,", guid, name_str);
    for (int i = 0; i < TOTAL_STEPS; i++) {
        if (strlen(lines[i]) > 0) {
            fprintf(file, "%s:%s", SDL_MAP_NAMES[i], lines[i]);
            if (i < TOTAL_STEPS - 1) fprintf(file, ",");
        }
    }
    fprintf(file, "\n");
    fflush(file); // FIXED: Force sync data execution onto storage partition
    fclose(file);
}
bool ParseCustomMappingFile(const char* filename, const char* target_guid, LocalControllerMap* out_map) {
    for (int i = 0; i < TOTAL_STEPS; i++) {
        out_map->buttons[i] = -1;
        out_map->axes[i] = -1;
    }
    out_map->hat_id = -1;
    for (int i = 0; i < 4; i++) out_map->hat_values[i] = -1;

    FILE* file = fopen(filename, "r");
    if (!file) return false;

    char line[1024]; // FIXED: Proper stack string buffer allocated
    bool found = false;

    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, target_guid) != NULL) {
            found = true;
            char* token = strtok(line, ",");
            while (token != NULL) {
                char* colon = strchr(token, ':');
                if (colon != NULL) {
                    *colon = '\0';
                    char* key = token;
                    char* val = colon + 1;

                    int step_idx = -1;
                    for (int i = 0; i < TOTAL_STEPS; i++) {
                        if (strcmp(key, SDL_MAP_NAMES[i]) == 0) {
                            step_idx = i;
                            break;
                        }
                    }

                    if (step_idx != -1) {
                        if (val[0] == 'b') {
                            out_map->buttons[step_idx] = atoi(val + 1);
                        } else if (val[0] == 'a') {
                            out_map->axes[step_idx] = atoi(val + 1);
                        } else if (val[0] == 'h') {
                            out_map->hat_id = val[1] - '0';
                            char* dot = strchr(val, '.');
                            if (dot != NULL && step_idx < 4) {
                                out_map->hat_values[step_idx] = atoi(dot + 1);
                            }
                        }
                    }
                }
                token = strtok(NULL, ",");
            }
            break;
        }
    }
    fclose(file);
    return found;
}
int main(int argc, char* argv[]) {
    InitSettings();
    SDL_Surface* screen = GFX_init(MODE_MAIN); 
    PWR_init();
    PWR_setCPUSpeed(CPU_SPEED_MENU);

    // Recupero del modello della console come fallback stabile
    char safe_model_name[64];
    const char* system_raw_ptr = PLAT_getModel();
    if (system_raw_ptr != NULL && strlen(system_raw_ptr) > 0) {
        snprintf(safe_model_name, sizeof(safe_model_name), "MyMinUI %s", system_raw_ptr);
    } else {
        snprintf(safe_model_name, sizeof(safe_model_name), "MyMinUI UnknownDevice");
    }

    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
        QuitSettings(); PWR_quit(); GFX_quit();
        return 1;
    }

    SDL_Joystick* joystick = NULL;
    SDL_Event event;
    int current_joystick_index = 0;

    // AUTO-ROUTING EXTERNAL PAD: Se rileva più di un controller, punta direttamente all'ultimo (esterno)
    int attached_count = SDL_NumJoysticks();
    if (attached_count > 1) {
        current_joystick_index = attached_count - 1; 
    }

    // Attesa e aggancio del joystick selezionato
    while (!joystick) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_JOYDEVICEADDED) {
                int live_count = SDL_NumJoysticks();
                current_joystick_index = (live_count > 1) ? (live_count - 1) : 0;
                joystick = SDL_JoystickOpen(current_joystick_index);
            }
            if (event.type == SDL_QUIT) {
                SDL_QuitSubSystem(SDL_INIT_JOYSTICK); QuitSettings(); PWR_quit(); GFX_quit(); 
                return 0;
            }
        }
        SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 10, 15, 30));
        GFX_flip(screen);
        SDL_Delay(50);
    }

    // Estrazione sicura del GUID hardware in un array allocato a 33 byte
    char guid_str[33]; 
    SDL_JoystickGUID guid = SDL_JoystickGetGUID(joystick);
    SDL_JoystickGetGUIDString(guid, guid_str, sizeof(guid_str));

    // Estrazione dinamica del nome del controller (es. Gamesir) con fallback sulla console se assente
    char controller_final_name[128]; 
    const char* sdl_hw_name = SDL_JoystickName(joystick);
    if (sdl_hw_name != NULL && strlen(sdl_hw_name) > 0) {
        snprintf(controller_final_name, sizeof(controller_final_name), "%s", sdl_hw_name);
    } else {
        snprintf(controller_final_name, sizeof(controller_final_name), "%s", safe_model_name);
    }

    // Inizializzazione corretta dell'array bidimensionale di stringhe per i 21 step
    char final_mappings[TOTAL_STEPS][16]; 
    for (int i = 0; i < TOTAL_STEPS; i++) final_mappings[i][0] = '\0';

    int current_step = 0;
    uint32_t step_start_time = SDL_GetTicks();
    int running = 1;
    
    char live_feedback[256]; 
    snprintf(live_feedback, sizeof(live_feedback), "Active Pad: %s", controller_final_name);

    bool waiting_for_release = false;
    int primary_active_input_id = -1;
    bool global_select_held = false;
    bool global_start_held = false;

    SDL_Color text_clr = { 255, 255, 255, 255 }; 
    Uint32 c_white = SDL_MapRGB(screen->format, 255, 255, 255);
    Uint32 c_gold  = SDL_MapRGB(screen->format, 241, 196, 15);
    Uint32 c_navy  = SDL_MapRGB(screen->format, 5, 10, 40);
    Uint32 c_red   = SDL_MapRGB(screen->format, 231, 76, 60);

    int center_x = screen->w / 2;
    int center_y = screen->h / 2;
    while (running) {
        uint32_t current_time = SDL_GetTicks();
        uint32_t time_elapsed = current_time - step_start_time;

        // Gestione protettiva del timeout (5 secondi)
        if (time_elapsed >= 5000) {
            current_step++;
            step_start_time = SDL_GetTicks();
            waiting_for_release = false;
            primary_active_input_id = -1;
            SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
        }

        // CONTROLLO DI SICUREZZA SUPERIORE: Intercetta l'indice 21 se incrementato dal timeout
        if (current_step >= TOTAL_STEPS) {
            LOG_info("controller found as GUID=%s - NAME=%s\n", guid_str, controller_final_name); 
            fflush(stdout);
            SaveMappingToFile("my_sdlcontroller.txt", guid_str, controller_final_name, final_mappings);
            SDL_Delay(1500);
            SDL_JoystickClose(joystick); SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
            QuitSettings(); PWR_quit(); GFX_quit(); return 0; 
        }

        const char* current_target = SDL_MAP_NAMES[current_step];

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;

            // Intercettazione combinazione grezza di uscita (SELECT + START hardware)
            if (event.type == SDL_JOYBUTTONDOWN) {
                int btn = event.jbutton.button;
                if (btn == 4 || btn == 11) global_select_held = true;
                if (btn == 5 || btn == 12) global_start_held = true;
            }
            if (event.type == SDL_JOYBUTTONUP) {
                int btn = event.jbutton.button;
                if (btn == 4 || btn == 11) global_select_held = false;
                if (btn == 5 || btn == 12) global_start_held = false;
            }
            if (global_select_held && global_start_held) { running = 0; break; }

            // GESTIONE DEL RILASCIO (Fissata con controllo atomico di fine corsa)
            if (waiting_for_release && primary_active_input_id != -1) {
                bool released = false;
                if (event.type == SDL_JOYBUTTONUP && event.jbutton.button == primary_active_input_id) released = true;
                else if (event.type == SDL_JOYHATMOTION && event.jhat.hat == primary_active_input_id && event.jhat.value == SDL_HAT_CENTERED) released = true;
                else if (event.type == SDL_JOYAXISMOTION && event.jaxis.axis == primary_active_input_id && abs(event.jaxis.value) < AXIS_RELEASE_THRESHOLD) released = true;

                if (released) {
                    waiting_for_release = false; primary_active_input_id = -1;
                    current_step++; step_start_time = SDL_GetTicks();

                    // INTERCETTAZIONE ATOMICA IMMEDIATA: Esce all'istante dall'ultimo rilascio, salvando il 21esimo tasto
                    if (current_step >= TOTAL_STEPS) {
                        LOG_info("controller found as GUID=%s - NAME=%s\n", guid_str, controller_final_name); fflush(stdout);
                        SaveMappingToFile("my_sdlcontroller.txt", guid_str, controller_final_name, final_mappings);
                        SDL_Delay(1500);
                        SDL_JoystickClose(joystick); SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
                        QuitSettings(); PWR_quit(); GFX_quit(); return 0; 
                    }
                }
                continue; 
            }

            // GESTIONE DELLA PRESSIONE INPUT
            if (!waiting_for_release) {
                if (current_step >= 4 && current_step <= 7) { 
                    if (event.type == SDL_JOYAXISMOTION && abs(event.jaxis.value) > AXIS_TRIGGER_THRESHOLD) {
                        int det_axis = event.jaxis.axis;
                        sprintf(final_mappings[current_step], "a%d", det_axis);
                        primary_active_input_id = det_axis; waiting_for_release = true;
                    }
                } 
                else {
                    if (event.type == SDL_JOYBUTTONDOWN) {
                        int det_btn = event.jbutton.button;
                        sprintf(final_mappings[current_step], "b%d", det_btn);
                        primary_active_input_id = det_btn; waiting_for_release = true;
                    }
                    else if (event.type == SDL_JOYHATMOTION && event.jhat.value != SDL_HAT_CENTERED && current_step < 4) {
                        sprintf(final_mappings[current_step], "h%d.%d", event.jhat.hat, event.jhat.value);
                        primary_active_input_id = event.jhat.hat; waiting_for_release = true;
                    }
                }
            }
        }

        if (!running) break;

        // =================================================================
        // COMPOSITING INTERFACCIA GRAFICA (Sicura al 100% da indici out of bounds)
        // =================================================================
        SDL_FillRect(screen, NULL, c_navy); 

        int bar_width = (int)(((5000 - time_elapsed) / 5000.0f) * screen->w);
        SurfaceDrawBox(screen, 0, 0, bar_width, 12, c_red);

        SurfaceDrawText(screen, "=== INTERACTIVE CONTROLLER MAPPING ===", 40, 30, text_clr);
        SurfaceDrawText(screen, live_feedback, 40, screen->h - 55, text_clr);
        SurfaceDrawText(screen, "Hotkey: SELECT+START held together will force direct quick exit", 40, screen->h - 35, text_clr);
        
        int text_width_offset = (strlen(STEP_PROMPTS[current_step]) * 7) / 2;
        SurfaceDrawText(screen, STEP_PROMPTS[current_step], center_x - text_width_offset, center_y, text_clr);

        // Croce D-Pad Sinistra
        int dpad_center_x = center_x - 225; int dpad_center_y = center_y;
        SurfaceDrawCircle(screen, dpad_center_x, dpad_center_y - 50, 18, (strcmp(current_target, "dpup") == 0) ? c_gold : c_white);
        SurfaceDrawCircle(screen, dpad_center_x, dpad_center_y + 50, 18, (strcmp(current_target, "dpdown") == 0) ? c_gold : c_white);
        SurfaceDrawCircle(screen, dpad_center_x - 50, dpad_center_y, 18, (strcmp(current_target, "dpleft") == 0) ? c_gold : c_white);
        SurfaceDrawCircle(screen, dpad_center_x + 50, dpad_center_y, 18, (strcmp(current_target, "dpright") == 0) ? c_gold : c_white);

        // Diamante Pulsanti Frontali Destra
        SurfaceDrawCircle(screen, center_x + 175, center_y, 18, (strcmp(current_target, "y") == 0) ? c_gold : c_white);      
        SurfaceDrawCircle(screen, center_x + 225, center_y - 50, 18, (strcmp(current_target, "x") == 0) ? c_gold : c_white); 
        SurfaceDrawCircle(screen, center_x + 275, center_y, 18, (strcmp(current_target, "a") == 0) ? c_gold : c_white);      
        SurfaceDrawCircle(screen, center_x + 225, center_y + 50, 18, (strcmp(current_target, "b") == 0) ? c_gold : c_white); 

        // Modificatori Centrali
        SurfaceDrawBox(screen, center_x - 70, center_y + 175, 40, 15, (strcmp(current_target, "back") == 0) ? c_gold : c_white);
        SurfaceDrawBox(screen, center_x + 30, center_y + 175, 40, 15, (strcmp(current_target, "start") == 0) ? c_gold : c_white);
        SurfaceDrawCircle(screen, center_x, center_y - 110, 12, (strcmp(current_target, "guide") == 0) ? c_gold : c_white);

        // Spalline Dorsali e Trigger Superiori
        SurfaceDrawBox(screen, center_x - 160, center_y - 170, 50, 15, (strcmp(current_target, "leftshoulder") == 0) ? c_gold : c_white);
        SurfaceDrawBox(screen, center_x + 110, center_y - 170, 50, 15, (strcmp(current_target, "rightshoulder") == 0) ? c_gold : c_white);
        SurfaceDrawBox(screen, center_x - 100, center_y - 170, 50, 15, (strcmp(current_target, "lefttrigger") == 0) ? c_gold : c_white);
        SurfaceDrawBox(screen, center_x + 50, center_y - 170, 50, 15, (strcmp(current_target, "righttrigger") == 0) ? c_gold : c_white);

        // Box Stick Sinistro
        int box_l_x = center_x - 150, box_l_y = center_y + 55;
        int active_l = (strcmp(current_target, "leftx") == 0 || strcmp(current_target, "lefty") == 0 || strcmp(current_target, "leftstick") == 0);
        SurfaceDrawBoxOutline(screen, box_l_x, box_l_y, STICK_BOX_SIZE, active_l ? c_gold : c_white);
        if (active_l) {
            if (strcmp(current_target, "leftstick") == 0) SurfaceDrawCircle(screen, box_l_x + (STICK_BOX_SIZE / 2), box_l_y + (STICK_BOX_SIZE / 2), 12, c_red); 
            else SurfaceDrawTargetPoint(screen, box_l_x, box_l_y, current_target, c_gold);
        }

        // Box Stick Destro
        int box_r_x = center_x + 46, box_r_y = center_y + 55;
        int active_r = (strcmp(current_target, "rightx") == 0 || strcmp(current_target, "righty") == 0 || strcmp(current_target, "rightstick") == 0);
        SurfaceDrawBoxOutline(screen, box_r_x, box_r_y, STICK_BOX_SIZE, active_r ? c_gold : c_white);
        if (active_r) {
            if (strcmp(current_target, "rightstick") == 0) SurfaceDrawCircle(screen, box_r_x + (STICK_BOX_SIZE / 2), box_r_y + (STICK_BOX_SIZE / 2), 12, c_red); 
            else SurfaceDrawTargetPoint(screen, box_r_x, box_r_y, current_target, c_gold);
        }

        GFX_flip(screen);
        SDL_Delay(16); 
    }

    if (joystick) SDL_JoystickClose(joystick);
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    QuitSettings(); PWR_quit(); GFX_quit();
    return 0;
}
