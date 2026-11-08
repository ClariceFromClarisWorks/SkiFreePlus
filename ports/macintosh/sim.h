#ifndef SKI_SIM_H
#define SKI_SIM_H
#include <stdint.h>
enum {INPUT_NONE,INPUT_LEFT,INPUT_RIGHT,INPUT_DOWN,INPUT_UP,INPUT_JUMP,INPUT_FAST,INPUT_SW,INPUT_SE,INPUT_W,INPUT_E,INPUT_CLICK,INPUT_KEYPAD_JUMP};
typedef struct {int16_t sprite,x,y;} DrawItem;
typedef struct {int32_t x,y,speed,style,elapsed; int alive,mode,frame,air,turbo; int resultMode; int32_t resultValue,distance; unsigned objects,steps;} GameInfo;
void game_init(uint32_t seed,int x,int y);
int game_course_test(void);
int game_gate_feedback(void);
int game_gate_flash(void);
void game_step(void);
void game_start_course(int mode);
void game_resize(int w,int h);
int game_compact(void);
void game_set_compact(int enabled);
void game_input(int key);
void game_mouse(int x,int y);
const DrawItem* game_items(unsigned* count);
GameInfo game_info(void);
uint32_t game_hash(void);
void platform_fault(int line);
#endif

unsigned game_steps(void);
int game_has_result(void);

void game_profile_step(unsigned long(*clock)(void),unsigned long*core,unsigned long*paint);

void game_set_fast_paint(int enabled);

void game_set_snapshot_actors(int enabled);

void game_set_camera(int enabled);
int game_camera_shift(void);
int game_shadow(int top,int*groundY);

#if AUTOTEST
void game_test_jump(void);
void game_test_gate(int missed);
#endif

/* Current player pose only when wholly above the visible playfield. */
int game_air_pose(int top);
