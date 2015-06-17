#include "config_options.h"
#include "wright.h"

ConfigOptions config;

// Defaults the default values of the config options.  Note that these
// defaults are used only if the Pebble is not connected to the phone
// at the time of launch; otherwise, the defaults in pebble-js-app.js
// are used instead.
static ConfigOptions default_options = {
  DEFAULT_BATTERY_GAUGE, DEFAULT_BLUETOOTH, ENABLE_SECOND_HAND, ENABLE_HOUR_BUZZER,
  true, 0, CDM_tenths, false,
  0, DEFAULT_FACE_INDEX,
  { DEFAULT_DATE_WINDOWS },
  DEFAULT_LUNAR_BACKGROUND, false, 0, DEFAULT_TOP_SUBDIAL,
};

void sanitize_config() {
  // Ensures that the newly-loaded config parameters are within a
  // reasonable range for the program and won't cause crashes.
  config.battery_gauge = config.battery_gauge % (IM_digital + 1);
  config.bluetooth_indicator = config.bluetooth_indicator % (IM_always + 1);
  config.draw_mode = config.draw_mode & 1;
  config.chrono_dial = config.chrono_dial % (CDM_dual + 1);
  config.display_lang = config.display_lang % num_langs;
  config.face_index = config.face_index % NUM_FACES;
  for (int i = 0; i < NUM_DATE_WINDOWS; ++i) {
    config.date_windows[i] = config.date_windows[i] % (DWM_moon + 1);
  }
  config.top_subdial = config.top_subdial % (TSM_moon_phase + 1);
  config.color_mode = config.color_mode % NUM_FACE_COLORS;
}

#ifndef NDEBUG
const char *show_config() {
#define CONFIG_BUFFER_SIZE 100
  static char buffer[CONFIG_BUFFER_SIZE];
  snprintf(buffer, CONFIG_BUFFER_SIZE, "bat: %d, bt: %d, sh: %d, hb: %d, bb: %d, dm: %d, cm: %d, cd: %d, sw: %d, dl: %d, fi: %d, lb: %d, ld: %d, dw: ", config.battery_gauge, config.bluetooth_indicator, config.second_hand, config.hour_buzzer, config.bluetooth_buzzer, config.draw_mode, config.color_mode, config.chrono_dial, config.sweep_seconds, config.display_lang, config.face_index, config.lunar_background, config.lunar_direction);

  if (NUM_DATE_WINDOWS > 0) {
    char b2[12];
    snprintf(b2, 12, "%d", config.date_windows[0]);
    strncat(buffer, b2, CONFIG_BUFFER_SIZE);
    for (int i = 1; i < NUM_DATE_WINDOWS; ++i) {
      snprintf(b2, 12, ",%d", config.date_windows[i]);
      strncat(buffer, b2, CONFIG_BUFFER_SIZE);
    }
  }
  
  return buffer;
}
#endif  // NDEBUG

void save_config() {
  int wrote = persist_write_data(PERSIST_KEY, &config, sizeof(config));
  if (wrote == sizeof(config)) {
    app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "Saved config (%d, %d): %s", PERSIST_KEY, sizeof(config), show_config());
  } else {
    app_log(APP_LOG_LEVEL_ERROR, __FILE__, __LINE__, "Error saving config (%d, %d): %d", PERSIST_KEY, sizeof(config), wrote);
  }
}

void load_config() {
  config = default_options;

  ConfigOptions local_config;
  int read_size = persist_read_data(PERSIST_KEY, &local_config, sizeof(local_config));
  if (read_size == sizeof(local_config)) {
    config = local_config;
    app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "Loaded config (%d, %d): %s", PERSIST_KEY, sizeof(config), show_config());
  } else {
    app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "No previous config (%d, %d): %d", PERSIST_KEY, sizeof(config), read_size);
  }

  sanitize_config();
}


void dropped_config_handler(AppMessageResult reason, void *context) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "dropped message: 0x%04x", reason);
}

void receive_config_handler(DictionaryIterator *received, void *context) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "receive_config_handler");
  ConfigOptions orig_config = config;

  Tuple *battery_gauge = dict_find(received, CK_battery_gauge);
  if (battery_gauge != NULL) {
    config.battery_gauge = (IndicatorMode)battery_gauge->value->int32;
  }

  Tuple *bluetooth_indicator = dict_find(received, CK_bluetooth_indicator);
  if (bluetooth_indicator != NULL) {
    config.bluetooth_indicator = (IndicatorMode)bluetooth_indicator->value->int32;
  }

  Tuple *second_hand = dict_find(received, CK_second_hand);
  if (second_hand != NULL) {
    config.second_hand = (second_hand->value->int32 != 0);
  }

  Tuple *hour_buzzer = dict_find(received, CK_hour_buzzer);
  if (hour_buzzer != NULL) {
    config.hour_buzzer = (hour_buzzer->value->int32 != 0);
  }

  Tuple *bluetooth_buzzer = dict_find(received, CK_bluetooth_buzzer);
  if (bluetooth_buzzer != NULL) {
    config.bluetooth_buzzer = (bluetooth_buzzer->value->int32 != 0);
  }

  Tuple *draw_mode = dict_find(received, CK_draw_mode);
  if (draw_mode != NULL) {
    config.draw_mode = draw_mode->value->int32;
  }

  Tuple *color_mode = dict_find(received, CK_color_mode);
  if (color_mode != NULL) {
    config.color_mode = color_mode->value->int32;
  }

  Tuple *chrono_dial = dict_find(received, CK_chrono_dial);
  if (chrono_dial != NULL) {
    config.chrono_dial = (ChronoDialMode)chrono_dial->value->int32;
  }

  Tuple *sweep_seconds = dict_find(received, CK_sweep_seconds);
  if (sweep_seconds != NULL) {
    config.sweep_seconds = sweep_seconds->value->int32;
  }

  Tuple *display_lang = dict_find(received, CK_display_lang);
  if (display_lang != NULL) {
    config.display_lang = display_lang->value->int32;
  }

  Tuple *face_index = dict_find(received, CK_face_index);
  if (face_index != NULL) {
    config.face_index = face_index->value->int32;
  }

  for (int i = 0; i < NUM_DATE_WINDOWS; ++i) {
    char key = DATE_WINDOW_KEYS[i];
    Tuple *date_window_x = dict_find(received, CK_date_window_a + (key - 'a'));
    if (date_window_x != NULL) {
      config.date_windows[i] = date_window_x->value->int32;
    }
  }

  Tuple *lunar_background = dict_find(received, CK_lunar_background);
  if (lunar_background != NULL) {
    config.lunar_background = (lunar_background->value->int32 != 0);
  }

  Tuple *lunar_direction = dict_find(received, CK_lunar_direction);
  if (lunar_direction != NULL) {
    config.lunar_direction = (lunar_direction->value->int32 != 0);
  }

  Tuple *top_subdial = dict_find(received, CK_top_subdial);
  if (top_subdial != NULL) {
    config.top_subdial = top_subdial->value->int32;
  }

  sanitize_config();

  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "New config: %s", show_config());
  if (memcmp(&orig_config, &config, sizeof(config)) == 0) {
    app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "Config is unchanged.");
  } else {
    save_config();
    apply_config();
  }
}

// The following functions are all designed to support rolling through
// different configs with the buttons, when SCREENSHOT_BUILD is
// defined.

#ifdef SCREENSHOT_BUILD
static unsigned int current_config_index = 0;
#endif  // SCREENSHOT_BUILD

#ifdef SCREENSHOT_BUILD
static void int_to_config() {
  static ConfigOptions screenshot_options[] = {
    {
      DEFAULT_BATTERY_GAUGE, DEFAULT_BLUETOOTH, ENABLE_SECOND_HAND, ENABLE_HOUR_BUZZER,
      true, 0, CDM_tenths, false,
      0, DEFAULT_FACE_INDEX,
      { DEFAULT_DATE_WINDOWS },
      DEFAULT_LUNAR_BACKGROUND, false, 0, DEFAULT_TOP_SUBDIAL,
    },

#if PERSIST_KEY == 0x5151 + 0xc5  // Rosewright A
    { IM_off, IM_off, true, false,
      true, 0, CDM_tenths, false,
      0, 1,
      { DWM_weekday, DWM_date, DWM_off, DWM_off },
      false, false, 1, TSM_moon_phase,
    },

    { IM_off, IM_off, true, false,
      true, 0, CDM_tenths, false,
      11, 0,
      { DWM_weekday, DWM_month, DWM_ampm, DWM_date },
      false, false, 2, TSM_off,
    },
    
    { IM_always, IM_always, false, false,
      true, 0, CDM_tenths, false,
      0, 0,
      { DWM_off, DWM_off, DWM_off, DWM_off },
      true, false, 3, TSM_off,
    },
    
    { IM_always, IM_always, false, false,
      true, 1, CDM_tenths, false,
      0, 0,
      { DWM_off, DWM_off, DWM_off, DWM_off },
      true, false, 3, TSM_off,
    },

#elif PERSIST_KEY == 0x5151 + 0xc6  // Rosewright B
    { IM_off, IM_off, true, false,
      true, 0, CDM_tenths, false,
      0, 0,
      { DWM_weekday, DWM_date },
      true, false, 1, TSM_moon_phase,
    },

    { IM_off, IM_off, true, false,
      true, 0, CDM_tenths, false,
      5, 0,
      { DWM_date, DWM_weekday },
      false, false, 2, TSM_off,
    },
    
    { IM_always, IM_always, false, false,
      true, 0, CDM_tenths, false,
      0, 0,
      { DWM_off, DWM_off },
      true, false, 3, TSM_off,
    },
    
    { IM_always, IM_always, false, false,
      true, 1, CDM_tenths, false,
      0, 0,
      { DWM_off, DWM_off },
      true, false, 3, TSM_off,
    },

#elif PERSIST_KEY == 0x5151 + 0xc7  // Rosewright Chronograph

#elif PERSIST_KEY == 0x5151 + 0xc8  // Rosewright D

#elif PERSIST_KEY == 0x5151 + 0xc9  // Rosewright E

#endif  // PERSIST_KEY
  };

  static const int num_screenshot_options = sizeof(screenshot_options) / sizeof(ConfigOptions);

  if (current_config_index < (int)num_screenshot_options) {
    // Choose one of the pre-canned configs.
    config = screenshot_options[current_config_index];

  } else {
    // Generic iteration through config indexes (especially colors).
    config = default_options;
    unsigned int index = (unsigned int)current_config_index - num_screenshot_options;
    
    // Third digit: color_mode.
    config.color_mode = index % NUM_FACE_COLORS;
    index /= NUM_FACE_COLORS;
    
    // Second digit: draw_mode.
    config.draw_mode = index % 2;
    index /= 2;
    
    // First digit: face_index.
    config.face_index = index % NUM_FACES;
    index /= NUM_FACES;
  }

  sanitize_config();
}
#endif  // SCREENSHOT_BUILD

#ifdef SCREENSHOT_BUILD
static void config_button_up_handler(ClickRecognizerRef recognizer, void *context) {
  ++current_config_index;
  int_to_config();
  apply_config();
}
#endif  // SCREENSHOT_BUILD

#ifdef SCREENSHOT_BUILD
static void config_button_down_handler(ClickRecognizerRef recognizer, void *context) {
  --current_config_index;
  int_to_config();
  apply_config();
}
#endif  // SCREENSHOT_BUILD

#ifdef SCREENSHOT_BUILD
static void config_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, &config_button_up_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, &config_button_down_handler);
}
#endif  // SCREENSHOT_BUILD


#ifdef SCREENSHOT_BUILD
void config_set_click_config(struct Window *window) {
  window_set_click_config_provider(window, &config_click_config_provider);
  int_to_config();
}
#endif  // SCREENSHOT_BUILD
