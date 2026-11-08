/* Integer display scheduling for the compact Mac tick clock. A 40 ms
 * simulation step cannot be represented by a whole 60.15 Hz tick: distribute
 * two- and three-tick intervals using a fractional accumulator. Retain overdue
 * simulation debt, but bound work per iteration and prevent burst presents.
 * Emulator success does not establish identical timing on physical hardware.
 */
#ifndef SKI_DEADLINE_CLOCK_H
#define SKI_DEADLINE_CLOCK_H
#include <stdint.h>
/* Compact Mac tick rate ~60.15 Hz (Inside Macintosh: Processes). 500 fixed
   40 ms steps occupy 1203 ticks. Integer phase accumulation has no per-frame
   rounding drift. Deadline tests are wrap-safe for intervals <2^31 ticks. */
typedef struct {
  uint32_t deadline, last;
  unsigned phase, ready;
} DeadlineClock;
static int deadline_before(uint32_t a, uint32_t b) {
  return (int32_t)(a - b) < 0;
}
static void deadline_advance(DeadlineClock *c) {
  unsigned n = c->phase + 203;
  c->deadline += 2;
  if (n >= 500) {
    c->deadline++;
    n -= 500;
  }
  c->phase = n;
}
static void deadline_start(DeadlineClock *c, uint32_t now) {
  c->deadline = now;
  c->phase = 499;
  c->last = now;
  c->ready = 0;
  deadline_advance(c);
}
/* Keep ALL fixed physics steps. When late, omit intermediate drawings only.
   Bound work per presentation; remaining debt is retained, never discarded. */
static unsigned deadline_steps(DeadlineClock *c, uint32_t now, unsigned limit) {
  unsigned count = 1;
  DeadlineClock next = *c;
  deadline_advance(&next);
  while (count < limit && !deadline_before(now, next.deadline)) {
    *c = next;
    count++;
    deadline_advance(&next);
  }
  return count;
}
static uint32_t deadline_target(const DeadlineClock *c, int bounded) {
  uint32_t t = c->deadline;
  if (bounded && c->ready && deadline_before(t, c->last + 2))
    t = c->last + 2;
  return t;
}
static void deadline_presented(DeadlineClock *c, uint32_t now) {
  c->last = now;
  c->ready = 1;
  deadline_advance(c);
}
static unsigned deadline_debt(const DeadlineClock *c, uint32_t now) {
  DeadlineClock next = *c;
  unsigned n = 0;
  while (n < 65535 && !deadline_before(now, next.deadline)) {
    n++;
    deadline_advance(&next);
  }
  return n;
}
#endif
