#ifndef BLUETOOTH_INDICATOR_H
#define BLUETOOTH_INDICATOR_H

void init_bluetooth_indicator();
void deinit_bluetooth_indicator();
void draw_bluetooth_indicator(GContext *ctx, int x, int y, bool invert);

#ifdef PBL_PLATFORM_APLITE
#define poll_quiet_time_state() (false)
#else  // PBL_PLATFORM_APLITE
bool poll_quiet_time_state();
#endif  // PBL_PLATFORM_APLITE

#endif  // BLUETOOTH_INDICATOR_H
