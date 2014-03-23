#include "bwd.h"
#include "assert.h"
#include "../resources/generated_config.h"
//#define SUPPORT_RLE

// From bitmapgen.py:
/*
# Bitmap struct (NB: All fields are little-endian)
#         (uint16_t) row_size_bytes
#         (uint16_t) info_flags
#                         bit 0 : reserved (must be zero for bitmap files)
#                    bits 12-15 : file version
#         (int16_t)  bounds.origin.x
#         (int16_t)  bounds.origin.y
#         (int16_t)  bounds.size.w
#         (int16_t)  bounds.size.h
#         (uint32_t) image data (word-aligned, 0-padded rows of bits)
*/
typedef struct {
  uint16_t row_size_bytes;
  uint16_t info_flags;
  int16_t origin_x;
  int16_t origin_y;
  int16_t size_w;
  int16_t size_h;
} BitmapDataHeader;

BitmapWithData bwd_create(GBitmap *bitmap, void *data) {
  BitmapWithData bwd;
  bwd.bitmap = bitmap;
  bwd.data = data;
  return bwd;
}

void bwd_destroy(BitmapWithData *bwd) {
  if (bwd->bitmap != NULL) {
    gbitmap_destroy(bwd->bitmap);
    bwd->bitmap = NULL;
  }
  if (bwd->data != NULL) {
    free(bwd->data);
    bwd->data = NULL;
  }
}

// Initialize a bitmap from a regular unencoded resource (i.e. as
// loaded from a png file).  This is the same as
// gbitmap_create_with_resource(), but wrapped within the
// BitmapWithData interface to be consistent with rle_bwd_create().
// The returned bitmap must be released with bwd_destroy().
BitmapWithData png_bwd_create(int resource_id) {
  GBitmap *image = gbitmap_create_with_resource(resource_id);
  return bwd_create(image, NULL);
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
  uint8_t _buffer[RBUFFER_SIZE];
} RBuffer;

// Begins reading from a raw resource.  Should be matched by a later
// call to rbuffer_deinit() to free this stuff.
static void rbuffer_init(int resource_id, RBuffer *rb) {
  //  rb->_buffer = (uint8_t *)malloc(RBUFFER_SIZE);
  //  assert(rb->_buffer != NULL);
  
  rb->_rh = resource_get_handle(resource_id);
  rb->_i = 0;
  rb->_filled_size = resource_load_byte_range(rb->_rh, 0, rb->_buffer, RBUFFER_SIZE);
  rb->_bytes_read = rb->_filled_size;
}

// Gets the next byte from the rbuffer.  Returns EOF at end.
static int rbuffer_getc(RBuffer *rb) {
  if (rb->_i >= RBUFFER_SIZE) {
    rb->_filled_size = resource_load_byte_range(rb->_rh, rb->_bytes_read, rb->_buffer, RBUFFER_SIZE);
    rb->_bytes_read += rb->_filled_size;
    rb->_i = 0;
  }
  if (rb->_i >= rb->_filled_size) {
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
} Rl2Unpacker;

static void rl2unpacker_init(Rl2Unpacker *rl2, RBuffer *rb, int n) {
  // assumption: n is an integer divisor of 8.
  assert(n * (8 / n) == 8);

  rl2->rb = rb;
  rl2->n = n;
  rl2->b = rbuffer_getc(rb);
  rl2->bi = 8;
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
  int height = image->bounds.size.h;
  int width = image->bounds.size.w;
  int width_bytes = width / 8;
  int stride = image->row_size_bytes; // multiple of 4, by Pebble convention.
  uint8_t *data = image->addr;

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

// Initialize a bitmap from an rle-encoded resource.  The returned
// bitmap must be released with bwd_destroy().  See make_rle.py for
// the program that generates these rle sequences.
BitmapWithData
rle_bwd_create(int resource_id) {
  RBuffer rb;
  rbuffer_init(resource_id, &rb);
  int width = rbuffer_getc(&rb);
  int height = rbuffer_getc(&rb);
  int stride = rbuffer_getc(&rb);
  int n = rbuffer_getc(&rb);
  int do_unscreen = (n & 0x80);
  n = n & 0x7f;

  Rl2Unpacker rl2;
  rl2unpacker_init(&rl2, &rb, n);

  size_t data_size = height * stride;
  size_t total_size = sizeof(BitmapDataHeader) + data_size;
  uint8_t *bitmap = (uint8_t *)malloc(total_size);
  assert(bitmap != NULL);
  memset(bitmap, 0, total_size);
  BitmapDataHeader *bitmap_header = (BitmapDataHeader *)bitmap;
  uint8_t *bitmap_data = bitmap + sizeof(BitmapDataHeader);
  bitmap_header->row_size_bytes = stride;
  bitmap_header->size_w = width;
  bitmap_header->size_h = height;

  // The initial value is 0.
  uint8_t *dp = bitmap_data;
  uint8_t *dp_stop = dp + stride * height;
  int value = 0;
  int b = 0;
  int count = rl2unpacker_getc(&rl2);
  if (count != EOF) {
    assert(count > 0);
    // We discard the first, implicit black pixel; it's not part of the image.
    --count;
  }
  while (count != EOF) {
    assert(dp < dp_stop);
    if (value) {
      // Generate count 1-bits.
      int b1 = b + count;
      if (b1 < 8) {
        // We're still within the same byte.
        int mask = ~((1 << (b)) - 1);
        mask &= ((1 << (b1)) - 1);
        *dp |= mask;
        b = b1;
      } else {
        // We've crossed over a byte boundary.
        *dp |= ~((1 << (b)) - 1);
        ++dp;
        b += 8;
        while (b1 / 8 != b / 8) {
          assert(dp < dp_stop);
          *dp = 0xff;
          ++dp;
          b += 8;
        }
        b1 = b1 % 8;
        assert(dp < dp_stop);
        *dp |= ((1 << (b1)) - 1);
        b = b1;
      }
    } else {
      // Skip over count 0-bits.
      b += count;
      dp += b / 8;
      b = b % 8;
    }
    value = 1 - value;
    count = rl2unpacker_getc(&rl2);
  }
  rbuffer_deinit(&rb);

  GBitmap *image = gbitmap_create_with_data(bitmap);
  if (do_unscreen) {
    unscreen_bitmap(image);
  }
  return bwd_create(image, bitmap);
}
#endif  // SUPPORT_RLE
