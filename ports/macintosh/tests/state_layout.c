#include <stdio.h>
#include <stdlib.h>
#include "../sim.c"
#include "../scale_replay.h"
void platform_fault(int line){fprintf(stderr,"FAULT %d\n",line);exit(1);}
int main(void){
#ifdef TEST_FAST_PAINT
game_set_fast_paint(1);
#endif
int seed,scene,scale,i;for(scale=0;scale<2;scale++)for(seed=1;seed<=8;seed++)for(scene=0;scene<9;scene++){uint32_t h=0;game_set_compact(scale);game_resize(512,342);game_init(seed,scene==1||scene==2?-128:scene==4?0:scene==6?-448:scene==7?416:scene==8?0:1000,scene==5?31200:0);if(scene==2)game_input(INPUT_FAST);scale_prev_x=scale_prev_y=-32768;for(i=0;i<1500;i++){Actor*a;unsigned j;scale_input(i,scene%6,512,342);game_step();for(a=actorListPtr;a;a=a->next)if(a->flags>255||a->frameNo>89||a->typeMaybe<0||a->typeMaybe>18)return 2;for(j=0;j<permObjectCount;j++)if(permObjects[j].actorFrameNo>89||permObjects[j].actorTypeMaybe<0||permObjects[j].actorTypeMaybe>18)return 3;h=hash_word(h,game_hash());}printf("TRACE,%d,%d,%d,%08x\n",scale,seed,scene,h);}return 0;}
