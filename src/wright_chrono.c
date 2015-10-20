#include "wright_chrono.h"

// The code in this file is used only when enabling Chronograph
// features, including start/stop and lap buttons on the chrono dials.
// Specifically, this code is used only for Rosewright Chronograph.

#ifdef MAKE_CHRONOGRAPH

// The screen width of the chrono_digital_layer.  Always 144, even on Chalk.
#define DIGITAL_LAYER_WIDTH 144

// The height of each row of the text.
#ifdef PBL_ROUND
#define LAP_HEIGHT 28
#else
#define LAP_HEIGHT 30
#endif

const GSize chrono_dial_size = { 56, 56 };

struct HandCache chrono_minute_cache;
struct HandCache chrono_second_cache;
struct HandCache chrono_tenth_cache;

BitmapWithData chrono_dial_white;

struct ResourceCache chrono_second_resource_cache[CHRONO_SECOND_RESOURCE_CACHE_SIZE + CHRONO_SECOND_MASK_RESOURCE_CACHE_SIZE];
size_t chrono_second_resource_cache_size = CHRONO_SECOND_RESOURCE_CACHE_SIZE +  + CHRONO_SECOND_MASK_RESOURCE_CACHE_SIZE;

// This window is pushed on top of the chrono dial to display the
// readout in digital form for ease of recording.
Window *chrono_digital_window;
#ifdef PBL_SDK_3
StatusBarLayer *chrono_status_bar_layer = NULL;
Layer *chrono_digital_contents_layer = NULL;
#endif  // PBL_SDK_3
TextLayer *chrono_digital_current_layer = NULL;
TextLayer *chrono_digital_laps_layer[CHRONO_MAX_LAPS];
Layer *chrono_digital_line_layer = NULL;
bool chrono_digital_window_showing = false;
AppTimer *chrono_digital_timer = NULL;

#define CHRONO_DIGITAL_BUFFER_SIZE 11 // Enough space for "hh:mm:ss.d" plus a null byte
char chrono_current_buffer[CHRONO_DIGITAL_BUFFER_SIZE];
char chrono_laps_buffer[CHRONO_MAX_LAPS][CHRONO_DIGITAL_BUFFER_SIZE];

#define CHRONO_DIGITAL_TICK_MS 100 // Every 0.1 seconds

// True if we're currently showing tenths, false if we're currently
// showing hours, in the chrono subdial.
bool chrono_dial_shows_tenths = false;

int sweep_chrono_seconds_ms = 60 * 1000 / NUM_STEPS_CHRONO_SECOND;

ChronoData chrono_data = { false, false, 0, 0, { 0, 0, 0, 0 } };
ChronoData saved_chrono_data;

static const uint32_t tap_segments[] = { 50 };
VibePattern tap = {
  tap_segments,
  1,
};

// Returns the number of milliseconds since midnight, UTC.
unsigned int get_time_ms() {
  time_t gmt;
  uint16_t t_ms;
  unsigned int ms_utc;

  time_ms(&gmt, &t_ms);
  ms_utc = (unsigned int)((gmt % SECONDS_PER_DAY) * 1000 + t_ms);

#ifdef FAST_TIME
  ms_utc *= 67;
#endif  // FAST_TIME

  return ms_utc;
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

void compute_chrono_hands(unsigned int ms, struct HandPlacement *placement) {
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
    invalidate_clock_face();
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

void draw_chrono_dial(GContext *ctx) {
  //  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "draw_chrono_dial");
  
  if (config.chrono_dial != CDM_off) {
#ifdef PBL_PLATFORM_APLITE
    BitmapWithData chrono_dial_black;
    if (chrono_dial_shows_tenths) {
      chrono_dial_black = rle_bwd_create(RESOURCE_ID_CHRONO_DIAL_TENTHS_BLACK);
    } else {
      chrono_dial_black = rle_bwd_create(RESOURCE_ID_CHRONO_DIAL_HOURS_BLACK);
    }
    if (chrono_dial_black.bitmap == NULL) {
      bwd_destroy(&chrono_dial_black);
      trigger_memory_panic(__LINE__);
    }
#endif  // PBL_PLATFORM_APLITE

    // In Basalt, we only load the "white" image.
    if (chrono_dial_white.bitmap == NULL) {
      if (chrono_dial_shows_tenths) {
        chrono_dial_white = rle_bwd_create(RESOURCE_ID_CHRONO_DIAL_TENTHS_WHITE);
      } else {
        chrono_dial_white = rle_bwd_create(RESOURCE_ID_CHRONO_DIAL_HOURS_WHITE);
      }
      if (chrono_dial_white.bitmap == NULL) {
        trigger_memory_panic(__LINE__);
        return;
      }
    
      // We apply the color scheme as needed.
      remap_colors_clock(&chrono_dial_white);
    }
  
    int x = chrono_tenth_hand_def.place_x - chrono_dial_size.w / 2;
    int y = chrono_tenth_hand_def.place_y - chrono_dial_size.h / 2;
    
    GRect destination = GRect(x, y, chrono_dial_size.w, chrono_dial_size.h);

#ifdef PBL_PLATFORM_APLITE
    graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode ^ APLITE_INVERT].paint_fg);
    graphics_draw_bitmap_in_rect(ctx, chrono_dial_black.bitmap, destination);
    graphics_context_set_compositing_mode(ctx, draw_mode_table[config.draw_mode ^ APLITE_INVERT].paint_bg);
    graphics_draw_bitmap_in_rect(ctx, chrono_dial_white.bitmap, destination);
#else  // PBL_PLATFORM_APLITE
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, chrono_dial_white.bitmap, destination);
#endif  // PBL_PLATFORM_APLITE

    if (!keep_assets) {
      bwd_destroy(&chrono_dial_white);
    }
#ifdef PBL_PLATFORM_APLITE
    bwd_destroy(&chrono_dial_black);
#endif  // PBL_PLATFORM_APLITE
  }
}


void update_chrono_hands(struct HandPlacement *new_placement) {
#ifdef ENABLE_CHRONO_MINUTE_HAND
  if (new_placement->chrono_minute_hand_index != current_placement.chrono_minute_hand_index) {
    current_placement.chrono_minute_hand_index = new_placement->chrono_minute_hand_index;
    layer_mark_dirty(clock_hands_layer);
  }
#endif  // ENABLE_CHRONO_MINUTE_HAND

#ifdef ENABLE_CHRONO_SECOND_HAND
  if (new_placement->chrono_second_hand_index != current_placement.chrono_second_hand_index) {
    current_placement.chrono_second_hand_index = new_placement->chrono_second_hand_index;
    layer_mark_dirty(clock_hands_layer);
  }
#endif  // ENABLE_CHRONO_SECOND_HAND

#ifdef ENABLE_CHRONO_TENTH_HAND
  if (new_placement->chrono_tenth_hand_index != current_placement.chrono_tenth_hand_index) {
    current_placement.chrono_tenth_hand_index = new_placement->chrono_tenth_hand_index;
    layer_mark_dirty(clock_hands_layer);
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
}

void chrono_start_stop_handler(ClickRecognizerRef recognizer, void *context) {
  Window *window = (Window *)context;
  unsigned int ms = get_time_ms();

  // The start/stop button was pressed.
  if (chrono_data.running) {
    // If the chronograph is currently running, this means to stop (or
    // pause).
    chrono_data.hold_ms = ms - chrono_data.start_ms;
    chrono_data.running = false;
    chrono_data.lap_paused = false;
    vibes_enqueue_custom_pattern(tap);
    update_hands(NULL);
    reset_tick_timer();

    // We change the click config provider according to the chrono run
    // state.  When the chrono is stopped, we listen for a different
    // set of buttons than when it is started.
    window_set_click_config_provider(window, &stopped_click_config_provider);
    if (chrono_digital_window != NULL) {
      window_set_click_config_provider(chrono_digital_window, &stopped_click_config_provider);
    }
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
    reset_tick_timer();

    window_set_click_config_provider(window, &started_click_config_provider);
    if (chrono_digital_window != NULL) {
      window_set_click_config_provider(chrono_digital_window, &started_click_config_provider);
    }
  }
}

void chrono_lap_button() {
  unsigned int ms;
 
  ms = get_time_ms();

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
  reset_tick_timer();
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
  
void chrono_digital_line_layer_update_callback(Layer *me, GContext *ctx) {
  GRect destination = layer_get_bounds(me);

  // Paint the background black.
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, destination, 0, GCornerNone);
}

void chrono_digital_window_load_handler(struct Window *window) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "chrono digital loads");

  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);


#ifdef PBL_SDK_3
  Layer *chrono_digital_window_layer = window_get_root_layer(chrono_digital_window);
  chrono_status_bar_layer = status_bar_layer_create();
  if (chrono_status_bar_layer == NULL) {
    trigger_memory_panic(__LINE__);
    return;
  }    
  layer_add_child(chrono_digital_window_layer, status_bar_layer_get_layer(chrono_status_bar_layer));
  
  chrono_digital_contents_layer = layer_create(GRect((SCREEN_WIDTH - DIGITAL_LAYER_WIDTH) / 2, STATUS_BAR_LAYER_HEIGHT, DIGITAL_LAYER_WIDTH, SCREEN_HEIGHT - STATUS_BAR_LAYER_HEIGHT));
  if (chrono_digital_contents_layer == NULL) {
    trigger_memory_panic(__LINE__);
    return;
  }    
  layer_add_child(chrono_digital_window_layer, chrono_digital_contents_layer);

#else  // PBL_SDK_3
  // On SDK 2.0 and before, we don't create a separate contents layer;
  // we just use the root layer.
  Layer *chrono_digital_contents_layer = window_get_root_layer(chrono_digital_window);
  
#endif  // PBL_SDK_3

  chrono_digital_current_layer = text_layer_create(GRect(25, LAP_HEIGHT * CHRONO_MAX_LAPS, 94, LAP_HEIGHT));
  if (chrono_digital_current_layer == NULL) {
    trigger_memory_panic(__LINE__);
    return;
  }    
  int i;
  for (i = 0; i < CHRONO_MAX_LAPS; ++i) {
    chrono_digital_laps_layer[i] = text_layer_create(GRect(25, LAP_HEIGHT * i, 94, LAP_HEIGHT));
    if (chrono_digital_laps_layer[i] == NULL) {
      trigger_memory_panic(__LINE__);
      return;
    }    

    text_layer_set_text(chrono_digital_laps_layer[i], chrono_laps_buffer[i]);
    text_layer_set_text_color(chrono_digital_laps_layer[i], GColorBlack);
    text_layer_set_text_alignment(chrono_digital_laps_layer[i], GTextAlignmentRight);
    text_layer_set_overflow_mode(chrono_digital_laps_layer[i], GTextOverflowModeFill);
    text_layer_set_font(chrono_digital_laps_layer[i], font);
    layer_add_child(chrono_digital_contents_layer, (Layer *)chrono_digital_laps_layer[i]);
  }

  text_layer_set_text(chrono_digital_current_layer, chrono_current_buffer);
  text_layer_set_text_color(chrono_digital_current_layer, GColorBlack);
  text_layer_set_text_alignment(chrono_digital_current_layer, GTextAlignmentRight);
  text_layer_set_overflow_mode(chrono_digital_current_layer, GTextOverflowModeFill);
  text_layer_set_font(chrono_digital_current_layer, font);
  layer_add_child(chrono_digital_contents_layer, (Layer *)chrono_digital_current_layer);

  chrono_digital_line_layer = layer_create(GRect(0, LAP_HEIGHT * CHRONO_MAX_LAPS + 1, SCREEN_WIDTH, 1));
  if (chrono_digital_line_layer == NULL) {
    trigger_memory_panic(__LINE__);
    return;
  }    
  layer_set_update_proc(chrono_digital_line_layer, &chrono_digital_line_layer_update_callback);
  layer_add_child(chrono_digital_contents_layer, (Layer *)chrono_digital_line_layer);
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
    layer_destroy(chrono_digital_line_layer);
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

#ifdef PBL_SDK_3
  if (chrono_digital_contents_layer != NULL) {
    layer_destroy(chrono_digital_contents_layer);
    chrono_digital_contents_layer = NULL;
  }
  if (chrono_status_bar_layer != NULL) {
    status_bar_layer_destroy(chrono_status_bar_layer);
    chrono_status_bar_layer = NULL;
  }
#endif  // PDL_SDK_3
}

void push_chrono_digital_handler(ClickRecognizerRef recognizer, void *context) {
  //  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "push chrono digital");
  if (!chrono_digital_window_showing) {

    // Release some caches and repack our memory allocations before we
    // push the window.
    recreate_all_objects();
    
    // If we don't already have a chrono_digital_window object
    // created, create it now.
    if (chrono_digital_window == NULL) {
      chrono_digital_window = window_create();
      if (chrono_digital_window == NULL) {
	trigger_memory_panic(__LINE__);
	return;
      }

      struct WindowHandlers chrono_digital_window_handlers;
      memset(&chrono_digital_window_handlers, 0, sizeof(chrono_digital_window_handlers));
      chrono_digital_window_handlers.load = chrono_digital_window_load_handler;
      chrono_digital_window_handlers.appear = chrono_digital_window_appear_handler;
      chrono_digital_window_handlers.disappear = chrono_digital_window_disappear_handler;
      chrono_digital_window_handlers.unload = chrono_digital_window_unload_handler;
      window_set_window_handlers(chrono_digital_window, chrono_digital_window_handlers);
    }

    // We'll push without animation, mainly so the pop also comes
    // without animation, which is important because the main window
    // needs to be able to grab the framebuffer once it draws, and
    // there's no way to ask the Pebble SDK when the window has
    // finished animating into its proper place (to know when it's
    // safe to grab the framebuffer).
    window_stack_push(chrono_digital_window, false);
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

void chrono_set_click_config(struct Window *window) {
  if (chrono_data.running) {
    window_set_click_config_provider(window, &started_click_config_provider);
  } else {
    window_set_click_config_provider(window, &stopped_click_config_provider);
  }
}


void update_chrono_laps_time() {
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
}

void record_chrono_lap(int chrono_ms) {
  // Lose the first one.
  memmove(&chrono_data.laps[0], &chrono_data.laps[1], sizeof(chrono_data.laps[0]) * (CHRONO_MAX_LAPS - 1));
  chrono_data.laps[CHRONO_MAX_LAPS - 1] = chrono_ms;
  update_chrono_laps_time();
}

void update_chrono_current_time() {
  unsigned int ms = get_time_ms();
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

void reset_chrono_digital_timer() {
  if (chrono_digital_timer != NULL) {
    app_timer_cancel(chrono_digital_timer);
    chrono_digital_timer = NULL;
  }
  update_chrono_current_time();
  if (chrono_digital_window_showing && chrono_data.running) {
    // Set the timer for the next update.
    chrono_digital_timer = app_timer_register(CHRONO_DIGITAL_TICK_MS, &handle_chrono_digital_timer, 0);
  }
}

void
load_chrono_data() {
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
      unsigned int ms = get_time_ms();
      unsigned int chrono_ms = (ms - chrono_data.start_ms + MS_PER_DAY) % MS_PER_DAY;
      app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "Modulated start_ms from %u to %u", chrono_data.start_ms, ms - chrono_ms);
      chrono_data.start_ms = ms - chrono_ms;
    }
    
    update_chrono_laps_time();
  } else {
    app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "Wrong previous chrono_data size or no previous data.");
  }
}

void save_chrono_data() {
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
}

void create_chrono_objects() {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "create_chrono_objects");
  hand_cache_init(&chrono_minute_cache);
  hand_cache_init(&chrono_second_cache);
  hand_cache_init(&chrono_tenth_cache);

  update_chrono_laps_time();
}


void destroy_chrono_objects() {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "destroy_chrono_objects");

  if (chrono_digital_window != NULL) {
    window_destroy(chrono_digital_window);
    chrono_digital_window = NULL;
  }

  hand_cache_destroy(&chrono_minute_cache);
  hand_cache_destroy(&chrono_second_cache);
  hand_cache_destroy(&chrono_tenth_cache);

  bwd_destroy(&chrono_dial_white);
}


#endif  // MAKE_CHRONOGRAPH
