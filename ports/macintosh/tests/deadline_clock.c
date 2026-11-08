#include <assert.h>
#include <stdio.h>
#include "../deadline_clock.h"
int main(void){setbuf(stdout,NULL);DeadlineClock c;unsigned i,n;uint32_t epoch=0xfffffff0u,previous=epoch;deadline_start(&c,epoch);
for(i=1;i<=500000;i++){uint32_t expected=epoch+(uint32_t)(((uint64_t)i*1203+499)/500);assert(c.deadline==expected);assert(c.deadline-previous==2||c.deadline-previous==3);previous=c.deadline;deadline_presented(&c,expected);}assert(previous-epoch==1203000);
deadline_start(&c,0);n=deadline_steps(&c,20,8);assert(n==8&&c.deadline==20);deadline_presented(&c,20);assert(c.deadline==22);assert(deadline_target(&c,1)==22);
deadline_start(&c,0);deadline_presented(&c,20);assert(deadline_target(&c,1)==22);assert(deadline_target(&c,0)==5);n=deadline_steps(&c,20,8);assert(n==7);deadline_presented(&c,22);assert(deadline_target(&c,1)==24);
/* Equal work streams: normal, isolated stalls, repeated stalls. No 0/1-tick
   bursts in bounded mode, all simulation states traversed, no clock rebasing. */
for(n=0;n<3;n++){uint32_t now=0;unsigned updates=0,frames=0,merged=0;deadline_start(&c,now);while(updates<10000){unsigned count=deadline_steps(&c,now,8);uint32_t last=now,target;updates+=count;merged+=count-1;now+=1;if(n&&frames%37==18)now+=n==1?5:12;target=deadline_target(&c,1);if(deadline_before(now,target))now=target;assert(!frames||now-last>=2);deadline_presented(&c,now);frames++;}assert(updates==frames+merged);printf("scenario %u updates %u frames %u merged %u\n",n,updates,frames,merged);if(n==0)assert(merged==0);if(n==2)assert(merged>0);}
puts("DEADLINE_PASS: 500000 deadlines, 60.15Hz rational cadence, tick wrap, bounded catch-up, stalls, physics-step conservation");}
