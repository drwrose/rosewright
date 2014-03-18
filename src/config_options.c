#include "config_options.h"
#include "hand_table.h"
#include "lang_table.h"
#include "../resources/generated_config.h"

ConfigOptions config;

void init_default_options() {
  // Initializes the config options with their default values.  Note
  // that these defaults are used only if the Pebble is not connected
  // to the phone at the time of launch; otherwise, the defaults in
  // pebble-js-app.js are used instead.
  config.keep_battery_gauge = DEFAULT_BATTERY_GAUGE;
  config.keep_bluetooth_indicator = DEFAULT_BLUETOOTH;
  config.second_hand = SHOW_SECOND_HAND;
  config.hour_buzzer = ENABLE_HOUR_BUZZER;
  config.draw_mode = 0;
  config.chrono_dial = CDM_tenths;
  config.sweep_seconds = 0;
  config.show_day = DEFAULT_DAY_CARD;
  config.show_date = DEFAULT_DATE_CARD;
  config.display_lang = 0;
}

#ifdef ENABLE_LOG
const char *show_config() {
  static char buffer[80];
  snprintf(buffer, 80, "bat: %d, bt: %d, sh: %d, hb: %d, dm: %d, cd: %d, sw: %d, day: %d, date: %d, dl: %d", config.keep_battery_gauge, config.keep_bluetooth_indicator, config.second_hand, config.hour_buzzer, config.draw_mode, config.chrono_dial, config.sweep_seconds, config.show_day, config.show_date, config.display_lang);
  return buffer;
}
#endif  // ENABLE_LOG

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

  Tuple *show_day = dict_find(received, CK_show_day);
  if (show_day != NULL) {
    config.show_day = show_day->value->int32;
  }

  Tuple *show_date = dict_find(received, CK_show_date);
  if (show_date != NULL) {
    config.show_date = show_date->value->int32;
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

  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "New config: %s", show_config());
  if (memcmp(&orig_config, &config, sizeof(config)) == 0) {
    app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "Config is unchanged.");
  } else {
    save_config();
    apply_config();
  }
}
