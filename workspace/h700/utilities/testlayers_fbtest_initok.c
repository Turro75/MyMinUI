#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

// ============================================================================
// CONFIGURAZIONE RIGIDA STRUTTURE E IOCTL ALLWINNER DISP2 / CEDARC
// ============================================================================

#define ION_IOC_SUNXI_FREE           0xC0044901 // La ioctl nativa Allwinner di gestione handle
#define ION_IOC_SUNXI_ALLOC          0xC0144900 // L'ABI da 20 byte validata
#define ION_IOC_SUNXI_SHARE          0xC0084904 // Lo Share a 8 byte validato

#define SUNXI_DISP_GET_SCN_WIDTH     7          // Interroga la larghezza reale del pannello
#define SUNXI_DISP_GET_SCN_HEIGHT    8          // Interroga l'altezza reale del pannello

struct ion_allocation_data_v1 {
    size_t len;
    size_t align;
    unsigned int heap_id_mask;
    unsigned int flags;
    int handle;
};

struct ion_fd_data_v1 { 
    int handle; 
    int fd; 
};

// Contesto statico ad indici per i descrittori hardware
static struct {
    int ionfd;
    int dispfd; // 🎯 Nuovo campo aggiunto nel contesto globale
    unsigned int page_size;
    int32_t ion_fds[5];
    int32_t ion_handles[5];
    void *virt_addrs[5];
} vid;

// Riproduzione dei log di errore interni di CdcIonFree presenti nell'output originale
void print_sunxi_cedar_error(int ret, int err_code) {
    printf("[2026-07-03 16:07:14] ERROR  : cedarc <CdcIonFree:265>: free ion_handle err, ret %d errno:%d\n\n", ret, err_code);
}

// Funzione di chiusura sicura delle risorse hardware
void clean_up(void) {
    int i;
    printf("\n[INFO] Avvio sequenza di pulizia hardware...\n");
    
    // 🎯 Se l'FD del display è valido, lo chiudiamo
    if (vid.dispfd >= 0) {
        close(vid.dispfd);
    }

    for (i = 0; i < 5; i++) {
        if (vid.virt_addrs[i] && vid.virt_addrs[i] != MAP_FAILED) {
            munmap(vid.virt_addrs[i], vid.page_size);
        }
        if (vid.ion_fds[i] >= 0) close(vid.ion_fds[i]);
    }
    if (vid.ionfd >= 0) close(vid.ionfd);
    printf("[INFO] Pulizia completata.\n");
}

// ============================================================================
// MAIN PIPELINE DI INIZIALIZZAZIONE MEMORIA + DISPLAY ENGINE
// ============================================================================
int main(int argc, char *argv[]) {
    int i, ret;
    uint32_t check_arg = 0;

    // Variabile a 64-bit per emulare il tipo time_t locale svelato da Ghidra
    uint64_t spurious_time_stack = 0;

    vid.page_size = 1847296; 
    vid.ionfd = -1;
    vid.dispfd = -1; // Inizializzato a -1 prima dell'apertura
    
    for (i = 0; i < 5; i++) { 
        vid.ion_fds[i] = -1; 
        vid.virt_addrs[i] = MAP_FAILED; 
        vid.ion_handles[i] = -1;
    }


    puts("use:xres yres bpp");
    puts("default lcd:  640x480x16 ");
    puts("default hdmi: 1280x720x16 ");
    printf("---lcd=640x480, bpp =16, hdmi=1280x720\n");

    // 1. Apriamo unicamente /dev/ion all'inizio (Prende l'FD 3)
    vid.ionfd = open("/dev/ion", O_RDWR);
    if (vid.ionfd < 0) {
        printf("[ERROR] Impossibile aprire /dev/ion\n");
        return 1;
    }

    // 2. Ciclo sequenziale dei 5 buffer basato sull'esatta cascata di ioctl
    for (i = 0; i < 5; i++) {
        struct ion_allocation_data_v1 alloc_v1;
        struct ion_fd_data_v1 share_data;

        // CHIAMATA 1 (Check iniziale di CdcIonAllocFd su local_144 = 0)
        check_arg = 0;
        ret = ioctl(vid.ionfd, ION_IOC_SUNXI_FREE, &check_arg);
        if (ret < 0) print_sunxi_cedar_error(ret, errno);

        // Allocazione dell'handle a 20-byte (0xC0144900)
        memset(&alloc_v1, 0, sizeof(alloc_v1));
        alloc_v1.len = vid.page_size;
        alloc_v1.align = 4096;
        alloc_v1.heap_id_mask = 1;
        alloc_v1.flags = 3;

        if (ioctl(vid.ionfd, ION_IOC_SUNXI_ALLOC, &alloc_v1) < 0) {
            printf("[ERROR] Allocazione fallita all'indice %d\n", i);
            clean_up(); return 1;
        }
        vid.ion_handles[i] = alloc_v1.handle;

        // CHIAMATA Civetta aggiuntiva eseguita all'inizio di CdcIonShare (Ghidra Pagina 24)
        check_arg = 0;
        ret = ioctl(vid.ionfd, ION_IOC_SUNXI_FREE, &check_arg);
        if (ret < 0) print_sunxi_cedar_error(ret, errno);

        // Generazione del File Descriptor condivisibile via SHARE (0xC0084904)
        memset(&share_data, 0, sizeof(share_data));
        share_data.handle = alloc_v1.handle;
        share_data.fd = -1;
        if (ioctl(vid.ionfd, ION_IOC_SUNXI_SHARE, &share_data) < 0) {
            printf("[ERROR] Share fallito all'indice %d\n", i);
            clean_up(); return 1;
        }
        vid.ion_fds[i] = share_data.fd;

        // CHIAMATA 2 (Il rilascio di local_144 allineato male a 64-bit)
        spurious_time_stack = (uint64_t)alloc_v1.handle | 0xDEADBEEF00000000ULL;
        int32_t handle_to_free = (int32_t)spurious_time_stack;
        ret = ioctl(vid.ionfd, ION_IOC_SUNXI_FREE, &handle_to_free);
        if (ret < 0) print_sunxi_cedar_error(ret, errno);

        // Mappatura virtuale coerente Allwinner (prot=3, flags=1)
        vid.virt_addrs[i] = mmap(NULL, vid.page_size, PROT_READ|PROT_WRITE, MAP_SHARED, vid.ion_fds[i], 0);
        if (vid.virt_addrs[i] == MAP_FAILED) {
            printf("[ERROR] Mmap fallita all'indice %d\n", i);
            clean_up(); return 1;
        }

        printf("ion alloc fd= %d, size = %u, addr_vir = %p\n", 
               vid.ion_fds[i], vid.page_size, vid.virt_addrs[i]);
    }

    // 🎯 3. APERTURA DI /DEV/DISP (Prende esattamente l'FD 9 in modo speculare a fbtest3)
    vid.dispfd = open("/dev/disp", O_RDWR);
    if (vid.dispfd < 0) {
        printf("[ERROR] Impossibile aprire /dev/disp: %s\n", strerror(errno));
        clean_up();
        return 1;
    }
    printf("ion alloc fd= %d\n", vid.dispfd);

 // 🎯 REPLICA INTEGRALE: Cattura della geometria nativa dal valore di ritorno dell'ioctl
    uint32_t disp_args[4] = {0}; // 🎯 L'array di configurazione pulito per DISP2

    memset(disp_args, 0, sizeof(disp_args)); // [0, 0, 0, 0] -> Head ID 0
    uint32_t orig_screenwidth  = ioctl(vid.dispfd, SUNXI_DISP_GET_SCN_WIDTH, (void*)disp_args); 
    uint32_t orig_screenheight = ioctl(vid.dispfd, SUNXI_DISP_GET_SCN_HEIGHT, (void*)disp_args); 
    
    printf("Physical screen size detected WxH: %dx%d\n", orig_screenwidth, orig_screenheight);

    printf("\n[SUCCESS] Inizializzazione della memoria speculare a fbtest3 completata!\n");

    // Eseguiamo la chiusura controllata prima del termine regolare dell'app
    clean_up();
    return 0;
}
