#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#if defined(USE_SDL2)
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "SDL2_rotozoom.h"
#else
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include "SDL_rotozoom.h"
#endif

#include <msettings.h>

#include "defines.h"
#include "utils.h"
#include "api.h"

#define SRCDIR  SDCARD_PATH "/Tools/" THISPLATFORM "/Convert BoxArt.pak/srcimgdir"
#define DSTDIR SRCDIR "/outdir"

int X,Y,W,H,SW,SH;
enum Aspect aspect;



int makeBoxart(char *, char *, myBoxartData);

int
filter(const struct dirent *name)
{
  return 1;
}

int main(int argc, char* argv[]) {
    PWR_setCPUSpeed(CPU_SPEED_PERFORMANCE);

    SDL_Surface* screen = GFX_init(MODE_MAIN);
    PWR_init();
    InitSettings();

    SDL_Event event;
    int quit = 0;
    int save_changes = 0;

    struct dirent **namelist;
    int n;
    myBoxartData boxartdata;
    readBoxartcfg(TOOLBOXART_CFGFILE, &boxartdata);
    
    //char cmd[512];
    //sprintf(cmd,"cp \"/mnt/sdcard/Tools/imgdir/PSorig.png\" " MYINPUT);
    //system(cmd);
    //sprintf(cmd,"cp \"/mnt/sdcard/Roms/NeoGeo CD (NGCD)/Imgs/mslug5.png\" \"/mnt/sdcard/Roms/NeoGeo (NG)/Imgs/mslug5.png\"");
    //system(cmd);
    LOG_info("Scanning image dir %s\n", SRCDIR);
    n = scandir(SRCDIR, &namelist, filter, alphasort);
    if (n == -1) {
     LOG_info("Failed scandir");
     return -1;
    }
    DIR* dir = opendir(DSTDIR);
    if (dir) {
    /* Directory exists. */
         closedir(dir);
    } else {
    /* Directory does not exist. */
    //create DSTDIR
        LOG_info("Creating image dest dir %s\n", DSTDIR);
        mkdir(DSTDIR, 0755);
    } 
    int total = n-1;
    char src[256];
    char dst[256];
    char progressstr[100];
    n=0;
    while (n<=total) {
        if (namelist[n]->d_type == DT_REG)
        {              
            sprintf(src,SRCDIR "/%s",namelist[n]->d_name);
            sprintf(dst,DSTDIR "/%s",namelist[n]->d_name);
            makeBoxart(src,dst, boxartdata);
            sprintf(progressstr,"Converted %d/%d\n%s",n,total,namelist[n]->d_name);
        } else {
            sprintf(progressstr,"Skipped %d/%d\n%s",n,total,namelist[n]->d_name);
        }        
        GFX_clear(screen);
        GFX_blitMessage(font.large, progressstr, screen, &(SDL_Rect){0,0,screen->w,screen->h});
        free(namelist[n]);
        n++;
        GFX_flip(screen);
    }
    free(namelist);
    //GFX_clear(screen);
    GFX_blitMessage(font.large, "Press A to Exit", screen, &(SDL_Rect){0,200,screen->w,screen->h-200});

    GFX_flip(screen);
    PAD_init();
    // Wait for user's input
    while (!quit) {
        PAD_poll();
        if (PAD_justPressed(BTN_A)) {
            quit = 1;
        } else {
            GFX_sync();
        }
    }

    PWR_setCPUSpeed(CPU_SPEED_MENU);
    QuitSettings();    
    PWR_quit();
    GFX_quit();
    PAD_quit();
    return EXIT_SUCCESS;
}

int makeBoxart(char *infilename, char *outfilename, myBoxartData mydata) {
    SDL_Surface *image = IMG_Load(infilename);
    SDL_Surface *mysurface = SDL_CreateRGBSurface(0, mydata.sW , mydata.sH ,16,0,0,0,0);
    //SDL_Surface *unscaled_myimg = IMG_Load(BACKGROUND);
    if (image == NULL){
        LOG_info("error loading the image %s", infilename);
        return -1;
    }
    double xfactr, yfactr;
    double myaspect = 1.0 * image->w / image->h; 
    int localX = mydata.bX;
    int localY = mydata.bY; 
    switch (mydata.aspect) {
        case ASPECT: 
            LOG_info("ASPECT = ASPECT\n");
            //first calculate othe original aspectratio
            if (myaspect > 1.0){
                //the W is bigger than H
                    //resize to fit W
                    xfactr = 1.0 * mydata.bW / image->w;
                    yfactr = xfactr; 
                    localY += (mydata.bH - (image->h * yfactr))/2;
            } else { //image is higher than wider
                    //resize to fit H
                    yfactr = 1.0 * mydata.bH / image->h;
                    xfactr = yfactr; 
                    localX += (mydata.bW - (image->w * xfactr))/2;
            }
            break;
        case NATIVE: 
            LOG_info("ASPECT = NATIVE\n");
            if ((mydata.bW > image->w) && (mydata.bH > image->h)){
                //image is smaller than target box
                xfactr = 1.0;
                yfactr = 1.0; 
                //change x y to place the image in the center of the target box
                localX += (mydata.bW - image->w)/2;
                localY += (mydata.bH - image->h)/2;
            } else {
                //image is bigger than target box so apply ASPECT rule
                if (myaspect > 1.0){
                    //the W is bigger than H
                        //resize to fit W
                        xfactr = 1.0 * W / image->w;
                        yfactr = xfactr; 
                        localY += (mydata.bH - (image->h * yfactr))/2;
                } else { //image is higher than wider
                        //resize to fit H
                        yfactr = 1.0 * mydata.bH / image->h;
                        xfactr = yfactr; 
                        localX += (mydata.bW - (image->w * xfactr))/2;
                }
            }            
            break;
        case FULL: 
            //fill target box by shrinking/streching the original image
            LOG_info("ASPECT = FULL\n");
            xfactr = 1.0 * mydata.bW / image->w;
            yfactr = 1.0 * mydata.bH / image->h; 
            break;
    }
    SDL_Surface *scaled_myimg = zoomSurface(image, xfactr , yfactr, 0);   
    SDL_BlitSurface(scaled_myimg,NULL,mysurface,&(SDL_Rect){localX,localY}); 
    //char mygradient[256];
    //sprintf(mygradient,"%s",mydata.gradient);
    
    if (strncmp(mydata.gradient,"NONE",4) != 0){
        SDL_Surface *blackgradient = IMG_Load(mydata.gradient);
        if (blackgradient == NULL){
            LOG_info("Failed loading Gradient: %s\n", IMG_GetError());
            return -1;  
        } 
#if defined (USE_SDL2)
		SDL_SetSurfaceBlendMode(blackgradient,SDL_BLENDMODE_BLEND);
#else
		SDL_SetColorKey(blackgradient, SDL_TRUE, SDL_MapRGB(blackgradient->format, 0, 0, 0));
#endif
        SDL_BlitSurface(blackgradient,NULL,mysurface,NULL);
        SDL_FreeSurface(blackgradient);
    }

    SDL_RWops* out = SDL_RWFromFile(outfilename, "wb");
#if defined (USE_SDL2)
    IMG_SavePNG_RW(mysurface, out, 1); 
#else
    SDL_SaveBMP_RW(mysurface, out, 1); 
    bmp2png(outfilename);
#endif   
    SDL_FreeSurface(scaled_myimg); 
    SDL_FreeSurface(mysurface);
    SDL_FreeSurface(image);
    return 1;
}