#include <stdio.h>
#include <stdlib.h>
#include "../sim.c"
void platform_fault(int line){fprintf(stderr,"FAULT %d\n",line);exit(2);}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"INSET line %d\n",__LINE__);exit(1);}}while(0)
int main(void){int cam,frame,air,n=0;for(cam=0;cam<2;cam++){
 game_init(1,1000,0);game_set_camera(1);if(cam)cameraOffset=171-skierScreenYOffset;
 for(frame=0;frame<22;frame++)for(air=0;air<=200;air++){
  RECT r;uint32_t before;int pose;setActorFrameNo(playerActor,frame);playerActor->isInAir=air;
  updateRectForSpriteAtLocation(&r,playerActor->spritePtr,playerActor->xPosMaybe,playerActor->yPosMaybe,air);
  before=game_hash();pose=game_air_pose(16);CHECK(before==game_hash());
  CHECK((pose!=0)==(air>0&&r.bottom+cameraOffset<=16));if(pose)CHECK(pose==playerActor->spriteIdx2);n++;
 }
}
{int i;const int frames[]={13,18,19,13};game_init(1,1000,0);game_set_camera(1);playerActor->isInAir=180;setActorFrameNo(playerActor,13);for(i=0;i<4;i++){CHECK((int)playerActor->frameNo==frames[i]);CHECK(game_air_pose(16)==playerActor->spriteIdx2);CHECK(game_air_pose(16)==frames[i]+1);if(i<3)game_input(INPUT_CLICK);}printf("JUMPCAM_CLICK,13_to_18_to_19_to_13,live_pose,PASS\n");}
printf("AIR_INSET,%d,boundaries_all_poses_camera_positions,no_state_changes,PASS\n",n);return 0;}
