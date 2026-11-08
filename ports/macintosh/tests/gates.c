#include <stdio.h>
#include <stdlib.h>
#include "../sim.c"
void platform_fault(int line){fprintf(stderr,"FAULT %d\n",line);exit(2);}
int main(void){int mode,bad,side,tests=0;for(mode=1;mode<=2;mode++)for(bad=0;bad<2;bad++)for(side=23;side<=24;side++){
 int x=mode==1?-448:416,gx,gy;uint32_t before;PermObject*gate;game_start_course(mode);playerActor->xPosMaybe=x;playerActor->yPosMaybe=650;currentTickCount=1000;prevTickCount=960;if(mode==1)updateSsGameMode(playerActor,x,630);else updateGsGameMode(playerActor,x,630);gate=currentSlalomFlag;gate->spriteIdx=side;before=timedGameRelated;gx=gate->maybeX+((side==23)==bad?20:-20);gy=gate->maybeY;playerActor->xPosMaybe=gx;playerActor->yPosMaybe=gy+1;if(mode==1)updateSsGameMode(playerActor,gx,gy-1);else updateGsGameMode(playerActor,gx,gy-1);
 if(gate->spriteIdx!=(bad?26:25)||(uint32_t)timedGameRelated!=before-(bad?5000u:0u)){printf("GATE_FAIL,%d,%d,%d\n",mode,bad,side);return 1;}if(game_gate_feedback()!=(bad?2:1))return 2;if(!game_gate_flash())return 6;stepCount+=4;if(game_gate_flash())return 7;stepCount+=3;if(!game_gate_flash())return 8;stepCount+=4;if(game_gate_flash())return 9;stepCount+=13;if(!game_gate_feedback())return 3;stepCount++;if(game_gate_feedback())return 4;game_start_course(mode);if(game_gate_feedback())return 5;tests++;
 }printf("GATES,%d,PASS\n",tests);return 0;}
