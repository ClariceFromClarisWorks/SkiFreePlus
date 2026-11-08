#ifndef SKI_HUD_FORMAT_H
#define SKI_HUD_FORMAT_H
#include <stdint.h>
#include "sim.h"
/* Unsigned magnitude permits INT32_MIN without signed overflow. No division. */
static char *hud_u32_c(char *p,uint32_t n){
 static const uint32_t powers[]={1000000000u,100000000u,10000000u,1000000u,100000u,10000u,1000u,100u,10u,1u};
 unsigned i,started=0;for(i=0;i<10;i++){unsigned digit=0;while(n>=powers[i]){n-=powers[i];digit++;}if(digit||started||i==9){*p++=(char)('0'+digit);started=1;}}return p;
}
#ifndef HUD_HOST_TEST
extern char *hud_u32_asm(char *,uint32_t);
#else
#define hud_u32_asm hud_u32_c
#endif
static char *hud_number(char*p,int32_t value,int method){uint32_t n=(uint32_t)value;if(value<0){*p++='-';n=0u-n;}return method==2?hud_u32_asm(p,n):hud_u32_c(p,n);}
static char *hud_string(char*p,const char*s){while(*s)*p++=*s++;return p;}
static void hud_format(char*out,GameInfo g,int full,int method){
 char*p=out;const char*mode=g.mode==1?(full?"Slalom":"SS"):g.mode==2?(full?"Tree slalom":"TS"):g.mode==3?(full?"Freestyle":"FS"):g.resultMode?(full?"Finished":"Done"):(full?"Free ski":"Free");
 int32_t tenth=(g.elapsed/100)%10;if(tenth<0)tenth=-tenth;
 if(full){p=hud_number(p,g.distance,method);p=hud_string(p,"m  ");p=hud_number(p,g.speed,method);p=hud_string(p,"m/s  Style ");p=hud_number(p,g.style,method);p=hud_string(p,"  Time ");}
 else{p=hud_string(p,mode);*p++=' ';}
 p=hud_number(p,g.elapsed/1000,method);*p++='.';*p++=(char)('0'+tenth);
 if(full){p=hud_string(p,"s  ");p=hud_string(p,mode);}else{*p++='s';*p++=' ';p=hud_number(p,g.distance,method);p=hud_string(p,"m ");p=hud_number(p,g.style,method);p=hud_string(p,"pt");}*p=0;
}
#endif
