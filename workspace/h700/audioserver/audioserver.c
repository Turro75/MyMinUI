#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <alsa/asoundlib.h>
#include <signal.h>

#define FIFO_PATH "/tmp/dsp"
#define BUFFER_SIZE 1024 

#define SAMPLING_FREQ     48000
#define LATENCY_TARGET_MS 20
#define BUSY_THRESHOLD    ((SAMPLING_FREQ * LATENCY_TARGET_MS) / 1000) 

// 🎯 STATE TRACKING VARIABLES
static volatile sig_atomic_t route_changed = 0;
static volatile sig_atomic_t target_output_hdmi = 0; // 0 = Speaker, 1 = HDMI
static int current_output_hdmi = 0;                  // 0 = Speaker, 1 = HDMI (Tracks actual hardware state)

void handle_audio_route_signal(int sig) {
    if (sig == SIGUSR1) {       
        target_output_hdmi = 1;
        route_changed = 1;
    } else if (sig == SIGUSR2) { 
        target_output_hdmi = 0;
        route_changed = 1;
    }
}

int configure_alsa_device(snd_pcm_t **handle, const char *dev_name) {
    if (*handle != NULL) {
        snd_pcm_close(*handle);
        *handle = NULL;
    }
    
    printf("[AUDIO SERVER] Opening ALSA device target: %s\n", dev_name);
    if (snd_pcm_open(handle, dev_name, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        fprintf(stderr, "[AUDIO SERVER ERROR] Failed to open device: %s\n", dev_name);
        return -1;
    }
    
    if (snd_pcm_set_params(*handle, SND_PCM_FORMAT_S16_LE, 
                           SND_PCM_ACCESS_RW_INTERLEAVED, 2, SAMPLING_FREQ, 1, 40000) < 0) {
        fprintf(stderr, "[AUDIO SERVER ERROR] Hardware parameters initialization failed on %s\n", dev_name);
        snd_pcm_close(*handle);
        *handle = NULL;
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    snd_pcm_t *alsa_handle = NULL;
    char buffer[BUFFER_SIZE];
    
    signal(SIGUSR1, handle_audio_route_signal);
    signal(SIGUSR2, handle_audio_route_signal);

    // Initial state: Booting into Speaker mode (Card 0)
    if (configure_alsa_device(&alsa_handle, "default") < 0) {
        return 1;
    }
    current_output_hdmi = 0; 
    
    printf("[AUDIO SERVER] Initialized (Jitter-Free State-Aware Backpressure Mode)\n");
    unlink(FIFO_PATH);
    if (mkfifo(FIFO_PATH, 0666) < 0) {
        snd_pcm_close(alsa_handle);
        return 1;
    }

    int fifo_fd = open(FIFO_PATH, O_RDWR);
    if (fifo_fd < 0) {
        snd_pcm_close(alsa_handle);
        return 1;
    }

    while (1) {
        // 🎯 THE FIX: Check route only if signaled AND the requested target differs from actual current state
        if (route_changed) {
            route_changed = 0; // Clear signal flag immediately
            
            if (target_output_hdmi != current_output_hdmi) {
                printf("[AUDIO SERVER] ⚡ Executing real hardware switch: Current=%d -> Target=%d\n", 
                       current_output_hdmi, target_output_hdmi);
                
                if (target_output_hdmi == 1) {
                    if (configure_alsa_device(&alsa_handle, "hw:2,0") == 0) {
                        current_output_hdmi = 1; // Update actual tracking state on success
                    } else {
                        // Fallback to speaker if HDMI hardware allocation fails
                        configure_alsa_device(&alsa_handle, "default");
                        current_output_hdmi = 0;
                    }
                } else {
                    if (configure_alsa_device(&alsa_handle, "default") == 0) {
                        current_output_hdmi = 0; // Update actual tracking state
                    }
                }
            } else {
                // Ignore the signal if we are already streaming to the correct interface
                // This eliminates the periodic 1-second audio "bump" completely!
                printf("[AUDIO SERVER] Switch ignored. Already routed to requested output (%d).\n", current_output_hdmi);
            }
        }

        ssize_t bytes_read = read(fifo_fd, buffer, BUFFER_SIZE);
        
        if (bytes_read > 0) {
            snd_pcm_uframes_t frames = bytes_read / 4; 
            
            if (alsa_handle != NULL) {
                snd_pcm_sframes_t written = snd_pcm_writei(alsa_handle, buffer, frames);
                if (written < 0) {
                    snd_pcm_prepare(alsa_handle); 
                }

                // MINUI BACKPRESSURE SYNC BLOCK
                snd_pcm_sframes_t delay_frames;
                if (snd_pcm_delay(alsa_handle, &delay_frames) == 0) {
                    if (delay_frames > BUSY_THRESHOLD) {
                        snd_pcm_uframes_t extra_frames = delay_frames - BUSY_THRESHOLD;
                        useconds_t usleep_time = (uint64_t)extra_frames * 1000000 / SAMPLING_FREQ;
                        if (usleep_time > 0) {
                            usleep(usleep_time); 
                        }
                    }
                }
            }
        } else if (bytes_read == 0) {
            usleep(1000); 
        }
    }

    close(fifo_fd);
    if (alsa_handle) snd_pcm_close(alsa_handle);
    unlink(FIFO_PATH);
    return 0;
}
