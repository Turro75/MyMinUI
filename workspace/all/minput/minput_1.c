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
#define TOTAL_STEPS 21
#define MAX_SUPPORTED_PADS 5 

typedef enum {
    STATE_BOOT_CHECK,
    STATE_VERIFY,
    STATE_MENU,
    STATE_CALIBRATE
} AppState;

typedef enum {
    MAP_DPUP = 0,
    MAP_DPDOWN,
    MAP_DPLEFT,
    MAP_DPRIGHT,
    MAP_LEFTX,
    MAP_LEFTY,
    MAP_RIGHTX,
    MAP_RIGHTY,
    MAP_A,
    MAP_B,
    MAP_X,
    MAP_Y,
    MAP_BACK,
    MAP_START,
    MAP_LEFTSHOULDER,
    MAP_RIGHTSHOULDER,
    MAP_LEFTTRIGGER,
    MAP_RIGHTTRIGGER,
    MAP_GUIDE,
    MAP_LEFTSTICK,
    MAP_RIGHTSTICK
} MapIndex;

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
    bool is_mapped;           
    char custom_name[128];    
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
// BLOCCO 2: Parser Manuale e Salvataggio Atomico con Auto-Clean dei Duplicati (FISSATO)
// FIXED: Cambiata la firma in char lines[TOTAL_STEPS][16] per allinearla al 100% con la dichiarazione nel main
void SaveMappingToFile(const char* filename, const char* guid, const char* name_str, char lines[TOTAL_STEPS][16]) {
    // 1. Fase di Pulizia: Rimuove le vecchie istanze dello stesso GUID per evitare letture fantasma
    FILE* file_in = fopen(filename, "r");
    FILE* file_tmp = fopen("my_sdlcontroller.tmp", "w");
    
    if (file_in && file_tmp) {
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), file_in)) {
            // Se la riga NON contiene il GUID del controller che stiamo salvando, la copia nel file temporaneo
            if (strstr(buffer, guid) == NULL) {
                fputs(buffer, file_tmp);
            }
        }
        fclose(file_in);
        fclose(file_tmp);
        rename("my_sdlcontroller.tmp", filename); // Sostituisce il vecchio database con quello pulito
    } else {
        if (file_in) fclose(file_in);
        if (file_tmp) fclose(file_tmp);
    }

    // 2. Fase di Scrittura: Scrive la nuova calibrazione pulita e aggiornata
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
    fflush(file); 
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

    char line[1024]; 
    bool found = false;

    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, target_guid) != NULL) {
            found = true;
            out_map->is_mapped = true;
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
                        if (*val == 'b') {
                            out_map->buttons[step_idx] = atoi(val + 1);
                        } else if (*val == 'a') {
                            out_map->axes[step_idx] = atoi(val + 1);
                        } else if (*val == 'h') {
                            out_map->hat_id = *(val + 1) - '0';
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

bool LoadControllerFromDatabases(const char* guid, LocalControllerMap* out_map) {
    out_map->is_mapped = false;
    if (ParseCustomMappingFile("my_sdlcontroller.txt", guid, out_map)) return true;
    if (ParseCustomMappingFile("gamecontrollerdb.txt", guid, out_map)) return true;
    return false;
}



int main(int argc, char* argv[]) {
    InitSettings();
    SDL_Surface* screen = GFX_init(MODE_MAIN); 
    PWR_init();
    PWR_setCPUSpeed(CPU_SPEED_MENU);

    char safe_model_name[64]; 
    const char* system_raw_ptr = PLAT_getModel();
    if (system_raw_ptr != NULL && strlen(system_raw_ptr) > 0) {
        snprintf(safe_model_name, sizeof(safe_model_name), "MyMinUI %s", system_raw_ptr);
    } else {
        snprintf(safe_model_name, sizeof(safe_model_name), "MyMinUI UnknownDevice");
    }

    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
        printf("ERROR: SDL Joystick subsystem initialization failed!\n"); fflush(stdout);
        QuitSettings(); PWR_quit(); GFX_quit(); return 1;
    }

    // Sincronizzazione hardware a freddo per MinUI
    int init_wait = 0;
    while (SDL_NumJoysticks() == 0 && init_wait < 15) {
        SDL_PumpEvents(); SDL_Delay(50); init_wait++;
    }

    // --- LOGICA DI SICUREZZA E CONTROLLO CONNESSONI SU STDOUT ---
    int total_connected_pads = SDL_NumJoysticks();
    printf("--- SYSTEM JOYSTICK SCANNER ---\n");
    printf("Total gamepads detected: %d\n", total_connected_pads);
    fflush(stdout);

    // CRITICO: Se non rileva alcun controller (0 collegati), inutile calibrare. Stampa ed esce pulito.
    if (total_connected_pads == 0) {
        printf("CRITICAL ERROR: No gamepads detected on this device. Terminating app.\n");
        fflush(stdout);
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK); QuitSettings(); PWR_quit(); GFX_quit();
        return 0; 
    }

    LocalControllerMap pads[MAX_SUPPORTED_PADS];
    SDL_Joystick* open_joysticks[MAX_SUPPORTED_PADS];
    char pad_guids[MAX_SUPPORTED_PADS][33];

    for (int i = 0; i < MAX_SUPPORTED_PADS; i++) {
        open_joysticks[i] = NULL;
        pads[i].is_mapped = false;
        strcpy(pads[i].custom_name, "Disconnected");
        strcpy(pad_guids[i], "");
    }

    if (total_connected_pads > MAX_SUPPORTED_PADS) total_connected_pads = MAX_SUPPORTED_PADS;

    // Apertura dei controller ed estrazione dell'elenco hardware dettagliato su stdout
    for (int i = 0; i < total_connected_pads; i++) {
        open_joysticks[i] = SDL_JoystickOpen(i);
        if (open_joysticks[i]) {
            SDL_JoystickGUID j_guid = SDL_JoystickGetGUID(open_joysticks[i]);
            SDL_JoystickGetGUIDString(j_guid, pad_guids[i], 33);
            
            const char* hw_name = SDL_JoystickName(open_joysticks[i]);
            if (hw_name && strlen(hw_name) > 0) snprintf(pads[i].custom_name, 128, "%s", hw_name);
            else snprintf(pads[i].custom_name, 128, "Pad %d (%s)", i, (i == 0) ? "INTERNAL" : "EXTERNAL");

            // STAMPA LOG SU STDOUT: Mostra l'indice, il nome hardware e il GUID hex reale di ogni pad trovato
            printf("  -> Pad #%d: Name=\"%s\" | GUID=%s\n", i, pads[i].custom_name, pad_guids[i]);
            fflush(stdout);

            LoadControllerFromDatabases(pad_guids[i], &pads[i]);
        }
    }
    printf("-------------------------------\n"); fflush(stdout);

    AppState state = STATE_VERIFY;
    int active_pad_idx = 0; 

    // REGOLE DI BOOT A CASCATA:
    // Se il controller interno (0) NON è riconosciuto dal DB, va in mappatura forzata diretta ed esce al termine
    if (!pads[0].is_mapped) {
        state = STATE_CALIBRATE;
        active_pad_idx = 0;
        printf("Boot state rule: Internal controller not recognized. Forcing direct calibration map.\n");
        fflush(stdout);
    } 
    // Se il controller interno è riconosciuto, l'app parte in modalità verifica sul pad interno
    else {
        state = STATE_VERIFY;
        active_pad_idx = 0; 
    }

    char final_mappings[TOTAL_STEPS][16];
    int current_step = 0;
    uint32_t step_start_time = 0;
    bool waiting_for_release = false;
    int primary_active_input_id = -1;

    bool hkey_select_held = false;
    bool hkey_start_held = false;
    bool hkey_menu_held = false;
    
    int menu_selected_item = 0;
    char live_pressed_buttons[256];
    strcpy(live_pressed_buttons, "Pressed: None");
    int running = 1;

    SDL_Color text_clr = { 255, 255, 255, 255 }; 
    Uint32 c_white = SDL_MapRGB(screen->format, 255, 255, 255);
    Uint32 c_gold  = SDL_MapRGB(screen->format, 241, 196, 15);
    Uint32 c_navy  = SDL_MapRGB(screen->format, 5, 10, 40);
    Uint32 c_red   = SDL_MapRGB(screen->format, 231, 76, 60);

    int center_x = screen->w / 2;
    int center_y = screen->h / 2;

    while (running) {
        SDL_JoystickUpdate();
        SDL_Event event;

        if (state == STATE_CALIBRATE) {
            if (step_start_time == 0) {
                step_start_time = SDL_GetTicks();
                for (int i = 0; i < TOTAL_STEPS; i++) final_mappings[i][0] = '\0';
                current_step = 0;
                waiting_for_release = false;
                primary_active_input_id = -1;
            }

            uint32_t current_time = SDL_GetTicks();
            uint32_t time_elapsed = current_time - step_start_time;

            if (time_elapsed >= 5000) {
                current_step++; step_start_time = SDL_GetTicks();
                waiting_for_release = false; primary_active_input_id = -1;
                SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
            }

            if (current_step >= TOTAL_STEPS) {
                SaveMappingToFile("my_sdlcontroller.txt", pad_guids[active_pad_idx], pads[active_pad_idx].custom_name, final_mappings);
                LoadControllerFromDatabases(pad_guids[active_pad_idx], &pads[active_pad_idx]);
                step_start_time = 0;
                
                if (active_pad_idx == 0) {
                    running = 0; break;
                } else {
                    state = STATE_VERIFY; 
                    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
                    continue;
                }
            }

            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) running = 0;

                if (waiting_for_release && primary_active_input_id != -1) {
                    bool released = false;
                    if (event.type == SDL_JOYBUTTONUP && event.jbutton.button == primary_active_input_id) released = true;
                    else if (event.type == SDL_JOYHATMOTION && event.jhat.hat == primary_active_input_id && event.jhat.value == SDL_HAT_CENTERED) released = true;
                    else if (event.type == SDL_JOYAXISMOTION && event.jaxis.axis == primary_active_input_id && abs(event.jaxis.value) < AXIS_RELEASE_THRESHOLD) released = true;

                    if (released) {
                        waiting_for_release = false; primary_active_input_id = -1;
                        current_step++; step_start_time = SDL_GetTicks();
                        
                        if (current_step >= TOTAL_STEPS) {
                            SaveMappingToFile("my_sdlcontroller.txt", pad_guids[active_pad_idx], pads[active_pad_idx].custom_name, final_mappings);
                            LoadControllerFromDatabases(pad_guids[active_pad_idx], &pads[active_pad_idx]);
                            step_start_time = 0;
                            if (active_pad_idx == 0) { running = 0; break; }
                            else { state = STATE_VERIFY; SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT); break; }
                        }
                    }
                    continue;
                }

                if (!waiting_for_release) {
                    if (current_step >= 4 && current_step <= 7) { 
                        if (event.type == SDL_JOYAXISMOTION && abs(event.jaxis.value) > AXIS_TRIGGER_THRESHOLD && event.jaxis.which == active_pad_idx) {
                            int det_axis = event.jaxis.axis;
                            sprintf(final_mappings[current_step], "a%d", det_axis);
                            primary_active_input_id = det_axis; waiting_for_release = true;
                        }
                    } 
                    else {
                        if (event.type == SDL_JOYBUTTONDOWN && event.jbutton.which == active_pad_idx) {
                            int det_btn = event.jbutton.button;
                            sprintf(final_mappings[current_step], "b%d", det_btn);
                            primary_active_input_id = det_btn; waiting_for_release = true;
                        }
                        else if (event.type == SDL_JOYHATMOTION && event.jhat.value != SDL_HAT_CENTERED && current_step < 4 && event.jhat.which == active_pad_idx) {
                            sprintf(final_mappings[current_step], "h%d.%d", event.jhat.hat, event.jhat.value);
                            primary_active_input_id = event.jhat.hat; waiting_for_release = true;
                        }
                    }
                }
            }
            if (!running) break;
        }
        else {
            LocalControllerMap* m = &pads[active_pad_idx];

            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) running = 0;

                if (event.type == SDL_JOYBUTTONDOWN && event.jbutton.which == active_pad_idx) {
                    int btn = event.jbutton.button;

                    // FISSATO: Utilizzo delle costanti enum per estrarre gli ID hardware reali dal database
                    if (m->is_mapped) {
                        if (btn == m->buttons[MAP_BACK])  hkey_select_held = true; 
                        if (btn == m->buttons[MAP_START]) hkey_start_held = true;  
                        if (btn == m->buttons[MAP_GUIDE]) hkey_menu_held = true;   
                    } else {
                        // Fallback hardware di emergenza
                        if (btn == 4 || btn == 11) hkey_select_held = true;
                        if (btn == 5 || btn == 12) hkey_start_held = true;
                        if (btn == 2 || btn == 16) hkey_menu_held = true;
                    }

                    // --- NAVIGAZIONE INTERNA DEL MENU OVERLAY ---
                    if (state == STATE_MENU) {
                        int total_menu_items = total_connected_pads + 2; 
                        
                        if (m->is_mapped) {
                            // Navigazione Menu leggetibile con D-Pad logico mappato
                            if (btn == m->buttons[MAP_DPDOWN]) menu_selected_item = (menu_selected_item + 1) % total_menu_items; 
                            if (btn == m->buttons[MAP_DPUP])   menu_selected_item = (menu_selected_item - 1 + total_menu_items) % total_menu_items; 
                            
                            // Conferma selezione del Menu mappata sul Tasto A logico
                            if (btn == m->buttons[MAP_A]) { 
                                if (menu_selected_item < total_connected_pads) {
                                    active_pad_idx = menu_selected_item;
                                    state = STATE_CALIBRATE; step_start_time = 0;
                                } else if (menu_selected_item == total_connected_pads) {
                                    state = STATE_VERIFY;
                                } else { running = 0; }
                            }
                        } else {
                            // Fallback menu grezzo
                            if (btn == 0 || btn == 1) menu_selected_item = (menu_selected_item + 1) % total_menu_items;
                            if (btn == 10 || btn == 3) {
                                if (menu_selected_item < total_connected_pads) {
                                    active_pad_idx = menu_selected_item; state = STATE_CALIBRATE; step_start_time = 0;
                                } else if (menu_selected_item == total_connected_pads) { state = STATE_VERIFY; }
                                else { running = 0; }
                            }
                        }
                    }
                }

                if (event.type == SDL_JOYBUTTONUP && event.jbutton.which == active_pad_idx) {
                    int btn = event.jbutton.button;
                    if (m->is_mapped) {
                        if (btn == m->buttons[MAP_BACK])  hkey_select_held = false;
                        if (btn == m->buttons[MAP_START]) hkey_start_held = false;
                        if (btn == m->buttons[MAP_GUIDE]) hkey_menu_held = false;
                    } else {
                        if (btn == 4 || btn == 11) hkey_select_held = false;
                        if (btn == 5 || btn == 12) hkey_start_held = false;
                        if (btn == 2 || btn == 16) hkey_menu_held = false;
                    }
                }
                
                if (state == STATE_MENU && event.type == SDL_JOYHATMOTION && event.jhat.which == active_pad_idx) {
                    int total_menu_items = total_connected_pads + 2;
                    if (event.jhat.value & SDL_HAT_DOWN) menu_selected_item = (menu_selected_item + 1) % total_menu_items;
                    if (event.jhat.value & SDL_HAT_UP) menu_selected_item = (menu_selected_item - 1 + total_menu_items) % total_menu_items;
                }
            }

            // VALUTAZIONE DELLE COMBINAZIONI LOGICHE UNIFICATE
            if (hkey_menu_held && hkey_start_held) { running = 0; break; }

            if (hkey_menu_held && hkey_select_held && state == STATE_VERIFY) {
                hkey_menu_held = false; hkey_select_held = false; 
                if (total_connected_pads > 1) {
                    int next_pad = active_pad_idx;
                    for (int i = 0; i < total_connected_pads; i++) {
                        next_pad = (next_pad + 1) % total_connected_pads;
                        if (pads[next_pad].is_mapped) { active_pad_idx = next_pad; break; }
                    }
                }
                SDL_Delay(250); 
            }

            if (hkey_select_held && hkey_start_held) {
                hkey_select_held = false; hkey_start_held = false;
                if (state == STATE_VERIFY) { state = STATE_MENU; menu_selected_item = 0; } 
                else if (state == STATE_MENU) { state = STATE_VERIFY; }
                SDL_Delay(250);
            }
        }

        // =================================================================
        // PIPELINE REALE DI RENDERING GRAFICO SULLA SURFACE
        // =================================================================
        SDL_FillRect(screen, NULL, c_navy);

        if (state == STATE_CALIBRATE) {
            const char* current_target = SDL_MAP_NAMES[current_step];
            uint32_t time_elapsed = SDL_GetTicks() - step_start_time;
            int bar_width = (int)(((5000 - time_elapsed) / 5000.0f) * screen->w);
            SurfaceDrawBox(screen, 0, 0, bar_width, 12, c_red);

            SurfaceDrawText(screen, "=== INTERACTIVE CONTROLLER MAPPING ===", 40, 30, text_clr);
            char setup_info[128];
            snprintf(setup_info, sizeof(setup_info), "Mapping Pad %d: %s", active_pad_idx, pads[active_pad_idx].custom_name);
            SurfaceDrawText(screen, setup_info, 40, screen->h - 55, text_clr);
            
            int text_width_offset = (strlen(STEP_PROMPTS[current_step]) * 7) / 2;
            SurfaceDrawText(screen, STEP_PROMPTS[current_step], center_x - text_width_offset, center_y, text_clr);

            int d_cx = center_x - 225;
            SurfaceDrawCircle(screen, d_cx, center_y - 50, 18, (strcmp(current_target, "dpup") == 0) ? c_gold : c_white);
            SurfaceDrawCircle(screen, d_cx, center_y + 50, 18, (strcmp(current_target, "dpdown") == 0) ? c_gold : c_white);
            SurfaceDrawCircle(screen, d_cx - 50, center_y, 18, (strcmp(current_target, "dpleft") == 0) ? c_gold : c_white);
            SurfaceDrawCircle(screen, d_cx + 50, center_y, 18, (strcmp(current_target, "dpright") == 0) ? c_gold : c_white);

            SurfaceDrawCircle(screen, center_x + 175, center_y, 18, (strcmp(current_target, "y") == 0) ? c_gold : c_white);      
            SurfaceDrawCircle(screen, center_x + 225, center_y - 50, 18, (strcmp(current_target, "x") == 0) ? c_gold : c_white); 
            SurfaceDrawCircle(screen, center_x + 275, center_y, 18, (strcmp(current_target, "a") == 0) ? c_gold : c_white);      
            SurfaceDrawCircle(screen, center_x + 225, center_y + 50, 18, (strcmp(current_target, "b") == 0) ? c_gold : c_white); 

            SurfaceDrawBox(screen, center_x - 70, center_y + 175, 40, 15, (strcmp(current_target, "back") == 0) ? c_gold : c_white);
            SurfaceDrawBox(screen, center_x + 30, center_y + 175, 40, 15, (strcmp(current_target, "start") == 0) ? c_gold : c_white);
            SurfaceDrawCircle(screen, center_x, center_y - 110, 12, (strcmp(current_target, "guide") == 0) ? c_gold : c_white);

            SurfaceDrawBox(screen, center_x - 160, center_y - 170, 50, 15, (strcmp(current_target, "leftshoulder") == 0) ? c_gold : c_white);
            SurfaceDrawBox(screen, center_x + 110, center_y - 170, 50, 15, (strcmp(current_target, "rightshoulder") == 0) ? c_gold : c_white);
            SurfaceDrawBox(screen, center_x - 100, center_y - 170, 50, 15, (strcmp(current_target, "lefttrigger") == 0) ? c_gold : c_white);
            SurfaceDrawBox(screen, center_x + 50, center_y - 170, 50, 15, (strcmp(current_target, "righttrigger") == 0) ? c_gold : c_white);

            int b_l_x = center_x - 150, b_l_y = center_y + 55;
            int act_l = (strcmp(current_target, "leftx") == 0 || strcmp(current_target, "lefty") == 0 || strcmp(current_target, "leftstick") == 0);
            // FIXED: Sostituito active_l con la variabile corretta act_l per combaciare con l'inizializzazione dello stack
            SurfaceDrawBoxOutline(screen, b_l_x, b_l_y, STICK_BOX_SIZE, act_l ? c_gold : c_white);
            if (act_l) {
                if (strcmp(current_target, "leftstick") == 0) SurfaceDrawCircle(screen, b_l_x + (STICK_BOX_SIZE/2), b_l_y + (STICK_BOX_SIZE/2), 12, c_red);
                else SurfaceDrawTargetPoint(screen, b_l_x, b_l_y, current_target, c_gold);
            }

            int b_r_x = center_x + 46, b_r_y = center_y + 55;
            int act_r = (strcmp(current_target, "rightx") == 0 || strcmp(current_target, "righty") == 0 || strcmp(current_target, "rightstick") == 0);
            // FIXED: Sostituito active_r con la variabile corretta act_r per allineare l'assegnazione
            SurfaceDrawBoxOutline(screen, b_r_x, b_r_y, STICK_BOX_SIZE, act_r ? c_gold : c_white);
            if (act_r) {
                if (strcmp(current_target, "rightstick") == 0) SurfaceDrawCircle(screen, b_r_x + (STICK_BOX_SIZE/2), b_r_y + (STICK_BOX_SIZE/2), 12, c_red);
                else SurfaceDrawTargetPoint(screen, b_r_x, b_r_y, current_target, c_gold);
            }
        } 
        else {
            // --- RENDERING DELLA COMPOSIZIONE MODALITÀ TESTER (VERIFY) ---
            SurfaceDrawText(screen, "=== CONTROLLER VISUAL VERIFICATION SECURE ===", 40, 30, text_clr);
            SurfaceDrawText(screen, "START+SELECT: Open Options Menu | MENU+START: Exit Application", 40, 55, text_clr);
            
            char pad_info[256];
            snprintf(pad_info, sizeof(pad_info), "Testing Active Pad %d: %s", active_pad_idx, pads[active_pad_idx].custom_name);
            SurfaceDrawText(screen, pad_info, 40, screen->h - 35, text_clr);

            LocalControllerMap* m = &pads[active_pad_idx];
            SDL_Joystick* current_joy = open_joysticks[active_pad_idx];

            // 1. Lettura D-PAD e Assi Analogici tramite Enum
            bool d_u = (m->buttons[MAP_DPUP] != -1)    ? SDL_JoystickGetButton(current_joy, m->buttons[MAP_DPUP]) : false;
            bool d_d = (m->buttons[MAP_DPDOWN] != -1)  ? SDL_JoystickGetButton(current_joy, m->buttons[MAP_DPDOWN]) : false;
            bool d_l = (m->buttons[MAP_DPLEFT] != -1)  ? SDL_JoystickGetButton(current_joy, m->buttons[MAP_DPLEFT]) : false;
            bool d_r = (m->buttons[MAP_DPRIGHT] != -1) ? SDL_JoystickGetButton(current_joy, m->buttons[MAP_DPRIGHT]) : false;

            if (m->hat_id != -1) {
                Uint8 hv = SDL_JoystickGetHat(current_joy, m->hat_id);
                if (hv & SDL_HAT_UP) d_u = true;    if (hv & SDL_HAT_DOWN) d_d = true;
                if (hv & SDL_HAT_LEFT) d_l = true;  if (hv & SDL_HAT_RIGHT) d_r = true;
            }

            int sx_l = (m->axes[MAP_LEFTX] != -1)  ? SDL_JoystickGetAxis(current_joy, m->axes[MAP_LEFTX]) : 0;
            int sy_l = (m->axes[MAP_LEFTY] != -1)  ? SDL_JoystickGetAxis(current_joy, m->axes[MAP_LEFTY]) : 0;
            int sx_r = (m->axes[MAP_RIGHTX] != -1) ? SDL_JoystickGetAxis(current_joy, m->axes[MAP_RIGHTX]) : 0;
            int sy_r = (m->axes[MAP_RIGHTY] != -1) ? SDL_JoystickGetAxis(current_joy, m->axes[MAP_RIGHTY]) : 0;

            // 2. Lettura Grilletti L2/R2 (Analogici con Fallback Digitale)
            int ax_l2 = (m->axes[MAP_LEFTTRIGGER] != -1)  ? SDL_JoystickGetAxis(current_joy, m->axes[MAP_LEFTTRIGGER]) : -32768;
            int ax_r2 = (m->axes[MAP_RIGHTTRIGGER] != -1) ? SDL_JoystickGetAxis(current_joy, m->axes[MAP_RIGHTTRIGGER]) : -32768;
            bool l2_a = (ax_l2 > 10000) || ((m->buttons[MAP_LEFTTRIGGER] != -1)  ? SDL_JoystickGetButton(current_joy, m->buttons[MAP_LEFTTRIGGER]) : false);
            bool r2_a = (ax_r2 > 10000) || ((m->buttons[MAP_RIGHTTRIGGER] != -1) ? SDL_JoystickGetButton(current_joy, m->buttons[MAP_RIGHTTRIGGER]) : false);

            // 3. Costruzione della stringa Pressed ESPLICITA (Niente più cicli for o alias di memoria sfasati)
            strcpy(live_pressed_buttons, "Pressed: "); bool any_p = false;
            if (d_u) { strcat(live_pressed_buttons, "dpup "); any_p = true; }
            if (d_d) { strcat(live_pressed_buttons, "dpdown "); any_p = true; }
            if (d_l) { strcat(live_pressed_buttons, "dpleft "); any_p = true; }
            if (d_r) { strcat(live_pressed_buttons, "dpright "); any_p = true; }

            // Verifica esplicita bottone per bottone usando gli indici nativi dell'enum
            if (m->buttons[MAP_A] != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_A])) { strcat(live_pressed_buttons, "a "); any_p = true; }
            if (m->buttons[MAP_B] != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_B])) { strcat(live_pressed_buttons, "b "); any_p = true; }
            if (m->buttons[MAP_X] != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_X])) { strcat(live_pressed_buttons, "x "); any_p = true; }
            if (m->buttons[MAP_Y] != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_Y])) { strcat(live_pressed_buttons, "y "); any_p = true; }
            if (m->buttons[MAP_BACK] != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_BACK])) { strcat(live_pressed_buttons, "back "); any_p = true; }
            if (m->buttons[MAP_START] != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_START])) { strcat(live_pressed_buttons, "start "); any_p = true; }
            if (m->buttons[MAP_LEFTSHOULDER] != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_LEFTSHOULDER])) { strcat(live_pressed_buttons, "leftshoulder "); any_p = true; }
            if (m->buttons[MAP_RIGHTSHOULDER] != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_RIGHTSHOULDER])) { strcat(live_pressed_buttons, "rightshoulder "); any_p = true; }
            if (l2_a) { strcat(live_pressed_buttons, "lefttrigger "); any_p = true; }
            if (r2_a) { strcat(live_pressed_buttons, "righttrigger "); any_p = true; }
            if (m->buttons[MAP_GUIDE] != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_GUIDE])) { strcat(live_pressed_buttons, "guide "); any_p = true; }
            if (m->buttons[MAP_LEFTSTICK] != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_LEFTSTICK])) { strcat(live_pressed_buttons, "leftstick "); any_p = true; }
            if (m->buttons[MAP_RIGHTSTICK] != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_RIGHTSTICK])) { strcat(live_pressed_buttons, "rightstick "); any_p = true; }

            if (!any_p) strcpy(live_pressed_buttons, "Pressed: None");
            int tw = (strlen(live_pressed_buttons) * 7) / 2;
            SurfaceDrawText(screen, live_pressed_buttons, center_x - tw, center_y, text_clr);

            // 4. RENDERING GRAFICO DEI CERCHI (Isolati tramite l'enum al 100%)
            int d_x = center_x - 225;
            SurfaceDrawCircle(screen, d_x, center_y - 50, 18, d_u ? c_gold : c_white);
            SurfaceDrawCircle(screen, d_x, center_y + 50, 18, d_d ? c_gold : c_white);
            SurfaceDrawCircle(screen, d_x - 50, center_y, 18, d_l ? c_gold : c_white);
            SurfaceDrawCircle(screen, d_x + 50, center_y, 18, d_r ? c_gold : c_white);

            SurfaceDrawCircle(screen, center_x + 175, center_y, 18, ((m->buttons[MAP_Y]) != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_Y])) ? c_gold : c_white); 
            SurfaceDrawCircle(screen, center_x + 225, center_y - 50, 18, ((m->buttons[MAP_X]) != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_X])) ? c_gold : c_white); 
            SurfaceDrawCircle(screen, center_x + 275, center_y, 18, ((m->buttons[MAP_A]) != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_A])) ? c_gold : c_white);   
            SurfaceDrawCircle(screen, center_x + 225, center_y + 50, 18, ((m->buttons[MAP_B]) != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_B])) ? c_gold : c_white);   

            SurfaceDrawBox(screen, center_x - 70, center_y + 175, 40, 15, ((m->buttons[MAP_BACK])  != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_BACK])) ? c_gold : c_white); 
            SurfaceDrawBox(screen, center_x + 30, center_y + 175, 40, 15, ((m->buttons[MAP_START]) != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_START])) ? c_gold : c_white); 
            SurfaceDrawCircle(screen, center_x, center_y - 110, 12, ((m->buttons[MAP_GUIDE])  != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_GUIDE])) ? c_gold : c_white);      

            SurfaceDrawBox(screen, center_x - 160, center_y - 150, 50, 15, ((m->buttons[MAP_LEFTSHOULDER])  != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_LEFTSHOULDER])) ? c_gold : c_white); 
            SurfaceDrawBox(screen, center_x + 110, center_y - 150, 50, 15, ((m->buttons[MAP_RIGHTSHOULDER]) != -1 && SDL_JoystickGetButton(current_joy, m->buttons[MAP_RIGHTSHOULDER])) ? c_gold : c_white); 
            SurfaceDrawBox(screen, center_x - 100, center_y - 150, 50, 15, l2_a ? c_gold : c_white); 
            SurfaceDrawBox(screen, center_x + 50, center_y - 150, 50, 15, r2_a ? c_gold : c_white);  

            int bx_l = center_x - 150, bx_l_y = center_y + 55;
            bool l3_a = (m->buttons[MAP_LEFTSTICK] != -1) ? SDL_JoystickGetButton(current_joy, m->buttons[MAP_LEFTSTICK]) : false;
            SurfaceDrawBoxOutline(screen, bx_l, bx_l_y, STICK_BOX_SIZE, l3_a ? c_red : c_white);
            SurfaceDrawCircle(screen, bx_l + (STICK_BOX_SIZE / 2) + (int)((sx_l / 32768.0f) * (STICK_BOX_SIZE / 2)), bx_l_y + (STICK_BOX_SIZE / 2) + (int)((sy_l / 32768.0f) * (STICK_BOX_SIZE / 2)), 6, c_gold); 

            int bx_r = center_x + 46, bx_r_y = center_y + 55;
            bool r3_a = (m->buttons[MAP_RIGHTSTICK] != -1) ? SDL_JoystickGetButton(current_joy, m->buttons[MAP_RIGHTSTICK]) : false;
            SurfaceDrawBoxOutline(screen, bx_r, bx_r_y, STICK_BOX_SIZE, r3_a ? c_red : c_white);
            SurfaceDrawCircle(screen, bx_r + (STICK_BOX_SIZE / 2) + (int)((sx_r / 32768.0f) * (STICK_BOX_SIZE / 2)), bx_r_y + (STICK_BOX_SIZE / 2) + (int)((sy_r / 32768.0f) * (STICK_BOX_SIZE / 2)), 6, c_gold); 

            // Menu Overlay
            if (state == STATE_MENU) {
                SurfaceDrawBox(screen, center_x - 180, center_y - 120, 360, 240, SDL_MapRGB(screen->format, 15, 25, 55));
                SurfaceDrawBoxOutline(screen, center_x - 180, center_y - 120, 360, c_white); 
                SurfaceDrawText(screen, "--- MANUAL OPTIONS MENU ---", center_x - 100, center_y - 100, text_clr);
                
                int item_y = center_y - 60;
                for (int i = 0; i < total_connected_pads; i++) {
                    char menu_txt[256];
                    snprintf(menu_txt, sizeof(menu_txt), "%s Calibrate Pad %d: %-15.15s", (menu_selected_item == i) ? ">" : " ", i, pads[i].custom_name);
                    SurfaceDrawText(screen, menu_txt, center_x - 150, item_y, text_clr);
                    item_y += 25;
                }
                char txt_ret[256]; snprintf(txt_ret, sizeof(txt_ret), "%s Return to Tester Mode", (menu_selected_item == total_connected_pads) ? ">" : " ");
                SurfaceDrawText(screen, txt_ret, center_x - 150, item_y, text_clr);
                item_y += 25;
                char txt_exit[256];
                snprintf(txt_exit, sizeof(txt_exit), "%s Exit to MinUI Front-End", (menu_selected_item == (total_connected_pads + 1)) ? ">" : " ");
                SurfaceDrawText(screen, txt_exit, center_x - 150, item_y, text_clr);
                }
            }
            GFX_flip(screen);
            SDL_Delay(16);
        }
        for (int i = 0;i < MAX_SUPPORTED_PADS;i++) {
            if (open_joysticks[i]) SDL_JoystickClose(open_joysticks[i]);
        }
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    QuitSettings();
    PWR_quit();
    GFX_quit();
    return 0;
}