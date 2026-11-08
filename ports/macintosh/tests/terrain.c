/* Controlled host-side terrain encounters; no input-driven jumps. */
#include <stdio.h>
#include <stdlib.h>
#include "../sim.c"
void platform_fault(int line){fprintf(stderr,"FAULT %d\n",line);exit(2);}
int main(void){int ids[]={0,47,48,52},j,i;for(j=0;j<4;j++){
 Actor*a;int seen=0,air=0,landing=0,maxAir=0,wasAir=0;game_init(1,5000,0);game_input(INPUT_DOWN);
 if(ids[j]){a=addActorOfTypeWithSpriteIdx(ids[j]==52?ACTOR_TYPE_16_JUMP:ACTOR_TYPE_15_BUMP,ids[j]);if(!a)return 2;
 updateActorPositionMaybe(a,playerX,playerY+80,0);capture_frame();}
 for(i=0;i<80;i++){unsigned n,k;const DrawItem*d=game_items(&n);for(k=0;k<n;k++)if(d[k].sprite==ids[j])seen++;game_step();if(playerActor->isInAir){air++;if(playerActor->isInAir>maxAir)maxAir=playerActor->isInAir;}else if(wasAir)landing++;wasAir=playerActor->isInAir;}
 printf("TERRAIN,%d,seen,%d,air_steps,%d,max_height,%d,landings,%d,style,%ld\n",ids[j],seen,air,maxAir,landing,(long)stylePoints);if(ids[j]&&(!seen||!air||!landing))return 1;if(!ids[j]&&(air||landing))return 1;
 }return 0;}
