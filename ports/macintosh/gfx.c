/* Compact-Mac 1-bit renderer: 512 x 342 pixels, 64 bytes per scanline.
 * Persistent pixels plus dirty tiles avoid clearing/copying the whole screen.
 * Erasing an old sprite or overlay must also redraw scenery beneath it.
 * Generated, pre-shifted sprite code trades resource size for 68000 time;
 * clipped edges use the masked fallback. AUTOTEST retains a pixel reference.
 */
#ifndef AUTOTEST
#define AUTOTEST 0
#endif
#include <string.h>
#include <stdint.h>
#include <Resources.h>
#include <Memory.h>
#include "sim.h"
#include "gfx.h"
typedef uint8_t U8;
typedef uint16_t U16;
typedef void (*BlitFn)(U8 *, long);
typedef struct {
  int w, h, words;
  const U16 *mask, *bits;
  const U16 **phases;
  BlitFn *compiled;
} GfxSprite;
static BlitFn bankFunctions[90][16];
static Handle blitBank;
#define art original_art
#include "bank_assets.h"
#undef art
/* Alternate-scale bank lives in the separate advanced edition. */
static GfxSprite *art = original_art;
extern void clear_asm(void *, long);
extern void phase_build_asm(U16 *, const U16 *, const U16 *, long, long, long);
extern void masked_clip_asm(U8 *, const U16 *, long, long, long, long);
#define CAP 21888
/* Guarded overscan rows let compiled sprites cross vertical edges safely. */
#define PAD (64 * 64)
#define FRAME (PAD + 32)
extern int perfKernel;
unsigned gfxCacheWords;
/* The 128-slot experiment lost; retain only the selected 32 slots. */
static U16 phaseCache[32][800], phaseKey[32];
/* Only the widened title needs more than 800 words; do not grow every slot. */
static U16 titlePhase[1024], titlePhaseKey;

static void phase_build_c(U16 *cache, const U16 *mask, const U16 *bits, long h,
                          long words, long ph) {
  long yy, jj;
  for (yy = 0; yy < h; yy++)
    for (jj = 0; jj < words; jj++) {
      long ix = yy * words + jj;
      U16 m = mask[ix] >> ph, b = bits[ix] >> ph;
      if (ph && jj) {
        m |= (U16)(mask[ix - 1] << (16 - ph));
        b |= (U16)(bits[ix - 1] << (16 - ph));
      }
      cache[ix * 2] = m ^ 65535;
      cache[ix * 2 + 1] = b;
    }
}
int gfx_phase_test(void) {
  int id, ph, j;
  static U16 expected[1024];
  for (id = 1; id < 90; id++) {
    GfxSprite *s = &art[id];
    if (s->words * s->h * 2 > (id == 53 ? 1024 : 800))
      return 1;
    for (ph = 0; ph < 16; ph++) {
      phase_build_c(expected, s->mask, s->bits, s->h, s->words, ph);
      phase_build_asm(titlePhase, s->mask, s->bits, s->h, s->words, ph);
      for (j = 0; j < s->h * s->words * 2; j++)
        if (expected[j] != titlePhase[j])
          return id * 16 + ph;
    }
  }
  memset(phaseKey, 0, sizeof(phaseKey));
  titlePhaseKey = 0;
  return 0;
}
unsigned gfx_phase_bench(int mode) {
  int i;
  unsigned sum = 0;
  GfxSprite *s = &art[49];
  for (i = 0; i < 256; i++) {
    if (mode)
      phase_build_asm(phaseCache[0], s->mask, s->bits, s->h, s->words, i & 15);
    else
      phase_build_c(phaseCache[0], s->mask, s->bits, s->h, s->words, i & 15);
    sum += phaseCache[0][50];
  }
  memset(phaseKey, 0, sizeof(phaseKey));
  titlePhaseKey = 0;
  return sum;
}
static U8 *screen;
static U8 buffer[CAP + 64 + 2 * PAD], reference[CAP + 64 + 2 * PAD];
static uint32_t dirty[22], eraseNext[22], erased[22];
static int compositionInvalid = 1;
static DrawItem previous[100];
static unsigned previousCount;
static int firstFrame = 1;
extern void clear_rect_asm(void *, long, long, long);
extern void present_rect_asm(void *, const void *, long, long, long);
U8 *gfx_target(void) { return buffer + FRAME; }
static const uint32_t edgeMask[33] = {
    0x0u,        0x1u,        0x3u,       0x7u,       0xfu,       0x1fu,
    0x3fu,       0x7fu,       0xffu,      0x1ffu,     0x3ffu,     0x7ffu,
    0xfffu,      0x1fffu,     0x3fffu,    0x7fffu,    0xffffu,    0x1ffffu,
    0x3ffffu,    0x7ffffu,    0xfffffu,   0x1fffffu,  0x3fffffu,  0x7fffffu,
    0xffffffu,   0x1ffffffu,  0x3ffffffu, 0x7ffffffu, 0xfffffffu, 0x1fffffffu,
    0x3fffffffu, 0x7fffffffu, 0xffffffffu};
static uint32_t span_mask(int l, int r) {
  if (perfKernel & 4194304)
    return edgeMask[r] ^ edgeMask[l];
  return (0xffffffffu << l) & (r == 32 ? 0xffffffffu : ((1u << r) - 1));
}
static void mark(uint32_t *map, int x, int y, int w, int h) {
  int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y, right = x + w, bottom = y + h,
      row, l, r;
  uint32_t mask;
  if (right > 512)
    right = 512;
  if (bottom > 342)
    bottom = 342;
  if (right <= left || bottom <= top)
    return;
  l = left >> 4;
  r = (right + 15) >> 4;
  mask = span_mask(l, r);
  for (row = top >> 4; row < (bottom + 15) / 16; row++)
    map[row] |= mask;
}
int gfx_intersects(int x, int y, int w, int h) {
  int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y, right = x + w, bottom = y + h,
      l, r, row;
  uint32_t mask;
  if (right > 512)
    right = 512;
  if (bottom > 342)
    bottom = 342;
  if (right <= left || bottom <= top)
    return 0;
  l = left >> 4;
  r = (right + 15) >> 4;
  mask = span_mask(l, r);
  for (row = top >> 4; row < (bottom + 15) / 16; row++)
    if ((dirty[row] | erased[row]) & mask)
      return 1;
  return 0;
}
void gfx_dirty(int x, int y, int w, int h) { mark(dirty, x, y, w, h); }
void gfx_erase_next(int x, int y, int w, int h) { mark(eraseNext, x, y, w, h); }
static const unsigned char trailingZeros[256] = {
    8, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0,
    3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, 0, 1, 0, 2, 0, 1, 0,
    3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0,
    3, 0, 1, 0, 2, 0, 1, 0, 7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0,
    3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0,
    3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0};
static void erase_regions(void) {
  int row, x, r, y, h;
  for (row = 0; row < 22; row++) {
    uint32_t mask = dirty[row] | eraseNext[row];
    x = 0;
    y = row * 16;
    h = y + 16 <= 342 ? 16 : 342 - y;
    if (perfKernel & 2097152) {
      while (row < 21 && (dirty[row + 1] | eraseNext[row + 1]) == mask) {
        row++;
        h += row == 21 ? 6 : 16;
      }
    }
    while (mask) {
      if (perfKernel & 8) {
        int skip;
        while (!(mask & 255)) {
          mask >>= 8;
          x += 8;
        }
        skip = trailingZeros[mask & 255];
        mask >>= skip;
        x += skip;
      } else
        while (!(mask & 1)) {
          mask >>= 1;
          x++;
        }
      r = x;
      if (perfKernel & 8388608) {
        int take;
        while ((mask & 255) == 255) {
          mask >>= 8;
          x += 8;
        }
        take = trailingZeros[(~mask) & 255];
        mask >>= take;
        x += take;
      } else
        while (mask & 1) {
          mask >>= 1;
          x++;
        }
      clear_rect_asm(buffer + FRAME + y * 64 + r * 2, (x - r) * 2, h, 64);
    }
  }
}
void gfx_present(int mode) {
  int row, x, r, y, h, yy, ww;
  U8 *src = buffer + FRAME;
  for (row = 0; row < 22; row++) {
    uint32_t mask = dirty[row];
    x = 0;
    y = row * 16;
    h = y + 16 <= 342 ? 16 : 342 - y;
    if (perfKernel & 2097152) {
      while (row < 21 && dirty[row + 1] == mask) {
        row++;
        h += row == 21 ? 6 : 16;
      }
    }
    while (mask) {
      if (perfKernel & 8) {
        int skip;
        while (!(mask & 255)) {
          mask >>= 8;
          x += 8;
        }
        skip = trailingZeros[mask & 255];
        mask >>= skip;
        x += skip;
      } else
        while (!(mask & 1)) {
          mask >>= 1;
          x++;
        }
      r = x;
      if (perfKernel & 8388608) {
        int take;
        while ((mask & 255) == 255) {
          mask >>= 8;
          x += 8;
        }
        take = trailingZeros[(~mask) & 255];
        mask >>= take;
        x += take;
      } else
        while (mask & 1) {
          mask >>= 1;
          x++;
        }
      ww = (x - r) * 2;
      if (mode == 2)
        present_rect_asm(screen + y * 64 + r * 2, src + y * 64 + r * 2, ww, h,
                         64);
      else
        for (yy = 0; yy < h; yy++) {
          U16 *d = (U16 *)(screen + (y + yy) * 64 + r * 2);
          const U16 *p = (const U16 *)(src + (y + yy) * 64 + r * 2);
          int n = ww / 2;
          while (n--)
            *d++ = *p++;
        }
    }
  }
  firstFrame = 0;
}
int gfx_present_verified(void) {
  return !memcmp(screen, buffer + FRAME, CAP) && gfx_guards();
}
static void inset_init(void);
void gfx_init(U8 *p) {
  gfx_set_compact(0);
  inset_init();
  screen = p;
  memset(buffer, 0xa5, sizeof(buffer));
  if (AUTOTEST)
    memset(reference, 0xa5, sizeof(reference));
}
static void draw(U8 *d, int id, int x, int y, int mode) {
  GfxSprite *s = &art[id];
  int wx = x >> 4, first = wx < 0 ? -wx : 0, last = s->words,
      top = y < 0 ? -y : 0, bottom = s->h, r, j, words;
  const U16 *q;
  U8 *dst;
  /* Do not shift an entire clipped sprite on a cache miss when only the
     vertical edge clips it: the existing compiled bank can write overscan. */
  if ((perfKernel & 1048576) && mode >= 1 && x >= 0 && y < 342 &&
      y + s->h > 0 && s->h <= 64 && x + s->w <= 512 &&
      (wx + s->words) * 2 <= 64) {
    s->compiled[x & 15](d + y * 64 + wx * 2, 64);
    return;
  }
  if (mode >= 1 && x >= 0 && y >= 0 && x + s->w <= 512 && y + s->h <= 342 &&
      (wx + s->words) * 2 <= 64) {
    s->compiled[x & 15](d + y * 64 + wx * 2, 64);
    return;
  }
  if (wx + last > 32)
    last = 32 - wx;
  if (y + bottom > 342)
    bottom = 342 - y;
  if (first >= last || top >= bottom)
    return;
  {
    int ph = x & 15, yy, jj, key = id * 16 + ph, slot = (id * 17 + ph) & 31;
    U16 *cache = id == 53 ? titlePhase : phaseCache[slot];
    U16 *keyPtr = id == 53 ? &titlePhaseKey : &phaseKey[slot];
    if (*keyPtr != key) {
      gfxCacheWords += s->h * s->words;
      if (mode == 2)
        phase_build_asm(cache, s->mask, s->bits, s->h, s->words, ph);
      else
        for (yy = 0; yy < s->h; yy++)
          for (jj = 0; jj < s->words; jj++) {
            int ix = yy * s->words + jj;
            U16 mask = s->mask[ix] >> ph, bits = s->bits[ix] >> ph;
            if (ph && jj) {
              mask |= (U16)(s->mask[ix - 1] << (16 - ph));
              bits |= (U16)(s->bits[ix - 1] << (16 - ph));
            }
            cache[ix * 2] = mask ^ 65535;
            cache[ix * 2 + 1] = bits;
          }
      *keyPtr = key;
    }
    q = cache + (top * s->words + first) * 2;
  }
  dst = d + (y + top) * 64 + (wx + first) * 2;
  words = last - first;
  if (mode == 2) {
    masked_clip_asm(dst, q, bottom - top, words, 64, (s->words - words) * 4);
    return;
  }
  for (r = top; r < bottom; r++) {
    U16 *p = (U16 *)dst;
    for (j = 0; j < words; j++)
      p[j] = (p[j] & q[j * 2]) | q[j * 2 + 1];
    dst += 64;
    q += s->words * 2;
  }
}
static void pixel(U8 *d, int id, int x, int y) {
  GfxSprite *s = &art[id];
  int xx, yy;
  for (yy = 0; yy < s->h; yy++)
    for (xx = 0; xx < s->w; xx++) {
      int dx = x + xx, dy = y + yy;
      U16 b = 0x8000u >> (xx & 15);
      int ix = yy * s->words + (xx >> 4);
      if (dx >= 0 && dx < 512 && dy >= 0 && dy < 342 && (s->mask[ix] & b)) {
        U8 mask = 0x80u >> (dx & 7);
        U8 *p = d + dy * 64 + (dx >> 3);
        if (s->bits[ix] & b)
          *p |= mask;
        else
          *p &= (U8)~mask;
      }
    }
}
static int item_changed(const DrawItem *a, const DrawItem *b) {
  if (!(perfKernel & 4194304))
    return memcmp(a, b, sizeof(*a));
  return a->sprite != b->sprite || a->x != b->x || a->y != b->y;
}
/* Reconstruct damaged background before redrawing intersecting actors in draw-list order; skipping covered actors would leave holes after movement. */
void gfx_frame(int mode) {
  unsigned i, n;
  const DrawItem *items = game_items(&n);
  U8 *dst = buffer + FRAME;
  gfxCacheWords = 0;
  memset(dirty, 0, sizeof(dirty));
  if (firstFrame) {
    gfx_dirty(0, 0, 512, 342);
  }
  for (i = 0; i < previousCount; i++) {
    GfxSprite *s = &art[previous[i].sprite];
    if (i >= n || item_changed(&previous[i], &items[i]))
      gfx_dirty(previous[i].x, previous[i].y, s->w, s->h);
  }
  if (perfKernel & 2048) {
    if (firstFrame || compositionInvalid)
      memset(erased, 255, sizeof(erased));
    else
      memcpy(erased, eraseNext, sizeof(erased));
  }
  if (firstFrame || compositionInvalid)
    clear_asm(dst, CAP);
  else
    erase_regions();
  compositionInvalid = 0;
  memset(eraseNext, 0, sizeof(eraseNext));
  for (i = 0; i < n; i++) {
    GfxSprite *s = &art[items[i].sprite];
    int changed = i >= previousCount || item_changed(&previous[i], &items[i]);
    if (changed)
      gfx_dirty(items[i].x, items[i].y, s->w, s->h);
    if (!(perfKernel & 2048) || changed ||
        gfx_intersects(items[i].x, items[i].y, s->w, s->h))
      draw(dst, items[i].sprite, items[i].x, items[i].y, mode);
  }
  memcpy(previous, items, n * sizeof(DrawItem));
  previousCount = n;
}
int gfx_guards(void) {
  int i;
  for (i = 0; i < 32; i++)
    if (buffer[i] != 0xa5 || reference[i] != 0xa5 ||
        buffer[CAP + 32 + 2 * PAD + i] != 0xa5 ||
        reference[CAP + 32 + 2 * PAD + i] != 0xa5)
      return 0;
  return 1;
}
int gfx_verify(int mode) {
  memset(phaseKey, 0, sizeof(phaseKey));
  titlePhaseKey = 0;
  unsigned i, n;
  const DrawItem *items = game_items(&n);
  U8 *a = buffer + FRAME, *b = reference + FRAME;
  memset(a, 0, CAP);
  memset(b, 0, CAP);
  for (i = 0; i < n; i++) {
    draw(a, items[i].sprite, items[i].x, items[i].y, mode);
    pixel(b, items[i].sprite, items[i].x, items[i].y);
  }
  compositionInvalid = 1;
  return !memcmp(a, b, CAP) && gfx_guards();
}

void gfx_invalidate(void) {
  firstFrame = 1;
  compositionInvalid = 1;
  previousCount = 0;
  memset(eraseNext, 0, sizeof(eraseNext));
}
void gfx_composed(void) { firstFrame = 0; }

void gfx_regions(int width, int height, void (*present)(int, int, int, int)) {
  int row, x, r, y, h, right;
  for (row = 0; row < (height + 15) / 16; row++) {
    uint32_t mask = dirty[row];
    x = 0;
    y = row * 16;
    h = y + 16 <= height ? 16 : height - y;
    while (mask) {
      if (perfKernel & 8) {
        int skip;
        while (!(mask & 255)) {
          mask >>= 8;
          x += 8;
        }
        skip = trailingZeros[mask & 255];
        mask >>= skip;
        x += skip;
      } else
        while (!(mask & 1)) {
          mask >>= 1;
          x++;
        }
      r = x;
      if (perfKernel & 8388608) {
        int take;
        while ((mask & 255) == 255) {
          mask >>= 8;
          x += 8;
        }
        take = trailingZeros[(~mask) & 255];
        mask >>= take;
        x += take;
      } else
        while (mask & 1) {
          mask >>= 1;
          x++;
        }
      right = x * 16;
      if (right > width)
        right = width;
      if (r * 16 < right)
        present(r * 16, y, right - r * 16, h);
    }
  }
  firstFrame = 0;
}
/* Every generated phase, including mixed background and opaque-white faces. */
int gfx_compiled_test(void) {
  int id, ph;
  for (id = 1; id < 90; id++)
    for (ph = 0; ph < 16; ph++) {
      memset(buffer + FRAME, 0xa5, CAP);
      memset(reference + FRAME, 0xa5, CAP);
      draw(buffer + FRAME, id, 96 + ph, 100, 2);
      pixel(reference + FRAME, id, 96 + ph, 100);
      if (memcmp(buffer + FRAME, reference + FRAME, CAP) || !gfx_guards())
        return id * 16 + ph;
    }
  gfx_invalidate();
  return 0;
}

int gfx_composition_check(void) {
  unsigned i, n;
  const DrawItem *items = game_items(&n);
  memset(reference + FRAME, 0, CAP);
  for (i = 0; i < n; i++)
    pixel(reference + FRAME, items[i].sprite, items[i].x, items[i].y);
  return !memcmp(buffer + FRAME, reference + FRAME, CAP) && gfx_guards();
}

void gfx_set_compact(int enabled) {
  int id, ph;
  U8 *base;
  const uint32_t *offsets;
  if (blitBank) {
    HUnlock(blitBank);
    ReleaseResource(blitBank);
    blitBank = NULL;
  }
  blitBank = GetResource('BLIT', 128);
  if (!blitBank)
    platform_fault(8180);
  LoadResource(blitBank);
  if (!*blitBank)
    platform_fault(8181);
  HLock(blitBank);
  base = (U8 *)*blitBank;
  offsets = (const uint32_t *)base;
  for (id = 1; id < 90; id++)
    for (ph = 0; ph < 16; ph++)
      bankFunctions[id][ph] = (BlitFn)(base + offsets[(id - 1) * 16 + ph]);
  art = original_art;
  memset(phaseKey, 0, sizeof(phaseKey));
  titlePhaseKey = 0;
  gfx_invalidate();
}

int gfx_overscan_test(void) {
  int id, ph, k;
  unsigned cases = 0;
  for (id = 1; id < 90; id++)
    for (ph = 0; ph < 16; ph++)
      for (k = 0; k < 6; k++) {
        int y = k == 0   ? -art[id].h + 1
                : k == 1 ? -art[id].h / 2
                : k == 2 ? -1
                : k == 3 ? 341
                : k == 4 ? 342 - art[id].h / 2
                         : 342 - art[id].h + 1;
        memset(buffer + FRAME, 0xa5, CAP);
        memset(reference + FRAME, 0xa5, CAP);
        draw(buffer + FRAME, id, 96 + ph, y, 2);
        pixel(reference + FRAME, id, 96 + ph, y);
        if (memcmp(buffer + FRAME, reference + FRAME, CAP) || !gfx_guards())
          return -1;
        cases++;
      }
  gfx_invalidate();
  return cases;
}

/* 80x80 pose monitor, 16 px from right edge and 8 px below HUD.
   All 22 poses expanded once at startup: no live scaling or allocation. */
static U8 insetBanks[22][800] __attribute__((aligned(2)));
static U8 *insetPixels;
static int insetShown;
extern void inset_copy_asm(U8 *, const U8 *);
static __attribute__((noinline)) void inset_copy_c(U8 *d, const U8 *s) {
  int y;
  for (y = 0; y < 80; y++)
    memcpy(d + y * 64, s + y * 10, 10);
}
/* Startup-only 2x pixel expansion. Table lookup duplicates nibble bits without smoothing, preserving the original pose silhouette. */
static void inset_build(int id) {
  static const U8 twice[16] = {0,   3,   12,  15,  48,  51,  60,  63,
                               192, 195, 204, 207, 240, 243, 252, 255};
  GfxSprite *s = &art[id];
  int x, y, ox = (80 - s->w * 2) / 2, oy = (80 - s->h * 2) / 2, shift = ox & 7,
            byte = ox / 8;
  insetPixels = insetBanks[id - 1];
  memset(insetPixels, 0, 800);
  memset(insetPixels, 255, 10);
  memset(insetPixels + 790, 255, 10);
  for (y = 1; y < 79; y++) {
    insetPixels[y * 10] = 128;
    insetPixels[y * 10 + 9] = 1;
  }
  for (y = 0; y < s->h; y++) {
    U8 *d = insetPixels + (oy + y * 2) * 10 + byte;
    for (x = 0; x < (s->w + 7) / 8; x++) {
      unsigned v = (s->bits[y * s->words + x / 2] >> ((x & 1) ? 0 : 8)) & 255;
      uint32_t expanded = (((uint32_t)twice[v >> 4] << 8) | twice[v & 15]) << 8;
      expanded >>= shift;
      d[x * 2] |= expanded >> 16;
      d[x * 2 + 1] |= expanded >> 8;
      if (shift)
        d[x * 2 + 2] |= expanded;
    }
    memcpy(insetPixels + (oy + y * 2 + 1) * 10, insetPixels + (oy + y * 2) * 10,
           10);
  }
}
#if AUTOTEST
static void inset_build_reference(int id) {
  GfxSprite *s = &art[id];
  int x, y, ox = (80 - s->w * 2) / 2, oy = (80 - s->h * 2) / 2;
  insetPixels = insetBanks[id - 1];
  memset(insetPixels, 0, 800);
  memset(insetPixels, 255, 10);
  memset(insetPixels + 790, 255, 10);
  for (y = 1; y < 79; y++) {
    insetPixels[y * 10] = 128;
    insetPixels[y * 10 + 9] = 1;
  }
  for (y = 0; y < s->h; y++)
    for (x = 0; x < s->w; x++)
      if (s->bits[y * s->words + x / 16] & (0x8000u >> (x & 15))) {
        int xx = ox + x * 2, yy = oy + y * 2, k, j;
        for (j = 0; j < 2; j++)
          for (k = 0; k < 2; k++)
            insetPixels[(yy + j) * 10 + (xx + k) / 8] |= 128u >> ((xx + k) & 7);
      }
}
#endif
static void inset_init(void) {
  int id;
  for (id = 1; id <= 22; id++)
    inset_build(id);
}
/* A zero pose dismisses the overlay. The old footprint must remain scheduled for restoration even when there is no new image. */
void gfx_air_inset(int pose, int mode) {
  if (pose <= 0 || pose > 22 || art[pose].w > 36 || art[pose].h > 36) {
    if (insetShown)
      gfx_dirty(416, 24, 80, 80);
    insetShown = 0;
    return;
  }
  insetShown = 1;
  insetPixels = insetBanks[pose - 1];
  if (mode == 2)
    inset_copy_asm(gfx_target() + 24 * 64 + 52, insetPixels);
  else
    inset_copy_c(gfx_target() + 24 * 64 + 52, insetPixels);
  gfx_dirty(416, 24, 80, 80);
  gfx_erase_next(416, 24, 80, 80);
}
#if AUTOTEST
int gfx_inset_test(void) {
  int id, x, y;
  static U8 a[80 * 64 + 32], b[80 * 64 + 32];
  for (id = 1; id <= 22; id++) {
    inset_build_reference(id);
    memcpy(a, insetPixels, 800);
    inset_build(id);
    if (memcmp(a, insetPixels, 800))
      return 300 + id;
    memset(a, 0xa5, sizeof(a));
    memcpy(b, a, sizeof(a));
    inset_copy_c(a + 16, insetPixels);
    inset_copy_asm(b + 16, insetPixels);
    if (memcmp(a, b, sizeof(a)))
      return id;
    for (y = 1; y < 79; y++)
      for (x = 1; x < 79; x++) {
        GfxSprite *s = &art[id];
        int sx = x - (80 - s->w * 2) / 2, sy = y - (80 - s->h * 2) / 2;
        int expected = sx >= 0 && sy >= 0 && sx < s->w * 2 && sy < s->h * 2 &&
                       (s->bits[(sy / 2) * s->words + (sx / 2) / 16] &
                        (0x8000u >> ((sx / 2) & 15)));
        if (!!(insetPixels[y * 10 + x / 8] & (128u >> (x & 7))) != !!expected)
          return 100 + id;
      }
  }
  gfx_invalidate();
  gfx_frame(2);
  gfx_air_inset(14, 2);
  gfx_composed();
  gfx_frame(2);
  gfx_air_inset(0, 2);
  if (!gfx_intersects(416, 24, 80, 80) || !gfx_composition_check())
    return 200;
  gfx_invalidate();
  return 0;
}
unsigned gfx_inset_bench(int mode) {
  int i;
  unsigned sum = 0;
  inset_build(14);
  for (i = 0; i < 5000; i++) {
    if (mode)
      inset_copy_asm(gfx_target(), insetPixels);
    else
      inset_copy_c(gfx_target(), insetPixels);
    sum += gfx_target()[i & 7];
  }
  gfx_invalidate();
  return sum;
}

#endif
#if AUTOTEST
unsigned gfx_inset_build_bench(int mode) {
  int i;
  unsigned sum = 0;
  for (i = 0; i < 220; i++) {
    if (mode)
      inset_build(1 + i % 22);
    else
      inset_build_reference(1 + i % 22);
    sum += insetPixels[100];
  }
  return sum;
}
#endif
