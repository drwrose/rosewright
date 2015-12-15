#ifndef BATTERY_GAUGE_H
#define BATTERY_GAUGE_H

// Define this to update the battery gauge every two seconds for development.
//#define BATTERY_HACK 1

void init_battery_gauge();
void deinit_battery_gauge();
void draw_battery_gauge(GContext *ctx, int x, int y, bool invert);

#endif  // BATTERY_GAUGE_H
