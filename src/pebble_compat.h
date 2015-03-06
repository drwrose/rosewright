#ifndef PEBBLE_COMPAT_H
#define PEBBLE_COMPAT_H

// Some definitions to aid in cross-compatibility between Aplite and Basalt.

// GBitmap accessors, not appearing on Aplite.
#ifdef PBL_PLATFORM_APLITE
#define gbitmap_get_bounds(bm) ((bm)->bounds)
#define gbitmap_get_bytes_per_row(bm) ((bm)->row_size_bytes)
#define gbitmap_get_data(bm) ((bm)->addr)
#define gbitmap_get_format(bm) (0)
#endif  // PBL_PLATFORM_APLITE

// GColor static initializers, slightly different syntax needed on Basalt.
#ifdef PBL_PLATFORM_APLITE

#define GColorBlackInit GColorBlack
#define GColorWhiteInit GColorWhite
#define GColorClearInit GColorClear

#else  // PBL_PLATFORM_APLITE

#define GColorBlackInit { GColorBlackARGB8 }
#define GColorWhiteInit { GColorWhiteARGB8 }
#define GColorClearInit { GColorClearARGB8 }

#endif  // PBL_PLATFORM_APLITE


#endif
