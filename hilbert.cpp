#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <stdbool.h>

#include <map>

std::map<uint32_t,int> khist;

#include <SDL.h>

uint64_t ram_size = 0;
int max_count = 0;

// Needed because SDL's rects (in 1.2) are signed shorts!
typedef struct my_rect {
    int x, y;
    int w, h;
} my_rect;

typedef struct point {
    int x;
    int y;
} point;

void hexDump (const char *desc, void *addr, int len, int offset);
void rot(int n, int *x, int *y, int rx, int ry);
void d2xy(int n, int d, int *x, int *y);
int xy2d (int n, int x, int y);
void render_full(SDL_Surface *dest, my_rect *view, int hilbert_size);
bool init_sdl(void);
void do_pan_mode(void);

#define SIZE 256
#define SCALE 3

//rotate/flip a quadrant appropriately
void rot(int n, int *x, int *y, int rx, int ry) {
    assert(__builtin_popcount(n) == 1);
    if (ry == 0) {
        if (rx == 1) {
            *x = n-1 - *x;
            *y = n-1 - *y;
        }
 
        //Swap x and y
        int t  = *x;
        *x = *y;
        *y = t;
    }
}

//convert (x,y) to d
int xy2d (int n, int x, int y) {
    assert(__builtin_popcount(n) == 1);
    int rx, ry, s, d=0;
    for (s=n/2; s>0; s/=2) {
        rx = (x & s) > 0;
        ry = (y & s) > 0;
        d += s * s * ((3 * rx) ^ ry);
        rot(s, &x, &y, rx, ry);
    }
    return d;
}
 
//convert d to (x,y)
void d2xy(int n, int d, int *x, int *y) {
    assert(__builtin_popcount(n) == 1);
    int rx, ry, s, t=d;
    *x = *y = 0;
    for (s=1; s<n; s*=2) {
        rx = 1 & (t/2);
        ry = 1 & (t ^ rx);
        rot(s, x, y, rx, ry);
        *x += s * rx;
        *y += s * ry;
        t /= 4;
    }
}
 
void render_full(SDL_Surface *dest, my_rect *view, int hilbert_size) {
    //uint8_t byte = 0;
    // Clear screen
    SDL_FillRect(dest, NULL, 0x000000);
    // Fill the pixel buffer
    //printf("View: %d,%d %dx%d\n", view->x, view->y, view->w, view->h);
    // xy2d expects real x,y coordinates, but our view may be scaled
    // So we instead loop over the real coordinates by dividing through by SCALE
    for(int x = view->x/SCALE; x < (view->x+view->w)/SCALE; x++) {
        for (int y = view->y/SCALE; y < (view->y+view->h)/SCALE; y++) {
            int d = xy2d(hilbert_size, x, y);
            
            // Skip parts of the curve outside of RAM
            if (khist.find(d) == khist.end()) continue;
            float frac = (1 - (khist[d] / (float) max_count));
            uint8_t scaled = frac * 255;
            uint8_t color[] = {scaled, scaled, scaled};
            // Map it back into the window
            int adj_x, adj_y;
            adj_x = (x*SCALE) - view->x;
            adj_y = (y*SCALE) - view->y;
            //if (-1 == panda_physical_memory_rw(d, &byte, 1, false)) {
            //    byte = 0;
            //}
            //uint8_t *color = colortable[byte];
            // We end up drawing SCALExSCALE pixels for each byte. So 4 pixels at 2x magnification
            for (int i = 0; i < SCALE; i++) {
                for (int j = 0; j < SCALE; j++) {
                    memcpy((Uint8 *)dest->pixels + ((adj_y+j) * dest->pitch) + ((adj_x+i) * sizeof(Uint8) * 3), color, 3);
                }
            }
        }
    }
}

int hilbert_size = 1;
// view controls which portion of the whole curve we can currently see
my_rect view = {0,0,SIZE*SCALE,SIZE*SCALE};
SDL_Surface *win;

void random_recenter(void) {
    uint32_t n = arc4random_uniform(khist.size() - 1);
    int i = 0;
    uint32_t addr = 0;
    for (std::map<uint32_t,int>::iterator kvp = khist.begin(); kvp != khist.end(); kvp++) {
        if (i++ < n) continue;
        point p;
        d2xy(hilbert_size, kvp->first, &p.x, &p.y);
        view.x = ((p.x*SCALE) > view.w / 2) ? ((p.x*SCALE) - (view.w / 2)) : 0;
        view.y = ((p.y*SCALE) > view.h / 2) ? ((p.y*SCALE) - (view.h / 2)) : 0;
        addr = kvp->first;
        break;
    }
    printf("Recentering at random address %08x\n", 0x80000000 + addr);
}

void do_pan_mode(void) {
    SDL_Event event;
    while (1) {
        if (SDL_PollEvent(&event)) {
            switch(event.type) {
                case SDL_QUIT:
                    SDL_Quit();
                    exit(0);
                    break;
                case SDL_KEYUP:
                    {
                    bool should_rerender = false;
                    switch(event.key.keysym.sym) {
                        case SDLK_LEFT:
                            view.x -= SIZE*SCALE / 2;
                            if (view.x < 0) view.x = 0;
                            should_rerender = true;
                            break;
                        case SDLK_RIGHT:
                            view.x += SIZE*SCALE / 2;
                            if (view.x > (hilbert_size-SIZE)*SCALE) view.x = (hilbert_size - SIZE)*SCALE;
                            should_rerender = true;
                            break;
                        case SDLK_UP:
                            view.y -= SIZE*SCALE / 2;
                            if (view.y < 0) view.y = 0;
                            should_rerender = true;
                            break;
                        case SDLK_DOWN:
                            view.y += SIZE*SCALE / 2;
                            if (view.y > (hilbert_size-SIZE)*SCALE) view.y = (hilbert_size - SIZE)*SCALE;
                            should_rerender = true;
                            break;
                        case SDLK_q:
                            SDL_Quit();
                            exit(0);
                            break;
                        case SDLK_r:
                            random_recenter();
                            should_rerender = true;
                            break;
                        default:
                            break;
                    }
                    if (should_rerender) {
                        render_full(win, &view, hilbert_size);
                        SDL_Flip(win);
                    }
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    {
                        uint32_t addr;
                        addr = xy2d(hilbert_size, (event.button.x+view.x)/SCALE, (event.button.y+view.y)/SCALE);
                        printf("Address 0x%08x appears in %d out of %d samples.\n", addr+0x80000000, khist[addr], max_count);
                    }
                    break;
            }
        }
    }
}

bool init_sdl(void) {
    // Figure out how big the whole hilbert curve needs to be
    hilbert_size = 1;
    while ((hilbert_size*hilbert_size) < ram_size) {
        hilbert_size <<= 1;
    }
    fprintf(stderr, "NOTE: Creating a %d x %d canvas.\n", hilbert_size, hilbert_size);

    // Init SDL
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return false;
    }
    win = SDL_SetVideoMode(SIZE*SCALE, SIZE*SCALE, 24, SDL_HWSURFACE | SDL_DOUBLEBUF);
    if (win == NULL) {
        fprintf(stderr, "SDL_SetVideoMode failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_WM_SetCaption("Hilbert", "Hilbert");

    // Do the initial rendering
    fprintf(stderr, "View size: (real) %d x %d (virtual) %d x %d\n", view.w, view.h, view.w/SCALE, view.w/SCALE);

    return true;
}

int main(int argc, char **argv) {
    FILE *fp = fopen(argv[1], "r");
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    uint32_t first_addr = 0;
    printf("Reading file data... "); fflush(stdout);
    while ((linelen = getline(&line, &linecap, fp)) > 0) {
        uint32_t addr;
        int count;
        sscanf(line, "0x%08x  %d", &addr, &count);
        khist[addr - 0x80000000] = count;
        ram_size = (ram_size < addr) ? addr : ram_size;
        max_count = (max_count < count) ? count: max_count;
        if (first_addr == 0) first_addr = addr - 0x80000000;
    }
    printf("Done.\n");
    ram_size -= 0x80000000;

    init_sdl();

    // Set the initial view randomly
    random_recenter();
    render_full(win, &view, hilbert_size);
    SDL_Flip(win);

    do_pan_mode();
    SDL_Quit();
    return 0;
}
