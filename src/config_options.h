#ifndef CONFIG_OPTIONS_H
#define CONFIG_OPTIONS_H

#include <pebble.h>

// These keys are used to communicate with Javascript and must match
// the corresponding index numbers in appinfo.json.in.
typedef enum {
  CK_keep_battery_gauge = 0,
  CK_keep_bluetooth_indicator = 1,
  CK_second_hand = 2,
  CK_hour_buzzer = 3,
  CK_draw_mode = 4,
  CK_chrono_dial = 5,
  CK_sweep_seconds = 6,
  CK_show_day = 7,
  CK_show_date = 8,
  CK_display_lang = 9,
} ConfigKey;

typedef enum {
  DL_english,
  DL_french,
  DL_german,
  DL_italian,
  DL_dutch,
  DL_spanish,
  DL_portuguese,
  DL_num_languages,
} DisplayLanguages;

typedef enum {
  CDM_off = 0,
  CDM_tenths = 1,
  CDM_hours = 2,
  CDM_dual = 3,
} ChronoDialMode;

typedef struct {
  bool keep_battery_gauge;
  bool keep_bluetooth_indicator;
  bool second_hand;
  bool hour_buzzer;
  unsigned char draw_mode;
  unsigned char chrono_dial;
  bool sweep_seconds;
  bool show_day;
  bool show_date;
  DisplayLanguages display_lang;
} __attribute__((__packed__)) ConfigOptions;

extern ConfigOptions config;

void init_default_options();
void save_config();
void load_config();
void receive_config_handler(DictionaryIterator *received, void *context);

void apply_config();  // implemented in the main program

#endif  // CONFIG_OPTIONS_H
