#include "wright.h"
#include "wright_chrono.h"
#include "hand_table.h"
#include <ctype.h>

#include "../resources/generated_table.c"
#include "../resources/lang_table.c"

bool memory_panic_flag = false;
int memory_panic_count = 0;
GFont fallback_font;

Window *window;

BitmapWithData clock_face;
int face_index = -1;
Layer *clock_face_layer;

BitmapWithData date_window;
BitmapWithData date_window_mask;

// For now, the size of the date window is hardcoded.
#ifdef PBL_ROUND
//const GRect date_window_layer_size = {{ 0, 0 }, { 44, 24 }};
const GRect date_window_box = {
  { 2, 0 }, { 42, 22 }
};
// This is the position to draw the lunar image if it's inverted.
const GRect date_window_box_offset = {
  { -1, 0 }, { 45, 22 }
};

#else  // PBL_ROUND
//const GRect date_window_layer_size = {{ 0, 0 }, { 39, 19 }};
const GRect date_window_box = {
  { 2, 0 }, { 37, 19 }
};
// This is the position to draw the lunar image if it's inverted.
const GRect date_window_box_offset = {
  { -1, 0 }, { 40, 19 }
};
#endif  // PBL_ROUND

// This structure is the data associated with a date window layer.
typedef struct __attribute__((__packed__)) {
  unsigned char date_window_index;
} DateWindowData;

GFont date_numeric_font = NULL;
GFont date_lang_font = NULL;

// This structure specifies how to load, and how to shift each
// different font to appear properly within the date window.
struct FontPlacement {
  unsigned char resource_id;
  signed char vshift;  // Value determined empirically for each font.
};

#define NUM_DATE_LANG_FONTS 9
struct FontPlacement date_lang_font_placement[NUM_DATE_LANG_FONTS] = {
#ifdef PBL_ROUND
  { RESOURCE_ID_DAY_FONT_LATIN_20, -2 },
  { RESOURCE_ID_DAY_FONT_EXTENDED_16, 1 },
  { RESOURCE_ID_DAY_FONT_RTL_16, 1 },
  { RESOURCE_ID_DAY_FONT_ZH_18, -1 },  // Chinese
  { RESOURCE_ID_DAY_FONT_JA_18, -1 },  // Japanese
  { RESOURCE_ID_DAY_FONT_KO_18, -2 },  // Korean
  { RESOURCE_ID_DAY_FONT_TH_18, -1 },  // Thai
  { RESOURCE_ID_DAY_FONT_TA_18, -2 },  // Tamil
  { RESOURCE_ID_DAY_FONT_HI_18, 0 },  // Hindi
#else  // PBL_ROUND
  { RESOURCE_ID_DAY_FONT_LATIN_16, -1 },
  { RESOURCE_ID_DAY_FONT_EXTENDED_14, 1 },
  { RESOURCE_ID_DAY_FONT_RTL_14, 1 },
  { RESOURCE_ID_DAY_FONT_ZH_16, -1 },  // Chinese
  { RESOURCE_ID_DAY_FONT_JA_16, -1 },  // Japanese
  { RESOURCE_ID_DAY_FONT_KO_16, -2 },  // Korean
  { RESOURCE_ID_DAY_FONT_TH_16, -1 },  // Thai
  { RESOURCE_ID_DAY_FONT_TA_16, -2 },  // Tamil
  { RESOURCE_ID_DAY_FONT_HI_16, 0 },  // Hindi
#endif  // PBL_ROUND
};

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

// Triggered at regular intervals to implement sweep seconds.
AppTimer *sweep_timer = NULL;
int sweep_timer_ms = 1000;

int sweep_seconds_ms = 60 * 1000 / NUM_STEPS_SECOND;

Layer *clock_hands_layer;

struct HandCache hour_cache;
struct HandCache minute_cache;
struct HandCache second_cache;

struct ResourceCache hour_resource_cache[HOUR_RESOURCE_CACHE_SIZE + HOUR_MASK_RESOURCE_CACHE_SIZE];
struct ResourceCache minute_resource_cache[MINUTE_RESOURCE_CACHE_SIZE + MINUTE_MASK_RESOURCE_CACHE_SIZE];
struct ResourceCache second_resource_cache[SECOND_RESOURCE_CACHE_SIZE + SECOND_MASK_RESOURCE_CACHE_SIZE];
size_t hour_resource_cache_size = HOUR_RESOURCE_CACHE_SIZE + HOUR_MASK_RESOURCE_CACHE_SIZE;
size_t minute_resource_cache_size = MINUTE_RESOURCE_CACHE_SIZE + MINUTE_MASK_RESOURCE_CACHE_SIZE;
size_t second_resource_cache_size = SECOND_RESOURCE_CACHE_SIZE + SECOND_MASK_RESOURCE_CACHE_SIZE;

struct HandPlacement current_placement;

#ifdef PBL_PLATFORM_APLITE
// In Aplite, we have to decide carefully what compositing mode to
// draw the hands.
DrawModeTable draw_mode_table[2] = {
  { GCompOpClear, GCompOpOr, GCompOpAssign, { GColorClearInit, GColorBlackInit, GColorWhiteInit } },
  { GCompOpOr, GCompOpClear, GCompOpAssignInverted, { GColorClearInit, GColorWhiteInit, GColorBlackInit } },
};

#else  // PBL_PLATFORM_APLITE

// In Basalt, we always use GCompOpSet to draw the hands, because we
// always use the alpha channel.  The exception is paint_assign,
// because assign means assign.
DrawModeTable draw_mode_table[2] = {
  { GCompOpSet, GCompOpSet, GCompOpAssign, { GColorClearInit, GColorBlackInit, GColorWhiteInit } },
  { GCompOpSet, GCompOpSet, GCompOpAssign, { GColorClearInit, GColorWhiteInit, GColorBlackInit } },
};

#endif  // PBL_PLATFORM_APLITE

void destroy_objects();
void create_objects();
void draw_full_date_window(GContext *ctx, int date_window_index);

// Loads a font from the resource and returns it.  It may return
// either the intended font, or the fallback font.  If it returns
// the fallback font, this function automatically triggers a memory
// panic alert.
GFont safe_load_custom_font(int resource_id) {
  ResHandle resource = resource_get_handle(resource_id);
  GFont font = fonts_load_custom_font(resource);
  if (font == fallback_font) {
    app_log(APP_LOG_LEVEL_WARNING, __FILE__, __LINE__, "font %d failed to load", resource_id);
    trigger_memory_panic(__LINE__);
  }
  app_log(APP_LOG_LEVEL_WARNING, __FILE__, __LINE__, "loaded font %d as %p", resource_id, font);
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
  
// Determines the specific hand bitmaps that should be displayed based
// on the current time.
void compute_hands(struct tm *time, struct HandPlacement *placement) {
  // Get the Unix time (in UTC).
  time_t gmt;
  uint16_t t_ms;
  time_ms(&gmt, &t_ms);

  // Compute the number of milliseconds elapsed since midnight, local time.
  struct tm *tm = localtime(&gmt);
  unsigned int s = (unsigned int)(tm->tm_hour * 60 + tm->tm_min) * 60 + tm->tm_sec;
  unsigned int ms = (unsigned int)(s * 1000 + t_ms);

#ifdef MAKE_CHRONOGRAPH
  // For the chronograph, compute the number of milliseconds elapsed
  // since midnight, UTC.
  unsigned int ms_utc = (unsigned int)((gmt % SECONDS_PER_DAY) * 1000 + t_ms);
#endif  // MAKE_CHRONOGRAPH
  
#ifdef FAST_TIME
  if (time != NULL) {
    time->tm_wday = (s / 3) % 7;
    time->tm_mon = (s / 4) % 12;
    time->tm_mday = (s % 31) + 1;
    time->tm_hour = s % 24;
  }
  ms *= 67;
#ifdef MAKE_CHRONOGRAPH
  ms_utc *= 67;
#endif  // MAKE_CHRONOGRAPH
#endif  // FAST_TIME

#ifdef SCREENSHOT_BUILD
  // Freeze the time to 10:09 for screenshots.
  {
    ms = ((10*60 + 9)*60 + 36) * 1000;  // 10:09:36
    if (time != NULL) {
      time->tm_wday = 3;
      time->tm_mday = 9;
      time->tm_mon = 6;
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
  if (time != NULL) {
    placement->day_index = time->tm_wday;
    placement->month_index = time->tm_mon;
    placement->date_value = time->tm_mday;
    placement->year_value = time->tm_year;
    placement->ampm_value = (time->tm_hour >= 12);

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

#ifdef FAST_TIME
      // One subdial shift every 10 seconds.
      lunar_age_s = (s * 255144) / NUM_STEPS_MOON;
#endif  // FAST_TIME

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

  placement->hour_buzzer = (ms / (SECONDS_PER_HOUR * 1000)) % 24;

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

#ifndef PBL_PLATFORM_APLITE
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
#endif  // PBL_PLATFORM_APLITE

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

  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "flip_bitmap_x, width_bytes = %d, stride=%d", width_bytes, stride);

  uint8_t *data = gbitmap_get_data(image);

  for (int y = 0; y < height; ++y) {
#ifdef PBL_SDK_2
    uint8_t *row = data + y * stride;
#else
    // Get the min and max x values for this row
    GBitmapDataRowInfo info = gbitmap_get_data_row_info(image, y);
    uint8_t *row = &info.data[info.min_x];
    width = info.max_x - info.min_x + 1;
    assert(width % pixels_per_byte == 0);
    int width_bytes = width / pixels_per_byte;
#endif  // PBL_SDK_2

    switch (pixels_per_byte) {
    case 8:
      for (int x1 = (width_bytes - 1) / 2; x1 >= 0; --x1) {
        int x2 = width_bytes - 1 - x1;
        uint8_t b = reverse_bits(row[x1]);
        row[x1] = reverse_bits(row[x2]);
        row[x2] = b;
      }
      break;

#ifndef PBL_PLATFORM_APLITE
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
#endif  // PBL_PLATFORM_APLITE
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

#ifdef PBL_SDK_2
    uint8_t *row1 = data + y1 * stride;
    uint8_t *row2 = data + y2 * stride;
#else
    // Get the min and max x values for this row
    GBitmapDataRowInfo info1 = gbitmap_get_data_row_info(image, y1);
    GBitmapDataRowInfo info2 = gbitmap_get_data_row_info(image, y2);
    int width1 = info1.max_x - info1.min_x + 1;
    int width2 = info2.max_x - info2.min_x + 1;
    assert(width1 == width2);  // We hope the bitmap is vertically symmetric
    uint8_t *row1 = &info1.data[info1.min_x];
    uint8_t *row2 = &info2.data[info2.min_x];
    width_bytes = width1 / pixels_per_byte;
#endif  // PBL_SDK_2
    
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

#ifdef PBL_PLATFORM_APLITE
  GColor color = draw_mode_table[config.draw_mode ^ APLITE_INVERT].colors[2];
    
#else  // PBL_PLATFORM_APLITE
  // On Basalt, draw lines using the indicated color channel.
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
#endif  // PBL_PLATFORM_APLITE
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
void draw_bitmap_hand_mask(struct HandCache *hand_cache, struct ResourceCache *resource_cache, size_t resource_cache_size, struct HandDef *hand_def, int hand_index, bool no_basalt_mask, GContext *ctx) {
  struct BitmapHandTableRow *hand = &hand_def->bitmap_table[hand_index];
  int bitmap_index = hand->bitmap_index;
  struct BitmapHandCenterRow *lookup = &hand_def->bitmap_centers[bitmap_index];

  int hand_resource_id = hand_def->resource_id + bitmap_index;
  int hand_resource_mask_id = hand_def->resource_mask_id + bitmap_index;

#ifdef PBL_PLATFORM_APLITE
  if (hand_def->resource_id == hand_def->resource_mask_id)
#else
  if (no_basalt_mask || hand_def->resource_id == hand_def->resource_mask_id)
#endif  // PBL_PLATFORM_APLITE
  {
    // The draw-without-a-mask case.  Do nothing here.
  } else {
    // The hand has a mask, so use it to draw the hand opaquely.
    if (hand_cache->image.bitmap == NULL) {
      if (hand_def->use_rle) {
        hand_cache->image = rle_bwd_create_with_cache(hand_def->resource_id, hand_resource_id, resource_cache, resource_cache_size);
        hand_cache->mask = rle_bwd_create_with_cache(hand_def->resource_id, hand_resource_mask_id, resource_cache, resource_cache_size);
      } else {
        hand_cache->image = png_bwd_create_with_cache(hand_def->resource_id, hand_resource_id, resource_cache, resource_cache_size);
        hand_cache->mask = png_bwd_create_with_cache(hand_def->resource_id, hand_resource_mask_id, resource_cache, resource_cache_size);
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

    graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode ^ APLITE_INVERT].paint_fg);
    graphics_draw_bitmap_in_rect(ctx, hand_cache->mask.bitmap, destination);
  }
}

// Draws a given hand on the face, using the bitmap structures.  You
// must have already called draw_bitmap_hand_mask().
void draw_bitmap_hand_fg(struct HandCache *hand_cache, struct ResourceCache *resource_cache, size_t resource_cache_size, struct HandDef *hand_def, int hand_index, bool no_basalt_mask, GContext *ctx) {
  struct BitmapHandTableRow *hand = &hand_def->bitmap_table[hand_index];
  int bitmap_index = hand->bitmap_index;
  struct BitmapHandCenterRow *lookup = &hand_def->bitmap_centers[bitmap_index];

  int hand_resource_id = hand_def->resource_id + bitmap_index;

#ifdef PBL_PLATFORM_APLITE
  if (hand_def->resource_id == hand_def->resource_mask_id)
#else
  if (no_basalt_mask || hand_def->resource_id == hand_def->resource_mask_id)
#endif  // PBL_PLATFORM_APLITE
  {
    // The hand does not have a mask.  Draw the hand on top of the scene.
    if (hand_cache->image.bitmap == NULL) {
      // All right, load it from the resource file.
      if (hand_def->use_rle) {
        hand_cache->image = rle_bwd_create_with_cache(hand_def->resource_id, hand_resource_id, resource_cache, resource_cache_size);
      } else {
        hand_cache->image = png_bwd_create_with_cache(hand_def->resource_id, hand_resource_id, resource_cache, resource_cache_size);
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
    graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode ^ APLITE_INVERT].paint_bg);
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
    
    graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode ^ APLITE_INVERT].paint_bg);
    graphics_draw_bitmap_in_rect(ctx, hand_cache->image.bitmap, destination);
  }
}

// In general, prepares a hand for being drawn.  Specifically, this
// clears the background behind a hand, if necessary.
void draw_hand_mask(struct HandCache *hand_cache, struct ResourceCache *resource_cache, size_t resource_cache_size, struct HandDef *hand_def, int hand_index, bool no_basalt_mask, GContext *ctx) {
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

    draw_bitmap_hand_mask(hand_cache, resource_cache, resource_cache_size, hand_def, hand_index, no_basalt_mask, ctx);
  }
}

// Draws a given hand on the face, after draw_hand_mask(), using the
// vector and/or bitmap structures.  A given hand may be represented
// by a bitmap or a vector, or a combination of both.
void draw_hand_fg(struct HandCache *hand_cache, struct ResourceCache *resource_cache, size_t resource_cache_size, struct HandDef *hand_def, int hand_index, bool no_basalt_mask, GContext *ctx) {
  if (hand_def->vector_hand != NULL) {
    draw_vector_hand(hand_cache, hand_def, hand_index, ctx);
  }

  if (hand_def->bitmap_table != NULL) {
    draw_bitmap_hand_fg(hand_cache, resource_cache, resource_cache_size, hand_def, hand_index, no_basalt_mask, ctx);
  }
}

void draw_hand(struct HandCache *hand_cache, struct ResourceCache *resource_cache, size_t resource_cache_size, struct HandDef *hand_def, int hand_index, GContext *ctx) {
  draw_hand_mask(hand_cache, resource_cache, resource_cache_size, hand_def, hand_index, true, ctx);
  draw_hand_fg(hand_cache, resource_cache, resource_cache_size, hand_def, hand_index, true, ctx);
}

// Applies the appropriate Basalt color-remapping according to the
// selected color mode, for the indicated clock-face or clock-hands
// bitmap.
void remap_colors_clock(BitmapWithData *bwd) {
#ifndef PBL_PLATFORM_APLITE
  struct FaceColorDef *cd = &clock_face_color_table[config.color_mode];
  bwd_remap_colors(bwd, (GColor8){.argb=cd->cb_argb8}, (GColor8){.argb=cd->c1_argb8}, (GColor8){.argb=cd->c2_argb8}, (GColor8){.argb=cd->c3_argb8}, config.draw_mode);
#endif  // PBL_PLATFORM_APLITE
}

// Applies the appropriate Basalt color-remapping according to the
// selected color mode, for the indicated date-window bitmap.
static void remap_colors_date(BitmapWithData *bwd) {
#ifndef PBL_PLATFORM_APLITE
  struct FaceColorDef *cd = &clock_face_color_table[config.color_mode];
  GColor bg, fg;
  bg.argb = cd->db_argb8;
  fg.argb = cd->d1_argb8;

  bwd_remap_colors(bwd, bg, fg, GColorBlack, GColorWhite, config.draw_mode);
#endif  // PBL_PLATFORM_APLITE
}

// Applies the appropriate Basalt color-remapping according to the
// selected color mode, for the indicated lunar bitmap.
static void remap_colors_moon(BitmapWithData *bwd) {
#ifndef PBL_PLATFORM_APLITE
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
#endif  // PBL_PLATFORM_APLITE
}

#ifdef TOP_SUBDIAL
// Draws a special moon subdial window that shows the lunar phase in more detail.
void draw_moon_phase_subdial(Layer *me, GContext *ctx, bool invert) {
  // The draw_mode is the color to draw the frame of the subdial.
  unsigned int draw_mode = invert ^ config.draw_mode ^ APLITE_INVERT;

  // The moon_draw_mode is the color to draw the moon within the subdial.
  unsigned int moon_draw_mode = draw_mode;
  if (config.lunar_background) {
    // If the user specified an always-black background, that means moon_draw_mode is always 1.
    moon_draw_mode = 1;
  }

  const struct IndicatorTable *window = &top_subdial[config.face_index];
  GRect destination = GRect(window->x, window->y, 80, 41);
  
  // First draw the subdial details (including the background).
#ifdef PBL_PLATFORM_APLITE
  BitmapWithData top_subdial_mask;
  top_subdial_mask = rle_bwd_create(RESOURCE_ID_TOP_SUBDIAL_MASK);
  if (top_subdial_mask.bitmap == NULL) {
    trigger_memory_panic(__LINE__);
    return;
  }
  graphics_context_set_compositing_mode(ctx, draw_mode_table[moon_draw_mode].paint_bg);
  graphics_draw_bitmap_in_rect(ctx, top_subdial_mask.bitmap, destination);
  bwd_destroy(&top_subdial_mask);
#endif  // PBL_PLATFORM_APLITE
  
  BitmapWithData top_subdial_bitmap;
  top_subdial_bitmap = rle_bwd_create(RESOURCE_ID_TOP_SUBDIAL);
  if (top_subdial_bitmap.bitmap == NULL) {
    trigger_memory_panic(__LINE__);
    return;
  }
#ifndef PBL_PLATFORM_APLITE
  remap_colors_date(&top_subdial_bitmap);
#endif  // PBL_PLATFORM_APLITE
  
  graphics_context_set_compositing_mode(ctx, draw_mode_table[draw_mode].paint_fg);
  graphics_draw_bitmap_in_rect(ctx, top_subdial_bitmap.bitmap, destination);
  bwd_destroy(&top_subdial_bitmap);

  // Now draw the moon wheel.

  BitmapWithData moon_wheel_bitmap;
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
    
#ifdef PBL_PLATFORM_APLITE
  // On Aplite, we load either "black" or "white" icons, according
  // to what color we need the background to be.
  if (moon_draw_mode == 0) {
    moon_wheel_bitmap = rle_bwd_create(RESOURCE_ID_MOON_WHEEL_WHITE_0 + index);
  } else {
    moon_wheel_bitmap = rle_bwd_create(RESOURCE_ID_MOON_WHEEL_BLACK_0 + index);
  }
#else  // PBL_PLATFORM_APLITE
  // On Basalt, we only use the "black" icons, and we remap the colors at load time.
  moon_wheel_bitmap = rle_bwd_create(RESOURCE_ID_MOON_WHEEL_BLACK_0 + index);
  remap_colors_moon(&moon_wheel_bitmap);
#endif  // PBL_PLATFORM_APLITE
  if (moon_wheel_bitmap.bitmap == NULL) {
    trigger_memory_panic(__LINE__);
    return;
  }
  
  // In the Aplite case, we draw the moon in the fg color.  This will
  // be black-on-white if moon_draw_mode = 0, or white-on-black if
  // moon_draw_mode = 1.  Since we have selected the particular moon
  // resource above based on draw_mode, we will always draw the moon
  // in the correct color, so that it looks like the moon.  (Drawing
  // the moon in the inverted color would look weird.)

  // In the Basalt case, the only difference between moon_black and
  // moon_white is the background color; in either case we draw them
  // both in GCompOpSet.
  graphics_context_set_compositing_mode(ctx, draw_mode_table[moon_draw_mode].paint_fg);
  //graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  graphics_draw_bitmap_in_rect(ctx, moon_wheel_bitmap.bitmap, destination);
  bwd_destroy(&moon_wheel_bitmap);
}
#endif  // TOP_SUBDIAL

void draw_clock_face(Layer *me, GContext *ctx) {
  // Load the clock face from the resource file if we haven't already.
  BitmapWithData face_bitmap;
  
  face_bitmap = rle_bwd_create(clock_face_table[config.face_index].resource_id);
  if (face_bitmap.bitmap == NULL) {
    trigger_memory_panic(__LINE__);
    return;
  }
  remap_colors_clock(&face_bitmap);

  // Draw the clock face into the layer.
  GRect destination = layer_get_bounds(me);
  destination.origin.x = 0;
  destination.origin.y = 0;
  graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode ^ APLITE_INVERT].paint_assign);
  graphics_draw_bitmap_in_rect(ctx, face_bitmap.bitmap, destination);
  bwd_destroy(&face_bitmap);

  // Draw the top subdial if enabled.
  {
    const struct IndicatorTable *window = &top_subdial[config.face_index];
  
    switch (config.top_subdial) {
    case TSM_off:
    break;
    
    case TSM_moon_phase:
      draw_moon_phase_subdial(me, ctx, window->invert);
      break;
    }
  }

  // Draw the date windows.
  {
    for (int i = 0; i < NUM_DATE_WINDOWS; ++i) {
      draw_full_date_window(ctx, i);
    }
    bwd_destroy(&date_window);
    bwd_destroy(&date_window_mask);
  }
}

void clock_face_layer_update_callback(Layer *me, GContext *ctx) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "clock_face_layer, memory_panic_count = %d", memory_panic_count);
  if (memory_panic_count > 9) {
    // In case we're in extreme memory panic mode--too little
    // available memory to even keep the clock face resident--we do
    // nothing in this function.
    return;
  }

  if (clock_face.bitmap == NULL) {
    // We haven't drawn the face yet in its current form.  Draw the
    // clock face into the frame buffer.
    draw_clock_face(me, ctx);

    // Now save the render for next time.
    GBitmap *fb = graphics_capture_frame_buffer(ctx);
    clock_face = bwd_copy_bitmap(fb);
    graphics_release_frame_buffer(ctx, fb);

  } else {
    // The clock_face render is already saved from a previous update;
    // draw it now.
    GRect destination = layer_get_bounds(me);
    destination.origin.x = 0;
    destination.origin.y = 0;
    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
    graphics_draw_bitmap_in_rect(ctx, clock_face.bitmap, destination);
  }
}
  
void clock_hands_layer_update_callback(Layer *me, GContext *ctx) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "clock_hands_layer");

#ifdef MAKE_CHRONOGRAPH

  // A special case for Chronograph support.  All six hands are drawn
  // individually, in a specific order (that's slightly different from
  // the non-chrono order--we draw the three subdials first, and this
  // includes the normal second hand).
  if (config.second_hand || chrono_data.running || chrono_data.hold_ms != 0) {
    draw_hand(&chrono_minute_cache, chrono_minute_resource_cache, chrono_minute_resource_cache_size, &chrono_minute_hand_def, current_placement.chrono_minute_hand_index, ctx);
  }

  if (config.chrono_dial != CDM_off) {
    if (config.second_hand || chrono_data.running || chrono_data.hold_ms != 0) {
      draw_hand(&chrono_tenth_cache, chrono_tenth_resource_cache, chrono_tenth_resource_cache_size, &chrono_tenth_hand_def, current_placement.chrono_tenth_hand_index, ctx);
    }
  }

  if (config.second_hand) {
    draw_hand(&second_cache, second_resource_cache, second_resource_cache_size, &second_hand_def, current_placement.second_hand_index, ctx);
  }

  draw_hand(&hour_cache, hour_resource_cache, hour_resource_cache_size, &hour_hand_def, current_placement.hour_hand_index, ctx);

  draw_hand(&minute_cache, minute_resource_cache, minute_resource_cache_size, &minute_hand_def, current_placement.minute_hand_index, ctx);

  if (config.second_hand || chrono_data.running || chrono_data.hold_ms != 0) {
    draw_hand(&chrono_second_cache, chrono_second_resource_cache, chrono_second_resource_cache_size, &chrono_second_hand_def, current_placement.chrono_second_hand_index, ctx);
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
  // hands, and we don't want the hands to erase each other.
  draw_hand_mask(&hour_cache, hour_resource_cache, hour_resource_cache_size, &hour_hand_def, current_placement.hour_hand_index, false, ctx);
  draw_hand_mask(&minute_cache, minute_resource_cache, minute_resource_cache_size, &minute_hand_def, current_placement.minute_hand_index, false, ctx);

  draw_hand_fg(&hour_cache, hour_resource_cache, hour_resource_cache_size, &hour_hand_def, current_placement.hour_hand_index, false, ctx);
  draw_hand_fg(&minute_cache, minute_resource_cache, minute_resource_cache_size, &minute_hand_def, current_placement.minute_hand_index, false, ctx);

#else  //  HOUR_MINUTE_OVERLAP

  // Draw the hour and minute hands separately.  All of the other
  // Rosewrights share this property, because their hands are wide,
  // with complex interiors that must be erased; and their haloes (if
  // present) are comparatively thinner and don't threaten to erase
  // overlapping hands.
  draw_hand(&hour_cache, hour_resource_cache, hour_resource_cache_size, &hour_hand_def, current_placement.hour_hand_index, ctx);

  draw_hand(&minute_cache, minute_resource_cache, minute_resource_cache_size, &minute_hand_def, current_placement.minute_hand_index, ctx);
#endif  //  HOUR_MINUTE_OVERLAP

  if (config.second_hand) {
    draw_hand(&second_cache, second_resource_cache, second_resource_cache_size, &second_hand_def, current_placement.second_hand_index, ctx);
  }
  
#endif  // MAKE_CHRONOGRAPH
}

// Draws the frame and optionally fills the background of the current date window.
void draw_date_window_background(GContext *ctx, int date_window_index, unsigned int fg_draw_mode, unsigned int bg_draw_mode) {
  const struct IndicatorTable *window = &date_windows[date_window_index][config.face_index];
  GRect box = GRect(window->x, window->y, date_window_box.size.w, date_window_box.size.h);

#ifdef PBL_PLATFORM_APLITE
  // We only need the mask on Aplite.
  if (date_window_mask.bitmap == NULL) {
    date_window_mask = rle_bwd_create(RESOURCE_ID_DATE_WINDOW_MASK);
    if (date_window_mask.bitmap == NULL) {
      trigger_memory_panic(__LINE__);
      return;
    }
  }
  graphics_context_set_compositing_mode(ctx, draw_mode_table[bg_draw_mode].paint_bg);
  graphics_draw_bitmap_in_rect(ctx, date_window_mask.bitmap, box);
#endif  // PBL_PLATFORM_APLITE
  
  if (date_window.bitmap == NULL) {
    date_window = rle_bwd_create(RESOURCE_ID_DATE_WINDOW);
    if (date_window.bitmap == NULL) {
      bwd_destroy(&date_window_mask);
      trigger_memory_panic(__LINE__);
      return;
    }
#ifndef PBL_PLATFORM_APLITE
    remap_colors_date(&date_window);
#endif  // PBL_PLATFORM_APLITE
  }
  
  graphics_context_set_compositing_mode(ctx, draw_mode_table[fg_draw_mode].paint_fg);
  graphics_draw_bitmap_in_rect(ctx, date_window.bitmap, box);
}

// Draws a date window with the specified text contents.  Usually this is
// something like a numeric date or the weekday name.
void draw_window(GContext *ctx, int date_window_index, const char *text, struct FontPlacement *font_placement, 
		 GFont *font, bool invert) {
  const struct IndicatorTable *window = &date_windows[date_window_index][config.face_index];
  GRect box = GRect(window->x, window->y, date_window_box.size.w, date_window_box.size.h);

  unsigned int draw_mode = invert ^ config.draw_mode ^ APLITE_INVERT;
  draw_date_window_background(ctx, date_window_index, draw_mode, draw_mode);

#ifdef PBL_PLATFORM_APLITE
  graphics_context_set_text_color(ctx, draw_mode_table[draw_mode].colors[1]);
#else
  struct FaceColorDef *cd = &clock_face_color_table[config.color_mode];
  GColor fg;
  fg.argb = cd->d1_argb8;
  if (config.draw_mode) {
    fg.argb ^= 0x3f;
  }
  graphics_context_set_text_color(ctx, fg);
#endif  // PBL_PLATFORM_APLITE

  box.origin.y += font_placement->vshift;

  // The Pebble text routines seem to be a bit too conservative when
  // deciding whether a given bit of text will fit within its assigned
  // box, meaning the text is likely to be trimmed even if it would
  // have fit.  We avoid this problem by cheating and expanding the
  // box a bit wider and taller than we actually intend it to be.
  box.origin.x -= 4;
  box.size.w += 8;
  box.size.h += 4;

  if ((*font) == NULL) {
    (*font) = safe_load_custom_font(font_placement->resource_id);
  }

  graphics_draw_text(ctx, text, (*font), box,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}

// Draws the background and contents of the specified date window.
void draw_full_date_window(GContext *ctx, int date_window_index) {
  //  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "draw_full_date_window %c", date_window_index + 'a');

  DateWindowMode dwm = config.date_windows[date_window_index];
  if (dwm == DWM_off) {
    // Do nothing.
    return;
  }

  const struct IndicatorTable *window = &date_windows[date_window_index][config.face_index];

  // Format the date or weekday or whatever text for display.
#define DATE_WINDOW_BUFFER_SIZE 16
  char buffer[DATE_WINDOW_BUFFER_SIZE];

  GFont *font = &date_numeric_font;
  struct FontPlacement *font_placement = &date_lang_font_placement[0];
  if (dwm >= DWM_weekday && dwm <= DWM_ampm) {
    // Draw text using date_lang_font.
    const LangDef *lang = &lang_table[config.display_lang];
    font = &date_lang_font;
    font_placement = &date_lang_font_placement[lang->font_index];
  }

  char *text = buffer;

  switch (dwm) {
  case DWM_identify:
    snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%c", toupper((int)(DATE_WINDOW_KEYS[date_window_index])));
    break;

  case DWM_date:
    snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%d", current_placement.date_value);
    break;

  case DWM_year:
    snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%d", current_placement.year_value + 1900);
    break;

  case DWM_debug_heap_free:
    snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%dk", heap_bytes_free() / 1024);
    break;

  case DWM_debug_memory_panic_count:
    snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%d!", memory_panic_count);
    break;

  case DWM_debug_resource_reads:
    snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%d", bwd_resource_reads);
    break;

#ifdef SUPPORT_RESOURCE_CACHE
  case DWM_debug_cache_hits:
    snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%d", bwd_cache_hits);
    break;

  case DWM_debug_cache_total_size:
    snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%dk", bwd_cache_total_size / 1024);
    break;
#endif  // SUPPORT_RESOURCE_CACHE
    
  case DWM_weekday:
    text = date_names[current_placement.day_index];
    break;
    
  case DWM_month:
    text = date_names[current_placement.month_index + NUM_WEEKDAY_NAMES];
    break;

  case DWM_ampm:
    text = date_names[current_placement.ampm_value + NUM_WEEKDAY_NAMES + NUM_MONTH_NAMES];
    break;

  case DWM_moon_unused:
  default:
    buffer[0] = '\0';
  }

  draw_window(ctx, date_window_index, text, font_placement, font, window->invert);
}

// Called once per epoch (e.g. once per second, or once per minute) to
// compute the new positions for all of the hands on the watch based
// on the current time.  This does not actually draw the hands; it
// only computes which position each hand should hold, and it marks
// the appropriate layers dirty, to eventually redraw the hands that
// have moved since the last call.
void update_hands(struct tm *time) {
  struct HandPlacement new_placement = current_placement;

  compute_hands(time, &new_placement);
  if (new_placement.hour_hand_index != current_placement.hour_hand_index) {
    current_placement.hour_hand_index = new_placement.hour_hand_index;
    layer_mark_dirty(clock_hands_layer);
  }

  if (new_placement.minute_hand_index != current_placement.minute_hand_index) {
    current_placement.minute_hand_index = new_placement.minute_hand_index;
    layer_mark_dirty(clock_hands_layer);
  }

  if (new_placement.second_hand_index != current_placement.second_hand_index) {
    current_placement.second_hand_index = new_placement.second_hand_index;
    layer_mark_dirty(clock_hands_layer);
  }

  if (new_placement.hour_buzzer != current_placement.hour_buzzer) {
    current_placement.hour_buzzer = new_placement.hour_buzzer;
    if (config.hour_buzzer) {
      // The hour has changed; ring the buzzer if it's enabled.
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

    // Shorthand for the below for loop.  This achieves the same thing.
    layer_mark_dirty(clock_face_layer);
    bwd_destroy(&clock_face);
  }

#ifdef TOP_SUBDIAL
  // And the lunar index in the moon subdial.
  if (new_placement.lunar_index != current_placement.lunar_index) {
    current_placement.lunar_index = new_placement.lunar_index;
    layer_mark_dirty(clock_face_layer);
    bwd_destroy(&clock_face);
  }
#endif  // TOP_SUBDIAL
}

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

// Reset the sweep_timer according to the configured settings.  If
// sweep_seconds is enabled, this sets the sweep_timer to wake us up
// at the next sub-second interval.  If sweep_seconds is not enabled,
// the sweep_timer is not used.
void reset_sweep() {
  if (sweep_timer != NULL) {
    app_timer_cancel(sweep_timer);
    sweep_timer = NULL;
  }
  if (sweep_timer_ms < 1000) {
    sweep_timer = app_timer_register(sweep_timer_ms, &handle_sweep, 0);
  }
}

// The callback on the per-second (or per-minute) system timer that
// handles most mundane tasks.
void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  if (memory_panic_flag) {
    reset_memory_panic();
  }
  update_hands(tick_time);
  reset_sweep();
}

void window_load_handler(struct Window *window) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "main window loads");
}

void window_appear_handler(struct Window *window) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "main window appears");

#ifdef SCREENSHOT_BUILD
  config_set_click_config(window);
#elif defined(MAKE_CHRONOGRAPH)
  chrono_set_click_config(window);
#endif  // MAKE_CHRONOGRAPH
}

void window_disappear_handler(struct Window *window) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "main window disappears");
}

void window_unload_handler(struct Window *window) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "main window unloads");
}


// Sets the tick timer to fire at the appropriate rate based on current conditions.
void reset_tick_timer() {
  tick_timer_service_unsubscribe();

#if defined(FAST_TIME) || defined(BATTERY_HACK)
  tick_timer_service_subscribe(SECOND_UNIT, handle_tick);

#elif defined(MAKE_CHRONOGRAPH)
  if (config.second_hand || chrono_data.running) {
    tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
  } else {
    tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
  }

  reset_chrono_digital_timer();

#else
  if (config.second_hand) {
    tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
  } else {
    tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
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

void move_layers() {
  // Move any subordinate layers to their correct position on the face.

  // The indicator_face_index is like config.face_index, but also
  // includes the top_subdial setting--having top_subdial enabled is
  // like a separate face for this purpose, which allows us to
  // reposition the indicators around the subdial when necessary.
#ifdef TOP_SUBDIAL
  int indicator_face_index = config.face_index * 2 + (config.top_subdial != TSM_off);
#else  // TOP_SUBDIAL
  int indicator_face_index = config.face_index;
#endif  // TOP_SUBDIAL
  assert(indicator_face_index < NUM_INDICATOR_FACES);
  {
    const struct IndicatorTable *window = &battery_table[indicator_face_index];
    move_battery_gauge(window->x, window->y, window->invert);
  }
  {
    const struct IndicatorTable *window = &bluetooth_table[indicator_face_index];
    move_bluetooth_indicator(window->x, window->y, window->invert);
  }
}

// Updates any runtime settings as needed when the config changes.
void apply_config() {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "apply_config");

  // Reset the memory panic count when we get a new config setting.
  // Maybe the user knows what he's doing.
  memory_panic_count = 0;

  if (face_index != config.face_index) {
    // Update the face bitmap if it's changed.
    face_index = config.face_index;
    bwd_destroy(&clock_face);
    move_layers();
  }

  if (display_lang != config.display_lang) {
    // Unload the day font if it changes with the language.
    if (date_lang_font != NULL && (display_lang == -1 || lang_table[display_lang].font_index != lang_table[config.display_lang].font_index)) {
      app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "apply_config unload date_lang_font %p", date_lang_font);
      safe_unload_custom_font(&date_lang_font);
    }

    // Reload the weekday, month, and ampm names from the appropriate
    // language resource.
    fill_date_names(date_names, NUM_DATE_NAMES, date_names_buffer, DATE_NAMES_MAX_BUFFER, lang_table[config.display_lang].date_name_id);

    display_lang = config.display_lang;
  }

  // Reload all bitmaps just for good measure.  Maybe the user changed
  // the draw mode or something else.
  destroy_objects();
  create_objects();

  layer_mark_dirty(clock_face_layer);

  reset_tick_timer();
}

// Creates all of the objects needed for the watch.  Normally called
// only by handle_init(), but might be invoked midstream in a
// memory-panic situation.
void create_objects() {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "create_objects");
  window = window_create();
  assert(window != NULL);

  struct WindowHandlers window_handlers;
  memset(&window_handlers, 0, sizeof(window_handlers));
  window_handlers.load = window_load_handler;
  window_handlers.appear = window_appear_handler;
  window_handlers.disappear = window_disappear_handler;
  window_handlers.unload = window_unload_handler;
  window_set_window_handlers(window, window_handlers);

#ifdef PBL_PLATFORM_APLITE
  window_set_fullscreen(window, true);
#endif  //  PBL_PLATFORM_APLITE
  window_stack_push(window, false);

  hand_cache_init(&hour_cache);
  hand_cache_init(&minute_cache);
  hand_cache_init(&second_cache);

  Layer *window_layer = window_get_root_layer(window);
  GRect window_frame = layer_get_frame(window_layer);

  clock_face_layer = layer_create(window_frame);
  assert(clock_face_layer != NULL);
  layer_set_update_proc(clock_face_layer, &clock_face_layer_update_callback);
  layer_add_child(window_layer, clock_face_layer);
  
  init_battery_gauge(window_layer);
  init_bluetooth_indicator(window_layer);

  // Now put all of the layers we just created into their correct
  // positions.
  move_layers();

#ifdef MAKE_CHRONOGRAPH
  create_chrono_objects();
#endif  // MAKE_CHRONOGRAPH

  clock_hands_layer = layer_create(window_frame);
  assert(clock_hands_layer != NULL);
  layer_set_update_proc(clock_hands_layer, &clock_hands_layer_update_callback);
  layer_add_child(window_layer, clock_hands_layer);
}

// Destroys the objects created by create_objects().
void destroy_objects() {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "destroy_objects");
  window_stack_pop_all(false);
  layer_destroy(clock_face_layer);
  clock_face_layer = NULL;
  bwd_destroy(&clock_face);
  face_index = -1;

#ifdef MAKE_CHRONOGRAPH
  destroy_chrono_objects();
#endif  // MAKE_CHRONOGRAPH

  deinit_battery_gauge();
  deinit_bluetooth_indicator();

  bwd_clear_cache(hour_resource_cache, HOUR_RESOURCE_CACHE_SIZE + HOUR_MASK_RESOURCE_CACHE_SIZE);
  bwd_clear_cache(minute_resource_cache, MINUTE_RESOURCE_CACHE_SIZE + MINUTE_MASK_RESOURCE_CACHE_SIZE);
  bwd_clear_cache(second_resource_cache, SECOND_RESOURCE_CACHE_SIZE + SECOND_MASK_RESOURCE_CACHE_SIZE);

#ifdef MAKE_CHRONOGRAPH
  bwd_clear_cache(chrono_minute_resource_cache, CHRONO_MINUTE_RESOURCE_CACHE_SIZE + CHRONO_MINUTE_MASK_RESOURCE_CACHE_SIZE);
  bwd_clear_cache(chrono_second_resource_cache, CHRONO_SECOND_RESOURCE_CACHE_SIZE + CHRONO_SECOND_MASK_RESOURCE_CACHE_SIZE);
  bwd_clear_cache(chrono_tenth_resource_cache, CHRONO_TENTH_RESOURCE_CACHE_SIZE + CHRONO_TENTH_MASK_RESOURCE_CACHE_SIZE);
#endif  // MAKE_CHRONOGRAPH
  
  layer_destroy(clock_hands_layer);

  hand_cache_destroy(&hour_cache);
  hand_cache_destroy(&minute_cache);
  hand_cache_destroy(&second_cache);

  if (date_lang_font != NULL) {
    safe_unload_custom_font(&date_lang_font);
  }
  if (date_numeric_font != NULL) {
    safe_unload_custom_font(&date_numeric_font);
  }
  display_lang = -1;

  window_destroy(window);
  window = NULL;
}

// Called at program exit to cleanly shut everything down.
void handle_deinit() {
#ifdef MAKE_CHRONOGRAPH
  save_chrono_data();
#endif  // MAKE_CHRONOGRAPH
  tick_timer_service_unsubscribe();

  destroy_objects();
}

// Called at program start to bootstrap everything.
void handle_init() {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "handle_init");

  // Record the fallback font pointer so we can identify if this one
  // is accidentally returned from fonts_load_custom_font().
  fallback_font = fonts_get_system_font(FONT_KEY_FONT_FALLBACK);

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
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "available message space %u, %u", (unsigned int)inbox_max, (unsigned int)outbox_max);
  if (inbox_max > INBOX_MESSAGE_SIZE) {
    inbox_max = INBOX_MESSAGE_SIZE;
  }
  if (outbox_max > OUTBOX_MESSAGE_SIZE) {
    outbox_max = OUTBOX_MESSAGE_SIZE;
  }
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "app_message_open(%u, %u)", (unsigned int)inbox_max, (unsigned int)outbox_max);
  AppMessageResult open_result = app_message_open(inbox_max, outbox_max);
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "open_result = %d", open_result);

#else  // NDEBUG
  app_message_open(INBOX_MESSAGE_SIZE, OUTBOX_MESSAGE_SIZE);
#endif  // NDEBUG

  time_t now = time(NULL);
  struct tm *startup_time = localtime(&now);
  
  create_objects();
  compute_hands(startup_time, &current_placement);
  apply_config();
}

void trigger_memory_panic(int line_number) {
  // Something failed to allocate properly, so we'll set a flag so we
  // can try to clean up unneeded memory.
  app_log(APP_LOG_LEVEL_WARNING, __FILE__, __LINE__, "memory_panic at line %d!", line_number);
  memory_panic_flag = true;
}

void reset_memory_panic() {
  // The memory_panic_flag was set, indicating that something
  // somewhere failed to allocate memory.  Destory and recreate
  // everything, in the hopes this will clear out memory
  // fragmentation.

  memory_panic_flag = false;
  ++memory_panic_count;

  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "reset_memory_panic begin, count = %d", memory_panic_count);

  destroy_objects();
  create_objects();

  // Start resetting some options if the memory panic count grows too high.
  if (memory_panic_count > 1) {
    hour_resource_cache_size = 0;
#ifdef MAKE_CHRONOGRAPH
    chrono_tenth_resource_cache_size = 0;
#endif  // MAKE_CHRONOGRAPH
  }
  if (memory_panic_count > 2) {
    minute_resource_cache_size = 0;
#ifdef MAKE_CHRONOGRAPH
    chrono_minute_resource_cache_size = 0;
#endif  // MAKE_CHRONOGRAPH
  }
  if (memory_panic_count > 3) {
    second_resource_cache_size = 0;
#ifdef MAKE_CHRONOGRAPH
    chrono_second_resource_cache_size = 0;
#endif  // MAKE_CHRONOGRAPH
  }
  if (memory_panic_count > 4) {
    config.battery_gauge = IM_off;
    config.bluetooth_indicator = IM_off;
  }
  if (memory_panic_count > 5) {
    config.second_hand = false;
  } 
  if (memory_panic_count > 6) {
    for (int i = 0; i < NUM_DATE_WINDOWS; ++i) {
      if (config.date_windows[i] != DWM_debug_memory_panic_count) {
        config.date_windows[i] = DWM_off;
      }
    }
  } 
  if (memory_panic_count > 7) {
    config.chrono_dial = 0;
  }
  if (memory_panic_count > 8) {
    config.top_subdial = false;
  }
  if (memory_panic_count > 9) {
    // At this point we hide the clock face.  Drastic!
  }

  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "reset_memory_panic done, count = %d", memory_panic_count);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}
