#include "wright.h"
#include <pebble.h>
#include "battery_gauge.h"
#include "config_options.h"
#include "bwd.h"
#include "qapp_log.h"

BitmapWithData battery_gauge_empty;
BitmapWithData battery_gauge_charged;
BitmapWithData battery_gauge_mask;
BitmapWithData charging;
BitmapWithData charging_mask;

bool got_charge_state = false;
BatteryChargeState charge_state;

void destroy_battery_gauge_bitmaps() {
  bwd_destroy(&battery_gauge_empty);
  bwd_destroy(&battery_gauge_charged);
  bwd_destroy(&battery_gauge_mask);
  bwd_destroy(&charging);
  bwd_destroy(&charging_mask);
}

void draw_battery_gauge(GContext *ctx, int x, int y, bool invert) {
  if (config.battery_gauge == IM_off) {
    return;
  }

  if (!got_charge_state) {
    charge_state = battery_state_service_peek();
    got_charge_state = true;
  }

#ifdef BATTERY_HACK
  time_t now = time(NULL);
  charge_state.charge_percent = 100 - ((now / 2) % 11) * 10;
#endif  // BATTERY_HACK

  bool show_gauge = (config.battery_gauge != IM_when_needed) || charge_state.is_charging || (charge_state.is_plugged || charge_state.charge_percent <= 20);
  if (!show_gauge) {
    return;
  }

  GRect box;
  box.origin.x = x;
  box.origin.y = y;
  box.size.w = BATTERY_GAUGE_FILL_X + BATTERY_GAUGE_FILL_W;
  box.size.h = BATTERY_GAUGE_FILL_Y + BATTERY_GAUGE_FILL_H;

  GCompOp fg_mode;
  GColor fg_color, bg_color;

#ifdef PBL_BW
  GCompOp mask_mode;
  if (invert ^ config.draw_mode ^ BW_INVERT) {
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
#else  // PBL_BW
  // In Basalt, we always use GCompOpSet because the icon includes its
  // own alpha channel.
  extern struct FaceColorDef clock_face_color_table[];
  struct FaceColorDef *cd = &clock_face_color_table[config.color_mode];
  fg_mode = GCompOpSet;
  bg_color.argb = cd->db_argb8;
  fg_color.argb = cd->d1_argb8;
  if (config.draw_mode) {
    bg_color.argb ^= 0x3f;
    fg_color.argb ^= 0x3f;
  }
#endif  // PBL_BW

  bool fully_charged = (!charge_state.is_charging && charge_state.is_plugged && charge_state.charge_percent >= 80);

#ifdef PBL_BW
  // Draw the background of the layer.
  if (charge_state.is_charging) {
    // Erase the charging icon shape.
    if (charging_mask.bitmap == NULL) {
      charging_mask = rle_bwd_create(RESOURCE_ID_CHARGING_MASK);
    }
    graphics_context_set_compositing_mode(ctx, mask_mode);
    graphics_draw_bitmap_in_rect(ctx, charging_mask.bitmap, box);
  }
#endif  // PBL_BW

  if (config.battery_gauge != IM_digital || fully_charged) {
#ifdef PBL_BW
    // Erase the battery gauge shape.
    if (battery_gauge_mask.bitmap == NULL) {
      battery_gauge_mask = rle_bwd_create(RESOURCE_ID_BATTERY_GAUGE_MASK);
    }
    graphics_context_set_compositing_mode(ctx, mask_mode);
    graphics_draw_bitmap_in_rect(ctx, battery_gauge_mask.bitmap, box);
#endif  // PBL_BW
  } else {
    // Erase a rectangle for text.
    graphics_context_set_fill_color(ctx, bg_color);
    graphics_fill_rect(ctx, GRect(x + BATTERY_GAUGE_FILL_X, y + BATTERY_GAUGE_FILL_Y, BATTERY_GAUGE_FILL_W, BATTERY_GAUGE_FILL_H), 0, GCornerNone);
  }

  if (charge_state.is_charging) {
    // Actively charging.  Draw the charging icon.
    if (charging.bitmap == NULL) {
      charging = rle_bwd_create(RESOURCE_ID_CHARGING);
      remap_colors_date(&charging);
    }
    graphics_context_set_compositing_mode(ctx, fg_mode);
    graphics_draw_bitmap_in_rect(ctx, charging.bitmap, box);
  }

  if (fully_charged) {
    // Plugged in but not charging.  Draw the charged icon.
    if (battery_gauge_charged.bitmap == NULL) {
      battery_gauge_charged = rle_bwd_create(RESOURCE_ID_BATTERY_GAUGE_CHARGED);
      remap_colors_date(&battery_gauge_charged);
    }
    graphics_context_set_compositing_mode(ctx, fg_mode);
    graphics_draw_bitmap_in_rect(ctx, battery_gauge_charged.bitmap, box);

  } else if (config.battery_gauge != IM_digital) {
    // Not plugged in.  Draw the analog battery icon.
    if (battery_gauge_empty.bitmap == NULL) {
      battery_gauge_empty = rle_bwd_create(RESOURCE_ID_BATTERY_GAUGE_EMPTY);
      remap_colors_date(&battery_gauge_empty);
    }
    graphics_context_set_compositing_mode(ctx, fg_mode);
    graphics_context_set_fill_color(ctx, fg_color);
    graphics_draw_bitmap_in_rect(ctx, battery_gauge_empty.bitmap, box);
    int bar_width = charge_state.charge_percent * BATTERY_GAUGE_BAR_W / 100;
    graphics_fill_rect(ctx, GRect(x + BATTERY_GAUGE_BAR_X, y + BATTERY_GAUGE_BAR_Y, bar_width, BATTERY_GAUGE_BAR_H), 0, GCornerNone);

  } else {
    // Draw the digital text percentage.
    char text_buffer[4];
    snprintf(text_buffer, 4, "%d", charge_state.charge_percent);
    GFont font = fonts_get_system_font(BATTERY_GAUGE_SYSTEM_FONT);
    graphics_context_set_text_color(ctx, fg_color);
    graphics_draw_text(ctx, text_buffer, font, GRect(x + BATTERY_GAUGE_FILL_X, y + BATTERY_GAUGE_FILL_Y + BATTERY_GAUGE_FONT_VSHIFT, BATTERY_GAUGE_FILL_W, BATTERY_GAUGE_FILL_H),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                       NULL);
  }

  if (!keep_assets) {
    destroy_battery_gauge_bitmaps();
  }
}

// Update the battery guage.
void handle_battery(BatteryChargeState new_charge_state) {
  if (got_charge_state && memcmp(&charge_state, &new_charge_state, sizeof(charge_state)) == 0) {
    // No change; ignore the update.
    qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "battery update received, no change to battery");
    return;
  }

  charge_state = new_charge_state;
  got_charge_state = true;
  qapp_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "battery changed to %d%%, %d %d", charge_state.charge_percent, charge_state.is_charging, charge_state.is_plugged);

  if (config.battery_gauge != IM_off) {
    invalidate_clock_face();
  }
}

void init_battery_gauge() {
  battery_state_service_subscribe(&handle_battery);
}

void deinit_battery_gauge() {
  battery_state_service_unsubscribe();
  destroy_battery_gauge_bitmaps();
}
