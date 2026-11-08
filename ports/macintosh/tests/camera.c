#include <stdio.h>
#include <stdlib.h>
#include "../sim.c"
void platform_fault(int line){fprintf(stderr,"FAULT %d\n",line);exit(1);}
#define CHECK(x) do { if(!(x)){fprintf(stderr,"camera line %d\n",__LINE__);exit(1);} } while(0)
int main(void){int sizes[][2]={{304,297},{480,297},{512,342},{240,160},{320,224}};unsigned v;int n;
 for(v=0;v<sizeof(sizes)/sizeof(sizes[0]);v++){
  int w=sizes[v][0],h=sizes[v][1];game_resize(w,h);w=512;h=342;game_init(1,1000,0);
  CHECK(skierScreenYOffset==SKI_CAMERA_Y(h));CHECK(skierScreenXOffset==w/2);
  CHECK(playerActor->someRect.bottom==SKI_CAMERA_Y(h));
  game_mouse(w/2,SKI_CAMERA_Y(h)+100);CHECK(playerActor->frameNo==0);
  for(n=0;n<300;n++){if(n%60==0)game_input(INPUT_CLICK);game_step();CHECK(skierScreenYOffset==SKI_CAMERA_Y(h));}
  {unsigned steps=game_info().steps;int x=playerX,y=playerY;
   game_resize(304,297);CHECK(playerX==x&&playerY==y);CHECK(game_info().steps==steps);
   CHECK(skierScreenYOffset==SKI_CAMERA_Y(342));}
 }
 game_resize(512,342);game_init(1,1000,0);game_set_camera(1);
 CHECK(game_camera_shift()==342/5-SKI_CAMERA_Y(342));
 {int ground,previous=game_camera_shift(),i;playerActor->yPosMaybe=playerY=31950;playerActor->verticalVelocityMaybe=16;
  for(i=0;i<100;i++){camera_step();CHECK(game_camera_shift()>=previous);CHECK(game_camera_shift()-previous<=2);previous=game_camera_shift();}
  CHECK(skierScreenYOffset+game_camera_shift()==171);
  CHECK(playerActor->verticalVelocityMaybe==16&&playerY==31950);
  playerActor->isInAir=154;CHECK(game_shadow(16,&ground)==-1);
  playerActor->isInAir=155;CHECK(game_shadow(16,&ground)==0&&ground==171);
  playerActor->isInAir=250;CHECK(game_shadow(16,&ground)==3&&ground==171);
  playerActor->isInAir=0;CHECK(game_shadow(16,&ground)==-1);
  game_init(1,1000,0);CHECK(game_camera_shift()==342/5-SKI_CAMERA_Y(342));
 }
 puts("CAMERA_PASS,5_resize_requests_fixed_compact_view,1500_updates,easing_and_shadow_boundaries");return 0;
}
