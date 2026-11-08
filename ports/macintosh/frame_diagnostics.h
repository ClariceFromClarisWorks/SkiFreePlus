#ifndef SKI_FRAME_DIAGNOSTICS_H
#define SKI_FRAME_DIAGNOSTICS_H
#include <stdint.h>
#include <string.h>
typedef struct {uint32_t frames,intervals,ticks,last,gap[7],maxGap,debtFrames,maxDebt;unsigned active,ready;} FrameDiagnostics;
static void diagnostic_start(FrameDiagnostics*d){memset(d,0,sizeof(*d));d->active=1;}
static void diagnostic_break(FrameDiagnostics*d){d->ready=0;}
static void diagnostic_frame(FrameDiagnostics*d,uint32_t now,uint32_t debt){uint32_t gap;if(!d->active)return;d->frames++;if(debt){d->debtFrames++;if(debt>d->maxDebt)d->maxDebt=debt;}if(d->ready){gap=now-d->last;d->intervals++;d->ticks+=gap;d->gap[gap<6?gap:6]++;if(gap>d->maxGap)d->maxGap=gap;}d->last=now;d->ready=1;}
static unsigned diagnostic_fps10(const FrameDiagnostics*d){return d->ticks?(unsigned)((d->intervals*600u+d->ticks/2)/d->ticks):0;}
#endif
