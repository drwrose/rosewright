#ifndef BWD_H
#define BWD_H

// This module defines functions for dynamic reading of bitmap objects
// from resources into a BitmapWithData structure.  This supports
// RLE-encoded compression in the resource file.

#include <pebble.h>

typedef struct {
  GBitmap *bitmap;
  // We used to piggyback additional data here, but this is no longer
  // needed.  Who knows, maybe one day we'll add something else.
} BitmapWithData;

BitmapWithData bwd_create(GBitmap *bitmap);
void bwd_destroy(BitmapWithData *bwd);
BitmapWithData png_bwd_create(int resource_id);
BitmapWithData rle_bwd_create(int resource_id);

void bwd_remap_colors(BitmapWithData *bwd, GColor cb, GColor c1, GColor c2, GColor c3, bool invert_colors);

#endif
