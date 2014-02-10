#include <pebble.h>

#include "hand_table.h"
#include "../resources/generated_config.h"
#include "../resources/generated_table.c"
#include "bluetooth_indicator.h"
#include "battery_gauge.h"
#include "config_options.h"

#define SECONDS_PER_DAY 86400
#define MS_PER_DAY (SECONDS_PER_DAY * 1000)

#define SCREEN_WIDTH 144
#define SCREEN_HEIGHT 168

Window *window;

GBitmap *clock_face_bitmap;
BitmapLayer *clock_face_layer;

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
  int hour_hand_index;
  int minute_hand_index;
  int second_hand_index;
  int chrono_minute_hand_index;
  int chrono_second_hand_index;
  int chrono_tenth_hand_index;
  int day_index;
  int date_value;

  // Not really a hand placement, but this is used to keep track of
  // whether we have buzzed for the top of the hour or not.
  int hour_buzzer;
};

struct HandPlacement current_placement;

typedef struct {
  GCompOp paint_black;
  GCompOp paint_white;
  GCompOp paint_assign;
  GColor colors[3];
} DrawModeTable;

DrawModeTable draw_mode_table[2] = {
  { GCompOpClear, GCompOpOr, GCompOpAssign, { GColorClear, GColorBlack, GColorWhite } },
  { GCompOpOr, GCompOpClear, GCompOpAssignInverted, { GColorClear, GColorWhite, GColorBlack } },
};

static const uint32_t tap_segments[] = { 50 };
VibePattern tap = {
  tap_segments,
  1,
};

int stacking_order[] = {
STACKING_ORDER_LIST
};

int chrono_running = false;       // the chronograph has been started
int chrono_lap_paused = false;    // the "lap" button has been pressed
int chrono_start_ms = 0; // consulted if chrono_running && !chrono_lap_paused
int chrono_hold_ms = 0;  // consulted if !chrono_running || chrono_lap_paused

// Returns the number of milliseconds since midnight.
int get_time_ms(struct tm *time) {
  time_t s;
  uint16_t ms;
  int result;

  time_ms(&s, &ms);
  result = (s % SECONDS_PER_DAY) * 1000 + ms;

#ifdef FAST_TIME
  if (time != NULL) {
    time->tm_wday = s % 7;
    time->tm_mday = (s % 31) + 1;
  }
  result *= 67;
#endif  // FAST_TIME

  return result;
}

// Determines the specific hand bitmaps that should be displayed based
// on the current time.
void compute_hands(struct tm *time, struct HandPlacement *placement) {
  int ms;

  ms = get_time_ms(time);

  placement->hour_hand_index = ((NUM_STEPS_HOUR * ms) / (3600 * 12 * 1000)) % NUM_STEPS_HOUR;
  placement->minute_hand_index = ((NUM_STEPS_MINUTE * ms) / (3600 * 1000)) % NUM_STEPS_MINUTE;
  placement->second_hand_index = ((NUM_STEPS_SECOND * ms) / (60 * 1000)) % NUM_STEPS_SECOND;

#ifdef SHOW_DAY_CARD
  if (time != NULL) {
    placement->day_index = time->tm_wday;
  }
#endif  // SHOW_DAY_CARD

#ifdef SHOW_DATE_CARD
  if (time != NULL) {
    placement->date_value = time->tm_mday;
  }
#endif  // SHOW_DATE_CARD

  placement->hour_buzzer = (ms / (3600 * 1000)) % 24;

#ifdef MAKE_CHRONOGRAPH
  {
    int chrono_ms;
    if (chrono_running && !chrono_lap_paused) {
      // The chronograph is running.  Show the active elapsed time.
      chrono_ms = (ms - chrono_start_ms + MS_PER_DAY) % MS_PER_DAY;
    } else {
      // The chronograph is paused.  Show the time it is paused on.
      chrono_ms = chrono_hold_ms;
    }

#ifdef SHOW_CHRONO_MINUTE_HAND
    // The chronograph minute hand rolls completely around in 30
    // minutes (not 60).
    placement->chrono_minute_hand_index = ((NUM_STEPS_CHRONO_MINUTE * chrono_ms) / (1800 * 1000)) % NUM_STEPS_CHRONO_MINUTE;
#endif  // SHOW_CHRONO_MINUTE_HAND

#ifdef SHOW_CHRONO_SECOND_HAND
    placement->chrono_second_hand_index = ((NUM_STEPS_CHRONO_SECOND * chrono_ms) / (60 * 1000)) % NUM_STEPS_CHRONO_SECOND;
#endif  // SHOW_CHRONO_SECOND_HAND

#ifdef SHOW_CHRONO_TENTH_HAND
    if (chrono_running && !chrono_lap_paused) {
      // We don't actually show the tenths time while the chrono is running.
      placement->chrono_tenth_hand_index = 0;
    } else {
      // We show the tenths time when the chrono is stopped or showing
      // the lap time.
      placement->chrono_tenth_hand_index = ((NUM_STEPS_CHRONO_TENTH * chrono_ms) / (100)) % NUM_STEPS_CHRONO_TENTH;
    }
#endif  // SHOW_CHRONO_TENTH_HAND

  }
#endif  // MAKE_CHRONOGRAPH
}


#define MAX_DIGITS 12
const char *quick_itoa(int value) {
  int neg = 0;
  static char digits[MAX_DIGITS + 2];
  char *p = digits + MAX_DIGITS + 1;
  *p = '\0';

  if (value < 0) {
    value = -value;
    neg = 1;
  }

  do {
    --p;
    if (p < digits) {
      digits[0] = '*';
      return digits;
    }

    *p = '0' + (value % 10);
    value /= 10;
  } while (value != 0);

  if (neg) {
    --p;
    if (p < digits) {
      digits[0] = '*';
      return digits;
    }

    *p = '-';
  }

  return p;
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
void draw_vector_hand(struct VectorHandTable *hand, int hand_index, int num_steps,
                      int place_x, int place_y, GContext *ctx) {
  GPoint center = { place_x, place_y };
  int32_t angle = TRIG_MAX_ANGLE * hand_index / num_steps;
  int gi;

  for (gi = 0; gi < hand->num_groups; ++gi) {
    struct VectorHandGroup *group = &hand->group[gi];

    GPath *path = gpath_create(&group->path_info);

    gpath_rotate_to(path, angle);
    gpath_move_to(path, center);

    if (group->fill != GColorClear) {
      graphics_context_set_fill_color(ctx, draw_mode_table[config.draw_mode].colors[group->fill]);
      gpath_draw_filled(ctx, path);
    }
    if (group->outline != GColorClear) {
      graphics_context_set_stroke_color(ctx, draw_mode_table[config.draw_mode].colors[group->outline]);
      gpath_draw_outline(ctx, path);
    }

    gpath_destroy(path);
  }
}

// Draws a given hand on the face, using the bitmap structures.
void draw_bitmap_hand(struct BitmapHandTableRow *hand, int place_x, int place_y, GContext *ctx) {
  int cx, cy;
  cx = hand->cx;
  cy = hand->cy;

  if (hand->mask_id == hand->image_id) {
    // The hand does not have a mask.  Draw the hand on top of the scene.
    GBitmap *image;
    image = gbitmap_create_with_resource(hand->image_id);
    
    if (hand->flip_x) {
      // To minimize wasteful resource usage, if the hand is symmetric
      // we can store only the bitmaps for the right half of the clock
      // face, and flip them for the left half.
      flip_bitmap_x(image, &cx);
    }
    
    if (hand->flip_y) {
      // We can also do this vertically.
      flip_bitmap_y(image, &cy);
    }
    
    // We make sure the dimensions of the GRect to draw into
    // are equal to the size of the bitmap--otherwise the image
    // will automatically tile.
    GRect destination = image->bounds;
    
    // Place the hand's center point at place_x, place_y.
    destination.origin.x = place_x - cx;
    destination.origin.y = place_y - cy;
    
    // Specify a compositing mode to make the hands overlay on top of
    // each other, instead of the background parts of the bitmaps
    // blocking each other.

    if (hand->paint_black) {
      // Painting foreground ("white") pixels as black.
      graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode].paint_black);
    } else {
      // Painting foreground ("white") pixels as white.
      graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode].paint_white);
    }
      
    graphics_draw_bitmap_in_rect(ctx, image, destination);
    
    gbitmap_destroy(image);

  } else {
    // The hand has a mask, so use it to draw the hand opaquely.
    GBitmap *image, *mask;
    image = gbitmap_create_with_resource(hand->image_id);
    mask = gbitmap_create_with_resource(hand->mask_id);
    
    if (hand->flip_x) {
      // To minimize wasteful resource usage, if the hand is symmetric
      // we can store only the bitmaps for the right half of the clock
      // face, and flip them for the left half.
      flip_bitmap_x(image, &cx);
      flip_bitmap_x(mask, NULL);
    }
    
    if (hand->flip_y) {
      // We can also do this vertically.
      flip_bitmap_y(image, &cy);
      flip_bitmap_y(mask, NULL);
    }
    
    GRect destination = image->bounds;
    
    destination.origin.x = place_x - cx;
    destination.origin.y = place_y - cy;

    graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode].paint_white);
    graphics_draw_bitmap_in_rect(ctx, mask, destination);
    
    graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode].paint_black);
    graphics_draw_bitmap_in_rect(ctx, image, destination);
    
    gbitmap_destroy(image);
    gbitmap_destroy(mask);
  }
}
  
void hour_layer_update_callback(Layer *me, GContext *ctx) {
  (void)me;

#ifdef VECTOR_HOUR_HAND
  draw_vector_hand(&hour_hand_vector_table, current_placement.hour_hand_index,
                   NUM_STEPS_HOUR, HOUR_HAND_X, HOUR_HAND_Y, ctx);
#endif

#ifdef BITMAP_HOUR_HAND
  draw_bitmap_hand(&hour_hand_bitmap_table[current_placement.hour_hand_index],
                   HOUR_HAND_X, HOUR_HAND_Y, ctx);
#endif
}

void minute_layer_update_callback(Layer *me, GContext *ctx) {
  (void)me;

#ifdef VECTOR_MINUTE_HAND
  draw_vector_hand(&minute_hand_vector_table, current_placement.minute_hand_index,
                   NUM_STEPS_MINUTE, MINUTE_HAND_X, MINUTE_HAND_Y, ctx);
#endif

#ifdef BITMAP_MINUTE_HAND
  draw_bitmap_hand(&minute_hand_bitmap_table[current_placement.minute_hand_index],
                   MINUTE_HAND_X, MINUTE_HAND_Y, ctx);
#endif
}

void second_layer_update_callback(Layer *me, GContext *ctx) {
  (void)me;

  if (config.second_hand) {
#ifdef VECTOR_SECOND_HAND
    draw_vector_hand(&second_hand_vector_table, current_placement.second_hand_index,
		     NUM_STEPS_SECOND, SECOND_HAND_X, SECOND_HAND_Y, ctx);
#endif
    
#ifdef BITMAP_SECOND_HAND
    draw_bitmap_hand(&second_hand_bitmap_table[current_placement.second_hand_index],
		     SECOND_HAND_X, SECOND_HAND_Y, ctx);
#endif
  }
}

#ifdef SHOW_CHRONO_MINUTE_HAND
void chrono_minute_layer_update_callback(Layer *me, GContext *ctx) {
  (void)me;

  if (config.second_hand || chrono_running || chrono_hold_ms != 0) {
#ifdef VECTOR_CHRONO_MINUTE_HAND
    draw_vector_hand(&chrono_minute_hand_vector_table, current_placement.chrono_minute_hand_index,
		     NUM_STEPS_CHRONO_MINUTE, CHRONO_MINUTE_HAND_X, CHRONO_MINUTE_HAND_Y, ctx);
#endif
    
#ifdef BITMAP_CHRONO_MINUTE_HAND
    draw_bitmap_hand(&chrono_minute_hand_bitmap_table[current_placement.chrono_minute_hand_index],
		     CHRONO_MINUTE_HAND_X, CHRONO_MINUTE_HAND_Y, ctx);
#endif
  }
}
#endif  // SHOW_CHRONO_MINUTE_HAND

#ifdef SHOW_CHRONO_SECOND_HAND
void chrono_second_layer_update_callback(Layer *me, GContext *ctx) {
  (void)me;

  if (config.second_hand || chrono_running || chrono_hold_ms != 0) {
#ifdef VECTOR_CHRONO_SECOND_HAND
    draw_vector_hand(&chrono_second_hand_vector_table, current_placement.chrono_second_hand_index,
		     NUM_STEPS_CHRONO_SECOND, CHRONO_SECOND_HAND_X, CHRONO_SECOND_HAND_Y, ctx);
#endif
    
#ifdef BITMAP_CHRONO_SECOND_HAND
    draw_bitmap_hand(&chrono_second_hand_bitmap_table[current_placement.chrono_second_hand_index],
		     CHRONO_SECOND_HAND_X, CHRONO_SECOND_HAND_Y, ctx);
#endif
  }
}
#endif  // SHOW_CHRONO_SECOND_HAND

#ifdef SHOW_CHRONO_TENTH_HAND
void chrono_tenth_layer_update_callback(Layer *me, GContext *ctx) {
  (void)me;

  if (config.second_hand || chrono_running || chrono_hold_ms != 0) {
#ifdef VECTOR_CHRONO_TENTH_HAND
    draw_vector_hand(&chrono_tenth_hand_vector_table, current_placement.chrono_tenth_hand_index,
		     NUM_STEPS_CHRONO_TENTH, CHRONO_TENTH_HAND_X, CHRONO_TENTH_HAND_Y, ctx);
#endif
    
#ifdef BITMAP_CHRONO_TENTH_HAND
    draw_bitmap_hand(&chrono_tenth_hand_bitmap_table[current_placement.chrono_tenth_hand_index],
		     CHRONO_TENTH_HAND_X, CHRONO_TENTH_HAND_Y, ctx);
#endif
  }
}
#endif  // SHOW_CHRONO_TENTH_HAND

void draw_card(Layer *me, GContext *ctx, const char *text, bool on_black, bool bold) {
  GFont font;
  GRect box;

  if (bold) {
    font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  } else {
    font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  }
  box = layer_get_frame(me);
  box.origin.x = 0;
  box.origin.y = 0;
  if (on_black) {
    graphics_context_set_text_color(ctx, GColorWhite);
  } else {
    graphics_context_set_text_color(ctx, GColorBlack);
  }

  box.origin.y -= 3;  // Determined empirically.

  graphics_draw_text(ctx, text, font, box,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}

#ifdef SHOW_DAY_CARD
void day_layer_update_callback(Layer *me, GContext *ctx) {
  draw_card(me, ctx, weekday_names[current_placement.day_index], DAY_CARD_ON_BLACK, DAY_CARD_BOLD);
}
#endif  // SHOW_DAY_CARD

#ifdef SHOW_DATE_CARD
void date_layer_update_callback(Layer *me, GContext *ctx) {
  draw_card(me, ctx, quick_itoa(current_placement.date_value), DATE_CARD_ON_BLACK, DATE_CARD_BOLD);
}
#endif  // SHOW_DATE_CARD

void update_hands(struct tm *time) {
  struct HandPlacement new_placement;

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

#ifdef MAKE_CHRONOGRAPH

#ifdef SHOW_CHRONO_MINUTE_HAND
  if (new_placement.chrono_minute_hand_index != current_placement.chrono_minute_hand_index) {
    current_placement.chrono_minute_hand_index = new_placement.chrono_minute_hand_index;
    layer_mark_dirty(chrono_minute_layer);
  }
#endif  // SHOW_CHRONO_MINUTE_HAND

#ifdef SHOW_CHRONO_SECOND_HAND
  if (new_placement.chrono_second_hand_index != current_placement.chrono_second_hand_index) {
    current_placement.chrono_second_hand_index = new_placement.chrono_second_hand_index;
    layer_mark_dirty(chrono_second_layer);
  }
#endif  // SHOW_CHRONO_SECOND_HAND

#ifdef SHOW_CHRONO_TENTH_HAND
  if (new_placement.chrono_tenth_hand_index != current_placement.chrono_tenth_hand_index) {
    current_placement.chrono_tenth_hand_index = new_placement.chrono_tenth_hand_index;
    layer_mark_dirty(chrono_tenth_layer);
  }
#endif  // SHOW_CHRONO_TENTH_HAND

#endif  // MAKE_CHRONOGRAPH

#ifdef SHOW_DAY_CARD
  if (new_placement.day_index != current_placement.day_index) {
    current_placement.day_index = new_placement.day_index;
    layer_mark_dirty(day_layer);
  }
#endif  // SHOW_DAY_CARD

#ifdef SHOW_DATE_CARD
  if (new_placement.date_value != current_placement.date_value) {
    current_placement.date_value = new_placement.date_value;
    layer_mark_dirty(date_layer);
  }
#endif  // SHOW_DATE_CARD
}

// Compute new hand positions once a minute (or once a second).
void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  update_hands(tick_time);
}

// Forward references.
void stopped_click_config_provider(void *context);
void started_click_config_provider(void *context);

void chrono_start_stop_handler(ClickRecognizerRef recognizer, void *context) {
  Window *window = (Window *)context;
  int ms = get_time_ms(NULL);

  // The start/stop button was pressed.
  if (chrono_running) {
    // If the chronograph is currently running, this means to stop (or
    // pause).
    chrono_hold_ms = ms - chrono_start_ms;
    chrono_running = false;
    chrono_lap_paused = false;
    vibes_enqueue_custom_pattern(tap);
    update_hands(NULL);
    apply_config();

    // We change the click config provider according to the chrono run
    // state.  When the chrono is stopped, we listen for a different
    // set of buttons than when it is started.
    window_set_click_config_provider(window, &stopped_click_config_provider);
  } else {
    // If the chronograph is not currently running, this means to
    // start, from the currently showing Chronograph time.
    chrono_start_ms = ms - chrono_hold_ms;
    chrono_running = true;
    vibes_enqueue_custom_pattern(tap);
    update_hands(NULL);
    apply_config();

    window_set_click_config_provider(window, &started_click_config_provider);
  }
}

void chrono_lap_button() {
  int ms;
 
  ms = get_time_ms(NULL);

  if (chrono_lap_paused) {
    // If we were already paused, this resumes the motion, jumping
    // ahead to the currently elapsed time.
    chrono_lap_paused = false;
    vibes_enqueue_custom_pattern(tap);
    update_hands(NULL);
  } else {
    // If we were not already paused, this pauses the hands here (but
    // does not stop the timer).
    chrono_hold_ms = ms - chrono_start_ms;
    chrono_lap_paused = true;
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
  chrono_running = false;
  chrono_lap_paused = false;
  chrono_start_ms = 0;
  chrono_hold_ms = 0;
  vibes_double_pulse();
  update_hands(this_time);
  apply_config();
}

void chrono_lap_handler(ClickRecognizerRef recognizer, void *context) {
  // The lap/reset button was pressed (briefly).

  // We only do anything here if the chronograph is currently running.
  if (chrono_running) {
    chrono_lap_button();
  }
}

void chrono_lap_or_reset_handler(ClickRecognizerRef recognizer, void *context) {
  // The lap/reset button was pressed (long press).

  // This means a lap if the chronograph is running, and a reset if it
  // is not.
  if (chrono_running) {
    chrono_lap_button();
  } else {
    chrono_reset_button();
  }
}

// Enable the set of buttons active while the chrono is stopped.
void stopped_click_config_provider(void *context) {
  // single click config:
  window_single_click_subscribe(BUTTON_ID_UP, &chrono_start_stop_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, NULL);

  // long click config:
  window_long_click_subscribe(BUTTON_ID_DOWN, 700, &chrono_lap_or_reset_handler, NULL);
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
}

// Updates any runtime settings as needed when the config changes.
void apply_config() {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "apply_config, second_hand=%d", config.second_hand);
  tick_timer_service_unsubscribe();

#if defined(FAST_TIME) || defined(BATTERY_HACK)
  tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
#else
  if (config.second_hand || chrono_running) {
    tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
  } else {
    tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
  }
#endif

  // Also adjust the draw mode on the clock_face_layer.  (The other
  // layers all draw themselves interactively.)
  bitmap_layer_set_compositing_mode(clock_face_layer, draw_mode_table[config.draw_mode].paint_assign);
  layer_mark_dirty((Layer *)clock_face_layer);
}

void handle_init() {
  load_config();

  app_message_register_inbox_received(receive_config_handler);
  app_message_open(64, 64);

  time_t now = time(NULL);
  struct tm *startup_time = localtime(&now);
  int i;

  window = window_create();
  window_set_fullscreen(window, true);
  window_stack_push(window, true /* Animated */);

  compute_hands(startup_time, &current_placement);

  Layer *window_layer = window_get_root_layer(window);
  GRect window_frame = layer_get_frame(window_layer);

  clock_face_bitmap = gbitmap_create_with_resource(RESOURCE_ID_CLOCK_FACE);
  clock_face_layer = bitmap_layer_create(window_frame);
  bitmap_layer_set_bitmap(clock_face_layer, clock_face_bitmap);
  layer_add_child(window_layer, bitmap_layer_get_layer(clock_face_layer));

  init_battery_gauge(window_layer, BATTERY_GAUGE_X, BATTERY_GAUGE_Y, BATTERY_GAUGE_ON_BLACK, false);
  init_bluetooth_indicator(window_layer, BLUETOOTH_X, BLUETOOTH_Y, BLUETOOTH_ON_BLACK, false);

#ifdef SHOW_DAY_CARD
  day_layer = layer_create(GRect(DAY_CARD_X - 15, DAY_CARD_Y - 8, 31, 19));
  layer_set_update_proc(day_layer, &day_layer_update_callback);
  layer_add_child(window_layer, day_layer);
#endif  // SHOW_DAY_CARD

#ifdef SHOW_DATE_CARD
  date_layer = layer_create(GRect(DATE_CARD_X - 15, DATE_CARD_Y - 8, 31, 19));
  layer_set_update_proc(date_layer, &date_layer_update_callback);
  layer_add_child(window_layer, date_layer);
#endif  // SHOW_DATE_CARD

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
#ifdef SHOW_CHRONO_MINUTE_HAND
      chrono_minute_layer = layer_create(window_frame);
      layer_set_update_proc(chrono_minute_layer, &chrono_minute_layer_update_callback);
      layer_add_child(window_layer, chrono_minute_layer);
#endif  // SHOW_CHRONO_MINUTE_HAND
      break;

    case STACKING_ORDER_CHRONO_SECOND:
#ifdef SHOW_CHRONO_SECOND_HAND
      chrono_second_layer = layer_create(window_frame);
      layer_set_update_proc(chrono_second_layer, &chrono_second_layer_update_callback);
      layer_add_child(window_layer, chrono_second_layer);
#endif  // SHOW_CHRONO_SECOND_HAND
      break;

    case STACKING_ORDER_CHRONO_TENTH:
#ifdef SHOW_CHRONO_TENTH_HAND
      chrono_tenth_layer = layer_create(window_frame);
      layer_set_update_proc(chrono_tenth_layer, &chrono_tenth_layer_update_callback);
      layer_add_child(window_layer, chrono_tenth_layer);
#endif  // SHOW_CHRONO_TENTH_HAND
      break;
    }
  }

#ifdef MAKE_CHRONOGRAPH
 window_set_click_config_provider(window, &stopped_click_config_provider);
#endif  // MAKE_CHRONOGRAPH

  apply_config();
}


void handle_deinit() {
  tick_timer_service_unsubscribe();

  window_stack_pop_all(false);  // Not sure if this is needed?
  bitmap_layer_destroy(clock_face_layer);
  gbitmap_destroy(clock_face_bitmap);

  deinit_battery_gauge();
  deinit_bluetooth_indicator();

#ifdef SHOW_DAY_CARD
  layer_destroy(day_layer);
#endif
#ifdef SHOW_DATE_CARD
  layer_destroy(date_layer);
#endif
  layer_destroy(minute_layer);
  layer_destroy(hour_layer);
  layer_destroy(second_layer);
#ifdef SHOW_CHRONO_MINUTE_HAND
  layer_destroy(chrono_minute_layer);
#endif
#ifdef SHOW_CHRONO_SECOND_HAND
  layer_destroy(chrono_second_layer);
#endif
#ifdef SHOW_CHRONO_TENTH_HAND
  layer_destroy(chrono_tenth_layer);
#endif

  window_destroy(window);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}
