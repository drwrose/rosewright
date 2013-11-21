#include <pebble.h>
#include "bluetooth_indicator.h"
#include "config_options.h"

// Define this to ring the buzzer when the bluetooth connection is
// lost.
#define BLUETOOTH_BUZZER 1

GBitmap *bluetooth_disconnected_bitmap;
GBitmap *bluetooth_connected_bitmap;
Layer *bluetooth_layer;
bool bluetooth_state = false;

bool bluetooth_on_black = false;

void bluetooth_layer_update_callback(Layer *me, GContext *ctx) {
  GRect box;

  box = layer_get_frame(me);
  box.origin.x = 0;
  box.origin.y = 0;

  if (bluetooth_on_black) {
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
  } else {
    graphics_context_set_compositing_mode(ctx, GCompOpAnd);
  }

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
    if (config.keep_bluetooth_indicator) {
      // We only draw the "connected" bitmap if
      // keep_bluetooth_indicator is configured true.
      graphics_draw_bitmap_in_rect(ctx, bluetooth_connected_bitmap, box);
    }
  } else {
    graphics_draw_bitmap_in_rect(ctx, bluetooth_disconnected_bitmap, box);
  }
}

// Update the bluetooth guage.
void handle_bluetooth(bool connected) {
  layer_mark_dirty(bluetooth_layer);
}

void init_bluetooth_indicator(Layer *window_layer, int x, int y, bool on_black) {
  bluetooth_on_black = on_black;
  bluetooth_disconnected_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH_DISCONNECTED);
  bluetooth_connected_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH_CONNECTED);
  bluetooth_layer = layer_create(GRect(x, y, 18, 18));
  layer_set_update_proc(bluetooth_layer, &bluetooth_layer_update_callback);
  layer_add_child(window_layer, bluetooth_layer);
  bluetooth_connection_service_subscribe(&handle_bluetooth);
}

void deinit_bluetooth_indicator() {
  bluetooth_connection_service_unsubscribe();
  layer_destroy(bluetooth_layer);
  gbitmap_destroy(bluetooth_disconnected_bitmap);
  gbitmap_destroy(bluetooth_connected_bitmap);
}
