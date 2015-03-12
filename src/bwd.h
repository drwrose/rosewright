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

void bwd_adjust_colors(BitmapWithData *bwd, uint8_t and_argb8, uint8_t or_argb8, uint8_t xor_argb8);

#endif
