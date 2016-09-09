import PIL.Image
import PIL.ImageFont
import PIL.ImageDraw
import PIL.ImageChops
import math
import sys

def round(f):
    return int(f + 0.5)

threshold = 127
thresholdMap = [0] * (256 - threshold) + [255] * (threshold)

class FaceMaker:

    """ This class serves to collect several useful functions for
    generating clock (watch) faces for the Wright watch app.  Since
    clock faces tend to be rigidly geometric and repeating, and are
    often complex, I often find it easier to write a program to
    generate them rather than to paint them by hand.

    There is, of course, no requirement to generate clock faces with a
    program.  It's perfectly possible to paint them by hand as well,
    or some combination of auto-generated and hand-painted.

    This tool generates content into a large buffer then downsamples
    and filters it into the final results on the call to flush().

    """

    standardLabels = [(30, '1'), (60, '2'), (90, '3'), (120, '4'),
                      (150, '5'), (180, '6'), (210, '7'), (240, '8'),
                      (270, '9'), (300, '10'), (330, '11'), (0, '12')]
    romanLabels = [(30, 'I'), (60, 'II'), (90, 'III'), (120, 'IIII'),
                   (150, 'V'), (180, 'VI'), (210, 'VII'), (240, 'VIII'),
                   (270, 'IX'), (300, 'X'), (330, 'XI'), (0, 'XII')]

    defaultMode = '8bit'
    #defaultMode = '1bit'
    defaultFormat = { '1bit' : '1', '8bit' : 'L' }

    def __init__(self, filter = 4.0, zoom = 1.0, origin = (0.5, 0.5),
                 bg = 255, fg = 0, upscale = 1.0, format = None, mode = None, screenSize = (144, 168)):
        """ Sets up the buffers to generate a face.

        filter - the scale factor to enlarge the internal buffer.
        Larger numbers will increase the antialiasing effect.

        zoom - a scale to magnify the resulting face.  Larger numbers
        make the face larger than the specified parameters.  You can
        use this to experiment with different face sizes after you
        have laid out the face.

        origin - the (0, 0) point of the source coordinates, usually
        the center of the screen.

        bg - the background color.
        fg - the foreground color, default for drawing.

        upscale - generates an extra-big image for debugging clarity.
        format - specifies the output format, for debugging clarity.

        """

        if mode is None:
            mode = self.defaultMode
        self.mode = mode
        if format is None:
            format = self.defaultFormat[self.mode]

        self.filter = filter
        self.zoom = zoom
        self.origin = origin
        self.bg = bg
        self.fg = fg
        self.upscale = upscale
        self.format = format

        # Size of the target screen.
        self.targetSize = (round(screenSize[0] * self.upscale), round(screenSize[1] * self.upscale))
        self.maxTargetSize = max(self.targetSize)
        self.cropOrigin = ((self.maxTargetSize - self.targetSize[0]) / 2,
                           (self.maxTargetSize - self.targetSize[1]) / 2)

        # Size of the internal buffer.  Square.
        self.maxSize = round(self.maxTargetSize * self.filter)
        self.size = (self.maxSize, self.maxSize)

        self.buffer = PIL.Image.new('L', self.size, 0)
        self.draw = PIL.ImageDraw.Draw(self.buffer)

        self.target = PIL.Image.new(self.format, self.targetSize, self.bg)
        self.tdraw = PIL.ImageDraw.Draw(self.target)

        self.fullFg = PIL.Image.new(self.format, self.targetSize, self.fg)

    def setFg(self, fg):
        """ Changes the foreground color for future draw operations.
        This requires a flush. """

        self.flush()
        self.fg = fg

        self.fullFg = PIL.Image.new('L', self.targetSize, self.fg)
        self.fullFg = self.fullFg.convert(self.format)

        ## if fg == 128:
        ##     # Special case.
        ##     for yi in range(self.fullFg.size[1]):
        ##         for xi in range(self.fullFg.size[0]):
        ##             if (xi ^ yi) & 1:
        ##                 self.fullFg.putpixel((xi, yi), 255)
        ##             else:
        ##                 self.fullFg.putpixel((xi, yi), 0)

    def fill(self):
        """ Fills the entire face with the current foreground color.
        Requires a flush."""

        self.flush()
        self.target.paste(self.fullFg, (0, 0))

    def flush(self, dither = True):
        """ Copies the recently-drawn buffer to the target and clears
        the buffer for more drawing. """

        b = self.buffer.resize((self.maxTargetSize, self.maxTargetSize), PIL.Image.ANTIALIAS)
        b = b.crop((self.cropOrigin[0],
                    self.cropOrigin[1],
                    self.cropOrigin[0] + self.targetSize[0],
                    self.cropOrigin[1] + self.targetSize[1]))
        if not dither and self.format == '1':
            b = b.point(thresholdMap)
        b = b.convert(self.format)
        self.target.paste(self.fullFg, (0, 0), b)

        self.buffer = PIL.Image.new('L', self.size, 0)
        self.draw = PIL.ImageDraw.Draw(self.buffer)

    def computePolar(self, angle, r, center = (0, 0)):
        """ Computes the (x, y) point given a center, angle, and radius. """
        angle -= 90 # start from 12
        x = r * math.cos(angle * math.pi / 180.0) + center[0]
        y = r * math.sin(angle * math.pi / 180.0) + center[1]
        return x, y

    def d2b(self, d):
        """ Scales the floating-point delta value into a buffer delta. """
        return round(d * self.zoom * self.maxSize)

    def b2d(self, s):
        """ Scales buffer delta into face coordinates. """
        return float(s) / (self.zoom * self.maxSize)

    def p2b(self, *p):
        """ Scales the floating-point point (or list of points) into a
        buffer pixel. """

        result = []
        for pi in range(0, len(p), 2):
            result += [self.d2b(p[pi] + self.origin[0] / self.zoom),
                       self.d2b(p[pi + 1] + self.origin[1] / self.zoom)]
        return result

    def d2s(self, d):
        """ Scales the floating-point delta value into a screen delta. """
        return round(d * self.zoom * self.maxTargetSize)

    def s2d(self, s):
        """ Scales screen delta into face coordinates. """
        return float(s) / (self.zoom * self.maxTargetSize)

    def s2p(self, *p):
        """ Scales screen point into face coordinates. """

        result = []
        for pi in range(0, len(p), 2):
            result += [self.s2d(p[pi] + self.cropOrigin[0]) - self.origin[0] * self.zoom,
                       self.s2d(p[pi + 1] + self.cropOrigin[1]) - self.origin[1] * self.zoom]
        return result

    def pixelScaleToHandScale(self, s):
        """ Converts a scale into screen units without rounding, for
        computing the hand scale in config_watch.py. """

        return (self.zoom * self.maxTargetSize / self.upscale) / s

    def handScaleToPixelScale(self, s):
        """ Inverse operation of pixelScaleToHandScale(). """

        return (self.zoom * self.maxTargetSize / self.upscale) / s

    def p2s(self, *p):
        """ Scales the floating-point point (or list of points) into a
        screen pixel. """

        result = []
        for pi in range(0, len(p), 2):
            result += [self.d2s(p[pi] + self.origin[0] / self.zoom) - self.cropOrigin[0],
                       self.d2s(p[pi + 1] + self.origin[1] / self.zoom) - self.cropOrigin[1]]
        return result


    def fillCircle(self, diameter, center = (0, 0)):
        """ Draws a filled circle into the buffer.  Same as drawRing with width = None. """
        r = diameter / 2.0
        self.draw.ellipse(self.p2b(center[0] - r, center[1] - r, center[0] + r, center[1] + r), fill = 255)

    def clearCircle(self, diameter, center = (0, 0)):
        """ Clears a circle in the buffer. """
        r = diameter / 2.0
        self.draw.ellipse(self.p2b(center[0] - r, center[1] - r, center[0] + r, center[1] + r), fill = 0)

    def drawRing(self, diameter,  width = 0.003, center = (0, 0)):
        """ Draws a filled circle with a thin or thick ring into the
        buffer.  The circle is always filled, so draw concentric rings
        from the outside in. """

        r = diameter / 2.0
        outer = self.p2b(center[0] - r, center[1] - r, center[0] + r, center[1] + r)
        self.draw.ellipse(outer, fill = 255)

        if width is not None:
            r -= width
            inner = self.p2b(center[0] - r, center[1] - r, center[0] + r, center[1] + r)
            # Make sure the ring has at least one pixel.
            inner = [max(inner[0], outer[0] + 1), max(inner[1], outer[1] + 1),
                     min(inner[2], outer[2] - 1), min(inner[3], outer[3] - 1)]

            self.draw.ellipse(inner, fill = 0)

    def drawLine(self, points, width = 0.003):
        """ Draws a line into the buffer in buffer coordinates. """

        self.draw.line(self.p2b(*points), fill = 255, width = self.d2b(width))

    def drawTicks(self, ticks, ring1, ring2, width = 0.003, center = (0, 0)):
        """ Draws tick marks around a ring.  ticks is either the
        number of ticks around a full circle, or a list of angles;
        ring1 and ring2 are the radius to draw from and to, and width
        is the thickness of each tick mark. """

        if not ticks:
            return

        r1 = ring1 / 2.0
        r2 = ring2 / 2.0

        if isinstance(ticks, type(0)):
            # If we're given a number of ticks, make it a list.
            ticks = map(lambda t: t * 360.0 / ticks, range(ticks))

        for angle in ticks:
            p1 = self.computePolar(angle, r1, center = center)
            p2 = self.computePolar(angle, r2, center = center)
            self.drawLine(p1 + p2, width = width)

    def drawCircularSpots(self, ticks, ring, center = (0, 0), spotDiameter = 0.01, width = 0.003):
        """ Draws spots (actually, rings as in drawRing) in a circle,
        similar to drawTicks().  """

        if not ticks:
            return

        r = ring / 2.0

        if isinstance(ticks, type(0)):
            # If we're given a number of ticks, make it a list.
            ticks = map(lambda t: t * 360.0 / ticks, range(ticks))

        for angle in ticks:
            p = self.computePolar(angle, r, center = center)
            self.drawRing(spotDiameter, width = width, center = p)

    def loadFont(self, filename, fontHeight, directDraw = True):
        """ Loads and returns a font suitable for rendering.  The
        fontHeight is given in face units, and is scaled to the target
        pixels. """
        if directDraw:
            h = self.d2s(fontHeight)
        else:
            h = self.d2b(fontHeight)
        try:
            font = PIL.ImageFont.truetype(filename, h)
        except ImportError:
            raise
            font = None
        return font

    def drawLabel(self, p, text, font, align = 'cc'):
        """ Draws text on the face at the indicated point.  align is
        one of ul, uc, ur, cl, cc, cr, ll, lc, lr.

        The text is drawn directly into the target screen, rather than
        into the offscreen buffer, in an attempt to minimize scaling
        artifacts. """
        if not font:
            return

        sp = self.p2s(*p)
        w, h = self.tdraw.textsize(text, font = font)

        if align[0] == 'c':
            sp = (sp[0], sp[1] - h / 2)
        elif align[0] == 'l':
            sp = (sp[0], sp[1] - h)

        if align[1] == 'c':
            sp = (sp[0] - w / 2, sp[1])
        elif align[1] == 'r':
            sp = (sp[0] - w, sp[1])

        self.tdraw.text(sp, text, fill = self.fg, font = font)

    def drawCircularLabels(self, labels, diameter, font, center = (0, 0), align = 'c', rotate = False, scale = None, directDraw = True):
        """ Draws text in a circle.  labels is a list of (angle,
        text).

        Each label is placed along the ring of the specified diameter,
        according to align: 'c' to center on the ring, 'i' to place
        the label just inside the ring, and 'o' to place it just
        outside.

        If directDraw is True, then the text is drawn directly into
        the target screen, rather than into the offscreen buffer, in
        an attempt to minimize scaling artifacts. """

        if not font:
            return

        for angle, text in labels:
            w, h = self.tdraw.textsize(text, font = font)
            tr = max(w, h) / 2.0  # text radius
            if directDraw:
                tr = self.s2d(tr)
            else:
                tr = self.b2d(tr)
            r = diameter / 2.0
            if align == 'i':
                r -= tr
            elif align == 'o':
                r += tr
            p = self.computePolar(angle, r, center = center)

            if directDraw and not rotate:
                # Just paste the text directly into the buffer, for
                # minimal aliasing.
                sp = self.p2s(*p)
                self.tdraw.text((sp[0] - w / 2, sp[1] - h / 2), text, fill = self.fg, font = font)

            elif directDraw and rotate:
                # Support directDraw with rotate.
                sp = self.p2s(*p)

                b = PIL.Image.new(self.format, (w, h), 0)
                bdraw = PIL.ImageDraw.Draw(b)
                bdraw.text((0, 0), text, fill = 255, font = font)
                b = self.__rotateImage(b, angle)

                w, h = b.size

                # We need to expand it to full size to allow the
                # indirect pasting via fullFg.
                f = PIL.Image.new(self.format, self.targetSize, 0)
                f.paste(b, (sp[0] - w / 2, sp[1] - h / 2))
                self.target.paste(self.fullFg, (0, 0), f)

            else:
                # Paste the text into a temporary buffer, rotate it
                # and/or scale it, then paste it into the target buffer.
                sp = self.p2b(*p)

                b = PIL.Image.new('L', (w, h), 0)
                bdraw = PIL.ImageDraw.Draw(b)
                bdraw.text((0, 0), text, fill = 255, font = font)
                if scale:
                    w, h = b.size
                    ws, hs = (int(w * scale[0] + 0.5), int(h * scale[1] + 0.5))
                    b = b.resize((ws, hs), PIL.Image.ANTIALIAS)
                if rotate:
                    b = self.__rotateImage(b, angle)

                w, h = b.size
                self.buffer.paste(b, (sp[0] - w / 2, sp[1] - h / 2))

                # We need to expand it to full size to allow the
                # indirect pasting via fullFg.
                ## f = PIL.Image.new(self.format, self.targetSize, 0)
                ## f.paste(b, (sp[0] - w / 2, sp[1] - h / 2))
                ## self.target.paste(self.fullFg, (0, 0), f)

    def __rotateImage(self, im, angle):
        r = (-angle) % 360
        if r == 0:
            pass
        elif r == 90:
            im = im.transpose(PIL.Image.ROTATE_90)
        elif r == 180:
            im = im.transpose(PIL.Image.ROTATE_180)
        elif r == 270:
            im = im.transpose(PIL.Image.ROTATE_270)
        else:
            im = im.rotate(r, PIL.Image.BICUBIC, True)
        return im

    def pasteImage(self, p, filename, pivot, pixelScale, rotate = 0):
        """ pixelScale is the pixels per unit, or None to paste
        unscaled directly onto the target face.  pivot is the origin
        of the image in image pixels.  p is the location in face
        units. """

        im = PIL.Image.open(filename)
        if im.mode.endswith('A'):
            im = im.convert('LA')
        else:
            im = im.convert('L')
            im = PIL.ImageChops.invert(im)
            black = PIL.Image.new('L', im.size, 0)
            im = PIL.Image.merge('LA', (black, im))

        if rotate:
            # Center the source image on its pivot, and pad it with transparent.
            border = (pivot[0], pivot[1], im.size[0] - pivot[0], im.size[1] - pivot[1])
            size = (max(border[0], border[2]) * 2, max(border[1], border[3]) * 2)
            center = (size[0] / 2, size[1] / 2)
            large = PIL.Image.new('LA', size, 0)
            large.paste(im, (center[0] - pivot[0], center[1] - pivot[1]))
            im = self.__rotateImage(large, rotate)
            pivot = (im.size[0] / 2, im.size[1] / 2)

        if pixelScale is None:
            p = self.p2s(*p)
            color, mask = im.split()
            if self.format == '1':
                mask = mask.point(thresholdMap)
            self.target.paste(color, (p[0] - pivot[0], p[1] - pivot[1]), mask)

        else:
            pixelScale = float(pixelScale)
            size = (self.d2b(im.size[0] / pixelScale),
                    self.d2b(im.size[1] / pixelScale))
            pivot = (self.d2b(pivot[0] / pixelScale),
                     self.d2b(pivot[1] / pixelScale))
            p = self.p2b(*p)
            im = im.resize(size, PIL.Image.ANTIALIAS)
            color, mask = im.split()
            color.paste(255, (0, 0, color.size[0], color.size[1]))
            self.buffer.paste(color, (p[0] - pivot[0], p[1] - pivot[1]), mask)

    def save(self, filename):
        """ Saves the resulting face. """
        self.flush()
        self.target.save(filename)
