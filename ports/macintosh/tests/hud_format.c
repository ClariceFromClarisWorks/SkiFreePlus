#define HUD_HOST_TEST 1
#include "../hud_format.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main(void){uint32_t v=1;int i,full,errors=0;for(i=0;i<10000;i++){GameInfo g={0};char a[160],b[160];v=v*1664525u+1013904223u;g.distance=(int32_t)v;g.speed=(int32_t)(v^0x80000000u);g.style=(int32_t)(v*19u);g.elapsed=(int32_t)(v*7u);g.mode=i%4;g.resultMode=i%2;
 for(full=0;full<2;full++){hud_format(a,g,full,1);if(full)sprintf(b,"%ldm  %ldm/s  Style %ld  Time %ld.%lds  %s",(long)g.distance,(long)g.speed,(long)g.style,(long)g.elapsed/1000,labs((long)g.elapsed/100)%10,g.mode==1?"Slalom":g.mode==2?"Tree slalom":g.mode==3?"Freestyle":g.resultMode?"Finished":"Free ski");else sprintf(b,"%s %ld.%lds %ldm %ldpt",g.mode==1?"SS":g.mode==2?"TS":g.mode==3?"FS":g.resultMode?"Done":"Free",(long)g.elapsed/1000,labs((long)g.elapsed/100)%10,(long)g.distance,(long)g.style);if(strcmp(a,b)){puts(a);puts(b);errors++;}}
 }printf("HUD_FORMAT,20000,%d\n",errors);return errors!=0;}
