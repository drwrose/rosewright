#ifndef HAND_TABLE_H
#define HAND_TABLE_H

struct BitmapHandTableRow {
  unsigned char image_id;
  unsigned char mask_id;
  signed char cx;
  signed char cy;
  unsigned char flip_x;
  unsigned char flip_y;
  unsigned char paint_black;
};

struct VectorHandGroup {
  int outline;
  int fill;
  GPathInfo path_info;
};

struct VectorHandTable {
  int num_groups;
  struct VectorHandGroup *group;
};

// These symbols are used to define the stacking order for hands.

#define STACKING_ORDER_HOUR 1
#define STACKING_ORDER_MINUTE 2
#define STACKING_ORDER_SECOND 3
#define STACKING_ORDER_CHRONO_MINUTE 4
#define STACKING_ORDER_CHRONO_SECOND 5
#define STACKING_ORDER_CHRONO_TENTH 6
#define STACKING_ORDER_DONE 0

#endif

