#include "wright.h"
#include "wright_chrono.h"
#include "antialiasing.h"

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
const GRect date_window_box = {
  { 2, 0 }, { 37, 19 }
};
// This is the position to draw the lunar image if it's inverted.
const GRect date_window_box_offset = {
  { -1, 0 }, { 40, 19 }
};

#ifdef SUPPORT_MOON
BitmapWithData moon_bitmap;
#endif // SUPPORT_MOON

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

struct FontPlacement date_numeric_font_placement = {
  0, -3
};
#define NUM_DATE_LANG_FONTS 9
struct FontPlacement date_lang_font_placement[NUM_DATE_LANG_FONTS] = {
  { RESOURCE_ID_DAY_FONT_LATIN_16, -1 },
  { RESOURCE_ID_DAY_FONT_EXTENDED_14, 1 },
  { RESOURCE_ID_DAY_FONT_RTL_14, 1 },
  { RESOURCE_ID_DAY_FONT_ZH_16, -1 },  // Chinese
  { RESOURCE_ID_DAY_FONT_JA_16, -1 },  // Japanese
  { RESOURCE_ID_DAY_FONT_KO_16, -2 },  // Korean
  { RESOURCE_ID_DAY_FONT_TH_16, -1 },  // Thai
  { RESOURCE_ID_DAY_FONT_TA_16, -2 },  // Tamil
  { RESOURCE_ID_DAY_FONT_HI_16, 0 },  // Hindi
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

Layer *hour_layer;
Layer *minute_layer;
Layer *second_layer;

Layer *date_window_layers[NUM_DATE_WINDOWS];

struct HandCache hour_cache;
struct HandCache minute_cache;
struct HandCache second_cache;

struct HandPlacement current_placement;

#ifdef PBL_PLATFORM_APLITE
// In Aplite, we have to decide carefully what compositing mode to
// draw the hands.
DrawModeTable draw_mode_table[2] = {
  { GCompOpClear, GCompOpOr, GCompOpAssign, GCompOpAnd, GCompOpSet, { GColorClearInit, GColorBlackInit, GColorWhiteInit } },
  { GCompOpOr, GCompOpClear, GCompOpAssignInverted, GCompOpSet, GCompOpAnd, { GColorClearInit, GColorWhiteInit, GColorBlackInit } },
};

#else  // PBL_PLATFORM_APLITE

// In Basalt, we always use GCompOpSet to draw the hands, because we
// always use the alpha channel.  The only exception is paint_assign,
// because assign means assign.
DrawModeTable draw_mode_table[2] = {
  { GCompOpSet, GCompOpSet, GCompOpAssign, GCompOpSet, GCompOpSet, { GColorClearInit, GColorBlackInit, GColorWhiteInit } },
  { GCompOpSet, GCompOpSet, GCompOpAssign, GCompOpSet, GCompOpSet, { GColorClearInit, GColorWhiteInit, GColorBlackInit } },
};

#endif  // PBL_PLATFORM_APLITE

unsigned char stacking_order[] = {
  STACKING_ORDER_LIST
};

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
  time_t s;
  uint16_t t_ms;
  unsigned int ms;

  // Compute the number of milliseconds since midnight.
  time_ms(&s, &t_ms);
  ms = (unsigned int)((s % SECONDS_PER_DAY) * 1000 + t_ms);

#ifdef FAST_TIME
  if (time != NULL) {
    time->tm_wday = (s / 3) % 7;
    time->tm_mon = (s / 4) % 12;
    time->tm_mday = (s % 31) + 1;
    time->tm_hour = s % 24;
  }
  ms *= 67;
#endif  // FAST_TIME

  /*
  // Hack for screenshots.
  {
    ms = ((10*60 + 9)*60 + 36) * 1000;  // 10:09:36
    if (time != NULL) {
      time->tm_mday = 9;
    }
  }
  */
  
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

#ifdef SUPPORT_MOON
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
      unsigned int lunar_offset_s = (unsigned int)s - 1401302400;
      
      // Now we have the number of seconds elapsed since a known new
      // moon.  To compute modulo 29.5305882 days using integer
      // arithmetic, we actually compute modulo 2551443 seconds.
      // (This integer computation is a bit less precise than the full
      // decimal value--by 2114 it have drifted off by about 2 hours.
      // Close enough.  We don't account for timezone here anyway, and
      // that's a much bigger error than this minor drift, but even
      // that's pretty minor.)
      unsigned int lunar_age_s = lunar_offset_s % 2551443;

      // That gives the age of the moon in seconds.  We really want it
      // in the range 0 .. 7, so we divide by (2551443 / 8) to give us
      // that.
      unsigned int lunar_phase = (8 * lunar_age_s) / 2551443;

#ifdef FAST_TIME
      lunar_phase = s;
#endif  // FAST_TIME

      // We shouldn't need to take modulo 8 here (since we already
      // computed modulo 2551443 above), but we do it anyway just in
      // case there's an unexpected edge condition.
      placement->lunar_phase = lunar_phase % 8;
    }
#endif  // SUPPORT_MOON
  }

  placement->hour_buzzer = (ms / (SECONDS_PER_HOUR * 1000)) % 24;

#ifdef MAKE_CHRONOGRAPH
  compute_chrono_hands(ms, placement);
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

// Horizontally flips the indicated GBitmap in-place.  Requires
// that the width be a multiple of 8 pixels.
void flip_bitmap_x(GBitmap *image, short *cx) {
  if (image == NULL) {
    // Trivial no-op.
    return;
  }
  
  int height = gbitmap_get_bounds(image).size.h;
  int width = gbitmap_get_bounds(image).size.w;
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
    pixels_per_byte = 1;
    break;
  }
#endif  // PBL_PLATFORM_APLITE
    
  assert(width % pixels_per_byte == 0);  // This must be an even divisor, by our convention.
  int width_bytes = width / pixels_per_byte;
  int stride = gbitmap_get_bytes_per_row(image);
  assert(stride >= width_bytes);

  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "flip_bitmap_x, width_bytes = %d, stride=%d", width_bytes, stride);

  uint8_t *data = gbitmap_get_data(image);

  for (int y = 0; y < height; ++y) {
    uint8_t *row = data + y * stride;
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
  int stride = gbitmap_get_bytes_per_row(image); // multiple of 4.
  uint8_t *data = gbitmap_get_data(image);

  uint8_t buffer[stride]; // gcc lets us do this.
  for (int y1 = (height - 1) / 2; y1 >= 0; --y1) {
    int y2 = height - 1 - y1;
    // Swap rows y1 and y2.
    memcpy(buffer, data + y1 * stride, stride);
    memcpy(data + y1 * stride, data + y2 * stride, stride);
    memcpy(data + y2 * stride, buffer, stride);
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

    if (group->fill != 0) {
      graphics_context_set_fill_color(ctx, draw_mode_table[config.draw_mode].colors[group->fill]);
      gpath_draw_filled(ctx, hand_cache->path[gi]);
    }
    if (group->outline != 0) {
      graphics_context_set_stroke_color(ctx, draw_mode_table[config.draw_mode].colors[group->outline]);
      gpath_draw_outline_antialiased(ctx, hand_cache->path[gi], draw_mode_table[config.draw_mode].colors[group->outline]);
    }
  }
}

// Draws a given hand on the face, using the bitmap structures.
void draw_bitmap_hand(struct HandCache *hand_cache, struct HandDef *hand_def, int hand_index, GContext *ctx) {
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

  struct BitmapHandTableRow *hand = &hand_def->bitmap_table[hand_index];
  int bitmap_index = hand->bitmap_index;
  struct BitmapHandCenterRow *lookup = &hand_def->bitmap_centers[bitmap_index];

  int hand_resource_id = hand_def->resource_id + bitmap_index;
  int hand_resource_mask_id = hand_def->resource_mask_id + bitmap_index;

#ifdef PBL_PLATFORM_APLITE
  if (hand_def->resource_id == hand_def->resource_mask_id)
#else
    if (true)  // On Basalt, we always draw without the mask.
#endif  // PBL_PLATFORM_APLITE
    {
    // The hand does not have a mask.  Draw the hand on top of the scene.
    if (hand_cache->image.bitmap == NULL) {
      if (hand_def->use_rle) {
	hand_cache->image = rle_bwd_create(hand_resource_id);
      } else {
	hand_cache->image = png_bwd_create(hand_resource_id);
      }
      if (hand_cache->image.bitmap == NULL) {
        hand_cache_destroy(hand_cache);
	trigger_memory_panic(__LINE__);
        return;
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
    GRect destination = gbitmap_get_bounds(hand_cache->image.bitmap);
    
    // Place the hand's center point at place_x, place_y.
    destination.origin.x = hand_def->place_x - hand_cache->cx;
    destination.origin.y = hand_def->place_y - hand_cache->cy;
    
    // Specify a compositing mode to make the hands overlay on top of
    // each other, instead of the background parts of the bitmaps
    // blocking each other.

    if (hand_def->paint_black) {
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
      if (hand_def->use_rle) {
	hand_cache->image = rle_bwd_create(hand_resource_id);
	hand_cache->mask = rle_bwd_create(hand_resource_mask_id);
      } else {
	hand_cache->image = png_bwd_create(hand_resource_id);
	hand_cache->mask = png_bwd_create(hand_resource_mask_id);
      }
      if (hand_cache->image.bitmap == NULL || hand_cache->mask.bitmap == NULL) {
        hand_cache_destroy(hand_cache);
	trigger_memory_panic(__LINE__);
        return;
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
    
    GRect destination = gbitmap_get_bounds(hand_cache->image.bitmap);
    
    destination.origin.x = hand_def->place_x - hand_cache->cx;
    destination.origin.y = hand_def->place_y - hand_cache->cy;

    graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode].paint_white);
    graphics_draw_bitmap_in_rect(ctx, hand_cache->mask.bitmap, destination);
    
    graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode].paint_black);
    graphics_draw_bitmap_in_rect(ctx, hand_cache->image.bitmap, destination);
  }
}

// Draws a given hand on the face, using the vector and/or bitmap
// structures.  A given hand may be represented by a bitmap or a
// vector, or a combination of both.
void draw_hand(struct HandCache *hand_cache, struct HandDef *hand_def, int hand_index, GContext *ctx) {
  if (hand_def->vector_hand != NULL) {
    draw_vector_hand(hand_cache, hand_def, hand_index, ctx);
  }

  if (hand_def->bitmap_table != NULL) {
    draw_bitmap_hand(hand_cache, hand_def, hand_index, ctx);
  }
}

void clock_face_layer_update_callback(Layer *me, GContext *ctx) {
  //  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "clock_face_layer");
  if (memory_panic_count > 5) {
    // In case we're in extreme memory panic mode--too little
    // available memory to even keep the clock face resident--we do
    // nothing in this function.
    return;
  }

  // Load the clock face from the resource file if we haven't already.
  if (clock_face.bitmap == NULL) {
    clock_face = rle_bwd_create(clock_face_table[config.face_index]);
    if (clock_face.bitmap == NULL) {
      trigger_memory_panic(__LINE__);
      return;
    }
  }

  // Draw the clock face into the layer.
  GRect destination = layer_get_bounds(me);
  destination.origin.x = 0;
  destination.origin.y = 0;
  graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode].paint_assign);
  graphics_draw_bitmap_in_rect(ctx, clock_face.bitmap, destination);
}
  
void hour_layer_update_callback(Layer *me, GContext *ctx) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "hour_layer");

  draw_hand(&hour_cache, &hour_hand_def, current_placement.hour_hand_index, ctx);
}

void minute_layer_update_callback(Layer *me, GContext *ctx) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "minute_layer");

  draw_hand(&minute_cache, &minute_hand_def, current_placement.minute_hand_index, ctx);
}

void second_layer_update_callback(Layer *me, GContext *ctx) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "second_layer");

  if (config.second_hand) {
    draw_hand(&second_cache, &second_hand_def, current_placement.second_hand_index, ctx);
  }
}

// Draws the frame and optionally fills the background of the current date window.
void draw_date_window_background(GContext *ctx, unsigned int fg_draw_mode, unsigned int bg_draw_mode, bool opaque_layer) {
#ifdef PBL_PLATFORM_APLITE
  // We only need the mask on Aplite.
  if (opaque_layer || bg_draw_mode != fg_draw_mode) {
    if (date_window_mask.bitmap == NULL) {
      date_window_mask = rle_bwd_create(RESOURCE_ID_DATE_WINDOW_MASK);
      if (date_window_mask.bitmap == NULL) {
	trigger_memory_panic(__LINE__);
        return;
      }
    }
    graphics_context_set_compositing_mode(ctx, draw_mode_table[bg_draw_mode].paint_mask);
    graphics_draw_bitmap_in_rect(ctx, date_window_mask.bitmap, date_window_box);
  }
#endif  // PBL_PLATFORM_APLITE
  
  if (date_window.bitmap == NULL) {
    date_window = rle_bwd_create(RESOURCE_ID_DATE_WINDOW);
    if (date_window.bitmap == NULL) {
      bwd_destroy(&date_window_mask);
      trigger_memory_panic(__LINE__);
      return;
    }
  }
  
  graphics_context_set_compositing_mode(ctx, draw_mode_table[fg_draw_mode].paint_fg);
  graphics_draw_bitmap_in_rect(ctx, date_window.bitmap, date_window_box);
}

// Draws a date window with the specified text contents.  Usually this is
// something like a numeric date or the weekday name.
void draw_window(Layer *me, GContext *ctx, const char *text, struct FontPlacement *font_placement, 
		 GFont *font, bool invert, bool opaque_layer) {
  GRect box = date_window_box;

  unsigned int draw_mode = invert ^ config.draw_mode;
  draw_date_window_background(ctx, draw_mode, draw_mode, opaque_layer);

  graphics_context_set_text_color(ctx, draw_mode_table[draw_mode].colors[1]);

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

#ifdef SUPPORT_MOON

// Draws a date window with the current lunar phase.
void draw_lunar_window(Layer *me, GContext *ctx, DateWindowMode dwm, bool invert, bool opaque_layer) {
  // The draw_mode is the color to draw the frame of the date window.
  unsigned int draw_mode = invert ^ config.draw_mode;

  // The moon_draw_mode is the color to draw the moon within the date window.
  unsigned int moon_draw_mode = draw_mode;
  if (config.lunar_background) {
    // If the user specified an always-black background, that means moon_draw_mode is always 1.
    moon_draw_mode = 1;
  }

  draw_date_window_background(ctx, draw_mode, moon_draw_mode, opaque_layer);

  if (moon_bitmap.bitmap == NULL) {
    assert(current_placement.lunar_phase <= 7);
    if (moon_draw_mode == 0) {
      moon_bitmap = rle_bwd_create(RESOURCE_ID_MOON_BLACK_0 + current_placement.lunar_phase);
    } else {
      moon_bitmap = rle_bwd_create(RESOURCE_ID_MOON_WHITE_0 + current_placement.lunar_phase);
    }
    if (moon_bitmap.bitmap == NULL) {
      trigger_memory_panic(__LINE__);
      return;
    }

    if (config.lunar_direction) {
      // Draw the moon phases animating from left-to-right, as seen in
      // the southern hemisphere.  (This really means drawing the moon
      // upside-down, as it would be seen by someone facing north.)
      flip_bitmap_x(moon_bitmap.bitmap, NULL);
      flip_bitmap_y(moon_bitmap.bitmap, NULL);
    }
  }

  // Draw the moon in the fg color.  This will be black-on-white if
  // moon_draw_mode = 0, or white-on-black if moon_draw_mode = 1.
  // Since we have selected the particular moon resource above based
  // on draw_mode, we will always draw the moon in the correct color,
  // so that it looks like the moon.  (Drawing the moon in the
  // inverted color would look weird.)
  graphics_context_set_compositing_mode(ctx, draw_mode_table[moon_draw_mode].paint_black);

  if (config.lunar_direction) {
    graphics_draw_bitmap_in_rect(ctx, moon_bitmap.bitmap, date_window_box_offset);
  } else {
    graphics_draw_bitmap_in_rect(ctx, moon_bitmap.bitmap, date_window_box);
  }
}
#endif  // SUPPORT_MOON

void date_window_layer_update_callback(Layer *me, GContext *ctx) {
  DateWindowData *data = (DateWindowData *)layer_get_data(me);
  unsigned int date_window_index = data->date_window_index;
  //  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "date_window_layer %c", date_window_index + 'a');

  DateWindowMode dwm = config.date_windows[date_window_index];
  if (dwm == DWM_off) {
    // Do nothing.
    return;
  }

  const struct IndicatorTable *window = &date_windows[date_window_index][config.face_index];

#ifdef SUPPORT_MOON
  if (dwm == DWM_moon) {
    // Draw the lunar phase.
    draw_lunar_window(me, ctx, dwm, window->invert, window->opaque);
    return;
  }
#endif  // SUPPORT_MOON

  // Format the date or weekday or whatever text for display.
#define DATE_WINDOW_BUFFER_SIZE 16
  char buffer[DATE_WINDOW_BUFFER_SIZE];

  GFont *font = &date_numeric_font;
  struct FontPlacement *font_placement = &date_numeric_font_placement;
  if (dwm >= DWM_weekday) {
    // Draw text using date_lang_font.
    const LangDef *lang = &lang_table[config.display_lang];
    font = &date_lang_font;
    font_placement = &date_lang_font_placement[lang->font_index];
  }

  char *text = buffer;

  switch (dwm) {
  case DWM_identify:
    snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%c", 'A' + date_window_index);
    break;

  case DWM_date:
    snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%d", current_placement.date_value);
    break;

  case DWM_year:
    snprintf(buffer, DATE_WINDOW_BUFFER_SIZE, "%d", current_placement.year_value + 1900);
    break;

  case DWM_weekday:
    text = date_names[current_placement.day_index];
    break;
    
  case DWM_month:
    text = date_names[current_placement.month_index + NUM_WEEKDAY_NAMES];
    break;

  case DWM_ampm:
    text = date_names[current_placement.ampm_value + NUM_WEEKDAY_NAMES + NUM_MONTH_NAMES];
    break;

  case DWM_moon:
  default:
    buffer[0] = '\0';
  }

  draw_window(me, ctx, text, font_placement, font, window->invert, window->opaque);
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
    /*
    for (int i = 0; i < NUM_DATE_WINDOWS; ++i) {
      layer_mark_dirty(date_window_layers[i]);
    }
    */
  }

#ifdef SUPPORT_MOON
  // Also check the lunar phase, in a separate check from the other
  // date windows, so it doesn't necessarily have to wait till
  // midnight to flip over to the next phase.
  if (new_placement.lunar_phase != current_placement.lunar_phase) {
    current_placement.lunar_phase = new_placement.lunar_phase;
    bwd_destroy(&moon_bitmap);
    layer_mark_dirty(clock_face_layer);
  }
#endif  // SUPPORT_MOON
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

#ifdef MAKE_CHRONOGRAPH
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

    // Also move any layers to their new position on this face.
    for (int i = 0; i < NUM_DATE_WINDOWS; ++i) {
      const struct IndicatorTable *window = &date_windows[i][config.face_index];
      layer_set_frame((Layer *)date_window_layers[i], GRect(window->x - 19, window->y - 8, 39, 19));
    }

    {
      const struct IndicatorTable *window = &battery_table[config.face_index];
      move_battery_gauge(window->x, window->y, window->invert, window->opaque);
    }
    {
      const struct IndicatorTable *window = &bluetooth_table[config.face_index];
      move_bluetooth_indicator(window->x, window->y, window->invert, window->opaque);
    }
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

#ifdef SUPPORT_MOON
  // Reload the moon bitmap just for good measure.  Maybe the user
  // changed the draw mode or the lunar direction.
  bwd_destroy(&moon_bitmap);
#endif  // SUPPORT_MOON

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

  window_set_fullscreen(window, true);
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

  {
    const struct IndicatorTable *window = &battery_table[config.face_index];
    init_battery_gauge(window_layer, window->x, window->y, window->invert, window->opaque);
  }
  {
    const struct IndicatorTable *window = &bluetooth_table[config.face_index];
    init_bluetooth_indicator(window_layer, window->x, window->y, window->invert, window->opaque);
  }

  for (int i = 0; i < NUM_DATE_WINDOWS; ++i) {
    const struct IndicatorTable *window = &date_windows[i][config.face_index];
    Layer *layer = layer_create_with_data(GRect(window->x - 19, window->y - 8, 39, 19), sizeof(DateWindowData));
    assert(layer != NULL);
    date_window_layers[i] = layer;
    DateWindowData *data = (DateWindowData *)layer_get_data(layer);
    data->date_window_index = i;

    layer_set_update_proc(layer, &date_window_layer_update_callback);
    layer_add_child(window_layer, layer);
  }

#ifdef MAKE_CHRONOGRAPH
  create_chrono_objects();
#endif  // MAKE_CHRONOGRAPH

  // Init all of the hands, taking care to arrange them in the correct
  // stacking order.
  int i;
  for (i = 0; stacking_order[i] != STACKING_ORDER_DONE; ++i) {
    switch (stacking_order[i]) {
    case STACKING_ORDER_HOUR:
      hour_layer = layer_create(window_frame);
      assert(hour_layer != NULL);
      layer_set_update_proc(hour_layer, &hour_layer_update_callback);
      layer_add_child(window_layer, hour_layer);
      break;

    case STACKING_ORDER_MINUTE:
      minute_layer = layer_create(window_frame);
      assert(minute_layer != NULL);
      layer_set_update_proc(minute_layer, &minute_layer_update_callback);
      layer_add_child(window_layer, minute_layer);
      break;

    case STACKING_ORDER_SECOND:
      second_layer = layer_create(window_frame);
      assert(second_layer != NULL);
      layer_set_update_proc(second_layer, &second_layer_update_callback);
      layer_add_child(window_layer, second_layer);
      break;

    case STACKING_ORDER_CHRONO_MINUTE:
#if defined(ENABLE_CHRONO_MINUTE_HAND) && defined(MAKE_CHRONOGRAPH)
      chrono_minute_layer = layer_create(window_frame);
      assert(chrono_minute_layer != NULL);
      layer_set_update_proc(chrono_minute_layer, &chrono_minute_layer_update_callback);
      layer_add_child(window_layer, chrono_minute_layer);
#endif  // ENABLE_CHRONO_MINUTE_HAND
      break;

    case STACKING_ORDER_CHRONO_SECOND:
#if defined(ENABLE_CHRONO_SECOND_HAND) && defined(MAKE_CHRONOGRAPH)
      chrono_second_layer = layer_create(window_frame);
      assert(chrono_second_layer != NULL);
      layer_set_update_proc(chrono_second_layer, &chrono_second_layer_update_callback);
      layer_add_child(window_layer, chrono_second_layer);
#endif  // ENABLE_CHRONO_SECOND_HAND
      break;

    case STACKING_ORDER_CHRONO_TENTH:
#if defined(ENABLE_CHRONO_TENTH_HAND) && defined(MAKE_CHRONOGRAPH)
      chrono_tenth_layer = layer_create(window_frame);
      assert(chrono_tenth_layer != NULL);
      layer_set_update_proc(chrono_tenth_layer, &chrono_tenth_layer_update_callback);
      layer_add_child(window_layer, chrono_tenth_layer);
#endif  // ENABLE_CHRONO_TENTH_HAND
      break;
    }
  }
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

  for (int i = 0; i < NUM_DATE_WINDOWS; ++i) {
    layer_destroy(date_window_layers[i]);
    date_window_layers[i] = NULL;
  }

  bwd_destroy(&date_window);
  bwd_destroy(&date_window_mask);

#ifdef SUPPORT_MOON
  bwd_destroy(&moon_bitmap);
#endif  // SUPPORT_MOON
  
  layer_destroy(minute_layer);
  layer_destroy(hour_layer);
  layer_destroy(second_layer);

  hand_cache_destroy(&hour_cache);
  hand_cache_destroy(&minute_cache);
  hand_cache_destroy(&second_cache);

  if (date_lang_font != NULL) {
    safe_unload_custom_font(&date_lang_font);
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

  date_numeric_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

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
    config.battery_gauge = IM_off;
    config.bluetooth_indicator = IM_off;
  }
  if (memory_panic_count > 2) {
    config.second_hand = false;
  } 
  if (memory_panic_count > 3) {
    for (int i = 0; i < NUM_DATE_WINDOWS; ++i) {
      config.date_windows[i] = DWM_off;
    }
  } 
  if (memory_panic_count > 4) {
    config.chrono_dial = 0;
  } 
  if (memory_panic_count > 5) {
    // At this point we hide the clock face.  Drastic!
  }

  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "reset_memory_panic done, count = %d", memory_panic_count);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}
