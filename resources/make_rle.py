#! /usr/bin/env python

import PIL.Image
import sys
import os

help = """
make_rle.py

Converts an image from a standard format (for instance, a png) into an
.rle file for loading pre-compressed into a Pebble watch app.

make_rle.py [opts]

Options:

   -t Generate png-trans type entries (white/black pairs) for transparent PNG's.
        
"""

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

def generate_pixels_b(image, stride):
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

def generate_pixels_c(image):
    """ This generator yields a sequence of 0..255 values for the
    8-bit pixels of the image. """

    w, h = image.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = image.getpixel((x, y))
            if a == 0:
                value = 0
            else:
                value = (a & 0xc0) | ((r & 0xc0) >> 2) | ((g & 0xc0) >> 4) | ((b & 0xc0) >> 6)
            yield value

    raise StopIteration

def generate_rle_b(source):
    """ This generator yields a sequence of run lengths of a binary
    (B&W) input--the input is either 0 or 255, so the rle is a simple
    sequence of positive numbers representing alternate values, and
    explicit values are not necessary. """

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
            next = source.next()
        yield count
        current = next
        count = 0

def generate_rle_c(source):
    """ This generator yields a sequence of run lengths of a color
    input--the input is a sequence of 0..255 values, so the rle is a
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

    def __init__(self, str, n):
        # assumption: n is an integer divisor of 8.
        assert n * (8 / n) == 8
          
        self.str = str
        self.n = n
        self.si = 0
        self.bi = 8

    def getList(self):
        result = []
        v = self.getNextValue()
        while v != 0:
            result.append(v)
            v = self.getNextValue()
        return result

    def getNextValue(self):
        """ Returns the next value in the sequence.  Returns 0 at the
        end of the sequence. """

        if self.si >= len(self.str):
            return 0
        
        # First, count the number of zero chunks until we come to a nonzero chunk.
        zeroCount = 0
        b = ord(self.str[self.si])
        bmask = (1 << self.n) - 1
        bv = b & (bmask << (self.bi - self.n))
        while bv == 0:
            zeroCount += 1
            self.bi -= self.n
            if self.bi <= 0:
                self.si += 1
                self.bi = 8
                if self.si >= len(self.str):
                    return 0
                
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
            
def make_rle_b_image(rleFilename, image):
    image = image.convert('1')
    w, h = image.size
    stride = ((w + 31) / 32) * 4
    fullSize = h * stride
    
    if w % 8 != 0:
        # Must be a multiple of 8 pixels wide.  If not, expand it.
        w = ((w + 7) / 8) * 8
        im2 = PIL.Image.new('1', (w, h), 0)
        im2.paste(image, (0, 0))
        image = im2

    assert w <= 0xff and h <= 0xff
    unscreened = unscreen(image)

    # The number of bytes in a row.  Must be a multiple of 4, per
    # Pebble conventions.
    stride = ((w + 31) / 32) * 4
    assert stride <= 0xff

    rle_normal = list(generate_rle_b(generate_pixels_b(image, stride)))
    rle_unscreened = list(generate_rle_b(generate_pixels_b(unscreened, stride)))

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

    # Verify the result matches.
    unpacker = Rl2Unpacker(result, n & 0x7f)
    verify = unpacker.getList()
    if n & 0x80:
        assert verify == rle_unscreened
    else:
        assert verify == rle_normal

    # The third byte is the non-zero stride to indicate a 1-bit B&W file.
    rle = open(rleFilename, 'wb')
    rle.write('%c%c%c%c' % (w, h, stride, n))
    rle.write(result)
    rle.close()
    
    print '%s: %s vs. %s' % (rleFilename, 4 + len(result), fullSize)

def make_rle_c_image(rleFilename, image):
    image = image.convert('RGBA')
    w, h = image.size
    fullSize = w * h

    values_rle = generate_rle_c(generate_pixels_c(image))
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
    unpacker = Rl2Unpacker(result, n & 0x7f)
    verify = unpacker.getList()
    assert verify == rle

    # Get the offset into the file at which the values start.
    vo = 6 + len(result)
    assert(vo < 0x10000)
    vo_lo = vo & 0xff
    vo_hi = (vo >> 8) & 0xff

    # The third byte is 0 to indicate an 8-bit ARGB file.
    rle = open(rleFilename, 'wb')
    rle.write('%c%c%c%c%c%c' % (w, h, 0, n, vo_lo, vo_hi))
    rle.write(result)
    rle.write(pack_rle(values, 8))
    rle.close()
    
    print '%s: %s vs. %s' % (rleFilename, 6 + len(result) + len(values), fullSize)
            
def make_rle_image(rleFilename, image):
    if rleFilename.find('~color') != -1:
        return make_rle_c_image(rleFilename, image)
    else:
        return make_rle_b_image(rleFilename, image)

def make_rle(filename, prefix = 'resources/', useRle = True):
    if useRle:
        image = PIL.Image.open(prefix + filename)
        basename = os.path.splitext(filename)[0]
        rleFilename = basename + '.rle'
        make_rle_image(prefix + rleFilename, image)
        return rleFilename, 'raw'
    else:
        ptype = 'pbi'
        print filename
        return filename, ptype

def make_rle_trans(filename, prefix = 'resources/', useRle = True):
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
    
    if useRle:
        basename = os.path.splitext(filename)[0]
        rleWhiteFilename = basename + '_white.rle'
        make_rle_image(prefix + rleWhiteFilename, white)
        rleBlackFilename = basename + '_black.rle'
        make_rle_image(prefix + rleBlackFilename, black)
        
        return rleWhiteFilename, rleBlackFilename, 'raw'
    else:
        basename = os.path.splitext(filename)[0]
        rleWhiteFilename = basename + '_white.png'
        white.save(prefix + rleWhiteFilename)
        rleBlackFilename = basename + '_black.png'
        black.save(prefix + rleBlackFilename)
        print filename
        return rleWhiteFilename, rleBlackFilename, 'pbi'

def unpack_rle_file(rleFilename):
    rb = open(rleFilename, 'rb')
    width = ord(rb.read(1))
    height = ord(rb.read(1))
    stride = ord(rb.read(1))
    n = ord(rb.read(1))
    do_unscreen = ((n & 0x80) != 0)
    data_8bit = (stride == 0)
    if data_8bit:
        stride = width
    n = n & 0x7f

    if data_8bit:
        # Unpack an 8-bit ARGB file.

        # Get the offset into the file at which the values start.
        vo_lo = ord(rb.read(1))
        vo_hi = ord(rb.read(1))
        vo = (vo_hi << 8) | vo_lo

        print "vo = %x" % (vo)

        data = rb.read(vo - 6)
        values = map(ord, rb.read())

        unpacker = Rl2Unpacker(data, n)
        rle = unpacker.getList()

        pixels = []
        vi = 0
        for count in rle:
            value = values[vi]
            vi += 1
            a = ((value >> 6) & 0x03) * 0x55
            r = ((value >> 4) & 0x03) * 0x55
            g = ((value >> 2) & 0x03) * 0x55
            b = ((value) & 0x03) * 0x55
            value = (r, g, b, a)
            pixels += [value] * count

        image = PIL.Image.new('RGBA', (width, height), 0)
        
        pi = 0
        for yi in range(height):
            for xi in range(width):
                assert pi < len(pixels)
                image.putpixel((xi, yi), pixels[pi])
                pi += 1

        assert pi == len(pixels)
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
        opts, args = getopt.getopt(sys.argv[1:], 'tuh')
    except getopt.error, msg:
        usage(1, msg)

    makeTrans = False
    doUnpack = False
    for opt, arg in opts:
        if opt == '-t':
            makeTrans = True
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
            make_rle(filename, prefix = '')
            
