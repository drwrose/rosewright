#ifndef WRIGHT_H
#define WRIGHT_H

#include <pebble.h>

//#define PBL_PLATFORM_EMERY  // hack

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

//#define SECONDS_PER_DAY 86400
//#define SECONDS_PER_HOUR 3600
#define MS_PER_DAY (SECONDS_PER_DAY * 1000)

#ifndef PBL_PLATFORM_DIORITE
// It turns out that, so far, only Diorite has enough spare RAM to
// keep the face bitmap around all the time.
#define NEVER_KEEP_FACE_ASSET
#endif  // PBL_PLATFORM_DIORITE

#ifdef PBL_PLATFORM_APLITE
// And on Aplite, we should just not bother trying to keep any assets
// around.
#define NEVER_KEEP_ASSETS
#endif  // PBL_PLATFORM_APLITE

#ifdef PBL_PLATFORM_DIORITE
// Although I'm told that Pebble Time can support heart rate with a
// Smartstrap, I haven't seen any such beast out there, and I'm
// therefore (for now) defining this support only for diorite.
#define SUPPORT_HEART_RATE
#endif  // PBL_PLATFORM_DIORITE

// Drawing the phase hands separately doesn't seem to be a performance
// win for some reason.
#define SEPARATE_PHASE_HANDS 0
//#define SEPARATE_PHASE_HANDS (config.second_hand)

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

  // Not really a hand placement, but this is the hour at which we
  // last rang (or should next ring) the buzzer.
  unsigned char buzzed_hour;
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

#ifdef NEVER_KEEP_ASSETS
#define keep_assets false
#else
extern bool keep_assets;
#endif  // NEVER_KEEP_ASSETS

#ifdef NEVER_KEEP_FACE_ASSET
#define keep_face_asset false
#else
extern bool keep_face_asset;
#endif  // NEVER_KEEP_FACE_ASSET

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
void remap_colors_date(BitmapWithData *bwd);
void invalidate_clock_face();
void destroy_objects();
void create_objects();
void recreate_all_objects();


#endif
