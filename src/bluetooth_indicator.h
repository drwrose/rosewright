#ifndef BLUETOOTH_INDICATOR_H
#define BLUETOOTH_INDICATOR_H

void init_bluetooth_indicator();
void deinit_bluetooth_indicator();
void draw_bluetooth_indicator(GContext *ctx, int x, int y, bool invert);

#endif  // BLUETOOTH_INDICATOR_H
