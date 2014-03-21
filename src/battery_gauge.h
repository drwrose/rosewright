#ifndef BATTERY_GAUGE_H
#define BATTERY_GAUGE_H

// Define this to update the battery gauge every two seconds for development.
//#define BATTERY_HACK 1

void init_battery_gauge(Layer *window_layer, int x, int y, bool on_black, bool opaque_layer);
void move_battery_gauge(int x, int y, bool on_black, bool opaque_layer);
void deinit_battery_gauge();
void refresh_battery_gauge();

#endif  // BATTERY_GAUGE_H
