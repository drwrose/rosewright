#include "bwd.h"
#include "assert.h"
#include "pebble_compat.h"
#include "../resources/generated_config.h"
//#define SUPPORT_RLE 1

BitmapWithData bwd_create(GBitmap *bitmap) {
  BitmapWithData bwd;
  bwd.bitmap = bitmap;
  return bwd;
}

void bwd_destroy(BitmapWithData *bwd) {
  if (bwd->bitmap != NULL) {
    gbitmap_destroy(bwd->bitmap);
    bwd->bitmap = NULL;
  }
}

// Initialize a bitmap from a regular unencoded resource (i.e. as
// loaded from a png file).  This is the same as
// gbitmap_create_with_resource(), but wrapped within the
// BitmapWithData interface to be consistent with rle_bwd_create().
// The returned bitmap must be released with bwd_destroy().
BitmapWithData png_bwd_create(int resource_id) {
  GBitmap *image = gbitmap_create_with_resource(resource_id);
  return bwd_create(image);
}

#ifndef SUPPORT_RLE

// Here's the dummy implementation of rle_bwd_create(), if SUPPORT_RLE
// is not defined.
BitmapWithData rle_bwd_create(int resource_id) {
  return png_bwd_create(resource_id);
}

#else  // SUPPORT_RLE

// Here's the proper implementation of rle_bwd_create() and its support functions.

#define RBUFFER_SIZE 64
typedef struct {
  ResHandle _rh;
  size_t _i;
  size_t _filled_size;
  size_t _bytes_read;
  size_t _total_size;
  uint8_t _buffer[RBUFFER_SIZE];
} RBuffer;

// Begins reading from a raw resource.  Should be matched by a later
// call to rbuffer_deinit() to free this stuff.
static void rbuffer_init(int resource_id, RBuffer *rb, size_t offset) {
  //  rb->_buffer = (uint8_t *)malloc(RBUFFER_SIZE);
  //  assert(rb->_buffer != NULL);
  
  rb->_rh = resource_get_handle(resource_id);
  rb->_total_size = resource_size(rb->_rh);
  rb->_i = 0;
  rb->_filled_size = 0;
  rb->_bytes_read = offset;
}

// Specifies the maximum number of bytes that may be read from this
// rbuffer.  Effectively shortens the buffer to the indicated size.
static void rbuffer_set_limit(RBuffer *rb, size_t limit) {
  if (rb->_total_size > limit) {
    rb->_total_size = limit;

    if (rb->_bytes_read > limit) {
      size_t bytes_over = rb->_bytes_read - limit;
      if (rb->_filled_size > bytes_over) {
        rb->_filled_size -= bytes_over;
      } else {
        // Whoops, we've already overrun the new limit.
        rb->_filled_size = 0;
        rb->_i = 0;
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

  int result = rb->_buffer[rb->_i];
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
rle_bwd_create(int resource_id) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "rle_bwd_create(%d)", resource_id);

  // RLE header (NB: All fields are little-endian)
  //         (uint8_t)  width
  //         (uint8_t)  height
  //         (uint8_t)  n (number of chunks of pixels to take at a time; unscreen if 0x80 set)
  //         (uint8_t)  format (see below)
  //         (uint16_t) offset to start of values, or 0 if format == 0
  //         (uint16_t) offset to start of palette, or 0 if format <= 1
  
  RBuffer rb;
  rbuffer_init(resource_id, &rb, 0);
  int width = rbuffer_getc(&rb);
  int height = rbuffer_getc(&rb);
  int n = rbuffer_getc(&rb);
  GBitmapFormat format = (GBitmapFormat)rbuffer_getc(&rb);

  uint8_t vo_lo = rbuffer_getc(&rb);
  uint8_t vo_hi = rbuffer_getc(&rb);
  unsigned int vo = (vo_hi << 8) | vo_lo;

  uint8_t po_lo = rbuffer_getc(&rb);
  uint8_t po_hi = rbuffer_getc(&rb);
  unsigned int po = (po_hi << 8) | po_lo;

  assert(vo != 0 && po >= vo && po <= rb._total_size);
  
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
    return bwd_create(NULL);
  }
  int stride = gbitmap_get_bytes_per_row(image);
  uint8_t *bitmap_data = gbitmap_get_data(image);
  assert(bitmap_data != NULL);
  size_t data_size = height * stride;

  Rl2Unpacker rl2;
  rl2unpacker_init(&rl2, &rb, n, true);

  // The values start at vo; this means the original rb buffer gets
  // shortened to that point.
  rbuffer_set_limit(&rb, vo);

  // We create a new rb_vo buffer to read the values data which begins
  // at vo.
  RBuffer rb_vo;
  Rl2Unpacker rl2_vo;
  if (vn != 0) {
    rbuffer_init(resource_id, &rb_vo, vo);
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
  rbuffer_deinit(&rb);
  rbuffer_deinit(&rb_vo);
  
  if (do_unscreen) {
    unscreen_bitmap(image);
  }
  
  if (palette_count != 0) {
    // Now we need to apply the palette.
    ResHandle rh = resource_get_handle(resource_id);
    size_t total_size = resource_size(rh);
    assert(total_size > po);
    size_t palette_size = total_size - po;
    assert(palette_size <= palette_count);
    assert(palette == gbitmap_get_palette(image));
    size_t bytes_read = resource_load_byte_range(rh, po, (uint8_t *)palette, palette_size);
    assert(bytes_read == palette_size);
  }
  
  return bwd_create(image);
}

#else  // PBL_PLATFORM_APLITE

// Here's the simpler Aplite implementation, which only supports GColorFormat1Bit.

// Initialize a bitmap from an rle-encoded resource.  The returned
// bitmap must be released with bwd_destroy().  See make_rle.py for
// the program that generates these rle sequences.
BitmapWithData
rle_bwd_create(int resource_id) {
  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "rle_bwd_create(%d)", resource_id);

  // RLE header (NB: All fields are little-endian)
  //         (uint8_t)  width
  //         (uint8_t)  height
  //         (uint8_t)  n (number of chunks of pixels to take at a time; unscreen if 0x80 set)
  //         (uint8_t)  format (see below)
  //         (uint16_t) offset to start of values, or 0 if format == 0
  //         (uint16_t) offset to start of palette, or 0 if format <= 1
  
  RBuffer rb;
  rbuffer_init(resource_id, &rb, 0);
  int width = rbuffer_getc(&rb);
  int height = rbuffer_getc(&rb);
  assert(width > 0 && width <= 144 && height > 0 && height <= 168);
  int n = rbuffer_getc(&rb);
  int format = rbuffer_getc(&rb);
  if (format != 0) {
    app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "cannot support format %d", format);
    return bwd_create(NULL);
  }

  /*uint8_t vo_lo = */rbuffer_getc(&rb);
  /*uint8_t vo_hi = */rbuffer_getc(&rb);
  /*uint8_t po_lo = */rbuffer_getc(&rb);
  /*uint8_t po_hi = */rbuffer_getc(&rb);
  
  int do_unscreen = (n & 0x80);
  n = n & 0x7f;

  app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "reading bitmap %d x %d, n = %d, format = %d", width, height, n, format);
  
  GBitmap *image = __gbitmap_create_blank(GSize(width, height));
  if (image == NULL) {
    return bwd_create(NULL);
  }
  int stride = gbitmap_get_bytes_per_row(image);
  uint8_t *bitmap_data = gbitmap_get_data(image);
  assert(bitmap_data != NULL);
  size_t data_size = height * stride;
  //app_log(APP_LOG_LEVEL_INFO, __FILE__, __LINE__, "stride = %d, data_size = %d", stride, data_size);

  Rl2Unpacker rl2;
  rl2unpacker_init(&rl2, &rb, n, true);

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
  rbuffer_deinit(&rb);
  
  if (do_unscreen) {
    unscreen_bitmap(image);
  }
  
  return bwd_create(image);
}

#endif // PBL_PLATFORM_APLITE

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
