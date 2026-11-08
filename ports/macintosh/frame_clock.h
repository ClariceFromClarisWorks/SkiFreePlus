#ifndef FRAME_CLOCK_H
#define FRAME_CLOCK_H
#include <Timer.h>
#include <stddef.h>
#pragma pack(push,2)
typedef struct {TMTask task;volatile uint32_t*counter;int32_t period;} SkiTimer;
#pragma pack(pop)
typedef char TimerOffsetCheck[offsetof(SkiTimer,counter)==22 && offsetof(SkiTimer,period)==26?1:-1];
extern unsigned char ski_timer_begin[],ski_timer_end[];
static unsigned char timerCode[128] __attribute__((aligned(2)));
static struct {volatile uint32_t fired;} frameTimer;static SkiTimer phaseTimers[16];static int timerPhases=8;static TMTask measureTimer;static int timerRunning,measureRunning,timerCodeReady;
static void timer_code_init(void){if(!timerCodeReady){unsigned n=(unsigned)(ski_timer_end-ski_timer_begin);memcpy(timerCode,ski_timer_begin,n);timerCode[n]=0x4e;timerCode[n+1]=0x75;timerCodeReady=1;}}
static void timer_clock_stop(void){if(timerRunning){int i;for(i=0;i<timerRunning;i++)RmvTime((QElemPtr)&phaseTimers[i]);timerRunning=0;}}
static void timer_clock_start(void){int i;timer_clock_stop();timer_code_init();frameTimer.fired=0;memset(phaseTimers,0,sizeof(phaseTimers));for(i=0;i<timerPhases;i++){phaseTimers[i].task.tmAddr=(ProcPtr)timerCode;phaseTimers[i].counter=&frameTimer.fired;phaseTimers[i].period=-40000*timerPhases;InsTime((QElemPtr)&phaseTimers[i]);}timerRunning=timerPhases;for(i=0;i<timerPhases;i++)PrimeTime((QElemPtr)&phaseTimers[i],-40000*(i+1));}
static void timer_measure_start(void){if(measureRunning)RmvTime((QElemPtr)&measureTimer);timer_code_init();memset(&measureTimer,0,sizeof(measureTimer));measureTimer.tmAddr=(ProcPtr)(timerCode+(ski_timer_end-ski_timer_begin));InsTime((QElemPtr)&measureTimer);measureRunning=1;PrimeTime((QElemPtr)&measureTimer,-600000000);}
static unsigned long timer_measure_stop(void){long left;if(!measureRunning)return 0;RmvTime((QElemPtr)&measureTimer);measureRunning=0;left=measureTimer.tmCount;return left<0?600000000u+left:600000000u-(unsigned long)left*1000u;}
/* Independent elapsed-time sampling for the on-screen FPS display. It is not
   derived from the scheduled timer rate or from the number of physics steps. */
static TMTask fpsTimer;static int fpsTimerRunning;
static void fps_clock_stop(void){if(fpsTimerRunning){RmvTime((QElemPtr)&fpsTimer);fpsTimerRunning=0;}}
static void fps_clock_start(void){fps_clock_stop();timer_code_init();memset(&fpsTimer,0,sizeof(fpsTimer));fpsTimer.tmAddr=(ProcPtr)(timerCode+(ski_timer_end-ski_timer_begin));InsTime((QElemPtr)&fpsTimer);fpsTimerRunning=1;PrimeTime((QElemPtr)&fpsTimer,-600000000);}
static unsigned long fps_clock_sample(void){long left;unsigned long elapsed;fps_clock_stop();left=fpsTimer.tmCount;elapsed=left<0?600000000u+left:600000000u-(unsigned long)left*1000u;fps_clock_start();return elapsed;}
#endif
