#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <linux/fb.h>

// Ioctl del Display Engine Allwinner
#define DISP_LAYER_SET_CONFIG  0x47

// Strutture rigide a 20 e 8 byte per lo strato compat_ioctl dell'H700
struct ion_alloc_data_h700 {
    uint32_t len;
    uint32_t align;
    uint32_t heap_id_mask;
    uint32_t flags;
    int32_t  handle;
};

struct ion_fd_data_h700 {
    int32_t handle;
    int32_t fd;
};

#undef ION_IOC_ALLOC
#undef ION_IOC_SHARE
#define ION_IOC_ALLOC _IOWR('I', 0, struct ion_alloc_data_h700) // 0xC0144900
#define ION_IOC_SHARE _IOWR('I', 4, struct ion_fd_data_h700)    // 0xC0084904

#define IOCTL_ENGINE_REQ        0x206
#define IOCTL_ENGINE_REL        0x207
#define IOCTL_GET_IOMMU_ADDR    0x502
#define IOCTL_FREE_IOMMU_ADDR   0x503

struct user_iommu_param {
    int fd;
    unsigned int iommu_addr;
};

// Geometria fissa Allwinner DISP2
struct disp_rect64 { int64_t x; int64_t y; int64_t width; int64_t height; };
struct disp_rect   { int x; int y; unsigned int width; unsigned int height; };
struct disp_rectsz { unsigned int width; unsigned int height; };

struct disp_fb_info {
    unsigned long long addr[3];
    struct disp_rectsz size[3];
    unsigned int align[3];
    int format;
    int color_space;	
    unsigned int trd_right_addr[3];	
    uint32_t pre_multiply;	
    struct disp_rect64 crop;	
    int flags;
    int scan;
    unsigned int lbc_en;
    unsigned int lbc_info; 
};

struct disp_layer_info {
    int mode;                  
    unsigned int zorder;       
    unsigned int alpha_mode;   
    unsigned int alpha_value;  
    struct disp_fb_info fb;      
    struct disp_rect screen_win; 
    int b_trd_out;
    int out_trd_mode;
    unsigned int color;
};

struct disp_layer_config {
    struct disp_layer_info info;
    unsigned int enable;
    unsigned int channel;
    unsigned int layer_id;
};

static struct {
    int dispfd;
    int ionfd;
    int cedarfd;
    int fdfb;
    int mem_fd;
    unsigned int single_frame_size;
    void *virt_addr_base;
    unsigned int phy_addr_base;
    struct disp_layer_config layer_config;
    struct fb_var_screeninfo vinfo;
} vid;

struct dma_buf_sync {
    uint64_t flags;
};
#define DMA_BUF_SYNC_READ  (1 << 0)
#define DMA_BUF_SYNC_WRITE (2 << 0)
#define DMA_BUF_SYNC_RW    (DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE)
#define DMA_BUF_SYNC_START (0 << 2)
#define DMA_BUF_SYNC_END   (1 << 2)
#define DMA_BUF_IOCTL_SYNC _IOW('b', 0, struct dma_buf_sync)

struct cedar_cache_range {
    void *start_virt_addr;    // Indirizzo virtuale di partenza (userspace)
    unsigned int length;      // Lunghezza del buffer da ripulire
};
#undef  IOCTL_FLUSH_CACHE_RANGE
#define IOCTL_FLUSH_CACHE_RANGE 0x506

struct ion_custom_data_h700 {
    unsigned int cmd;
    unsigned int arg; // Puntatore a 32-bit castato
};

struct sunxi_cache_range_args {
    void *start_virt_addr;
    unsigned int length;
};

#undef ION_IOC_CUSTOM
#define ION_IOC_CUSTOM _IOWR('I', 6, struct ion_custom_data_h700)

// I sotto-comandi segreti del modulo sunxi_ion per la gestione della cache
#define AW_ION_IOC_CLEAN_CACHE          0x02
#define AW_ION_IOC_FLUSH_CACHE          0x04 // 🎯 QUESTO forza lo svuotamento da CPU a RAM fisica!

struct dma_buf_sync_64 {
    uint64_t flags; // 🎯 FIX: Deve essere rigorosamente a 64-bit sia a 32 che a 64 bit!
};
#define DMA_BUF_SYNC_READ  (1 << 0)
#define DMA_BUF_SYNC_WRITE (2 << 0)
#define DMA_BUF_SYNC_RW    (DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE)
#define DMA_BUF_SYNC_START (0 << 2)
#define DMA_BUF_SYNC_END   (1 << 2)

// Genera il codice ioctl esatto preteso dallo strato a 64-bit del kernel Anbernic
#define REAL_DMA_BUF_IOCTL_SYNC _IOW('b', 0, struct dma_buf_sync_64)

int main(int argc, char *argv[]) {
    struct ion_alloc_data_h700 alloc_data;
    struct ion_fd_data_h700 fd_data;
    struct user_iommu_param iommu_param;
    int i, current_page = 0, current_frame = 0;
    uint32_t args[4] = {0};

    unsigned int pixels_per_page = 640 * 480;
    vid.single_frame_size = pixels_per_page * 4; // 1.228.800 byte

    vid.ionfd = open("/dev/ion", O_RDWR);
    vid.cedarfd = open("/dev/cedar_dev", O_RDWR);
    vid.dispfd = open("/dev/disp", O_RDWR);
    vid.fdfb = open("/dev/fb0", O_RDWR);
    
    ioctl(vid.cedarfd, IOCTL_ENGINE_REQ, 0);
    ioctl(vid.fdfb, FBIOGET_VSCREENINFO, &vid.vinfo);

    // 1. ALLOCAZIONE DOPPIA DIMENSIONE VIA ION (2.45 MB totali per due pagine inline)
    memset(&alloc_data, 0, sizeof(alloc_data));
    alloc_data.len = vid.single_frame_size * 2; 
    alloc_data.align = 4096;

    alloc_data.heap_id_mask = (1 << 2) | (1 << 4); 
    alloc_data.flags = 0; // Scrittura diretta non-cached
    

    if (ioctl(vid.ionfd, ION_IOC_ALLOC, &alloc_data) < 0) return 1;
    
    memset(&fd_data, 0, sizeof(fd_data));
    fd_data.handle = alloc_data.handle;
    ioctl(vid.ionfd, ION_IOC_SHARE, &fd_data);
    vid.mem_fd = fd_data.fd;

    vid.virt_addr_base = mmap(NULL, alloc_data.len, PROT_READ|PROT_WRITE, MAP_SHARED, vid.mem_fd, 0);
    
    iommu_param.fd = vid.mem_fd;
    ioctl(vid.cedarfd, IOCTL_GET_IOMMU_ADDR, &iommu_param);
    vid.phy_addr_base = iommu_param.iommu_addr;

    // Definiamo i puntatori virtuali alle due metà del buffer ION
    uint32_t *page0_virt = (uint32_t *)vid.virt_addr_base;
    uint32_t *page1_virt = (uint32_t *)(vid.virt_addr_base + vid.single_frame_size);

    // 2. SCRITTURA STATICA ASINCRONA INIZIALE
    for (i = 0; i < pixels_per_page; i++) page0_virt[i] = 0xFF0000FF; // Pagina 0: Tutto Blu
    for (i = 0; i < pixels_per_page; i++) page1_virt[i] = 0xFFFF00FF; // Pagina 1: Tutto Rosso

    // 3. CONFIGURAZIONE STATICA DEL LAYER (Canale 1, Layer 0 - Sopra dmenu)
    memset(&vid.layer_config, 0, sizeof(struct disp_layer_config));
    vid.layer_config.channel = 1;  
    vid.layer_config.layer_id = 0; 
    vid.layer_config.enable = 1;               
    vid.layer_config.info.mode = 0;             // LAYER_MODE_BUFFER
    vid.layer_config.info.zorder = 150;         
    vid.layer_config.info.alpha_mode = 1;       
    vid.layer_config.info.alpha_value = 255;    

    vid.layer_config.info.fb.size[0].width = 640;
    vid.layer_config.info.fb.size[0].height = 480;
    vid.layer_config.info.fb.align[0] = 32;
    vid.layer_config.info.fb.format = 4;        // XRGB8888 (Forza i pixel opachi)
    vid.layer_config.info.fb.crop.width = ((int64_t)640) << 32;
    vid.layer_config.info.fb.crop.height = ((int64_t)480) << 32;
    vid.layer_config.info.screen_win.width = 640;   
    vid.layer_config.info.screen_win.height = 480;

    printf("[LOOP] Avvio Double Buffering ION guidato da Layer + Pan Sync (300 frame)...\n");

    // Predisponiamo le strutture di controllo stabili per i dma-buf
    struct dma_buf_sync_64 sync_start = { .flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE };
    struct dma_buf_sync_64 sync_end   = { .flags = DMA_BUF_SYNC_END   | DMA_BUF_SYNC_WRITE };

    while (current_frame < 300) {
        
        // 1. Notifichiamo l'apertura del buffer: invalida la cache in lettura/scrittura
        ioctl(vid.mem_fd, REAL_DMA_BUF_IOCTL_SYNC, &sync_start);

        if (current_page == 0) {
            uint32_t color_blue = 0x000000FF; // Blu XRGB nativo Allwinner
            
            volatile uint32_t *ptr0 = (volatile uint32_t *)page0_virt;
            for (i = 0; i < pixels_per_page; i++) {
                ptr0[i] = color_blue;
            }

            // Chiediamo al Layer di inquadrare la pagina 0
            vid.layer_config.info.fb.addr[0] = vid.phy_addr_base;
            current_page = 1;
        } 
        else {
            uint32_t color_red = 0x00FF0000; // Rosso XRGB nativo Allwinner

            volatile uint32_t *ptr1 = (volatile uint32_t *)page1_virt;
            for (i = 0; i < pixels_per_page; i++) {
                ptr1[i] = color_red;
            }

            // Chiediamo al Layer di saltare in avanti alla pagina 1
            vid.layer_config.info.fb.addr[0] = vid.phy_addr_base + vid.single_frame_size;
            current_page = 0;
        }

        // 2. 🎯 IL VERO FLUSH: Comunichiamo la fine della scrittura.
        // Questa chiamata forza l'interruzione hardware dell'MMU, svuota la cache L2 della CPU
        // e spinge fisicamente i pixel blu e rossi dentro la RAM fisica (0xFF600000).
        ioctl(vid.mem_fd, REAL_DMA_BUF_IOCTL_SYNC, &sync_end);

        // Preparazione array argomenti per ioctl 0x47
        args[0] = 0; 
        args[1] = (uintptr_t)&vid.layer_config;
        args[2] = 1; 
        args[3] = 0;

        // 3. Applica lo switch dell'indirizzo fisico via Layer
        ioctl(vid.dispfd, DISP_LAYER_SET_CONFIG, (unsigned long)args);

        // 4. Il FRENO + SBLOCCO SHADOW: Chiamiamo il Pan su fb0
        vid.vinfo.yoffset = 0;
        vid.vinfo.xoffset = 0;
        ioctl(vid.fdfb, FBIOPAN_DISPLAY, &vid.vinfo);

        current_frame++;
    }

    // Sequenza di ripristino sicuro del sistema
    vid.layer_config.enable = 0;
    args[1] = (uintptr_t)&vid.layer_config;
    ioctl(vid.dispfd, DISP_LAYER_SET_CONFIG, (unsigned long)args);

    munmap(vid.virt_addr_base, alloc_data.len);
    close(vid.mem_fd);
    ioctl(vid.cedarfd, IOCTL_ENGINE_REL, 0);
    close(vid.cedarfd); close(vid.ionfd); close(vid.dispfd); close(vid.fdfb);
    printf("[EXIT] Test completato.\n");
    return 0;
}
