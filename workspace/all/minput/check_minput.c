#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <msettings.h>

#include "sdl.h"
#include "defines.h"
#include "api.h"
#include "utils.h"

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define STICK_BOX_SIZE 104  // FIXED: Ingrandito del 30% rispetto agli 80px originali
#define TOTAL_STEPS 21

const char* SDL_MAP_NAMES[] = {
    "dpup", "dpdown", "dpleft", "dpright",
    "leftx", "lefty", "rightx", "righty",
    "a", "b", "x", "y", "back", "start", 
    "leftshoulder", "rightshoulder", "lefttrigger", "righttrigger",
    "guide", "leftstick", "rightstick"
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
    // Linee di mira interne adattate alla nuova dimensione
    SurfaceDrawBox(dest, x + (size / 2) - 1, y + 4, 2, size - 8, SDL_MapRGB(dest->format, 35, 45, 75));
    SurfaceDrawBox(dest, x + 4, y + (size / 2) - 1, size - 8, 2, SDL_MapRGB(dest->format, 35, 45, 75));
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

    char line[512];
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

    char safe_model_name[64];
    const char* system_raw_ptr = PLAT_getModel();
    if (system_raw_ptr != NULL && strlen(system_raw_ptr) > 0) {
        snprintf(safe_model_name, sizeof(safe_model_name), "%s", system_raw_ptr);
    } else {
        snprintf(safe_model_name, sizeof(safe_model_name), "UnknownDevice");
    }

    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
        QuitSettings(); PWR_quit(); GFX_quit();
        return 1;
    }

    SDL_Joystick* joystick = NULL;
    SDL_Event event;

    while (!joystick) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_JOYDEVICEADDED) joystick = SDL_JoystickOpen(event.jdevice.which);
            if (event.type == SDL_QUIT) {
                SDL_QuitSubSystem(SDL_INIT_JOYSTICK); QuitSettings(); PWR_quit(); GFX_quit(); return 0;
            }
        }
        SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 10, 15, 30));
        GFX_flip(screen);
        SDL_Delay(50);
    }

    char guid_str[33];
    SDL_JoystickGUID guid = SDL_JoystickGetGUID(joystick);
    SDL_JoystickGetGUIDString(guid, guid_str, sizeof(guid_str));

    LocalControllerMap my_map;
    bool db_status = ParseCustomMappingFile("my_sdlcontroller.txt", guid_str, &my_map);

    int running = 1;
    char diagnostic_log[128];
    snprintf(diagnostic_log, sizeof(diagnostic_log), "Model: %s | DB: %s", 
             safe_model_name, db_status ? "PARSED MY_SDLCONTROLLER.TXT" : "NO GUID FOUND (RAW TEST MODE)");

    SDL_Color text_clr = { 255, 255, 255, 255 }; 
    Uint32 c_white = SDL_MapRGB(screen->format, 255, 255, 255);
    Uint32 c_gold  = SDL_MapRGB(screen->format, 241, 196, 15);
    Uint32 c_navy  = SDL_MapRGB(screen->format, 5, 10, 40);
    Uint32 c_red   = SDL_MapRGB(screen->format, 231, 76, 60);

    int center_x = screen->w / 2;
    int center_y = screen->h / 2;

    char live_pressed_buttons[256];

    while (running) {
        SDL_JoystickUpdate();

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_JOYDEVICEREMOVED) { running = 0; break; }
        }

        if (my_map.buttons[12] != -1 && my_map.buttons[13] != -1) {
            if (SDL_JoystickGetButton(joystick, my_map.buttons[12]) && SDL_JoystickGetButton(joystick, my_map.buttons[13])) {
                running = 0;
                break;
            }
        }

        strcpy(live_pressed_buttons, "Pressed: ");
        bool any_pressed = false;

        bool btn_dpup    = (my_map.buttons[0] != -1) ? SDL_JoystickGetButton(joystick, my_map.buttons[0]) : false;
        bool btn_dpdown  = (my_map.buttons[1] != -1) ? SDL_JoystickGetButton(joystick, my_map.buttons[1]) : false;
        bool btn_dpleft  = (my_map.buttons[2] != -1) ? SDL_JoystickGetButton(joystick, my_map.buttons[2]) : false;
        bool btn_dpright = (my_map.buttons[3] != -1) ? SDL_JoystickGetButton(joystick, my_map.buttons[3]) : false;

        if (my_map.hat_id != -1) {
            Uint8 hat_val = SDL_JoystickGetHat(joystick, my_map.hat_id);
            if (hat_val & my_map.hat_values[0]) btn_dpup = true;
            if (hat_val & my_map.hat_values[1]) btn_dpdown = true;
            if (hat_val & my_map.hat_values[2]) btn_dpleft = true;
            if (hat_val & my_map.hat_values[3]) btn_dpright = true;
        }

        if (btn_dpup)    { strcat(live_pressed_buttons, "dpup ");    any_pressed = true; }
        if (btn_dpdown)  { strcat(live_pressed_buttons, "dpdown ");  any_pressed = true; }
        if (btn_dpleft)  { strcat(live_pressed_buttons, "dpleft ");  any_pressed = true; }
        if (btn_dpright) { strcat(live_pressed_buttons, "dpright "); any_pressed = true; }

        int raw_lx = (my_map.axes[4] != -1) ? SDL_JoystickGetAxis(joystick, my_map.axes[4]) : 0;
        int raw_ly = (my_map.axes[5] != -1) ? SDL_JoystickGetAxis(joystick, my_map.axes[5]) : 0;
        int raw_rx = (my_map.axes[6] != -1) ? SDL_JoystickGetAxis(joystick, my_map.axes[6]) : 0;
        int raw_ry = (my_map.axes[7] != -1) ? SDL_JoystickGetAxis(joystick, my_map.axes[7]) : 0;

        int raw_l2 = (my_map.axes[16] != -1) ? SDL_JoystickGetAxis(joystick, my_map.axes[16]) : -32768;
        int raw_r2 = (my_map.axes[17] != -1) ? SDL_JoystickGetAxis(joystick, my_map.axes[17]) : -32768;
        bool act_l2 = (raw_l2 > 10000) || ((my_map.buttons[16] != -1) ? SDL_JoystickGetButton(joystick, my_map.buttons[16]) : false);
        bool act_r2 = (raw_r2 > 10000) || ((my_map.buttons[17] != -1) ? SDL_JoystickGetButton(joystick, my_map.buttons[17]) : false);

        for (int i = 8; i < TOTAL_STEPS; i++) {
            if (i == 16 && act_l2) { strcat(live_pressed_buttons, "lefttrigger ");  any_pressed = true; continue; }
            if (i == 17 && act_r2) { strcat(live_pressed_buttons, "righttrigger "); any_pressed = true; continue; }

            if (my_map.buttons[i] != -1 && SDL_JoystickGetButton(joystick, my_map.buttons[i])) {
                strcat(live_pressed_buttons, SDL_MAP_NAMES[i]);
                strcat(live_pressed_buttons, " ");
                any_pressed = true;
            }
        }

        if (!any_pressed) {
            strcpy(live_pressed_buttons, "Pressed: None");
        }

        // =================================================================
        // RENDERING GRAFICO
        // =================================================================
        SDL_FillRect(screen, NULL, c_navy); 

        SurfaceDrawText(screen, "=== HARDWARE JOYSTICK VERIFICATION ===", 40, 30, text_clr);
        SurfaceDrawText(screen, "Press SELECT + START together to return to menu", 40, 55, text_clr);
        SurfaceDrawText(screen, diagnostic_log, 40, screen->h - 35, text_clr);
        
        // CORRETTO: La scritta Pressed ora è perfettamente centrata in orizzontale (center_x - offset proporzionale) e in verticale (center_y)
        int text_width_offset = (strlen(live_pressed_buttons) * 7) / 2; // Stima della larghezza del testo pixel per centrarlo al millimetro
        SurfaceDrawText(screen, live_pressed_buttons, center_x - text_width_offset, center_y, text_clr);

        // --- INTERFACCIA TASTI FRONTALI (Distanziati per non sovrapporsi alla scritta centrale) ---
        bool act_a = (my_map.buttons[8] != -1)  && SDL_JoystickGetButton(joystick, my_map.buttons[8]);
        bool act_b = (my_map.buttons[9] != -1)  && SDL_JoystickGetButton(joystick, my_map.buttons[9]);
        bool act_x = (my_map.buttons[10] != -1) && SDL_JoystickGetButton(joystick, my_map.buttons[10]);
        bool act_y = (my_map.buttons[11] != -1) && SDL_JoystickGetButton(joystick, my_map.buttons[11]);

        SurfaceDrawCircle(screen, center_x + 175, center_y, 18, act_y ? c_gold : c_white);      // Y (Sinistra)
        SurfaceDrawCircle(screen, center_x + 225, center_y - 50, 18, act_x ? c_gold : c_white); // X (Alto)
        SurfaceDrawCircle(screen, center_x + 275, center_y, 18, act_a ? c_gold : c_white);      // A (Destra)
        SurfaceDrawCircle(screen, center_x + 225, center_y + 50, 18, act_b ? c_gold : c_white); // B (Basso)

        // Tasti di controllo centrali (Spostati leggermente in basso rispetto al testo fisso)
        SurfaceDrawBox(screen, center_x - 70, center_y + 175, 40, 15, ((my_map.buttons[12] != -1) && SDL_JoystickGetButton(joystick, my_map.buttons[12])) ? c_gold : c_white); // SELECT
        SurfaceDrawBox(screen, center_x + 30, center_y + 175, 40, 15, ((my_map.buttons[13] != -1) && SDL_JoystickGetButton(joystick, my_map.buttons[13])) ? c_gold : c_white); // START
        SurfaceDrawCircle(screen, center_x, center_y - 110, 12, ((my_map.buttons[18] != -1) && SDL_JoystickGetButton(joystick, my_map.buttons[18])) ? c_gold : c_white);       // MENU/GUIDE

        // Dorsali e Grilletti
        SurfaceDrawBox(screen, center_x - 160, center_y - 150, 50, 15, ((my_map.buttons[14] != -1) && SDL_JoystickGetButton(joystick, my_map.buttons[14])) ? c_gold : c_white); // L1
        SurfaceDrawBox(screen, center_x + 110, center_y - 150, 50, 15, ((my_map.buttons[15] != -1) && SDL_JoystickGetButton(joystick, my_map.buttons[15])) ? c_gold : c_white); // R1
        SurfaceDrawBox(screen, center_x - 100, center_y - 150, 50, 15, act_l2 ? c_gold : c_white); // L2
        SurfaceDrawBox(screen, center_x + 50, center_y - 150, 50, 15, act_r2 ? c_gold : c_white);  // R2

                // --- CROCE DIREZIONALE D-PAD (Modificata con cerchi speculari ad A/B/X/Y) ---
        int dpad_center_x = center_x - 225;
        int dpad_center_y = center_y;

        SurfaceDrawCircle(screen, dpad_center_x, dpad_center_y - 50, 18, btn_dpup    ? c_gold : c_white); // D-PAD SU (Alto)
        SurfaceDrawCircle(screen, dpad_center_x, dpad_center_y + 50, 18, btn_dpdown  ? c_gold : c_white); // D-PAD GIÙ (Basso)
        SurfaceDrawCircle(screen, dpad_center_x - 50, dpad_center_y, 18, btn_dpleft  ? c_gold : c_white); // D-PAD SINISTRA (Sinistra)
        SurfaceDrawCircle(screen, dpad_center_x + 50, dpad_center_y, 18, btn_dpright ? c_gold : c_white); // D-PAD DESTRA (Destra)


        // --- QUADRATI DI TRACCIAMENTO STICK (CORRETTI: Ingranditi a 104px e ri-centrati) ---
        // Stick Sinistro (L3)
        int box_l_x = center_x - 150, box_l_y = center_y + 55;
        bool act_l3 = (my_map.buttons[19] != -1) ? SDL_JoystickGetButton(joystick, my_map.buttons[19]) : false;
        SurfaceDrawBoxOutline(screen, box_l_x, box_l_y, STICK_BOX_SIZE, act_l3 ? c_red : c_white);
        
        int point_l_x = box_l_x + (STICK_BOX_SIZE / 2) + (int)((raw_lx / 32768.0f) * (STICK_BOX_SIZE / 2));
        int point_l_y = box_l_y + (STICK_BOX_SIZE / 2) + (int)((raw_ly / 32768.0f) * (STICK_BOX_SIZE / 2));
        SurfaceDrawCircle(screen, point_l_x, point_l_y, 6, c_gold); 

        // Stick Destro (R3)
        int box_r_x = center_x + 46, box_r_y = center_y + 55;
        bool act_r3 = (my_map.buttons[20] != -1) ? SDL_JoystickGetButton(joystick, my_map.buttons[20]) : false;
        SurfaceDrawBoxOutline(screen, box_r_x, box_r_y, STICK_BOX_SIZE, act_r3 ? c_red : c_white);
        int point_r_x = box_r_x + (STICK_BOX_SIZE / 2) + (int)((raw_rx / 32768.0f) * (STICK_BOX_SIZE / 2));
        int point_r_y = box_r_y + (STICK_BOX_SIZE / 2) + (int)((raw_ry / 32768.0f) * (STICK_BOX_SIZE / 2));
        SurfaceDrawCircle(screen, point_r_x, point_r_y, 6, c_gold);
        GFX_flip(screen);
        SDL_Delay(16);
    }
    SDL_JoystickClose(joystick);
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    QuitSettings();
    PWR_quit();
    GFX_quit();
    return 0;
}