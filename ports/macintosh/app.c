/* Macintosh application shell. The simulation advances in fixed 40 ms steps;
 * drawing and display deadlines are separate. Native dialogs suspend gameplay
 * and reset the clock on return so time spent reading never becomes catch-up
 * physics. Keep Toolbox calls and allocations out of the frame hot paths.
 */
#include <Quickdraw.h>
#include <Windows.h>
#include <Dialogs.h>
#include <Events.h>
#include <Fonts.h>
#include <Menus.h>
#include <MacMemory.h>
#include <OSUtils.h>
#include <Serial.h>
#include <Devices.h>
#include <Files.h>
#include <SegLoad.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim.h"
#include "scores.h"
#include "gfx.h"
#include "hud_format.h"
#include "replay_input.h"
#include "mouse_replay.h"
#include "frame_stats.h"
#include "frame_diagnostics.h"
#ifndef AUTOTEST
#define AUTOTEST 0
#endif
#include "frame_clock.h"
#include "deadline_clock.h"
static DeadlineClock playDeadline;
static unsigned pacingMerged, pacingMaxSteps = 1;
#define FRAME_TICKS 3
/* Selected kernels; the benchmark overrides this for matched comparisons. */
int perfKernel = 33536251;
static int hudAdvance = 8, hudBytes = 18, hudCellSafe, hudForce, hudX;
static unsigned char hudLast[4][14];
extern void hud_cell_asm(unsigned char *, const unsigned char *, unsigned long);
static unsigned char shiftedGlyphs[4][95][24];
extern void hud_packed_asm(unsigned char *, const unsigned char *);
static unsigned char hudPixels[16 * 64], pausedOverlay[1800], hudGlyphs[95][12];
extern void copy_move(void *, const void *, long);
extern void clear_asm(void *, long);
extern void hud_glyph_asm(unsigned char *, const unsigned char *);
static GrafPort hudPort;
static GrafPort fullPort;
static GrafPtr oldPort;
static unsigned char *screen, *savedScreen;
static short serialOut;
static FrameStats frameStats;
static int perfEnabled, perfRefresh = 1;
static unsigned char perfPixels[14 * 64];
static FrameDiagnostics diagnostics;
static int diagnosticPreviousFPS;
static unsigned timerFps10;
static unsigned fpsWindows, tickFpsMin, tickFpsMax, timerFpsMin, timerFpsMax;
static void fps_diagnostic_reset(void) {
  fpsWindows = 0;
  tickFpsMin = timerFpsMin = 65535;
  tickFpsMax = timerFpsMax = 0;
}
static int record_frame(uint32_t now, unsigned updates) {
  int ready = frameStats.ready, result;
  unsigned n = frameStats.frames + 1;
  result = frame_stats_record(&frameStats, now, updates);
  if (!ready)
    fps_clock_start();
  else if (result) {
    unsigned long us = fps_clock_sample();
    timerFps10 = us ? (n * 10000000u + us / 2) / us : 0;
    if (AUTOTEST || diagnostics.active) {
      fpsWindows++;
      if (frameStats.fps10 < tickFpsMin)
        tickFpsMin = frameStats.fps10;
      if (frameStats.fps10 > tickFpsMax)
        tickFpsMax = frameStats.fps10;
      if (timerFps10 < timerFpsMin)
        timerFpsMin = timerFps10;
      if (timerFps10 > timerFpsMax)
        timerFpsMax = timerFps10;
    }
  }
  return result;
}
static void toggle_perf(void) {
  fps_clock_stop();
  perfEnabled = !perfEnabled;
  frame_stats_reset(&frameStats);
  perfRefresh = 1;
  gfx_invalidate();
}
static int wmTest, inputTest, mouseTest, mouseScene, mouseStep,
    mouseApplied = -1;
static unsigned long inputCalls;
static int paused, quitGame, gfxMode = 2, portReady;
static unsigned char previousKeys[16];
static Point previousMouse;
static unsigned long rendered, missed;
static int faultCode;
static int windowWidth = 304, windowHeight = 297, growReady;
static unsigned char growPixels[32];
static short savedMenuHeight;
static RgnHandle savedDesktop;
#define MENU_HEIGHT (*(volatile short *)0x0baa)
#define DESKTOP_REGION (*(RgnHandle *)0x09ee)
static void restore_menu(void) {
  if (savedDesktop) {
    CopyRgn(savedDesktop, DESKTOP_REGION);
    DisposeRgn(savedDesktop);
    savedDesktop = NULL;
    MENU_HEIGHT = savedMenuHeight;
    DrawMenuBar();
  }
}
static RgnHandle contentRegion;
static WindowPtr gameWindow, fullWindow;
static int fullscreen = AUTOTEST;
static SkiScores scores;
static int scoreSeen, scoreSaveFailed;
static int backgrounded;
static int cursorTest;
static unsigned char windowFooter[532];
static int perfRebuilt;
static int hudRebuilt;
static int hudInitialized;
static int cursorHidden;
static short benchLog = -1;
static int benchLogError;
static void send(const char *s) {
  if (AUTOTEST) {
    long n = strlen(s), wanted = n;
    int done = !strcmp(s, "SKIGAME_PASS\n") || !strncmp(s, "SKIGAME_FAIL", 12);
    const char *out = s;
    if (benchLog >= 0 && (FSWrite(benchLog, &n, s) || n != wanted))
      benchLogError = 1;
    if (done && benchLog >= 0) {
      if (FSClose(benchLog))
        benchLogError = 1;
      benchLog = -1;
      if (FlushVol(NULL, 0))
        benchLogError = 1;
    }
    if (done && benchLogError)
      out = "SKIGAME_FAIL,RESULT_FILE\n";
    n = strlen(out);
    FSWrite(serialOut, &n, out);
  }
}
static void cleanup(void) {
  fps_clock_stop();
  timer_clock_stop();
  timer_measure_stop();
  restore_menu();
  if (fullWindow) {
    DisposeWindow(fullWindow);
    fullWindow = NULL;
  }
  if (contentRegion) {
    DisposeRgn(contentRegion);
    contentRegion = NULL;
  }
  if (gameWindow) {
    DisposeWindow(gameWindow);
    gameWindow = NULL;
  }
  if (portReady) {
    if (AUTOTEST && savedScreen)
      memcpy(screen, savedScreen, 21888);
    if (cursorHidden) {
      ShowCursor();
      cursorHidden = 0;
    }
    SetPort(oldPort);
    if (hudInitialized) {
      ClosePort(&hudPort);
      hudInitialized = 0;
    }
    ClosePort(&fullPort);
    portReady = 0;
  }
  if (savedScreen) {
    DisposePtr((Ptr)savedScreen);
    savedScreen = NULL;
  }
}
void platform_fault(int line) {
  char s[100];
  faultCode = line;
  {
    char *p = hud_string(s, "SKIGAME_FAIL,");
    p = hud_number(p, line, 1);
    *p++ = '\n';
    *p = 0;
  }
  send(s);
  if (!AUTOTEST && portReady) {
    MoveTo(20, 180);
    DrawText((Ptr)s, 0, strlen(s));
    while (!Button())
      SystemTask();
  }
  cleanup();
  ExitToShell();
  for (;;) {
  }
}
static void label(const char *s, int x, int y) {
  MoveTo(x, y);
  DrawText((Ptr)s, 0, strlen(s));
}

#include "native_dialog.h"
static int resetClock, lastScoreCourse, lastScoreRank = -1;
static void score_value(char *out, int course, int32_t value) {
  char *p;
  if (course == 3)
    p = hud_number(out, value, 1);
  else {
    long v = value;
    p = hud_number(out, v / 60000, 1);
    *p++ = ':';
    *p++ = '0' + ((v / 1000) % 60) / 10;
    *p++ = '0' + (v / 1000) % 10;
    *p++ = '.';
    *p++ = '0' + (v / 100) % 10;
    *p++ = '0' + (v / 10) % 10;
  }
  *p = 0;
}
static void result_dialog(GameInfo g, int course, int rank) {
  DialogLines *t = dialog_lines_new();
  int i, oldPause = paused;
  char text[96], value[32], *p;
  if (!t)
    return;
  paused = 1;
  dialog_line(t, course == 1   ? "Slalom - finished"
                 : course == 2 ? "Tree slalom - finished"
                               : "Freestyle - finished");
  dialog_line(t, "");
  if (course) {
    dialog_line(t, "Best scores");
    for (i = 0; i < scores.count[course - 1]; i++) {
      p = hud_number(text, i + 1, 1);
      p = hud_string(p, ". ");
      score_value(value, course, scores.value[course - 1][i]);
      p = hud_string(p, value);
      if (i == rank)
        p = hud_string(p, "   <- This is you");
      *p = 0;
      dialog_line(t, text);
    }
    if (rank < 0) {
      score_value(value, course, g.resultValue);
      p = hud_string(text, "This is you: ");
      p = hud_string(p, value);
      p = hud_string(p, " (outside the top ten)");
      *p = 0;
      dialog_line(t, text);
    }
  }

  if (scoreSaveFailed)
    dialog_line(t, "The score could not be saved on this disk.");
  native_text_dialog("\pSkiFree+", t, "Continue");
  DisposePtr((Ptr)t);
  paused = oldPause;
  resetClock = 1;
  gfx_invalidate();
  rendered = 0;
}
static void observe_score(void) {
  GameInfo g;
  if (!game_has_result()) {
    scoreSeen = 0;
    return;
  }
  g = game_info();
  if (!g.resultMode)
    scoreSeen = 0;
  else if (!scoreSeen) {
    scoreSeen = 1;
    lastScoreCourse = g.resultMode;
    lastScoreRank = score_insert(&scores, g.resultMode, g.resultValue);
    if (lastScoreRank >= 0)
      scoreSaveFailed = !scores_write(&scores, "SkiFree Scores");
    result_dialog(g, g.resultMode, lastScoreRank);
    return;
  }
}
static void show_scores(void) {
  DialogLines *t = dialog_lines_new();
  int c, i;
  char line[96], value[32], *p;
  if (!t)
    return;
  paused = 1;
  for (c = 0; c < 3; c++) {
    dialog_line(t, c == 0 ? "Slalom" : c == 1 ? "Tree slalom" : "Freestyle");
    if (!scores.count[c])
      dialog_line(t, "No scores yet.");
    for (i = 0; i < scores.count[c]; i++) {
      p = hud_number(line, i + 1, 1);
      p = hud_string(p, ". ");
      score_value(value, c + 1, scores.value[c][i]);
      p = hud_string(p, value);
      if (c + 1 == lastScoreCourse && i == lastScoreRank)
        p = hud_string(p, "   <- This is you");
      *p = 0;
      dialog_line(t, line);
    }
    dialog_line(t, "");
  }
  dialog_line(t, scoreSaveFailed ? "Scores could not be saved on this disk."
                                 : "Press P to resume after closing.");
  native_text_dialog("\pSkiFree+ Scores", t, "OK");
  DisposePtr((Ptr)t);
  resetClock = 1;
  gfx_invalidate();
}
static void diagnostic_toggle(void) {
  DialogLines *t;
  char line[128];
  unsigned fps;
  int oldPause = paused;
  if (!diagnostics.active) {
    diagnostic_start(&diagnostics);
    pacingMerged = 0;
    pacingMaxSteps = 1;
    fps_diagnostic_reset();
    diagnosticPreviousFPS = perfEnabled;
    perfEnabled = 1;
    frame_stats_reset(&frameStats);
    perfRefresh = 1;
    return;
  }
  diagnostics.active = 0;
  paused = 1;
  fps = diagnostic_fps10(&diagnostics);
  t = dialog_lines_new();
  if (t) {
    dialog_line(t, "SkiFree+ build 27 - timing report");
    sprintf(line, "Ticks; combined: %u; max steps: %u", pacingMerged,
            pacingMaxSteps);
    dialog_line(t, line);
    sprintf(line, "Frames: %lu   Play: %lu.%lus",
            (unsigned long)diagnostics.frames,
            (unsigned long)(diagnostics.ticks / 60),
            (unsigned long)((diagnostics.ticks % 60) / 6));
    dialog_line(t, line);
    sprintf(line, "Average FPS (ticks): %u.%u", fps / 10, fps % 10);
    dialog_line(t, line);
    if (fpsWindows) {
      sprintf(line, "FPS windows: %u.%u to %u.%u", tickFpsMin / 10,
              tickFpsMin % 10, tickFpsMax / 10, tickFpsMax % 10);
      dialog_line(t, line);
      sprintf(line, "Timer M: %u.%u to %u.%u", timerFpsMin / 10,
              timerFpsMin % 10, timerFpsMax / 10, timerFpsMax % 10);
      dialog_line(t, line);
    } else
      dialog_line(t, "Record >2 seconds for both clock readings.");
    sprintf(line, "Long gaps: %lu   Short: %lu",
            (unsigned long)(diagnostics.gap[4] + diagnostics.gap[5] +
                            diagnostics.gap[6]),
            (unsigned long)(diagnostics.gap[0] + diagnostics.gap[1]));
    dialog_line(t, line);
    sprintf(line, "Longest gap: %lu ticks", (unsigned long)diagnostics.maxGap);
    dialog_line(t, line);
    sprintf(line, "Behind schedule: %lu frames (max %lu)",
            (unsigned long)diagnostics.debtFrames,
            (unsigned long)diagnostics.maxDebt);
    dialog_line(t, line);
    dialog_line(t, "Gap ticks: 0 / 1 / 2 / 3 / 4 / 5 / 6+");
    sprintf(
        line, "%lu / %lu / %lu / %lu / %lu / %lu / %lu",
        (unsigned long)diagnostics.gap[0], (unsigned long)diagnostics.gap[1],
        (unsigned long)diagnostics.gap[2], (unsigned long)diagnostics.gap[3],
        (unsigned long)diagnostics.gap[4], (unsigned long)diagnostics.gap[5],
        (unsigned long)diagnostics.gap[6]);
    dialog_line(t, line);
    dialog_line(t, "Paused time excluded.");
    native_text_dialog_width("\pSkiFree+ diagnostics", t, "Resume", 488);
    DisposePtr((Ptr)t);
  }
  perfEnabled = diagnosticPreviousFPS;
  perfRefresh = 1;
  paused = oldPause;
  resetClock = 1;
  gfx_invalidate();
  rendered = 0;
}
static void about_box(void) {
  DialogLines *t = dialog_lines_new();
  int oldPause = paused;
  if (!t)
    return;
  paused = 1;
  dialog_line(t, "SkiFree+");
  dialog_line(t, "Original SkiFree by Chris Pirih");
  dialog_line(t, "Copyright 1991 Chris Pirih");
  dialog_line(t, "Original game: ski.ihoc.net");
  dialog_line(t, "");
  dialog_line(t, "Based on Eric Fry's C decompilation");
  dialog_line(t, "and the jeff-1amstudios SDL source port.");
  dialog_line(t, "");
  dialog_line(t, "Project direction and hardware testing:");
  dialog_line(t, "ClariceFromClarisWorks");
  dialog_line(t, "Port implementation and optimisation:");
  dialog_line(t, "OpenAI Codex (AI-assisted development).");
  dialog_line(t, "");
  dialog_line(t, "Press I to toggle FPS.");
  dialog_line(t, "D: record timing; D again shows results.");
  dialog_line(t, "512 x 342 monochrome compact-Mac edition.");
  dialog_line(t, "F1: About.");
  native_text_dialog("\pAbout SkiFree+", t, "OK");
  DisposePtr((Ptr)t);
  paused = oldPause;
  resetClock = 1;
  gfx_invalidate();
}
static void toggle_display(void);
static void compact_view(int enabled) {
  game_set_compact(enabled);
  game_set_snapshot_actors(!enabled && (perfKernel & 4096));
  game_set_camera(!enabled && (perfKernel & 262144));
  gfx_set_compact(enabled);
  rendered = 0;
  resetClock = 1;
  GetMouse(&previousMouse);
}
static void menus_init(void) {
  MenuHandle m = NewMenu(127, "\p\024");
  AppendMenu(m, "About SkiFree+...;(-");
  AppendResMenu(m, 'DRVR');
  InsertMenu(m, 0);
  m = NewMenu(128, "\pGame");
  AppendMenu(m, "Slalom/1;Tree slalom/2;Freestyle/3;Free "
                "ski/4;(-;Pause/P;Scoreboard.../H;Frame counter/I;(-;Quit/Q");
  InsertMenu(m, 0);
  DrawMenuBar();
}
static void game_menu(long choice) {
  int id = choice >> 16, item = choice & 65535;
  if (id == 127) {
    if (item == 1)
      about_box();
    else if (item > 2) {
      Str255 name;
      GetMenuItemText(GetMenuHandle(127), item, name);
      OpenDeskAcc(name);
    }
  } else if (id == 128) {
    if (item >= 1 && item <= 4) {
      game_start_course(item == 4 ? 0 : item);
      scoreSeen = 0;
      paused = 0;
      gfx_invalidate();
    } else if (item == 6) {
      paused = !paused;
      gfx_invalidate();
    } else if (item == 7)
      show_scores();
    else if (item == 8)
      toggle_perf();
    else if (item == 10)
      quitGame = 1;
  }
  HiliteMenu(0);
}

static void hud_init(void) {
  Rect r;
  int y;
  GrafPtr old;
  GetPort(&old);
  OpenPort(&hudPort);
  hudPort.portBits = qd.screenBits;
  hudPort.portBits.baseAddr = (Ptr)hudPixels;
  SetRect(&hudPort.portBits.bounds, 0, 0, 512, 16);
  hudPort.portRect = hudPort.portBits.bounds;
  SetPort(&hudPort);
  SetRect(&r, 0, 0, 512, 16);
  ClipRect(&r);
  TextFont(3);
  TextSize(9);
  TextMode(srcCopy);
  EraseRect(&r);
  {
    int c;
    TextFont(4);
    TextSize(9);
    hudAdvance = CharWidth(48);
    if (hudAdvance < 6 || hudAdvance > 8)
      hudAdvance = 8;
    hudBytes = ((14 * hudAdvance + 8 + 15) / 16) * 2;
    SetRect(&r, 0, 0, 8, 12);
    for (c = 32; c <= 126; c++) {
      EraseRect(&r);
      MoveTo(0, 9);
      DrawChar(c);
      for (y = 0; y < 12; y++)
        hudGlyphs[c - 32][y] = hudPixels[y * 64];
    }
    TextFont(3);
    TextSize(9);
  }
  /* Render the pause card directly into its own small bitmap. */
  hudPort.portBits.baseAddr = (Ptr)pausedOverlay;
  hudPort.portBits.rowBytes = 36;
  SetRect(&hudPort.portBits.bounds, 0, 0, 288, 50);
  hudPort.portRect = hudPort.portBits.bounds;
  r = hudPort.portRect;
  ClipRect(&r);
  EraseRect(&r);
  FrameRect(&r);
  label("Paused - P to resume, Esc to quit", 18, 20);
  label("Mouse / arrows steer; click / Space jumps", 8, 38);
  hudPort.portBits.baseAddr = (Ptr)hudPixels;
  hudPort.portBits.rowBytes = 64;
  SetRect(&hudPort.portBits.bounds, 0, 0, 512, 16);
  hudPort.portRect = hudPort.portBits.bounds;
  r = hudPort.portRect;
  ClipRect(&r);
  {
    int ph, c, yy;
    for (ph = 0; ph < 4; ph++)
      for (c = 0; c < 95; c++)
        for (yy = 0; yy < 12; yy++) {
          unsigned v = ((unsigned)hudGlyphs[c][yy] << 8) >> (ph * 2);
          shiftedGlyphs[ph][c][yy * 2] = v >> 8;
          shiftedGlyphs[ph][c][yy * 2 + 1] = v;
        }
  }
  hudCellSafe = hudAdvance == 6 || hudAdvance == 8;
  {
    int c, yy;
    for (c = 0; c < 95; c++)
      for (yy = 0; yy < 12; yy++)
        if (hudGlyphs[c][yy] & ((1u << (8 - hudAdvance)) - 1))
          hudCellSafe = 0;
  }
  SetPort(old);
  hudInitialized = 1;
}
/* Fullscreen is the only playable geometry in the base 68000 edition. */
static void toggle_display(void) {
  Rect r;
  if (fullWindow)
    return;
  HideCursor();
  savedDesktop = NewRgn();
  if (!savedDesktop) {
    ShowCursor();
    return;
  }
  CopyRgn(DESKTOP_REGION, savedDesktop);
  savedMenuHeight = MENU_HEIGHT;
  MENU_HEIGHT = 0;
  SetRect(&r, 0, 0, 512, 342);
  RectRgn(DESKTOP_REGION, &r);
  fullWindow = NewWindow(NULL, &r, "\pSkiFree+", true, plainDBox, (WindowPtr)-1,
                         false, 0);
  if (fullWindow) {
    if (gameWindow)
      HideWindow(gameWindow);
    fullscreen = 1;
    SetPort(fullWindow);
    game_resize(512, 342);
  } else
    restore_menu();
  if (contentRegion)
    RectRgn(contentRegion, &r);
  backgrounded = 0;
  gfx_invalidate();
  rendered = 0;
  ShowCursor();
  GetMouse(&previousMouse);
}
/* Original four-row, upper-right status panel; update at the source's
   roughly 330 ms cadence, independent of drawing rate. */
extern void present_rect_asm(void *, const void *, long, long, long);
static void classic_text_full(const char *s, int row) {
  int x, y, base = row ? 8 + row * 128 : 32, top = 2;
  for (x = 0; s[x] && x < (row ? 14 : 11); x++) {
    unsigned c = (unsigned char)s[x];
    const unsigned char *g = hudGlyphs[(c >= 32 && c <= 126) ? c - 32 : 0];
    int bit = base + x * hudAdvance, byte = bit / 8, shift = bit & 7;
    if ((perfKernel & 16) && !(shift & 1)) {
      hud_packed_asm(
          hudPixels + top * 64 + byte,
          shiftedGlyphs[shift / 2][(c >= 32 && c <= 126) ? c - 32 : 0]);
      continue;
    }
    for (y = 0; y < 12; y++) {
      unsigned v = (unsigned)g[y] << 8;
      v >>= shift;
      hudPixels[(top + y) * 64 + byte] |= v >> 8;
      if (shift)
        hudPixels[(top + y) * 64 + byte + 1] |= v;
    }
  }
}
static void hud_cell_c(unsigned char *dst, const unsigned char *g,
                       unsigned long keep) {
  int y;
  for (y = 0; y < 12; y++) {
    dst[y * 64] = (dst[y * 64] & (keep >> 8)) | g[y * 2];
    dst[y * 64 + 1] = (dst[y * 64 + 1] & keep) | g[y * 2 + 1];
  }
}
static void classic_text(const char *s, int row) {
  int x, ended = 0, base = row ? 8 + row * 128 : 32, top = 2;
  if (!(perfKernel & 32768) || !hudCellSafe) {
    classic_text_full(s, row);
    return;
  }
  for (x = 0; x < (row ? 14 : 11); x++) {
    unsigned c = ended ? ' ' : (unsigned char)s[x];
    if (!c) {
      ended = 1;
      c = ' ';
    }
    if (hudForce || hudLast[row][x] != c) {
      int bit = base + x * hudAdvance, shift = bit & 7;
      unsigned long keep =
          65535u ^ (((255u << (8 - hudAdvance)) & 255u) << (8 - shift));
      unsigned char *dst = hudPixels + top * 64 + bit / 8;
      const unsigned char *g =
          shiftedGlyphs[shift / 2][c >= 32 && c <= 126 ? c - 32 : 0];
      if (gfxMode == 2)
        hud_cell_asm(dst, g, keep);
      else
        hud_cell_c(dst, g, keep);
      hudLast[row][x] = c;
      gfx_dirty(hudX + bit, top, hudAdvance, 12);
    }
  }
}
/* Steady verdict icon: update only this 16x12 HUD tile on transitions. */
#include "gate_faces.h"
static void gate_hud(int force) {
  static int previous = -1;
  int result = game_gate_feedback();
  int pattern = result, y;
  if (force || pattern != previous) {
    for (y = 0; y < 12; y++) {
      unsigned bits = gateFaces[pattern][y];
      hudPixels[(y + 2) * 64 + 1] = bits >> 8;
      hudPixels[(y + 2) * 64 + 2] = bits;
    }
    gfx_dirty(8, 2, 16, 12);
    previous = pattern;
  }
}
static void classic_hud(void) {
  static unsigned lastStep = ~0u;
  static int oldOverlay = -1;
  GameInfo g;
  unsigned step = game_steps();
  int x, y, overlay = paused ? 1 : 0, strip = 1,
            panelBytes = strip ? 64 : hudBytes, panelHeight = strip ? 16 : 52;
  unsigned char *dst = gfx_target();
  char line[80], *p;
  long t, sec, mins;
  hudRebuilt = 0;
  if (!hudInitialized)
    hud_init();
  panelBytes = strip ? 64 : hudBytes;
  x = strip ? 0 : (fullscreen ? 512 : windowWidth) - panelBytes * 8;
  hudX = x;
  hudForce = rendered == 0;
  if (rendered == 0 || step < lastStep || step - lastStep >= 9) {
    hudRebuilt = 1;
    g = game_info();
    t = g.elapsed < 0 ? 0 : g.elapsed;
    sec = t / 1000;
    mins = sec / 60;
    if (!g.alive)
      g.distance = 0;
    if (!(perfKernel & 32768) || !hudCellSafe || hudForce) {
      if (gfxMode == 2)
        clear_asm(hudPixels, panelHeight * 64);
      else
        memset(hudPixels, 0, panelHeight * 64);
    }
    p = hud_string(line, "T: ");
    p = hud_number(p, mins, 2);
    *p++ = ':';
    *p++ = '0' + (sec % 60) / 10;
    *p++ = '0' + sec % 10;
    *p++ = '.';
    *p++ = '0' + (t % 1000) / 100;
    *p++ = '0' + (t % 100) / 10;
    *p = 0;
    classic_text(line, 0);
    p = hud_string(line, "Dist: ");
    p = hud_number(p, g.distance, 1);
    p = hud_string(p, "m");
    *p = 0;
    classic_text(line, 1);
    p = hud_string(line, "Speed:");
    p = hud_number(p, g.speed, 1);
    p = hud_string(p, "m/s");
    *p = 0;
    classic_text(line, 2);
    p = hud_string(line, "Style:");
    p = hud_number(p, g.style, 1);
    *p = 0;
    line[12] = 0;
    classic_text(line, 3);
    if (!(perfKernel & 32768) || !hudCellSafe || hudForce) {
      memset(hudPixels, 255, panelBytes);
      memset(hudPixels + (panelHeight - 1) * 64, 255, panelBytes);
      for (y = 1; y < panelHeight - 1; y++) {
        hudPixels[y * 64] |= 128;
        hudPixels[y * 64 + panelBytes - 1] |= 1;
        if (strip) {
          hudPixels[y * 64 + 15] |= 1;
          hudPixels[y * 64 + 31] |= 1;
          hudPixels[y * 64 + 47] |= 1;
        }
      }
      gfx_dirty(x, 0, panelBytes * 8, panelHeight);
    }
    lastStep = g.steps;
  }
  gate_hud(hudForce || ((!(perfKernel & 32768) || !hudCellSafe) && hudRebuilt));
  if (!(perfKernel & 2048) ||
      gfx_intersects(x, 0, panelBytes * 8, panelHeight)) {
    if (gfxMode == 2)
      present_rect_asm(dst + x / 8, hudPixels, panelBytes, panelHeight, 64);
    else
      for (y = 0; y < panelHeight; y++)
        memcpy(dst + y * 64 + x / 8, hudPixels + y * 64, panelBytes);
  }
  if (!(perfKernel & 2))
    gfx_erase_next(x, 0, panelBytes * 8, panelHeight);
  if (overlay != oldOverlay) {
    gfx_dirty(fullscreen ? 112 : 8, 140, 288, 50);
    oldOverlay = overlay;
  }
  if (overlay) {
    for (y = 140; y < 190; y++)
      memcpy(dst + y * 64 + (fullscreen ? 14 : 1),
             pausedOverlay + (y - 140) * 36, 36);
    gfx_erase_next(fullscreen ? 112 : 8, 140, 288, 50);
  }
}
static Point windowOrigin;
static int directWindow;
extern void present_rect_asm(void *, const void *, long, long, long);
static void window_region(int x, int y, int w, int h) {
  if (directWindow)
    present_rect_asm(screen + (windowOrigin.v + y) * 64 +
                         (windowOrigin.h + x) / 8,
                     gfx_target() + y * 64 + x / 8, w / 8, h, 64);
  else {
    BitMap src = qd.screenBits;
    Rect r;
    src.baseAddr = (Ptr)gfx_target();
    SetRect(&r, x, y, x + w, y + h);
    CopyBits(&src, &(fullscreen ? fullWindow : gameWindow)->portBits, &r, &r,
             srcCopy, NULL);
  }
}
/* Optional once-per-two-second text refresh, using cached System 6 glyphs. */
static void perf_overlay(void) {
  int x, y, height = fullscreen ? 342 : windowHeight;
  char text[64], *p;
  perfRebuilt = 0;
  if (!perfEnabled)
    return;
  if (perfRefresh && !hudRebuilt) {
    perfRebuilt = 1;
    memset(perfPixels, 0, sizeof(perfPixels));
    if (!frameStats.ready)
      p = hud_string(text, "FPS --.- M--.- G- B- T-");
    else {
      p = hud_string(text, diagnostics.active ? "REC FPS " : "FPS ");
      p = hud_number(p, frameStats.fps10 / 10, 1);
      *p++ = '.';
      *p++ = '0' + frameStats.fps10 % 10;
      p = hud_string(p, " M");
      p = hud_number(p, timerFps10 / 10, 1);
      *p++ = '.';
      *p++ = '0' + timerFps10 % 10;
      p = hud_string(p, " G");
      p = hud_number(p, frameStats.shownGaps, 1);
      p = hud_string(p, " B");
      p = hud_number(p, frameStats.shownBursts, 1);
      p = hud_string(p, " T");
      p = hud_number(p, frameStats.shownMax, 1);
    }
    *p = 0;
    for (x = 0; text[x] && x < 40; x++) {
      unsigned c = (unsigned char)text[x];
      hud_glyph_asm(perfPixels + x,
                    hudGlyphs[(c >= 32 && c <= 126) ? c - 32 : 0]);
    }
    gfx_dirty(0, height - 14, 320, 14);
    perfRefresh = 0;
  }
  if (!(perfKernel & 2048) || gfx_intersects(0, height - 14, 320, 14))
    present_rect_asm(gfx_target() + (height - 14) * 64, perfPixels, 40, 14, 64);
  if (!(perfKernel & 2))
    gfx_erase_next(0, height - 14, 320, 14);
}
#include "shadow.h"
/* Order matters: world first, then airborne aids and HUD. Each overlay registers damage so disappearance restores the world. */
static void compose(void) {
  if (backgrounded && !AUTOTEST)
    return;
  if (!AUTOTEST)
    observe_score();
  gfx_frame(gfxMode);
  shadow_overlay();
  gfx_air_inset(fullscreen ? game_air_pose(16) : 0, gfxMode);
  classic_hud();
  perf_overlay();
}
/* Only the validated front-window path can write directly to screen memory. Cursor ownership stays with the OS. */
static void present(void) {
  WindowPtr active = fullscreen ? fullWindow : gameWindow;
  if (backgrounded && !AUTOTEST)
    return;
  if (!AUTOTEST || cursorTest)
    HideCursor();
  if (AUTOTEST && fullscreen && !wmTest)
    gfx_present(gfxMode);
  else {
    SetPort(active);
    windowOrigin.h = windowOrigin.v = 0;
    LocalToGlobal(&windowOrigin);
    directWindow = !(windowOrigin.h & 15) && windowOrigin.h >= 0 &&
                   windowOrigin.h + (fullscreen ? 512 : windowWidth) <= 512 &&
                   windowOrigin.v >= 0 &&
                   windowOrigin.v + (fullscreen ? 342 : windowHeight) <= 342 &&
                   FrontWindow() == active && contentRegion &&
                   EqualRgn(contentRegion, active->visRgn);
    if ((perfKernel & 2097152) && fullscreen && directWindow)
      gfx_present(gfxMode);
    else
      gfx_regions(fullscreen ? 512 : windowWidth,
                  fullscreen ? 342 : windowHeight, window_region);
  }
  if (!AUTOTEST || cursorTest)
    ShowCursor();
  rendered++;
}
static void render(void) {
  compose();
  present();
}

static void mouse_position(Point p) {
  if ((fullscreen ||
       (p.h >= 0 && p.h < windowWidth && p.v >= 0 && p.v < windowHeight)) &&
      (p.h != previousMouse.h || p.v != previousMouse.v))
    game_mouse(p.h, p.v + ((AUTOTEST && mouseTest) ? game_camera_shift() : 0));
}
static void game_click(void) {
  if (!paused)
    game_input(INPUT_CLICK);
}
static int key(const unsigned char *k, int code) {
  return (k[code >> 3] >> (code & 7)) & 1;
}
static void input(void) {
  unsigned char k[16], pressed[16] = {0};
  Point p;
  static long repeatAt;
  static int wasButton;
  long now = TickCount();
  int down;
  if (!AUTOTEST || inputTest) {
    EventRecord ev;
    WindowPtr hit;
    while (WaitNextEvent(everyEvent, &ev, 0, NULL)) {
      if (ev.what == keyDown) {
        unsigned code = (ev.message >> 8) & 127;
        pressed[code >> 3] |= 1 << (code & 7);
        continue;
      }
      if (ev.what == osEvt && ((ev.message >> 24) & 255) == 1) {
        backgrounded = !(ev.message & 1);
        if (backgrounded) {
          if (fullWindow)
            HideWindow(fullWindow);
          restore_menu();
          paused = 1;
        } else if (fullWindow) {
          Rect desk;
          savedDesktop = NewRgn();
          if (savedDesktop) {
            CopyRgn(DESKTOP_REGION, savedDesktop);
            savedMenuHeight = MENU_HEIGHT;
            MENU_HEIGHT = 0;
            SetRect(&desk, 0, 0, 512, 342);
            RectRgn(DESKTOP_REGION, &desk);
          }
          ShowWindow(fullWindow);
          SelectWindow(fullWindow);
          resetClock = 1;
        }
        gfx_invalidate();
        continue;
      }
      if (ev.what == activateEvt) {
        if ((WindowPtr)ev.message == (fullscreen ? fullWindow : gameWindow)) {
          backgrounded = !(ev.modifiers & activeFlag);
          if (backgrounded)
            paused = 1;
          gfx_invalidate();
        }
        continue;
      }
      if (ev.what == updateEvt) {
        BeginUpdate((WindowPtr)ev.message);
        EndUpdate((WindowPtr)ev.message);
        gfx_invalidate();
      } else if (ev.what == mouseDown) {
        short part = FindWindow(ev.where, &hit);
        if (part == inMenuBar) {
          game_menu(MenuSelect(ev.where));
        } else if (part == inSysWindow) {
          SystemClick(&ev, hit);
        } else if (part == inGoAway && hit == gameWindow) {
          if (TrackGoAway(hit, ev.where))
            quitGame = 1;
        } else if (part == inContent &&
                   hit == (fullscreen ? fullWindow : gameWindow)) {
          SelectWindow(hit);
          if (!AUTOTEST || !mouseTest)
            game_click();
        }
      }
    }
    SetPort(fullscreen ? fullWindow : gameWindow);
  }
  if (backgrounded)
    return;
  GetKeys(k);
  GetMouse(&p);
  down = Button();
  if (AUTOTEST && mouseTest) {
    int x, y, click;
    inputCalls++;
    mouse_trace(mouseStep, mouseScene, fullscreen ? 512 : windowWidth,
                fullscreen ? 342 : windowHeight, &x, &y, &click);
    p.h = x;
    p.v = y;
    memset(k, 0, 16);
    memset(pressed, 0, 16);
    down = 0;
  }

#define EDGE(c) ((key(k, c) || key(pressed, c)) && !key(previousKeys, c))
  if (EDGE(53) || (key(k, 55) && EDGE(12)))
    quitGame = 1;
  if (!AUTOTEST && EDGE(122))
    about_box();

  if (!AUTOTEST && EDGE(2))
    diagnostic_toggle();
  if (!AUTOTEST && EDGE(4))
    show_scores();
  if (!AUTOTEST && EDGE(34))
    toggle_perf();

  if (EDGE(35) || EDGE(99)) {
    paused = !paused;
    gfx_invalidate();
  }
  if (EDGE(3))
    game_input(INPUT_FAST);
  if (EDGE(18) || EDGE(19) || EDGE(20) || EDGE(21)) {
    game_start_course(EDGE(18) ? 1 : EDGE(19) ? 2 : EDGE(20) ? 3 : 0);
    paused = 0;
    resetClock = 1;
    gfx_invalidate();
  }
  if (EDGE(15) || EDGE(120) || ((EDGE(36) || EDGE(76)) && !game_info().alive)) {
    game_init(1, 0, 0);
    paused = 0;
    resetClock = 1;
  }
  if (!paused) {
    mouse_position(p);
    if (AUTOTEST && mouseTest && mouseApplied != mouseStep) {
      int x, y, click;
      mouse_trace(mouseStep, mouseScene, fullscreen ? 512 : windowWidth,
                  fullscreen ? 342 : windowHeight, &x, &y, &click);
      if (click)
        game_click();
#if AUTOTEST
      if (mouseScene == 1 && mouseStep % 30 == 0)
        game_test_gate((mouseStep / 30) & 1);
      if (mouseScene == 3 && mouseStep % 100 == 0)
        game_test_jump();
#endif
      mouseApplied = mouseStep;
    }
    if (EDGE(49))
      game_input(INPUT_JUMP);
    if (EDGE(82))
      game_input(INPUT_KEYPAD_JUMP);
    if (AUTOTEST && fullscreen && down && !wasButton)
      game_input(INPUT_CLICK);
    if (EDGE(83))
      game_input(INPUT_SW);
    if (EDGE(85))
      game_input(INPUT_SE);
    if (EDGE(89))
      game_input(INPUT_W);
    if (EDGE(92))
      game_input(INPUT_E);
    if (now >= repeatAt) {
      int code = (key(k, 123) || key(k, 86))   ? INPUT_LEFT
                 : (key(k, 124) || key(k, 88)) ? INPUT_RIGHT
                 : (key(k, 125) || key(k, 84)) ? INPUT_DOWN
                 : (key(k, 126) || key(k, 91)) ? INPUT_UP
                                               : INPUT_NONE;
      if (code) {
        game_input(code);
        repeatAt = now + 6;
      }
    }
  }
  memcpy(previousKeys, k, 16);
  previousMouse = p;
  wasButton = down;
#undef EDGE
}

static void advance(unsigned *steps, int sc, int *accum, int ticks) {
  *accum += ticks * 5;
  while (*accum >= 12) {
    replay_input(*steps, sc);
    game_step();
    (*steps)++;
    *accum -= 12;
  }
}
#include "pacing.h"
#include "scale_test.h"
#include "timing_test.h"
static void autotest(void) {
  timing_test();
  const int xs[] = {1000, -448, 0, 416, 1000, 1000, 1000, 1000, 1000, 0},
            ys[] = {0, 0, 0, 0, 31200, 0, 0, 0, 0, 0};
  int sc, i, r, m, o;
  char line[220];
  long start, end;
  unsigned checks = 0;
  uint32_t hashes[10] = {0};
  send("SKIGAME_START\n");
  if (gfx_compiled_test())
    platform_fault(9704);
  send("COMPILED,1424,PIXEL_REFERENCE,PASS\n");
  if (gfx_phase_test())
    platform_fault(9703);
  send("PHASES,1424,C_ASM,PASS\n");
  {
    int n;
    uint32_t v = 1;
    char a[20], b[20];
    for (n = 0; n < 4096; n++) {
      char *e;
      v = v * 1664525u + 1013904223u;
      e = hud_u32_asm(a, v);
      *e = 0;
      sprintf(b, "%lu", (unsigned long)v);
      if (strcmp(a, b))
        platform_fault(9701);
      e = hud_u32_c(a, v);
      *e = 0;
      if (strcmp(a, b))
        platform_fault(9702);
    }
    send("FORMAT,4096,C_ASM_LIBRARY,PASS\n");
  }
  {
    int e = game_course_test();
    if (e)
      platform_fault(9600 + e);
    send("COURSES,3,start_penalty_finish_restart,PASS\n");
  }
  sprintf(line, "MEMORY,free_heap,%ld\n", (long)FreeMem());
  send(line);
  for (sc = 0; sc < 10; sc++) {
    game_init(1, xs[sc], ys[sc]);
    coverage_reset();
    for (i = 0; i < 1500; i++) {
      replay_input(i, sc);
      game_step();
      coverage_step();
      if (i == 299 || i == 1499) {
        char cov[220];
        coverage_line(cov, sc, i + 1);
        send(cov);
      }
      if ((i + 1) % 100 == 0) {
        GameInfo g = game_info();
        sprintf(line, "STATE,%d,%d,%08lx,%ld,%ld,%u,%d\n", sc, i + 1,
                (unsigned long)game_hash(), (long)g.x, (long)g.y, g.objects,
                g.alive);
        send(line);
        for (m = 0; m < 3; m++) {
          if (!gfx_verify(m))
            platform_fault(9100 + sc);
          checks++;
        }
      }
    }
  }

  for (sc = 0; sc < 10; sc++) {
    game_init(1, xs[sc], ys[sc]);
    gfx_invalidate();
    for (i = 0; i < 60; i++) {
      replay_input(i, sc);
      game_step();
      gfx_frame(2);
      if (!gfx_composition_check())
        platform_fault(9710 + sc);
      gfx_composed();
    }
  }
  send("COMPOSITION,600,PIXEL_REFERENCE,PASS\n");
  game_init(1, 1000, 0);
  for (i = 0; i < 200; i++) {
    replay_input(i, 0);
    game_step();
  }
  start = TickCount();
  for (i = 0; i < 300; i++) {
    replay_input(i, 0);
    game_step();
  }
  end = TickCount();
  sprintf(line, "PROFILE,sim,300,%ld\n", end - start);
  send(line);
  start = TickCount();
  for (i = 0; i < 300; i++)
    gfx_frame(2);
  end = TickCount();
  sprintf(line, "PROFILE,gfx,300,%ld\n", end - start);
  send(line);
  start = TickCount();
  for (i = 0; i < 300; i++) {
    rendered = 0;
    classic_hud();
  }
  end = TickCount();
  sprintf(line, "PROFILE,hud_refresh,300,%ld\n", end - start);
  send(line);
#if 0 /* Diagnostic-only exit; the delivered build runs the complete suite. */
 cleanup();send("SKIGAME_PASS\n");ExitToShell();
#endif
  for (r = 0; r < 5; r++)
    for (o = 0; o < 3; o++) {
      uint32_t v = 1;
      volatile unsigned sum = 0;
      char formatted[20];
      m = (o + r) % 3;
      start = TickCount();
      for (i = 0; i < 4096; i++) {
        char *e;
        v = v * 1664525u + 1013904223u;
        if (!m)
          sprintf(formatted, "%lu", (unsigned long)v);
        else {
          e = m == 1 ? hud_u32_c(formatted, v) : hud_u32_asm(formatted, v);
          *e = 0;
        }
        sum += (unsigned char)formatted[0];
      }
      end = TickCount();
      sprintf(line, "FORMAT_BENCH,%d,%d,4096,%ld,%u\n", m, r, end - start, sum);
      send(line);
    }
  for (r = 0; r < 5; r++)
    for (o = 0; o < 2; o++) {
      unsigned sum;
      m = (o + r) % 2;
      start = TickCount();
      sum = gfx_phase_bench(m);
      end = TickCount();
      sprintf(line, "PHASE_BENCH,%d,%d,256,%ld,%u\n", m, r, end - start, sum);
      send(line);
    }
  send("BENCH,scene,mode,repeat,frames,ticks,state_hash\n");
  for (sc = 0; sc < 10; sc++)
    for (r = 0; r < 5; r++)
      for (o = 0; o < 3; o++) {
        unsigned steps = 0;
        int accum = 0;
        m = (o + r) % 3;
        gfxMode = m;
        game_init(1, xs[sc], ys[sc]);
        gfx_invalidate();
        rendered = 0;
        render();
        rendered = 0;
        start = TickCount();
        for (i = 0; i < 240; i++) {
          advance(&steps, sc, &accum, 3);
          render();
        }
        end = TickCount();
        if (!gfx_present_verified()) {
          int z;
          for (z = 0; z < 21888; z++)
            if (screen[z] != gfx_target()[z]) {
              sprintf(line, "SCREEN_DIFF,%d,%d,%d,%d,%d\n", sc, m, z, screen[z],
                      gfx_target()[z]);
              send(line);
              break;
            }
          platform_fault(9200 + sc);
        }
        if (!gfx_verify(m))
          platform_fault(9250 + sc);
        checks++;
        if (!r && !o)
          hashes[sc] = game_hash();
        else if (hashes[sc] != game_hash())
          platform_fault(9300 + sc);
        sprintf(line, "BENCH,%d,%d,%d,240,%ld,%08lx\n", sc, m, r, end - start,
                (unsigned long)game_hash());
        send(line);
      }
  send("PACED,scene,frames,ticks,late_frames,max_work_ticks,state_hash\n");
  gfxMode = 2;
  for (sc = 0; sc < 10; sc++) {
    unsigned steps = 0;
    int accum = 0;
    unsigned late = 0, maxWork = 0;
    long next, frameStart, finish;
    game_init(1, xs[sc], ys[sc]);
    rendered = 0;
    render();
    rendered = 0;
    start = TickCount();
    next = start;
    for (i = 0; i < 240; i++) {
      while ((long)(TickCount() - next) < 0) {
      }
      frameStart = TickCount();
      advance(&steps, sc, &accum, 3);
      render();
      finish = TickCount();
      if (finish - frameStart > maxWork)
        maxWork = finish - frameStart;
      if (finish > next + 3)
        late++;
      next += 3;
      if (finish - next >= FRAME_TICKS)
        next = finish;
    }
    while ((long)(TickCount() - next) < 0) {
    }
    end = TickCount();
    if (!gfx_present_verified() || !gfx_verify(2) || hashes[sc] != game_hash())
      platform_fault(9400 + sc);
    checks++;
    sprintf(line, "PACED,%d,240,%ld,%u,%u,%08lx\n", sc, end - start, late,
            maxWork, (unsigned long)game_hash());
    send(line);
  }

  {
    unsigned steps = 0, late = 0, maxWork = 0;
    int accum = 0;
    long next, frameStart, finish;
    game_init(1, 1000, 0);
    rendered = 0;
    render();
    rendered = 0;
    start = TickCount();
    next = start;
    for (i = 0; i < 1800; i++) {
      while ((long)(TickCount() - next) < 0) {
      }
      frameStart = TickCount();
      advance(&steps, 0, &accum, 3);
      render();
      finish = TickCount();
      if (finish - frameStart > maxWork)
        maxWork = finish - frameStart;
      if (finish > next + 3)
        late++;
      next += 3;
      if (finish - next >= FRAME_TICKS)
        next = finish;
    }
    while ((long)(TickCount() - next) < 0) {
    }
    end = TickCount();
    if (!gfx_present_verified() || !gfx_verify(2))
      platform_fault(9450);
    checks++;
    sprintf(line, "SOAK,1800,%ld,%u,%u,%08lx\n", end - start, late, maxWork,
            (unsigned long)game_hash());
    send(line);
  }
  /* Pause must not advance simulation; restart must reproduce the same initial
   * state. */
  game_init(1, 0, 0);
  {
    uint32_t h = game_hash();
    paused = 1;
    start = TickCount();
    while (TickCount() - start < 60) {
      render();
      SystemTask();
    }
    if (game_hash() != h)
      platform_fault(9501);
    paused = 0;
    game_step();
    game_init(1, 0, 0);
    if (game_hash() != h)
      platform_fault(9502);
    checks += 2;
  }
  if (!gfx_guards())
    platform_fault(9503);
  /* Actual window/fullscreen transitions, cursor protection and clipped
   * presentation. */
  {
    Rect wr;
    int pass;
    SetRect(&wr, 96, 40, 400, 337);
    gameWindow = NewWindow(NULL, &wr, "\pSkiFree+ display test", true,
                           documentProc, (WindowPtr)-1, true, 0);
    if (!gameWindow)
      platform_fault(9600);
    ShowCursor();
    cursorHidden = 0;
    cursorTest = 1;
    for (pass = 0; pass < 3; pass++) {
      unsigned steps = 0, late = 0;
      int accum = 0;
      long next, finish;
      GameInfo before, after;
      before = game_info();
      toggle_display();
      after = game_info();
      if (before.x != after.x || before.y != after.y ||
          before.style != after.style || before.steps != after.steps)
        platform_fault(9601);
      game_init(1, 1000, 0);
      rendered = 0;
      render();
      start = TickCount();
      next = start;
      for (i = 0; i < 240; i++) {
        while ((long)(TickCount() - next) < 0) {
        }
        advance(&steps, 6 + pass, &accum, 3);
        render();
        finish = TickCount();
        if (finish > next + 3)
          late++;
        next += 3;
        if (finish - next >= FRAME_TICKS)
          next = finish;
      }
      while ((long)(TickCount() - next) < 0) {
      }
      end = TickCount();
      HideCursor();
      if (fullscreen) {
        if (!gfx_present_verified())
          platform_fault(9602);
      } else {
        int yy;
        for (yy = 0; yy < 297; yy++)
          if (memcmp(screen + (40 + yy) * 64 + 12, gfx_target() + yy * 64, 38))
            platform_fault(9603);
      }
      ShowCursor();
      if (!gfx_guards())
        platform_fault(9604);
      sprintf(line, "DISPLAY,%d,%d,240,%ld,%u,%08lx\n", pass, fullscreen,
              end - start, late, (unsigned long)game_hash());
      send(line);
    }
    cursorTest = 0;
  }
  /* Exercise the shipping Window Manager surface and event servicing,
     separately from the legacy renderer comparisons. Guest input is scripted.
   */
  {
    int pass;
    Rect bounds;
    wmTest = 1;
    cursorTest = 1;
    for (pass = 0; pass < 3; pass++) {
      unsigned steps = 0, late = 0;
      int accum = 0;
      long next, finish;
      if (pass < 2) {
        windowWidth = pass ? 480 : 304;
        SizeWindow(gameWindow, windowWidth, 297, true);
        MoveWindow(gameWindow, pass ? 16 : 96, 40, false);
        game_resize(windowWidth, windowHeight);
        SetRect(&bounds, 0, 0, windowWidth, 297);
        RectRgn(contentRegion, &bounds);
        SetPort(gameWindow);
      } else
        toggle_display();
      gfx_invalidate();
      game_init(1, 0, 0);
      rendered = 0;
      render();
      start = TickCount();
      next = start;
      for (i = 0; i < 240; i++) {
        EventRecord event;
        while ((long)(TickCount() - next) < 0) {
        }
        if (WaitNextEvent(everyEvent, &event, 0, NULL) &&
            event.what == updateEvt) {
          BeginUpdate((WindowPtr)event.message);
          EndUpdate((WindowPtr)event.message);
          gfx_invalidate();
        }
        SetPort(fullscreen ? fullWindow : gameWindow);
        advance(&steps, 9, &accum, 3);
        render();
        finish = TickCount();
        if (finish > next + 3)
          late++;
        next += 3;
        if (finish - next >= FRAME_TICKS)
          next = finish;
      }
      while ((long)(TickCount() - next) < 0) {
      }
      end = TickCount();
      HideCursor();
      {
        int yy, width = fullscreen ? 512 : windowWidth,
                height = fullscreen ? 342 : windowHeight,
                ox = fullscreen ? 0
                     : pass     ? 16
                                : 96,
                oy = fullscreen ? 0 : 40;
        for (yy = 0; yy < height; yy++)
          if (memcmp(screen + (oy + yy) * 64 + ox / 8, gfx_target() + yy * 64,
                     width / 8))
            platform_fault(9801 + pass);
      }
      ShowCursor();
      sprintf(line, "WM_DISPLAY,%d,%d,240,%ld,%u,%08lx\n", pass,
              fullscreen ? 512 : windowWidth, end - start, late,
              (unsigned long)game_hash());
      send(line);
    }
    toggle_display();
    cursorTest = 0;
  }
  /* 25 Hz: one simulation update and draw per scheduled callback.
     Rational deadlines avoid silently rounding 40 ms to 2 or 3 Mac ticks. */
  {
    int trial;
    wmTest = 0;
    cursorTest = 0;
    HideCursor();
    cursorHidden = 1;
    if (gameWindow)
      HideWindow(gameWindow);
    fullscreen = 1;
    SetPort(&fullPort);
    game_resize(512, 342);
    for (trial = 0; trial < 2; trial++)
      for (sc = 0; sc < 10; sc++) {
        unsigned late = 0, maxWork = 0;
        long next, finish, begin;
        game_init(1, xs[sc], ys[sc]);
        gfx_invalidate();
        rendered = 0;
        render();
        start = TickCount();
        next = start;
        for (i = 0; i < 300; i++) {
          while ((long)(TickCount() - next) < 0) {
          }
          begin = TickCount();
          replay_input(i, sc);
          game_step();
          render();
          finish = TickCount();
          if (finish - begin > maxWork)
            maxWork = finish - begin;
          next = start + ((i + 1) * 12 + 4) / 5;
          if (finish > next)
            late++;
        }
        while ((long)(TickCount() - next) < 0) {
        }
        end = TickCount();
        if (game_hash() != hashes[sc] || !gfx_present_verified() ||
            !gfx_verify(2))
          platform_fault(9900 + sc);
        sprintf(line, "PACE25,%d,%d,300,%ld,%u,%u,%08lx\n", sc, trial,
                end - start, late, maxWork, (unsigned long)game_hash());
        send(line);
      }
  }
  /* Shipping window/fullscreen paths, 25 Hz, matched 300-update runs. */
  {
    int view, trial;
    Rect bounds;
    wmTest = 1;
    cursorTest = 1;
    ShowCursor();
    cursorHidden = 0;
    /* Return from the legacy diagnostic surface to a real window. */
    fullscreen = 0;
    ShowWindow(gameWindow);
    SelectWindow(gameWindow);
    for (view = 0; view < 3; view++) {
      if (view < 2) {
        windowWidth = view ? 480 : 304;
        MoveWindow(gameWindow, view ? 16 : 96, 40, false);
        SizeWindow(gameWindow, windowWidth, 297, true);
        SetRect(&bounds, 0, 0, windowWidth, 297);
        RectRgn(contentRegion, &bounds);
        SetPort(gameWindow);
        game_resize(windowWidth, windowHeight);
      } else
        toggle_display();
      for (trial = 0; trial < 2; trial++)
        for (sc = 0; sc < 10; sc++) {
          unsigned late = 0, maxWork = 0;
          long next, finish, begin;
          game_init(1, xs[sc], ys[sc]);
          gfx_invalidate();
          rendered = 0;
          render();
          start = TickCount();
          next = start;
          for (i = 0; i < 300; i++) {
            EventRecord ev;
            while ((long)(TickCount() - next) < 0) {
            }
            begin = TickCount();
            if (WaitNextEvent(everyEvent, &ev, 0, NULL) &&
                ev.what == updateEvt) {
              BeginUpdate((WindowPtr)ev.message);
              EndUpdate((WindowPtr)ev.message);
              gfx_invalidate();
            }
            SetPort(fullscreen ? fullWindow : gameWindow);
            replay_input(i, sc);
            game_step();
            render();
            finish = TickCount();
            if (finish - begin > maxWork)
              maxWork = finish - begin;
            next = start + ((i + 1) * 12 + 4) / 5;
            if (finish > next)
              late++;
          }
          while ((long)(TickCount() - next) < 0) {
          }
          end = TickCount();
          HideCursor();
          {
            int yy, width = fullscreen ? 512 : windowWidth,
                    height = fullscreen ? 342 : windowHeight,
                    ox = fullscreen ? 0
                         : view     ? 16
                                    : 96,
                    oy = fullscreen ? 0 : 40;
            for (yy = 0; yy < height; yy++)
              if (memcmp(screen + (oy + yy) * 64 + ox / 8,
                         gfx_target() + yy * 64, width / 8))
                platform_fault(9950 + view);
          }
          ShowCursor();
          if (!gfx_guards())
            platform_fault(9954);
          sprintf(line, "WINDOW25,%d,%d,%d,300,%ld,%u,%u,%08lx\n", view, sc,
                  trial, end - start, late, maxWork,
                  (unsigned long)game_hash());
          send(line);
        }
    }
    toggle_display();
    cursorTest = 0;
  }
  /* Same event pump, GetKeys/GetMouse/Button, steering and click handler as
     play. Deterministic coordinates replace GetMouse's value only in this
     diagnostic. Host native pointer movement is checked separately on the
     playable binary. */
  {
    int view, scn, trial;
    Rect bounds;
    wmTest = 1;
    inputTest = 1;
    mouseTest = 1;
    cursorTest = 1;
    for (view = 0; view < 3; view++) {
      if (view < 2) {
        windowWidth = view ? 480 : 304;
        MoveWindow(gameWindow, view ? 16 : 96, 40, false);
        SizeWindow(gameWindow, windowWidth, 297, true);
        SetRect(&bounds, 0, 0, windowWidth, 297);
        RectRgn(contentRegion, &bounds);
        SetPort(gameWindow);
        game_resize(windowWidth, windowHeight);
      } else
        toggle_display();
      for (trial = 0; trial < 2; trial++)
        for (scn = 0; scn < 3; scn++) {
          unsigned late = 0, maxWork = 0;
          long next, finish, begin;
          uint32_t expected;
          int px = -32768, py = -32768;
          /* Untimed direct replay gives an independent input-routing state
           * check. */
          game_init(1, 1000, 0);
          for (i = 0; i < 300; i++) {
            int x, y, click;
            mouse_trace(i, scn, view == 2 ? 512 : windowWidth,
                        view == 2 ? 342 : 297, &x, &y, &click);
            if (x != px || y != py)
              game_mouse(x, y);
            if (click)
              game_input(INPUT_CLICK);
            px = x;
            py = y;
            game_step();
          }
          expected = game_hash();
          perfEnabled = trial;
          frame_stats_reset(&frameStats);
          perfRefresh = 1;
          game_init(1, 1000, 0);
          gfx_invalidate();
          rendered = 0;
          render();
          paused = backgrounded = 0;
          mouseScene = scn;
          mouseStep = 0;
          mouseApplied = -1;
          previousMouse.h = previousMouse.v = -32768;
          inputCalls = 0;
          coverage_reset();
          start = TickCount();
          next = start;
          for (i = 0; i < 300; i++) {
            while ((long)(TickCount() - next) < 0) {
              if (i)
                input();
            }
            mouseStep = i;
            begin = TickCount();
            input();
            game_step();
            coverage_step();
            render();
            finish = TickCount();
            if (perfEnabled &&
                frame_stats_record(&frameStats, (uint32_t)finish, 1))
              perfRefresh = 1;
            if (finish - begin > maxWork)
              maxWork = finish - begin;
            next = start + ((i + 1) * 12 + 4) / 5;
            if (finish > next)
              late++;
          }
          while ((long)(TickCount() - next) < 0) {
          }
          end = TickCount();
          if (game_hash() != expected || !gfx_guards())
            platform_fault(9960 + scn);
          HideCursor();
          {
            int yy, width = fullscreen ? 512 : windowWidth,
                    height = fullscreen ? 342 : windowHeight,
                    ox = fullscreen ? 0
                         : view     ? 16
                                    : 96,
                    oy = fullscreen ? 0 : 40;
            for (yy = 0; yy < height; yy++)
              if (memcmp(screen + (oy + yy) * 64 + ox / 8,
                         gfx_target() + yy * 64, width / 8))
                platform_fault(9970 + view);
          }
          ShowCursor();
          sprintf(line,
                  "MOUSE25,%d,%d,%d,300,%ld,%u,%u,%08lx,%lu,%u,%u,%u,%u\n",
                  view, scn, trial, end - start, late, maxWork,
                  (unsigned long)game_hash(), inputCalls, coverage.left,
                  coverage.right, coverage.air, coverage.flip);
          send(line);
        }
    }
    perfEnabled = 0;
    mouseTest = inputTest = 0;
    toggle_display();
    cursorTest = 0;
  }
  sprintf(line, "CHECKS,%u\n", checks);
  send(line);
  cleanup();
  send("SKIGAME_PASS\n");
  ExitToShell();
}
int main(void) {
  Rect r;
  MaxApplZone();
  InitGraf(&qd.thePort);
  InitFonts();
  InitWindows();
  InitMenus();
  InitDialogs(NULL);
  InitCursor();
  GetPort(&oldPort);
  if (qd.screenBits.rowBytes != 64 ||
      qd.screenBits.bounds.right - qd.screenBits.bounds.left != 512 ||
      qd.screenBits.bounds.bottom - qd.screenBits.bounds.top != 342) {
    DialogLines *t = dialog_lines_new();
    if (t) {
      dialog_line(t,
                  "This compact edition needs a 512 x 342 monochrome display.");
      native_text_dialog("\pSkiFree+", t, "OK");
      DisposePtr((Ptr)t);
    }
    ExitToShell();
    return 1;
  }
  screen = (unsigned char *)qd.screenBits.baseAddr;
  savedScreen = AUTOTEST ? (unsigned char *)NewPtr(21888) : NULL;
  if ((AUTOTEST && !savedScreen) || qd.screenBits.rowBytes != 64)
    ExitToShell();
  if (savedScreen)
    memcpy(savedScreen, screen, 21888);
  OpenPort(&fullPort);
  fullPort.portBits = qd.screenBits;
  fullPort.portRect = qd.screenBits.bounds;
  SetPort(&fullPort);
  SetRect(&r, 0, 0, 512, 342);
  ClipRect(&r);
  TextFont(3);
  TextSize(9);
  TextMode(srcCopy);
  if (AUTOTEST) {
    HideCursor();
    cursorHidden = 1;
  }
  portReady = 1;
  if (AUTOTEST) {
    SerShk h;
    long start;
    Create("\pSkiFree+ Bench Results", 0, 'ttxt', 'TEXT');
    if (FSOpen("\pSkiFree+ Bench Results", 0, &benchLog) ||
        SetEOF(benchLog, 0)) {
      benchLog = -1;
      benchLogError = 1;
    }
    OpenDriver("\p.AOut", &serialOut);
    SerReset(serialOut, baud9600 | data8 | noParity | stop10);
    memset(&h, 0, sizeof(h));
    Control(serialOut, kSERDHandshake, &h);
    start = TickCount();
    while (TickCount() - start < 180)
      SystemTask();
    send("BUILD,SkiFree+ compact 27\n");
    send("BOOT_GFX\n");
  }
  gfx_init(screen);
  send("BOOT_SIM\n");
  game_init(1, 0, 0);
  GetMouse(&previousMouse);
  {
    contentRegion = NewRgn();
    if (contentRegion) {
      SetRect(&r, 0, 0, 304, 297);
      RectRgn(contentRegion, &r);
    }
  }
  if (AUTOTEST) {
    send("BOOT_READY\n");
    autotest();
    return 0;
  }
  scores_read(&scores, "SkiFree Scores");
  menus_init();
  compact_view(0);
  toggle_display();
  if (!fullWindow) {
    cleanup();
    ExitToShell();
    return 1;
  }
  resetClock = 1;
  while (!quitGame) {
    input();
    if (quitGame)
      break;
    if (backgrounded || paused) {
      diagnostic_break(&diagnostics);
      timer_clock_stop();
      resetClock = 1;
      if (perfEnabled)
        frame_stats_reset(&frameStats);
      SystemTask();
      if (!backgrounded)
        render();
      continue;
    }
    if (resetClock) {
      diagnostic_break(&diagnostics);
      timer_clock_stop();
      deadline_start(&playDeadline, (uint32_t)TickCount());
      resetClock = 0;
      frame_stats_reset(&frameStats);
    }
    {
      unsigned count = deadline_steps(&playDeadline, (uint32_t)TickCount(), 8),
               j;
      uint32_t target, now;
      for (j = 0; j < count; j++)
        game_step();
      if (diagnostics.active) {
        pacingMerged += count - 1;
        if (count > pacingMaxSteps)
          pacingMaxSteps = count;
      }
      compose();
      /* Result dialogs can suspend play inside compose(). Discard that
         wall-time interval and restart the pacing epoch before presenting
         another frame. */
      if (quitGame)
        break;
      if (paused || backgrounded || resetClock) {
        diagnostic_break(&diagnostics);
        continue;
      }
      target = deadline_target(&playDeadline, 1);
      while (deadline_before((uint32_t)TickCount(), target)) {
      }
      present();
      now = (uint32_t)TickCount();
      deadline_presented(&playDeadline, now);
      if (perfEnabled || diagnostics.active) {
        if (diagnostics.active)
          diagnostic_frame(&diagnostics, now,
                           deadline_debt(&playDeadline, now));
        if (record_frame(now, count))
          perfRefresh = 1;
      }
    }
  }

  cleanup();
  ExitToShell();
  return 0;
}
