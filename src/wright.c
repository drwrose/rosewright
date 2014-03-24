#include <pebble.h>

#include "hand_table.h"
#include "lang_table.h"
#include "../resources/generated_config.h"
#include "../resources/generated_table.c"
#include "../resources/lang_table.c"
#include "bluetooth_indicator.h"
#include "battery_gauge.h"
#include "config_options.h"
#include "assert.h"
#include "bwd.h"

#define SECONDS_PER_DAY 86400
#define SECONDS_PER_HOUR 3600
#define MS_PER_DAY (SECONDS_PER_DAY * 1000)

#define SCREEN_WIDTH 144
#define SCREEN_HEIGHT 168

Window *window;

BitmapWithData clock_face;
int face_index = -1;
BitmapLayer *clock_face_layer;

BitmapWithData date_card;
BitmapWithData date_card_mask;

BitmapWithData chrono_dial_white;
BitmapWithData chrono_dial_black;
Layer *chrono_dial_layer;

struct FontPlacement {
  unsigned char resource_id;
  signed char vshift;  // Value determined empirically for each font.
};

struct FontPlacement date_font_placement = {
  0, -3
};
GFont date_font = NULL;
GFont day_font = NULL;

#define NUM_DAY_FONTS 5
struct FontPlacement day_font_placement[NUM_DAY_FONTS] = {
  { RESOURCE_ID_DAY_FONT_LATIN_16, -1 },
  { RESOURCE_ID_DAY_FONT_EXTENDED_14, 1 },
  { RESOURCE_ID_DAY_FONT_ZH_16, -1 },  // Chinese
  { RESOURCE_ID_DAY_FONT_JA_16, -1 },  // Japanese
  { RESOURCE_ID_DAY_FONT_KO_16, -2 },  // Korean
};

// Number of laps preserved for the laps digital display
#define CHRONO_MAX_LAPS 4

// This window is pushed on top of the chrono dial to display the
// readout in digital form for ease of recording.
Window *chrono_digital_window;
InverterLayer *chrono_digital_line_layer = NULL;
TextLayer *chrono_digital_current_layer = NULL;
TextLayer *chrono_digital_laps_layer[CHRONO_MAX_LAPS];
bool chrono_digital_window_showing = false;
AppTimer *chrono_digital_timer = NULL;
#define CHRONO_DIGITAL_BUFFER_SIZE 11
char chrono_current_buffer[CHRONO_DIGITAL_BUFFER_SIZE];
char chrono_laps_buffer[CHRONO_MAX_LAPS][CHRONO_DIGITAL_BUFFER_SIZE];

#define CHRONO_DIGITAL_TICK_MS 100 // Every 0.1 seconds

// True if we're currently showing tenths, false if we're currently
// showing hours, in the chrono subdial.
bool chrono_dial_shows_tenths = false;

// Triggered at regular intervals to implement sweep seconds.
AppTimer *sweep_timer = NULL;
int sweep_timer_ms = 1000;

int sweep_seconds_ms = 60 * 1000 / NUM_STEPS_SECOND;
int sweep_chrono_seconds_ms = 60 * 1000 / NUM_STEPS_CHRONO_SECOND;

Layer *hour_layer;
Layer *minute_layer;
Layer *second_layer;
Layer *chrono_minute_layer;
Layer *chrono_second_layer;
Layer *chrono_tenth_layer;

Layer *day_layer;  // day of the week (abbr)
Layer *date_layer; // numeric date of the month

// This structure keeps track of the things that change on the visible
// watch face and their current state.
struct HandPlacement {
  unsigned int hour_hand_index;
  unsigned int minute_hand_index;
  unsigned int second_hand_index;
  unsigned int chrono_minute_hand_index;
  unsigned int chrono_second_hand_index;
  unsigned int chrono_tenth_hand_index;
  unsigned int day_index;
  unsigned int date_value;

  // Not really a hand placement, but this is used to keep track of
  // whether we have buzzed for the top of the hour or not.
  unsigned int hour_buzzer;
};

// Keeps track of the current bitmap and/or path for a particular
// hand, so we don't need to do as much work if we're redrawing a hand
// in the same position as last time.
#define HAND_CACHE_MAX_GROUPS 2
struct HandCache {
  int bitmap_hand_index;
  BitmapWithData image;
  BitmapWithData mask;
  int vector_hand_index;
  int cx, cy;
  GPath *path[HAND_CACHE_MAX_GROUPS];
};

struct HandCache hour_cache;
struct HandCache minute_cache;
struct HandCache second_cache;
struct HandCache chrono_minute_cache;
struct HandCache chrono_second_cache;
struct HandCache chrono_tenth_cache;

struct HandPlacement current_placement;

typedef struct {
  GCompOp paint_black;
  GCompOp paint_white;
  GCompOp paint_assign;
  GCompOp paint_fg;
  GCompOp paint_mask;
  GColor colors[3];
} DrawModeTable;

DrawModeTable draw_mode_table[2] = {
  { GCompOpClear, GCompOpOr, GCompOpAssign, GCompOpAnd, GCompOpSet, { GColorClear, GColorBlack, GColorWhite } },
  { GCompOpOr, GCompOpClear, GCompOpAssignInverted, GCompOpSet, GCompOpAnd, { GColorClear, GColorWhite, GColorBlack } },
};

static const uint32_t tap_segments[] = { 50 };
VibePattern tap = {
  tap_segments,
  1,
};

int stacking_order[] = {
STACKING_ORDER_LIST
};

typedef struct {
  unsigned int start_ms;              // consulted if chrono_data.running && !chrono_data.lap_paused
  unsigned int hold_ms;               // consulted if !chrono_data.running || chrono_data.lap_paused
  unsigned char running;              // the chronograph has been started
  unsigned char lap_paused;           // the "lap" button has been pressed
  unsigned int laps[CHRONO_MAX_LAPS];
} __attribute__((__packed__)) ChronoData;

ChronoData chrono_data = { false, false, 0, 0, { 0, 0, 0, 0 } };
ChronoData saved_chrono_data;

// Returns the number of milliseconds since midnight.
unsigned int get_time_ms(struct tm *time) {
  time_t s;
  uint16_t ms;
  unsigned int result;

  time_ms(&s, &ms);
  result = (unsigned int)((s % SECONDS_PER_DAY) * 1000 + ms);

#ifdef FAST_TIME
  if (time != NULL) {
    time->tm_wday = (s / 4) % 7;
    time->tm_mday = (s % 31) + 1;
  }
  result *= 67;
#endif  // FAST_TIME

  return result;
}

// Returns the time showing on the chronograph, given the ms returned
// by get_time_ms().  Returns the current lap time if the lap is
// paused.
unsigned int get_chrono_ms(unsigned int ms) {
  unsigned int chrono_ms;
  if (chrono_data.running && !chrono_data.lap_paused) {
    // The chronograph is running.  Show the active elapsed time.
    chrono_ms = (ms - chrono_data.start_ms + MS_PER_DAY) % MS_PER_DAY;
  } else {
    // The chronograph is paused.  Show the time it is paused on.
    chrono_ms = chrono_data.hold_ms;
  }

  return chrono_ms;
}

#ifdef MAKE_CHRONOGRAPH
void load_chrono_dial() {
  bwd_destroy(&chrono_dial_white);
  bwd_destroy(&chrono_dial_black);
  if (chrono_dial_shows_tenths) {
    chrono_dial_white = rle_bwd_create(RESOURCE_ID_CHRONO_DIAL_TENTHS_WHITE);
    chrono_dial_black = rle_bwd_create(RESOURCE_ID_CHRONO_DIAL_TENTHS_BLACK);
  } else {
    chrono_dial_white = rle_bwd_create(RESOURCE_ID_CHRONO_DIAL_HOURS_WHITE);
    chrono_dial_black = rle_bwd_create(RESOURCE_ID_CHRONO_DIAL_HOURS_BLACK);
  }
}
#endif  // MAKE_CHRONOGRAPH
  
// Determines the specific hand bitmaps that should be displayed based
// on the current time.
void compute_hands(struct tm *time, struct HandPlacement *placement) {
  unsigned int ms;

  ms = get_time_ms(time);

  {
    // Avoid overflowing the integer arithmetic by pre-constraining
    // the ms value to the appropriate range.
    unsigned int use_ms = ms % (SECONDS_PER_HOUR * 12* 1000);
    placement->hour_hand_index = ((NUM_STEPS_HOUR * use_ms) / (SECONDS_PER_HOUR * 12 * 1000)) % NUM_STEPS_HOUR;
  }
  {
    unsigned int use_ms = ms % (SECONDS_PER_HOUR * 1000);
    placement->minute_hand_index = ((NUM_STEPS_MINUTE * use_ms) / (SECONDS_PER_HOUR * 1000)) % NUM_STEPS_MINUTE;
  }
  {
    unsigned int use_ms = ms % (60 * 1000);
    if (!config.sweep_seconds) {
      // Also constrain to an integer second if we've not enabled
      // sweep-second resolution.
      use_ms = (use_ms / 1000) * 1000;
    }
    placement->second_hand_index = ((NUM_STEPS_SECOND * use_ms) / (60 * 1000));
  }

#ifdef ENABLE_DAY_CARD
  if (time != NULL) {
    placement->day_index = time->tm_wday;
  }
#endif  // ENABLE_DAY_CARD

#ifdef ENABLE_DATE_CARD
  if (time != NULL) {
    placement->date_value = time->tm_mday;
  }
#endif  // ENABLE_DATE_CARD

  placement->hour_buzzer = (ms / (SECONDS_PER_HOUR * 1000)) % 24;

#ifdef MAKE_CHRONOGRAPH
  {
    unsigned int chrono_ms = get_chrono_ms(ms);

    bool chrono_dial_wants_tenths = true;
    switch (config.chrono_dial) {
    case CDM_off:
      break;

    case CDM_tenths:
      chrono_dial_wants_tenths = true;
      break;

    case CDM_hours:
      chrono_dial_wants_tenths = false;
      break;

    case CDM_dual:
      // In dual mode, we show either tenths or hours, depending on the
      // amount of elapsed time.  Less than 30 minutes shows tenths.
      chrono_dial_wants_tenths = (chrono_ms < 30 * 60 * 1000);
      break;
    }

    if (chrono_dial_shows_tenths != chrono_dial_wants_tenths) {
      // The dial has changed states; reload and redraw it.
      chrono_dial_shows_tenths = chrono_dial_wants_tenths;
      bwd_destroy(&chrono_dial_white);
      bwd_destroy(&chrono_dial_black);
      if (chrono_dial_layer != NULL) {
	layer_mark_dirty(chrono_dial_layer);
      }
    }
    
#ifdef ENABLE_CHRONO_MINUTE_HAND
    // The chronograph minute hand rolls completely around in 30
    // minutes (not 60).
    {
      unsigned int use_ms = chrono_ms % (1800 * 1000);
      placement->chrono_minute_hand_index = ((NUM_STEPS_CHRONO_MINUTE * use_ms) / (1800 * 1000)) % NUM_STEPS_CHRONO_MINUTE;
    }
#endif  // ENABLE_CHRONO_MINUTE_HAND

#ifdef ENABLE_CHRONO_SECOND_HAND
    {
      // Avoid overflowing the integer arithmetic by pre-constraining
      // the ms value to the appropriate range.
      unsigned int use_ms = chrono_ms % (60 * 1000);
      if (!config.sweep_seconds) {
	// Also constrain to an integer second if we've not enabled sweep-second resolution.
	use_ms = (use_ms / 1000) * 1000;
      }
      placement->chrono_second_hand_index = ((NUM_STEPS_CHRONO_SECOND * use_ms) / (60 * 1000));
    }
#endif  // ENABLE_CHRONO_SECOND_HAND

#ifdef ENABLE_CHRONO_TENTH_HAND
    if (config.chrono_dial == CDM_off) {
      // Don't keep updating this hand if we're not showing it anyway.
      placement->chrono_tenth_hand_index = 0;
    } else {
      if (chrono_dial_shows_tenths) {
	// Drawing tenths-of-a-second.
	if (chrono_data.running && !chrono_data.lap_paused) {
	  // We don't actually show the tenths time while the chrono is running.
	  placement->chrono_tenth_hand_index = 0;
	} else {
	  // We show the tenths time when the chrono is stopped or showing
	  // the lap time.
	  unsigned int use_ms = chrono_ms % 1000;
	  // Truncate to the previous 0.1 seconds (100 ms), just to
	  // make the dial easier to read.
	  use_ms = 100 * (use_ms / 100);
	  placement->chrono_tenth_hand_index = ((NUM_STEPS_CHRONO_TENTH * use_ms) / (1000)) % NUM_STEPS_CHRONO_TENTH;
	}
      } else {
	// Drawing hours.  12-hour scale.
        unsigned int use_ms = chrono_ms % (12 * SECONDS_PER_HOUR * 1000);
	placement->chrono_tenth_hand_index = ((NUM_STEPS_CHRONO_TENTH * use_ms) / (12 * SECONDS_PER_HOUR * 1000)) % NUM_STEPS_CHRONO_TENTH;
      }
    }
#endif  // ENABLE_CHRONO_TENTH_HAND

  }
#endif  // MAKE_CHRONOGRAPH
}


// Reverse the bits of a byte.
// http://www-graphics.stanford.edu/~seander/bithacks.html#BitReverseTable
uint8_t reverse_bits(uint8_t b) {
  return ((b * 0x0802LU & 0x22110LU) | (b * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16; 
}

// Horizontally flips the indicated GBitmap in-place.  Requires
// that the width be a multiple of 8 pixels.
void flip_bitmap_x(GBitmap *image, int *cx) {
  int height = image->bounds.size.h;
  int width = image->bounds.size.w;  // multiple of 8, by our convention.
  int width_bytes = width / 8;
  int stride = image->row_size_bytes; // multiple of 4, by Pebble.
  uint8_t *data = image->addr;

  for (int y = 0; y < height; ++y) {
    uint8_t *row = data + y * stride;
    for (int x1 = (width_bytes - 1) / 2; x1 >= 0; --x1) {
      int x2 = width_bytes - 1 - x1;
      uint8_t b = reverse_bits(row[x1]);
      row[x1] = reverse_bits(row[x2]);
      row[x2] = b;
    }
  }

  if (cx != NULL) {
    *cx = width- 1 - *cx;
  }
}

// Vertically flips the indicated GBitmap in-place.
void flip_bitmap_y(GBitmap *image, int *cy) {
  int height = image->bounds.size.h;
  int stride = image->row_size_bytes; // multiple of 4.
  uint8_t *data = image->addr;

#if 1
  /* This is the slightly slower flip, that requires less RAM on the
     stack. */
  uint8_t buffer[stride]; // gcc lets us do this.
  for (int y1 = (height - 1) / 2; y1 >= 0; --y1) {
    int y2 = height - 1 - y1;
    // Swap rows y1 and y2.
    memcpy(buffer, data + y1 * stride, stride);
    memcpy(data + y1 * stride, data + y2 * stride, stride);
    memcpy(data + y2 * stride, buffer, stride);
  }

#else
  /* This is the slightly faster flip, that requires more RAM on the
     stack.  I have no idea what our stack limit is on the Pebble, or
     what happens if we exceed it. */
  uint8_t buffer[height * stride]; // gcc lets us do this.
  memcpy(buffer, data, height * stride);
  for (int y1 = 0; y1 < height; ++y1) {
    int y2 = height - 1 - y1;
    memcpy(data + y1 * stride, buffer + y2 * stride, stride);
  }
#endif

  if (cy != NULL) {
    *cy = height - 1 - *cy;
  }
}

// Draws a given hand on the face, using the vector structures.
void draw_vector_hand(struct HandCache *hand_cache, struct VectorHandTable *hand, int hand_index, int num_steps,
                      int place_x, int place_y, GContext *ctx) {
  int gi;
  if (hand_cache->vector_hand_index != hand_index) {
    // Force a new path.
    for (gi = 0; gi < hand->num_groups; ++gi) {
      if (hand_cache->path[gi] != NULL) {
        gpath_destroy(hand_cache->path[gi]);
        hand_cache->path[gi] = NULL;
      }
    }
    hand_cache->vector_hand_index = hand_index;
  }

  GPoint center = { place_x, place_y };
  int32_t angle = TRIG_MAX_ANGLE * hand_index / num_steps;

  assert(hand->num_groups <= HAND_CACHE_MAX_GROUPS);
  for (gi = 0; gi < hand->num_groups; ++gi) {
    struct VectorHandGroup *group = &hand->group[gi];

    if (hand_cache->path[gi] == NULL) {
      hand_cache->path[gi] = gpath_create(&group->path_info);

      gpath_rotate_to(hand_cache->path[gi], angle);
      gpath_move_to(hand_cache->path[gi], center);
    }

    if (group->fill != GColorClear) {
      graphics_context_set_fill_color(ctx, draw_mode_table[config.draw_mode].colors[group->fill]);
      gpath_draw_filled(ctx, hand_cache->path[gi]);
    }
    if (group->outline != GColorClear) {
      graphics_context_set_stroke_color(ctx, draw_mode_table[config.draw_mode].colors[group->outline]);
      gpath_draw_outline(ctx, hand_cache->path[gi]);
    }
  }
}

void hand_cache_init(struct HandCache *hand_cache) {
  memset(hand_cache, 0, sizeof(struct HandCache));
}

void hand_cache_destroy(struct HandCache *hand_cache) {
  if (hand_cache->image.bitmap != NULL) {
    bwd_destroy(&hand_cache->image);
  }
  if (hand_cache->mask.bitmap != NULL) {
    bwd_destroy(&hand_cache->mask);
  }
  int gi;
  for (gi = 0; gi < HAND_CACHE_MAX_GROUPS; ++gi) {
    if (hand_cache->path[gi] != NULL) {
      gpath_destroy(hand_cache->path[gi]);
      hand_cache->path[gi] = NULL;
    }
  }
}

// Draws a given hand on the face, using the bitmap structures.
void draw_bitmap_hand(struct HandCache *hand_cache, struct BitmapHandLookupRow *lookup_table, struct BitmapHandTableRow *hand_table, int hand_index, bool use_rle, int place_x, int place_y, bool paint_black, GContext *ctx) {
  if (hand_cache->bitmap_hand_index != hand_index) {
    // Force a new bitmap.
    if (hand_cache->image.bitmap != NULL) {
      bwd_destroy(&hand_cache->image);
    }
    if (hand_cache->mask.bitmap != NULL) {
      bwd_destroy(&hand_cache->mask);
    }
    hand_cache->bitmap_hand_index = hand_index;
  }

  struct BitmapHandTableRow *hand = &hand_table[hand_index];
  struct BitmapHandLookupRow *lookup = &lookup_table[hand->lookup_index];
  
  if (lookup->mask_id == lookup->image_id) {
    // The hand does not have a mask.  Draw the hand on top of the scene.
    if (hand_cache->image.bitmap == NULL) {
      if (use_rle) {
	hand_cache->image = rle_bwd_create(lookup->image_id);
      } else {
	hand_cache->image = png_bwd_create(lookup->image_id);
      }
      hand_cache->cx = lookup->cx;
      hand_cache->cy = lookup->cy;
    
      if (hand->flip_x) {
        // To minimize wasteful resource usage, if the hand is symmetric
        // we can store only the bitmaps for the right half of the clock
        // face, and flip them for the left half.
        flip_bitmap_x(hand_cache->image.bitmap, &hand_cache->cx);
      }
    
      if (hand->flip_y) {
        // We can also do this vertically.
        flip_bitmap_y(hand_cache->image.bitmap, &hand_cache->cy);
      }
    }
      
    // We make sure the dimensions of the GRect to draw into
    // are equal to the size of the bitmap--otherwise the image
    // will automatically tile.
    GRect destination = hand_cache->image.bitmap->bounds;
    
    // Place the hand's center point at place_x, place_y.
    destination.origin.x = place_x - hand_cache->cx;
    destination.origin.y = place_y - hand_cache->cy;
    
    // Specify a compositing mode to make the hands overlay on top of
    // each other, instead of the background parts of the bitmaps
    // blocking each other.

    if (paint_black) {
      // Painting foreground ("white") pixels as black.
      graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode].paint_black);
    } else {
      // Painting foreground ("white") pixels as white.
      graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode].paint_white);
    }
      
    graphics_draw_bitmap_in_rect(ctx, hand_cache->image.bitmap, destination);
    
  } else {
    // The hand has a mask, so use it to draw the hand opaquely.
    if (hand_cache->image.bitmap == NULL) {
      if (use_rle) {
	hand_cache->image = rle_bwd_create(lookup->image_id);
	hand_cache->mask = rle_bwd_create(lookup->mask_id);
      } else {
	hand_cache->image = png_bwd_create(lookup->image_id);
	hand_cache->mask = png_bwd_create(lookup->mask_id);
      }
      hand_cache->cx = lookup->cx;
      hand_cache->cy = lookup->cy;
    
      if (hand->flip_x) {
        // To minimize wasteful resource usage, if the hand is symmetric
        // we can store only the bitmaps for the right half of the clock
        // face, and flip them for the left half.
        flip_bitmap_x(hand_cache->image.bitmap, &hand_cache->cx);
        flip_bitmap_x(hand_cache->mask.bitmap, NULL);
      }
    
      if (hand->flip_y) {
        // We can also do this vertically.
        flip_bitmap_y(hand_cache->image.bitmap, &hand_cache->cy);
        flip_bitmap_y(hand_cache->mask.bitmap, NULL);
      }
    }
    
    GRect destination = hand_cache->image.bitmap->bounds;
    
    destination.origin.x = place_x - hand_cache->cx;
    destination.origin.y = place_y - hand_cache->cy;

    graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode].paint_white);
    graphics_draw_bitmap_in_rect(ctx, hand_cache->mask.bitmap, destination);
    
    graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode].paint_black);
    graphics_draw_bitmap_in_rect(ctx, hand_cache->image.bitmap, destination);
  }
}
  
void hour_layer_update_callback(Layer *me, GContext *ctx) {
  //  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "hour_layer");

#ifdef VECTOR_HOUR_HAND
  draw_vector_hand(&hour_cache, &hour_hand_vector_table, current_placement.hour_hand_index,
		   NUM_STEPS_HOUR, HOUR_HAND_X, HOUR_HAND_Y, ctx);
#endif

#ifdef BITMAP_HOUR_HAND
  draw_bitmap_hand(&hour_cache, hour_hand_bitmap_lookup, hour_hand_bitmap_table, current_placement.hour_hand_index,
                   true, HOUR_HAND_X, HOUR_HAND_Y, BITMAP_HOUR_HAND_PAINT_BLACK, ctx);
#endif
}

void minute_layer_update_callback(Layer *me, GContext *ctx) {
  //  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "minute_layer");

#ifdef VECTOR_MINUTE_HAND
  draw_vector_hand(&minute_cache, &minute_hand_vector_table, current_placement.minute_hand_index,
                   NUM_STEPS_MINUTE, MINUTE_HAND_X, MINUTE_HAND_Y, ctx);
#endif

#ifdef BITMAP_MINUTE_HAND
  draw_bitmap_hand(&minute_cache, minute_hand_bitmap_lookup, minute_hand_bitmap_table, current_placement.minute_hand_index,
                   true, MINUTE_HAND_X, MINUTE_HAND_Y, BITMAP_MINUTE_HAND_PAINT_BLACK, ctx);
#endif
}

void second_layer_update_callback(Layer *me, GContext *ctx) {
  //  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "second_layer");

  if (config.second_hand) {
#ifdef VECTOR_SECOND_HAND
    draw_vector_hand(&second_cache, &second_hand_vector_table, current_placement.second_hand_index,
		     NUM_STEPS_SECOND, SECOND_HAND_X, SECOND_HAND_Y, ctx);
#endif
    
#ifdef BITMAP_SECOND_HAND
    draw_bitmap_hand(&second_cache, second_hand_bitmap_lookup, second_hand_bitmap_table, current_placement.second_hand_index,
		     false, SECOND_HAND_X, SECOND_HAND_Y, BITMAP_SECOND_HAND_PAINT_BLACK, ctx);
#endif
  }
}

#ifdef ENABLE_CHRONO_MINUTE_HAND
void chrono_minute_layer_update_callback(Layer *me, GContext *ctx) {
  //  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "chrono_minute_layer");

  if (config.second_hand || chrono_data.running || chrono_data.hold_ms != 0) {
#ifdef VECTOR_CHRONO_MINUTE_HAND
    draw_vector_hand(&chrono_minute_cache, &chrono_minute_hand_vector_table, current_placement.chrono_minute_hand_index,
		     NUM_STEPS_CHRONO_MINUTE, CHRONO_MINUTE_HAND_X, CHRONO_MINUTE_HAND_Y, ctx);
#endif
    
#ifdef BITMAP_CHRONO_MINUTE_HAND
    draw_bitmap_hand(&chrono_minute_cache, chrono_minute_hand_bitmap_lookup, chrono_minute_hand_bitmap_table, current_placement.chrono_minute_hand_index,
		     true, CHRONO_MINUTE_HAND_X, CHRONO_MINUTE_HAND_Y, BITMAP_CHRONO_MINUTE_HAND_PAINT_BLACK, ctx);
#endif
  }
}
#endif  // ENABLE_CHRONO_MINUTE_HAND

#ifdef ENABLE_CHRONO_SECOND_HAND
void chrono_second_layer_update_callback(Layer *me, GContext *ctx) {
  //  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "chrono_second_layer");

  if (config.second_hand || chrono_data.running || chrono_data.hold_ms != 0) {
#ifdef VECTOR_CHRONO_SECOND_HAND
    draw_vector_hand(&chrono_second_cache, &chrono_second_hand_vector_table, current_placement.chrono_second_hand_index,
		     NUM_STEPS_CHRONO_SECOND, CHRONO_SECOND_HAND_X, CHRONO_SECOND_HAND_Y, ctx);
#endif
    
#ifdef BITMAP_CHRONO_SECOND_HAND
    draw_bitmap_hand(&chrono_second_cache, chrono_second_hand_bitmap_lookup, chrono_second_hand_bitmap_table, current_placement.chrono_second_hand_index,
		     false, CHRONO_SECOND_HAND_X, CHRONO_SECOND_HAND_Y, BITMAP_CHRONO_SECOND_HAND_PAINT_BLACK, ctx);
#endif
  }
}
#endif  // ENABLE_CHRONO_SECOND_HAND

#ifdef ENABLE_CHRONO_TENTH_HAND
void chrono_tenth_layer_update_callback(Layer *me, GContext *ctx) {
  //  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "chrono_tenth_layer");

  if (config.chrono_dial != CDM_off) {
    if (config.second_hand || chrono_data.running || chrono_data.hold_ms != 0) {
#ifdef VECTOR_CHRONO_TENTH_HAND
      draw_vector_hand(&chrono_tenth_cache, &chrono_tenth_hand_vector_table, current_placement.chrono_tenth_hand_index,
		       NUM_STEPS_CHRONO_TENTH, CHRONO_TENTH_HAND_X, CHRONO_TENTH_HAND_Y, ctx);
#endif
      
#ifdef BITMAP_CHRONO_TENTH_HAND
      draw_bitmap_hand(&chrono_tenth_cache, chrono_tenth_hand_bitmap_lookup, chrono_tenth_hand_bitmap_table, current_placement.chrono_tenth_hand_index,
		       true, CHRONO_TENTH_HAND_X, CHRONO_TENTH_HAND_Y, BITMAP_CHRONO_TENTH_HAND_PAINT_BLACK, ctx);
#endif
    }
  }
}
#endif  // ENABLE_CHRONO_TENTH_HAND

void draw_card(Layer *me, GContext *ctx, const char *text, struct FontPlacement *font_placement, 
	       GFont *font, bool invert, bool opaque_layer) {
  GRect box;

  box = layer_get_frame(me);
  box.origin.x = 0;
  box.origin.y = 0;

  unsigned int draw_mode = invert ^ config.draw_mode;

  if (opaque_layer) {
    if (date_card_mask.bitmap == NULL) {
      date_card_mask = rle_bwd_create(RESOURCE_ID_DATE_CARD_MASK);
    }
    graphics_context_set_compositing_mode(ctx, draw_mode_table[draw_mode].paint_mask);
    graphics_draw_bitmap_in_rect(ctx, date_card_mask.bitmap, box);
  }
  
  if (date_card.bitmap == NULL) {
    date_card = rle_bwd_create(RESOURCE_ID_DATE_CARD);
  }

  graphics_context_set_compositing_mode(ctx, draw_mode_table[draw_mode].paint_fg);
  graphics_draw_bitmap_in_rect(ctx, date_card.bitmap, box);

  graphics_context_set_text_color(ctx, draw_mode_table[draw_mode].colors[1]);

  box.origin.y += font_placement->vshift;

  // Cheat for a bit more space for text
  box.origin.x -= 2;
  box.size.w += 4;
  box.size.h += 4;

  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "drawing card %02x, font = %p, shift = %d", (unsigned int)text[0], (void *)(*font), font_placement->vshift);
  if ((*font) == NULL) {
    (*font) = fonts_load_custom_font(resource_get_handle(font_placement->resource_id));
    app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "loaded font %d, font = %p", font_placement->resource_id, (void *)(*font));
  }

  graphics_draw_text(ctx, text, (*font), box,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}

#ifdef MAKE_CHRONOGRAPH
void chrono_dial_layer_update_callback(Layer *me, GContext *ctx) {
  //  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "chrono_dial_layer");
  if (config.chrono_dial != CDM_off) {
    if (chrono_dial_white.bitmap == NULL) {
      load_chrono_dial();
    }
    GRect destination = layer_get_bounds(me);
    destination.origin.x = 0;
    destination.origin.y = 0;

    graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode].paint_black);
    graphics_draw_bitmap_in_rect(ctx, chrono_dial_black.bitmap, destination);
    graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode].paint_white);
    graphics_draw_bitmap_in_rect(ctx, chrono_dial_white.bitmap, destination);
  }
}
#endif  // MAKE_CHRONOGRAPH

#ifdef ENABLE_DAY_CARD
void day_layer_update_callback(Layer *me, GContext *ctx) {
  //  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "day_layer");

  if (config.show_day) {
    const LangDef *lang = &lang_table[config.display_lang % num_langs];
    const char *weekday_name = lang->weekday_names[current_placement.day_index];
    const struct IndicatorTable *card = &day_table[config.face_index];
    draw_card(me, ctx, weekday_name, &day_font_placement[lang->font_index], &day_font, card->invert, card->opaque);
  }
}
#endif  // ENABLE_DAY_CARD

#ifdef ENABLE_DATE_CARD
void date_layer_update_callback(Layer *me, GContext *ctx) {
  //  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "date_layer");

  if (config.show_date) {
    static const int buffer_size = 16;
    char buffer[buffer_size];
    snprintf(buffer, buffer_size, "%d", current_placement.date_value);
    const struct IndicatorTable *card = &date_table[config.face_index];
    draw_card(me, ctx, buffer, &date_font_placement, &date_font, card->invert, card->opaque);
  }
}
#endif  // ENABLE_DATE_CARD

void update_hands(struct tm *time) {
  struct HandPlacement new_placement = current_placement;

  compute_hands(time, &new_placement);
  if (new_placement.hour_hand_index != current_placement.hour_hand_index) {
    current_placement.hour_hand_index = new_placement.hour_hand_index;
    layer_mark_dirty(hour_layer);
  }

  if (new_placement.minute_hand_index != current_placement.minute_hand_index) {
    current_placement.minute_hand_index = new_placement.minute_hand_index;
    layer_mark_dirty(minute_layer);
  }

  if (new_placement.second_hand_index != current_placement.second_hand_index) {
    current_placement.second_hand_index = new_placement.second_hand_index;
    layer_mark_dirty(second_layer);
  }

  if (new_placement.hour_buzzer != current_placement.hour_buzzer) {
    current_placement.hour_buzzer = new_placement.hour_buzzer;
    if (config.hour_buzzer) {
      vibes_short_pulse();
    }
  }

  // Make sure the sweep timer is fast enough to capture the second
  // hand.
  sweep_timer_ms = 1000;
  if (config.sweep_seconds) {
    sweep_timer_ms = sweep_seconds_ms;
  }

#ifdef MAKE_CHRONOGRAPH

#ifdef ENABLE_CHRONO_MINUTE_HAND
  if (new_placement.chrono_minute_hand_index != current_placement.chrono_minute_hand_index) {
    current_placement.chrono_minute_hand_index = new_placement.chrono_minute_hand_index;
    layer_mark_dirty(chrono_minute_layer);
  }
#endif  // ENABLE_CHRONO_MINUTE_HAND

#ifdef ENABLE_CHRONO_SECOND_HAND
  if (new_placement.chrono_second_hand_index != current_placement.chrono_second_hand_index) {
    current_placement.chrono_second_hand_index = new_placement.chrono_second_hand_index;
    layer_mark_dirty(chrono_second_layer);
  }
#endif  // ENABLE_CHRONO_SECOND_HAND

#ifdef ENABLE_CHRONO_TENTH_HAND
  if (new_placement.chrono_tenth_hand_index != current_placement.chrono_tenth_hand_index) {
    current_placement.chrono_tenth_hand_index = new_placement.chrono_tenth_hand_index;
    layer_mark_dirty(chrono_tenth_layer);
  }
#endif  // ENABLE_CHRONO_TENTH_HAND

  if (config.sweep_seconds) {
    if (chrono_data.running && !chrono_data.lap_paused && !chrono_digital_window_showing) {
      // With the chronograph running, the sweep timer must be fast
      // enough to capture the chrono second hand.
      if (sweep_chrono_seconds_ms < sweep_timer_ms) {
        sweep_timer_ms = sweep_chrono_seconds_ms;
      }
    }
  }

#endif  // MAKE_CHRONOGRAPH

#ifdef ENABLE_DAY_CARD
  if (new_placement.day_index != current_placement.day_index) {
    current_placement.day_index = new_placement.day_index;
    layer_mark_dirty(day_layer);
  }
#endif  // ENABLE_DAY_CARD

#ifdef ENABLE_DATE_CARD
  if (new_placement.date_value != current_placement.date_value) {
    current_placement.date_value = new_placement.date_value;
    layer_mark_dirty(date_layer);
  }
#endif  // ENABLE_DATE_CARD
}

// Triggered at sweep_timer_ms intervals to run the sweep-second hand.
void handle_sweep(void *data) {
  sweep_timer = NULL;  // When the timer is handled, it is implicitly canceled.
  if (sweep_timer_ms < 1000) {
    update_hands(NULL);
    sweep_timer = app_timer_register(sweep_timer_ms, &handle_sweep, 0);
  }
}

void reset_sweep() {
  if (sweep_timer != NULL) {
    app_timer_cancel(sweep_timer);
    sweep_timer = NULL;
  }
  if (sweep_timer_ms < 1000) {
    sweep_timer = app_timer_register(sweep_timer_ms, &handle_sweep, 0);
  }
}

// Compute new hand positions once a minute (or once a second).
void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  update_hands(tick_time);
  reset_sweep();
}

// Forward references.
void stopped_click_config_provider(void *context);
void started_click_config_provider(void *context);
void reset_chrono_digital_timer();
void record_chrono_lap(int chrono_ms);
void update_chrono_laps_time();

void chrono_start_stop_handler(ClickRecognizerRef recognizer, void *context) {
  Window *window = (Window *)context;
  unsigned int ms = get_time_ms(NULL);

  // The start/stop button was pressed.
  if (chrono_data.running) {
    // If the chronograph is currently running, this means to stop (or
    // pause).
    chrono_data.hold_ms = ms - chrono_data.start_ms;
    chrono_data.running = false;
    chrono_data.lap_paused = false;
    vibes_enqueue_custom_pattern(tap);
    update_hands(NULL);
    apply_config();

    // We change the click config provider according to the chrono run
    // state.  When the chrono is stopped, we listen for a different
    // set of buttons than when it is started.
    window_set_click_config_provider(window, &stopped_click_config_provider);
    window_set_click_config_provider(chrono_digital_window, &stopped_click_config_provider);
  } else {
    // If the chronograph is not currently running, this means to
    // start, from the currently showing Chronograph time.
    chrono_data.start_ms = ms - chrono_data.hold_ms;
    chrono_data.running = true;
    if (config.sweep_seconds) {
      if (sweep_chrono_seconds_ms < sweep_timer_ms) {
        sweep_timer_ms = sweep_chrono_seconds_ms;
      }
    }
    vibes_enqueue_custom_pattern(tap);
    update_hands(NULL);
    apply_config();

    window_set_click_config_provider(window, &started_click_config_provider);
    window_set_click_config_provider(chrono_digital_window, &started_click_config_provider);
  }
}

void chrono_lap_button() {
  unsigned int ms;
 
  ms = get_time_ms(NULL);

  if (chrono_data.lap_paused) {
    // If we were already paused, this resumes the motion, jumping
    // ahead to the currently elapsed time.
    chrono_data.lap_paused = false;
    vibes_enqueue_custom_pattern(tap);
    update_hands(NULL);
  } else {
    // If we were not already paused, this pauses the hands here (but
    // does not stop the timer).
    unsigned int lap_ms = ms - chrono_data.start_ms;
    record_chrono_lap(lap_ms);
    if (!chrono_digital_window_showing) {
      // Actually, we only pause the hands if we're not looking at the
      // digital timer.
      chrono_data.hold_ms = lap_ms;
      chrono_data.lap_paused = true;
    }
    vibes_enqueue_custom_pattern(tap);
    update_hands(NULL);
  }
}

void chrono_reset_button() {
  // Resets the chronometer to 0 time.
  time_t now;
  struct tm *this_time;

  now = time(NULL);
  this_time = localtime(&now);
  chrono_data.running = false;
  chrono_data.lap_paused = false;
  chrono_data.start_ms = 0;
  chrono_data.hold_ms = 0;
  memset(&chrono_data.laps[0], 0, sizeof(chrono_data.laps[0]) * CHRONO_MAX_LAPS);
  vibes_double_pulse();
  update_chrono_laps_time();
  update_hands(this_time);
  apply_config();
}

void chrono_lap_handler(ClickRecognizerRef recognizer, void *context) {
  // The lap/reset button was pressed (briefly).

  // We only do anything here if the chronograph is currently running.
  if (chrono_data.running) {
    chrono_lap_button();
  }
}

void chrono_lap_or_reset_handler(ClickRecognizerRef recognizer, void *context) {
  // The lap/reset button was pressed (long press).

  // This means a lap if the chronograph is running, and a reset if it
  // is not.
  if (chrono_data.running) {
    chrono_lap_button();
  } else {
    chrono_reset_button();
  }
}

void push_chrono_digital_handler(ClickRecognizerRef recognizer, void *context) {
  //  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "push chrono digital");
  if (!chrono_digital_window_showing) {
    window_stack_push(chrono_digital_window, true);
  }
}

// Enable the set of buttons active while the chrono is stopped.
void stopped_click_config_provider(void *context) {
  // single click config:
  window_single_click_subscribe(BUTTON_ID_UP, &chrono_start_stop_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, NULL);

  // long click config:
  window_long_click_subscribe(BUTTON_ID_DOWN, 700, &chrono_lap_or_reset_handler, NULL);

  // To push the digital chrono display.
  window_single_click_subscribe(BUTTON_ID_SELECT, &push_chrono_digital_handler);
}

// Enable the set of buttons active while the chrono is running.
void started_click_config_provider(void *context) {
  // single click config:
  window_single_click_subscribe(BUTTON_ID_UP, &chrono_start_stop_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, &chrono_lap_handler);

  // It's important to disable the lock_click handler while the chrono
  // is running, so that the normal click handler (above) can be
  // immediately responsive.  If we leave the long_click handler
  // active, then the underlying SDK has to wait the full 700 ms to
  // differentiate a long_click from a click, which makes the lap
  // response sluggish.
  window_long_click_subscribe(BUTTON_ID_DOWN, 0, NULL, NULL);

  // To push the digital chrono display.
  window_single_click_subscribe(BUTTON_ID_SELECT, &push_chrono_digital_handler);
}

void window_load_handler(struct Window *window) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "main window loads");
}

void window_appear_handler(struct Window *window) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "main window appears");

#ifdef MAKE_CHRONOGRAPH
  if (chrono_data.running) {
    window_set_click_config_provider(window, &started_click_config_provider);
  } else {
    window_set_click_config_provider(window, &stopped_click_config_provider);
  }
#endif  // MAKE_CHRONOGRAPH
}

void window_disappear_handler(struct Window *window) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "main window disappears");
}

void window_unload_handler(struct Window *window) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "main window unloads");
}

void update_chrono_laps_time() {
#ifdef MAKE_CHRONOGRAPH
  int i;

  for (i = 0; i < CHRONO_MAX_LAPS; ++i) {
    unsigned int chrono_ms = chrono_data.laps[i];
    unsigned int chrono_h = chrono_ms / (1000 * 60 * 60);
    unsigned int chrono_m = (chrono_ms / (1000 * 60)) % 60;
    unsigned int chrono_s = (chrono_ms / (1000)) % 60;
    unsigned int chrono_t = (chrono_ms / (100)) % 10;

    if (chrono_ms == 0) {
      // No data: empty string.
      chrono_laps_buffer[i][0] = '\0';
    } else {
      // Real data: formatted string.
      snprintf(chrono_laps_buffer[i], CHRONO_DIGITAL_BUFFER_SIZE, "%u:%02u:%02u.%u", 
	       chrono_h, chrono_m, chrono_s, chrono_t);
    }
    if (chrono_digital_laps_layer[i] != NULL) {
      layer_mark_dirty((Layer *)chrono_digital_laps_layer[i]);
    }
  }
#endif  // MAKE_CHRONOGRAPH
}

#ifdef MAKE_CHRONOGRAPH
void record_chrono_lap(int chrono_ms) {
  // Lose the first one.
  memmove(&chrono_data.laps[0], &chrono_data.laps[1], sizeof(chrono_data.laps[0]) * (CHRONO_MAX_LAPS - 1));
  chrono_data.laps[CHRONO_MAX_LAPS - 1] = chrono_ms;
  update_chrono_laps_time();
}

void update_chrono_current_time() {
  unsigned int ms = get_time_ms(NULL);
  unsigned int chrono_ms = get_chrono_ms(ms);
  unsigned int chrono_h = chrono_ms / (1000 * 60 * 60);
  unsigned int chrono_m = (chrono_ms / (1000 * 60)) % 60;
  unsigned int chrono_s = (chrono_ms / (1000)) % 60;
  unsigned int chrono_t = (chrono_ms / (100)) % 10;
  
  snprintf(chrono_current_buffer, CHRONO_DIGITAL_BUFFER_SIZE, "%u:%02u:%02u.%u", 
	   chrono_h, chrono_m, chrono_s, chrono_t);
  if (chrono_digital_current_layer != NULL) {
    layer_mark_dirty((Layer *)chrono_digital_current_layer);
  }
}

void handle_chrono_digital_timer(void *data) {
  chrono_digital_timer = NULL;  // When the timer is handled, it is implicitly canceled.

  reset_chrono_digital_timer();
}

void chrono_digital_window_load_handler(struct Window *window) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "chrono digital loads");

  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);

  Layer *chrono_digital_window_layer = window_get_root_layer(chrono_digital_window);

  chrono_digital_current_layer = text_layer_create(GRect(25, 120, 94, 48));
  int i;
  for (i = 0; i < CHRONO_MAX_LAPS; ++i) {
    chrono_digital_laps_layer[i] = text_layer_create(GRect(25, 30 * i, 94, 30));

    text_layer_set_text(chrono_digital_laps_layer[i], chrono_laps_buffer[i]);
    text_layer_set_text_color(chrono_digital_laps_layer[i], GColorBlack);
    text_layer_set_text_alignment(chrono_digital_laps_layer[i], GTextAlignmentRight);
    text_layer_set_overflow_mode(chrono_digital_laps_layer[i], GTextOverflowModeFill);
    text_layer_set_font(chrono_digital_laps_layer[i], font);
    layer_add_child(chrono_digital_window_layer, (Layer *)chrono_digital_laps_layer[i]);
  }

  text_layer_set_text(chrono_digital_current_layer, chrono_current_buffer);
  text_layer_set_text_color(chrono_digital_current_layer, GColorBlack);
  text_layer_set_text_alignment(chrono_digital_current_layer, GTextAlignmentRight);
  text_layer_set_overflow_mode(chrono_digital_current_layer, GTextOverflowModeFill);
  text_layer_set_font(chrono_digital_current_layer, font);
  layer_add_child(chrono_digital_window_layer, (Layer *)chrono_digital_current_layer);

  chrono_digital_line_layer = inverter_layer_create(GRect(0, 121, SCREEN_WIDTH, 1));
  layer_add_child(chrono_digital_window_layer, (Layer *)chrono_digital_line_layer);
}

void chrono_digital_window_appear_handler(struct Window *window) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "chrono digital appears");
  chrono_digital_window_showing = true;

  // We never have the lap timer paused while the digital window is visible.
  chrono_data.lap_paused = false;

  if (chrono_data.running) {
    window_set_click_config_provider(chrono_digital_window, &started_click_config_provider);
  } else {
    window_set_click_config_provider(chrono_digital_window, &stopped_click_config_provider);
  }

  reset_chrono_digital_timer();
}

void chrono_digital_window_disappear_handler(struct Window *window) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "chrono digital disappears");
  chrono_digital_window_showing = false;
  reset_chrono_digital_timer();
}

void chrono_digital_window_unload_handler(struct Window *window) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "chrono digital unloads");

  if (chrono_digital_line_layer != NULL) {
    inverter_layer_destroy(chrono_digital_line_layer);
    chrono_digital_line_layer = NULL;
  }

  if (chrono_digital_current_layer != NULL) {
    text_layer_destroy(chrono_digital_current_layer);
    chrono_digital_current_layer = NULL;
  }
  int i;
  for (i = 0; i < CHRONO_MAX_LAPS; ++i) {
    if (chrono_digital_laps_layer[i] != NULL) {
      text_layer_destroy(chrono_digital_laps_layer[i]);
      chrono_digital_laps_layer[i] = NULL;
    }
  }
}
#endif  // MAKE_CHRONOGRAPH

void reset_chrono_digital_timer() {
#ifdef MAKE_CHRONOGRAPH
  if (chrono_digital_timer != NULL) {
    app_timer_cancel(chrono_digital_timer);
    chrono_digital_timer = NULL;
  }
  update_chrono_current_time();
  if (chrono_digital_window_showing && chrono_data.running) {
    // Set the timer for the next update.
    chrono_digital_timer = app_timer_register(CHRONO_DIGITAL_TICK_MS, &handle_chrono_digital_timer, 0);
  }
#endif  // MAKE_CHRONOGRAPH
}


// Updates any runtime settings as needed when the config changes.
void apply_config() {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "apply_config, second_hand=%d", config.second_hand);
  tick_timer_service_unsubscribe();

#if defined(FAST_TIME) || defined(BATTERY_HACK)
  tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
#else
  if (config.second_hand || chrono_data.running) {
    tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
  } else {
    tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
  }
#endif

  if (face_index != config.face_index) {
    // Update the face bitmap if it's changed.
    face_index = config.face_index;
    bwd_destroy(&clock_face);
    clock_face = rle_bwd_create(clock_face_table[face_index]);
    bitmap_layer_set_bitmap(clock_face_layer, clock_face.bitmap);

    // Also move any layers to their new position on this face.
#ifdef ENABLE_DAY_CARD
    {
      const struct IndicatorTable *card = &day_table[config.face_index];
      app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "day_layer moved to %d, %d", card->x, card->y);
      layer_set_frame((Layer *)day_layer, GRect(card->x - 15, card->y - 8, 31, 19));
    }
#endif  // ENABLE_DAY_CARD

#ifdef ENABLE_DATE_CARD
    {
      const struct IndicatorTable *card = &date_table[config.face_index];
      layer_set_frame((Layer *)date_layer, GRect(card->x - 15, card->y - 8, 31, 19));
    }
#endif  // ENABLE_DATE_CARD
    {
      const struct IndicatorTable *card = &battery_table[config.face_index];
      move_battery_gauge(card->x, card->y, card->invert, card->opaque);
    }
    {
      const struct IndicatorTable *card = &bluetooth_table[config.face_index];
      move_bluetooth_indicator(card->x, card->y, card->invert, card->opaque);
    }
  }

  // Unload the day font just in case it changes with the language.
  if (day_font != NULL) {
    fonts_unload_custom_font(day_font);
    day_font = NULL;
  }

  // Also adjust the draw mode on the clock_face_layer.  (The other
  // layers all draw themselves interactively.)
  bitmap_layer_set_compositing_mode(clock_face_layer, draw_mode_table[config.draw_mode].paint_assign);
  layer_mark_dirty((Layer *)clock_face_layer);
  reset_chrono_digital_timer();
}

void
load_chrono_data() {
#ifdef MAKE_CHRONOGRAPH
  ChronoData local_data;
  if (persist_read_data(PERSIST_KEY + 0x100, &local_data, sizeof(local_data)) == sizeof(local_data)) {
    chrono_data = local_data;
    saved_chrono_data = local_data;
    // Ensure the start time is within modulo 24 hours of the current
    // day, to minimize the danger of integer overflow after 49 days.
    // As long as you launch the Chronograph at least once every 49
    // days, we'll keep an accurate time measurement (but we only ever
    // show modulo 24 hours).
    app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "Loaded chrono_data");
    if (chrono_data.running) {
      unsigned int ms = get_time_ms(NULL);
      unsigned int chrono_ms = (ms - chrono_data.start_ms + MS_PER_DAY) % MS_PER_DAY;
      app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "Modulated start_ms from %u to %u", chrono_data.start_ms, ms - chrono_ms);
      chrono_data.start_ms = ms - chrono_ms;
    }
    
    update_chrono_laps_time();
  } else {
    app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "Wrong previous chrono_data size or no previous data.");
  }
#endif  // MAKE_CHRONOGRAPH
}

void save_chrono_data() {
#ifdef MAKE_CHRONOGRAPH
  if (memcmp(&chrono_data, &saved_chrono_data, sizeof(chrono_data)) == 0) {
    app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "chrono_data unchanged.");
  } else {
    int wrote = persist_write_data(PERSIST_KEY + 0x100, &chrono_data, sizeof(chrono_data));
    if (wrote == sizeof(chrono_data)) {
      app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "Saved chrono_data (%d, %d)", PERSIST_KEY + 0x100, sizeof(chrono_data));
      saved_chrono_data = chrono_data;
    } else {
      app_log(APP_LOG_LEVEL_ERROR, __FILE__, __LINE__, "Error saving chrono_data (%d, %d): %d", PERSIST_KEY + 0x100, sizeof(chrono_data), wrote);
    }
  }
#endif  // MAKE_CHRONOGRAPH
}

void handle_init() {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "handle_init begin");
  load_config();
  load_chrono_data();

  app_message_register_inbox_received(receive_config_handler);
  app_message_register_inbox_dropped(dropped_config_handler);

  uint32_t inbox_max = app_message_inbox_size_maximum();
  uint32_t outbox_max = app_message_outbox_size_maximum();
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "available message space %u, %u", (unsigned int)inbox_max, (unsigned int)outbox_max);
  if (inbox_max > 200) {
    inbox_max = 200;
  }
  if (outbox_max > 50) {
    outbox_max = 50;
  }
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "app_message_open(%u, %u)", (unsigned int)inbox_max, (unsigned int)outbox_max);
  AppMessageResult open_result = app_message_open(inbox_max, outbox_max);
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "open_result = %d", open_result);

  time_t now = time(NULL);
  struct tm *startup_time = localtime(&now);
  int i;

  window = window_create();

  struct WindowHandlers window_handlers;
  memset(&window_handlers, 0, sizeof(window_handlers));
  window_handlers.load = window_load_handler;
  window_handlers.appear = window_appear_handler;
  window_handlers.disappear = window_disappear_handler;
  window_handlers.unload = window_unload_handler;
  window_set_window_handlers(window, window_handlers);

  window_set_fullscreen(window, true);
  window_stack_push(window, true /* Animated */);

  hand_cache_init(&hour_cache);
  hand_cache_init(&minute_cache);
  hand_cache_init(&second_cache);
  hand_cache_init(&chrono_minute_cache);
  hand_cache_init(&chrono_second_cache);
  hand_cache_init(&chrono_tenth_cache);

  compute_hands(startup_time, &current_placement);

  Layer *window_layer = window_get_root_layer(window);
  GRect window_frame = layer_get_frame(window_layer);

  clock_face_layer = bitmap_layer_create(window_frame);
  layer_add_child(window_layer, bitmap_layer_get_layer(clock_face_layer));

  date_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

#ifdef MAKE_CHRONOGRAPH
  {
    // We defer loading the chrono dial until we actually need to render it.
    //load_chrono_dial();
    int height = 56;   //chrono_dial_white.bitmap->bounds.size.h;
    int width = 56;    //chrono_dial_white.bitmap->bounds.size.w;
    int x = CHRONO_TENTH_HAND_X - width / 2;
    int y = CHRONO_TENTH_HAND_Y - height / 2;

    chrono_dial_layer = layer_create(GRect(x, y, width, height));
  }
  layer_set_update_proc(chrono_dial_layer, &chrono_dial_layer_update_callback);
  layer_add_child(window_layer, chrono_dial_layer);

  chrono_digital_window = window_create();

  struct WindowHandlers chrono_digital_window_handlers;
  memset(&chrono_digital_window_handlers, 0, sizeof(chrono_digital_window_handlers));
  chrono_digital_window_handlers.load = chrono_digital_window_load_handler;
  chrono_digital_window_handlers.appear = chrono_digital_window_appear_handler;
  chrono_digital_window_handlers.disappear = chrono_digital_window_disappear_handler;
  chrono_digital_window_handlers.unload = chrono_digital_window_unload_handler;
  window_set_window_handlers(chrono_digital_window, chrono_digital_window_handlers);

#endif  // MAKE_CHRONOGRAPH

  {
    const struct IndicatorTable *card = &battery_table[config.face_index];
    init_battery_gauge(window_layer, card->x, card->y, card->invert, card->opaque);
  }
  {
    const struct IndicatorTable *card = &bluetooth_table[config.face_index];
    init_bluetooth_indicator(window_layer, card->x, card->y, card->invert, card->opaque);
  }

#ifdef ENABLE_DAY_CARD
  {
    const struct IndicatorTable *card = &day_table[config.face_index];
    day_layer = layer_create(GRect(card->x - 15, card->y - 8, 31, 19));
    app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "day_layer created at %d, %d", card->x, card->y);
    layer_set_update_proc(day_layer, &day_layer_update_callback);
    layer_add_child(window_layer, day_layer);
  }
#endif  // ENABLE_DAY_CARD

#ifdef ENABLE_DATE_CARD
  {
    const struct IndicatorTable *card = &date_table[config.face_index];
    date_layer = layer_create(GRect(card->x - 15, card->y - 8, 31, 19));
    layer_set_update_proc(date_layer, &date_layer_update_callback);
    layer_add_child(window_layer, date_layer);
  }
#endif  // ENABLE_DATE_CARD

  // Init all of the hands, taking care to arrange them in the correct
  // stacking order.
  for (i = 0; stacking_order[i] != STACKING_ORDER_DONE; ++i) {
    switch (stacking_order[i]) {
    case STACKING_ORDER_HOUR:
      hour_layer = layer_create(window_frame);
      layer_set_update_proc(hour_layer, &hour_layer_update_callback);
      layer_add_child(window_layer, hour_layer);
      break;

    case STACKING_ORDER_MINUTE:
      minute_layer = layer_create(window_frame);
      layer_set_update_proc(minute_layer, &minute_layer_update_callback);
      layer_add_child(window_layer, minute_layer);
      break;

    case STACKING_ORDER_SECOND:
      second_layer = layer_create(window_frame);
      layer_set_update_proc(second_layer, &second_layer_update_callback);
      layer_add_child(window_layer, second_layer);
      break;

    case STACKING_ORDER_CHRONO_MINUTE:
#ifdef ENABLE_CHRONO_MINUTE_HAND
      chrono_minute_layer = layer_create(window_frame);
      layer_set_update_proc(chrono_minute_layer, &chrono_minute_layer_update_callback);
      layer_add_child(window_layer, chrono_minute_layer);
#endif  // ENABLE_CHRONO_MINUTE_HAND
      break;

    case STACKING_ORDER_CHRONO_SECOND:
#ifdef ENABLE_CHRONO_SECOND_HAND
      chrono_second_layer = layer_create(window_frame);
      layer_set_update_proc(chrono_second_layer, &chrono_second_layer_update_callback);
      layer_add_child(window_layer, chrono_second_layer);
#endif  // ENABLE_CHRONO_SECOND_HAND
      break;

    case STACKING_ORDER_CHRONO_TENTH:
#ifdef ENABLE_CHRONO_TENTH_HAND
      chrono_tenth_layer = layer_create(window_frame);
      layer_set_update_proc(chrono_tenth_layer, &chrono_tenth_layer_update_callback);
      layer_add_child(window_layer, chrono_tenth_layer);
#endif  // ENABLE_CHRONO_TENTH_HAND
      break;
    }
  }

  update_chrono_laps_time();
  apply_config();
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "handle_init done");
}


void handle_deinit() {
  save_chrono_data();
  tick_timer_service_unsubscribe();

  window_stack_pop_all(false);
  bitmap_layer_destroy(clock_face_layer);
  bwd_destroy(&clock_face);

#ifdef MAKE_CHRONOGRAPH
  layer_destroy(chrono_dial_layer);
  bwd_destroy(&chrono_dial_white);
  bwd_destroy(&chrono_dial_black);

  window_destroy(chrono_digital_window);
#endif  // MAKE_CHRONOGRAPH

  deinit_battery_gauge();
  deinit_bluetooth_indicator();

#ifdef ENABLE_DAY_CARD
  layer_destroy(day_layer);
#endif
#ifdef ENABLE_DATE_CARD
  layer_destroy(date_layer);
#endif
#if defined(ENABLE_DAY_CARD) || defined(ENABLE_DATE_CARD)
  bwd_destroy(&date_card);
  bwd_destroy(&date_card_mask);
#endif
  layer_destroy(minute_layer);
  layer_destroy(hour_layer);
  layer_destroy(second_layer);
#ifdef ENABLE_CHRONO_MINUTE_HAND
  layer_destroy(chrono_minute_layer);
#endif
#ifdef ENABLE_CHRONO_SECOND_HAND
  layer_destroy(chrono_second_layer);
#endif
#ifdef ENABLE_CHRONO_TENTH_HAND
  layer_destroy(chrono_tenth_layer);
#endif

  hand_cache_destroy(&hour_cache);
  hand_cache_destroy(&minute_cache);
  hand_cache_destroy(&second_cache);
  hand_cache_destroy(&chrono_minute_cache);
  hand_cache_destroy(&chrono_second_cache);
  hand_cache_destroy(&chrono_tenth_cache);

  if (day_font != NULL) {
    fonts_unload_custom_font(day_font);
    day_font = NULL;
  }

  window_destroy(window);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}
