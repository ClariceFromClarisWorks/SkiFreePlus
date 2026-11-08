#include <assert.h>
#include <stdio.h>
#include "../frame_stats.h"
int main(void){FrameStats s={0};unsigned i;uint32_t start=0xfffffff0u;
 frame_stats_record(&s,start,1);for(i=1;i<=50;i++)frame_stats_record(&s,start+(i*12+4)/5,1);
 assert(s.fps10==250&&s.shownGaps==0&&s.shownCoalesced==0&&s.shownMax==3);
 frame_stats_reset(&s);frame_stats_record(&s,1000,1);for(i=1;i<=30;i++)frame_stats_record(&s,1000+i*4,2);
 assert(s.fps10==150&&s.shownGaps==30&&s.shownCoalesced==30&&s.shownMax==4);
 frame_stats_reset(&s);assert(!frame_stats_record(&s,50000,1));assert(!frame_stats_record(&s,50003,1));assert(s.frames==1&&s.gaps==0);
 /* Real drops and catch-up bursts must remain visible, never clamped to 25. */
 frame_stats_reset(&s);frame_stats_record(&s,0,1);for(i=1;i<=42;i++)frame_stats_record(&s,(i*120+41)/42,1);assert(s.fps10==210);
 frame_stats_reset(&s);frame_stats_record(&s,0,1);for(i=1;i<=66;i++)frame_stats_record(&s,(i*120+65)/66,1);assert(s.fps10==330&&s.shownBursts>0);
 puts("FRAME_STATS,PASS: 25Hz, stalls, coalesced steps, pause reset, tick rollover");return 0;}
