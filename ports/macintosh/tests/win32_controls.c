#include <stdio.h>
#include <stdlib.h>
#include "../sim.c"
void platform_fault(int line){fprintf(stderr,"FAULT %d\n",line);exit(2);}
typedef uint32_t UINT;
#define max(a,b) ((a)>(b)?(a):(b))
#define min(a,b) ((a)<(b)?(a):(b))
#define __fastcall
#define ShowWindow(a,b) ((void)0)
enum{VK_ESCAPE=27,VK_F3=114,VK_RETURN=13,VK_F2=113};
enum{SDLK_LEFT=0x25,SDLK_RIGHT=0x27,SDLK_DOWN=0x28,SDLK_UP=0x26,SDLK_KP_7=0x67,SDLK_KP_9=0x69};
static void togglePausedState(void){}
static void handleGameReset(void){game_init(1,0,0);}
static short min_(short a,short b){return a<b?a:b;}
#include "original_win32_controls.h"
static void fixture(int frame,int air,int velocity){game_init(1,0,0);setActorFrameNo(playerActor,frame);playerActor->isInAir=air;playerActor->verticalVelocityMaybe=velocity;playerActor->HorizontalVelMaybe=0;playerActor->inAirCounter=0;redrawRequired=0;}
int main(void){int f,a,v,k,errors=0,count=0;int keys[]={INPUT_LEFT,INPUT_RIGHT,INPUT_DOWN,INPUT_UP,INPUT_W,INPUT_E,INPUT_SW,INPUT_SE,INPUT_KEYPAD_JUMP,INPUT_CLICK};int original[]={SDLK_LEFT,SDLK_RIGHT,SDLK_DOWN,SDLK_UP,SDLK_KP_7,SDLK_KP_9,0x61,99,0x60,0};
 for(f=0;f<22;f++)for(a=0;a<=2;a++)for(v=0;v<=16;v+=4)for(k=0;k<10;k++){
  int frame,dx,dy,jump;fixture(f,a,v);if(k==9)original_handleMouseClick();else original_handleKeydownMessage(original[k]);frame=playerActor->frameNo;dx=playerActor->HorizontalVelMaybe;dy=playerActor->verticalVelocityMaybe;jump=playerActor->inAirCounter;
  fixture(f,a,v);game_input(keys[k]);count++;
  if(frame!=playerActor->frameNo||dx!=playerActor->HorizontalVelMaybe||dy!=playerActor->verticalVelocityMaybe||jump!=playerActor->inAirCounter){if(errors++<12)printf("MISMATCH frame=%d air=%d speed=%d key=%d original=%d/%d/%d/%d port=%u/%d/%d/%d\n",f,a,v,k,frame,dx,dy,jump,playerActor->frameNo,playerActor->HorizontalVelMaybe,playerActor->verticalVelocityMaybe,playerActor->inAirCounter);}
 }
 for(a=0;a<=1;a++)for(v=-342;v<=342;v+=6)for(k=-512;k<=512;k+=8){int expected=a?original_getSkierInAirSpriteFromMousePosition(k,v):original_getSkierGroundSpriteFromMousePosition(k,v);fixture(0,a,8);game_mouse(k+skierScreenXOffset,v+skierScreenYOffset);count++;if(playerActor->frameNo!=(unsigned)expected)errors++;}
 printf("WIN32_CONTROLS,%d,%d\n",count,errors);return errors!=0;
}
