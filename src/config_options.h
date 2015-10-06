#ifndef CONFIG_OPTIONS_H
#define CONFIG_OPTIONS_H

#include <pebble.h>
#include "../resources/generated_config.h"

// These keys are used to communicate with Javascript and must match
// the corresponding index numbers in appinfo.json.in.
typedef enum {
  CK_battery_gauge = 0,
  CK_bluetooth_indicator = 1,
  CK_second_hand = 2,
  CK_hour_buzzer = 3,
  CK_draw_mode = 4,
  CK_chrono_dial = 5,
  CK_sweep_seconds = 6,
  CK_display_lang = 7,
  CK_face_index = 8,
  CK_date_window_a = 9,
  CK_date_window_b = 10,
  CK_date_window_c = 11,
  CK_date_window_d = 12,
  CK_bluetooth_buzzer = 13,
  CK_lunar_background = 14,
  CK_lunar_direction = 15,
  CK_color_mode = 16,
  CK_top_subdial = 17,
  CK_show_debug = 18,
} ConfigKey;

typedef enum {
  IM_off = 0,
  IM_when_needed = 1,
  IM_always = 2,
  IM_digital = 3,
} IndicatorMode;

typedef enum {
  CDM_off = 0,
  CDM_tenths = 1,
  CDM_hours = 2,
  CDM_dual = 3,
} ChronoDialMode;

typedef enum {
  DWM_off = 0,

  // Uses date_numeric_font.
  DWM_identify = 1,
  DWM_date = 2,
  DWM_year = 3,

  // Uses date_lang_font.
  DWM_weekday = 4,
  DWM_month = 5,
  DWM_ampm = 6,
  
  // Special case.
  DWM_moon_unused = 7,

  // Debug options.
  DWM_debug_heap_free = 8,
  DWM_debug_memory_panic_count = 9,
  DWM_debug_resource_reads = 10,
  DWM_debug_cache_hits = 11,
  DWM_debug_cache_total_size = 12,
} DateWindowMode;

typedef enum {
  TSM_off = 0,
  TSM_pebble_label = 1,
  TSM_moon_phase = 2,
} TopSubdialMode;

typedef struct {
  IndicatorMode battery_gauge;
  IndicatorMode bluetooth_indicator;
  bool second_hand;
  bool hour_buzzer;

  bool bluetooth_buzzer;
  unsigned char draw_mode;
  ChronoDialMode chrono_dial;
  bool sweep_seconds;

  unsigned char display_lang;
  unsigned char face_index;

  DateWindowMode date_windows[NUM_DATE_WINDOWS];

  bool lunar_background;   // true for always-dark background.
  bool lunar_direction;    // true for southern hemisphere (left-to-right).
  unsigned char color_mode;
  TopSubdialMode top_subdial;
  bool show_debug;
} __attribute__((__packed__)) ConfigOptions;

extern ConfigOptions config;

void init_default_options();
void save_config();
void load_config();

void dropped_config_handler(AppMessageResult reason, void *context);
void receive_config_handler(DictionaryIterator *received, void *context);

void apply_config();  // implemented in the main program

#ifdef SCREENSHOT_BUILD
void config_set_click_config(struct Window *window);
#endif // SCREENSHOT_BUILD

#endif  // CONFIG_OPTIONS_H
