#ifndef SKI_PACING_H
#define SKI_PACING_H
/* Shared by live play and the presentation diagnostic. TickCount is nominal 60 Hz. */
static int game_pump(int dt,int*accum){int updated=0;if(dt>12)dt=12;*accum+=dt*5;while(*accum>=12){game_step();*accum-=12;updated++;}return updated;}
typedef struct {uint32_t deadline;int phase;} PresentClock;
static void present_clock_start(PresentClock*c,uint32_t now){c->deadline=now;c->phase=4;}
static void present_clock_advance(PresentClock*c,int updates){c->phase+=updates*12;while(c->phase>=5){c->deadline++;c->phase-=5;}}
#endif
