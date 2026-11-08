/* Deterministic whole-game controls, shared by host and Macintosh validation. */
static void replay_input(unsigned step,int scene){
 if(step%20==0)game_input(INPUT_DOWN);
 if(scene==2&&step%100==0)game_input(INPUT_JUMP);
 if(scene==5){
  if(step%120==40)game_input(INPUT_LEFT);
  if(step%120==46)game_input(INPUT_LEFT);
  if(step%120==70)game_input(INPUT_RIGHT);
  if(step%120==76)game_input(INPUT_UP);
  if(step%120==90)game_input(INPUT_JUMP);
  if(step==60||step==180)game_input(INPUT_FAST);
 }
 if(scene==6||scene==7){if(step%20==1||step%20==2)game_input(scene==6?INPUT_LEFT:INPUT_RIGHT);}
 if(scene==8){if(step%20==1||step%20==2)game_input((step/60)%2?INPUT_LEFT:INPUT_RIGHT);}
 if(scene==9){if(step%64==8||step%64==11||step%64==14)game_input(INPUT_JUMP);}
}

typedef struct {long x,y;unsigned left,right,diagonal,air,flip,land,fast,ramp,mound,maxObjects;int wasAir;} Coverage;
static Coverage coverage;
static void coverage_reset(void){GameInfo g=game_info();memset(&coverage,0,sizeof(coverage));coverage.x=g.x;coverage.y=g.y;}
static void coverage_step(void){GameInfo g=game_info();unsigned n,i;const DrawItem*d=game_items(&n);
 if(g.x<coverage.x)coverage.left++;if(g.x>coverage.x)coverage.right++;if(g.x!=coverage.x&&g.y!=coverage.y)coverage.diagonal++;
 if(g.air)coverage.air++;if(g.air&&g.frame>=18&&g.frame<=21)coverage.flip++;if(coverage.wasAir&&!g.air)coverage.land++;if(g.turbo)coverage.fast++;
 for(i=0;i<n;i++){if(d[i].sprite==52)coverage.ramp++;if(d[i].sprite==47||d[i].sprite==48||d[i].sprite==27)coverage.mound++;}
 if(n>coverage.maxObjects)coverage.maxObjects=n;coverage.x=g.x;coverage.y=g.y;coverage.wasAir=g.air;
}
static void coverage_line(char*s,int scene,int steps){sprintf(s,"COVERAGE,%d,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",scene,steps,coverage.left,coverage.right,coverage.diagonal,coverage.air,coverage.flip,coverage.land,coverage.fast,coverage.ramp,coverage.mound,coverage.maxObjects);}
