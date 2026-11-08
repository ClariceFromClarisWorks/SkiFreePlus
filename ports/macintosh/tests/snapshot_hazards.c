#define main trace_main
#include "snapshot.c"
#undef main
int main(int argc,char**argv){int kind,dx,i;int ids[]={49,51,45,47,48,52,23,24};game_set_snapshot_actors(argc>1?atoi(argv[1]):0);game_set_compact(0);game_resize(512,342);for(kind=0;kind<8;kind++)for(dx=-48;dx<=48;dx++){Actor*a;uint32_t states=0,images=0;int type=kind<2?13:kind==2?14:kind<5?15:kind==5?16:12;game_init(1,5000,0);game_input(INPUT_DOWN);a=addActorOfTypeWithSpriteIdx(type,ids[kind]);if(!a)return 2;updateActorPositionMaybe(a,playerX+dx,playerY+80,0);capture_frame();for(i=0;i<80;i++){game_step();states=hash_word(states,state());images=hash_word(images,pixels(0));}printf("HAZARD,%d,%d,%08x,%08x\n",kind,dx,states,images);}if(game_course_test())return 3;return 0;}
