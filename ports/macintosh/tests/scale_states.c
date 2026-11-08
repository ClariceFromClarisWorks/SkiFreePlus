#include <stdio.h>
#include <stdlib.h>
#include "../sim.h"
#include "../scale_replay.h"
void platform_fault(int line){fprintf(stderr,"FAULT %d\n",line);exit(1);}
int main(void){int compact,view,scene,i;for(compact=0;compact<2;compact++)for(view=0;view<3;view++)for(scene=0;scene<6;scene++){int w=view==0?304:view==1?480:512,h=view==2?342:297;game_set_compact(compact);game_resize(w,h);scale_start(scene);for(i=0;i<300;i++){scale_input(i,scene,w,h);game_step();}printf("SCALE_STATE,%d,%d,%d,%08lx\n",compact,view,scene,(unsigned long)game_hash());}return 0;}
