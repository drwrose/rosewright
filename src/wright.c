#include "wright.h"
#include "wright_chrono.h"
#include "hand_table.h"
#include "qapp_log.h"
#include <ctype.h>

#include "../resources/generated_table.c"
#include "../resources/lang_table.c"

bool memory_panic_flag = false;
int memory_panic_count = 0;
int draw_face_count = 0;
bool app_in_focus = true;

Window *window;

BitmapWithData clock_face;
int face_index = -1;
Layer *clock_face_layer;

BitmapWithData date_window;
BitmapWithData date_window_mask;
bool date_window_dynamic = false;
bool tick_seconds_subscribed = false;
int cached_sleep_time = -1;
int cached_sleep_restful_time = -1;
int cached_step_count = -1;
int cached_step_count_10 = -1;
int cached_active_time = -1;
int cached_walked_distance = -1;
int cached_calories_burned = -1;
int cached_heart_rate = -1;

BitmapWithData face_bitmap;
BitmapWithData top_subdial_frame_mask;
BitmapWithData top_subdial_mask;
BitmapWithData top_subdial_bitmap;
BitmapWithData moon_wheel_bitmap;

#ifndef PREBAKE_LABEL
BitmapWithData pebble_label;
#endif  // PREBAKE_LABEL

GFont fallback_font = NULL;
GFont date_numeric_font = NULL;
GFont date_lang_font = NULL;

bool save_framebuffer = true;
bool any_obstructed_area = false;

#ifndef NEVER_KEEP_ASSETS
bool keep_assets = true;
#endif  // NEVER_KEEP_ASSETS

#ifndef NEVER_KEEP_FACE_ASSET
bool keep_face_asset = true;
#endif  // NEVER_KEEP_FACE_ASSET

bool hide_date_windows = false;
bool hide_clock_face = false;
bool redraw_clock_face = false;

#define MIN_BYTES_FREE 512  // maybe this is enough?

#define DATE_WINDOW_BUFFER_SIZE 16

#define PEBBLE_LABEL_OFFSET_X ((SUBDIAL_SIZE_X - PEBBLE_LABEL_SIZE_X) / 2)
#define PEBBLE_LABEL_OFFSET_Y ((SUBDIAL_SIZE_Y - PEBBLE_LABEL_SIZE_Y) / 2)

// This structure is the data associated with a date window layer.
typedef struct __attribute__((__packed__)) {
  unsigned char date_window_index;
} DateWindowData;


// These structures are filled in from the appropriate resource file
// to reflect the names we are displaying in the weekday/month window
// based on configuration settings.

// The date_names array contains three sets of strings, in the order:
// all weekday names, then all month names, then all ampm names.
#define NUM_WEEKDAY_NAMES 7  // Enough for 7 days
#define NUM_MONTH_NAMES 12   // Enough for 12 months
#define NUM_AMPM_NAMES 2     // Enough for am and pm

#define NUM_DATE_NAMES (NUM_WEEKDAY_NAMES + NUM_MONTH_NAMES + NUM_AMPM_NAMES)
//#define DATE_NAMES_MAX_BUFFER xx // defined in lang_table.c.
char date_names_buffer[DATE_NAMES_MAX_BUFFER + 1];
char *date_names[NUM_DATE_NAMES];

int display_lang = -1;

#if ENABLE_SWEEP_SECONDS
// Triggered at regular intervals to implement sweep seconds.
AppTimer *sweep_timer = NULL;
int sweep_timer_ms = 1000;
#endif  // ENABLE_SWEEP_SECONDS

int sweep_seconds_ms = 60 * 1000 / NUM_STEPS_SECOND;

struct HandCache hour_cache;
struct HandCache minute_cache;
struct HandCache second_cache;

#ifdef SUPPORT_RESOURCE_CACHE
struct ResourceCache second_resource_cache[SECOND_RESOURCE_CACHE_SIZE + SECOND_MASK_RESOURCE_CACHE_SIZE];
size_t second_resource_cache_size = SECOND_RESOURCE_CACHE_SIZE + SECOND_MASK_RESOURCE_CACHE_SIZE;
#endif  // SUPPORT_RESOURCE_CACHE

struct HandPlacement current_placement;

#ifdef PBL_BW
// On B&W watches, we have to decide carefully what compositing mode
// to draw the hands.
DrawModeTable draw_mode_table[2] = {
  { GCompOpClear, GCompOpOr, GCompOpAssign, { GColorClearInit, GColorBlackInit, GColorWhiteInit } },
  { GCompOpOr, GCompOpClear, GCompOpAssignInverted, { GColorClearInit, GColorWhiteInit, GColorBlackInit } },
};

#else  // PBL_BW

// On color watches, we always use GCompOpSet to draw the hands,
// because we always use the alpha channel.  The exception is
// paint_assign, because assign means assign.
DrawModeTable draw_mode_table[2] = {
  { GCompOpSet, GCompOpSet, GCompOpAssign, { GColorClearInit, GColorBlackInit, GColorWhiteInit } },
  { GCompOpSet, GCompOpSet, GCompOpAssign, { GColorClearInit, GColorWhiteInit, GColorBlackInit } },
};

#endif  // PBL_BW

static const uint32_t tap_segments[] = { 50, 200, 75 };
VibePattern tap = {
  tap_segments,
  3,
};

void create_temporal_objects();
void destroy_temporal_objects();
void recreate_all_objects();
void draw_full_date_window(GContext *ctx, int date_window_index);
void draw_date_window_dynamic_text(GContext *ctx, int date_window_index);
void health_event_handler(HealthEventType event, void *context);

// Loads a font from the resource and returns it.  It may return
// either the intended font, or the fallback font.  If it returns
// the fallback font, this function automatically triggers a memory
// panic alert.
GFont safe_load_custom_font(int resource_id) {
  ResHandle resource = resource_get_handle(resource_id);
  qapp_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "loading font %d, heap_bytes_free = %d", resource_id, heap_bytes_free());

  if (fallback_font == NULL) {
    // Record the fallback font pointer so we can identify if this one
    // is accidentally returned from fonts_load_custom_font().
    fallback_font = fonts_get_system_font(FONT_KEY_FONT_FALLBACK);
  }

  GFont font = fonts_load_custom_font(resource);
  if (font == fallback_font) {
    qapp_log(APP_LOG_LEVEL_WARNING, __FILE__, __LINE__, "font %d failed to load", resource_id);
    //trigger_memory_panic(__LINE__);
    return font;
  }
  qapp_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "loaded font %d as %p, heap_bytes_free = %d", resource_id, font, heap_bytes_free());
  return font;
}

// Unloads a font pointer returned by fonts_load_custom_font()
// without crashing.
void safe_unload_custom_font(GFont *font) {
  // (Since the font pointer returned by fonts_load_custom_font()
  // might actually be a pointer to the fallback font instead of to an
  // actual custom font, and since fonts_unload_custom_font() will
  // *crash* if we try to pass in the fallback font, we have to detect
  // that case and avoid it.)
  qapp_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "unloaded font %p", *font);
  if ((*font) != fallback_font) {
    fonts_unload_custom_font(*font);
  }
  (*font) = NULL;
}

// Initialize a HandCache structure.
void hand_cache_init(struct HandCache *hand_cache) {
  memset(hand_cache, 0, sizeof(struct HandCache));
}

// Release any memory held within a HandCache structure.
void hand_cache_destroy(struct HandCache *hand_cache) {
  bwd_destroy(&hand_cache->image);
  bwd_destroy(&hand_cache->mask);
  int gi;
  for (gi = 0; gi < HAND_CACHE_MAX_GROUPS; ++gi) {
    if (hand_cache->path[gi] != NULL) {
      gpath_destroy(hand_cache->path[gi]);
      hand_cache->path[gi] = NULL;
    }
  }
}

time_t
make_gmt_date(int mday, int mon, int year) {
  struct tm t;
  memset(&t, 0, sizeof(t));
  t.tm_mday = mday;
  t.tm_mon = mon;
  t.tm_year = year;
  return mktime(&t);
}

// Determines the specific hand bitmaps that should be displayed based
// on the current time.
void compute_hands(struct tm *stime, struct HandPlacement *placement) {
  // Check whether we need to compute sub-second precision.
#if defined(MAKE_CHRONOGRAPH)
  #define needs_sub_second true
#elif !ENABLE_SWEEP_SECONDS
  #define needs_sub_second false
#else
  bool needs_sub_second = config.sweep_seconds;
#endif

  time_t gmt;
  uint16_t t_ms = 0;
  unsigned int ms;

  if (needs_sub_second || stime == NULL) {
    // If we do need sub-second precision, it replaces the stime
    // structure we were passed in.

    // Get the Unix time (in UTC).
    time_ms(&gmt, &t_ms);

    // Compute the number of milliseconds elapsed since midnight, local time.
    struct tm *tm = localtime(&gmt);
    unsigned int s = (unsigned int)(tm->tm_hour * 60 + tm->tm_min) * 60 + tm->tm_sec;
    ms = (unsigned int)(s * 1000 + t_ms);

  } else {
    // If we don't need sub-second precision, just use the existing
    // stime structure.  We still need UTC time, though, for the lunar
    // phase at least.
    assert(stime != NULL);
    gmt = time(NULL);
    unsigned int s = (unsigned int)(stime->tm_hour * 60 + stime->tm_min) * 60 + stime->tm_sec;
    ms = (unsigned int)(s * 1000);
  }

#ifdef MAKE_CHRONOGRAPH
  // For the chronograph, compute the number of milliseconds elapsed
  // since midnight, UTC.
  unsigned int ms_utc = (unsigned int)((gmt % SECONDS_PER_DAY) * 1000 + t_ms);
#endif  // MAKE_CHRONOGRAPH

#ifdef FAST_TIME
  {
    if (stime != NULL) {
      int s = ms / 1000;
      int yday = s % 365;

      gmt = make_gmt_date(yday, 0, stime->tm_year);
      (*stime) = *gmtime(&gmt);
    }
    ms *= 67;
#ifdef MAKE_CHRONOGRAPH
    ms_utc *= 67;
#endif  // MAKE_CHRONOGRAPH
  }
#endif  // FAST_TIME

#ifdef SCREENSHOT_BUILD
  // Freeze the time to 10:09 for screenshots.
  {
    ms = ((10*60 + 9)*60 + 36) * 1000;  // 10:09:36
    gmt = make_gmt_date(9, 6, 114);     // 9-Jul-2014
    //gmt = make_gmt_date(31, 11, 115);   // 31-Dec-2015
    //gmt = make_gmt_date(1, 0, 116);     // 1-Jan-2016
    gmt += ms / 1000;
    if (stime != NULL) {
      (*stime) = *gmtime(&gmt);
    }

    int moon_phase = 3;  // gibbous moon
    gmt = (2551443 * moon_phase + 159465) / 8 + 1401302400;

#ifdef MAKE_CHRONOGRAPH
    chrono_data.running = false;
    ms = ((10*60 + 9)*60 + 24) * 1000;  // 10:09:24
    chrono_data.hold_ms = (((1*60 + 36) * 60) + 36) * 1000 + 900;  // 1:36:36.9
#endif  // MAKE_CHRONOGRAPH
  }
#endif  // SCREENSHOT_BUILD

  {
    // Avoid overflowing the integer arithmetic by pre-constraining
    // the ms value to the appropriate range.
    unsigned int use_ms = ms % (SECONDS_PER_HOUR * 12 * 1000);
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

  // Record data for date windows.
  if (stime != NULL) {
    placement->day_index = stime->tm_wday;
    placement->month_index = stime->tm_mon;
    placement->date_value = stime->tm_mday;
    placement->year_value = stime->tm_year;
    placement->ampm_value = (stime->tm_hour >= 12);
    placement->ordinal_date_index = stime->tm_yday;

    {
      // Easy lunar phase calculation: the moon's synodic period,
      // meaning the average number of days that pass in a lunar
      // cycle, is 29.5305882 days.  Thus, we only have to take the
      // number of days elapsed since a known new moon, modulo
      // 29.5305882, to know the phase of the moon.

      // There was a new moon on 18:40 May 28 2014 UTC; we pick this
      // date arbitrarily as the new moon reference date since it was
      // the most recent new moon at the time I wrote this code (if a
      // Pebble user sets their watch before this time, the lunar
      // phase will be wrong--no big worries).  This date expressed in
      // Unix time is the value 1401302400.
      unsigned int lunar_offset_s = (unsigned int)(gmt - 1401302400);

      // Now we have the number of seconds elapsed since a known new
      // moon.  To compute modulo 29.5305882 days using integer
      // arithmetic, we actually compute modulo 2551443 seconds.
      // (This integer computation is a bit less precise than the full
      // decimal value--by 2114 it have drifted off by about 2 hours.
      // Close enough.)
      unsigned int lunar_age_s = lunar_offset_s % 2551443;

      // That gives the age of the moon in seconds.

#ifdef TOP_SUBDIAL
      // Compute the index of the moon wheel for the moon_phase
      // variant of the top subdial.  This ranges from 0 to
      // NUM_STEPS_MOON - 1 over the lunar month.
      unsigned int lunar_index = (NUM_STEPS_MOON * lunar_age_s) / 2551443;
      placement->lunar_index = lunar_index % NUM_STEPS_MOON;
#endif  // TOP_SUBDIAL
    }
  }

  placement->buzzed_hour = (ms / (SECONDS_PER_HOUR * 1000)) % 24;

#ifdef MAKE_CHRONOGRAPH
  compute_chrono_hands(ms_utc, placement);
#endif  // MAKE_CHRONOGRAPH
}


// Reverse the bits of a byte.
// http://www-graphics.stanford.edu/~seander/bithacks.html#BitReverseTable
uint8_t reverse_bits(uint8_t b) {
  return ((b * 0x0802LU & 0x22110LU) | (b * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16;
}

// Reverse the four two-bit components of a byte.
uint8_t reverse_2bits(uint8_t b) {
  return ((b & 0x3) << 6) | ((b & 0xc) << 2) | ((b & 0x30) >> 2) | ((b & 0xc0) >> 6);
}

// Reverse the high nibble and low nibble of a byte.
uint8_t reverse_nibbles(uint8_t b) {
  return ((b & 0xf) << 4) | ((b >> 4) & 0xf);
}

int get_pixels_per_byte(GBitmap *image) {
  int pixels_per_byte = 8;

#ifndef PBL_BW
  switch (gbitmap_get_format(image)) {
  case GBitmapFormat1Bit:
  case GBitmapFormat1BitPalette:
    pixels_per_byte = 8;
    break;

  case GBitmapFormat2BitPalette:
    pixels_per_byte = 4;
    break;

  case GBitmapFormat4BitPalette:
    pixels_per_byte = 2;
    break;

  case GBitmapFormat8Bit:
  case GBitmapFormat8BitCircular:
    pixels_per_byte = 1;
    break;
  }
#endif  // PBL_BW

  return pixels_per_byte;
}

// Horizontally flips the indicated GBitmap in-place.  Requires
// that the width be a multiple of 8 pixels.
void flip_bitmap_x(GBitmap *image, short *cx) {
  if (image == NULL) {
    // Trivial no-op.
    return;
  }

  int height = gbitmap_get_bounds(image).size.h;
  int width = gbitmap_get_bounds(image).size.w;
  int pixels_per_byte = get_pixels_per_byte(image);

  assert(width % pixels_per_byte == 0);  // This must be an even divisor, by our convention.
  int width_bytes = width / pixels_per_byte;
  int stride = gbitmap_get_bytes_per_row(image);
  assert(stride >= width_bytes);

  qapp_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "flip_bitmap_x, width_bytes = %d, stride=%d", width_bytes, stride);

  uint8_t *data = gbitmap_get_data(image);

  for (int y = 0; y < height; ++y) {
    // Get the min and max x values for this row
    GBitmapDataRowInfo info = gbitmap_get_data_row_info(image, y);
    uint8_t *row = &info.data[info.min_x];
    width = info.max_x - info.min_x + 1;
    assert(width % pixels_per_byte == 0);
    int width_bytes = width / pixels_per_byte;

    switch (pixels_per_byte) {
    case 8:
      for (int x1 = (width_bytes - 1) / 2; x1 >= 0; --x1) {
        int x2 = width_bytes - 1 - x1;
        uint8_t b = reverse_bits(row[x1]);
        row[x1] = reverse_bits(row[x2]);
        row[x2] = b;
      }
      break;

#ifndef PBL_BW
    case 4:
      for (int x1 = (width_bytes - 1) / 2; x1 >= 0; --x1) {
        int x2 = width_bytes - 1 - x1;
        uint8_t b = reverse_2bits(row[x1]);
        row[x1] = reverse_2bits(row[x2]);
        row[x2] = b;
      }
      break;

    case 2:
      for (int x1 = (width_bytes - 1) / 2; x1 >= 0; --x1) {
        int x2 = width_bytes - 1 - x1;
        uint8_t b = reverse_nibbles(row[x1]);
        row[x1] = reverse_nibbles(row[x2]);
        row[x2] = b;
      }
      break;

    case 1:
      for (int x1 = (width_bytes - 1) / 2; x1 >= 0; --x1) {
        int x2 = width_bytes - 1 - x1;
        uint8_t b = row[x1];
        row[x1] = row[x2];
        row[x2] = b;
      }
      break;
#endif  // PBL_BW
    }
  }

  if (cx != NULL) {
    *cx = width- 1 - *cx;
  }
}

// Vertically flips the indicated GBitmap in-place.
void flip_bitmap_y(GBitmap *image, short *cy) {
  int height = gbitmap_get_bounds(image).size.h;
  int width = gbitmap_get_bounds(image).size.w;
  int pixels_per_byte = get_pixels_per_byte(image);
  int stride = gbitmap_get_bytes_per_row(image); // multiple of 4.
  int width_bytes = width / pixels_per_byte;
  uint8_t *data = gbitmap_get_data(image);

  uint8_t buffer[width_bytes]; // gcc lets us do this.
  for (int y1 = (height - 1) / 2; y1 >= 0; --y1) {
    int y2 = height - 1 - y1;

    // Get the min and max x values for this row
    GBitmapDataRowInfo info1 = gbitmap_get_data_row_info(image, y1);
    GBitmapDataRowInfo info2 = gbitmap_get_data_row_info(image, y2);
    int width1 = info1.max_x - info1.min_x + 1;
    int width2 = info2.max_x - info2.min_x + 1;
    assert(width1 == width2);  // We hope the bitmap is vertically symmetric
    uint8_t *row1 = &info1.data[info1.min_x];
    uint8_t *row2 = &info2.data[info2.min_x];
    width_bytes = width1 / pixels_per_byte;

    // Swap rows y1 and y2.
    memcpy(buffer, row1, width_bytes);
    memcpy(row1, row2, width_bytes);
    memcpy(row2, buffer, width_bytes);
  }

  if (cy != NULL) {
    *cy = height - 1 - *cy;
  }
}

// Draws a given hand on the face, using the vector structures.
void draw_vector_hand(struct HandCache *hand_cache, struct HandDef *hand_def, int hand_index, GContext *ctx) {
  struct VectorHand *vector_hand = hand_def->vector_hand;

  int gi;
  if (hand_cache->vector_hand_index != hand_index) {
    // Force a new path.
    for (gi = 0; gi < vector_hand->num_groups; ++gi) {
      if (hand_cache->path[gi] != NULL) {
        gpath_destroy(hand_cache->path[gi]);
        hand_cache->path[gi] = NULL;
      }
    }
    hand_cache->vector_hand_index = hand_index;
  }

  GPoint center = { hand_def->place_x, hand_def->place_y };
  int32_t angle = TRIG_MAX_ANGLE * hand_index / hand_def->num_steps;

#ifdef PBL_BW
  GColor color = draw_mode_table[config.draw_mode ^ BW_INVERT].colors[2];

#else  // PBL_BW
  // On color watches, draw lines using the indicated color channel.
  struct FaceColorDef *cd = &clock_face_color_table[config.color_mode];
  GColor color;
  switch (vector_hand->paint_channel) {
  case 0:
  default:
    color.argb = cd->cb_argb8;
    break;
  case 1:
    color.argb = cd->c1_argb8;
    break;
  case 2:
    color.argb = cd->c2_argb8;
    break;
  case 3:
    color.argb = cd->c3_argb8;
    break;
  }
  if (config.draw_mode) {
    color.argb ^= 0x3f;
  }
#endif  // PBL_BW
  graphics_context_set_stroke_color(ctx, color);

  assert(vector_hand->num_groups <= HAND_CACHE_MAX_GROUPS);
  for (gi = 0; gi < vector_hand->num_groups; ++gi) {
    struct VectorHandGroup *group = &vector_hand->group[gi];

    if (hand_cache->path[gi] == NULL) {
      hand_cache->path[gi] = gpath_create(&group->path_info);
      if (hand_cache->path[gi] == NULL) {
        trigger_memory_panic(__LINE__);
        return;
      }

      gpath_rotate_to(hand_cache->path[gi], angle);
      gpath_move_to(hand_cache->path[gi], center);
    }
    gpath_draw_outline(ctx, hand_cache->path[gi]);
  }
}

// Clears the mask given hand on the face, using the bitmap
// structures, if the mask is in use.  This must be called before
// draw_bitmap_hand_fg().
void draw_bitmap_hand_mask(struct HandCache *hand_cache RESOURCE_CACHE_FORMAL_PARAMS, struct HandDef *hand_def, int hand_index, bool no_basalt_mask, GContext *ctx) {
  struct BitmapHandTableRow *hand = &hand_def->bitmap_table[hand_index];
  int bitmap_index = hand->bitmap_index;
  struct BitmapHandCenterRow *lookup = &hand_def->bitmap_centers[bitmap_index];

  int hand_resource_id = hand_def->resource_id + bitmap_index;
  int hand_resource_mask_id = hand_def->resource_mask_id + bitmap_index;

#ifdef PBL_BW
  if (hand_def->resource_id == hand_def->resource_mask_id)
#else
  if (no_basalt_mask || hand_def->resource_id == hand_def->resource_mask_id)
#endif  // PBL_BW
  {
    // The draw-without-a-mask case.  Do nothing here.
  } else {
    // The hand has a mask, so use it to draw the hand opaquely.
    if (hand_cache->image.bitmap == NULL) {
      if (hand_def->use_rle) {
        hand_cache->image = rle_bwd_create_with_cache(hand_def->resource_id, hand_resource_id RESOURCE_CACHE_PARAMS(resource_cache, resource_cache_size));
        hand_cache->mask = rle_bwd_create_with_cache(hand_def->resource_id, hand_resource_mask_id RESOURCE_CACHE_PARAMS(resource_cache, resource_cache_size));
      } else {
        hand_cache->image = png_bwd_create_with_cache(hand_def->resource_id, hand_resource_id RESOURCE_CACHE_PARAMS(resource_cache, resource_cache_size));
        hand_cache->mask = png_bwd_create_with_cache(hand_def->resource_id, hand_resource_mask_id RESOURCE_CACHE_PARAMS(resource_cache, resource_cache_size));
      }
      if (hand_cache->image.bitmap == NULL || hand_cache->mask.bitmap == NULL) {
        hand_cache_destroy(hand_cache);
        trigger_memory_panic(__LINE__);
        return;
      }
      remap_colors_clock(&hand_cache->image);
      remap_colors_clock(&hand_cache->mask);

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

    GRect destination = gbitmap_get_bounds(hand_cache->image.bitmap);
    destination.origin.x = hand_def->place_x - hand_cache->cx;
    destination.origin.y = hand_def->place_y - hand_cache->cy;

    graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode ^ BW_INVERT].paint_fg);
    graphics_draw_bitmap_in_rect(ctx, hand_cache->mask.bitmap, destination);
  }
}

// Draws a given hand on the face, using the bitmap structures.  You
// must have already called draw_bitmap_hand_mask().
void draw_bitmap_hand_fg(struct HandCache *hand_cache RESOURCE_CACHE_FORMAL_PARAMS, struct HandDef *hand_def, int hand_index, bool no_basalt_mask, GContext *ctx) {
  struct BitmapHandTableRow *hand = &hand_def->bitmap_table[hand_index];
  int bitmap_index = hand->bitmap_index;
  struct BitmapHandCenterRow *lookup = &hand_def->bitmap_centers[bitmap_index];

  int hand_resource_id = hand_def->resource_id + bitmap_index;

#ifdef PBL_BW
  if (hand_def->resource_id == hand_def->resource_mask_id)
#else
  if (no_basalt_mask || hand_def->resource_id == hand_def->resource_mask_id)
#endif  // PBL_BW
  {
    // The hand does not have a mask.  Draw the hand on top of the scene.
    if (hand_cache->image.bitmap == NULL) {
      // All right, load it from the resource file.
      if (hand_def->use_rle) {
        hand_cache->image = rle_bwd_create_with_cache(hand_def->resource_id, hand_resource_id RESOURCE_CACHE_PARAMS(resource_cache, resource_cache_size));
      } else {
        hand_cache->image = png_bwd_create_with_cache(hand_def->resource_id, hand_resource_id RESOURCE_CACHE_PARAMS(resource_cache, resource_cache_size));
      }
      if (hand_cache->image.bitmap == NULL) {
        hand_cache_destroy(hand_cache);
        trigger_memory_panic(__LINE__);
        return;
      }
      remap_colors_clock(&hand_cache->image);

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
    GRect destination = gbitmap_get_bounds(hand_cache->image.bitmap);

    // Place the hand's center point at place_x, place_y.
    destination.origin.x = hand_def->place_x - hand_cache->cx;
    destination.origin.y = hand_def->place_y - hand_cache->cy;

    // Specify a compositing mode to make the hands overlay on top of
    // each other, instead of the background parts of the bitmaps
    // blocking each other.

    // Painting foreground ("white") pixels as white.
    graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode ^ BW_INVERT].paint_bg);
    graphics_draw_bitmap_in_rect(ctx, hand_cache->image.bitmap, destination);

  } else {
    // The hand has a mask, so use it to draw the hand opaquely.
    if (hand_cache->image.bitmap == NULL) {
      // We have already loaded the image in draw_bitmap_hand_mask(),
      // so if it's NULL now then something's gone wrong (e.g. memory
      // panic).
      return;
    }

    GRect destination = gbitmap_get_bounds(hand_cache->image.bitmap);
    destination.origin.x = hand_def->place_x - hand_cache->cx;
    destination.origin.y = hand_def->place_y - hand_cache->cy;

    graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode ^ BW_INVERT].paint_bg);
    graphics_draw_bitmap_in_rect(ctx, hand_cache->image.bitmap, destination);
  }
}

// In general, prepares a hand for being drawn.  Specifically, this
// clears the background behind a hand, if necessary.
void draw_hand_mask(struct HandCache *hand_cache RESOURCE_CACHE_FORMAL_PARAMS, struct HandDef *hand_def, int hand_index, bool no_basalt_mask, GContext *ctx) {
  if (hand_def->bitmap_table != NULL) {
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

    draw_bitmap_hand_mask(hand_cache RESOURCE_CACHE_PARAMS(resource_cache, resource_cache_size), hand_def, hand_index, no_basalt_mask, ctx);
  }
}

// Draws a given hand on the face, after draw_hand_mask(), using the
// vector and/or bitmap structures.  A given hand may be represented
// by a bitmap or a vector, or a combination of both.
void draw_hand_fg(struct HandCache *hand_cache RESOURCE_CACHE_FORMAL_PARAMS, struct HandDef *hand_def, int hand_index, bool no_basalt_mask, GContext *ctx) {
  if (hand_def->vector_hand != NULL) {
    draw_vector_hand(hand_cache, hand_def, hand_index, ctx);
  }

  if (hand_def->bitmap_table != NULL) {
    draw_bitmap_hand_fg(hand_cache RESOURCE_CACHE_PARAMS(resource_cache, resource_cache_size), hand_def, hand_index, no_basalt_mask, ctx);
  }
}

void draw_hand(struct HandCache *hand_cache RESOURCE_CACHE_FORMAL_PARAMS, struct HandDef *hand_def, int hand_index, GContext *ctx) {
  draw_hand_mask(hand_cache RESOURCE_CACHE_PARAMS(resource_cache, resource_cache_size), hand_def, hand_index, true, ctx);
  draw_hand_fg(hand_cache RESOURCE_CACHE_PARAMS(resource_cache, resource_cache_size), hand_def, hand_index, true, ctx);
}

// Applies the appropriate color-remapping according to the selected
// color mode, for the indicated clock-face or clock-hands bitmap.
void remap_colors_clock(BitmapWithData *bwd) {
#ifndef PBL_BW
  struct FaceColorDef *cd = &clock_face_color_table[config.color_mode];
  GColor cb, c1, c2, c3;
  cb.argb = cd->cb_argb8;
  c1.argb = cd->c1_argb8;
  c2.argb = cd->c2_argb8;
  c3.argb = cd->c3_argb8;

  bwd_remap_colors(bwd, cb, c1, c2, c3, config.draw_mode);
#endif  // PBL_BW
}

// Applies the appropriate color-remapping according to the selected
// color mode, for the indicated date-window bitmap.
void remap_colors_date(BitmapWithData *bwd) {
#ifndef PBL_BW
  struct FaceColorDef *cd = &clock_face_color_table[config.color_mode];
  GColor db, d1;
  db.argb = cd->db_argb8;
  d1.argb = cd->d1_argb8;

  // I've experimented with using the blue channel for something
  // interesting, but couldn't really make it work.  So now only the
  // red channel is used in the date window (and battery gauge).
  bwd_remap_colors(bwd, db, d1, db, db, config.draw_mode);
#endif  // PBL_BW
}

// Applies the appropriate color-remapping according to the selected
// color mode, for the indicated lunar bitmap.
static void remap_colors_moon(BitmapWithData *bwd) {
#ifndef PBL_BW
  struct FaceColorDef *cd = &clock_face_color_table[config.color_mode];
  GColor bg, fg;
  bg.argb = cd->db_argb8;
  fg.argb = cd->d1_argb8;
  if (config.lunar_background) {
    // If the user specified an always-dark background, honor that.
    fg = GColorYellow;
    bg = GColorOxfordBlue;
  } else if (config.draw_mode) {
    // Inverting colors really means only to invert the two watchface colors.
    fg.argb ^= 0x3f;
    bg.argb ^= 0x3f;
  }

  bwd_remap_colors(bwd, bg, fg, GColorBlack, GColorPastelYellow, false);
#endif  // PBL_BW
}

#ifndef PREBAKE_LABEL
void draw_pebble_label(Layer *me, GContext *ctx, bool invert) {
  unsigned int draw_mode = invert ^ config.draw_mode ^ BW_INVERT;

  const struct IndicatorTable *window = &top_subdial[config.face_index];
  GRect destination = GRect(window->x + PEBBLE_LABEL_OFFSET_X, window->y + PEBBLE_LABEL_OFFSET_Y, PEBBLE_LABEL_SIZE_X, PEBBLE_LABEL_SIZE_Y);

#ifdef PBL_BW
  BitmapWithData pebble_label_mask;
  pebble_label_mask = rle_bwd_create(RESOURCE_ID_PEBBLE_LABEL_MASK);
  if (pebble_label_mask.bitmap == NULL) {
    trigger_memory_panic(__LINE__);
    return;
  }
  graphics_context_set_compositing_mode(ctx, draw_mode_table[draw_mode].paint_bg);
  graphics_draw_bitmap_in_rect(ctx, pebble_label_mask.bitmap, destination);
  bwd_destroy(&pebble_label_mask);
#endif  // PBL_BW

  if (pebble_label.bitmap == NULL) {
    pebble_label = rle_bwd_create(RESOURCE_ID_PEBBLE_LABEL);
    if (pebble_label.bitmap == NULL) {
      trigger_memory_panic(__LINE__);
      return;
    }
#ifndef PBL_BW
    remap_colors_clock(&pebble_label);
#endif  // PBL_BW
  }

  graphics_context_set_compositing_mode(ctx, draw_mode_table[draw_mode].paint_fg);
  graphics_draw_bitmap_in_rect(ctx, pebble_label.bitmap, destination);
  if (!keep_assets) {
    bwd_destroy(&pebble_label);
  }
}
#endif  // PREBAKE_LABEL

#ifdef TOP_SUBDIAL
// Draws a special moon subdial window that shows the lunar phase in more detail.
void draw_moon_phase_subdial(Layer *me, GContext *ctx, bool invert) {
  // The draw_mode is the color to draw the frame of the subdial.
  unsigned int draw_mode = invert ^ config.draw_mode ^ BW_INVERT;

  // The moon_draw_mode is the color to draw the moon within the subdial.
  unsigned int moon_draw_mode = draw_mode;
  if (config.lunar_background) {
    // If the user specified an always-black background, that means moon_draw_mode is always 1.
    moon_draw_mode = 1;
  }

  const struct IndicatorTable *window = &top_subdial[config.face_index];
  GRect destination = GRect(window->x, window->y, SUBDIAL_SIZE_X, SUBDIAL_SIZE_Y);

  // First draw the subdial details (including the background).
#ifdef PBL_BW
  if (top_subdial_frame_mask.bitmap == NULL) {
    top_subdial_frame_mask = rle_bwd_create(RESOURCE_ID_TOP_SUBDIAL_FRAME_MASK);
    if (top_subdial_frame_mask.bitmap == NULL) {
      trigger_memory_panic(__LINE__);
      return;
    }
  }
  graphics_context_set_compositing_mode(ctx, draw_mode_table[draw_mode].paint_bg);
  graphics_draw_bitmap_in_rect(ctx, top_subdial_frame_mask.bitmap, destination);
  if (!keep_assets) {
    bwd_destroy(&top_subdial_frame_mask);
  }

  if (top_subdial_mask.bitmap == NULL) {
    top_subdial_mask = rle_bwd_create(RESOURCE_ID_TOP_SUBDIAL_MASK);
    if (top_subdial_mask.bitmap == NULL) {
      trigger_memory_panic(__LINE__);
      return;
    }
  }
  graphics_context_set_compositing_mode(ctx, draw_mode_table[moon_draw_mode].paint_bg);
  graphics_draw_bitmap_in_rect(ctx, top_subdial_mask.bitmap, destination);
  if (!keep_assets) {
    bwd_destroy(&top_subdial_mask);
  }
#endif  // PBL_BW

  if (top_subdial_bitmap.bitmap == NULL) {
    top_subdial_bitmap = rle_bwd_create(RESOURCE_ID_TOP_SUBDIAL);
    if (top_subdial_bitmap.bitmap == NULL) {
      trigger_memory_panic(__LINE__);
      return;
    }
#ifndef PBL_BW
    remap_colors_clock(&top_subdial_bitmap);
#endif  // PBL_BW
  }

  graphics_context_set_compositing_mode(ctx, draw_mode_table[draw_mode].paint_fg);
  graphics_draw_bitmap_in_rect(ctx, top_subdial_bitmap.bitmap, destination);
  if (!keep_assets) {
    bwd_destroy(&top_subdial_bitmap);
  }

  // Now draw the moon wheel.

  int index = current_placement.lunar_index;
  assert(index < NUM_STEPS_MOON);

  if (config.lunar_direction) {
    // Draw the moon phases animating from left-to-right, as seen in
    // the southern hemisphere.  This means we spin the moon wheel
    // counter-clockwise instead of clockwise.  Strictly, we should
    // also invert the visual representation of the moon, but that
    // would mean a duplicated set of bitmaps, so (at least for now)
    // we don't bother.
    index = NUM_STEPS_MOON - 1 - index;
  }

  if (moon_wheel_bitmap.bitmap == NULL) {
#ifdef PBL_BW
    // On B&W watches, we load either "black" or "white" icons,
    // according to what color we need the background to be.
    if (moon_draw_mode == 0) {
      moon_wheel_bitmap = rle_bwd_create(RESOURCE_ID_MOON_WHEEL_WHITE_0 + index);
    } else {
      moon_wheel_bitmap = rle_bwd_create(RESOURCE_ID_MOON_WHEEL_BLACK_0 + index);
    }
#else  // PBL_BW
    // On color watches, we only use the "black" icons, and we remap
    // the colors at load time.
    moon_wheel_bitmap = rle_bwd_create(RESOURCE_ID_MOON_WHEEL_BLACK_0 + index);
    remap_colors_moon(&moon_wheel_bitmap);
#endif  // PBL_BW
    if (moon_wheel_bitmap.bitmap == NULL) {
      trigger_memory_panic(__LINE__);
      return;
    }
  }

  // In the B&W case, we draw the moon in the fg color.  This will
  // be black-on-white if moon_draw_mode = 0, or white-on-black if
  // moon_draw_mode = 1.  Since we have selected the particular moon
  // resource above based on draw_mode, we will always draw the moon
  // in the correct color, so that it looks like the moon.  (Drawing
  // the moon in the inverted color would look weird.)

  // In the color case, the only difference between moon_black and
  // moon_white is the background color; in either case we draw them
  // both in GCompOpSet.
  graphics_context_set_compositing_mode(ctx, draw_mode_table[moon_draw_mode].paint_fg);
  graphics_draw_bitmap_in_rect(ctx, moon_wheel_bitmap.bitmap, destination);
  if (!keep_assets) {
    bwd_destroy(&moon_wheel_bitmap);
  }
}
#endif  // TOP_SUBDIAL

int get_indicator_face_index() {
#ifdef TOP_SUBDIAL
  int indicator_face_index = config.face_index * 2 + (config.top_subdial > TSM_pebble_label);
#else  // TOP_SUBDIAL
  int indicator_face_index = config.face_index;
#endif  // TOP_SUBDIAL
  assert(indicator_face_index < NUM_INDICATOR_FACES);
  return indicator_face_index;
}

void draw_clock_face(Layer *me, GContext *ctx) {
  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "draw_clock_face");
  ++draw_face_count;

  // Reload the face bitmap from the resource file, if we don't
  // already have it.
  if (face_bitmap.bitmap == NULL) {
    int resource_id = clock_face_table[config.face_index].resource_id;
#ifdef PREBAKE_LABEL
    // If the pebble_label is enabled (and we've prebaked the labels
    // into the faces), we load the next consecutive resource instead.
    resource_id += (config.top_subdial == TSM_pebble_label);
#endif  // PREBAKE_LABEL

    face_bitmap = rle_bwd_create(resource_id);
    if (face_bitmap.bitmap == NULL) {
      trigger_memory_panic(__LINE__);
      return;
    }
    remap_colors_clock(&face_bitmap);
  }

  // Draw the clock face into the layer.
  GRect destination = layer_get_bounds(me);
  destination.origin.x = 0;
  destination.origin.y = 0;
  graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode ^ BW_INVERT].paint_assign);
  graphics_draw_bitmap_in_rect(ctx, face_bitmap.bitmap, destination);

  // Draw the top subdial if enabled.
  {
    const struct IndicatorTable *window = &top_subdial[config.face_index];

    switch (config.top_subdial) {
    case TSM_off:
      break;

    case TSM_pebble_label:
#ifndef PREBAKE_LABEL
      draw_pebble_label(me, ctx, window->invert);
#endif  // PREBAKE_LABEL
      break;

    case TSM_moon_phase:
      draw_moon_phase_subdial(me, ctx, window->invert);
      break;
    }
  }

  // Draw the date windows.
  {
    date_window_dynamic = false;
    for (int i = 0; i < NUM_DATE_WINDOWS; ++i) {
      draw_full_date_window(ctx, i);
    }
    if (!keep_assets) {
      bwd_destroy(&date_window);
      bwd_destroy(&date_window_mask);
    }
  }

#ifdef ENABLE_CHRONO_DIAL
  draw_chrono_dial(ctx);
#endif  // ENABLE_CHRONO_DIAL

  int indicator_face_index = get_indicator_face_index();
  {
    const struct IndicatorTable *window = &battery_table[indicator_face_index];
    draw_battery_gauge(ctx, window->x, window->y, window->invert);
  }
  {
    const struct IndicatorTable *window = &bluetooth_table[indicator_face_index];
    draw_bluetooth_indicator(ctx, window->x, window->y, window->invert);
  }
}

// Draws the hands that aren't the second hand--the hands that update
// once a minute or slower, and which may potentially be cached along
// with the clock face background.
void draw_phase_1_hands(GContext *ctx) {
#ifdef MAKE_CHRONOGRAPH

  // A special case for Chronograph support.  All six hands are drawn
  // individually, in a specific order (that's slightly different from
  // the non-chrono order--we draw the three subdials first, and this
  // includes the normal second hand).
  if (config.second_hand || chrono_data.running || chrono_data.hold_ms != 0) {
    draw_hand(&chrono_minute_cache RESOURCE_CACHE_PARAMS(NULL, 0), &chrono_minute_hand_def, current_placement.chrono_minute_hand_index, ctx);
  }

  if (config.chrono_dial != CDM_off) {
    if (config.second_hand || chrono_data.running || chrono_data.hold_ms != 0) {
      draw_hand(&chrono_tenth_cache RESOURCE_CACHE_PARAMS(NULL, 0), &chrono_tenth_hand_def, current_placement.chrono_tenth_hand_index, ctx);
    }
  }

  // Since the second hand is a tiny subdial in the Chrono case, and
  // is overlaid by the hour and minute hands, we have to draw the
  // second hand and everything else in phase_2.

#elif defined(ENABLE_CHRONO_DIAL)
  // In this case, we're not implementing full chrono functionality,
  // but we want to look like a chonograph, so draw the minute and
  // tenth hands at 0.
  if (config.second_hand) {
    draw_hand(&chrono_minute_cache RESOURCE_CACHE_PARAMS(NULL, 0), &chrono_minute_hand_def, 0, ctx);
  }

  if (config.chrono_dial != CDM_off) {
    if (config.second_hand) {
      draw_hand(&chrono_tenth_cache RESOURCE_CACHE_PARAMS(NULL, 0), &chrono_tenth_hand_def, 0, ctx);
    }
  }

#else  // MAKE_CHRONOGRAPH
  // The normal, non-chrono implementation, with only hour, minute,
  // and second hands; and we assume the second hand is a sweep-second
  // hand that covers the entire face and must be drawn last (rather
  // than a subdial second hand that must be drawn first).

#ifdef HOUR_MINUTE_OVERLAP
  // Draw the hour and minute hands overlapping, so they share a
  // common mask.  Rosewright A and B share this property, because
  // their hands are relatively thin, and their hand masks define an
  // invisible halo that erases to the background color around the
  // hands, but we don't want the hands to erase each other.
  draw_hand_mask(&hour_cache RESOURCE_CACHE_PARAMS(NULL, 0), &hour_hand_def, current_placement.hour_hand_index, false, ctx);
  draw_hand_mask(&minute_cache RESOURCE_CACHE_PARAMS(NULL, 0), &minute_hand_def, current_placement.minute_hand_index, false, ctx);

  draw_hand_fg(&hour_cache RESOURCE_CACHE_PARAMS(NULL, 0), &hour_hand_def, current_placement.hour_hand_index, false, ctx);
  draw_hand_fg(&minute_cache RESOURCE_CACHE_PARAMS(NULL, 0), &minute_hand_def, current_placement.minute_hand_index, false, ctx);

#else  //  HOUR_MINUTE_OVERLAP

  // Draw the hour and minute hands separately.  All of the other
  // Rosewrights share this property, because their hands are wide,
  // with complex interiors that must be erased; and their haloes (if
  // present) are comparatively thinner and don't threaten to erase
  // overlapping hands.
  draw_hand(&hour_cache RESOURCE_CACHE_PARAMS(NULL, 0), &hour_hand_def, current_placement.hour_hand_index, ctx);

  draw_hand(&minute_cache RESOURCE_CACHE_PARAMS(NULL, 0), &minute_hand_def, current_placement.minute_hand_index, ctx);
#endif  //  HOUR_MINUTE_OVERLAP

#endif  // MAKE_CHRONOGRAPH
}

// Draws the second hand or any other hands which must be redrawn once
// per second.  These are the hands that are never cached.
void draw_phase_2_hands(GContext *ctx) {
#ifdef MAKE_CHRONOGRAPH

  // The Chrono case.  Lots of hands end up here because it's the
  // second hand and everything that might overlay it.
  if (config.second_hand) {
    draw_hand(&second_cache RESOURCE_CACHE_PARAMS(second_resource_cache, second_resource_cache_size), &second_hand_def, current_placement.second_hand_index, ctx);
  }

  draw_hand(&hour_cache RESOURCE_CACHE_PARAMS(NULL, 0), &hour_hand_def, current_placement.hour_hand_index, ctx);

  draw_hand(&minute_cache RESOURCE_CACHE_PARAMS(NULL, 0), &minute_hand_def, current_placement.minute_hand_index, ctx);

  if (config.second_hand || chrono_data.running || chrono_data.hold_ms != 0) {
    draw_hand(&chrono_second_cache RESOURCE_CACHE_PARAMS(chrono_second_resource_cache, chrono_second_resource_cache_size), &chrono_second_hand_def, current_placement.chrono_second_hand_index, ctx);
  }

#elif defined(ENABLE_CHRONO_DIAL)
  // In this case, we're not implementing full chrono functionality,
  // but we still have to deal with that little second hand.
  if (config.second_hand) {
    draw_hand(&second_cache RESOURCE_CACHE_PARAMS(second_resource_cache, second_resource_cache_size), &second_hand_def, current_placement.second_hand_index, ctx);
  }

  draw_hand(&hour_cache RESOURCE_CACHE_PARAMS(NULL, 0), &hour_hand_def, current_placement.hour_hand_index, ctx);

  draw_hand(&minute_cache RESOURCE_CACHE_PARAMS(NULL, 0), &minute_hand_def, current_placement.minute_hand_index, ctx);

#else  // MAKE_CHRONOGRAPH
  // The normal, non-chrono implementation; and here in phase 2 we
  // only need to draw the second hand.

  if (config.second_hand) {
    draw_hand(&second_cache RESOURCE_CACHE_PARAMS(second_resource_cache, second_resource_cache_size), &second_hand_def, current_placement.second_hand_index, ctx);
  }

#endif  // MAKE_CHRONOGRAPH
}

// Triggers a memory panic if at least MIN_BYTES_FREE are not available.
void check_min_bytes_free() {
  // We insist on checking for contiguous bytes free, which we do by
  // attempting to allocate a buffer of MIN_BYTES_FREE.
  char *buffer = malloc(MIN_BYTES_FREE);
  if (buffer != NULL) {
    // It's available, great!
    free(buffer);
    return;
  }

  // Not available.  Too bad.
  trigger_memory_panic(__LINE__);
}

// This function should be called at the end of every callback
// function (before returning back to the system) to ensure we leave a
// minimum buffer available for system activity.
void check_memory_usage() {
  check_min_bytes_free();

  // Keep trying until sufficient memory is clear.
  while (memory_panic_flag) {
    reset_memory_panic();
    check_min_bytes_free();
  }
}

#if !defined(PBL_PLATFORM_APLITE) && PBL_API_EXISTS(layer_get_unobstructed_bounds)
void root_layer_update_callback(Layer *me, GContext *ctx) {
  //qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "root_layer");

  // Only bother filling in the root layer if part of the window is
  // obstructed.  We do this to ensure the entire window is cleared in
  // case we're not drawing all of it.
  if (any_obstructed_area) {
    GRect destination = layer_get_frame(me);
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, destination, 0, GCornerNone);
  }
}
#endif  // PBL_API_EXISTS(layer_get_unobstructed_bounds)

void clock_face_layer_update_callback(Layer *me, GContext *ctx) {
  // Make sure we have reset our memory usage before we start to draw.
  check_memory_usage();

  do {
    qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "clock_face_layer, memory_panic_count = %d, heap_bytes_free = %d", memory_panic_count, heap_bytes_free());

    // In case we're in extreme memory panic mode--too little
    // available memory to even keep the clock face resident--we don't
    // draw any clock background.
    if (!hide_clock_face) {
      // Perform framebuffer caching to minimize redraws.
      if (clock_face.bitmap == NULL || redraw_clock_face) {
        // The clock face needs to be redrawn (or drawn for the first
        // time).  This is every part of the display except for the
        // hands, including the date windows and top subdial.  If the
        // second hand is enabled, it also includes the hour and minute
        // hands.
        bwd_destroy(&clock_face);
        redraw_clock_face = false;

        // Draw the clock face into the frame buffer.
        draw_clock_face(me, ctx);

        if (SEPARATE_PHASE_HANDS) {
          // If the second hand is enabled, then we also draw the
          // phase_1 hands at this time, so they get cached in the clock
          // face buffer.
          draw_phase_1_hands(ctx);
        }

        if (save_framebuffer) {
          // Now save the render for next time.
          GBitmap *fb = graphics_capture_frame_buffer(ctx);
          assert(clock_face.bitmap == NULL);

          if (!keep_face_asset) {
            // Destroy face_bitmap only after we have already drawn
            // everything else that goes onto it, and just before we
            // dupe the framebuffer.  This helps minimize fragmentation.
#ifdef PBL_BW
            // On B&W we can go one step further (and we probably have
            // to because memory is so tight on Aplite): we can use
            // the *same* memory for framebuffer that we had already
            // allocated for face_bitmap, because they will be the
            // same bitmap format and size.
            clock_face = face_bitmap;
            face_bitmap.bitmap = NULL;
            bwd_copy_into_from_bitmap(&clock_face, fb);

#else  //  PBL_BW
            // On other platforms, they are likely to have a different
            // format (the clock face will be 4-bit palette), so we have
            // to deallocate and reallocate.
            bwd_destroy(&face_bitmap);
            clock_face = bwd_copy_bitmap(fb);
            if (clock_face.bitmap == NULL) {
              trigger_memory_panic(__LINE__);
            }
#endif  //  PBL_BW
          } else {
            // If we're confident we can keep both the face_bitmap and
            // clock_face around together, do so.
            clock_face = bwd_copy_bitmap(fb);
            if (clock_face.bitmap == NULL) {
              trigger_memory_panic(__LINE__);
            }
          }

          graphics_release_frame_buffer(ctx, fb);
        }

      } else {
        // The rendered clock face is already saved from a previous
        // update; redraw it now.
        GRect destination = layer_get_frame(me);
        destination.origin.x = -destination.origin.x;
        destination.origin.y = -destination.origin.y;
        graphics_context_set_compositing_mode(ctx, GCompOpAssign);
        graphics_draw_bitmap_in_rect(ctx, clock_face.bitmap, destination);
      }
    }

    if (date_window_dynamic) {
      // Now fill in the per-frame dynamic text, if needed.
      for (int i = 0; i < NUM_DATE_WINDOWS; ++i) {
        draw_date_window_dynamic_text(ctx, i);
      }
    }

    if (!SEPARATE_PHASE_HANDS || hide_clock_face) {
      // If the second hand is *not* enabled, then we draw the phase_1
      // hands at this time, so we don't have to invalidate the buffer
      // each minute.
      draw_phase_1_hands(ctx);
    }

    // And we always draw the phase_2 hands last, each update.  These
    // are the most dynamic hands that are never part of the captured
    // framebuffer.
    draw_phase_2_hands(ctx);

    if (!memory_panic_flag) {
      // If we successfully drew the clock face without memory
      // panicking, return.
      check_memory_usage();
      return;
    }

    // In this case we hit a memory_panic_flag while drawing.  Reset
    // and try again.
    reset_memory_panic();
  } while (true);
}

// Draws the frame and optionally fills the background of the current date window.
void draw_date_window_background(GContext *ctx, int date_window_index, unsigned int fg_draw_mode, unsigned int bg_draw_mode) {
  int indicator_face_index = get_indicator_face_index();
  const struct IndicatorTable *window = &date_windows[date_window_index][indicator_face_index];
  GRect box = GRect(window->x, window->y, DATE_WINDOW_SIZE_X, DATE_WINDOW_SIZE_Y);

#ifdef PBL_BW
  // We only need the mask on B&W watches.
  if (date_window_mask.bitmap == NULL) {
    date_window_mask = rle_bwd_create(RESOURCE_ID_DATE_WINDOW_MASK);
    if (date_window_mask.bitmap == NULL) {
      trigger_memory_panic(__LINE__);
      return;
    }
  }
  graphics_context_set_compositing_mode(ctx, draw_mode_table[bg_draw_mode].paint_bg);
  graphics_draw_bitmap_in_rect(ctx, date_window_mask.bitmap, box);
#endif  // PBL_BW

  if (date_window.bitmap == NULL) {
    date_window = rle_bwd_create(RESOURCE_ID_DATE_WINDOW);
    if (date_window.bitmap == NULL) {
      bwd_destroy(&date_window_mask);
      trigger_memory_panic(__LINE__);
      return;
    }
#ifndef PBL_BW
    remap_colors_date(&date_window);
#endif  // PBL_BW
  }

  graphics_context_set_compositing_mode(ctx, draw_mode_table[fg_draw_mode].paint_fg);
  graphics_draw_bitmap_in_rect(ctx, date_window.bitmap, box);
}

// Draws a date window with the specified text contents.  Usually this is
// something like a numeric date or the weekday name.
void draw_date_window_text(GContext *ctx, int date_window_index, const char *text, struct FontPlacement *font_placement, GFont font) {
  //  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "draw_date_window_text %c, %s, %p", date_window_index + 'a', text, font);
  if (font == NULL) {
    return;
  }
  int indicator_face_index = get_indicator_face_index();
  const struct IndicatorTable *window = &date_windows[date_window_index][indicator_face_index];
  GRect box = GRect(window->x, window->y, DATE_WINDOW_SIZE_X, DATE_WINDOW_SIZE_Y);

#ifdef PBL_BW
  unsigned int draw_mode = window->invert ^ config.draw_mode ^ BW_INVERT;
  graphics_context_set_text_color(ctx, draw_mode_table[draw_mode].colors[1]);
#else
  struct FaceColorDef *cd = &clock_face_color_table[config.color_mode];
  GColor fg;
  fg.argb = cd->d1_argb8;
  if (config.draw_mode) {
    fg.argb ^= 0x3f;
  }
  graphics_context_set_text_color(ctx, fg);
#endif  // PBL_BW

  box.origin.y += font_placement->vshift;

  // The Pebble text routines seem to be a bit too conservative when
  // deciding whether a given bit of text will fit within its assigned
  // box, meaning the text is likely to be trimmed even if it would
  // have fit.  We avoid this problem by cheating and expanding the
  // box a bit wider and taller than we actually intend it to be.
  box.origin.x -= 4;
  box.size.w += 8;
  box.size.h += 4;

  graphics_draw_text(ctx, text, font, box,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}

void format_date_number(char buffer[DATE_WINDOW_BUFFER_SIZE], int value) {
  snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%d", value);
}

#ifndef PBL_PLATFORM_APLITE
void format_health_metric_count_today(char buffer[DATE_WINDOW_BUFFER_SIZE], HealthMetric metric, int *cached_value, int scale_factor) {
  int value;
  if (*cached_value >= 0) {
    value = *cached_value;
  } else {
    value = (int)health_service_sum_today(metric) / scale_factor;
    *cached_value = value;
  }

  // Show only the bottom four digits.
  if (value < 10000) {
    snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%d", value);
  } else {
    snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%04d", value % 10000);
  }
}

void format_health_metric_calories_today(char buffer[DATE_WINDOW_BUFFER_SIZE], int *cached_value) {
  int value;
  if (*cached_value >= 0) {
    value = *cached_value;
  } else {
    value = (int)(health_service_sum_today(HealthMetricRestingKCalories) + health_service_sum_today(HealthMetricActiveKCalories));
    *cached_value = value;
  }
  snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%d", value);
}

void format_health_metric_time_today(char buffer[DATE_WINDOW_BUFFER_SIZE], HealthMetric metric, int *cached_value) {
  int value;
  if (*cached_value >= 0) {
    value = *cached_value;
  } else {
    value = (int)health_service_sum_today(metric);
    *cached_value = value;
  }
  int minutes = value / 60;
  int hours = minutes / 60;

  snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%d:%02d", hours, minutes % 60);
}

void format_health_metric_distance_today(char buffer[DATE_WINDOW_BUFFER_SIZE], HealthMetric metric, int *cached_value) {
  int value;
  if (*cached_value >= 0) {
    value = *cached_value;
  } else {
    int meters = (int)health_service_sum_today(metric);
    switch (health_service_get_measurement_system_for_display(metric)) {
    case MeasurementSystemImperial:
      {
        value = ((meters * 1000 + 80467) / 160934);  // 0.1 miles
      }
      break;

    case MeasurementSystemMetric:
    default:
      {
        value = (meters + 50) / 100;  // "hectometers", 0.1 kilometers
      }
      break;
    }
    *cached_value = value;
  }

  snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%d.%01d", value / 10, value % 10);
}

#endif  // PBL_PLATFORM_APLITE

#ifdef SUPPORT_HEART_RATE

void format_health_metric_count_peek(char buffer[DATE_WINDOW_BUFFER_SIZE], HealthMetric metric, int *cached_value) {
  int value;
  if (*cached_value >= 0) {
    value = *cached_value;
  } else {
    value = (int)health_service_peek_current_value(metric);
    *cached_value = value;
  }
  snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%d", value);
}

#endif  // SUPPORT_HEART_RATE

// yday is the current ordinal date [0..365], wday is the current day
// of the week [0..6], 0 = Sunday.  year is the current year less 1900.

// day_of_week is the first day of the week, while first_week_contains
// is the ordinal date that must be contained within week 1.
int raw_compute_week_number(int yday, int wday, int day_of_week, int first_week_contains) {
  // The ordinal date index (where Jan 1 is 0) of the next Sunday
  // following today.
  int sunday_index = yday - wday + 7;

  // The ordinal date index of the first day of an early week.
  int first_day_index = (sunday_index + day_of_week) % 7;

  if (first_day_index > first_week_contains) {
    first_day_index -= 7;
  }

  // Now first_day_index is the ordinal date index of the first day of
  // week 1.  It will be in the range [-7, 6].  (It might be negative
  // if week 1 began in the last few days of the previous year.)

  int week_number = (yday - first_day_index + 7) / 7;
  return week_number;
}

int compute_week_number(int yday, int wday, int year, int day_of_week, int first_week_contains) {
  int week_number = raw_compute_week_number(yday, wday, day_of_week, first_week_contains);

  if (week_number < 1) {
    // It's possible to come up with the answer 0, in which case we
    // really meant the last week of the previous year.
    time_t last_year = make_gmt_date(31, 11, year - 1);  // Dec 31
    struct tm *lyt = gmtime(&last_year);
    week_number = raw_compute_week_number(lyt->tm_yday, lyt->tm_wday, day_of_week, first_week_contains);

  } else if (week_number > 52) {
    // If we came up with the answer 53, the answer might really be
    // the first week of the next year.
    time_t next_year = make_gmt_date(1, 0, year + 1);  // Jan 1
    struct tm *nyt = gmtime(&next_year);
    int next_week_number = raw_compute_week_number(nyt->tm_yday, nyt->tm_wday, day_of_week, first_week_contains);
    if (next_week_number != 0) {
      week_number = next_week_number;
    }
  }

  return week_number;
}

void compute_and_format_week_number(char buffer[DATE_WINDOW_BUFFER_SIZE], int day_of_week, int first_week_contains) {
  int yday = current_placement.ordinal_date_index;
  int wday = current_placement.day_index;
  int year = current_placement.year_value;

  int week_number = compute_week_number(yday, wday, year, day_of_week, first_week_contains);
  format_date_number(buffer, week_number);
}

// Draws the background and contents of the specified date window.
void draw_full_date_window(GContext *ctx, int date_window_index) {
  //  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "draw_full_date_window %c", date_window_index + 'a');

  DateWindowMode dwm = config.date_windows[date_window_index];
  if (dwm == DWM_off) {
    // Do nothing.
    return;
  }

  int indicator_face_index = get_indicator_face_index();
  const struct IndicatorTable *window = &date_windows[date_window_index][indicator_face_index];

  unsigned int draw_mode = window->invert ^ config.draw_mode ^ BW_INVERT;
  draw_date_window_background(ctx, date_window_index, draw_mode, draw_mode);

  // Format the date or weekday or whatever text for display.
  char buffer[DATE_WINDOW_BUFFER_SIZE];

  GFont font = date_numeric_font;
  struct FontPlacement *font_placement = &date_lang_font_placement[0];
  if (dwm >= DWM_weekday && dwm <= DWM_ampm) {
    // Draw text using date_lang_font.
    const LangDef *lang = &lang_table[config.display_lang];
    font = date_lang_font;
    font_placement = &date_lang_font_placement[lang->font_index];
  }

  char *text = buffer;

  switch (dwm) {
  case DWM_identify:
    snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%c", toupper((int)(DATE_WINDOW_KEYS[date_window_index])));
    break;

  case DWM_date:
    format_date_number(buffer, current_placement.date_value);
    break;

  case DWM_year:
    format_date_number(buffer, current_placement.year_value + 1900);
    break;

  case DWM_week:
    switch (config.week_numbering) {
    case WNM_mon_4:
      compute_and_format_week_number(buffer, 1, 3);
      break;

    case WNM_sun_1:
      compute_and_format_week_number(buffer, 0, 0);
      break;

    case WNM_sat_1:
      compute_and_format_week_number(buffer, 6, 0);
      break;
    }
    break;

  case DWM_yday:
    format_date_number(buffer, current_placement.ordinal_date_index + 1);
    break;

  case DWM_debug_heap_free:
  case DWM_debug_memory_panic_count:
  case DWM_debug_resource_reads:
  case DWM_debug_draw_face_count:
    // We have some dynamic text that will need a separate pass to
    // re-render each frame.
    date_window_dynamic = true;
    return;

  case DWM_weekday:
    text = date_names[current_placement.day_index];
    break;

  case DWM_month:
    text = date_names[current_placement.month_index + NUM_WEEKDAY_NAMES];
    break;

  case DWM_ampm:
    text = date_names[current_placement.ampm_value + NUM_WEEKDAY_NAMES + NUM_MONTH_NAMES];
    break;

#ifndef PBL_PLATFORM_APLITE
  case DWM_step_count:
  case DWM_step_count_10:
  case DWM_active_time:
  case DWM_walked_distance:
  case DWM_calories_burned:
    date_window_dynamic = true;
    return;

  case DWM_sleep_time:
    format_health_metric_time_today(buffer, HealthMetricSleepSeconds, &cached_sleep_time);
    break;

  case DWM_sleep_restful_time:
    format_health_metric_time_today(buffer, HealthMetricSleepRestfulSeconds, &cached_sleep_restful_time);
    break;
#endif  // PBL_PLATFORM_APLITE

#ifdef PBL_PLATFORM_DIORITE
  case DWM_heart_rate:
    date_window_dynamic = true;
    return;
#endif  // PBL_PLATFORM_DIORITE

  case DWM_moon_unused:
  default:
    strcpy(buffer, "- -");
  }

  draw_date_window_text(ctx, date_window_index, text, font_placement, font);
}

// Fills in the dynamic text on top of the date window if needed.
// (This is re-rendered per frame and doesn't get baked into the
// clock_face bitmap.)
void draw_date_window_dynamic_text(GContext *ctx, int date_window_index) {
  //  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "draw_date_window_dynamic_text %c", date_window_index + 'a');

  DateWindowMode dwm = config.date_windows[date_window_index];

  // Format the date or weekday or whatever text for display.
#define DATE_WINDOW_BUFFER_SIZE 16
  char buffer[DATE_WINDOW_BUFFER_SIZE];

  char *text = buffer;

  switch (dwm) {
  case DWM_debug_heap_free:
    snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%dk", heap_bytes_free() / 1024);
    break;

  case DWM_debug_memory_panic_count:
    snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%d!", memory_panic_count);
    break;

  case DWM_debug_resource_reads:
    snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%d", bwd_resource_reads);
    break;

  case DWM_debug_draw_face_count:
    snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%d", draw_face_count);
    break;

#ifndef PBL_PLATFORM_APLITE
  case DWM_step_count:
    format_health_metric_count_today(buffer, HealthMetricStepCount, &cached_step_count, 1);
    break;

  case DWM_step_count_10:
    format_health_metric_count_today(buffer, HealthMetricStepCount, &cached_step_count_10, 10);
    break;

  case DWM_active_time:
    format_health_metric_time_today(buffer, HealthMetricActiveSeconds, &cached_active_time);
    break;

  case DWM_walked_distance:
    format_health_metric_distance_today(buffer, HealthMetricWalkedDistanceMeters, &cached_walked_distance);
    break;

  case DWM_calories_burned:
    format_health_metric_calories_today(buffer, &cached_calories_burned);
    break;
#endif  // PBL_PLATFORM_APLITE

#ifdef SUPPORT_HEART_RATE
  case DWM_heart_rate:
    format_health_metric_count_peek(buffer, HealthMetricHeartRateBPM, &cached_heart_rate);
    break;
#endif  // SUPPORT_HEART_RATE

  default:
    // Not a dynamic element.  Do nothing.
    return;
  }

  GFont font = date_numeric_font;
  struct FontPlacement *font_placement = &date_lang_font_placement[0];
  draw_date_window_text(ctx, date_window_index, text, font_placement, font);
}

// Called once per epoch (e.g. once per second, or once per minute) to
// compute the new positions for all of the hands on the watch based
// on the current time.  This does not actually draw the hands; it
// only computes which position each hand should hold, and it marks
// the appropriate layers dirty, to eventually redraw the hands that
// have moved since the last call.
void update_hands(struct tm *time) {
  (void)poll_quiet_time_state();
  struct HandPlacement new_placement = current_placement;

  compute_hands(time, &new_placement);
  if (new_placement.hour_hand_index != current_placement.hour_hand_index) {
    current_placement.hour_hand_index = new_placement.hour_hand_index;
    layer_mark_dirty(clock_face_layer);

    if (SEPARATE_PHASE_HANDS) {
      // If the hour and minute hands are baked into the clock face
      // cache, it must be redrawn now.
      qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "hour hand changed with SEPARATE_PHASE_HANDS");
      invalidate_clock_face();
    }
  }

  if (new_placement.minute_hand_index != current_placement.minute_hand_index) {
    current_placement.minute_hand_index = new_placement.minute_hand_index;
    layer_mark_dirty(clock_face_layer);

    if (SEPARATE_PHASE_HANDS) {
      // If the hour and minute hands are baked into the clock face
      // cache, it must be redrawn now.
      qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "minute hand changed with SEPARATE_PHASE_HANDS");
      invalidate_clock_face();
    }
  }

  if (new_placement.second_hand_index != current_placement.second_hand_index) {
    current_placement.second_hand_index = new_placement.second_hand_index;
    layer_mark_dirty(clock_face_layer);
  }

  if (new_placement.buzzed_hour != current_placement.buzzed_hour) {
    current_placement.buzzed_hour = new_placement.buzzed_hour;
    if (config.hour_buzzer) {
      // The hour has changed; ring the buzzer if it's enabled.
      if (!poll_quiet_time_state()) {
        vibes_enqueue_custom_pattern(tap);
      }
    }
  }

#if ENABLE_SWEEP_SECONDS
  // Make sure the sweep timer is fast enough to capture the second
  // hand.
  sweep_timer_ms = 1000;
  if (config.sweep_seconds) {
    sweep_timer_ms = sweep_seconds_ms;
  }
#endif  // ENABLE_SWEEP_SECONDS

#ifdef MAKE_CHRONOGRAPH
  update_chrono_hands(&new_placement);
#endif  // MAKE_CHRONOGRAPH

  // If any of the date window properties changes, update all of the
  // date windows.  (We cheat and only check the fastest changing
  // element, and the date value just in case someone's playing games
  // with the clock.)
  if (new_placement.ampm_value != current_placement.ampm_value ||
      new_placement.date_value != current_placement.date_value) {
    current_placement.day_index = new_placement.day_index;
    current_placement.month_index = new_placement.month_index;
    current_placement.date_value = new_placement.date_value;
    current_placement.year_value = new_placement.year_value;
    current_placement.ampm_value = new_placement.ampm_value;
    current_placement.ordinal_date_index = new_placement.ordinal_date_index;

    qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "date changed");
    invalidate_clock_face();
  }

#ifdef TOP_SUBDIAL
  // And the lunar index in the moon subdial.
  if (new_placement.lunar_index != current_placement.lunar_index) {
    current_placement.lunar_index = new_placement.lunar_index;
    bwd_destroy(&moon_wheel_bitmap);
    qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "moon changed");
    invalidate_clock_face();
  }
#endif  // TOP_SUBDIAL
}

#if ENABLE_SWEEP_SECONDS
// Triggered at sweep_timer_ms intervals to run the sweep-second hand
// (that is, when sweep_seconds is enabled, this timer runs faster
// than 1 second to update the second hand smoothly).
void handle_sweep(void *data) {
  sweep_timer = NULL;  // When the timer is handled, it is implicitly canceled.
  if (sweep_timer_ms < 1000) {
    update_hands(NULL);
    sweep_timer = app_timer_register(sweep_timer_ms, &handle_sweep, 0);
  }
}
#endif  // ENABLE_SWEEP_SECONDS

// Reset the sweep_timer according to the configured settings.  If
// sweep_seconds is enabled, this sets the sweep_timer to wake us up
// at the next sub-second interval.  If sweep_seconds is not enabled,
// the sweep_timer is not used.
#if ENABLE_SWEEP_SECONDS
void reset_sweep() {
  if (sweep_timer != NULL) {
    app_timer_cancel(sweep_timer);
    sweep_timer = NULL;
  }
  if (sweep_timer_ms < 1000) {
    sweep_timer = app_timer_register(sweep_timer_ms, &handle_sweep, 0);
  }
}
#endif  // ENABLE_SWEEP_SECONDS

// The callback on the per-second (or per-minute) system timer that
// handles most mundane tasks.
void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  update_hands(tick_time);

#if ENABLE_SWEEP_SECONDS
  reset_sweep();
#endif   //ENABLE_SWEEP_SECONDS

  check_memory_usage();
}

#ifndef PBL_PLATFORM_APLITE
void health_event_handler(HealthEventType event, void *context) {
  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "health event");

  switch (event) {
  case HealthEventSignificantUpdate:
    // Invalidate everything.
    if (cached_sleep_time < 0 &&
        cached_sleep_restful_time < 0 &&
        cached_step_count < 0 &&
        cached_step_count_10 < 0 &&
        cached_active_time < 0 &&
        cached_walked_distance < 0 &&
        cached_calories_burned < 0 &&
        cached_heart_rate < 0) {
      // No changes.
      return;
    }

    cached_sleep_time = -1;
    cached_sleep_restful_time = -1;
    cached_step_count = -1;
    cached_step_count_10 = -1;
    cached_active_time = -1;
    cached_walked_distance = -1;
    cached_calories_burned = -1;
    cached_heart_rate = -1;
    break;

  case HealthEventMovementUpdate:
    // Invalidate step count, active time, and distance walked.
    if (cached_step_count < 0 &&
        cached_step_count_10 < 0 &&
        cached_active_time < 0 &&
        cached_walked_distance < 0 &&
        cached_calories_burned < 0) {
      // No changes.
      return;
    }
    cached_step_count = -1;
    cached_step_count_10 = -1;
    cached_active_time = -1;
    cached_walked_distance = -1;
    cached_calories_burned = -1;
    break;

  case HealthEventSleepUpdate:
    // Invalidate sleep time.
    if (cached_sleep_time < 0 &&
        cached_sleep_restful_time < 0) {
      // No changes.
      return;
    }
    cached_sleep_time = -1;
    cached_sleep_restful_time = -1;
    break;

#ifdef SUPPORT_HEART_RATE
  case HealthEventHeartRateUpdate:
    // Invalidate heart rate.
    if (cached_heart_rate < 0) {
      // No changes.
      return;
    }
    cached_heart_rate = -1;
    break;
#endif  // SUPPORT_HEART_RATE

  default:
    // Some other event we don't care about.
    return;
  }

  if (!tick_seconds_subscribed) {
    // If we have a second hand update, we don't need to explicitly
    // redraw the watchface now (because redrawing on the next second
    // will be good enough); but if we're only updating on the minute,
    // then we should redraw so the user can see his new step count or
    // heart rate or whatever.
    layer_mark_dirty(clock_face_layer);
  }
}
#endif  // PBL_PLATFORM_APLITE

void did_focus_handler(bool new_in_focus) {
  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "did_focus: %d", (int)new_in_focus);
  if (new_in_focus == app_in_focus) {
    qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "no change to focus");
    return;
  }

  app_in_focus = new_in_focus;
  if (app_in_focus) {
    // We have just regained focus from a notification or something.
    // Ensure the window is completely redrawn.
    qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "regained focus");
    invalidate_clock_face();
  }
}


void window_load_handler(struct Window *window) {
  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "main window loads");
  check_memory_usage();
}

void window_appear_handler(struct Window *window) {
  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "main window appears");

#ifdef SCREENSHOT_BUILD
  config_set_click_config(window);
#elif defined(MAKE_CHRONOGRAPH)
  chrono_set_click_config(window);
#endif  // MAKE_CHRONOGRAPH
  check_memory_usage();
}

void window_disappear_handler(struct Window *window) {
  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "main window disappears");
  check_memory_usage();
}

void window_unload_handler(struct Window *window) {
  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "main window unloads");
  check_memory_usage();
}


// Sets the tick timer to fire at the appropriate rate based on current conditions.
void reset_tick_timer() {
  tick_timer_service_unsubscribe();

#if defined(FAST_TIME) || defined(BATTERY_HACK)
  tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
  tick_seconds_subscribed = true;

#elif defined(MAKE_CHRONOGRAPH)
  if (config.second_hand || chrono_data.running) {
    tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
    tick_seconds_subscribed = true;
  } else {
    tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
    tick_seconds_subscribed = false;
  }

  reset_chrono_digital_timer();

#else
  if (config.second_hand) {
    tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
    tick_seconds_subscribed = true;
  } else {
    tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
    tick_seconds_subscribed = false;
  }
#endif
}

void reset_clock_face() {
}

// Populates a table of names, like weekday or month names, from the language resource.
void fill_date_names(char *date_names[], int num_date_names, char date_names_buffer[], int date_names_max_buffer, int resource_id) {
  ResHandle rh = resource_get_handle(resource_id);
  size_t bytes_read = resource_load(rh, (void *)date_names_buffer, date_names_max_buffer);
  date_names_buffer[bytes_read] = '\0';
  int i = 0;
  char *p = date_names_buffer;
  date_names[i] = p;
  ++i;
  while (i < num_date_names && p < date_names_buffer + bytes_read) {
    if (*p == '\0') {
      ++p;
      date_names[i] = p;
      ++i;
    } else {
      ++p;
    }
  }
  while (i < num_date_names) {
    date_names[i] = NULL;
    ++i;
  }
}

// Restores memory_panic_count to 0, as in a fresh start.
void reset_memory_panic_count() {
  memory_panic_count = 0;

#ifdef SUPPORT_RESOURCE_CACHE
  second_resource_cache_size = SECOND_RESOURCE_CACHE_SIZE + SECOND_MASK_RESOURCE_CACHE_SIZE;
#ifdef MAKE_CHRONOGRAPH
  chrono_second_resource_cache_size = CHRONO_SECOND_RESOURCE_CACHE_SIZE +  + CHRONO_SECOND_MASK_RESOURCE_CACHE_SIZE;
#endif  // MAKE_CHRONOGRAPH
#endif  // SUPPORT_RESOURCE_CACHE

  // Confidently start out with the expectation that we keep keep all
  // of this cached in RAM, until proven otherwise.
#ifndef NEVER_KEEP_ASSETS
  keep_assets = true;
#endif  // NEVER_KEEP_ASSETS
#ifndef NEVER_KEEP_FACE_ASSET
  keep_face_asset = true;
#endif  // NEVER_KEEP_FACE_ASSET

  hide_date_windows = false;
  hide_clock_face = false;
}

// Updates any runtime settings as needed when the config changes.
void apply_config() {
  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "apply_config");

  // Reset the memory panic count when we get a new config setting.
  // Maybe the user knows what he's doing.
  reset_memory_panic_count();

  if (face_index != config.face_index) {
    // Update the face bitmap if it's changed.
    face_index = config.face_index;
  }

  if (display_lang != config.display_lang) {
    // Reload the weekday, month, and ampm names from the appropriate
    // language resource.
    fill_date_names(date_names, NUM_DATE_NAMES, date_names_buffer, DATE_NAMES_MAX_BUFFER, lang_table[config.display_lang].date_name_id);
    display_lang = config.display_lang;
  }

  // Reload all bitmaps just for good measure.  Maybe the user changed
  // the draw mode or something else.
  recreate_all_objects();
  reset_tick_timer();
}

// Call this to force the clock_face bitmap cache to be recomputed and
// redrawn next frame (e.g. if something on the face needs to be
// updated).
void invalidate_clock_face() {
  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "invalidate_clock_face");
  redraw_clock_face = true;
  bwd_destroy(&clock_face);
  if (clock_face_layer != NULL) {
    layer_mark_dirty(clock_face_layer);
  }
}

void unload_date_fonts() {
  if (date_numeric_font == date_lang_font) {
    // In this case, we just shared the same pointer; don't release it
    // twice.
    date_numeric_font = NULL;
  } else if (date_numeric_font != NULL) {
    safe_unload_custom_font(&date_numeric_font);
  }
  if (date_lang_font != NULL) {
    safe_unload_custom_font(&date_lang_font);
  }
}

void load_date_fonts() {
  unload_date_fonts();
  if (hide_date_windows) {
    // Don't load fonts if the memory panic count is over this
    // threshold.

  } else {
    // Load the date_window fonts up early, and keep them loaded, so
    // they're always here in the top of memory.  (Fonts seem to
    // sometimes fail to load correctly, with no reported error, when
    // memory is fragmented, so better to load them up front to avoid
    // this problem.)

    const LangDef *lang = &lang_table[config.display_lang];
    int lang_font_resource_id = date_lang_font_placement[lang->font_index].resource_id;
    int numeric_font_resource_id = date_lang_font_placement[0].resource_id;
    date_lang_font = safe_load_custom_font(lang_font_resource_id);
    if (numeric_font_resource_id == lang_font_resource_id) {
      date_numeric_font = date_lang_font;
    } else {
      date_numeric_font = safe_load_custom_font(numeric_font_resource_id);
    }
  }
}

#if !defined(PBL_PLATFORM_APLITE) && PBL_API_EXISTS(layer_get_unobstructed_bounds)
// The unobstructed area of the watchface is changing (e.g. due to a
// timeline quick view message).  Adjust layers accordingly.
void adjust_unobstructed_area() {
  struct Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_unobstructed_bounds(window_layer);
  GRect orig_bounds = layer_get_bounds(window_layer);
  any_obstructed_area = (memcmp(&bounds, &orig_bounds, sizeof(bounds)) != 0);

  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "unobstructed_area: %d %d %d %d, any_obstructed_area = %d", bounds.origin.x, bounds.origin.y, bounds.size.w, bounds.size.h, any_obstructed_area);

  // Shift the face layer to center the face within the new region.
  int cx = bounds.origin.x + bounds.size.w / 2;
  int cy = bounds.origin.y + bounds.size.h / 2;

  GRect face_layer_shifted = { { cx - SCREEN_WIDTH / 2, cy - SCREEN_HEIGHT / 2 },
                               { SCREEN_WIDTH, SCREEN_HEIGHT } };

  /*
  // hack
  GRect face_layer_shifted = { { bounds.size.w - SCREEN_WIDTH, 0 },
                               { SCREEN_WIDTH, SCREEN_HEIGHT } };
  */

  layer_set_frame(clock_face_layer, face_layer_shifted);
  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "unobstructed area changed");
  invalidate_clock_face();
}

void unobstructed_area_change_handler(AnimationProgress progress, void *context) {
  adjust_unobstructed_area();
}
#endif  // PBL_API_EXISTS(layer_get_unobstructed_bounds)

// This is called only once, at startup.
void create_permanent_objects() {
  window = window_create();
  assert(window != NULL);

  struct WindowHandlers window_handlers;
  memset(&window_handlers, 0, sizeof(window_handlers));
  window_handlers.load = window_load_handler;
  window_handlers.appear = window_appear_handler;
  window_handlers.disappear = window_disappear_handler;
  window_handlers.unload = window_unload_handler;
  window_set_window_handlers(window, window_handlers);

  window_stack_push(window, true);

  // clock_face_layer must be a permanent object, since we might call
  // destroy_temporal_objects() from within clock_face_layer_update_callback()!
  Layer *window_layer = window_get_root_layer(window);
  GRect window_frame = layer_get_frame(window_layer);

  clock_face_layer = layer_create(window_frame);
  assert(clock_face_layer != NULL);
  layer_set_update_proc(clock_face_layer, &clock_face_layer_update_callback);
  layer_add_child(window_layer, clock_face_layer);

#if !defined(PBL_PLATFORM_APLITE) && PBL_API_EXISTS(layer_get_unobstructed_bounds)
  layer_set_update_proc(window_layer, &root_layer_update_callback);

  struct UnobstructedAreaHandlers unobstructed_area_handlers;
  memset(&unobstructed_area_handlers, 0, sizeof(unobstructed_area_handlers));
  unobstructed_area_handlers.change = unobstructed_area_change_handler;
  unobstructed_area_service_subscribe(unobstructed_area_handlers, NULL);
  adjust_unobstructed_area();
#endif  // PBL_API_EXISTS(layer_get_unobstructed_bounds)
}

// This is, of course, called only once, at shutdown.
void destroy_permanent_objects() {
  layer_destroy(clock_face_layer);
  clock_face_layer = NULL;

  window_stack_pop_all(false);
  window_destroy(window);
  window = NULL;
}

// Creates all of the objects, other than permanently residing
// objects, needed during the normal watch functioning.  Normally
// called by handle_init(), and might also be invoked midstream when
// we need to reshuffle memory.
void create_temporal_objects() {
  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "create_temporal_objects");

  hand_cache_init(&hour_cache);
  hand_cache_init(&minute_cache);
  hand_cache_init(&second_cache);

  invalidate_clock_face();

  init_battery_gauge();
  init_bluetooth_indicator();

#ifdef ENABLE_CHRONO_DIAL
  create_chrono_objects();
#endif  // ENABLE_CHRONO_DIAL
}

// Destroys the objects created by create_temporal_objects().
void destroy_temporal_objects() {
  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "destroy_temporal_objects");

  bwd_destroy(&date_window);
  bwd_destroy(&date_window_mask);
  bwd_destroy(&face_bitmap);
  bwd_destroy(&top_subdial_bitmap);
  bwd_destroy(&moon_wheel_bitmap);

#ifndef PREBAKE_LABEL
  bwd_destroy(&pebble_label);
#endif  // PREBAKE_LABEL

  bwd_destroy(&clock_face);
  face_index = -1;

#ifdef ENABLE_CHRONO_DIAL
  destroy_chrono_objects();
#endif  // ENABLE_CHRONO_DIAL

  deinit_battery_gauge();
  deinit_bluetooth_indicator();

  bwd_clear_cache(second_resource_cache, SECOND_RESOURCE_CACHE_SIZE + SECOND_MASK_RESOURCE_CACHE_SIZE);

#ifdef MAKE_CHRONOGRAPH
  bwd_clear_cache(chrono_second_resource_cache, CHRONO_SECOND_RESOURCE_CACHE_SIZE + CHRONO_SECOND_MASK_RESOURCE_CACHE_SIZE);
#endif  // MAKE_CHRONOGRAPH

  hand_cache_destroy(&hour_cache);
  hand_cache_destroy(&minute_cache);
  hand_cache_destroy(&second_cache);

  display_lang = -1;
}

void recreate_all_objects() {
  unload_date_fonts();
  destroy_temporal_objects();
  create_temporal_objects();
  load_date_fonts();
  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "recreate_all_objects");
  invalidate_clock_face();
}

// Called at program exit to cleanly shut everything down.
void handle_deinit() {
#ifdef MAKE_CHRONOGRAPH
  save_chrono_data();
#endif  // MAKE_CHRONOGRAPH
  tick_timer_service_unsubscribe();

  unload_date_fonts();
  destroy_temporal_objects();
  destroy_permanent_objects();
}

// Called at program start to bootstrap everything.
void handle_init() {
  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "handle_init");

  load_config();

#ifdef MAKE_CHRONOGRAPH
  load_chrono_data();
#endif  // MAKE_CHRONOGRAPH

  app_message_register_inbox_received(receive_config_handler);
  app_message_register_inbox_dropped(dropped_config_handler);

#define INBOX_MESSAGE_SIZE 200
#define OUTBOX_MESSAGE_SIZE 50

#ifndef NDEBUG
  uint32_t inbox_max = app_message_inbox_size_maximum();
  uint32_t outbox_max = app_message_outbox_size_maximum();
  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "available message space %u, %u", (unsigned int)inbox_max, (unsigned int)outbox_max);
  if (inbox_max > INBOX_MESSAGE_SIZE) {
    inbox_max = INBOX_MESSAGE_SIZE;
  }
  if (outbox_max > OUTBOX_MESSAGE_SIZE) {
    outbox_max = OUTBOX_MESSAGE_SIZE;
  }
  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "app_message_open(%u, %u)", (unsigned int)inbox_max, (unsigned int)outbox_max);
  AppMessageResult open_result = app_message_open(inbox_max, outbox_max);
  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "open_result = %d", open_result);

#else  // NDEBUG
  app_message_open(INBOX_MESSAGE_SIZE, OUTBOX_MESSAGE_SIZE);
#endif  // NDEBUG

  reset_memory_panic_count();
  create_permanent_objects();
  create_temporal_objects();

  time_t now = time(NULL);
  struct tm *startup_time = localtime(&now);
  compute_hands(startup_time, &current_placement);

  apply_config();
  check_memory_usage();

  AppFocusHandlers focus_handlers;
  memset(&focus_handlers, 0, sizeof(focus_handlers));
  focus_handlers.did_focus = did_focus_handler;
  app_focus_service_subscribe_handlers(focus_handlers);

#ifndef PBL_PLATFORM_APLITE
  health_service_events_subscribe(health_event_handler, NULL);
#endif  // PBL_PLATFORM_APLITE
}

void trigger_memory_panic(int line_number) {
  // Something failed to allocate properly, so we'll set a flag so we
  // can try to clean up unneeded memory.
  qapp_log(APP_LOG_LEVEL_WARNING, __FILE__, __LINE__, "memory_panic at line %d, heap_bytes_free = %d!", line_number, heap_bytes_free());
  memory_panic_flag = true;

  invalidate_clock_face();
}

void reset_memory_panic() {
  // The memory_panic_flag was set, indicating that something
  // somewhere failed to allocate memory.  Destroy and recreate
  // everything, in the hopes this will clear out memory
  // fragmentation.

  memory_panic_flag = false;
  ++memory_panic_count;

  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "reset_memory_panic begin, count = %d", memory_panic_count);

  recreate_all_objects();

  // Start resetting some options if the memory panic count grows too high.
  if (memory_panic_count > 0) {
#ifndef NEVER_KEEP_FACE_ASSET
    keep_face_asset = false;
#endif  // NEVER_KEEP_FACE_ASSET
  }
  if (memory_panic_count > 1) {
#ifndef NEVER_KEEP_ASSETS
    keep_assets = false;
#endif  // NEVER_KEEP_ASSETS
  }
#ifdef SUPPORT_RESOURCE_CACHE
  if (memory_panic_count > 2) {
    second_resource_cache_size = 0;
#ifdef MAKE_CHRONOGRAPH
    chrono_second_resource_cache_size = 0;
#endif  // MAKE_CHRONOGRAPH
  }
#endif  // SUPPORT_RESOURCE_CACHE
  if (memory_panic_count > 3) {
    config.second_hand = false;
  }
  if (memory_panic_count > 4) {
    save_framebuffer = false;
  }
  if (memory_panic_count > 5) {
    config.battery_gauge = IM_off;
    config.bluetooth_indicator = IM_off;
  }
  if (memory_panic_count > 6) {
    config.top_subdial = false;
  }
  if (memory_panic_count > 7) {
    for (int i = 0; i < NUM_DATE_WINDOWS; ++i) {
      if (config.date_windows[i] != DWM_debug_memory_panic_count) {
        config.date_windows[i] = DWM_off;
      }
    }
    hide_date_windows = true;
  }
  if (memory_panic_count > 8) {
    config.chrono_dial = 0;
  }
  if (memory_panic_count > 9) {
    // At this point we hide the clock face.  Drastic!
    hide_clock_face = true;
  }

  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "reset_memory_panic done, count = %d", memory_panic_count);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}
