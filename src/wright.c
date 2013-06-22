#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "../resources/src/generated_config.h"
#include "hand_table.h"
#include "../resources/src/generated_table.c"


PBL_APP_INFO(MY_UUID,
             WATCH_NAME, "drwrose",
             1, 0, /* App version */
             DEFAULT_MENU_ICON,
             APP_INFO_WATCH_FACE);

#define SCREEN_WIDTH 144
#define SCREEN_HEIGHT 168

// The center of rotation of the hands.
#define HANDS_CX (SCREEN_WIDTH / 2)
#define HANDS_CY (SCREEN_HEIGHT / 2)

Window window;

BmpContainer clock_face_container;

Layer hour_layer;
Layer minute_layer;
Layer second_layer;

Layer day_layer;  // day of the week (abbr)
Layer date_layer; // numeric date of the month

// This structure keeps track of the things that change on the visible
// watch face and their current state.
struct HandPlacement {
  int hour_hand_index;
  int minute_hand_index;
  int second_hand_index;
  int day_index;
  int date_value;
};

struct HandPlacement current_placement;

// Determines the specific hand bitmaps that should be displayed based
// on the current time.
void compute_hands(struct HandPlacement *placement) {
  int seconds;
  PblTm time;

  get_time(&time);

  seconds = (time.tm_hour * 60 + time.tm_min) * 60 + time.tm_sec;

#ifdef FAST_TIME
  time.tm_wday = seconds % 7;
  time.tm_mday = (seconds % 31) + 1;
  seconds *= 67;
#endif  // FAST_TIME

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

#if 0
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

// Draws a given hand on the face.
void draw_hand(struct HandTable *hand, GContext *ctx) {
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
    
    // Place the hand's center point at HANDS_CX, HANDS_CY.
    destination.origin.x = HANDS_CX - cx;
    destination.origin.y = HANDS_CY - cy;
    
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
    
    destination.origin.x = HANDS_CX - cx;
    destination.origin.y = HANDS_CY - cy;

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

  draw_hand(&hour_hand_table[current_placement.hour_hand_index], ctx);
}

void minute_layer_update_callback(Layer *me, GContext* ctx) {
  (void)me;

  draw_hand(&minute_hand_table[current_placement.minute_hand_index], ctx);
}

#ifdef SHOW_SECOND_HAND
void second_layer_update_callback(Layer *me, GContext* ctx) {
  (void)me;

  draw_hand(&second_hand_table[current_placement.second_hand_index], ctx);
}
#endif  // SHOW_SECOND_HAND

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

// Compute new hand positions once a minute.
void handle_tick(AppContextRef ctx, PebbleTickEvent *t) {
  struct HandPlacement new_placement;
  (void)t;
  (void)ctx;

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

void handle_init(AppContextRef ctx) {
  (void)ctx;

  window_init(&window, WATCH_NAME);
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

  layer_init(&hour_layer, window.layer.frame);
  hour_layer.update_proc = &hour_layer_update_callback;
  layer_add_child(&window.layer, &hour_layer);

  layer_init(&minute_layer, window.layer.frame);
  minute_layer.update_proc = &minute_layer_update_callback;
  layer_add_child(&window.layer, &minute_layer);

#ifdef SHOW_SECOND_HAND
  layer_init(&second_layer, window.layer.frame);
  second_layer.update_proc = &second_layer_update_callback;
  layer_add_child(&window.layer, &second_layer);
#endif  // SHOW_SECOND_HAND
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
