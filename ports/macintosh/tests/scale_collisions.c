#include <stdio.h>
#include <stdlib.h>
#include "../sim.c"
#include "../scale_replay.h"
void platform_fault(int line){fprintf(stderr,"FAULT %d\n",line);exit(1);}
static uint32_t world_hash(void){uint32_t h=2166136261u;Actor*a;unsigned i;
#define H(v) h=hash_word(h,(uint32_t)(v))
 H(randomState);H(playerX);H(playerY);H(stylePoints);H(elapsedTime);H(stepCount);H(playerActor?playerActor-actors:-1);H(isSsGameMode);H(isGsGameMode);H(isFsGameMode);
 for(a=actorListPtr;a;a=a->next){H(a-actors);H(a->typeMaybe);H(a->spriteIdx2);H(a->xPosMaybe);H(a->yPosMaybe);H(a->HorizontalVelMaybe);H(a->verticalVelocityMaybe);H(a->isInAir);H(a->frameNo);H(a->flags);}
 for(i=0;i<permObjectCount;i++){PermObject*p=&permObjects[i];H(p->actorTypeMaybe);H(p->maybeX);H(p->maybeY);H(p->actorFrameNo);H(p->unk_0x18);H(p->unk_0x1e);H(p->unk_0x20);}return h;
#undef H
}
int main(void){static uint32_t reference[300];int compact,kind,dx,i,checks=0;int ids[]={49,51,45,47,48,52,23,24};
 for(kind=0;kind<8;kind++)for(dx=-48;dx<=48;dx++)for(compact=0;compact<2;compact++){
  Actor*a;int type=kind<2?13:kind==2?14:kind<5?15:kind==5?16:12;
  game_set_compact(compact);game_resize(compact?360:480,compact?240:320);game_init(1,5000,0);game_input(INPUT_DOWN);
  a=addActorOfTypeWithSpriteIdx(type,ids[kind]);if(!a)return 2;updateActorPositionMaybe(a,playerX+dx,playerY+80,0);capture_frame();
  for(i=0;i<80;i++){uint32_t h;game_step();h=world_hash();if(!compact)reference[i]=h;else {if(reference[i]!=h){fprintf(stderr,"DIFF kind%d dx%d step%d\n",kind,dx,i);return 1;}checks++;}}
 }
 for(kind=0;kind<6;kind++)for(compact=0;compact<2;compact++){
  game_set_compact(compact);game_resize(compact?360:480,compact?240:320);scale_start(kind);
  for(i=0;i<300;i++){uint32_t h;/* Keyboard controls isolate world physics from pointer quantisation. */
   if(i%20==0)game_input(INPUT_DOWN);if(i%64==8||i%64==11)game_input(INPUT_CLICK);if(i%100==50)game_input(INPUT_LEFT);game_step();h=world_hash();if(!compact)reference[i]=h;else{if(reference[i]!=h)return 3;checks++;}}
 }
 printf("COLLISION_WORLD_PASS,%d_checkpoints,8_hazards_97_offsets,6_natural_scenes\n",checks);return 0;
}
