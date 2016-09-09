#! /usr/bin/env python

import PIL.Image, PIL.ImageOps
import sys
import os
import shutil

help = """
make_rle.py

Converts an image from a standard format (for instance, a png) into an
.rle file for loading pre-compressed into a Pebble watch app.

make_rle.py [opts]

Options:

   -t
      Generate png-trans type entries (white/black pairs) for transparent PNG's.

   -p [aplite|basalt|auto]
      Specify the explicit platform type to generate.  The default is
      "auto", which guesses based on the filename.

"""

# RLE header (NB: All fields are little-endian)
#         (uint8_t)  width
#         (uint8_t)  height
#         (uint8_t)  n (number of chunks of pixels to take at a time; unscreen if 0x80 set)
#         (uint8_t)  format (see below)
#         (uint16_t) offset to end of rle data (and start of values data if present)
#         (uint16_t) offset to end of values data (and start of palette data if present)

RLEHeaderSize = 8

# Format codes (almost matches pebble.h):
GBitmapFormat1Bit        = 0
GBitmapFormat8Bit        = 1
GBitmapFormat1BitPalette = 2
GBitmapFormat2BitPalette = 3
GBitmapFormat4BitPalette = 4


thresholdMask = [0] + [255] * 255
threshold2Bit = [0] * 64 + [85] * 64 + [170] * 64 + [255] * 64


def usage(code, msg = ''):
    print >> sys.stderr, help
    print >> sys.stderr, msg
    sys.exit(code)

def unscreen(image):
    """ Returns a new copy of the indicated image xored with a 1x1
    checkerboard pattern.  The idea is to eliminate this kind of noise
    from the source image if it happens to be present. """

    image = image.copy()
    w, h = image.size
    for y in range(h):
        for x in range(w):
            if ((x ^ y) & 1):
                image.putpixel((x, y), 255 - image.getpixel((x, y)))
    return image

def generate_pixels_1bit(image, stride):
    """ This generator yields a sequence of 0/255 values for the 1-bit
    pixels of the image.  We extend the row to stride * 8 pixels. """

    w, h = image.size
    for y in range(h):
        for x in range(w):
            value = image.getpixel((x, y))
            yield value
        for x in range(w, stride * 8):
            # Pad out the row with zeroes.
            yield 0

    raise StopIteration

def pack_argb8(pixel):
    """ Given an (r, g, b, a) tuple returned by PIL, return the Basalt
    packed ARGB8 equivalent. """

    r, g, b, a = pixel
    if a == 0:
        value = 0
    else:
        value = (a & 0xc0) | ((r & 0xc0) >> 2) | ((g & 0xc0) >> 4) | ((b & 0xc0) >> 6)
    return value

def unpack_argb8(value):
    """ Given a packed ARGB8 value, return the (r, g, b, a) value for
    PIL. """
    a = ((value >> 6) & 0x03) * 0x55
    r = ((value >> 4) & 0x03) * 0x55
    g = ((value >> 2) & 0x03) * 0x55
    b = ((value) & 0x03) * 0x55
    return (r, g, b, a)

def generate_pixels_8bit(image):
    """ This generator yields a sequence of 0..255 values for the
    8-bit pixels of the image. """

    w, h = image.size
    for y in range(h):
        for x in range(w):
            value = pack_argb8(image.getpixel((x, y)))
            yield value

    raise StopIteration

def generate_pixels_palette(image, palette):
    """ This generator yields a sequence of 0..n values for the pixels
    of the image, indexing into the palette. """

    w, h = image.size
    for y in range(h):
        for x in range(w):
            pixel = image.getpixel((x, y))
            value = palette.index(pixel)
            assert value != -1
            yield value

    raise StopIteration

def generate_rle_1bit(source):
    """ This generator yields a sequence of run lengths of a binary
    (B&W) input--the input is either 0 or nonzero, so the rle is a
    simple sequence of positive numbers representing alternate values,
    and explicit values are not necessary. """

    current = 0
    count = 0
    # We start with an implicit black pixel, which isn't actually part
    # of the image.  The decoder must discard this pixel.  This
    # implicit black pixel ensures that there are no 0 counts anywhere
    # in the resulting data.
    next = 0
    while True:
        while current == next:
            count += 1
            try:
                next = source.next()
            except StopIteration:
                yield count
                raise StopIteration
        yield count
        current = next
        count = 0

def generate_rle_pairs(source):
    """ This generator yields a sequence of run lengths of a color
    input--the input is a sequence of numeric values, so the rle is a
    sequence of (value, count) pairs. """

    current = source.next()
    count = 0
    next = current
    while True:
        while current == next:
            count += 1
            try:
                next = source.next()
            except StopIteration:
                yield (current, count)
                raise StopIteration
        yield (current, count)
        current = next
        count = 0

def count_bits(num):
    count = 0
    while num >= (1 << count):
        count += 1
    return count

def chop_rle(source, n):
    """ Separates the rle lengths sequence into a sequence of n-bit
    chunks.  If a value is too large to fit into a single chunk, a
    series of 0-valued chunks will introduce it. """

    result = ''
    for v in source:
        # Count the minimum number of chunks we need to represent v.
        numChunks = (count_bits(v) + n - 1) / n

        # We write out a number of zeroes to indicate this.
        zeroCount = numChunks - 1
        for z in range(zeroCount):
            yield 0

        mask = (1 << n) - 1
        for x in range(numChunks):
            b = (v >> ((numChunks - x - 1) * n)) & mask
            assert b != 0 or x != 0
            yield b

def pack_rle(source, n):
    """ Packs a sequence of n-bit chunks into a byte string. """
    seq = list(source)
    result = ''
    if n == 1:
        seq += [0, 0, 0, 0, 0, 0, 0]
        for i in range(0, len(seq) - 7, 8):
            v = (seq[i + 0] << 7) | (seq[i + 1] << 6) | (seq[i + 2] << 5) | (seq[i +3] << 4) | (seq[i + 4] << 3) | (seq[i + 5] << 2) | (seq[i + 6] << 1) | (seq[i + 7])
            result += (chr(v))
    elif n == 2:
        seq += [0, 0, 0]
        for i in range(0, len(seq) - 3, 4):
            v = (seq[i + 0] << 6) | (seq[i + 1] << 4) | (seq[i + 2] << 2) | (seq[i + 3])
            result += (chr(v))
    elif n == 4:
        seq += [0]
        for i in range(0, len(seq) - 1, 2):
            v = (seq[i + 0] << 4) | (seq[i + 1])
            result += (chr(v))
    elif n == 8:
        for v in seq:
            result += chr(v)
    else:
        raise ValueError

    return result

class Rl2Unpacker:
    """ This class reverses chop_rle() and pack_rle()--it reads a
    string and returns the original rle sequence of positive integers.
    It's written using a class and a call interface instead of as a
    generator, so it can serve as a prototype for the C code to do the
    same thing. """

    def __init__(self, str, n, zero_expands = True):
        # assumption: n is an integer divisor of 8.
        assert n * (8 / n) == 8

        self.str = str
        self.n = n
        self.si = 0
        self.bi = 8

        # If this is true, it means a zero value indicates a larger
        # component follows.  If false, it means a zero value means
        # zero.
        self.zero_expands = zero_expands

    def getList(self):
        result = []
        v = self.getNextValue()
        while v >= 0:
            result.append(v)
            v = self.getNextValue()
        return result

    def getNextValue(self):
        """ Returns the next value in the sequence.  Returns -1 at the
        end of the sequence. """

        if self.si >= len(self.str):
            return -1

        # First, count the number of zero chunks until we come to a nonzero chunk.
        zeroCount = 0
        b = ord(self.str[self.si])
        if self.zero_expands:
            bmask = (1 << self.n) - 1
            bv = b & (bmask << (self.bi - self.n))
            while bv == 0:
                zeroCount += 1
                self.bi -= self.n
                if self.bi <= 0:
                    self.si += 1
                    self.bi = 8
                    if self.si >= len(self.str):
                        return -1

                    b = ord(self.str[self.si])
                bv = b & (bmask << (self.bi - self.n))

        # Infer from that the number of chunks, and hence the number
        # of bits, that make up the value we will extract.
        numChunks = (zeroCount + 1)
        bitCount = numChunks * self.n

        # OK, now we need to extract the next bitCount bits into a word.
        result = 0
        while bitCount >= self.bi:
            mask = (1 << self.bi) - 1
            value = (b & mask)
            result = (result << self.bi) | value
            bitCount -= self.bi

            self.si += 1
            self.bi = 8
            if self.si >= len(self.str):
                b = 0
                break

            b = ord(self.str[self.si])

        if bitCount > 0:
            # A partial word in the middle of the byte.
            bottomCount = self.bi - bitCount
            assert bottomCount > 0
            mask = ((1 << bitCount) - 1)
            value = ((b >> bottomCount) & mask)
            result = (result << bitCount) | value
            self.bi -= bitCount

        return result

def make_rle_image_1bit(rleFilename, image):
    image = image.convert('1')
    w, h = image.size
    stride = ((w + 31) / 32) * 4
    fullSize = h * stride
    pixels_per_byte = 8

    ## if w % 8 != 0:
    ##     # Must be a multiple of 8 pixels wide.  If not, expand it.
    ##     w = ((w + 7) / 8) * 8
    w_orig = w
    if w != stride * pixels_per_byte:
        # Must be stride bytes wide.  If not, expand it.
        w = stride * pixels_per_byte
        im2 = PIL.Image.new('1', (w, h), 0)
        im2.paste(image, (0, 0))
        image = im2

    assert w <= 0xff and h <= 0xff
    unscreened = unscreen(image)

    # The number of bytes in a row.  Must be a multiple of 4, per
    # Pebble conventions.
    stride = ((w + 31) / 32) * 4
    assert stride <= 0xff

    rle_normal = list(generate_rle_1bit(generate_pixels_1bit(image, stride)))
    rle_unscreened = list(generate_rle_1bit(generate_pixels_1bit(unscreened, stride)))

    # Find the best n for this image.
    result = None
    n = None
    for n0 in [1, 0x81, 2, 4, 8]:
        rle = rle_normal
        if n0 & 0x80:
            rle = rle_unscreened
        result0 = pack_rle(chop_rle(rle, n0 & 0x7f), n0 & 0x7f)
        #print n0, len(result0)
        if result is None or len(result0) < len(result):
            result = result0
            n = n0

    vo = RLEHeaderSize + len(result)
    assert(vo < 0x10000)
    vo_lo = vo & 0xff
    vo_hi = (vo >> 8) & 0xff

    # Verify the result matches.
    unpacker = Rl2Unpacker(result, n & 0x7f, zero_expands = True)
    verify = unpacker.getList()
    if n & 0x80:
        assert verify == rle_unscreened
        pixels = list(generate_pixels_1bit(unscreened, stride))
    else:
        assert verify == rle_normal
        pixels = list(generate_pixels_1bit(image, stride))

    format = GBitmapFormat1Bit

    #print "n = %s, format = %s, vo = %s, po = %s" % (n, format, vo, vo)

    rle = open(rleFilename, 'wb')
    rle.write('%c%c%c%c%c%c%c%c' % (w_orig, h, n, format, vo_lo, vo_hi, vo_lo, vo_hi))
    rle.write(result)
    rle.close()

    print '%s: %s, %s vs. %s' % (rleFilename, format, 8 + len(result), fullSize)

def make_rle_image_basalt(rleFilename, image):
    image = image.convert('RGBA')
    w, h = image.size

    r, g, b, a = image.split()

    # Ensure that the RGB image is black anywhere the alpha
    # is black.
    black = PIL.Image.new('L', image.size, 0)
    mask = a.point(thresholdMask)
    r = PIL.Image.composite(r, black, mask)
    g = PIL.Image.composite(g, black, mask)
    b = PIL.Image.composite(b, black, mask)

    # Ensure the image is reduced to Basalt's 64 colors.
    r = r.point(threshold2Bit)
    g = g.point(threshold2Bit)
    b = b.point(threshold2Bit)
    a = a.point(threshold2Bit)

    image = PIL.Image.merge('RGBA', [r, g, b, a])

    # Check the number of unique colors in the image to determine the
    # precise image type.
    colors = image.getcolors(16)

    if colors is None:
        # We have a full-color image.
        palette = None
        format = GBitmapFormat8Bit
        vn = 8
    else:
        # We have a palettized image.
        palette = zip(*colors)[1]
        if len(palette) <= 2:
            pixel0 = pack_argb8(palette[0])
            pixel1 = pack_argb8(palette[-1])
            if pixel0 in [0xc0, 0xff] and pixel1 in [0xc0, 0xff]:
                # This is a special case: it's really a 1-bit B&W image.
                return make_rle_image_1bit(rleFilename, image)
            # It's a 1-bit image with two specific colors.
            format = GBitmapFormat1BitPalette
            vn = 1
        elif len(palette) <= 4:
            format = GBitmapFormat2BitPalette
            vn = 2
        else:
            format = GBitmapFormat4BitPalette
            vn = 4

    pixels_per_byte = 8 / vn
    stride = (w + pixels_per_byte - 1) / pixels_per_byte

    # Apparently Basalt does not word-align the rows for these advanced format types.
    #stride = ((stride + 3) / 4) * 4

    fullSize = h * stride

    w_orig = w
    if w != stride * pixels_per_byte:
        # Must be stride bytes wide.  If not, expand it.
        w = stride * pixels_per_byte
        im2 = PIL.Image.new(image.mode, (w, h), 0)
        im2.paste(image, (0, 0))
        image = im2

    if palette is None:
        # Full-color image, no palette.
        pixels = generate_pixels_8bit(image)
    else:
        # Index into a palette.
        pixels = generate_pixels_palette(image, palette)

    if vn == 1:
        # With a 1-bit image, no need to record a values list.
        values = []
        rle = generate_rle_1bit(pixels)
    else:
        # With an n-bit image, the values list can't be inferred and
        # must be explicitly stored.
        values_rle = generate_rle_pairs(pixels)
        values, rle = zip(*list(values_rle))

    rle = list(rle)

    # Find the best n for this image.
    result = None
    n = None
    for n0 in [1, 2, 4, 8]:
        im = image
        result0 = pack_rle(chop_rle(rle, n0 & 0x7f), n0 & 0x7f)
        #print n0, len(result0)
        if result is None or len(result0) < len(result):
            result = result0
            n = n0

    # Verify the result matches.
    unpacker = Rl2Unpacker(result, n & 0x7f, zero_expands = True)
    verify = unpacker.getList()
    assert verify == rle

    # Get the offset into the file at which the values start.
    vo = RLEHeaderSize + len(result)
    assert(vo < 0x10000)
    vo_lo = vo & 0xff
    vo_hi = (vo >> 8) & 0xff

    values_result = pack_rle(values, vn)

    # Also get the offset into the file at which the palette starts.
    po = vo + len(values_result)
    assert(po < 0x10000)
    po_lo = po & 0xff
    po_hi = (po >> 8) & 0xff

    #print "n = %s, format = %s, vo = %s, po = %s" % (n, format, vo, po)

    rle = open(rleFilename, 'wb')
    rle.write('%c%c%c%c%c%c%c%c' % (w_orig, h, n, format, vo_lo, vo_hi, po_lo, po_hi))
    rle.write(result)
    assert rle.tell() == vo
    rle.write(values_result)
    if palette is not None:
        assert rle.tell() == po
        for pixel in palette:
            rle.write(chr(pack_argb8(pixel)))

    rle.close()

    print '%s: %s, %s vs. %s' % (rleFilename, format, 8 + len(result) + len(values), fullSize)

def make_rle_image(rleFilename, image, platform = 'auto'):
    if platform == 'auto' and rleFilename.find('~bw') != -1:
        platform = 'aplite'

    if platform == 'aplite':
        return make_rle_image_1bit(rleFilename, image)
    else:
        return make_rle_image_basalt(rleFilename, image)

def get_variants_for_platforms(platforms):
    variants = set()
    if "aplite" in platforms or "diorite" in platforms:
        variants.add("~bw")
    if "basalt" in platforms:
        variants.add("~color")
        variants.add("~color~rect")
    if "chalk" in platforms:
        variants.add("~color")
        variants.add("~color~round")
    return list(variants)

rawResourceEntry = """
    {
      "name": "%(name)s",
      "file": "%(file)s",
      "type": "raw",
      "targetPlatforms": [ %(targetPlatforms)s ]
    },"""

bwResourceEntry = """
    {
      "name": "%(name)s",
      "file": "%(file)s",
      "type": "bitmap",
      "memoryFormat" : "1Bit",
      "spaceOptimization" : "memory",
      "targetPlatforms": [ %(targetPlatforms)s ]
    },"""

colorResourceEntry = """
    {
      "name": "%(name)s",
      "file": "%(file)s",
      "type": "bitmap",
      "targetPlatforms": [ %(targetPlatforms)s ]
    },"""

def format_platforms(platforms):
    return ', '.join(map(lambda platform: '"%s"' % (platform), list(platforms)))

def make_rle(filename, name = None, prefix = 'resources/', useRle = True, platforms = None):
    resourceStr = ''

    for platform in platforms:
        basename, ext = os.path.splitext(filename)
        for variant in get_variants_for_platforms([platform]) + ['']:
            # Handle the specialized variant files.
            if os.path.exists(prefix + basename + variant + ext):
                resourceStr += make_rle_file(basename, variant, ext, name = name, prefix = prefix, useRle = useRle, platform = platform)
                break

    return resourceStr

def make_rle_file(basename, variant, ext, name = None, prefix = 'resources/', useRle = True, platform = None):
    filename = basename + variant + ext
    targetFilename = basename + variant
    needsCopy = False

    resourceStr = ''
    dirname, basename = os.path.split(basename)
    if dirname != 'build' or not basename.endswith(platform):
        # Copy it to the build directory.
        targetFilename = 'build/%s_%s' % (basename, platform)
        needsCopy = True

    if useRle:
        print filename, targetFilename + '.rle'
        image = PIL.Image.open(prefix + filename)
        make_rle_image(prefix + targetFilename + '.rle', image)

        resourceStr += rawResourceEntry % {
            'name' : name,
            'file' : targetFilename + '.rle',
            'targetPlatforms' : format_platforms([platform]),
            }

    else:
        print filename, targetFilename + '.png'
        if needsCopy:
            image = PIL.Image.open(prefix + filename)
            image.save(prefix + targetFilename + '.png')

        if platform in ['aplite', 'diorite']:
            resourceStr += bwResourceEntry % {
                'name' : name,
                'file' : targetFilename + '.png',
                'targetPlatforms' : format_platforms([platform])
                }
        else:
            resourceStr += colorResourceEntry % {
                'name' : name,
                'file' : targetFilename + '.png',
                'targetPlatforms' : format_platforms([platform])
                }

    return resourceStr

def make_rle_trans(filename, name = None, prefix = 'resources/', useRle = True, platforms = None):
    resourceStr = ''

    for platform in platforms:
        basename, ext = os.path.splitext(filename)
        for variant in get_variants_for_platforms([platform]) + ['']:
            # Handle the specialized variant files.
            if os.path.exists(prefix + basename + variant + ext):
                resourceStr += make_rle_trans_file(basename, variant, ext, name = name, prefix = prefix, useRle = useRle, platform = platform)
                break

    return resourceStr


def make_rle_trans_file(basename, variant, ext, name = None, prefix = 'resources/', useRle = True, platform = None):
    filename = basename + variant + ext
    if platform not in ['aplite', 'diorite']:
        # In the color case, this is almost the same as make_rle_file(),
        # but we append '_WHITE' to the name (and we don't create a
        # _BLACK version).
        return make_rle_file(basename, variant, ext, name = name + '_WHITE', prefix = prefix, useRle = useRle, platform = platform)

    # In the bw case, we split the image into white and black versions.
    resourceStr = ''
    image = PIL.Image.open(prefix + filename)
    bits, alpha = image.convert('LA').split()
    bits = bits.convert('1')
    alpha = alpha.convert('1')

    zero = PIL.Image.new('1', image.size, 0)
    one = PIL.Image.new('1', image.size, 1)

    black = PIL.Image.composite(zero, one, bits)
    black = PIL.Image.composite(black, zero, alpha)
    white = PIL.Image.composite(one, zero, bits)
    white = PIL.Image.composite(white, zero, alpha)

    whiteFilename = '%s_white_%s' % (basename, platform)
    blackFilename = '%s_black_%s' % (basename, platform)

    if useRle:
        make_rle_image(prefix + whiteFilename + '.rle', white)
        make_rle_image(prefix + blackFilename + '.rle', black)

        resourceStr += rawResourceEntry % {
            'name' : name + '_WHITE',
            'file' : whiteFilename + '.rle',
            'targetPlatforms' : format_platforms([platform]),
            }
        resourceStr += rawResourceEntry % {
            'name' : name + '_BLACK',
            'file' : blackFilename + '.rle',
            'targetPlatforms' : format_platforms([platform]),
            }
    else:
        white.save(prefix + whiteFilename + '.png')
        black.save(prefix + blackFilename + '.png')

        resourceStr += bwResourceEntry % {
            'name' : name + '_WHITE',
            'file' : whiteFilename + '.png',
            'targetPlatforms' : format_platforms([platform]),
            }
        resourceStr += bwResourceEntry % {
            'name' : name + '_BLACK',
            'file' : blackFilename + '.png',
            'targetPlatforms' : format_platforms([platform]),
            }

    return resourceStr

def unpack_rle_file(rleFilename):
    rb = open(rleFilename, 'rb')
    width = ord(rb.read(1))
    height = ord(rb.read(1))
    n = ord(rb.read(1))
    format = ord(rb.read(1))
    vo_lo = ord(rb.read(1))
    vo_hi = ord(rb.read(1))
    vo = (vo_hi << 8) | vo_lo
    po_lo = ord(rb.read(1))
    po_hi = ord(rb.read(1))
    po = (po_hi << 8) | po_lo

    do_unscreen = ((n & 0x80) != 0)
    n = n & 0x7f

    #print "n = %s, format = %s, vo = %s, po = %s" % (n, format, vo, po)

    if (format == GBitmapFormat1Bit or format == GBitmapFormat1BitPalette):
        pixels_per_byte = 8
        vn = 1
    elif format == GBitmapFormat2BitPalette:
        pixels_per_byte = 4
        vn = 2
    elif format == GBitmapFormat4BitPalette:
        pixels_per_byte = 2
        vn = 4
    elif format == GBitmapFormat8Bit:
        pixels_per_byte = 1
        vn = 8
    else:
        assert False

    stride = (width + pixels_per_byte - 1) / pixels_per_byte

    if format == GBitmapFormat1Bit:
        stride = ((stride + 3) / 4) * 4

    # Expand the width as needed to include the extra padding pixels.
    width2 = (stride * pixels_per_byte)

    assert(RLEHeaderSize == rb.tell())

    rle_data = rb.read(vo - RLEHeaderSize)
    assert(vo == rb.tell())

    values_data = rb.read(po - vo)
    assert(po == rb.tell())
    palette = map(ord, rb.read())

    # Unpack values_data into the list of values.
    unpacker = Rl2Unpacker(values_data, vn, zero_expands = False)
    values = unpacker.getList()

    # Unpack rle_data into the list of RLE lengths.
    unpacker = Rl2Unpacker(rle_data, n, zero_expands = True)
    rle = unpacker.getList()

    #print "rle = %s, values = %s, palette = %s" % (len(rle), len(values), len(palette))

    if vn == 1:
        # Unpack a 1-bit file.
        pixels = []

        # The initial value is 0.
        value = 0

        for count in rle:
            pixels += [value] * count
            value = 1 - value

        # We discard the first, implicit black pixel; it's not part of the image
        pixels = pixels[1:]

    else:
        # Unpack some other depth file.
        pixels = []
        vi = 0
        for count in rle:
            value = values[vi]
            vi += 1
            pixels += [value] * count

    assert len(pixels) == width2 * height

    if format == GBitmapFormat1Bit:
        image = PIL.Image.new('1', (width2, height), 0)
        palette = [0, 255]
    else:
        image = PIL.Image.new('RGBA', (width2, height), 0)
        if palette:
            palette = map(unpack_argb8, palette)

    if palette:
        # Apply the palette.
        for pi in range(len(pixels)):
            pixels[pi] = palette[pixels[pi]]

    elif format == GBitmapFormat8Bit:
        # Expand the packed ARGB8 format.
        for pi in range(len(pixels)):
            pixels[pi] = unpack_argb8(pixels[pi])

    pi = 0
    for yi in range(height):
        for xi in range(width2):
            assert pi < len(pixels)
            image.putpixel((xi, yi), pixels[pi])
            pi += 1

    assert pi == len(pixels)

    # Re-crop the image to its intended width.
    if width2 != width:
        image = image.crop((0, 0, width, height))

    return image


def unpack_rle(filename, prefix = 'resources/'):
    basename = os.path.splitext(filename)[0]
    pngFilename = basename + '_unpacked.png'
    image = unpack_rle_file(prefix + filename)

    print pngFilename
    image.save(pngFilename)


if __name__ == '__main__':
    # Main.
    import getopt

    try:
        opts, args = getopt.getopt(sys.argv[1:], 'tp:uh')
    except getopt.error, msg:
        usage(1, msg)

    makeTrans = False
    platform = 'auto'
    doUnpack = False
    for opt, arg in opts:
        if opt == '-t':
            makeTrans = True
        elif opt == '-p':
            platform = arg
        elif opt == '-u':
            doUnpack = True
        elif opt == '-h':
            usage(0)

    print args
    for filename in args:
        if doUnpack:
            unpack_rle(filename, prefix = '')
        elif makeTrans:
            make_rle_trans(filename, prefix = '')
        else:
            make_rle(filename, prefix = '', platform = platform)
