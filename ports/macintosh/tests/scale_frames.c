#include <stdio.h>
#include <stdlib.h>
#include "../sim.h"
#include "../scale_replay.h"
void platform_fault(int line){fprintf(stderr,"FAULT %d\n",line);exit(1);}
int main(void){int c,s,i;for(c=0;c<2;c++)for(s=0;s<6;s++){game_set_compact(c);game_resize(512,342);scale_start(s);for(i=0;i<=300;i++){if(i%50==0){unsigned n,k;const DrawItem*d=game_items(&n);printf("FRAME,%d,%d,%d,%u\n",c,s,i,n);for(k=0;k<n;k++)printf("%d,%d,%d\n",d[k].sprite,d[k].x,d[k].y);}scale_input(i,s,512,342);game_step();}}return 0;}
