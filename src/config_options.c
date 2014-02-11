#include "config_options.h"
#include "hand_table.h"
#include "../resources/generated_config.h"

ConfigOptions config;

void init_default_options() {
  // Initializes the config options with their default values.  Note
  // that these defaults are used only if the Pebble is not connected
  // to the phone at the time of launch; otherwise, the defaults in
  // pebble-js-app.js are used instead.
  config.keep_battery_gauge = SHOW_BATTERY_GAUGE_ALWAYS;
  config.keep_bluetooth_indicator = SHOW_BLUETOOTH_ALWAYS;
  config.second_hand = SHOW_SECOND_HAND;
  config.hour_buzzer = ENABLE_HOUR_BUZZER;
  config.draw_mode = 0;
  config.chrono_dial = CDM_dual;
}

const char *show_config() {
  static char buffer[64];
  snprintf(buffer, 64, "bat: %d, bt: %d, sh: %d, hb: %d, dm: %d, cd: %d", config.keep_battery_gauge, config.keep_bluetooth_indicator, config.second_hand, config.hour_buzzer, config.draw_mode, config.chrono_dial);
  return buffer;
}

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
  if (persist_read_data(PERSIST_KEY, &local_config, sizeof(local_config)) == sizeof(local_config)) {
    config = local_config;
    app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "Loaded config: %s", show_config());
  } else {
    app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "Wrong previous config size or no previous config.");
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

  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "New config: %s", show_config());
  if (memcmp(&orig_config, &config, sizeof(config)) == 0) {
    app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "Config is unchanged.");
  } else {
    save_config();
    apply_config();
  }
}
