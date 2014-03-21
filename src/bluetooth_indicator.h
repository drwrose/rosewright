#ifndef BLUETOOTH_INDICATOR_H
#define BLUETOOTH_INDICATOR_H

void init_bluetooth_indicator(Layer *window_layer, int x, int y, bool invert, bool opaque_layer);
void move_bluetooth_indicator(int x, int y, bool invert, bool opaque_layer);
void deinit_bluetooth_indicator();
void refresh_bluetooth_indicator();

#endif  // BLUETOOTH_INDICATOR_H
