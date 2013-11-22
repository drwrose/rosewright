#ifndef BLUETOOTH_INDICATOR_H
#define BLUETOOTH_INDICATOR_H

void init_bluetooth_indicator(Layer *window_layer, int x, int y, bool on_black, bool opaque_layer);
void deinit_bluetooth_indicator();
void refresh_bluetooth_indicator();

#endif  // BLUETOOTH_INDICATOR_H
