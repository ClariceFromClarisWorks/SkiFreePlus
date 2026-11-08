#define main trace_main
#include "snapshot.c"
#undef main
int main(int argc,char**argv){int i;game_set_snapshot_actors(atoi(argv[1]));game_set_compact(1);game_resize(512,342);game_init(1,-128,0);game_input(INPUT_FAST);scale_prev_x=scale_prev_y=-32768;for(i=0;i<1100;i++){unsigned n,j;const DrawItem*d;scale_input(i,2,512,342);game_step();printf("F,%d,%08x,%08x\n",i,state(),pixels(1));if(argc>2&&i==atoi(argv[2])){d=game_items(&n);for(j=0;j<n;j++)printf("D,%u,%d,%d,%d\n",j,d[j].sprite,d[j].x,d[j].y);}}return 0;}
