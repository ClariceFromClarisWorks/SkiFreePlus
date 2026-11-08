#ifndef FRAME_STATS_H
#define FRAME_STATS_H
#include <stdint.h>
typedef struct {uint32_t start,last,frames,gaps,coalesced,maxGap,bursts;unsigned ready,fps10,shownGaps,shownCoalesced,shownMax,shownBursts;} FrameStats;
static void frame_stats_reset(FrameStats*s){s->ready=0;}
/* Count completed presentations, never simulation steps or paused redraws.
   TickCount wraps; unsigned differences preserve short intervals across wrap. */
static int frame_stats_record(FrameStats*s,uint32_t now,unsigned updates){uint32_t gap,elapsed;
 if(!s->ready){s->ready=1;s->start=s->last=now;s->frames=s->gaps=s->coalesced=s->maxGap=s->bursts=0;return 0;}
 gap=now-s->last;s->last=now;s->frames++;if(gap<2)s->bursts++;if(gap>3)s->gaps++;if(gap>s->maxGap)s->maxGap=gap;if(updates>1)s->coalesced+=updates-1;
 elapsed=now-s->start;if(elapsed<120)return 0;
 s->fps10=(unsigned)((s->frames*600u+elapsed/2)/elapsed);s->shownGaps=(unsigned)(s->gaps>999?999:s->gaps);s->shownCoalesced=(unsigned)(s->coalesced>999?999:s->coalesced);s->shownBursts=(unsigned)(s->bursts>999?999:s->bursts);s->shownMax=(unsigned)(s->maxGap>999?999:s->maxGap);
 s->start=now;s->frames=s->gaps=s->coalesced=s->maxGap=s->bursts=0;return 1;
}
#endif
