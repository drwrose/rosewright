#ifndef HAND_TABLE_H
#define HAND_TABLE_H

struct __attribute__((__packed__)) BitmapHandCenterRow {
  signed int cx:8;
  signed int cy:8;
};

struct __attribute__((__packed__)) BitmapHandTableRow {
  unsigned int lookup_index:6;
  unsigned int flip_x:1;
  unsigned int flip_y:1;
};

struct __attribute__((__packed__)) VectorHandGroup {
  int outline:8;
  int fill:8;
  GPathInfo path_info;
};

struct __attribute__((__packed__)) VectorHandTable {
  unsigned int num_groups:8;
  struct VectorHandGroup *group;
};

struct __attribute__((__packed__)) HandDef {
  unsigned char num_steps;
  unsigned char resource_id, resource_mask_id;
  signed short place_x, place_y;
  bool use_rle;
  bool paint_black;
  struct BitmapHandCenterRow *bitmap_centers;
  struct BitmapHandTableRow *bitmap_table;
  struct VectorHandTable *vector_hand;
};

// These symbols are used to define the stacking order for hands.

#define STACKING_ORDER_HOUR 1
#define STACKING_ORDER_MINUTE 2
#define STACKING_ORDER_SECOND 3
#define STACKING_ORDER_CHRONO_MINUTE 4
#define STACKING_ORDER_CHRONO_SECOND 5
#define STACKING_ORDER_CHRONO_TENTH 6
#define STACKING_ORDER_DONE 0

// A table for bluetooth and battery icons, as well as day/date cards.
struct __attribute__((__packed__)) IndicatorTable {
  unsigned char x, y;
  unsigned int invert:1;
  unsigned int opaque:1;
};

#endif

