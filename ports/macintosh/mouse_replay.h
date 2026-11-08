#ifndef SKI_MOUSE_REPLAY_H
#define SKI_MOUSE_REPLAY_H
#include "camera.h"
/* Deterministic mouse workload, shared with host reference. */
static void mouse_trace(int step,int scene,int w,int h,int*x,int*y,int*click){
 int phase=step%120,dx=0;*click=0;
 if(scene){int moving=phase;if(scene==2&&phase>=80&&phase<100)moving=80;dx=moving<60?-60+moving*2:180-moving*2;}
 *x=w/2+dx;*y=SKI_CAMERA_Y(h)+105+(scene==0?(step&1):0);
 if(scene==2&&(phase==80||phase==83||phase==86))*click=1;
}

#endif
