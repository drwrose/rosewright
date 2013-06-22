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

#endif

