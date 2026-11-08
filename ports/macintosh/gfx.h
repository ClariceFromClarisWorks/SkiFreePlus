#ifndef SKI_GFX_H
#define SKI_GFX_H
#include <stdint.h>
void gfx_init(uint8_t* screen);
void gfx_frame(int mode);
uint8_t* gfx_target(void);
void gfx_dirty(int x,int y,int w,int h);
void gfx_present(int mode);
int gfx_present_verified(void);
int gfx_verify(int mode);
int gfx_guards(void);
#endif

void gfx_erase_next(int x,int y,int w,int h);

void gfx_invalidate(void);
void gfx_composed(void);

void gfx_regions(int width,int height,void(*present)(int,int,int,int));

int gfx_phase_test(void);
unsigned gfx_phase_bench(int mode);
int gfx_compiled_test(void);

int gfx_composition_check(void);

void gfx_set_compact(int enabled);

int gfx_intersects(int x,int y,int w,int h);

void gfx_air_inset(int pose,int mode);
int gfx_inset_test(void);
unsigned gfx_inset_bench(int mode);

unsigned gfx_inset_build_bench(int mode);
