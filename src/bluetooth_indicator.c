#include "wright.h"
#include <pebble.h>
#include "bluetooth_indicator.h"
#include "config_options.h"
#include "bwd.h"

BitmapWithData bluetooth_disconnected;
BitmapWithData bluetooth_connected;
BitmapWithData bluetooth_mask;
bool bluetooth_state = false;

void draw_bluetooth_indicator(GContext *ctx, int x, int y, bool invert) {
  if (config.bluetooth_indicator == IM_off) {
    return;
  }

  GRect box;
  box.origin.x = x;
  box.origin.y = y;
  box.size.w = 18;
  box.size.h = 18;

  GCompOp fg_mode;

#ifdef PBL_PLATFORM_APLITE
  GCompOp mask_mode;
  if (invert ^ config.draw_mode ^ APLITE_INVERT) {
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
      if (bluetooth_mask.bitmap == NULL) {
        bluetooth_mask = png_bwd_create(RESOURCE_ID_BLUETOOTH_MASK);
      }
      graphics_context_set_compositing_mode(ctx, mask_mode);
      graphics_draw_bitmap_in_rect(ctx, bluetooth_mask.bitmap, box);
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
    if (bluetooth_mask.bitmap == NULL) {
      bluetooth_mask = png_bwd_create(RESOURCE_ID_BLUETOOTH_MASK);
    }
    graphics_context_set_compositing_mode(ctx, mask_mode);
    graphics_draw_bitmap_in_rect(ctx, bluetooth_mask.bitmap, box);
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
  if (config.bluetooth_indicator != IM_off) {
    invalidate_clock_face();
  }
}

void init_bluetooth_indicator() {
  bluetooth_connection_service_subscribe(&handle_bluetooth);
}

void deinit_bluetooth_indicator() {
  bluetooth_connection_service_unsubscribe();
  bwd_destroy(&bluetooth_disconnected);
  bwd_destroy(&bluetooth_connected);
  bwd_destroy(&bluetooth_mask);
}
