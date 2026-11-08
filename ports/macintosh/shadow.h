#ifndef SKI_SHADOW_H
#define SKI_SHADOW_H
/* Offline-drawn 1-bit ovals: no per-frame ellipse, scaling or lighting work.
   Levels progress from near/strong to high/faint. The marker is ground-fixed. */
static const uint32_t shadowRows[4][5]={
 {0x000aa800u,0x00555400u,0x00aaaa00u,0x00555400u,0x000aa800u},
 {0x000aa000u,0x00155000u,0x002aa800u,0x00155000u,0x000aa000u},
 {0x00000000u,0x00055000u,0x000aa800u,0x00055000u,0x00000000u},
 {0x00000000u,0x00044000u,0x00022000u,0x00010000u,0x00000000u}
};
extern void shadow_stamp_asm(unsigned char*,const uint32_t*);
static __attribute__((noinline)) void shadow_stamp_c(unsigned char*dst,const uint32_t*rows){int y,x;for(y=0;y<5;y++)for(x=0;x<4;x++)dst[y*64+x]|=(unsigned char)(rows[y]>>(24-x*8));}
static unsigned shadowFrames,shadowLevels;
static void shadow_overlay(void){int ground,level;if(!(perfKernel&524288)||!fullscreen)return;
 level=game_shadow((perfKernel&65536)?16:0,&ground);if(level<0||ground<2||ground>339)return;
 #if AUTOTEST
 shadowFrames++;shadowLevels|=1u<<level;
#endif
 if(gfxMode==2)shadow_stamp_asm(gfx_target()+(ground-2)*64+30,shadowRows[level]);else shadow_stamp_c(gfx_target()+(ground-2)*64+30,shadowRows[level]);
 gfx_dirty(240,ground-2,32,5);gfx_erase_next(240,ground-2,32,5);
}
#endif
