#include "wright.h"
#include <pebble.h>
#include "battery_gauge.h"
#include "config_options.h"
#include "bwd.h"

BitmapWithData battery_gauge_empty;
BitmapWithData battery_gauge_charged;
BitmapWithData battery_gauge_mask;
BitmapWithData charging;
BitmapWithData charging_mask;

void draw_battery_gauge(GContext *ctx, int x, int y, bool invert) {
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

  GRect box;
  box.origin.x = x - 6;
  box.origin.y = y;
  box.size.w = 24;
  box.size.h = 10;

  GCompOp fg_mode;
  GColor fg_color, bg_color;
  GCompOp mask_mode;

  if (invert ^ config.draw_mode ^ APLITE_INVERT) {
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
    graphics_fill_rect(ctx, GRect(x, y, 18, 10), 0, GCornerNone);
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
    graphics_fill_rect(ctx, GRect(x + 4, y + 3, bar_width, 4), 0, GCornerNone);

  } else {
    // Draw the digital text percentage.
    char text_buffer[4];
    snprintf(text_buffer, 4, "%d", charge_state.charge_percent);
    GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
    graphics_context_set_text_color(ctx, fg_color);
    graphics_draw_text(ctx, text_buffer, font, GRect(x, y - 4, 18, 10),
		       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
		       NULL);
  }
}

// Update the battery guage.
void handle_battery(BatteryChargeState charge_state) {
  if (config.battery_gauge != IM_off) {
    invalidate_clock_face();
  }
}

void init_battery_gauge() {
  battery_state_service_subscribe(&handle_battery);
}

void deinit_battery_gauge() {
  battery_state_service_unsubscribe();
  bwd_destroy(&battery_gauge_empty);
  bwd_destroy(&battery_gauge_charged);
  bwd_destroy(&battery_gauge_mask);
  bwd_destroy(&charging);
  bwd_destroy(&charging_mask);
}
