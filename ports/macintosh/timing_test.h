/* Fullscreen OS/input path, unchanged renderer, matched fixed-step routes.
   Tick busy-waits below are deliberately injected stalls, not measured CPU
   cost estimates. Serial/disk output occurs only after each timed run. */
static void timing_test(void){int mode,scene,stress,i;char line[256];
 {int e=gfx_inset_test();if(e)platform_fault(9300+e);send("INSET27,22_poses,2x_pixels,C_ASM,guards,PASS\n");for(i=0;i<2;i++){unsigned long t=TickCount();unsigned sum=gfx_inset_bench(i);sprintf(line,"INSET27_COPY,%d,5000,%lu,%u\n",i,(unsigned long)(TickCount()-t),sum);send(line);}}
 {for(i=0;i<2;i++){unsigned long t=TickCount();unsigned sum=gfx_inset_build_bench(i);sprintf(line,"INSET27_BUILD,%d,220,%lu,%u\n",i,(unsigned long)(TickCount()-t),sum);send(line);}}
 send("SKIGAME_START\nTIMING26,baseline8=0,absolute=1,bounded=2,tick_hz=60.15\n");
 perfKernel=33536251;compact_view(0);send("RENDERER,build26_world,features27_overlays,per_route_framebuffer_checks\n");
 wmTest=cursorTest=inputTest=mouseTest=1;fullscreen=0;ShowCursor();cursorHidden=0;toggle_display();gfxMode=2;
 for(stress=0;stress<2;stress++)for(scene=0;scene<7;scene++)for(i=0;i<3;i++){
  DeadlineClock clock;unsigned frames=0,updates=0,merged=0,maxCount=0,workMax=0,workLong=0,late=0,maxLate=0;unsigned long begin,end,us;uint32_t hash;FrameDiagnostics d;
  mode=(i+scene)%3;perfEnabled=1;frame_stats_reset(&frameStats);fps_diagnostic_reset();perfRefresh=1;
  scale_start(scene);gfx_invalidate();rendered=0;render();paused=backgrounded=0;mouseScene=scale_scene_mouse(scene);mouseApplied=-1;previousMouse.h=previousMouse.v=-32768;
  diagnostic_start(&d);timer_measure_start();begin=TickCount();deadline_start(&clock,begin);if(mode==0)timer_clock_start();
  while(updates<300){unsigned count=1,j;uint32_t workStart=(uint32_t)TickCount(),workEnd,now,target;
   if(mode==2)count=deadline_steps(&clock,workStart,300-updates<8?300-updates:8);
   for(j=0;j<count;j++){mouseStep=updates+j;input();game_step();}updates+=count;merged+=count-1;if(count>maxCount)maxCount=count;compose();workEnd=TickCount();if(workEnd-workStart>workMax)workMax=workEnd-workStart;if(workEnd-workStart>2)workLong++;
   if(stress&&frames%47==23){uint32_t until=(uint32_t)TickCount()+5;while(deadline_before((uint32_t)TickCount(),until)){};}
   if(mode==0){while(frameTimer.fired<updates){};}else{target=deadline_target(&clock,mode==2);while(deadline_before((uint32_t)TickCount(),target)){};}
   present();now=(uint32_t)TickCount();
   if(mode==0){unsigned debt=frameTimer.fired>updates?frameTimer.fired-updates:0;diagnostic_frame(&d,now,debt);}else{unsigned delay=deadline_before(clock.deadline,now)?now-clock.deadline:0;if(delay){late++;if(delay>maxLate)maxLate=delay;}deadline_presented(&clock,now);diagnostic_frame(&d,now,0);}
   if(record_frame(now,count))perfRefresh=1;frames++;
  }
  end=TickCount();timer_clock_stop();us=timer_measure_stop();hash=game_hash();
  if(!gfx_guards()||!gfx_verify(2))platform_fault(9110+scene);
  {unsigned step;scale_start(scene);for(step=0;step<300;step++){scale_input(step,scene,512,342);game_step();}if(game_hash()!=hash){sprintf(line,"STATE_MISMATCH,%d,%d,%d,%08lx,%08lx\n",mode,scene,stress,(unsigned long)hash,(unsigned long)game_hash());send(line);platform_fault(9120+scene);}}
  sprintf(line,"TIMING26,%d,%d,%d,%u,%u,%lu,%lu,%u,%lu,%lu,%lu,%u,%u,%u,%u,%08lx\n",mode,scene,stress,frames,updates,end-begin,us,merged,(unsigned long)(d.gap[0]+d.gap[1]),(unsigned long)(d.gap[4]+d.gap[5]+d.gap[6]),(unsigned long)d.maxGap,workMax,workLong,late,maxLate,(unsigned long)hash);send(line);
 }
 send("TIMING26_PASS,42_routes,state_hashes,framebuffer,guards\n");cleanup();send("SKIGAME_PASS\n");ExitToShell();
}
