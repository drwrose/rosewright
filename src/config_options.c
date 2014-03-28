#include "config_options.h"
#include "wright.h"

ConfigOptions config;

void init_default_options() {
  // Initializes the config options with their default values.  Note
  // that these defaults are used only if the Pebble is not connected
  // to the phone at the time of launch; otherwise, the defaults in
  // pebble-js-app.js are used instead.
  static ConfigOptions default_options = {
    DEFAULT_BATTERY_GAUGE,
    DEFAULT_BLUETOOTH,
    ENABLE_SECOND_HAND,
    ENABLE_HOUR_BUZZER,
    false,
    CDM_tenths,
    false,
    0,
    0,
    { DEFAULT_DATE_WINDOWS },
  };
  
  config = default_options;
}

void sanitize_config() {
  // Ensures that the newly-loaded config parameters are within a
  // reasonable range for the program and won't cause crashes.
  config.draw_mode = config.draw_mode & 1;
  config.chrono_dial = config.chrono_dial % (CDM_dual + 1);
  config.display_lang = config.display_lang % num_langs;
  config.face_index = config.face_index % NUM_FACES;
  for (int i = 0; i < NUM_DATE_WINDOWS; ++i) {
    config.date_windows[i] = config.date_windows[i] % (DWM_ampm + 1);
  }
}

#ifndef NDEBUG
const char *show_config() {
#define CONFIG_BUFFER_SIZE 100
  static char buffer[CONFIG_BUFFER_SIZE];
  snprintf(buffer, CONFIG_BUFFER_SIZE, "bat: %d, bt: %d, sh: %d, hb: %d, dm: %d, cd: %d, sw: %d, dl: %d, fi: %d, dw: ", config.keep_battery_gauge, config.keep_bluetooth_indicator, config.second_hand, config.hour_buzzer, config.draw_mode, config.chrono_dial, config.sweep_seconds, config.display_lang, config.face_index);

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
  init_default_options();

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

  Tuple *keep_battery_gauge = dict_find(received, CK_keep_battery_gauge);
  if (keep_battery_gauge != NULL) {
    config.keep_battery_gauge = keep_battery_gauge->value->int32;
  }

  Tuple *keep_bluetooth_indicator = dict_find(received, CK_keep_bluetooth_indicator);
  if (keep_bluetooth_indicator != NULL) {
    config.keep_bluetooth_indicator = keep_bluetooth_indicator->value->int32;
  }

  Tuple *second_hand = dict_find(received, CK_second_hand);
  if (second_hand != NULL) {
    config.second_hand = second_hand->value->int32;
  }

  Tuple *hour_buzzer = dict_find(received, CK_hour_buzzer);
  if (hour_buzzer != NULL) {
    config.hour_buzzer = hour_buzzer->value->int32;
  }

  Tuple *draw_mode = dict_find(received, CK_draw_mode);
  if (draw_mode != NULL) {
    config.draw_mode = draw_mode->value->int32;
  }

  Tuple *chrono_dial = dict_find(received, CK_chrono_dial);
  if (chrono_dial != NULL) {
    config.chrono_dial = chrono_dial->value->int32;
  }

  Tuple *sweep_seconds = dict_find(received, CK_sweep_seconds);
  if (sweep_seconds != NULL) {
    config.sweep_seconds = sweep_seconds->value->int32;
  }

  Tuple *display_lang = dict_find(received, CK_display_lang);
  if (display_lang != NULL) {
    // Look for the matching language name in our table of known languages.
    for (int li = 0; li < num_langs; ++li) {
      if (strcmp(display_lang->value->cstring, lang_table[li].locale_name) == 0) {
	config.display_lang = li;
	break;
      }
    }
  }

  Tuple *face_index = dict_find(received, CK_face_index);
  if (face_index != NULL) {
    config.face_index = face_index->value->int32;
  }

  for (int i = 0; i < NUM_DATE_WINDOWS; ++i) {
    Tuple *date_window_x = dict_find(received, CK_date_window_a + i);
    if (date_window_x != NULL) {
      config.date_windows[i] = date_window_x->value->int32;
    }
  }

  sanitize_config();

  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, show_config());
  if (memcmp(&orig_config, &config, sizeof(config)) == 0) {
    app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "Config is unchanged.");
  } else {
    save_config();
    apply_config();
  }
}
