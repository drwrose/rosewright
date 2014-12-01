#include <pebble.h>
#include "battery_gauge.h"
#include "config_options.h"
#include "bwd.h"

BitmapWithData battery_gauge_empty;
BitmapWithData battery_gauge_charged;
BitmapWithData battery_gauge_mask;
BitmapWithData charging;
BitmapWithData charging_mask;
Layer *battery_gauge_layer;

bool battery_gauge_invert = false;
bool battery_gauge_opaque_layer = false;

void battery_gauge_layer_update_callback(Layer *me, GContext *ctx) {
  if (config.battery_gauge == IM_off) {
    return;
  }

  BatteryChargeState charge_state = battery_state_service_peek();

#ifdef BATTERY_HACK
  time_t now = time(NULL);  
  charge_state.charge_percent = 100 - ((now / 2) % 11) * 10;
#endif  // BATTERY_HACK

  bool show_gauge = (config.battery_gauge != IM_when_needed) || charge_state.is_charging || (charge_state.is_plugged || charge_state.charge_percent <= 20);
  if (!show_gauge) {
    return;
  }

  GRect box = layer_get_frame(me);
  box.origin.x = 0;
  box.origin.y = 0;

  GCompOp fg_mode;
  GColor fg_color, bg_color;
  GCompOp mask_mode;

  if (battery_gauge_invert ^ config.draw_mode) {
    fg_mode = GCompOpSet;
    bg_color = GColorBlack;
    fg_color = GColorWhite;
    mask_mode = GCompOpAnd;
  } else {
    fg_mode = GCompOpAnd;
    bg_color = GColorWhite;
    fg_color = GColorBlack;
    mask_mode = GCompOpSet;
  }

  if (battery_gauge_opaque_layer) {
    // Draw the background of the layer.
    if (charge_state.is_charging) {
      // Erase the charging icon shape.
      if (charging_mask.bitmap == NULL) {
	charging_mask = png_bwd_create(RESOURCE_ID_CHARGING_MASK);
      }
      graphics_context_set_compositing_mode(ctx, mask_mode);
      graphics_draw_bitmap_in_rect(ctx, charging_mask.bitmap, box);
    }

    if (config.battery_gauge != IM_digital) {
      // Erase the battery gauge shape.
      if (battery_gauge_mask.bitmap == NULL) {
	battery_gauge_mask = png_bwd_create(RESOURCE_ID_BATTERY_GAUGE_MASK);
      }
      graphics_context_set_compositing_mode(ctx, mask_mode);
      graphics_draw_bitmap_in_rect(ctx, battery_gauge_mask.bitmap, box);
    } else {
      // Erase a rectangle for text.
      graphics_context_set_fill_color(ctx, bg_color);
      graphics_fill_rect(ctx, GRect(6, 0, 18, 10), 0, GCornerNone);
    }
  }

  if (charge_state.is_charging) {
    // Actively charging.  Draw the charging icon.
    if (charging.bitmap == NULL) {
      charging = png_bwd_create(RESOURCE_ID_CHARGING);
    }
    graphics_context_set_compositing_mode(ctx, fg_mode);
    graphics_draw_bitmap_in_rect(ctx, charging.bitmap, box);
  }
  
  if (!charge_state.is_charging && charge_state.is_plugged && charge_state.charge_percent >= 80) {
    // Plugged in but not charging.  Draw the charged icon.
    if (battery_gauge_charged.bitmap == NULL) {
      battery_gauge_charged = png_bwd_create(RESOURCE_ID_BATTERY_GAUGE_CHARGED);
    }
    graphics_context_set_compositing_mode(ctx, fg_mode);
    graphics_draw_bitmap_in_rect(ctx, battery_gauge_charged.bitmap, box);

  } else if (config.battery_gauge != IM_digital) {
    // Not plugged in.  Draw the analog battery icon.
    if (battery_gauge_empty.bitmap == NULL) {
      battery_gauge_empty = png_bwd_create(RESOURCE_ID_BATTERY_GAUGE_EMPTY);
    }
    graphics_context_set_compositing_mode(ctx, fg_mode);
    graphics_context_set_fill_color(ctx, fg_color);
    graphics_draw_bitmap_in_rect(ctx, battery_gauge_empty.bitmap, box);
    int bar_width = charge_state.charge_percent / 10;
    graphics_fill_rect(ctx, GRect(10, 3, bar_width, 4), 0, GCornerNone);

  } else {
    // Draw the digital text percentage.
    char text_buffer[4];
    snprintf(text_buffer, 4, "%d", charge_state.charge_percent);
    GFont *font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
    graphics_context_set_text_color(ctx, fg_color);
    graphics_draw_text(ctx, text_buffer, font, GRect(6, -4, 18, 10),
		       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
		       NULL);
 }
}

// Update the battery guage.
void handle_battery(BatteryChargeState charge_state) {
  layer_mark_dirty(battery_gauge_layer);
}

void init_battery_gauge(Layer *window_layer, int x, int y, bool invert, bool opaque_layer) {
  battery_gauge_invert = invert;
  battery_gauge_opaque_layer = opaque_layer;
  battery_gauge_layer = layer_create(GRect(x - 6, y, 24, 10));
  layer_set_update_proc(battery_gauge_layer, &battery_gauge_layer_update_callback);
  layer_add_child(window_layer, battery_gauge_layer);
  battery_state_service_subscribe(&handle_battery);
}

void move_battery_gauge(int x, int y, bool invert, bool opaque_layer) {
  battery_gauge_invert = invert;
  battery_gauge_opaque_layer = opaque_layer;
  layer_set_frame((Layer *)battery_gauge_layer, GRect(x - 6, y, 24, 10));
}

void deinit_battery_gauge() {
  battery_state_service_unsubscribe();
  layer_destroy(battery_gauge_layer);
  battery_gauge_layer = NULL;
  bwd_destroy(&battery_gauge_empty);
  bwd_destroy(&battery_gauge_charged);
  bwd_destroy(&battery_gauge_mask);
  bwd_destroy(&charging);
  bwd_destroy(&charging_mask);
}

void refresh_battery_gauge() {
  layer_mark_dirty(battery_gauge_layer);
}
