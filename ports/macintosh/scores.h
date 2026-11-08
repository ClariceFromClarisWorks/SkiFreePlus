#ifndef SKI_SCORES_H
#define SKI_SCORES_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {int count[3]; int32_t value[3][10];} SkiScores;
static int score_insert(SkiScores*s,int course,int32_t value){int c=course-1,i,j,n;if(c<0||c>2||(c!=2&&value<0))return -1;n=s->count[c];for(i=0;i<n;i++)if(c==2?value>s->value[c][i]:value<s->value[c][i])break;if(i==10)return -1;if(n<10)s->count[c]=++n;for(j=n-1;j>i;j--)s->value[c][j]=s->value[c][j-1];s->value[c][i]=value;return i;}
/* Same text file format; integer-only I/O avoids linking scanf's float parser. */
static int score_token(FILE*f,char*out,unsigned cap){int ch;unsigned n=0;do{ch=fgetc(f);}while(ch==' '||ch=='\n'||ch=='\r'||ch=='\t');if(ch==EOF)return 0;do{if(n+1>=cap)return 0;out[n++]=(char)ch;ch=fgetc(f);}while(ch!=EOF&&ch!=' '&&ch!='\n'&&ch!='\r'&&ch!='\t');out[n]=0;return 1;}
static int score_getnum(FILE*f,int32_t*out){char b[16];unsigned i=0,negative=0;uint32_t n=0,limit;if(!score_token(f,b,sizeof(b)))return 0;if(b[i]=='-'||b[i]=='+'){negative=b[i]=='-';i++;}if(!b[i])return 0;limit=negative?2147483648u:2147483647u;for(;b[i];i++){unsigned d=(unsigned)(b[i]-'0');if(d>9||n>limit/10||(n==limit/10&&d>limit%10))return 0;n=n*10+d;}*out=(int32_t)(negative?0u-n:n);return 1;}
static void score_putnum(FILE*f,int32_t v){char b[12];unsigned n=0;uint32_t u=(uint32_t)v;if(v<0){fputc('-',f);u=0u-u;}do{b[n++]=(char)('0'+u%10);u/=10;}while(u);while(n)fputc(b[--n],f);}
static int scores_read(SkiScores*s,const char*path){FILE*f;SkiScores v;char magic[32];int c,i;int32_t x;memset(s,0,sizeof(*s));memset(&v,0,sizeof(v));f=fopen(path,"r");if(!f)return 0;if(!score_token(f,magic,sizeof(magic))||strcmp(magic,"SkiFreeScores1"))goto bad;for(c=0;c<3;c++){if(!score_getnum(f,&x)||x<0||x>10)goto bad;v.count[c]=x;for(i=0;i<v.count[c];i++){if(!score_getnum(f,&x)||(c!=2&&x<0))goto bad;v.value[c][i]=x;if(i&&(c==2?v.value[c][i]>v.value[c][i-1]:v.value[c][i]<v.value[c][i-1]))goto bad;}}fclose(f);*s=v;return 1;bad:fclose(f);return -1;}
static int scores_write(const SkiScores*s,const char*path){FILE*f=fopen(path,"w");int c,i,ok;if(!f)return 0;fputs("SkiFreeScores1\n",f);for(c=0;c<3;c++){score_putnum(f,s->count[c]);for(i=0;i<s->count[c];i++){fputc(' ',f);score_putnum(f,s->value[c][i]);}fputc('\n',f);}ok=!ferror(f);if(fclose(f))ok=0;return ok;}
#endif
