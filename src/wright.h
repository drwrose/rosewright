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
struct HandPlacement {
  unsigned int hour_hand_index;
  unsigned int minute_hand_index;
  unsigned int second_hand_index;
#ifdef MAKE_CHRONOGRAPH
  unsigned int chrono_minute_hand_index;
  unsigned int chrono_second_hand_index;
  unsigned int chrono_tenth_hand_index;
#endif  // MAKE_CHRONOGRAPH
  unsigned int day_index;
  unsigned int month_index;
  unsigned int date_value;

  // Not really a hand placement, but this is used to keep track of
  // whether we have buzzed for the top of the hour or not.
  unsigned int hour_buzzer;
};

// Keeps track of the current bitmap and/or path for a particular
// hand, so we don't need to do as much work if we're redrawing a hand
// in the same position as last time.
#define HAND_CACHE_MAX_GROUPS 2
__attribute__((__packed__)) struct HandCache {
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
void draw_vector_hand(struct HandCache *hand_cache, struct VectorHandTable *hand, int hand_index, int num_steps,
                      int place_x, int place_y, GContext *ctx);
void draw_bitmap_hand(struct HandCache *hand_cache, struct BitmapHandLookupRow *lookup_table, struct BitmapHandTableRow *hand_table, int hand_index, bool use_rle, int place_x, int place_y, bool paint_black, GContext *ctx);

#endif
