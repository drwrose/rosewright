#ifndef BWD_H
#define BWD_H

// This module defines functions for dynamic reading of bitmap objects
// from resources into a BitmapWithData structure.  This supports
// RLE-encoded compression in the resource file.

#include <pebble.h>
#include "../resources/generated_config.h"

typedef struct __attribute__((__packed__)) {
  GBitmap *bitmap;
  unsigned char *data;
} BitmapWithData;

// An array of these elements maintains an in-memory cache of resource
// data so we don't have to go to the resource file all the time.
struct __attribute__((__packed__)) ResourceCache {
  // We once kept the raw resource data here, but now we've got so
  // much elbow room to play with, we'll keep the decoded bitmaps
  // instead.
  BitmapWithData bwd;
};

extern int bwd_resource_reads;
extern int bwd_cache_hits;
extern size_t bwd_cache_total_size;

BitmapWithData bwd_create(GBitmap *bitmap, unsigned char *data);
void bwd_destroy(BitmapWithData *bwd);
BitmapWithData bwd_copy(BitmapWithData *source);
BitmapWithData bwd_copy_bitmap(GBitmap *bitmap);
void bwd_copy_into_from_bitmap(BitmapWithData *dest, GBitmap *source);
BitmapWithData png_bwd_create(int resource_id);
BitmapWithData rle_bwd_create(int resource_id);

#ifdef SUPPORT_RESOURCE_CACHE
void bwd_clear_cache(struct ResourceCache *resource_cache, size_t resource_cache_size);
BitmapWithData png_bwd_create_with_cache(int resource_id_offset, int resource_id, struct ResourceCache *resource_cache, size_t resource_cache_size);
BitmapWithData rle_bwd_create_with_cache(int resource_id_offset, int resource_id, struct ResourceCache *resource_cache, size_t resource_cache_size);

#else  // SUPPORT_RESOURCE_CACHE

#define bwd_clear_cache(resource_cache, resource_cache_size) { }
#define png_bwd_create_with_cache(resource_id_offset, resource_id, resource_cache, resource_cache_size) png_bwd_create(resource_id)
#define rle_bwd_create_with_cache(resource_id_offset, resource_id, resource_cache, resource_cache_size) rle_bwd_create(resource_id)

#endif  // SUPPORT_RESOURCE_CACHE

void bwd_remap_colors(BitmapWithData *bwd, GColor cb, GColor c1, GColor c2, GColor c3, bool invert_colors);

#endif
