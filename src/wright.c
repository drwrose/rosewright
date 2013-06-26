#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "hand_table.h"
#include "../resources/src/generated_config.h"
#include "../resources/src/generated_table.c"


PBL_APP_INFO(MY_UUID,
             WATCH_NAME, "drwrose",
             1, 2, /* App version */
             RESOURCE_ID_ROSE_ICON,
#ifdef MAKE_CHRONOGRAPH
             APP_INFO_STANDARD_APP
#else
             APP_INFO_WATCH_FACE
#endif
             );

#define SECONDS_PER_DAY 86400

#define SCREEN_WIDTH 144
#define SCREEN_HEIGHT 168

Window window;

BmpContainer clock_face_container;

Layer hour_layer;
Layer minute_layer;
Layer second_layer;
Layer chrono_minute_layer;
Layer chrono_second_layer;

Layer day_layer;  // day of the week (abbr)
Layer date_layer; // numeric date of the month

// This structure keeps track of the things that change on the visible
// watch face and their current state.
struct HandPlacement {
  int hour_hand_index;
  int minute_hand_index;
  int second_hand_index;
  int chrono_minute_hand_index;
  int chrono_second_hand_index;
  int day_index;
  int date_value;
};

struct HandPlacement current_placement;

static const uint32_t tap_segments[] = { 50 };
VibePattern tap = {
  tap_segments,
  1,
};

int chrono_running = false;       // the chronograph has been started
int chrono_lap_paused = false;    // the "lap" button has been pressed
int chrono_start_seconds = 0; // consulted if chrono_running && !chrono_lap_paused
int chrono_hold_seconds = 0;  // consulted if !chrono_running || chrono_lap_paused

// Returns the time-of-day in seconds.
int get_time_seconds(PblTm *time) {
  int seconds;

  get_time(time);
  seconds = (time->tm_hour * 60 + time->tm_min) * 60 + time->tm_sec;

#ifdef FAST_TIME
  time->tm_wday = seconds % 7;
  time->tm_mday = (seconds % 31) + 1;
  seconds *= 67;
#endif  // FAST_TIME

  return seconds;
}

// Determines the specific hand bitmaps that should be displayed based
// on the current time.
void compute_hands(struct HandPlacement *placement) {
  PblTm time;
  int seconds;

  seconds = get_time_seconds(&time);

  placement->hour_hand_index = ((NUM_STEPS_HOUR * seconds) / (3600 * 12)) % NUM_STEPS_HOUR;
  placement->minute_hand_index = ((NUM_STEPS_MINUTE * seconds) / 3600) % NUM_STEPS_MINUTE;

#ifdef SHOW_SECOND_HAND
  placement->second_hand_index = ((NUM_STEPS_SECOND * seconds) / 60) % NUM_STEPS_SECOND;
#endif  // SHOW_SECOND_HAND

#ifdef SHOW_DAY_CARD
  placement->day_index = time.tm_wday;
#endif  // SHOW_DAY_CARD

#ifdef SHOW_DATE_CARD
  placement->date_value = time.tm_mday;
#endif  // SHOW_DATE_CARD

#ifdef MAKE_CHRONOGRAPH
  {
    int chrono_seconds;
    if (chrono_running && !chrono_lap_paused) {
      // The chronograph is running.  Show the active elapsed time.
      chrono_seconds = (seconds - chrono_start_seconds + SECONDS_PER_DAY) % SECONDS_PER_DAY;
    } else {
      // The chronograph is paused.  Show the time it is paused on.
      chrono_seconds = chrono_hold_seconds;
    }

#ifdef SHOW_CHRONO_MINUTE_HAND
    // The chronograph minute hand rolls completely around in 30
    // minutes (not 60).
    placement->chrono_minute_hand_index = ((NUM_STEPS_CHRONO_MINUTE * chrono_seconds) / 1800) % NUM_STEPS_CHRONO_MINUTE;
#endif  // SHOW_CHRONO_MINUTE_HAND

#ifdef SHOW_CHRONO_SECOND_HAND
    placement->chrono_second_hand_index = ((NUM_STEPS_CHRONO_SECOND * chrono_seconds) / 60) % NUM_STEPS_CHRONO_SECOND;
#endif  // SHOW_CHRONO_SECOND_HAND
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

// Horizontally flips the indicated BmpContainer in-place.  Requires
// that the width be a multiple of 8 pixels.
void flip_bitmap_x(BmpContainer *image, int *cx) {
  int height = image->bmp.bounds.size.h;
  int width = image->bmp.bounds.size.w;  // multiple of 8, by our convention.
  int width_bytes = width / 8;
  int stride = image->bmp.row_size_bytes; // multiple of 4, by Pebble.
  uint8_t *data = image->bmp.addr;

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

// Vertically flips the indicated BmpContainer in-place.
void flip_bitmap_y(BmpContainer *image, int *cy) {
  int height = image->bmp.bounds.size.h;
  int stride = image->bmp.row_size_bytes; // multiple of 4.
  uint8_t *data = image->bmp.addr;

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

    GPath path;
    gpath_init(&path, &group->path_info);

    gpath_rotate_to(&path, angle);
    gpath_move_to(&path, center);

    if (group->fill != GColorClear) {
      graphics_context_set_fill_color(ctx, group->fill);
      gpath_draw_filled(ctx, &path);
    }
    if (group->outline != GColorClear) {
      graphics_context_set_stroke_color(ctx, group->outline);
      gpath_draw_outline(ctx, &path);
    }
  }
}

// Draws a given hand on the face, using the bitmap structures.
void draw_bitmap_hand(struct BitmapHandTableRow *hand, int place_x, int place_y, GContext *ctx) {
  int cx, cy;
  cx = hand->cx;
  cy = hand->cy;

  if (hand->mask_id == hand->image_id) {
    // The hand does not have a mask.  Draw the hand on top of the scene.
    BmpContainer image;
    bmp_init_container(hand->image_id, &image);
    
    if (hand->flip_x) {
      // To minimize wasteful resource usage, if the hand is symmetric
      // we can store only the bitmaps for the right half of the clock
      // face, and flip them for the left half.
      flip_bitmap_x(&image, &cx);
    }
    
    if (hand->flip_y) {
      // We can also do this vertically.
      flip_bitmap_y(&image, &cy);
    }
    
    // We make sure the dimensions of the GRect to draw into
    // are equal to the size of the bitmap--otherwise the image
    // will automatically tile.
    GRect destination = layer_get_frame(&image.layer.layer);
    
    // Place the hand's center point at place_x, place_y.
    destination.origin.x = place_x - cx;
    destination.origin.y = place_y - cy;
    
    // Specify a compositing mode to make the hands overlay on top of
    // each other, instead of the background parts of the bitmaps
    // blocking each other.

    if (hand->paint_black) {
      // Painting foreground ("white") pixels as black.
      graphics_context_set_compositing_mode(ctx, GCompOpClear);
    } else {
      // Painting foreground ("white") pixels as white.
      graphics_context_set_compositing_mode(ctx, GCompOpOr);
    }
      
    graphics_draw_bitmap_in_rect(ctx, &image.bmp, destination);
    
    bmp_deinit_container(&image);

  } else {
    // The hand has a mask, so use it to draw the hand opaquely.
    BmpContainer image, mask;
    bmp_init_container(hand->image_id, &image);
    bmp_init_container(hand->mask_id, &mask);
    
    if (hand->flip_x) {
      // To minimize wasteful resource usage, if the hand is symmetric
      // we can store only the bitmaps for the right half of the clock
      // face, and flip them for the left half.
      flip_bitmap_x(&image, &cx);
      flip_bitmap_x(&mask, NULL);
    }
    
    if (hand->flip_y) {
      // We can also do this vertically.
      flip_bitmap_y(&image, &cy);
      flip_bitmap_y(&mask, NULL);
    }
    
    GRect destination = layer_get_frame(&image.layer.layer);
    
    destination.origin.x = place_x - cx;
    destination.origin.y = place_y - cy;

    graphics_context_set_compositing_mode(ctx, GCompOpOr);
    graphics_draw_bitmap_in_rect(ctx, &mask.bmp, destination);
    
    graphics_context_set_compositing_mode(ctx, GCompOpClear);
    graphics_draw_bitmap_in_rect(ctx, &image.bmp, destination);
    
    bmp_deinit_container(&image);
    bmp_deinit_container(&mask);
  }
}
  
void hour_layer_update_callback(Layer *me, GContext* ctx) {
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

void minute_layer_update_callback(Layer *me, GContext* ctx) {
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

#ifdef SHOW_SECOND_HAND
void second_layer_update_callback(Layer *me, GContext* ctx) {
  (void)me;

#ifdef VECTOR_SECOND_HAND
  draw_vector_hand(&second_hand_vector_table, current_placement.second_hand_index,
                   NUM_STEPS_SECOND, SECOND_HAND_X, SECOND_HAND_Y, ctx);
#endif

#ifdef BITMAP_SECOND_HAND
  draw_bitmap_hand(&second_hand_bitmap_table[current_placement.second_hand_index],
                   SECOND_HAND_X, SECOND_HAND_Y, ctx);
#endif
}
#endif  // SHOW_SECOND_HAND

#ifdef SHOW_CHRONO_MINUTE_HAND
void chrono_minute_layer_update_callback(Layer *me, GContext* ctx) {
  (void)me;

#ifdef VECTOR_CHRONO_MINUTE_HAND
  draw_vector_hand(&chrono_minute_hand_vector_table, current_placement.chrono_minute_hand_index,
                   NUM_STEPS_CHRONO_MINUTE, CHRONO_MINUTE_HAND_X, CHRONO_MINUTE_HAND_Y, ctx);
#endif

#ifdef BITMAP_CHRONO_MINUTE_HAND
  draw_bitmap_hand(&chrono_minute_hand_bitmap_table[current_placement.chrono_minute_hand_index],
                   CHRONO_MINUTE_HAND_X, CHRONO_MINUTE_HAND_Y, ctx);
#endif
}
#endif  // SHOW_CHRONO_MINUTE_HAND

#ifdef SHOW_CHRONO_SECOND_HAND
void chrono_second_layer_update_callback(Layer *me, GContext* ctx) {
  (void)me;

#ifdef VECTOR_CHRONO_SECOND_HAND
  draw_vector_hand(&chrono_second_hand_vector_table, current_placement.chrono_second_hand_index,
                   NUM_STEPS_CHRONO_SECOND, CHRONO_SECOND_HAND_X, CHRONO_SECOND_HAND_Y, ctx);
#endif

#ifdef BITMAP_CHRONO_SECOND_HAND
  draw_bitmap_hand(&chrono_second_hand_bitmap_table[current_placement.chrono_second_hand_index],
                   CHRONO_SECOND_HAND_X, CHRONO_SECOND_HAND_Y, ctx);
#endif
}
#endif  // SHOW_CHRONO_SECOND_HAND

void draw_card(Layer *me, GContext* ctx, const char *text) {
  GFont font;
  GRect box;

  font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  box = layer_get_frame(me);
  box.origin.x = 0;
  box.origin.y = 0;
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  //  graphics_draw_round_rect(ctx, box, 0);

  box.origin.y -= 3;  // Determined empirically.

  graphics_text_draw(ctx, text, font, box,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}

void day_layer_update_callback(Layer *me, GContext* ctx) {
  draw_card(me, ctx, weekday_names[current_placement.day_index]);
}

void date_layer_update_callback(Layer *me, GContext* ctx) {
  draw_card(me, ctx, quick_itoa(current_placement.date_value));
}

void update_hands() {
  struct HandPlacement new_placement;

  compute_hands(&new_placement);
  if (new_placement.hour_hand_index != current_placement.hour_hand_index) {
    current_placement.hour_hand_index = new_placement.hour_hand_index;
    layer_mark_dirty(&hour_layer);
  }

  if (new_placement.minute_hand_index != current_placement.minute_hand_index) {
    current_placement.minute_hand_index = new_placement.minute_hand_index;
    layer_mark_dirty(&minute_layer);
  }

#ifdef SHOW_SECOND_HAND
  if (new_placement.second_hand_index != current_placement.second_hand_index) {
    current_placement.second_hand_index = new_placement.second_hand_index;
    layer_mark_dirty(&second_layer);
  }
#endif  // SHOW_SECOND_HAND

#ifdef MAKE_CHRONOGRAPH

#ifdef SHOW_CHRONO_MINUTE_HAND
  if (new_placement.chrono_minute_hand_index != current_placement.chrono_minute_hand_index) {
    current_placement.chrono_minute_hand_index = new_placement.chrono_minute_hand_index;
    layer_mark_dirty(&chrono_minute_layer);
  }
#endif  // SHOW_CHRONO_MINUTE_HAND

#ifdef SHOW_CHRONO_SECOND_HAND
  if (new_placement.chrono_second_hand_index != current_placement.chrono_second_hand_index) {
    current_placement.chrono_second_hand_index = new_placement.chrono_second_hand_index;
    layer_mark_dirty(&chrono_second_layer);
  }
#endif  // SHOW_CHRONO_SECOND_HAND
#endif  // MAKE_CHRONOGRAPH

#ifdef SHOW_DAY_CARD
  if (new_placement.day_index != current_placement.day_index) {
    current_placement.day_index = new_placement.day_index;
    layer_mark_dirty(&day_layer);
  }
#endif  // SHOW_DAY_CARD

#ifdef SHOW_DATE_CARD
  if (new_placement.date_value != current_placement.date_value) {
    current_placement.date_value = new_placement.date_value;
    layer_mark_dirty(&date_layer);
  }
#endif  // SHOW_DATE_CARD
}

// Compute new hand positions once a minute (or once a second).
void handle_tick(AppContextRef ctx, PebbleTickEvent *t) {
  (void)t;
  (void)ctx;

  update_hands();
}

// Forward references.
void stopped_click_config_provider(ClickConfig **config, Window *window);
void started_click_config_provider(ClickConfig **config, Window *window);

void chrono_start_stop_handler(ClickRecognizerRef recognizer, Window *window) {
  PblTm time;
  int seconds;

  seconds = get_time_seconds(&time);

  // The start/stop button was pressed.
  if (chrono_running) {
    // If the chronograph is currently running, this means to stop (or
    // pause).
    chrono_hold_seconds = seconds - chrono_start_seconds;
    chrono_running = false;
    chrono_lap_paused = false;
    vibes_enqueue_custom_pattern(tap);
    update_hands();

    // We change the click config provider according to the chrono run
    // state.  When the chrono is stopped, we listen for a different
    // set of buttons than when it is started.
    window_set_click_config_provider(window, (ClickConfigProvider)stopped_click_config_provider);
  } else {
    // If the chronograph is not currently running, this means to
    // start, from the currently showing Chronograph time.
    chrono_start_seconds = seconds - chrono_hold_seconds;
    chrono_running = true;
    vibes_enqueue_custom_pattern(tap);
    update_hands();

    window_set_click_config_provider(window, (ClickConfigProvider)started_click_config_provider);
  }
}

void chrono_lap_button() {
  int seconds;

  if (chrono_lap_paused) {
    // If we were already paused, this resumes the motion, jumping
    // ahead to the currently elapsed time.
    chrono_lap_paused = false;
    vibes_enqueue_custom_pattern(tap);
    update_hands();
  } else {
    // If we were not already paused, this pauses the hands here (but
    // does not stop the timer).
    PblTm time;
    seconds = get_time_seconds(&time);
    chrono_hold_seconds = seconds - chrono_start_seconds;
    chrono_lap_paused = true;
    vibes_enqueue_custom_pattern(tap);
    update_hands();
  }
}

void chrono_reset_button() {
  // Resets the chronometer to 0 time.
  chrono_running = false;
  chrono_lap_paused = false;
  chrono_start_seconds = 0;
  chrono_hold_seconds = 0;
  vibes_double_pulse();
  update_hands();
}

void chrono_lap_handler(ClickRecognizerRef recognizer, Window *window) {
  // The lap/reset button was pressed (briefly).

  // We only do anything here if the chronograph is currently running.
  if (chrono_running) {
    chrono_lap_button();
  }
}

void chrono_lap_or_reset_handler(ClickRecognizerRef recognizer, Window *window) {
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
void stopped_click_config_provider(ClickConfig **config, Window *window) {
  // single click config:
  config[BUTTON_ID_UP]->click.handler = (ClickHandler)chrono_start_stop_handler;

  config[BUTTON_ID_DOWN]->click.handler = NULL;

  // long click config:
  config[BUTTON_ID_DOWN]->long_click.handler = (ClickHandler)chrono_lap_or_reset_handler;
  config[BUTTON_ID_DOWN]->long_click.delay_ms = 700;
}

// Enable the set of buttons active while the chrono is running.
void started_click_config_provider(ClickConfig **config, Window *window) {
  // single click config:
  config[BUTTON_ID_UP]->click.handler = (ClickHandler)chrono_start_stop_handler;

  config[BUTTON_ID_DOWN]->click.handler = (ClickHandler)chrono_lap_handler;

  // It's important to disable the lock_click handler while the chrono
  // is running, so that the normal click handler (above) can be
  // immediately responsive.  If we leave the long_click handler
  // active, then the underlying SDK has to wait the full 700 ms to
  // differentiate a long_click from a click, which makes the lap
  // response sluggish.
  config[BUTTON_ID_DOWN]->long_click.handler = NULL;
  config[BUTTON_ID_DOWN]->long_click.delay_ms = 0;
}

void handle_init(AppContextRef ctx) {
  int i;
  (void)ctx;

  window_init(&window, WATCH_NAME);
  window_set_fullscreen(&window, true);
  window_stack_push(&window, true /* Animated */);

  compute_hands(&current_placement);

  resource_init_current_app(&APP_RESOURCES);

  bmp_init_container(RESOURCE_ID_CLOCK_FACE, &clock_face_container);
  layer_add_child(&window.layer, &clock_face_container.layer.layer);

#ifdef SHOW_DAY_CARD
  layer_init(&day_layer, GRect(DAY_CARD_X - 15, DAY_CARD_Y - 8, 31, 19));
  day_layer.update_proc = &day_layer_update_callback;
  layer_add_child(&window.layer, &day_layer);
#endif  // SHOW_DAY_CARD

#ifdef SHOW_DATE_CARD
  layer_init(&date_layer, GRect(DATE_CARD_X - 15, DATE_CARD_Y - 8, 31, 19));
  date_layer.update_proc = &date_layer_update_callback;
  layer_add_child(&window.layer, &date_layer);
#endif  // SHOW_DATE_CARD

  // Init all of the hands, taking care to arrange them in the correct
  // stacking order.
  for (i = 0; stacking_order[i] != STACKING_ORDER_DONE; ++i) {
    switch (stacking_order[i]) {
    case STACKING_ORDER_HOUR:
      layer_init(&hour_layer, window.layer.frame);
      hour_layer.update_proc = &hour_layer_update_callback;
      layer_add_child(&window.layer, &hour_layer);
      break;

    case STACKING_ORDER_MINUTE:
      layer_init(&minute_layer, window.layer.frame);
      minute_layer.update_proc = &minute_layer_update_callback;
      layer_add_child(&window.layer, &minute_layer);
      break;

    case STACKING_ORDER_SECOND:
#ifdef SHOW_SECOND_HAND
      layer_init(&second_layer, window.layer.frame);
      second_layer.update_proc = &second_layer_update_callback;
      layer_add_child(&window.layer, &second_layer);
#endif  // SHOW_SECOND_HAND
      break;

    case STACKING_ORDER_CHRONO_MINUTE:
#ifdef SHOW_CHRONO_MINUTE_HAND
      layer_init(&chrono_minute_layer, window.layer.frame);
      chrono_minute_layer.update_proc = &chrono_minute_layer_update_callback;
      layer_add_child(&window.layer, &chrono_minute_layer);
#endif  // SHOW_CHRONO_MINUTE_HAND
      break;

    case STACKING_ORDER_CHRONO_SECOND:
#ifdef SHOW_CHRONO_SECOND_HAND
      layer_init(&chrono_second_layer, window.layer.frame);
      chrono_second_layer.update_proc = &chrono_second_layer_update_callback;
      layer_add_child(&window.layer, &chrono_second_layer);
#endif  // SHOW_CHRONO_SECOND_HAND
      break;
    }
  }

#ifdef MAKE_CHRONOGRAPH
 window_set_click_config_provider(&window, (ClickConfigProvider)stopped_click_config_provider);
#endif  // MAKE_CHRONOGRAPH
}


void handle_deinit(AppContextRef ctx) {
  (void)ctx;

  bmp_deinit_container(&clock_face_container);

  // Is this needed?  Official Pebble examples don't seem to do it.
  //  text_layer_deinit(&date_layer);
}


void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,
    .tick_info = {
      .tick_handler = &handle_tick,
#if defined(FAST_TIME) || defined(SHOW_SECOND_HAND)
      .tick_units = SECOND_UNIT
#else
      .tick_units = MINUTE_UNIT
#endif
    }
  };
  app_event_loop(params, &handlers);
}
