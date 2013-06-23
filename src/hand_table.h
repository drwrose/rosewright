#ifndef HAND_TABLE_H
#define HAND_TABLE_H

struct HandTable {
  int image_id;
  int mask_id;
  int cx;
  int cy;
  int flip_x;
  int flip_y;
  int paint_black;
};

// These symbols are used to define the stacking order for hands.
#define STACKING_ORDER_HOUR 1
#define STACKING_ORDER_MINUTE 2
#define STACKING_ORDER_SECOND 3
#define STACKING_ORDER_CHRONO_MINUTE 4
#define STACKING_ORDER_CHRONO_SECOND 5
#define STACKING_ORDER_DONE 0

#endif

