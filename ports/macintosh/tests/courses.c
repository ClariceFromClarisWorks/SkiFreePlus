#include <stdio.h>
#include <stdlib.h>
#include "../sim.h"
void platform_fault(int line){fprintf(stderr,"FAULT %d\n",line);exit(2);}
int main(void){int e=game_course_test();printf("COURSE_CHECK,%d\n",e);return e!=0;}
