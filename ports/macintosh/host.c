#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim.h"
#include "replay_input.h"
void platform_fault(int line){fprintf(stderr,"SIM_FAULT,%d\n",line);exit(2);}
int main(void){int sc,i;const int xs[]={1000,-448,0,416,1000,1000,1000,1000,1000,0},ys[]={0,0,0,0,31200,0,0,0,0,0};for(sc=0;sc<10;sc++){game_init(1,xs[sc],ys[sc]);coverage_reset();for(i=0;i<1500;i++){replay_input(i,sc);game_step();coverage_step();if(i==299||i==1499){char cov[220];coverage_line(cov,sc,i+1);printf("%s",cov);}if((i+1)%100==0){GameInfo g=game_info();printf("STATE,%d,%d,%08lx,%ld,%ld,%u,%d\n",sc,i+1,(unsigned long)game_hash(),(long)g.x,(long)g.y,g.objects,g.alive);}}}game_init(1,1000,0);for(i=0;i<2250;i++){replay_input(i,0);game_step();}printf("SOAK_STATE,%08lx\n",(unsigned long)game_hash());return 0;}
