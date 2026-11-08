/* Adapter around the recovered game logic in engine.c. World coordinates and
 * collision rules belong to that engine; camera offsets affect presentation
 * only. Fixed pools avoid heap allocation during gameplay. Host test hooks
 * deliberately expose deterministic states without requiring Macintosh ROMs.
 */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "sim.h"
#include "camera.h"
#include "types.h"
#include "consts.h"
#include "data.h"
#include "prototypes.h"
#include "dimensions.h"
#include "logo_layout.h"
static Sprite spritePool[90];
static Actor actorPool[100];
static PermObject permPool[256];
static DrawItem drawItems[100];
static unsigned drawCount, stepCount;
static int cinematicCamera, cameraPhase, cameraOffset, cameraTriggered;
static int physicalWidth = 512, physicalHeight = 342;
static int scale75(int v) { return v >= 0 ? (v * 3) / 4 : -((-v * 3 + 3) / 4); }
static uint32_t randomState;
static int viewWidth = 512, viewHeight = 342;
#ifdef __m68k__
extern int perfKernel;
extern unsigned ski_random_asm(uint32_t *, unsigned);
#endif
static short sim_random(short max) {
  if (max <= 0) {
    platform_fault(9001);
    return 0;
  }
#ifdef __m68k__
  if (perfKernel & 64)
    return (short)ski_random_asm(&randomState, (unsigned)max);
#endif
  randomState = randomState * 214013u + 2531011u;
  return (short)(((randomState >> 16) & 32767u) % (unsigned)max);
}
#define ski_assert(exp, line)                                                  \
  do {                                                                         \
    if (!(exp))                                                                \
      platform_fault(line);                                                    \
  } while (0)
void assertFailed(const char *s, int line) { platform_fault(line); }
void playSound(Sound *s) {}
void startGameTimer(void) {}
void formatAndPrintStatusStrings(HDC p) {}
int showErrorMessage(char *s) {
  platform_fault(9002);
  return 0;
}
static void capture_sprite(int id, int x, int y) {
  if (id < 1 || id >= 90) {
    platform_fault(9003);
    return;
  }
  if (drawCount >= 100) {
    platform_fault(9004);
    return;
  }
  y += cameraOffset;
  /* Keep opening logo/credit and version below the 16-pixel HUD strip. */
  if (cinematicCamera && (id == 53 || id == 54))
    y += 16;
  /* Prefix is decorative: keep original wordmark and credit positions. */
  if (id == 53)
    x -= LOGO_PREFIX;
  drawItems[drawCount++] = (DrawItem){id, x, y};
}
static int gateResult;
static unsigned gateResultUntil;
static void gate_feedback(int sprite) {
  gateResult = sprite == 25 ? 1 : 2;
  gateResultUntil = stepCount + 25;
}
int game_gate_feedback(void) {
  return stepCount < gateResultUntil ? gateResult : 0;
}
int game_gate_flash(void) {
  unsigned left;
  if (!game_gate_feedback())
    return 0;
  left = gateResultUntil - stepCount;
  return left > 21 || (left <= 18 && left > 14);
}
static int snapshotActors;
void game_set_snapshot_actors(int enabled) { snapshotActors = enabled; }
static short actor_previous_y(Actor *a) {
  return snapshotActors ? (a->hasPreviousY ? a->previousY : a->yPosMaybe)
                        : getLinkedActorIfExists(a)->yPosMaybe;
}
#include "engine.c"
#include "paint_fast.h"
static int fastPaint;
void game_set_fast_paint(int enabled) { fastPaint = enabled; }
static void capture_frame(void) {
  Actor *a;
  Actor *cameraActors[100];
  unsigned char painted[100];
  int cameraCount = 0;
  RECT visible = windowClientRect;
  drawCount = 0;
  visible.top -= cameraOffset;
  visible.bottom -= cameraOffset;
  /* The SDL port's paintActors forces a complete paint and preserves original
   * ordering/bookkeeping. */
  for (a = actorListPtr; a; a = a->next) {
    RECT *r = (a->flags & FLAG_4) ? &a->someRect : updateActorSpriteRect(a);
    int across = r->right > 0 && r->left < viewWidth,
        shown = across && r->bottom > visible.top && r->top < visible.bottom;
    if (cinematicCamera && snapshotActors) {
      int original = across && r->bottom > 0 && r->top < viewHeight;
      if (original != shown) {
        cameraActors[cameraCount] = a;
        painted[cameraCount++] = (a->flags & FLAG_1) || original;
      }
    }
    if (shown)
      a->flags &= ~FLAG_1;
  }
  if (fastPaint)
    drawWindowFast(NULL, &visible);
  else
    drawWindow(NULL, &visible);
  if (snapshotActors)
    for (a = actorListPtr; a; a = a->next)
      a->hasPreviousY = 0;
  while (cameraCount) {
    cameraCount--;
    a = cameraActors[cameraCount];
    a->flags = (a->flags & ~FLAG_1) | (painted[cameraCount] ? FLAG_1 : 0);
  }
}
/* Presentation-only camera. The original world, population bounds and collision
   anchor are untouched. Both extreme views stay inside the 120-pixel margin. */
static void camera_reset(void) {
  cameraPhase = cameraTriggered = 0;
  cameraOffset =
      cinematicCamera ? viewHeight / 5 - SKI_CAMERA_Y(viewHeight) : 0;
}
static void camera_step(void) {
  static const unsigned short ease[101] = {
      0,   0,   0,   1,   1,   2,   3,   4,   5,   6,   7,   9,   10,  12,  14,
      16,  18,  20,  22,  24,  27,  29,  32,  34,  37,  40,  43,  46,  49,  52,
      55,  59,  62,  65,  69,  72,  76,  79,  83,  86,  90,  94,  98,  101, 105,
      109, 113, 116, 120, 124, 128, 132, 136, 140, 143, 147, 151, 155, 158, 162,
      166, 170, 173, 177, 180, 184, 187, 191, 194, 197, 201, 204, 207, 210, 213,
      216, 219, 222, 224, 227, 229, 232, 234, 236, 238, 240, 242, 244, 246, 247,
      249, 250, 251, 252, 253, 254, 255, 255, 256, 256, 256};
  int normal, target;
  long aheadX, aheadY;
  if (!cinematicCamera)
    return;
  if (playerActor && !cameraTriggered) {
    aheadX = (long)playerX + (long)playerActor->HorizontalVelMaybe * 100;
    aheadY = (long)playerY + (long)playerActor->verticalVelocityMaybe * 100;
    if (aheadY >= 32000 || aheadY <= -2000 || aheadX >= 16000 ||
        aheadX <= -16000)
      cameraTriggered = 1;
  }
  if (!cameraTriggered || cameraPhase == 100)
    return;
  /* Offline cubic ease table avoids division in the live loop. */
  target = ease[++cameraPhase];
  normal = viewHeight / 5;
  cameraOffset = normal - SKI_CAMERA_Y(viewHeight) +
                 ((viewHeight / 2 - normal) * target + 128) / 256;
}
void game_set_camera(int enabled) {
  cinematicCamera = enabled;
  camera_reset();
  capture_frame();
}
int game_camera_shift(void) { return cameraOffset; }
/* -1 means no marker. It appears only once the entire airborne skier is above
   the visible field, at the projected ground position rather than the sprite.
 */
int game_shadow(int top, int *groundY) {
  int bottom, height;
  if (!playerActor || !playerActor->isInAir)
    return -1;
  bottom = skierScreenYOffset + cameraOffset - playerActor->isInAir;
  if (bottom > top)
    return -1;
  *groundY = skierScreenYOffset + cameraOffset;
  height = top - bottom;
  return height < 16 ? 0 : height < 40 ? 1 : height < 80 ? 2 : 3;
}
int game_air_pose(int top) {
  if (!playerActor || !playerActor->isInAir)
    return 0;
  if (skierScreenYOffset + cameraOffset - playerActor->isInAir > top)
    return 0;
  return playerActor->spriteIdx2;
}
void game_init(uint32_t seed, int x, int y) {
  int i;
  memset(spritePool, 0, sizeof(spritePool));
  memset(actorPool, 0, sizeof(actorPool));
  memset(permPool, 0, sizeof(permPool));
  gateResult = 0;
  gateResultUntil = 0;
  camera_reset();
  sprites = spritePool;
  actors = actorPool;
  permObjects = permPool;
  sim_ticks = 0;
  stepCount = 0;
  randomState = seed;
  for (i = 1; i < 90; i++) {
    sprites[i].width = dims[i][0];
    sprites[i].height = dims[i][1];
    sprites[i].totalPixels = dims[i][0] * dims[i][1];
    sprites[i].sheet = (void *)(uintptr_t)i;
  }
  windowClientRect = (RECT){0, 0, viewWidth, viewHeight};
  windowClientRectWith120Margin =
      (RECT){-120, -120, viewWidth + 120, viewHeight + 120};
  windowWidth = viewWidth;
  windowHeight = viewHeight;
  skierScreenXOffset = viewWidth / 2;
  skierScreenYOffset = SKI_CAMERA_Y(viewHeight);
  /* Preserve source-port population rule; original padded-area formula audit is
   * documented. */
  windowWithMarginTotalArea = 582 * 582;
  resetGame();
  setupGame();
  if (x || y)
    updateActorPositionMaybe(playerActor, x, y, 0);
  DAT_0040c714 = 0;
  DAT_0040c5d8 = 0;
  DAT_0040c760 = 0;
  capture_frame();
}
/* One original-time quantum. Camera movement follows simulation but never changes actor positions or collision rectangles. */
void game_step(void) {
  sim_ticks += 40;
  prevTickCount = currentTickCount;
  currentTickCount = sim_ticks;
  timerFrameDurationInMillis = 40;
  updateGameState();
  stepCount++;
  camera_step();
  capture_frame();
}
/* Keep click and keyboard jump impulses distinct: the recovered controls use different grounded hops. In air, input advances the actual trick frame. */
void game_input(int key) {
  uint32_t f;
  short air;
  if (key == INPUT_CLICK && !playerActor) {
    game_init(1, 0, 0);
    return;
  }
  if (key == INPUT_FAST) {
    isTurboMode = !isTurboMode;
    return;
  }
  if (!playerActor)
    return;
  f = playerActor->frameNo;
  air = playerActor->isInAir;
  if (f == 11 || (f == 17 && !(key == INPUT_CLICK && !air)))
    return;
  if (key == INPUT_LEFT) {
    f = playerTurnFrameNoTbl[f].leftFrameNo;
    if (f == 7)
      playerActor->HorizontalVelMaybe =
          max_(playerActor->HorizontalVelMaybe - 8, -8);
  }
  if (key == INPUT_RIGHT) {
    f = playerTurnFrameNoTbl[f].rightFrameNo;
    if (f == 8)
      playerActor->HorizontalVelMaybe =
          playerActor->HorizontalVelMaybe + 8 > 8
              ? 8
              : playerActor->HorizontalVelMaybe + 8;
  }
  if (key == INPUT_DOWN) {
    if (!air)
      f = 0;
    else if (f == 13)
      f = 19;
    else if (f == 20)
      f = 14;
    else if (f == 21)
      f = 15;
    else if (f == 18)
      f = 13;
    else if (f == 19)
      f = 18;
  }
  if (key == INPUT_UP) {
    if (f == 13)
      f = 18;
    else if (f == 19)
      f = 13;
    else if (f == 14)
      f = 20;
    else if (f == 15)
      f = 21;
    else if (f == 18)
      f = 19;
    else if (f == 3 || f == 7 || f == 12) {
      if (!playerActor->verticalVelocityMaybe) {
        f = 9;
        playerActor->verticalVelocityMaybe = -4;
      }
    } else if (f == 6 || f == 8) {
      if (!playerActor->verticalVelocityMaybe) {
        f = 10;
        playerActor->verticalVelocityMaybe = -4;
      }
    }
  }
  if (key == INPUT_JUMP) {
    if (!air) {
      playerActor->inAirCounter = 2;
      if (playerActor->verticalVelocityMaybe > 4)
        playerActor->verticalVelocityMaybe -= 4;
      f = 13;
    } else if (f == 13)
      f = 18;
    else if (f == 18)
      f = 19;
    else if (f == 19)
      f = 13;
    else if (f == 14)
      f = 20;
    else if (f == 15)
      f = 21;
  }
  if (key == INPUT_KEYPAD_JUMP && !air) {
    playerActor->inAirCounter = 2;
    if (playerActor->verticalVelocityMaybe > 4)
      playerActor->verticalVelocityMaybe -= 4;
    f = 13;
  }
  if (key == INPUT_CLICK) {
    if (!air) {
      playerActor->inAirCounter = 4;
      f = 13;
    } else if (f == 13)
      f = 18;
    else if (f == 18)
      f = 19;
    else if (f == 19)
      f = 13;
    else if (f == 14)
      f = 20;
    else if (f == 15)
      f = 21;
  }
  if (!air) {
    if (key == INPUT_SW)
      f = 1;
    if (key == INPUT_SE)
      f = 4;
    if (key == INPUT_W)
      f = 3;
    if (key == INPUT_E)
      f = 6;
  }
  setActorFrameNo(playerActor, f);
}
void game_mouse(int x, int y) {
  int dx = x - skierScreenXOffset, dy = y - (skierScreenYOffset + cameraOffset),
      f;
  if (!playerActor || playerActor->frameNo == 11 || playerActor->frameNo == 17)
    return;
  if (playerActor->isInAir) {
    if (dx < 0)
      f = dy < 0 ? (dy < dx ? 16 : 14) : (-dy < dx ? 13 : 14);
    else
      f = dy < 0 ? (-dy <= dx ? 15 : 16) : (dy <= dx) + 13;
  } else if (dy > 0) {
    if (!dx)
      f = 0;
    else {
      int q = dy * 4 / dx;
      f = q <= -12  ? 0
          : q <= -6 ? 1
          : q <= -3 ? 2
          : q <= -1 ? 3
          : q >= 12 ? 0
          : q >= 6  ? 4
          : q >= 3  ? 5
          : q >= 1  ? 6
          : dx < 0  ? 3
                    : 6;
    }
  } else
    f = dx < 0 ? 3 : 6;
  setActorFrameNo(playerActor, f);
}
const DrawItem *game_items(unsigned *n) {
  *n = drawCount;
  return drawItems;
}
GameInfo game_info(void) {
  GameInfo g;
  g.x = playerX;
  g.y = playerY;
  g.speed =
      playerActor ? playerActor->verticalVelocityMaybe * 1000 / (40 * 16) : 0;
  g.style = stylePoints;
  g.elapsed = elapsedTime;
  g.alive = playerActor != NULL;
  g.air = playerActor ? playerActor->isInAir : 0;
  g.turbo = isTurboMode;
  g.frame = playerActor ? playerActor->frameNo : -1;
  g.mode = isSsGameMode ? 1 : isGsGameMode ? 2 : isFsGameMode ? 3 : 0;
  g.resultMode = INT_0040c964 ? 1 : INT_0040c960 ? 2 : INT_0040c968 ? 3 : 0;
  g.resultValue = g.resultMode == 3 ? stylePoints : elapsedTime;
  g.distance = (g.mode == 1                    ? 8640 - g.y
                : (g.mode == 2 || g.mode == 3) ? 16640 - g.y
                                               : g.y) /
               16;
  g.objects = drawCount;
  g.steps = stepCount;
  return g;
}
static uint32_t hash_word(uint32_t h, uint32_t v) {
  return (h ^ v) * 16777619u;
}
uint32_t game_hash(void) {
  uint32_t h = 2166136261u;
  Actor *a;
  unsigned i;
#define H(v) h = hash_word(h, (uint32_t)(v))
  H(randomState);
  H(playerX);
  H(playerY);
  H(stylePoints);
  H(elapsedTime);
  H(stepCount);
  H(playerActor ? playerActor - actors : -1);
  H(isSsGameMode);
  H(isGsGameMode);
  H(isFsGameMode);
  for (a = actorListPtr; a; a = a->next) {
    H(a - actors);
    H(a->typeMaybe);
    H(a->spriteIdx2);
    H(a->xPosMaybe);
    H(a->yPosMaybe);
    H(a->HorizontalVelMaybe);
    H(a->verticalVelocityMaybe);
    H(a->isInAir);
    H(a->frameNo);
    H(a->flags);
  }
  for (i = 0; i < permObjectCount; i++) {
    PermObject *p = &permObjects[i];
    H(p->actorTypeMaybe);
    H(p->maybeX);
    H(p->maybeY);
    H(p->actorFrameNo);
    H(p->unk_0x18);
    H(p->unk_0x1e);
    H(p->unk_0x20);
  }
  for (i = 0; i < drawCount; i++) {
    H(drawItems[i].sprite);
    H(drawItems[i].x);
    H(drawItems[i].y);
  }
#undef H
  return h;
}

void game_resize(int w, int h) {
  Actor *a;
  w = 512;
  h = 342;
  physicalWidth = w;
  physicalHeight = h;
  viewWidth = w;
  viewHeight = h;
  windowWidth = w;
  windowHeight = h;
  windowClientRect = (RECT){0, 0, w, h};
  windowClientRectWith120Margin = (RECT){-120, -120, w + 120, h + 120};
  skierScreenXOffset = w / 2;
  skierScreenYOffset = SKI_CAMERA_Y(h);
  camera_reset();
  for (a = actorListPtr; a; a = a->next)
    a->flags &= ~FLAG_4;
  capture_frame();
}

void game_start_course(int mode) {
  game_init(1, mode == 1 ? -448 : mode == 2 ? 416 : mode == 3 ? 0 : 1000, 0);
}

int game_course_test(void) {
  int mode;
  for (mode = 1; mode <= 3; mode++) {
    int x = mode == 1 ? -448 : mode == 2 ? 416 : 0;
    game_start_course(mode);
    playerActor->xPosMaybe = x;
    playerActor->yPosMaybe = 650;
    currentTickCount = 1000;
    prevTickCount = 960;
    updateSsGameMode(playerActor, x, 630);
    updateGsGameMode(playerActor, x, 630);
    updateFsGameMode(playerActor, x, 630);
    if (game_info().mode != mode)
      return 100 + mode;
    if (mode != 3) {
      uint32_t before = timedGameRelated;
      int gx = currentSlalomFlag->maybeX +
               (currentSlalomFlag->spriteIdx == 0x17 ? 20 : -20),
          gy = currentSlalomFlag->maybeY;
      playerActor->xPosMaybe = gx;
      playerActor->yPosMaybe = gy + 1;
      if (mode == 1)
        updateSsGameMode(playerActor, gx, gy - 1);
      else
        updateGsGameMode(playerActor, gx, gy - 1);
      if ((uint32_t)timedGameRelated != before - 5000u)
        return 200 + mode;
    } else
      stylePoints = 123;
    currentTickCount = 50000;
    prevTickCount = 49960;
    playerActor->xPosMaybe = x;
    playerActor->yPosMaybe = mode == 1 ? 8650 : 16650;
    updateSsGameMode(playerActor, x, mode == 1 ? 8630 : 16630);
    updateGsGameMode(playerActor, x, mode == 1 ? 8630 : 16630);
    updateFsGameMode(playerActor, x, mode == 1 ? 8630 : 16630);
    if (game_info().resultMode != mode)
      return 300 + mode;
    if (mode == 3 && game_info().resultValue != 123)
      return 403;
    if (mode != 3 && game_info().resultValue != 54000)
      return 400 + mode;
    game_start_course(mode);
    if (game_info().resultMode || game_info().mode)
      return 500 + mode;
  }
  return 0;
}

int game_compact(void) { return 0; }
void game_set_compact(int enabled) { game_resize(512, 342); }

unsigned game_steps(void) { return stepCount; }
int game_has_result(void) {
  return INT_0040c964 || INT_0040c960 || INT_0040c968;
}

void game_profile_step(unsigned long (*clock)(void), unsigned long *core,
                       unsigned long *paint) {
  unsigned long t;
  sim_ticks += 40;
  prevTickCount = currentTickCount;
  currentTickCount = sim_ticks;
  timerFrameDurationInMillis = 40;
  t = clock();
  updateGameState();
  *core += clock() - t;
  stepCount++;
  t = clock();
  capture_frame();
  *paint += clock() - t;
}

#if AUTOTEST
/* Explicit synthetic airborne stress fixture, never included in playable code.
 */
void game_test_gate(int missed) { gate_feedback(missed ? 26 : 25); }
void game_test_jump(void) {
  if (playerActor) {
    updateActorPositionMaybe(playerActor, playerActor->xPosMaybe,
                             playerActor->yPosMaybe, 180);
    playerActor->inAirCounter = 0;
    setActorFrameNo(playerActor, 13);
  }
}
#endif
