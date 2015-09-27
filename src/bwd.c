#include "bwd.h"
#include "assert.h"
#include "pebble_compat.h"
#include "../resources/generated_config.h"
#include "wright.h"  // for app_log() macro
//#define SUPPORT_RLE 1

int bwd_resource_reads = 0;
int bwd_cache_hits = 0;
size_t bwd_cache_total_size = 0;

#ifdef SUPPORT_RESOURCE_CACHE
void bwd_clear_cache(struct ResourceCache *resource_cache, size_t resource_cache_size) {
  for (int i = 0; i < (int)resource_cache_size; ++i) {
    struct ResourceCache *cache = &resource_cache[i];
    if (cache->data != NULL) {
      bwd_cache_total_size -= cache->data_size;
      free(cache->data);
      cache->data = NULL;
    }
  }
}
#endif  // SUPPORT_RESOURCE_CACHE

#ifdef SUPPORT_RESOURCE_CACHE
static void fill_cache(struct ResourceCache *cache, int resource_id) {
  if (cache->data == NULL) {
    // Go read the resource data.
    ++bwd_resource_reads;
    ResHandle rh = resource_get_handle(resource_id);
    cache->data_size = resource_size(rh);
    cache->data = (unsigned char *)malloc(cache->data_size);
    if (cache->data != NULL) {
      bwd_cache_total_size += cache->data_size;
      size_t bytes_copied = resource_load(rh, cache->data, cache->data_size);
      assert(bytes_copied == cache->data_size);
    }
  } else {
    ++bwd_cache_hits;
  }
}
#endif  // SUPPORT_RESOURCE_CACHE

BitmapWithData bwd_create(GBitmap *bitmap, unsigned char *data) {
  BitmapWithData bwd;
  bwd.bitmap = bitmap;
  bwd.data = data;
  return bwd;
}

void bwd_destroy(BitmapWithData *bwd) {
  if (bwd->data != NULL) {
    free(bwd->data);
    bwd->data = NULL;
  }
  if (bwd->bitmap != NULL) {
    gbitmap_destroy(bwd->bitmap);
    bwd->bitmap = NULL;
  }
}

BitmapWithData bwd_copy(BitmapWithData *source) {
  BitmapWithData dest;

  GSize size = gbitmap_get_bounds(source->bitmap).size;

#ifndef PBL_PLATFORM_APLITE
  GBitmapFormat format = gbitmap_get_format(source->bitmap);
  dest.bitmap = gbitmap_create_blank(size, format);

  size_t palette_count = 0;
  switch (format) {
  case GBitmapFormat1Bit:
  case GBitmapFormat8Bit:
  case GBitmapFormat8BitCircular:
    break;
    
  case GBitmapFormat1BitPalette:
    palette_count = 2;
    break;
    
  case GBitmapFormat2BitPalette:
    palette_count = 4;
    break;
    
  case GBitmapFormat4BitPalette:
    palette_count = 16;
    break;
  }

  if (palette_count != 0) {
    GColor *source_palette = gbitmap_get_palette(source->bitmap);
    GColor *dest_palette = gbitmap_get_palette(dest.bitmap);
    if (dest_palette == NULL) {
      bwd_destroy(&dest);
      return dest;
    }
    memcpy(dest_palette, source_palette, palette_count);
  }
  
#else  // PBL_PLATFORM_APLITE
  dest.bitmap = __gbitmap_create_blank(size);
#endif  // PBL_PLATFORM_APLITE
  if (dest.bitmap == NULL) {
    return dest;
  }

  int stride = gbitmap_get_bytes_per_row(source->bitmap);
  uint8_t *source_data = gbitmap_get_data(source->bitmap);
  size_t data_size = stride * size.h;
  
  uint8_t *dest_data = gbitmap_get_data(dest.bitmap);
  memcpy(dest_data, source_data, data_size);

  return dest;
}

// Initialize a bitmap from a regular unencoded resource (i.e. as
// loaded from a png file).  This is the same as
// gbitmap_create_with_resource(), but wrapped within the
// BitmapWithData interface to be consistent with rle_bwd_create().
// The returned bitmap must be released with bwd_destroy().
BitmapWithData png_bwd_create(int resource_id) {
  ++bwd_resource_reads;
  GBitmap *image = gbitmap_create_with_resource(resource_id);
  return bwd_create(image, NULL);
}

#ifdef SUPPORT_RESOURCE_CACHE
BitmapWithData png_bwd_create_with_cache(int resource_id_offset, int resource_id, struct ResourceCache *resource_cache, size_t resource_cache_size) {
  int index = resource_id - resource_id_offset;
  if (index >= (int)resource_cache_size) {
    // No cache in use.
    return png_bwd_create(resource_id);
  }

  struct ResourceCache *cache = &resource_cache[index];
  fill_cache(cache, resource_id);
  if (cache->data == NULL) {
    // Whoops, not enough RAM.
    return bwd_create(NULL, NULL);
  }

#ifndef PBL_PLATFORM_APLITE
  GBitmap *image = gbitmap_create_from_png_data(cache->data, cache->data_size);
  return bwd_create(image, NULL);
#else  // PBL_PLATFORM_APLITE
  unsigned char *data = (unsigned char *)malloc(cache->data_size);
  if (data == NULL) {
    return bwd_create(NULL, NULL);
  }
  memcpy(data, cache->data, cache->data_size);
  GBitmap *image = gbitmap_create_with_data(data);
  return bwd_create(image, data);
#endif  // PBL_PLATFORM_APLITE
}
#endif  // SUPPORT_RESOURCE_CACHE

#ifndef SUPPORT_RLE

// Here's the dummy implementation of rle_bwd_create(), if SUPPORT_RLE
// is not defined.
BitmapWithData rle_bwd_create(int resource_id) {
  return png_bwd_create(resource_id);
}

#ifdef SUPPORT_RESOURCE_CACHE
BitmapWithData rle_bwd_create_with_cache(int resource_id_offset, int resource_id, struct ResourceCache *resource_cache, size_t resource_cache_size) {
  return png_bwd_create_with_cache(resource_id_offset, resource_id, resource_cache, resource_cache_size);
}
#endif  // SUPPORT_RESOURCE_CACHE

#else  // SUPPORT_RLE

// Here's the proper implementation of rle_bwd_create() and its support functions.

#define RBUFFER_SIZE 64
typedef struct {
  ResHandle _rh;
  size_t _i;
  size_t _filled_size;
  size_t _bytes_read;
  size_t _total_size;
  const uint8_t *_data;
  uint8_t _buffer[RBUFFER_SIZE];
} RBuffer;

// Begins reading from a raw resource.  Should be matched by a later
// call to rbuffer_deinit().
static void rbuffer_init_resource(RBuffer *rb, int resource_id, size_t offset) {
  //  rb->_buffer = (uint8_t *)malloc(RBUFFER_SIZE);
  //  assert(rb->_buffer != NULL);
  
  rb->_rh = resource_get_handle(resource_id);
  rb->_total_size = resource_size(rb->_rh);
  rb->_i = 0;
  rb->_filled_size = 0;
  rb->_bytes_read = offset;
  rb->_data = rb->_buffer;
}

// Begins reading from a data buffer.  The data buffer should not be
// freed during the lifetime of the RBuffer.  Should be matched by a
// later call to rbuffer_deinit().
static void rbuffer_init_data(RBuffer *rb, unsigned char *data, size_t data_size) {
  //  rb->_buffer = (uint8_t *)malloc(RBUFFER_SIZE);
  //  assert(rb->_buffer != NULL);
  
  rb->_rh = 0;
  rb->_i = 0;
  rb->_total_size = rb->_filled_size = rb->_bytes_read = data_size;
  rb->_data = data;
}

// Splits an RBuffer into two discrete parts.  rb_front is truncated
// at the specified byte (it reads up to but not including point), and
// the rb_back is initialized with a new RBuffer that receives all of
// the bytes of the first RBuffer from point to the end.  The caller
// should eventually call rbuffer_deinit() on rb_back, which should
// not persist longer than rb_front does.
static void rbuffer_split(RBuffer *rb_front, RBuffer *rb_back, size_t point) {
  rb_back->_rh = rb_front->_rh;
  rb_back->_total_size = rb_front->_total_size;
  rb_back->_i = 0;
  rb_back->_filled_size = 0;
  rb_back->_bytes_read = point;
  rb_back->_data = rb_back->_buffer;
  if (rb_front->_rh == 0) {
    // This is an in-memory RBuffer.
    assert(rb_front->_data != rb_front->_buffer && rb_front->_filled_size == rb_front->_total_size);
    rb_back->_data = rb_front->_data;
    rb_back->_bytes_read = rb_back->_filled_size = rb_front->_total_size;
    rb_back->_i = point;
  }
  
  if (rb_front->_total_size > point) {
    rb_front->_total_size = point;

    if (rb_front->_bytes_read > point) {
      size_t bytes_over = rb_front->_bytes_read - point;
      if (rb_front->_filled_size > bytes_over) {
        rb_front->_filled_size -= bytes_over;
      } else {
        // Whoops, we've already overrun the new point.
        rb_front->_filled_size = 0;
        rb_front->_i = 0;
        assert(false);
      }
    }
  }
}

// Gets the next byte from the rbuffer.  Returns EOF at end.
static int rbuffer_getc(RBuffer *rb) {
  if (rb->_i >= rb->_filled_size) {
    // We've read past the end of the in-memory buffer.  Go fill the
    // buffer with more bytes from the resource.
    if (rb->_total_size > rb->_bytes_read) {
      // More bytes available to read; read them now.
      size_t bytes_remaining = rb->_total_size - rb->_bytes_read;
      size_t try_to_read = (bytes_remaining < RBUFFER_SIZE) ? bytes_remaining : (size_t)RBUFFER_SIZE;
      assert(rb->_rh != 0);
      size_t bytes_read = resource_load_byte_range(rb->_rh, rb->_bytes_read, rb->_buffer, try_to_read);
      //assert((ssize_t)bytes_read >= 0);
      if ((ssize_t)bytes_read < 0) {
        bytes_read = 0;
      }
      rb->_filled_size = bytes_read;
      rb->_bytes_read += bytes_read;
    } else {
      // No more bytes to read.
      rb->_filled_size = 0;
    }
    rb->_i = 0;
  }
  if (rb->_i >= rb->_filled_size) {
    // We've reached the end of the resource.
    return EOF;
  }

  int result = rb->_data[rb->_i];
  rb->_i++;
  return result;
}

// Frees the resources reserved in rbuffer_init().
static void rbuffer_deinit(RBuffer *rb) {
  //  assert(rb->_buffer != NULL);
  //  free(rb->_buffer);
  //rb->_buffer = NULL;
}

// Used to unpack the integers of an rl2-encoding back into their
// original rle sequence.  See make_rle.py.
typedef struct {
  RBuffer *rb;
  int n;
  int b;
  int bi;
  bool zero_expands;
} Rl2Unpacker;

static void rl2unpacker_init(Rl2Unpacker *rl2, RBuffer *rb, int n, bool zero_expands) {
  // assumption: n is an integer divisor of 8.
  assert(n * (8 / n) == 8);

  rl2->rb = rb;
  rl2->n = n;
  rl2->b = rbuffer_getc(rb);
  rl2->bi = 8;
  rl2->zero_expands = zero_expands;
}

// Gets the next integer from the rl2 encoding.  Returns EOF at end.
static int rl2unpacker_getc(Rl2Unpacker *rl2) {
  if (rl2->b == EOF) {
    return EOF;
  }

  // First, count the number of zero chunks until we come to a nonzero chunk.
  int zero_count = 0;
  int bmask = (1 << rl2->n) - 1;
  int bv = (rl2->b & (bmask << (rl2->bi - rl2->n)));
  if (rl2->zero_expands) {
    while (bv == 0) {
      ++zero_count;
      rl2->bi -= rl2->n;
      if (rl2->bi <= 0) {
        rl2->b = rbuffer_getc(rl2->rb);
        rl2->bi = 8;
        if (rl2->b == EOF) {
          return EOF;
        }
      }
      bv = (rl2->b & (bmask << (rl2->bi - rl2->n)));
    }
  }
    
  // Infer from that the number of chunks, and hence the number of
  // bits, that make up the value we will extract.
  int num_chunks = (zero_count + 1);
  int bit_count = num_chunks * rl2->n;

  // OK, now we need to extract the next bitCount bits into a word.
  int result = 0;
  while (bit_count >= rl2->bi) {
    int mask = (1 << rl2->bi) - 1;
    int value = (rl2->b & mask);
    result = (result << rl2->bi) | value;
    bit_count -= rl2->bi;

    rl2->b = rbuffer_getc(rl2->rb);
    rl2->bi = 8;
    if (rl2->b == EOF) {
      break;
    }
  }

  if (bit_count > 0) {
    // A partial word in the middle of the byte.
    int bottom_count = rl2->bi - bit_count;
    assert(bottom_count > 0);
    int mask = ((1 << bit_count) - 1);
    int value = ((rl2->b >> bottom_count) & mask);
    result = (result << bit_count) | value;
    rl2->bi -= bit_count;
  }

  return result;
}

// Xors the image in-place a 1x1 checkerboard pattern.  The idea is to
// eliminate this kind of noise from the source image if it happens to
// be present.
void unscreen_bitmap(GBitmap *image) {
  int height = gbitmap_get_bounds(image).size.h;
  int width = gbitmap_get_bounds(image).size.w;
  int width_bytes = width / 8;
  int stride = gbitmap_get_bytes_per_row(image); // multiple of 4, by Pebble convention.
  uint8_t *data = gbitmap_get_data(image);

  uint8_t mask = 0xaa;
  for (int y = 0; y < height; ++y) {
    uint8_t *p = data + y * stride;
    for (int x = 0; x < width_bytes; ++x) {
      (*p) ^= mask;
      ++p;
    }
    mask ^= 0xff;
  }
}

typedef void Packer(int value, int count, int *b, uint8_t **dp, uint8_t *dp_stop);

// Packs a series of identical 1-bit values into (*dp) beginning at bit (*b).
void pack_1bit(int value, int count, int *b, uint8_t **dp, uint8_t *dp_stop) {
  assert(*dp < dp_stop);
  
  if (value) {
    // Generate count 1-bits.
    int b1 = (*b) + count;
    if (b1 < 8) {
      // We're still within the same byte.
      int mask = ~((1 << (*b)) - 1);
      mask &= ((1 << (b1)) - 1);
      *(*dp) |= mask;
      (*b) = b1;
    } else {
      // We've crossed over a byte boundary.
      *(*dp) |= ~((1 << (*b)) - 1);
      ++(*dp);
      (*b) += 8;
      while (b1 / 8 != (*b) / 8) {
        assert(*dp < dp_stop);
        *(*dp) = 0xff;
        ++(*dp);
        (*b) += 8;
      }
      b1 = b1 % 8;
      if (b1 != 0) {
        assert(*dp < dp_stop);
        *(*dp) |= ((1 << (b1)) - 1);
      }
      (*b) = b1;
    }
  } else {
    // Skip over count 0-bits.
    (*b) += count;
    (*dp) += (*b) / 8;
    (*b) = (*b) % 8;
  }
}

#ifndef PBL_PLATFORM_APLITE
// The following functions are needed for unpacking advanced color
// modes not supported on Aplite.

// Packs a series of identical 2-bit values into (*dp) beginning at bit (*b).
void pack_2bit(int value, int count, int *b, uint8_t **dp, uint8_t *dp_stop) {
  assert(*dp < dp_stop);

  if (value != 0) {
    while (count > 0 && (*b) < 8) {
      // Put stuff in the middle or at the end of the first byte.
      assert(*dp < dp_stop);
      *(*dp) |= (value << (6 - (*b)));
      (*b) += 2;
      --count;
    }
    if ((*b) == 8) {
      ++(*dp);
      (*b) = 0;
    }
    assert((*b) == 0);
    uint8_t byte = (value << 6) | (value << 4) | (value << 2) | value;
    while (count >= 4) {
      // Now pack a full byte's worth at a time.
      assert(*dp < dp_stop);
      *(*dp) = byte;
      ++(*dp);
      count -= 4;
    }
    while (count > 0) {
      // Put stuff at the beginning of the last byte.
      assert((*b) < 8);
      assert(*dp < dp_stop);
      *(*dp) |= (value << (6 - (*b)));
      (*b) += 2;
      --count;
    }

  } else {
    // Skip over count 0-chunks.
    (*b) += count * 2;
    (*dp) += (*b) / 8;
    (*b) = (*b) % 8;
  }    
}


// Packs a series of identical 4-bit values into (*dp) beginning at bit (*b).
void pack_4bit(int value, int count, int *b, uint8_t **dp, uint8_t *dp_stop) {
  assert(*dp < dp_stop);

  if (value != 0) {
    if (count > 0 && (*b) == 4) {
      // Pack a nibble at the end of the first byte.
      assert(*dp < dp_stop);
      *(*dp) |= value;
      ++(*dp);
      (*b) = 0;
      --count;
    }
    uint8_t byte = (value << 4) | value;
    while (count >= 2) {
      // Now pack a full byte's worth at a time.
      assert(*dp < dp_stop);
      *(*dp) = byte;
      ++(*dp);
      count -= 2;
    }
    if (count == 1) {
      // Pack one more nibble at the top of the next byte.
      assert(*dp < dp_stop);
      *(*dp) |= (value << 4);
      (*b) = 4;
    }

  } else {
    // Skip over count 0-nibbles.
    (*b) += count * 4;
    (*dp) += (*b) / 8;
    (*b) = (*b) % 8;
  }    
}

// Packs a series of identical 8-bit values into (*dp).
void pack_8bit(int value, int count, int *b, uint8_t **dp, uint8_t *dp_stop) {
  assert(*dp < dp_stop);

  while (count > 0 && (*dp) < dp_stop) {
    assert((*dp) < dp_stop);
    *(*dp) = value;
    ++(*dp);
    --count;
  }
}

// Initialize a bitmap from an rle-encoded resource.  The returned
// bitmap must be released with bwd_destroy().  See make_rle.py for
// the program that generates these rle sequences.
BitmapWithData
rle_bwd_create_rb(RBuffer *rb) {
  // RLE header (NB: All fields are little-endian)
  //         (uint8_t)  width
  //         (uint8_t)  height
  //         (uint8_t)  n (number of chunks of pixels to take at a time; unscreen if 0x80 set)
  //         (uint8_t)  format (see below)
  //         (uint16_t) offset to start of values, or 0 if format == 0
  //         (uint16_t) offset to start of palette, or 0 if format <= 1
  
  int width = rbuffer_getc(rb);
  int height = rbuffer_getc(rb);
  int n = rbuffer_getc(rb);
  GBitmapFormat format = (GBitmapFormat)rbuffer_getc(rb);

  uint8_t vo_lo = rbuffer_getc(rb);
  uint8_t vo_hi = rbuffer_getc(rb);
  unsigned int vo = (vo_hi << 8) | vo_lo;

  uint8_t po_lo = rbuffer_getc(rb);
  uint8_t po_hi = rbuffer_getc(rb);
  unsigned int po = (po_hi << 8) | po_lo;

  assert(vo != 0 && po >= vo && po <= rb->_total_size);
  
  int do_unscreen = (n & 0x80);
  n = n & 0x7f;

  Packer *packer_func = NULL;
  size_t palette_count = 0;
  int vn = 0;
  switch (format) {
  case GBitmapFormat1BitPalette:
    palette_count = 2;
    // fall through.
  case GBitmapFormat1Bit:
    packer_func = pack_1bit;
    break;
    
  case GBitmapFormat2BitPalette:
    vn = 2;
    palette_count = 4;
    packer_func = pack_2bit;
    break;
    
  case GBitmapFormat4BitPalette:
    vn = 4;
    palette_count = 16;
    packer_func = pack_4bit;
    break;

  case GBitmapFormat8Bit:
    vn = 8;
    packer_func = pack_8bit;
    break;
  }
  assert(packer_func != NULL);

  GColor *palette = NULL;
  if (palette_count != 0) {
    palette = (GColor *)malloc(palette_count * sizeof(GColor));
  }
  
  GBitmap *image = gbitmap_create_blank_with_palette(GSize(width, height), format, palette, true);
  if (image == NULL) {
    free(palette);
    return bwd_create(NULL, NULL);
  }
  int stride = gbitmap_get_bytes_per_row(image);
  uint8_t *bitmap_data = gbitmap_get_data(image);
  assert(bitmap_data != NULL);
  size_t data_size = height * stride;

  Rl2Unpacker rl2;
  rl2unpacker_init(&rl2, rb, n, true);

  // The values start at vo; this means the original rb buffer gets
  // shortened to that point.  We also create a new rb_vo buffer to
  // read the values data which begins at vo.
  RBuffer rb_vo;
  rbuffer_split(rb, &rb_vo, vo);

  Rl2Unpacker rl2_vo;
  if (vn != 0) {
    rl2unpacker_init(&rl2_vo, &rb_vo, vn, false);
  }

  uint8_t *dp = bitmap_data;
  uint8_t *dp_stop = dp + data_size;
  int b = 0;
  
  if (packer_func == pack_1bit) {
    // Unpack a 1-bit file.

    // The initial value is 0.
    int value = 0;
    int count = rl2unpacker_getc(&rl2);
    if (count != EOF) {
      assert(count > 0);
      // We discard the first, implicit black pixel; it's not part of the image.
      --count;
    }
    while (count != EOF) {
      pack_1bit(value, count, &b, &dp, dp_stop);
      value = 1 - value;
      count = rl2unpacker_getc(&rl2);
    }

  } else {
    // Unpack a 2-, 4-, or 8-bit file.

    // Begin reading.
    int count = rl2unpacker_getc(&rl2);
    while (count != EOF) {
      int value = rl2unpacker_getc(&rl2_vo);
      (*packer_func)(value, count, &b, &dp, dp_stop);
      count = rl2unpacker_getc(&rl2);
    }
  }

  //app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "wrote %d bytes", dp - bitmap_data);
  assert(dp == dp_stop && b == 0);
  
  if (do_unscreen) {
    unscreen_bitmap(image);
  }

  if (palette_count != 0) {
    // Now we need to apply the palette.
    RBuffer rb_po;
    rbuffer_split(&rb_vo, &rb_po, po);
    for (int i = 0; i < (int)palette_count; ++i) {
      palette[i].argb = rbuffer_getc(&rb_po);
    }
    rbuffer_deinit(&rb_po);
  }
  
  rbuffer_deinit(&rb_vo);
  
  return bwd_create(image, NULL);
}

#else  // PBL_PLATFORM_APLITE

// Here's the simpler Aplite implementation, which only supports GColorFormat1Bit.

// Initialize a bitmap from an rle-encoded resource.  The returned
// bitmap must be released with bwd_destroy().  See make_rle.py for
// the program that generates these rle sequences.
BitmapWithData
rle_bwd_create_rb(RBuffer *rb) {
  // RLE header (NB: All fields are little-endian)
  //         (uint8_t)  width
  //         (uint8_t)  height
  //         (uint8_t)  n (number of chunks of pixels to take at a time; unscreen if 0x80 set)
  //         (uint8_t)  format (see below)
  //         (uint16_t) offset to start of values, or 0 if format == 0
  //         (uint16_t) offset to start of palette, or 0 if format <= 1
  
  int width = rbuffer_getc(rb);
  int height = rbuffer_getc(rb);
  assert(width > 0 && width <= SCREEN_WIDTH && height > 0 && height <= SCREEN_HEIGHT);
  int n = rbuffer_getc(rb);
  int format = rbuffer_getc(rb);
  if (format != 0) {
    app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "cannot support format %d", format);
    return bwd_create(NULL, NULL);
  }

  /*uint8_t vo_lo = */rbuffer_getc(rb);
  /*uint8_t vo_hi = */rbuffer_getc(rb);
  /*uint8_t po_lo = */rbuffer_getc(rb);
  /*uint8_t po_hi = */rbuffer_getc(rb);
  
  int do_unscreen = (n & 0x80);
  n = n & 0x7f;

  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "reading bitmap %d x %d, n = %d, format = %d", width, height, n, format);
  
  GBitmap *image = __gbitmap_create_blank(GSize(width, height));
  if (image == NULL) {
    return bwd_create(NULL, NULL);
  }
  int stride = gbitmap_get_bytes_per_row(image);
  uint8_t *bitmap_data = gbitmap_get_data(image);
  assert(bitmap_data != NULL);
  size_t data_size = height * stride;
  //app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "stride = %d, data_size = %d", stride, data_size);

  Rl2Unpacker rl2;
  rl2unpacker_init(&rl2, rb, n, true);

  uint8_t *dp = bitmap_data;
  uint8_t *dp_stop = dp + data_size;
  int b = 0;
  
  // Unpack a 1-bit file.

  // The initial value is 0.
  int value = 0;
  int count = rl2unpacker_getc(&rl2);
  if (count != EOF) {
    assert(count > 0);
    // We discard the first, implicit black pixel; it's not part of the image.
    --count;
  }
  while (count != EOF) {
    pack_1bit(value, count, &b, &dp, dp_stop);
    value = 1 - value;
    count = rl2unpacker_getc(&rl2);
  }

  //app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "wrote %d bytes", dp - bitmap_data);
  assert(dp == dp_stop && b == 0);
  
  if (do_unscreen) {
    unscreen_bitmap(image);
  }
  
  return bwd_create(image, NULL);
}

#endif // PBL_PLATFORM_APLITE

BitmapWithData
rle_bwd_create(int resource_id) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "rle_bwd_create(%d)", resource_id);
  ++bwd_resource_reads;
  
  RBuffer rb;
  rbuffer_init_resource(&rb, resource_id, 0);
  BitmapWithData result = rle_bwd_create_rb(&rb);
  rbuffer_deinit(&rb);
  return result;
}

#ifdef SUPPORT_RESOURCE_CACHE
BitmapWithData rle_bwd_create_with_cache(int resource_id_offset, int resource_id, struct ResourceCache *resource_cache, size_t resource_cache_size) {
  int index = resource_id - resource_id_offset;
  if (index >= (int)resource_cache_size) {
    // No cache in use.
    return rle_bwd_create(resource_id);
  }

  struct ResourceCache *cache = &resource_cache[index];
  fill_cache(cache, resource_id);
  if (cache->data == NULL) {
    // Whoops, not enough RAM.
    return bwd_create(NULL, NULL);
  }

  RBuffer rb;
  rbuffer_init_data(&rb, cache->data, cache->data_size);
  BitmapWithData result = rle_bwd_create_rb(&rb);
  rbuffer_deinit(&rb);
  return result;
}
#endif  // SUPPORT_RESOURCE_CACHE

#endif  // SUPPORT_RLE

// Replace each of the R, G, B channels with a different color, and
// blend the result together.  Only supported for palette bitmaps.
void bwd_remap_colors(BitmapWithData *bwd, GColor cb, GColor c1, GColor c2, GColor c3, bool invert_colors) {
#ifndef PBL_PLATFORM_APLITE
  if (bwd->bitmap == NULL) {
    return;
  }
  GBitmapFormat format = gbitmap_get_format(bwd->bitmap);
  int palette_size = 0;
  switch (format) {
  case GBitmapFormat1BitPalette:
    palette_size = 2;
    break;
  case GBitmapFormat2BitPalette:
    palette_size = 4;
    break;
  case GBitmapFormat4BitPalette:
    palette_size = 16;
    break;

  case GBitmapFormat1Bit:
  case GBitmapFormat8Bit:
  default:
    // We just refuse to adjust true-color images.  Technically, we
    // could apply the adjustment at least to GBitmapFormat8Bit images
    // (by walking through all of the pixels), but instead we'll flag
    // it as an error, to help catch accidental mistakes in image
    // preparation.
    app_log(APP_LOG_LEVEL_WARNING, __FILE__, __LINE__, "bwd_remap_colors cannot adjust non-palette format %d", format);
    return;
  }

  assert(palette_size != 0);
  GColor *palette = gbitmap_get_palette(bwd->bitmap);
  assert(palette != NULL);

  for (int pi = 0; pi < palette_size; ++pi) {
    int r = cb.r;
    int g = cb.g;
    int b = cb.b;
    
    GColor p = palette[pi];

    r = (3 * r + p.r * (c1.r - r)) / 3;  // Blend from r to c1.r
    r = (3 * r + p.g * (c2.r - r)) / 3;  // Blend from r to c2.r
    r = (3 * r + p.b * (c3.r - r)) / 3;  // Blend from r to c3.r

    g = (3 * g + p.r * (c1.g - g)) / 3;  // Blend from g to c1.g
    g = (3 * g + p.g * (c2.g - g)) / 3;  // Blend from g to c2.g
    g = (3 * g + p.b * (c3.g - g)) / 3;  // Blend from g to c3.g

    b = (3 * b + p.r * (c1.b - b)) / 3;  // Blend from b to c1.b
    b = (3 * b + p.g * (c2.b - b)) / 3;  // Blend from b to c2.b
    b = (3 * b + p.b * (c3.b - b)) / 3;  // Blend from b to c3.b

    palette[pi].r = (r < 0x3) ? r : 0x3;
    palette[pi].g = (g < 0x3) ? g : 0x3;
    palette[pi].b = (b < 0x3) ? b : 0x3;

    //    GColor q = palette[pi]; app_log(APP_LOG_LEVEL_WARNING, __FILE__, __LINE__, "cb = %02x, c1 = %02x, c2 = %02x, c3 = %02x.  %d: %02x/%02x/%02x/%02x becomes %02x/%02x/%02x/%02x (%d, %d, %d)", cb.argb, c1.argb, c2.argb, c3.argb, pi, p.argb & 0xc0, p.argb & 0x30, p.argb & 0x0c, p.argb & 0x03, q.argb & 0xc0, q.argb & 0x30, q.argb & 0x0c, q.argb & 0x03, r, g, b);
    
    if (invert_colors) {
      palette[pi].argb ^= 0x3f;
    }
  }
    
#endif // PBL_PLATFORM_APLITE
}
