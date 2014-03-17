#ifndef BWD_H
#define BWD_H

// This module defines functions for dynamic reading of bitmap objects
// from resources into a BitmapWithData structure.  This supports
// RLE-encoded compression in the resource file.

#include <pebble.h>

typedef struct {
  GBitmap *bitmap;
  uint8_t *data;
} BitmapWithData;

BitmapWithData bwd_create(GBitmap *bitmap, void *data);
void bwd_destroy(BitmapWithData *bwd);
BitmapWithData png_bwd_create(int resource_id);
BitmapWithData rle_bwd_create(int resource_id);

#endif
