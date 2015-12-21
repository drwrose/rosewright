#ifndef WRIGHT_H
#define WRIGHT_H

#include <pebble.h>
#include "pebble_compat.h"
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

#ifdef PBL_ROUND
#define SCREEN_WIDTH 180
#define SCREEN_HEIGHT 180
#else  // PBL_ROUND
#define SCREEN_WIDTH 144
#define SCREEN_HEIGHT 168
#endif  // PBL_ROUND

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
  unsigned char week_value;
  unsigned short ordinal_date_index;
  bool ampm_value;

#ifdef TOP_SUBDIAL
  unsigned char lunar_index;
#endif  // TOP_SUBDIAL

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

// The DrawModeTable is defined in write.c, and allows us to switch
// draw modes according to whether the face is drawn black-on-white
// (the default, draw_mode 0) or white-on-black (draw_mode 1).  In the
// comments below the "fg color" is black in draw mode 0 and white in
// draw mode 1.
typedef struct {
  GCompOp paint_fg;     // paint the white pixels of the hand in the fg color
  GCompOp paint_bg;     // paint the white pixels of the mask in the bg color
  GCompOp paint_assign; // paint the black pixels in the fg color and the white pixels in the bg color

  GColor colors[3];     //  { clear, fg color, bg color }
} __attribute__((__packed__)) DrawModeTable;

extern bool memory_panic_flag;
extern int memory_panic_count;
extern bool keep_assets;

extern DrawModeTable draw_mode_table[2];
extern int sweep_timer_ms;

extern struct HandPlacement current_placement;
extern Window *window;

extern Layer *clock_face_layer;

#ifdef SUPPORT_RESOURCE_CACHE
#define RESOURCE_CACHE_PARAMS(a, b) , a, b
#define RESOURCE_CACHE_FORMAL_PARAMS , struct ResourceCache *resource_cache, size_t resource_cache_size

#else
#define RESOURCE_CACHE_PARAMS(a, b)
#define RESOURCE_CACHE_FORMAL_PARAMS

#endif


void stopped_click_config_provider(void *context);
void started_click_config_provider(void *context);

void trigger_memory_panic(int line_number);
void reset_memory_panic();
void update_hands(struct tm *time);
void hand_cache_init(struct HandCache *hand_cache);
void hand_cache_destroy(struct HandCache *hand_cache);
void reset_tick_timer();
void draw_hand_mask(struct HandCache *hand_cache RESOURCE_CACHE_FORMAL_PARAMS, struct HandDef *hand_def, int hand_index, bool no_basalt_mask, GContext *ctx);
void draw_hand_fg(struct HandCache *hand_cache RESOURCE_CACHE_FORMAL_PARAMS, struct HandDef *hand_def, int hand_index, bool no_basalt_mask, GContext *ctx);
void draw_hand(struct HandCache *hand_cache RESOURCE_CACHE_FORMAL_PARAMS, struct HandDef *hand_def, int hand_index, GContext *ctx);
void remap_colors_clock(BitmapWithData *bwd);
void invalidate_clock_face();
void destroy_objects();
void create_objects();
void recreate_all_objects();


#endif
