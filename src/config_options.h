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
  CK_week_numbering = 19,
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
  DWM_yday = 4,
  DWM_week = 5,

  // Uses date_lang_font.
  DWM_weekday = 6,
  DWM_month = 7,
  DWM_ampm = 8,

  // No longer used.
  DWM_moon_unused = 9,

  // Uses date_numeric_font.
  DWM_step_count = 10,
  DWM_step_count_10 = 11,
  DWM_active_time = 12,
  DWM_walked_distance = 13,
  DWM_sleep_time = 14,
  DWM_sleep_restful_time = 15,
  DWM_calories_burned = 16,
  DWM_heart_rate = 17,

  // Debug options.  These are always at the end.
  DWM_debug_heap_free = 201,
  DWM_debug_memory_panic_count = 202,
  DWM_debug_resource_reads = 203,
  DWM_debug_cache_hits = 204,
  DWM_debug_cache_total_size = 205,
} DateWindowMode;

typedef enum {
  TSM_off = 0,
  TSM_pebble_label = 1,
  TSM_moon_phase = 2,
} TopSubdialMode;

typedef enum {
  WNM_mon_4 = 0,  // EU, Asia, Oceania
  WNM_sun_1 = 1,  // Americas, China, Japan
  WNM_sat_1 = 2,  // Middle East
} WeekNumberingMode;

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

  WeekNumberingMode week_numbering;
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
