#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>

SDL_Surface* img = NULL;
char* fb0_map;
int map_size = 0;
int fb0_fd = -1;

void killHandler(int retvalue)
{
	if (img) SDL_FreeSurface(img);	
	if (fb0_map) munmap(fb0_map, map_size);
	if (fb0_fd >= 0) close(fb0_fd);
	exit(retvalue);
}


int main(int argc , char* argv[]) {
	if (argc<2) {
		puts("Usage: show image.png");
		return 0;
	}
	signal(SIGKILL, killHandler);
	signal(SIGTERM, killHandler);
	signal(SIGINT, killHandler);
	signal(SIGQUIT, killHandler);
	char path[256];
	if (strchr(argv[1], '/')==NULL) sprintf(path, "/mnt/SDCARD/.system/res/%s", argv[1]);
	else strncpy(path,argv[1],256);
	
	if (access(path, F_OK)!=0) return 0; // nothing to show :(
		
	fb0_fd = open("/dev/fb0", O_RDWR);
	if (fb0_fd < 0) {
		fprintf(stdout,"Failed to open /dev/fb0\n");fflush(stdout);
		killHandler(-1);
	}
	struct fb_var_screeninfo vinfo;
	ioctl(fb0_fd, FBIOGET_VSCREENINFO, &vinfo);
	map_size = vinfo.xres * vinfo.yres * (vinfo.bits_per_pixel / 8); // 640x480x4
	fb0_map = (char*)mmap(0, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb0_fd, 0);
	memset(fb0_map, 0, map_size); // clear screen
	
	img = IMG_Load(path); // 24-bit opaque png
	if (!img) {
		fprintf(stdout,"Failed to load image %s\n",path);fflush(stdout);
		killHandler(-1);
	}
	uint8_t* dst = (uint8_t*)fb0_map; // rgba
	uint8_t* src = (uint8_t*)img->pixels; // bgr
	src += ((img->h * img->w) - 1) * 3;
	for (int y=0; y<img->h; y++) {
		for (int x=0; x<img->w; x++) {
			*(dst+0) = *(src+2); // r
			*(dst+1) = *(src+1); // g
			*(dst+2) = *(src+0); // b
			*(dst+3) = 0xf; // alpha
			dst += 4;
			src -= 3;
		}
	}
	sleep(1);
	killHandler(0);
}
