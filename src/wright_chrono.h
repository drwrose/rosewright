#ifndef WRIGHT_CHRONO_H
#define WRIGHT_CHRONO_H

#include "wright.h"

#ifdef MAKE_CHRONOGRAPH

// Number of laps preserved for the laps digital display
#define CHRONO_MAX_LAPS 4

typedef struct __attribute__((__packed__)) {
  unsigned int start_ms;              // consulted if chrono_data.running && !chrono_data.lap_paused
  unsigned int hold_ms;               // consulted if !chrono_data.running || chrono_data.lap_paused
  unsigned char running;              // the chronograph has been started
  unsigned char lap_paused;           // the "lap" button has been pressed
  unsigned int laps[CHRONO_MAX_LAPS];
} ChronoData;

extern ChronoData chrono_data;

extern struct HandCache chrono_minute_cache;
extern struct HandCache chrono_second_cache;
extern struct HandCache chrono_tenth_cache;

extern struct ResourceCache chrono_minute_resource_cache[CHRONO_MINUTE_RESOURCE_CACHE_SIZE];
extern struct ResourceCache chrono_second_resource_cache[CHRONO_SECOND_RESOURCE_CACHE_SIZE];
extern struct ResourceCache chrono_tenth_resource_cache[CHRONO_TENTH_RESOURCE_CACHE_SIZE];
extern size_t chrono_minute_resource_cache_size;
extern size_t chrono_second_resource_cache_size;
extern size_t chrono_tenth_resource_cache_size;

extern Layer *chrono_minute_layer;
extern Layer *chrono_second_layer;
extern Layer *chrono_tenth_layer;

void compute_chrono_hands(unsigned int ms, struct HandPlacement *placement);
void update_chrono_hands(struct HandPlacement *new_placement);
void reset_chrono_digital_timer();
void record_chrono_lap(int chrono_ms);
void update_chrono_laps_time();
void chrono_set_click_config(struct Window *window);

void create_chrono_objects();
void destroy_chrono_objects();
void load_chrono_data();
void save_chrono_data();

#ifdef ENABLE_CHRONO_MINUTE_HAND
void chrono_minute_layer_update_callback(Layer *me, GContext *ctx);
#endif

#ifdef ENABLE_CHRONO_SECOND_HAND
void chrono_second_layer_update_callback(Layer *me, GContext *ctx);
#endif

#ifdef ENABLE_CHRONO_TENTH_HAND
void chrono_tenth_layer_update_callback(Layer *me, GContext *ctx);
#endif

#endif  // MAKE_CHRONOGRAPH

#endif
