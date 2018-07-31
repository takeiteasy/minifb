//
//  main.c
//  roguelike
//
//  Created by Rory B. Bellows on 22/06/2018.
//  Copyright © 2018 Rory B. Bellows. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "threads.h"
#include "dungeon.h"
#include "vector.h"

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

/* TODO:
 * - Replace heap & stb_sb
 * - Implement no diag pathfinding in A*
 * - Mouse movement
 * - Better way to store tile map
 */

static bool running = true, active = true, mouse_enabled = true;
static int win_w = SCREEN_WIDTH, win_h = SCREEN_HEIGHT;
static surface_t win;

static struct mouse_t {
  bool btns[MOUSE_LAST - 1];
  point_t pos;
  float wheel[2];
  KEYMOD mod;
} mouse_handler;

static struct keyboard_t {
  bool keys[KB_KEY_LAST];
  KEYMOD mod;
} keyboard_handler;

void on_keyboard(KEYSYM sym, KEYMOD mod, bool down) {
  keyboard_handler.keys[sym] = down;
  keyboard_handler.mod = mod;
}

void on_mouse_btn(MOUSEBTN btn, KEYMOD mod, bool down) {
  mouse_handler.btns[btn - 1] = down;
  mouse_handler.mod = mod;
}

void on_mouse_move(int x, int y, int dx, int dy) {
  mouse_handler.pos.x = (int)(((float)x / (float)win_w) * win.w);
  mouse_handler.pos.y = (int)(((float)y / (float)win_h) * win.h);
}

void on_scroll(KEYMOD mod, float dx, float dy) {
  mouse_handler.wheel[0] = dx;
  mouse_handler.wheel[1] = dy;
  mouse_handler.mod = mod;
}

void on_focus(bool focused) {
  active = focused;
}

void on_resize(int w, int h) {
  win_w = w;
  win_h = h;
  fill(&win, BLACK);
  writelnf(&win, 4, 5, WHITE, -1, "%dx%d\n", w, h);
}

void on_error(ERRPRIO pri, const char* msg, const char* file, const char* func, int line) {
  fprintf(stderr, "ERROR ENCOUNTERED: %s\nFrom %s, in %s() at %d\n", msg, file, func, line);
  if (pri == PRIO_HIGH || pri == PRIO_NORM)
    abort();
}

typedef struct {
  surface_t ctx;
  point_t pos;
  const char* name;
} panel_t;

void create_panel(panel_t* p, const char* n, int x, int y, int w, int h) {
  surface(&p->ctx, w, h);
  fill(&p->ctx, BLACK);
  p->pos = (point_t){ x, y };
  p->name = n;
}

void draw_panel(surface_t* dst, panel_t* p) {
  rect(dst, p->pos.x - 1, p->pos.y - 1, p->ctx.w + 1, p->ctx.h + 1, WHITE, false);
  blit(dst, &p->pos, &p->ctx, NULL);
  if (p->name)
    sgl_writelnf(dst, p->pos.x + 10, p->pos.y - 4, WHITE, BLACK, "%s", p->name);
}

typedef struct {
  bool (*init)(void*);
  void (*update)(long);
  void (*render)(void);
  void (*dispose)(void);
} state_t;

static panel_t test_panel, test_panel_2;
static vector_t panels_test;
static state_t* current_state = NULL, test_state;
#define this current_state

void create_state(state_t* s, bool (*init_fn)(void*), void (*update_fn)(long), void (*render_fn)(void), void (*dispose_fn)(void)) {
  s->init = init_fn;
  s->update = update_fn;
  s->render = render_fn;
  s->dispose = dispose_fn;
}

bool update_main_state(state_t* s, void* ctx) {
  if (current_state)
    current_state->dispose();
  current_state = s;
  return current_state->init(ctx);
}

bool init_test(void* ctx) {
  vector_init(&panels_test);
  create_panel(&test_panel, "test", 10, 10, 100, 100);
  create_panel(&test_panel_2, "test2", 30, 30, 100, 100);
  vector_push(&panels_test, (void*)&test_panel);
  vector_push(&panels_test, (void*)&test_panel_2);
  return true;
}

void update_test(long time) {
  for (int i = 0; i < panels_test.length; ++i)
    fill(&((panel_t*)vector_get(&panels_test, i))->ctx, BLACK);
  
  panel_t* current = NULL;
  for (int i = panels_test.length - 1; i >= 0; --i) {
    current = (panel_t*)vector_get(&panels_test, i);
    rect_t aabb = {
      current->pos.x,
      current->pos.y,
      current->pos.x + current->ctx.w,
      current->pos.y + current->ctx.h
    };
    if (mouse_handler.pos.x >= aabb.x && mouse_handler.pos.y >= aabb.y && mouse_handler.pos.x <= aabb.w && mouse_handler.pos.y <= aabb.h)
      break;
    current = NULL;
  }
  
  if (current)
    fill(&current->ctx, RED);
}

void render_test(void) {
  panel_t* p;
  for (int i = 0; i < panels_test.length; ++i) {
    p = (panel_t*)vector_get(&panels_test, i);
    draw_panel(&win, p);
  }
}

void dispose_test(void) {
  for (int i = 0; i < panels_test.length; ++i)
    sgl_destroy(&((panel_t*)vector_get(&panels_test, i))->ctx);
  vector_free(&panels_test);
}

int main(int argc, const char * argv[]) {
  srand(((argc > 1) ? atoi(argv[1]) : (unsigned int)time(NULL)));
  
  error_callback(on_error);
  screen("roguelike", &win, SCREEN_WIDTH, SCREEN_HEIGHT, RESIZABLE);
  screen_callbacks(on_keyboard, on_mouse_btn, on_mouse_move, on_scroll, on_focus, on_resize);
  
  create_state(&test_state, init_test, update_test, render_test, dispose_test);
  update_main_state(&test_state, NULL);
  
  long prev_frame_tick;
  long curr_frame_tick = ticks();
  while (!closed() && running) {
    prev_frame_tick = curr_frame_tick;
    curr_frame_tick = ticks();
    poll();
    
#if defined(SGL_OSX)
    if ((keyboard_handler.keys[KB_KEY_Q] || keyboard_handler.keys[KB_KEY_W]) && keyboard_handler.mod == KB_MOD_SUPER)
#else
    if (keyboard_handler.keys[KB_KEY_F4] && keyboard_handler.mod == KB_MOD_ALT)
#endif
      running = false;
    
    fill(&win, BLACK);
    if (active) {
      current_state->update(curr_frame_tick - prev_frame_tick);
      current_state->render();
    }
    flush(&win);
  }
  
  if (current_state)
    current_state->dispose();
  destroy(&win);
  release();
  return 0;
}
