#include <stdio.h>
#include <stdlib.h>
#include "../sim.h"
#include "../dimensions.h"
void platform_fault(int line){fprintf(stderr,"FAULT %d\n",line);exit(1);}
int main(void){int view,lane,fast,seed,i;const int lanes[]={-256,-128,0,416,1000};puts("view,lane,fast,seed,steps,chair_frames,chair_items,clipped_chairs,all_items");
for(view=0;view<3;view++)for(lane=0;lane<5;lane++)for(fast=0;fast<2;fast++)for(seed=1;seed<=4;seed++){int w=view==0?304:view==1?480:512,h=view==2?342:297;unsigned cf=0,ci=0,clip=0,total=0;game_resize(w,h);game_init(seed,lanes[lane],0);if(fast)game_input(INPUT_FAST);
for(i=0;i<1500;i++){unsigned j,n,c=0;const DrawItem*d;if(i%20==0)game_input(INPUT_DOWN);game_step();d=game_items(&n);total+=n;for(j=0;j<n;j++)if(d[j].sprite>=64&&d[j].sprite<=67){int id=d[j].sprite;c++;if(d[j].x<0||d[j].y<0||d[j].x+dims[id][0]>w||d[j].y+dims[id][1]>h)clip++;}if(c)cf++;ci+=c;}
printf("%d,%d,%d,%d,1500,%u,%u,%u,%u\n",view,lanes[lane],fast,seed,cf,ci,clip,total);
}return 0;}
