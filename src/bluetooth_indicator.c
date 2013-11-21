#include <pebble.h>
#include "hand_table.h"
#include "../resources/generated_config.h"
#include "bluetooth_indicator.h"

// Define this to ring the buzzer when the bluetooth connection is
// lost.  Only works when the clock face defines a bluetooth icon
// (i.e. SHOW_BLUETOOTH is defined).
#define BLUETOOTH_BUZZER 1

GBitmap *bluetooth_disconnected_bitmap;
GBitmap *bluetooth_connected_bitmap;
Layer *bluetooth_layer;
bool bluetooth_state = false;

#ifdef SHOW_BLUETOOTH
void bluetooth_layer_update_callback(Layer *me, GContext *ctx) {
  GRect box;

  box = layer_get_frame(me);
  box.origin.x = 0;
  box.origin.y = 0;

#if BLUETOOTH_ON_BLACK
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
#else
  graphics_context_set_compositing_mode(ctx, GCompOpAnd);
#endif  // BLUETOOTH_ON_BLACK

  bool new_state = bluetooth_connection_service_peek();
  if (new_state != bluetooth_state) {
    bluetooth_state = new_state;
#ifdef BLUETOOTH_BUZZER
    if (!bluetooth_state) {
      // We just lost the bluetooth connection.  Ring the buzzer.
      vibes_short_pulse();
    }
#endif
  }
  if (bluetooth_state) {
#if SHOW_BLUETOOTH_ALWAYS
    // We only draw the "connected" bitmap if SHOW_BLUETOOTH_ALWAYS is
    // configured true (e.g. with -I to config_watch.py).
    graphics_draw_bitmap_in_rect(ctx, bluetooth_connected_bitmap, box);
#endif  // SHOW_BLUETOOTH_ALWAYS
  } else {
    graphics_draw_bitmap_in_rect(ctx, bluetooth_disconnected_bitmap, box);
  }
}
#endif  // SHOW_BLUETOOTH

#ifdef SHOW_BLUETOOTH
// Update the bluetooth guage.
void handle_bluetooth(bool connected) {
  layer_mark_dirty(bluetooth_layer);
}
#endif  // SHOW_BLUETOOTH

void init_bluetooth_indicator(Layer *window_layer) {
#ifdef SHOW_BLUETOOTH
  bluetooth_disconnected_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH_DISCONNECTED);
  bluetooth_connected_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH_CONNECTED);
  bluetooth_layer = layer_create(GRect(BLUETOOTH_X, BLUETOOTH_Y, 18, 18));
  layer_set_update_proc(bluetooth_layer, &bluetooth_layer_update_callback);
  layer_add_child(window_layer, bluetooth_layer);
  bluetooth_connection_service_subscribe(&handle_bluetooth);
#endif  // SHOW_BLUETOOTH
}

void deinit_bluetooth_indicator() {
#ifdef SHOW_BLUETOOTH
  bluetooth_connection_service_unsubscribe();
  layer_destroy(bluetooth_layer);
  gbitmap_destroy(bluetooth_disconnected_bitmap);
  gbitmap_destroy(bluetooth_connected_bitmap);
#endif
}
