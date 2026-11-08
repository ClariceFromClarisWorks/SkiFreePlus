#ifndef SKI_COMPAT_H
#define SKI_COMPAT_H
#include <stdint.h>
typedef void SDL_Texture;typedef void SDL_Window;typedef void SDL_Renderer;typedef void SDL_Surface;typedef void TTF_Font;typedef int SDL_TimerID;
typedef struct {int x,y,w,h;} SDL_Rect;
static uint32_t sim_ticks;
static unsigned SDL_GetTicks(void){return sim_ticks;}
static void capture_sprite(int id,int x,int y);
static int SDL_RenderCopy(void*a,void*b,void*c,void*d){SDL_Rect*r=d;capture_sprite((int)(uintptr_t)b,r->x,r->y);return 0;}
static void SDL_FreeSurface(void*p){}
static void* SDL_CreateRGBSurface(int a,int b,int c,int d,unsigned e,unsigned f,unsigned g,unsigned h){return (void*)1;}
#endif
