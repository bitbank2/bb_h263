#include <SDL2/SDL.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <bb_h263.h>
#include <time.h>

BB_H263 h263;

#define PIXEL_TYPE LITTLE_ENDIAN_PIXELS
#define BITS_PER_PIXEL 16

//#define PIXEL_TYPE GIF_PALETTE_RGB8888
//#define BITS_PER_PIXEL 32

void Draw(H263DRAW *pDraw)
{
}

int main(int argc, char *argv[])
{
    SDL_Window *win;
    SDL_Surface *canvas, *winSurface;
    int rc, w, h;
    
    if (argc != 2) {
        printf("sdl2 h.263 player\nUsage: sdl2_play <filename>\n");
        return -1;
    }
    rc = h263.open(argv[1], Draw);
    if (rc != H263_SUCCESS) {
    	printf("Error opening %s = %d\n", argv[1], rc);
    	return -1;
    }
    w = h263.getWidth(); h = h263.getHeight();
    printf("%s opened, size: %d x %d\n", argv[1], w, h);
 
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    win = SDL_CreateWindow("H.263 Player", 352, 288, w, h, SDL_WINDOW_SHOWN);
    if (win == nullptr) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    // Create a surface to hold the video frames
#if BITS_PER_PIXEL == 16
    canvas = SDL_CreateRGBSurfaceWithFormat(0, w, h, 16, SDL_PIXELFORMAT_RGB565);
#else
    canvas = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ABGR8888);
#endif
    if (canvas == nullptr) {
        printf("SDL_CreateSurface error %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
	SDL_Quit();
	return EXIT_FAILURE;
    }
    h263.setFrameBuf((uint8_t *)canvas->pixels); // draw into the SDL2 buffer
    winSurface = SDL_GetWindowSurface(win);
    
    bool bQuit = false;
    for (int iLoop = 0; iLoop < 5; iLoop++) { // run it 5 times
        rc = H263_SUCCESS;
        while (!bQuit && rc == H263_SUCCESS) {
            SDL_Rect rect;
            SDL_Event e;
            rc = h263.decodeFrame();
            while (SDL_PollEvent(&e)) { // take care of queued events
                if (e.type == SDL_QUIT || e.type == SDL_KEYDOWN) {
                    bQuit = true;
                }
            }
            rect.x = 0; rect.y = 0; // corner offset
            rect.w = h263.getWidth(); rect.h = h263.getHeight();
            SDL_BlitSurface(canvas, &rect, winSurface, &rect);
            SDL_UpdateWindowSurface(win);
	    usleep(h263.getFrameDelay()); // frame delay in microseconds
        }
    } // for i
    // Clean up
    SDL_FreeSurface(canvas);
    SDL_FreeSurface(winSurface);
    SDL_DestroyWindow(win);
    SDL_Quit();
    h263.close();
    return EXIT_SUCCESS;
} /* main() */
