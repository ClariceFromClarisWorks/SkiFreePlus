#include <stdio.h>
#include <stdlib.h>
#include "../sim.h"
#include "../mouse_replay.h"
void platform_fault(int line){fprintf(stderr,"FAULT %d\n",line);exit(1);}
int main(void){int v,s,i;for(v=0;v<3;v++){int w=v==0?304:v==1?480:512,h=v==2?342:297;game_resize(w,h);for(s=0;s<3;s++){int px=-32768,py=-32768;game_init(1,1000,0);for(i=0;i<300;i++){int x,y,c;mouse_trace(i,s,w,h,&x,&y,&c);if(x!=px||y!=py)game_mouse(x,y);if(c)game_input(INPUT_CLICK);px=x;py=y;game_step();}printf("MOUSE_STATE,%d,%d,%08lx\n",v,s,(unsigned long)game_hash());}}return 0;}
