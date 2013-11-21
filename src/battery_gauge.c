#include <pebble.h>
#include "hand_table.h"
#include "../resources/generated_config.h"
#include "battery_gauge.h"

GBitmap *battery_gauge_empty_bitmap;
GBitmap *battery_gauge_charging_bitmap;
Layer *battery_gauge_layer;

#ifdef SHOW_BATTERY_GAUGE
void battery_gauge_layer_update_callback(Layer *me, GContext *ctx) {
  GRect box;

  box = layer_get_frame(me);
  box.origin.x = 0;
  box.origin.y = 0;

  BatteryChargeState charge_state = battery_state_service_peek();

#if BATTERY_GAUGE_ON_BLACK
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_context_set_fill_color(ctx, GColorWhite);
#else
  graphics_context_set_compositing_mode(ctx, GCompOpAnd);
  graphics_context_set_fill_color(ctx, GColorBlack);
#endif  // BATTERY_GAUGE_ON_BLACK

  if (charge_state.is_charging) {
    graphics_draw_bitmap_in_rect(ctx, battery_gauge_charging_bitmap, box);
#if !SHOW_BATTERY_GAUGE_ALWAYS
  } else if (!charge_state.is_plugged && charge_state.charge_percent > 10) {
    // Unless SHOW_BATTERY_GAUGE_ALWAYS is configured true (e.g. with
    // -I to config_watch.py), then we don't bother showing the
    // battery gauge when it's in a normal condition.
#endif  // SHOW_BATTERY_GAUGE_ALWAYS
  } else {
    graphics_draw_bitmap_in_rect(ctx, battery_gauge_empty_bitmap, box);
    int bar_width = (charge_state.charge_percent * 9 + 50) / 100 + 2;
    graphics_fill_rect(ctx, GRect(2, 3, bar_width, 4), 0, GCornerNone);
  }
}
#endif  // SHOW_BATTERY_GAUGE

#ifdef SHOW_BATTERY_GAUGE
// Update the battery guage.
void handle_battery(BatteryChargeState charge_state) {
  layer_mark_dirty(battery_gauge_layer);
}
#endif  // SHOW_BATTERY_GAUGE

void init_battery_gauge(Layer *window_layer) {
#ifdef SHOW_BATTERY_GAUGE
  battery_gauge_empty_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_GAUGE_EMPTY);
  battery_gauge_charging_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_GAUGE_CHARGING);
  battery_gauge_layer = layer_create(GRect(BATTERY_GAUGE_X, BATTERY_GAUGE_Y, 18, 10));
  layer_set_update_proc(battery_gauge_layer, &battery_gauge_layer_update_callback);
  layer_add_child(window_layer, battery_gauge_layer);
  battery_state_service_subscribe(&handle_battery);
#endif  // SHOW_BATTERY_GAUGE
}

void deinit_battery_gauge() {
#ifdef SHOW_BATTERY_GAUGE
  battery_state_service_unsubscribe();
  layer_destroy(battery_gauge_layer);
  gbitmap_destroy(battery_gauge_empty_bitmap);
  gbitmap_destroy(battery_gauge_charging_bitmap);
#endif
}
