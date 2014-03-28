#ifndef WRIGHT_H
#define WRIGHT_H

#include <pebble.h>

#include "hand_table.h"
#include "lang_table.h"
#include "../resources/generated_config.h"
#include "../resources/generated_defs.h"
#include "bluetooth_indicator.h"
#include "battery_gauge.h"
#include "config_options.h"
#include "assert.h"
#include "bwd.h"

#ifdef NDEBUG
  // In a production build, eliminate log calls from even appearing in
  // the code.
  #define app_log(...)
#endif  // NDBEUG

#define SECONDS_PER_DAY 86400
#define SECONDS_PER_HOUR 3600
#define MS_PER_DAY (SECONDS_PER_DAY * 1000)

#define SCREEN_WIDTH 144
#define SCREEN_HEIGHT 168

// This structure keeps track of the things that change on the visible
// watch face and their current state.
struct __attribute__((__packed__)) HandPlacement {
  unsigned char hour_hand_index;
  unsigned char minute_hand_index;
  unsigned char second_hand_index;
#ifdef MAKE_CHRONOGRAPH
  unsigned char chrono_minute_hand_index;
  unsigned char chrono_second_hand_index;
  unsigned char chrono_tenth_hand_index;
#endif  // MAKE_CHRONOGRAPH

  // These are values that might be displayed in date windows.
  unsigned char day_index;
  unsigned char month_index;
  unsigned char date_value;
  unsigned char year_value;  // less 1900.
  bool ampm_value;

  // Not really a hand placement, but this is used to keep track of
  // whether we have buzzed for the top of the hour or not.
  bool hour_buzzer;
};

// Keeps track of the current bitmap and/or path for a particular
// hand, so we don't need to do as much work if we're redrawing a hand
// in the same position as last time.
#define HAND_CACHE_MAX_GROUPS 2
struct __attribute__((__packed__)) HandCache {
  unsigned char bitmap_hand_index;
  BitmapWithData image;
  BitmapWithData mask;
  unsigned char vector_hand_index;
  short cx, cy;
  GPath *path[HAND_CACHE_MAX_GROUPS];
};

typedef struct {
  GCompOp paint_black;
  GCompOp paint_white;
  GCompOp paint_assign;
  GCompOp paint_fg;
  GCompOp paint_mask;
  GColor colors[3];
} __attribute__((__packed__)) DrawModeTable;

extern DrawModeTable draw_mode_table[2];
extern int sweep_timer_ms;

extern struct HandPlacement current_placement;
extern Window *window;

void stopped_click_config_provider(void *context);
void started_click_config_provider(void *context);

void trigger_memory_panic(int line_number);
void reset_memory_panic();
unsigned int get_time_ms(struct tm *time);
void update_hands(struct tm *time);
void hand_cache_init(struct HandCache *hand_cache);
void hand_cache_destroy(struct HandCache *hand_cache);
void reset_tick_timer();
void draw_hand(struct HandCache *hand_cache, struct HandDef *hand_def, int hand_index, GContext *ctx);

#endif
