#include <pebble.h>
#include "bluetooth_indicator.h"
#include "config_options.h"
#include "bwd.h"

BitmapWithData bluetooth_disconnected;
BitmapWithData bluetooth_connected;
BitmapWithData bluetooth_mask;
Layer *bluetooth_layer;
bool bluetooth_state = false;

#ifdef PBL_PLATFORM_APLITE

// On Aplite, these parameters are passed in.
bool bluetooth_invert = false;
bool bluetooth_opaque_layer = false;

#else  // PBL_PLATFORM_APLITE

// On Basalt, the icon is never inverted, and the layer is never
// opaque in the Aplite sense.
#define bluetooth_invert false
#define bluetooth_opaque_layer false

#endif  // PBL_PLATFORM_APLITE


void bluetooth_layer_update_callback(Layer *me, GContext *ctx) {
  if (config.bluetooth_indicator == IM_off) {
    return;
  }

  GRect box = layer_get_frame(me);
  box.origin.x = 0;
  box.origin.y = 0;

  GCompOp fg_mode;

#ifdef PBL_PLATFORM_APLITE
  GCompOp mask_mode;
  if (bluetooth_invert ^ config.draw_mode) {
    fg_mode = GCompOpSet;
    mask_mode = GCompOpAnd;
  } else {
    fg_mode = GCompOpAnd;
    mask_mode = GCompOpSet;
  }
#else  // PBL_PLATFORM_APLITE
  // In Basalt, we always use GCompOpSet because the icon includes its
  // own alpha channel.
  fg_mode = GCompOpSet;
#endif  // PBL_PLATFORM_APLITE

  bool new_state = bluetooth_connection_service_peek();
  if (new_state != bluetooth_state) {
    bluetooth_state = new_state;
    if (config.bluetooth_buzzer && !bluetooth_state) {
      // We just lost the bluetooth connection.  Ring the buzzer.
      vibes_short_pulse();
    }
  }

  if (bluetooth_state) {
    if (config.bluetooth_indicator != IM_when_needed) {
      // We don't draw the "connected" bitmap if bluetooth_indicator
      // is set to IM_when_needed; only on IM_always.
#ifdef PBL_PLATFORM_APLITE      
      if (bluetooth_opaque_layer) {
	if (bluetooth_mask.bitmap == NULL) {
	  bluetooth_mask = png_bwd_create(RESOURCE_ID_BLUETOOTH_MASK);
	}
	graphics_context_set_compositing_mode(ctx, mask_mode);
	graphics_draw_bitmap_in_rect(ctx, bluetooth_mask.bitmap, box);
      }
#endif  // PBL_PLATFORM_APLITE      
      if (bluetooth_connected.bitmap == NULL) {
	bluetooth_connected = png_bwd_create(RESOURCE_ID_BLUETOOTH_CONNECTED);
      }
      graphics_context_set_compositing_mode(ctx, fg_mode);
      graphics_draw_bitmap_in_rect(ctx, bluetooth_connected.bitmap, box);
    }
  } else {
    // We always draw the disconnected bitmap (except in the IM_off
    // case, of course).
#ifdef PBL_PLATFORM_APLITE      
    if (bluetooth_opaque_layer) {
      if (bluetooth_mask.bitmap == NULL) {
	bluetooth_mask = png_bwd_create(RESOURCE_ID_BLUETOOTH_MASK);
      }
      graphics_context_set_compositing_mode(ctx, mask_mode);
      graphics_draw_bitmap_in_rect(ctx, bluetooth_mask.bitmap, box);
    }
#endif  // PBL_PLATFORM_APLITE      
    if (bluetooth_disconnected.bitmap == NULL) {
      bluetooth_disconnected = png_bwd_create(RESOURCE_ID_BLUETOOTH_DISCONNECTED);
    }
    graphics_context_set_compositing_mode(ctx, fg_mode);
    graphics_draw_bitmap_in_rect(ctx, bluetooth_disconnected.bitmap, box);
  }
}

// Update the bluetooth guage.
void handle_bluetooth(bool connected) {
  layer_mark_dirty(bluetooth_layer);
}

void init_bluetooth_indicator(Layer *window_layer, int x, int y, bool invert, bool opaque_layer) {
#ifdef PBL_PLATFORM_APLITE
  bluetooth_invert = invert;
  bluetooth_opaque_layer = opaque_layer;
#endif  // PBL_PLATFORM_APLITE
  bluetooth_layer = layer_create(GRect(x, y, 18, 18));
  layer_set_update_proc(bluetooth_layer, &bluetooth_layer_update_callback);
  layer_add_child(window_layer, bluetooth_layer);
  bluetooth_connection_service_subscribe(&handle_bluetooth);
}

void move_bluetooth_indicator(int x, int y, bool invert, bool opaque_layer) {
#ifdef PBL_PLATFORM_APLITE
  bluetooth_invert = invert;
  bluetooth_opaque_layer = opaque_layer;
#endif  // PBL_PLATFORM_APLITE
  layer_set_frame((Layer *)bluetooth_layer, GRect(x, y, 18, 18));
}

void deinit_bluetooth_indicator() {
  bluetooth_connection_service_unsubscribe();
  layer_destroy(bluetooth_layer);
  bluetooth_layer = NULL;
  bwd_destroy(&bluetooth_disconnected);
  bwd_destroy(&bluetooth_connected);
  bwd_destroy(&bluetooth_mask);
}

void refresh_bluetooth_indicator() {
  layer_mark_dirty(bluetooth_layer);
}
