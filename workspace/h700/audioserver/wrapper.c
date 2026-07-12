#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

static int fake_dsp_fd = -1;

// Funzione interna per verificare se il percorso è quello cercato da SDL2
static int is_dsp_path(const char *pathname) {
    if (!pathname) return 0;
    // Accetta sia il percorso originale che quello redirezionato
    if (strcmp(pathname, "/dev/dsp") == 0 || strcmp(pathname, "/tmp/dsp") == 0) {
        return 1;
    }
    return 0;
}

// Funzione interna per verificare se SDL sta cercando varianti numerate (es. /tmp/dsp0)
static int is_numbered_dsp(const char *pathname) {
    if (!pathname) return 0;
    if (strncmp(pathname, "/tmp/dsp", 8) == 0 && pathname[8] >= '0' && pathname[8] <= '9') {
        return 1;
    }
    if (strncmp(pathname, "/dev/dsp", 8) == 0 && pathname[8] >= '0' && pathname[8] <= '9') {
        return 1;
    }
    return 0;
}

// Forza la struttura stat a sembrare un Character Device hardware
static void spoof_stat(struct stat *buf) {
    if (buf) {
        buf->st_mode = (buf->st_mode & ~S_IFMT) | S_IFCHR;
    }
}

// --- INTERCETTAZIONE DI TUTTE LE VARIANTI DI STAT ---

int stat(const char *pathname, struct stat *buf) {
    if (is_numbered_dsp(pathname)) return -1; // Nascondi i falsi device numerati
    int (*orig_stat)(const char *, struct stat *) = dlsym(RTLD_NEXT, "stat");
    int ret = orig_stat(pathname, buf);
    if (ret == 0 && is_dsp_path(pathname)) spoof_stat(buf);
    return ret;
}

int lstat(const char *pathname, struct stat *buf) {
    if (is_numbered_dsp(pathname)) return -1;
    int (*orig_lstat)(const char *, struct stat *) = dlsym(RTLD_NEXT, "lstat");
    int ret = orig_lstat(pathname, buf);
    if (ret == 0 && is_dsp_path(pathname)) spoof_stat(buf);
    return ret;
}

int fstat(int fd, struct stat *buf) {
    int (*orig_fstat)(int, struct stat *) = dlsym(RTLD_NEXT, "fstat");
    int ret = orig_fstat(fd, buf);
    if (ret == 0 && fd == fake_dsp_fd && fake_dsp_fd != -1) {
        spoof_stat(buf);
    }
    return ret;
}

// Alcuni sistemi embedded usano le chiamate interne __xstat e __fxstat
int __xstat(int ver, const char *pathname, struct stat *buf) {
    if (is_numbered_dsp(pathname)) return -1;
    int (*orig_xstat)(int, const char *, struct stat *) = dlsym(RTLD_NEXT, "__xstat");
    int ret = orig_xstat(ver, pathname, buf);
    if (ret == 0 && is_dsp_path(pathname)) spoof_stat(buf);
    return ret;
}

int __fxstat(int ver, int fd, struct stat *buf) {
    int (*orig_fxstat)(int, int, struct stat *) = dlsym(RTLD_NEXT, "__fxstat");
    int ret = orig_fxstat(ver, fd, buf);
    if (ret == 0 && fd == fake_dsp_fd && fake_dsp_fd != -1) {
        spoof_stat(buf);
    }
    return ret;
}

// --- INTERCETTAZIONE DI OPEN, IOCTL E CLOSE ---

int open(const char *pathname, int flags, ...) {
    int (*orig_open)(const char *, int, ...) = dlsym(RTLD_NEXT, "open");

    if (is_dsp_path(pathname)) {
        // Se SDL2 prova ad aprire in modalità bloccante, noi la assecondiamo 
        // ma puliamo eventuali flag che creano conflitti con le FIFO.
        // Usiamo O_WRONLY perché SDL deve solo scrivere dati audio.
        fake_dsp_fd = orig_open("/tmp/dsp", O_WRONLY);
        return fake_dsp_fd;
    }

    va_list args;
    va_start(args, flags);
    mode_t mode = va_arg(args, mode_t);
    va_end(args);
    return orig_open(pathname, flags, mode);
}


// Definiamo le macro standard di OSS per evitare dipendenze esterne
#define OSS_SNDCTL_DSP_GETFMTS   0x8004500b
#define OSS_SNDCTL_DSP_SETFMT    0xc0045005
#define OSS_SNDCTL_DSP_CHANNELS  0xc0045006
#define OSS_SNDCTL_DSP_SPEED     0xc0045002

#define OSS_AFMT_S16_LE          0x00000010 // Formato 16-bit Little Endian

int ioctl(int fd, unsigned long request, ...) {
    va_list args;
    va_start(args, request);
    void *arg = va_arg(args, void *);
    va_end(args);

    if (fd == fake_dsp_fd && fake_dsp_fd != -1) {
        // 1. SDL chiede quali formati supporta la console
        if (request == OSS_SNDCTL_DSP_GETFMTS) {
            if (arg) {
                *(int *)arg = OSS_AFMT_S16_LE; // Diciamo che supportiamo SOLO i 16-bit
            }
            return 0;
        }

        // 2. SDL imposta il formato
        if (request == OSS_SNDCTL_DSP_SETFMT) {
            if (arg) {
                *(int *)arg = OSS_AFMT_S16_LE; // Confermiamo che è impostato a 16-bit
            }
            return 0;
        }

        // 3. SDL imposta i canali (1 o 2)
        if (request == OSS_SNDCTL_DSP_CHANNELS) {
            if (arg) {
                *(int *)arg = 2; // Forziamo Stereo (2 canali) verso l'audioserver
            }
            return 0;
        }

        // 4. SDL imposta la frequenza (Hz)
        if (request == OSS_SNDCTL_DSP_SPEED) {
            if (arg) {
                *(int *)arg = 48000; // Forziamo SDL a ricampionare internamente a 48kHz nativi dell'Anbernic
            }
            return 0;
        }

        // Intercetta in modo generico qualsiasi altra chiamata OSS (identificata dalla lettera 'P')
        if (((request >> 8) & 0xFF) == 'P') { 
            return 0; 
        }
    }

    // Per tutto il resto della console, usa la ioctl originale
    int (*orig_ioctl)(int, unsigned long, void *) = dlsym(RTLD_NEXT, "ioctl");
    return orig_ioctl(fd, request, arg);
}


int close(int fd) {
    int (*orig_close)(int) = dlsym(RTLD_NEXT, "close");
    if (fd == fake_dsp_fd && fake_dsp_fd != -1) {
        fake_dsp_fd = -1;
    }
    return orig_close(fd);
}
