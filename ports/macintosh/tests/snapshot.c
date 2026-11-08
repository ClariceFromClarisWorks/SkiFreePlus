#include <stdio.h>
#include <stdlib.h>
#include "../sim.c"
#include "../scale_replay.h"
typedef uint8_t U8;typedef uint16_t U16;typedef void(*BlitFn)(U8*,long);
typedef struct{int w,h,words;const U16*mask,*bits;const U16**phases;BlitFn*compiled;}GfxSprite;
static BlitFn bankFunctions[90][16];
#define art original_art
#include "../bank_assets.h"
#undef art
#include "../bank_compact_assets.h"
void platform_fault(int line){fprintf(stderr,"FAULT %d\n",line);exit(1);}
static uint32_t state(void){uint32_t h=0;Actor*a;unsigned j;
#define H(v) h=hash_word(h,(uint32_t)(v))
 H(randomState);H(playerX);H(playerY);H(stylePoints);H(elapsedTime);H(stepCount);H(playerActor!=NULL);H(isSsGameMode);H(isGsGameMode);H(isFsGameMode);H(INT_0040c964);H(INT_0040c960);H(INT_0040c968);
 for(a=actorListPtr;a;a=a->next)if(!(a->flags&FLAG_2)){H(a->typeMaybe);H(a->spriteIdx2);H(a->frameNo);H(a->xPosMaybe);H(a->yPosMaybe);H(a->isInAir);H(a->HorizontalVelMaybe);H(a->verticalVelocityMaybe);H(a->inAirCounter);H(a->flags&(FLAG_8|FLAG_20|FLAG_40));H(a->permObject?a->permObject-permObjects:-1);}
 H(permObjectCount);for(j=0;j<permObjectCount;j++){PermObject*p=&permObjects[j];H(p->actorTypeMaybe);H(p->actorFrameNo);H(p->spriteIdx);H(p->maybeX);H(p->maybeY);H(p->xVelocity);H(p->yVelocity);H(p->unk_0x18);H(p->unk_0x1e);H(p->unk_0x20);}
#undef H
 return h;}
static uint32_t pixels(int small){static U8 buffer[21888];GfxSprite*art=small?small_art:original_art;unsigned i,n;const DrawItem*d=game_items(&n);uint32_t hash=0;memset(buffer,0,sizeof(buffer));for(i=0;i<n;i++){GfxSprite*s=&art[d[i].sprite];int x,y;for(y=0;y<s->h;y++)for(x=0;x<s->w;x++){int xx=x+d[i].x,yy=y+d[i].y;U16 bit=0x8000u>>(x&15);if(xx>=0&&xx<512&&yy>=0&&yy<342&&(s->mask[y*s->words+(x>>4)]&bit)){U8 b=0x80u>>(xx&7);if(s->bits[y*s->words+(x>>4)]&bit)buffer[yy*64+(xx>>3)]|=b;else buffer[yy*64+(xx>>3)]&=~b;}}}for(i=0;i<sizeof(buffer);i++)hash=hash_word(hash,buffer[i]);return hash;}
int main(int argc,char**argv){int small,seed,scene,i;game_set_snapshot_actors(argc>1?atoi(argv[1]):0);game_set_camera(argc>2?atoi(argv[2]):0);for(small=0;small<2;small++)for(seed=1;seed<=8;seed++)for(scene=0;scene<9;scene++){uint32_t states=0,images=0;game_set_compact(small);game_resize(512,342);game_init(seed,scene==1||scene==2?-128:scene==4?0:scene==6?-448:scene==7?416:scene==8?0:1000,scene==5?31200:0);if(scene==2)game_input(INPUT_FAST);scale_prev_x=scale_prev_y=-32768;for(i=0;i<1500;i++){scale_input(i,scene%6,512,342);game_step();states=hash_word(states,state());images=hash_word(images,pixels(small));if(i%100==99)printf("SNAPSHOT,%d,%d,%d,%d,%08x,%08x\n",small,seed,scene,i,states,images);}}return 0;}
