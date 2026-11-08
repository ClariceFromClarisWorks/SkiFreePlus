#ifndef SCALE_REPLAY_H
#define SCALE_REPLAY_H
#include "mouse_replay.h"
static int scale_prev_x,scale_prev_y;
static int scale_scene_mouse(int scene){return scene==3?1:scene==4?2:scene==6?3:0;}
static void scale_start(int scene){scale_prev_x=scale_prev_y=-32768;game_init(1,scene==1||scene==2?-128:scene==4?0:1000,scene==5?31200:0);if(scene==2)game_input(INPUT_FAST);}
static void scale_input(int step,int scene,int w,int h){int x,y,click;
mouse_trace(step,scale_scene_mouse(scene),w,h,&x,&y,&click);if(x!=scale_prev_x||y!=scale_prev_y)game_mouse(x,y+game_camera_shift());scale_prev_x=x;scale_prev_y=y;if(click)game_input(INPUT_CLICK);
#if AUTOTEST
if(scene==3&&step%30==0)game_test_gate((step/30)&1);
if(scene==6&&step%100==0)game_test_jump();
#endif
}
#endif
