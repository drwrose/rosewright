#! /usr/bin/env python

import PIL.Image
import PIL.ImageChops
import sys
import os
import getopt
from resources.make_rle import make_rle, make_rle_trans

help = """
config_watch.py

This script generates the resources and code tables needed to compile
a watch face with a particular set of hands.  It must be run before
compiling.

config_watch.py [opts]

Options:

    -s style
        Specifies the watch style.  The following styles are available:
          %(watchStyles)s

    -H style
        Overrides the hand style.  The following styles are available:
          %(handStyles)s

    -F style
        Overrides the face style.  The following styles are available:
          %(faceStyles)s

    -S
        Suppress the second hand if it is defined.

    -b
        Enable a quick buzz at the top of the hour.

    -c
        Enable chronograph mode (if the selected hand style includes
        chrono hands).  This builds the watch as a standard app,
        instead of as a watch face, to activate the chronograph
        buttons.

    -i
        Invert the hand color, for instance to apply a set of watch
        hands meant for a white face onto a black face.

    -m
        Exclude moon-phase support.

    -x
        Perform no RLE compression of images.

    -p platform[,platform]
        Specifies the build platform (aplite and/or basalt).

    -d
        Compile for debugging.  Specifically this enables "fast time",
        so the hands move quickly about the face of the watch.  It
        also enables logging.

"""

def usage(code, msg = ''):
    watchStyles = watches.keys()
    watchStyles.sort()
    watchStyles = ' '.join(watchStyles)
    handStyles = hands.keys()
    handStyles.sort()
    handStyles = ' '.join(handStyles)
    faceStyles = faces.keys()
    faceStyles.sort()
    faceStyles = ' '.join(faceStyles)
    print >> sys.stderr, help % {
        'watchStyles' : watchStyles,
        'handStyles' : handStyles,
        'faceStyles' : faceStyles,
        }
    print >> sys.stderr, msg
    sys.exit(code)


# The default center for placing hands on the watch face, if not
# otherwise specified in the centers tables below.
centerX, centerY = 144 / 2, 168 / 2

# Table of watch styles.  A watch style is a combination of a hand
# style and face style, and a unique identifier.  For each style,
# specify the following:
#
#    name, handStyle, faceStyle, uuId
#
#  Where:
#
#   name      - the full name for this watch.
#   handStyle - the default hand style for this watch.
#   faceStyle - the default face style for this watch.
#   uuId      - the UUID to assign to this watch.
#
watches = {
    'a' : ('Rosewright A', 'a', 'a', [0xA4, 0x9C, 0x82, 0xFD, 0x83, 0x0E, 0x48, 0xB4, 0xA8, 0x2E, 0x9C, 0xF8, 0xDA, 0x77, 0xF4, 0xC5]),
    'b' : ('Rosewright B', 'b', 'b', [0xA4, 0x9C, 0x82, 0xFD, 0x83, 0x0E, 0x48, 0xB4, 0xA8, 0x2E, 0x9C, 0xF8, 0xDA, 0x77, 0xF4, 0xC6]),
    'c' : ('Rosewright Chronograph', 'c', 'c', [0xA4, 0x9C, 0x82, 0xFD, 0x83, 0x0E, 0x48, 0xB4, 0xA8, 0x2E, 0x9C, 0xF8, 0xDA, 0x77, 0xF4, 0xC7]),
    'd' : ('Rosewright D', 'd', 'd', [0xA4, 0x9C, 0x82, 0xFD, 0x83, 0x0E, 0x48, 0xB4, 0xA8, 0x2E, 0x9C, 0xF8, 0xDA, 0x77, 0xF4, 0xC8]),
    'e' : ('Rosewright E', 'e', 'e', [0xA4, 0x9C, 0x82, 0xFD, 0x83, 0x0E, 0x48, 0xB4, 0xA8, 0x2E, 0x9C, 0xF8, 0xDA, 0x77, 0xF4, 0xC9]),
    }


# Table of hand styles.  For each style, specify the following for
# each hand type.  Bitmapped hands will have bitmapParams defined and
# vectorParams None; vector hands will have vectorParams defined and
# bitmapParams None.  It is legal to have both bitmapParams and
# vectorParams defined for a given hand.
#
#    hand, bitmapParams, vectorParams
#
#  For bitmapParams:
#     (filename, colorMode, asymmetric, pivot, scale)
#
#   hand - the hand type being defined.
#   filename - the png image that defines this hand, pointing upward.
#   colorMode - indicate how the colors in the png file are to be interpreted.
#       The first part applies only to the Aplite build:
#       'b'  - black pixels are drawn as black, white pixels are ignored.
#       'w'  - white pixels are drawn as white, black pixels are ignored.
#       '-b' - black pixels are drawn as white, white pixels are ignored.
#       '-w' - white pixels are drawn as black, black pixels are ignored.
#       't'  - opaque pixels are drawn in their own color,
#              transparent pixels are ignored.  This doubles the
#              resource cost.
#       '-t' - opaque pixels are drawn in their opposite color,
#              transparent pixels are ignored.  This doubles the
#              resource cost.
#       In addition, if any of the above is suffixed with '%', it
#       means to dither the grayscale levels in the png file to
#       produce the final black or white color pixel.  Without this
#       symbol, the default is to use thresholding.
#   asymmetric - false if the hand is left/right symmetric and can be
#       drawn mirrored, or false if it must be drawn without
#       mirroring, which doubles the resource cost again.
#   pivot - the (x, y) pixel point of the center of rotation of the
#       hand in its image.
#   scale - a scale factor for reducing the hand to its final size.
#
#  For vectorParams:
#     [(fillType, points), (fillType, points), ...]
#
#   fillType - Specify the type of the drawing:
#       'b'  - black unfilled stroke
#       'w'  - white unfilled stroke
#       'bb' - black stroke filled with black
#       'bw' - black stroke filled with white
#       'wb' - white stroke filled with black
#       'ww' - white stroke filled with white
#   points - a list of points in the vector.  Draw the hand in the
#       vertical position, from the pivot at (0, 0).
#

hands = {
    'a' : [('hour', ('a_hour_hand.png', 'b', False, (78, 410), 0.12), None),
           ('minute', ('a_minute_hand.png', 'b', True, (37, 557), 0.12), None),
           ('second', ('a_second_hand.png', 'b', False, (37, -28), 0.12),
            [('b', [(0, -5), (0, -70)])]),
           ],
    'b' : [('hour', ('b_hour_hand.png', 'b', False, (33, 211), 0.27), None),
           ('minute', ('b_minute_hand.png', 'b', False, (24, 280), 0.27), None),
           ('second', ('b_second_hand.png', 'b', False, (33, -23), 0.27),
            [('b', [(0, -5), (0, -75)]),
             ]),
           ],
    'c' : [('hour', ('c_hour_hand.png', 't%', False, (59, 434), 0.14), None),
           ('minute', ('c_minute_hand.png', 't%', False, (38, 584), 0.14), None),
           ('second', ('c_chrono1_hand.png', 'w', False, (32, -27), 0.14),
            [('w', [(0, -2), (0, -26)]),
             ]),
           ('chrono_minute', ('c_chrono2_hand.png', 'w', False, (37, 195), 0.14), None),
           ('chrono_second', ('c_second_hand.png', 'w', False, (41, -29), 0.14),
            [('w', [(0, -4), (0, -88)]),
             ]),
           ('chrono_tenth', ('c_chrono2_hand.png', 'w', False, (37, 195), 0.14), None),
           ],
    'd' : [('hour', ('d_hour_hand.png', 't', False, (24, 193), 0.24), None),
           ('minute', ('d_minute_hand.png', 't', False, (27, 267), 0.24), None),
           ('second', ('d_second_hand.png', 'b', False, (14, -8), 0.24),
            [('b', [(0, -3), (0, -64)]),
             ]),
           ],
    'e' : [('hour', ('e_hour_hand.png', 't%', False, (28, 99), 0.53), None),
           ('minute', ('e_minute_hand.png', 't%', False, (22, 142), 0.53), None),
           ('second', ('e_second_hand.png', 't%', False, (18, 269), 0.24), None),
           ],
    }

# Table of face styles.  For each style, specify the following:
#
#   filename  - the background image for the face, or a list of optional faces.
#   chrono    - the (tenths, hours) images for the two chrono dials, if used.
#   hand_color    - the (and, or) tuple to adjust the hand colors in a Basalt build.
#   date_window_a - the (x, y, c) position, color, background of the first date window.
#   date_window_b - the (x, y, c) position, color, background of the second date window.
#   date_window_c - etc.  All date windows must be consecutively named.
#   date_window_filename - the (window, mask) images shared by all date windows.  The mask is used only if one of the date_window_* colors includes t for transparency.
#   battery   - the (x, y, c) position and color of the battery gauge, or None.
#   bluetooth - the (x, y, c) position and color of the bluetooth indicator, or None.
#   defaults  - a list of things enabled by default: one or more of 'date:X', 'day:X', 'battery', 'bluetooth', 'second'
#   centers   - a tuple of ((hand, x, y), ...) to indicate the position for
#               each kind of watch hand.  If the tuple is empty or a
#               hand is omitted, the default is the center.  This also
#               defines the stacking order of the hands--any
#               explicitly listed hands are drawn in the order
#               specified, followed by all of the implicit hands in
#               the usual order.
#

# Note that filename may be a single string if the face style supports
# a single watchface background, or it may be a list of strings if the
# face style supports multiple background options.  If it is a list of
# strings, then date_window_*, bluetooth, and battery may also be
# lists, in which case their definitions are applied separately for
# each background option.  (But they don't have to be lists, in which
# case their definitions are the same for all background options.)

# Al date windows must share the same (window, mask) images.  They
# must all be the same standard size, which is hardcoded in the source
# code at 39 x 19 pixels.

faces = {
    'a' : {
        'filename': ['a_face.png', 'a_face_unrotated.png'],
        'date_window_a': (38, 82, 'b'),
        'date_window_b': (106, 82, 'b'),
        'date_window_c' : (52, 109, 'b'),
        'date_window_d' : (92, 109, 'b'),
        'date_window_filename' : ('date_window.png', 'date_window_mask.png'),
        'bluetooth' : (37, 47, 'b'),
        'battery' : (92, 51, 'b'),
        'defaults' : [ 'date:b' ],
        },
    'b' : {
        'filename' : ['b_face_rect.png', 'b_face.png'],
        'date_window_a' : (72, 54, 'bt'),
        'date_window_b' : (52, 109, 'b'),
        'date_window_c' : (92, 109, 'b'),
        'date_window_filename' : ('date_window.png', 'date_window_mask.png'),
        'bluetooth' : (0, 0, 'bt'),
        'battery' : (125, 3, 'bt'),
        'defaults' : [ 'day:b', 'date:c' ],
        },
    'c' : {
        'filename' : ['c_face.png', 'c_face_rect.png'],
        'chrono' : ('c_face_chrono_tenths.png', 'c_face_chrono_hours.png'),
        'centers' : (('chrono_minute', 115, 84), ('chrono_tenth', 72, 126), ('second', 29, 84)),
        'date_window_a' : (52, 45, 'wt'),
        'date_window_b' : (92, 45, 'wt'),
        'date_window_filename' : ('date_window.png', 'date_window_mask.png'),
        'bluetooth' : [ (0, 0, 'w'), (16, 18, 'w'), ],
        'battery' : [ (125, 3, 'w'), (109, 21, 'w'), ],
        'defaults' : [ 'second' ],
        },
    'd' : {
        'filename' : ['d_face_rect.png', 'd_face_rect_clean.png', 'd_face.png', 'd_face_clean.png'],
        'hand_color' : [ (0xff, 0x02), (0xff, 0x02), (0xff, 0x02), (0xff, 0x02) ],
        'date_window_a': [ (49, 102, 'wt'), (49, 102, 'b'),
                           (41, 82, 'wt'), (41, 82, 'b'), ],
        'date_window_b': [ (95, 102, 'wt'), (95, 102, 'b'),
                           (103, 82, 'wt'), (103, 82, 'b'), ],
        'date_window_c' : [ (49, 125, 'wt'), (49, 125, 'b'),
                            (52, 107, 'wt'), (52, 107, 'b'), ],
        'date_window_d' : [ (95, 125, 'wt'), (95, 125, 'b'),
                            (92, 107, 'wt'), (92, 107, 'b'), ],
        'date_window_filename' : ('date_window.png', 'date_window_mask.png'),
        'bluetooth' : [ (49, 45, 'bt'), (49, 45, 'bt'),
                        (0, 0, 'w'), (0, 0, 'w'), ],
        'battery' : [ (79, 49, 'bt'), (79, 49, 'bt'),
                      (125, 3, 'w'), (125, 3, 'w'), ],
        'defaults' : [ 'day:c', 'date:d', 'bluetooth', 'battery' ],
        },
    'e' : {
        'filename' : ['e_face.png', 'e_face_white.png'],
        'hand_color' : [ (0xff, 0x00), (0xfe, 0x00) ],
        'date_window_a' : (72, 21, 'bt'),
        'date_window_b' : (21, 82, 'bt'),
        'date_window_c' : (123, 82, 'bt'),
        'date_window_d' : (72, 146, 'bt'),
        'date_window_filename' : ('date_window.png', 'date_window_mask.png'),
        'bluetooth' : [ (11, 12, 'w'), (11, 12, 'b'), ],
        'battery' : [ (115, 16, 'w'), (115, 16, 'b'), ],
        'defaults' : [ 'date:c' ],
        },
    }

makeChronograph = False
enableSecondHand = False
suppressSecondHand = False
enableHourBuzzer = False
enableChronoMinuteHand = False
enableChronoSecondHand = False
enableChronoTenthHand = False
date_windows = []
date_window_filename = None
bluetooth = [None]
battery = [None]
defaults = []

# The number of subdivisions around the face for each kind of hand.
# Increase these numbers to show finer movement; decrease them to save
# resource memory.
numSteps = {
    'hour' : 48,
    'minute' : 60,
    'second' : 60,
    'chrono_minute' : 30,
    'chrono_second' : 60,
    'chrono_tenth' : 24,
    }

# If you use the -w option to enable sweep seconds, it means we need a
# greater number of second-hand subdivisions.
numStepsSweep = {
    'second' : 180,
    'chrono_second' : 180,
    }

threshold1Bit = [0] * 128 + [255] * 128
threshold2Bit = [0] * 64 + [85] * 64 + [170] * 64 + [255] * 64

# Attempt to determine the directory in which we're operating.
rootDir = os.path.dirname(__file__) or '.'
resourcesDir = os.path.join(rootDir, 'resources')

def formatUuId(uuId):
    return '%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x' % tuple(uuId)

def parseColorMode(colorMode):
    paintBlack = False
    useTransparency = False
    invertColors = False
    dither = False
    blackToken, whiteToken = 'b', 'w'

    inverted = False
    if colorMode[0] == '-':
        inverted = True
        colorMode = colorMode[1:]
    if invertHands:
        inverted = not inverted

    if inverted:
        invertColors = True
        blackToken, whiteToken = 'w', 'b'

    if colorMode[0] == blackToken:
        # Black is the foreground color.
        invertColors = not invertColors
        paintBlack = True
    elif colorMode[0] == whiteToken:
        # White is the foreground color.
        paintBlack = False
    elif colorMode[0] == 't':
        invertColors = not invertColors
        paintBlack = True
        useTransparency = True

    if colorMode.endswith('%'):
        dither = True

    return paintBlack, useTransparency, invertColors, dither

def makeFaces(generatedTable, generatedDefs):

    resourceStr = ''

    faceResourceEntry = """
    {
      "name": "CLOCK_FACE_%(index)s",
      "file": "%(rleFilename)s",
      "type": "%(ptype)s"
    },"""

    dateWindowEntry = """
    {
      "name": "%(name)s",
      "file": "%(rleFilename)s",
      "type": "%(ptype)s"
    },"""

    chronoResourceEntry = """
    {
      "name": "CHRONO_DIAL_TENTHS_WHITE",
      "file": "%(targetChronoTenthsWhite)s",
      "type": "%(ptype)s"
    },
    {
      "name": "CHRONO_DIAL_TENTHS_BLACK",
      "file": "%(targetChronoTenthsBlack)s",
      "type": "%(ptype)s"
    },
    {
      "name": "CHRONO_DIAL_HOURS_WHITE",
      "file": "%(targetChronoHoursWhite)s",
      "type": "%(ptype)s"
    },
    {
      "name": "CHRONO_DIAL_HOURS_BLACK",
      "file": "%(targetChronoHoursBlack)s",
      "type": "%(ptype)s"
    },"""

    fd = faces[faceStyle]
    faceFilenames = fd.get('filename')
    if isinstance(faceFilenames, type('')):
        faceFilenames = [faceFilenames]

    targetChronoTenths = None
    targetChronoHours = None
    chronoFilenames = fd.get('chrono')
    if chronoFilenames:
        targetChronoTenths, targetChronoHours = chronoFilenames

    handColors = fd.get('hand_color')
    if not handColors:
      handColors = [(0xff, 0x00)] * len(faceFilenames)
    elif isinstance(handColors[0], type(0x00)):
      handColors = [handColors] * len(faceFilenames)
    assert len(faceFilenames) == len(handColors)
        
    print >> generatedTable, "struct FaceDef clock_face_table[NUM_FACES] = {"
    for i in range(len(faceFilenames)):
        print >> generatedTable, "  { RESOURCE_ID_CLOCK_FACE_%s, %s, %s }," % (
          i, handColors[i][0], handColors[i][1])

        rleFilename, ptype = make_rle('clock_faces/' + faceFilenames[i], useRle = supportRle)
        resourceStr += faceResourceEntry % {
            'index' : i,
            'rleFilename' : rleFilename,
            'ptype' : ptype,
            'and_argb8' : handColors[i][0],
            'or_argb8' : handColors[i][1],
            }
    print >> generatedTable, "};\n"

    if date_windows and date_window_filename:
        window, mask = date_window_filename

        rleFilename, ptype = make_rle('clock_faces/' + window, useRle = supportRle)
        resourceStr += dateWindowEntry % {
            'name' : 'DATE_WINDOW',
            'rleFilename' : rleFilename,
            'ptype' : ptype,
            }

        if mask:
            rleFilename, ptype = make_rle('clock_faces/' + mask, useRle = supportRle)
            resourceStr += dateWindowEntry % {
                'name' : 'DATE_WINDOW_MASK',
                'rleFilename' : rleFilename,
                'ptype' : ptype,
                }

    if targetChronoTenths:
        tenthsWhite, tenthsBlack, ptype = make_rle_trans('clock_faces/' + targetChronoTenths, useRle = supportRle)
        hoursWhite, hoursBlack, ptype = make_rle_trans('clock_faces/' + targetChronoHours, useRle = supportRle)
        resourceStr += chronoResourceEntry % {
            'targetChronoTenthsWhite' : tenthsWhite,
            'targetChronoTenthsBlack' : tenthsBlack,
            'targetChronoHoursWhite' : hoursWhite,
            'targetChronoHoursBlack' : hoursBlack,
            'ptype' : ptype,
            }

    return resourceStr

def makeVectorHands(generatedTable, generatedDefs, hand, groupList):
    resourceStr = ''

    colorMap = {
        'b' : '1',
        'w' : '2',
        '' : '0',
        }

    print >> generatedTable, "struct VectorHand %s_hand_vector_table = {" % (hand)

    print >> generatedTable, "  %s, (struct VectorHandGroup[]){" % (len(groupList))
    for fillType, points in groupList:
        stroke = colorMap[fillType[0]]
        fill = colorMap[fillType[1:2]]
        print >> generatedTable, "  { %s, %s, { %s, (GPoint[]){" % (stroke, fill, len(points))
        for px, py in points:
            print >> generatedTable, "    { %s, %s }," % (px, py)
        print >> generatedTable, "  } } },"

    print >> generatedTable, "  }"
    print >> generatedTable, "};\n"

    return resourceStr

def getNumSteps(hand):
    # Get the number of subdivisions for the hand.
    numStepsHand = numSteps[hand]

    # If we're building support for sweep-second hands, we might need
    # more subdivisions.
    if supportSweep:
        if makeChronograph:
            if hand == 'chrono_second':
                numStepsHand = numStepsSweep[hand]
        else:
            if hand == 'second':
                numStepsHand = numStepsSweep[hand]

    return numStepsHand


def makeBitmapHands(generatedTable, generatedDefs, useRle, hand, sourceFilename, colorMode, asymmetric, pivot, scale):
    resourceStr = ''
    maskResourceStr = ''

    resourceEntry = """
    {
      "name": "%(defName)s",
      "file": "%(targetFilename)s",
      "type": "%(ptype)s"
    },"""

    handLookupEntry = """  { %(cx)s, %(cy)s },  // %(symbolName)s"""
    handTableEntry = """  { %(lookup_index)s, %(flip_x)s, %(flip_y)s },"""

    handLookupLines = {}
    maxLookupIndex = -1
    handTableLines = []

    source = PIL.Image.open('%s/clock_hands/%s' % (resourcesDir, sourceFilename))
    paintBlack, useTransparency, invertColors, dither = parseColorMode(colorMode)

    if useTransparency or source.mode.endswith('A'):
        r, g, b, sourceMask = source.convert('RGBA').split()
        source = PIL.Image.merge('RGB', [r, g, b])
    else:
        source = source.convert('RGB')
        sourceMask = None

    # We must do the below operations with white as the foreground
    # color and black as the background color, because the
    # rotate() operation always fills with black, and getbbox() is
    # always based on black.  Also, the Pebble library likes to
    # use white as the foreground color too.  So, invert the image
    # if necessary to make white the foreground color.
    if invertColors:
        source = PIL.ImageChops.invert(source)

    # The mask already uses black as the background color, no need
    # to invert that.

    if sourceMask:
        # Ensure that the source image is black anywhere the mask
        # is black (sometimes there is junk in the original png
        # image outside of the alpha channel coverage that the
        # artist didn't even know about).
        black = PIL.Image.new('L', source.size, 0)
        r, g, b = source.split()
        r = PIL.Image.composite(r, black, sourceMask)
        g = PIL.Image.composite(g, black, sourceMask)
        b = PIL.Image.composite(b, black, sourceMask)
        source = PIL.Image.merge('RGB', [r, g, b])

    # Center the source image on its pivot, and pad it with black.
    border = (pivot[0], pivot[1], source.size[0] - pivot[0], source.size[1] - pivot[1])
    size = (max(border[0], border[2]) * 2, max(border[1], border[3]) * 2)
    center = (size[0] / 2, size[1] / 2)
    large = PIL.Image.new('RGB', size, 0)
    large.paste(source, (center[0] - pivot[0], center[1] - pivot[1]))

    if useTransparency:
        largeMask = PIL.Image.new('L', size, 0)
        largeMask.paste(sourceMask, (center[0] - pivot[0], center[1] - pivot[1]))

    numStepsHand = getNumSteps(hand)
    for i in range(numStepsHand):
        flip_x = False
        flip_y = False
        angle = i * 360.0 / numStepsHand

        # Check for quadrant symmetry, an easy resource-memory
        # optimization.  Instead of generating bitmaps for all 360
        # degrees of the hand, we may be able to generate the
        # first quadrant only (or the first half only) and quickly
        # flip it into the remaining quadrants.
        if not asymmetric:
            # If the hand is symmetric, we can treat the x and y
            # flips independently, and this means we really only
            # need a single quadrant.

            if angle > 90:
                # If we're outside of the first quadrant, maybe we can
                # just flip a first-quadrant hand into the appropriate
                # quadrant, and save a bit of resource memory.
                i2 = i
                if angle > 180:
                    # If we're in the right half of the circle, flip
                    # over from the left.
                    i = (numStepsHand - i)
                    flip_x = True
                    angle = i * 360.0 / numStepsHand

                if angle > 90 and angle < 270:
                    # If we're in the bottom half of the circle, flip
                    # over from the top.
                    i = (numStepsHand / 2 - i) % numStepsHand
                    flip_y = True
                    angle = i * 360.0 / numStepsHand
        else:
            # If the hand is asymmetric, then it's important not
            # to flip it an odd number of times.  But we can still
            # apply both flips at once (which is really a
            # 180-degree rotation), and this means we only need to
            # generate the right half, and rotate into the left.
            if angle >= 180:
                i -= (numStepsHand / 2)
                flip_x = True
                flip_y = True
                angle = i * 360.0 / numStepsHand

        symbolName = '%s_%s' % (hand.upper(), i)
        symbolMaskName = symbolName
        if useTransparency:
            symbolMaskName = '%s_%s_mask' % (hand.upper(), i)

        if i not in handLookupLines:
            # Here we have a new rotation of the bitmap image to
            # generate.  We might have decided to flip this image from
            # another image i, but we still need to scale and rotate
            # the source image now, if for no other reason than to
            # compute cx, cy.

            # We expect to encounter each i the first time in an
            # unflipped state, because we visit quadrant I first.
            assert not flip_x and not flip_y

            p = large.rotate(-angle, PIL.Image.BICUBIC, True)
            scaledSize = (int(p.size[0] * scale + 0.5), int(p.size[1] * scale + 0.5))
            p = p.resize(scaledSize, PIL.Image.ANTIALIAS)

            # Now make the 1-bit version for Aplite and the 2-bit
            # version for Basalt.
            r, g, b = p.split()
            if not dither:
                p1 = b.point(threshold1Bit).convert('1')
            else:
                p1 = b.convert('1')

            r, g, b = p.split()
            r = r.point(threshold2Bit).convert('L')
            g = g.point(threshold2Bit).convert('L')
            b = b.point(threshold2Bit).convert('L')
            p2 = PIL.Image.merge('RGB', [r, g, b])

            cx, cy = p2.size[0] / 2, p2.size[1] / 2
            cropbox = p2.getbbox()
            if useTransparency:
                pm = largeMask.rotate(-angle, PIL.Image.BICUBIC, True)
                pm = pm.resize(scaledSize, PIL.Image.ANTIALIAS)

                # And the 1-bit version and 2-bit versions of the
                # mask.
                pm1 = pm.point(threshold1Bit).convert('1')
                pm2 = pm.point(threshold2Bit).convert('L')

                # In the useTransparency case, it's important to take
                # the crop from the alpha mask, not from the color.
                cropbox = pm2.getbbox()
                pm1 = pm1.crop(cropbox)
                pm2 = pm2.crop(cropbox)

            p1 = p1.crop(cropbox)
            p2 = p2.crop(cropbox)

            cx, cy = cx - cropbox[0], cy - cropbox[1]

            # Now that we have scaled and rotated image i, write it
            # out.

            # We require our images to be an even multiple of 8 pixels
            # wide, to make it easier to reverse the bits
            # horizontally.  (Actually we only need it to be an even
            # multiple of bytes, but the Aplite build is the lowest
            # common denominator with 8 pixels per byte.)
            w = 8 * ((p2.size[0] + 7) / 8)
            if w != p2.size[0]:
                pt = PIL.Image.new('1', (w, p1.size[1]), 0)
                pt.paste(p1, (0, 0))
                p1 = pt
                pt = PIL.Image.new('RGB', (w, p2.size[1]), 0)
                pt.paste(p2, (0, 0))
                p2 = pt
                if useTransparency:
                    pt = PIL.Image.new('1', (w, pm1.size[1]), 0)
                    pt.paste(pm1, (0, 0))
                    pm1 = pt
                    pt = PIL.Image.new('L', (w, pm2.size[1]), 0)
                    pt.paste(pm2, (0, 0))
                    pm2 = pt

            if not useTransparency:
                # In the Basalt non-transparency case, the grayscale
                # color of the hand becomes the mask (which then
                # becomes the alpha channel).
                pm2 = p2.convert('L')
                p2 = PIL.Image.new('RGB', p2.size, 0)

            if useTransparency or not paintBlack:
                # Re-invert the color in the Basalt case so it's the
                # natural color.
                p2 = PIL.ImageChops.invert(p2)

            # In the Basalt case, apply the mask as the alpha channel.
            r, g, b = p2.split()
            p2 = PIL.Image.merge('RGBA', [r, g, b, pm2])

            if useTransparency:
                # In the Aplite transparency case, we need to load a
                # mask image.
                targetMaskFilename = 'clock_hands/flat_%s_%s_%s_mask.png' % (handStyle, hand, i)
                pm1.save('%s/%s' % (resourcesDir, targetMaskFilename))
                rleFilename, ptype = make_rle(targetMaskFilename, useRle = useRle)
                maskResourceStr += resourceEntry % {
                    'defName' : symbolMaskName,
                    'targetFilename' : rleFilename,
                    'ptype' : ptype,
                    }

            targetBasename = 'clock_hands/flat_%s_%s_%s' % (handStyle, hand, i)
            p1.save('%s/%s~bw.png' % (resourcesDir, targetBasename))
            p2.save('%s/%s.png' % (resourcesDir, targetBasename))
            rleFilename, ptype = make_rle(targetBasename + '.png', useRle = useRle)

            resourceStr += resourceEntry % {
                'defName' : symbolName,
                'targetFilename' : rleFilename,
                'ptype' : ptype,
                }

            line = handLookupEntry % {
                'symbolName' : symbolName,
                'cx' : cx,
                'cy' : cy,
                }
            handLookupLines[i] = line
            maxLookupIndex = max(maxLookupIndex, i)

        line = handTableEntry % {
            'lookup_index' : i,
            'flip_x' : int(flip_x),
            'flip_y' : int(flip_y),
            }
        handTableLines.append(line)

    print >> generatedTable, "struct BitmapHandCenterRow %s_hand_bitmap_lookup[] = {" % (hand)
    for i in range(maxLookupIndex + 1):
        line = handLookupLines.get(i, "  {},");
        print >> generatedTable, line
    print >> generatedTable, "};\n"

    print >> generatedTable, "struct BitmapHandTableRow %s_hand_bitmap_table[NUM_STEPS_%s] = {" % (hand, hand.upper())
    for line in handTableLines:
        print >> generatedTable, line
    print >> generatedTable, "};\n"

    return resourceStr + maskResourceStr

def makeHands(generatedTable, generatedDefs):
    """ Generates the required resources and tables for the indicated
    hand style.  Returns resourceStr. """

    resourceStr = ''

    handDefEntry = """struct HandDef %(hand)s_hand_def = {
    NUM_STEPS_%(handUpper)s,
    %(resourceId)s, %(resourceMaskId)s,
    %(placeX)s, %(placeY)s,
    %(useRle)s,
    %(paintBlack)s,
    %(bitmapCenters)s,
    %(bitmapTable)s,
    %(vectorTable)s,
};
"""

    for hand, bitmapParams, vectorParams in hands[handStyle]:
        useRle = supportRle
        if hand == 'second':
            global enableSecondHand
            enableSecondHand = True
            #useRle = False
        elif hand == 'chrono_minute':
            global enableChronoMinuteHand
            enableChronoMinuteHand = True
        elif hand == 'chrono_second':
            global enableChronoSecondHand
            enableChronoSecondHand = True
            #useRle = False
        elif hand == 'chrono_tenth':
            global enableChronoTenthHand
            enableChronoTenthHand = True

        resourceId = '0'
        resourceMaskId = resourceId
        paintBlack = False
        bitmapCenters = 'NULL'
        bitmapTable = 'NULL'
        vectorTable = 'NULL'

        if bitmapParams:
            resourceStr += makeBitmapHands(generatedTable, generatedDefs, useRle, hand, *bitmapParams)
            colorMode = bitmapParams[1]
            paintBlack, useTransparency, invertColors, dither = parseColorMode(colorMode)
            resourceId = 'RESOURCE_ID_%s_0' % (hand.upper())
            resourceMaskId = resourceId
            if useTransparency:
                resourceMaskId = 'RESOURCE_ID_%s_0_mask' % (hand.upper())
            bitmapCenters = '%s_hand_bitmap_lookup' % (hand)
            bitmapTable = '%s_hand_bitmap_table' % (hand)

        if vectorParams:
            resourceStr += makeVectorHands(generatedTable, generatedDefs, hand, vectorParams)
            vectorTable = '&%s_hand_vector_table' % (hand)

        handDef = handDefEntry % {
            'hand' : hand,
            'handUpper' : hand.upper(),
            'resourceId' : resourceId,
            'resourceMaskId' : resourceMaskId,
            'placeX' : cxd.get(hand, centerX),
            'placeY' : cyd.get(hand, centerY),
            'useRle' : int(bool(useRle)),
            'paintBlack' : int(bool(paintBlack)),
            'bitmapCenters' : bitmapCenters,
            'bitmapTable' : bitmapTable,
            'vectorTable' : vectorTable,
        }
        print >> generatedTable, handDef
        print >> generatedDefs, 'extern struct HandDef %s_hand_def;' % (hand)

    return resourceStr

def getIndicator(fd, indicator):
    """ Gets an indicator tuple from the config dictionary.  Finds
    either a tuple or a list of tuples; in either case returns a list
    of tuples. """

    list = fd.get(indicator, None)
    if not isinstance(list, type([])):
        list = [list]

    return list

def makeIndicatorTable(generatedTable, generatedDefs, name, indicator, anonymous = False):
    """ Makes an array of IndicatorTable values to define how a given
    indicator (that is, a bluetooth or battery indicator, or a
    day/date window) is meant to be rendered for each of the alternate
    faces. """

    if not indicator[0]:
        return
    if len(indicator) == 1 and numFaces != 1:
        indicator = [indicator[0]] * numFaces

    assert len(indicator) == numFaces

    if anonymous:
        # Anonymous structure within a table
        print >> generatedTable, "  { // %s" % (name)
    else:
        # Standalone named structure
        print >> generatedDefs, "extern struct IndicatorTable %s[NUM_FACES];" % (name)
        print >> generatedTable, "struct IndicatorTable %s[NUM_FACES] = {" % (name)
    for x, y, c in indicator:
        print >> generatedTable, "   { %s, %s, %s, %s }," % (
            x, y, int(c[0] == 'w'), int(c[-1] == 't'))

    if anonymous:
        # Anonymous structure within a table
        print >> generatedTable, "  },";
    else:
        print >> generatedTable, "};\n";

def makeMoon():
    """ Returns the resource strings needed to include the moon phase icons. """

    moonPhaseEntry = """
    {
      "name": "MOON_%(cat)s_%(index)s",
      "file": "%(rleFilename)s",
      "type": "%(ptype)s"
    },"""

    resourceStr = ''

    # moon_white_*.png is for when the moon is to be drawn as white pixels on black.
    # moon_black_*.png is for when the moon is to be drawn as black pixels on white.

    for cat in ['white', 'black']:
        for index in range(8):
            rleFilename, ptype = make_rle('clock_faces/moon_%s_%s.png' % (cat, index), useRle = supportRle)
            resourceStr += moonPhaseEntry % {
                'cat' : cat.upper(),
                'index' : index,
                'rleFilename' : rleFilename,
                'ptype' : ptype,
                }

    return resourceStr

def enquoteStrings(strings):
    """ Accepts a list of strings, returns a list of strings with
    embedded quotation marks. """
    quoted = []
    for str in strings:
        quoted.append('"%s"' % (str))
    return quoted

def configWatch():
    generatedTable = open('%s/generated_table.c' % (resourcesDir), 'w')
    generatedDefs = open('%s/generated_defs.h' % (resourcesDir), 'w')

    resourceStr = ''
    resourceStr += makeFaces(generatedTable, generatedDefs)
    resourceStr += makeHands(generatedTable, generatedDefs)

    if supportMoon:
        resourceStr += makeMoon()

    print >> generatedDefs, "extern struct IndicatorTable date_windows[NUM_DATE_WINDOWS][NUM_FACES];"
    print >> generatedTable, "struct IndicatorTable date_windows[NUM_DATE_WINDOWS][NUM_FACES] = {"
    for i in range(len(date_windows)):
        ch = chr(97 + i)
        makeIndicatorTable(generatedTable, generatedDefs, ch, date_windows[i], anonymous = True)
    print >> generatedTable, "};\n"

    makeIndicatorTable(generatedTable, generatedDefs, 'battery_table', battery)
    makeIndicatorTable(generatedTable, generatedDefs, 'bluetooth_table', bluetooth)

    resourceIn = open('%s/appinfo.json.in' % (rootDir), 'r').read()
    resource = open('%s/appinfo.json' % (rootDir), 'w')

    watchface = 'true'
    if makeChronograph and enableChronoSecondHand:
        watchface = 'false'

    langData = open('%s/lang_data.json' % (resourcesDir), 'r').read()
    generatedMedia = resourceStr[:-1]

    print >> resource, resourceIn % {
        'uuId' : formatUuId(uuId),
        'watchName' : watchName,
        'watchface' : watchface,
        'langData' : langData,
        'targetPlatforms' : ', '.join(enquoteStrings(targetPlatforms)),
        'generatedMedia' : generatedMedia,
        }

    displayLangLookup = open('%s/displayLangLookup.txt' % (resourcesDir), 'r').read()

    jsIn = open('%s/src/js/pebble-js-app.js.in' % (rootDir), 'r').read()
    js = open('%s/src/js/pebble-js-app.js' % (rootDir), 'w')

    if 'battery' in defaults:
        defaultBattery = 2
    else:
        defaultBattery = 1
    if 'bluetooth' in defaults:
        defaultBluetooth = 2
    else:
        defaultBluetooth = 1

    print >> js, jsIn % {
        'watchName' : watchName,
        'numFaces' : numFaces,
        'numDateWindows' : len(date_windows),
        'enableChronoDial' : int(makeChronograph),
        'supportMoon' : int(bool(supportMoon)),
        'defaultBluetooth' : defaultBluetooth,
        'defaultBattery' : defaultBattery,
        'enableSecondHand' : int(enableSecondHand and not suppressSecondHand),
        'enableHourBuzzer' : int(enableHourBuzzer),
        'enableSweepSeconds' : int(enableSecondHand and supportSweep),
        'defaultDateWindows' : repr(defaultDateWindows),
        'displayLangLookup' : displayLangLookup,
        }

    configIn = open('%s/generated_config.h.in' % (resourcesDir), 'r').read()
    config = open('%s/generated_config.h' % (resourcesDir), 'w')

    # Get the stacking orders of the hands too.
    implicitStackingOrder = ['hour', 'minute', 'second', 'chrono_minute', 'chrono_second', 'chrono_tenth']
    explicitStackingOrder = []
    for hand, x, y in centers:
        if hand in implicitStackingOrder:
            implicitStackingOrder.remove(hand)
            explicitStackingOrder.append(hand)
    stackingOrder = map(lambda hand: 'STACKING_ORDER_%s' % (hand.upper()), explicitStackingOrder + implicitStackingOrder)
    stackingOrder.append('STACKING_ORDER_DONE')

    print >> config, configIn % {
        'persistKey' : 0x5151 + uuId[-1],
        'supportRle' : int(bool(supportRle)),
        'numFaces' : numFaces,
        'numDateWindows' : len(date_windows),
        'numStepsHour' : numSteps['hour'],
        'numStepsMinute' : numSteps['minute'],
        'numStepsSecond' : getNumSteps('second'),
        'numStepsChronoMinute' : numSteps['chrono_minute'],
        'numStepsChronoSecond' : getNumSteps('chrono_second'),
        'numStepsChronoTenth' : numSteps['chrono_tenth'],
        'compileDebugging' : int(compileDebugging),
        'defaultDateWindows' : repr(defaultDateWindows)[1:-1],
        'enableBluetooth' : int(bool(bluetooth[0])),
        'defaultBluetooth' : int(bool('bluetooth' in defaults)),
        'enableBatteryGauge' : int(bool(battery[0])),
        'defaultBattery' : int(bool('battery' in defaults)),
        'enableSecondHand' : int(enableSecondHand and not suppressSecondHand),
        'enableSweepSeconds' : int(enableSecondHand and supportSweep),
        'enableHourBuzzer' : int(enableHourBuzzer),
        'makeChronograph' : int(makeChronograph and enableChronoSecondHand),
        'supportMoon' : int(bool(supportMoon)),
        'enableChronoMinuteHand' : int(enableChronoMinuteHand),
        'enableChronoSecondHand' : int(enableChronoSecondHand),
        'enableChronoTenthHand' : int(enableChronoTenthHand),
        'stackingOrder' : ', '.join(stackingOrder),
        }


# Main.
try:
    opts, args = getopt.getopt(sys.argv[1:], 's:H:F:Sbciwmxp:dh')
except getopt.error, msg:
    usage(1, msg)

watchStyle = None
handStyle = None
faceStyle = None
invertHands = False
supportSweep = False
compileDebugging = False
supportMoon = True
supportRle = True
#supportRle = False
targetPlatforms = [ ]
for opt, arg in opts:
    if opt == '-s':
        watchStyle = arg
        if watchStyle not in watches:
            print >> sys.stderr, "Unknown watch style '%s'." % (arg)
            sys.exit(1)
    elif opt == '-H':
        handStyle = arg
        if handStyle not in hands:
            print >> sys.stderr, "Unknown hand style '%s'." % (arg)
            sys.exit(1)
    elif opt == '-F':
        faceStyle = arg
        if faceStyle not in faces:
            print >> sys.stderr, "Unknown face style '%s'." % (arg)
            sys.exit(1)
    elif opt == '-S':
        suppressSecondHand = True
    elif opt == '-b':
        enableHourBuzzer = True
    elif opt == '-c':
        makeChronograph = True
    elif opt == '-i':
        invertHands = True
    elif opt == '-w':
        supportSweep = True
    elif opt == '-m':
        supportMoon = False
    elif opt == '-x':
        supportRle = False
    elif opt == '-p':
        targetPlatforms += arg.split(',')
    elif opt == '-d':
        compileDebugging = True
    elif opt == '-h':
        usage(0)

if not watchStyle:
    print >> sys.stderr, "You must specify a desired watch style."
    sys.exit(1)

if not targetPlatforms:
    targetPlatforms = [ "aplite", "basalt" ]

watchName, defaultHandStyle, defaultFaceStyle, uuId = watches[watchStyle]

if not handStyle:
    handStyle = defaultHandStyle
if not faceStyle:
    faceStyle = defaultFaceStyle

fd = faces[faceStyle]

faceFilenames = fd.get('filename')
if isinstance(faceFilenames, type('')):
    faceFilenames = [faceFilenames]
numFaces = len(faceFilenames)

date_windows = []
i = 0
ch = chr(97 + i)
dw = getIndicator(fd, 'date_window_%s' % (ch))
while dw[0]:
    date_windows.append(dw)
    i += 1
    ch = chr(97 + i)
    dw = getIndicator(fd, 'date_window_%s' % (ch))

date_window_filename = fd.get('date_window_filename', None)
bluetooth = getIndicator(fd, 'bluetooth')
battery = getIndicator(fd, 'battery')
defaults = fd.get('defaults', [])
centers = fd.get('centers', ())

# Look for 'day' and 'date' prefixes in the defaults.
defaultDateWindows = [0] * len(date_windows)
for keyword in defaults:
    if keyword.startswith('day:'):
        ch = keyword.split(':', 1)[1]
        i = ord(ch) - 97
        defaultDateWindows[i] = 4  # == DWM_weekday
    elif keyword.startswith('date:'):
        ch = keyword.split(':', 1)[1]
        i = ord(ch) - 97
        defaultDateWindows[i] = 2  # == date

# Map the centers tuple into a dictionary of points for x and y.
cxd = dict(map(lambda (hand, x, y): (hand, x), centers))
cyd = dict(map(lambda (hand, x, y): (hand, y), centers))

configWatch()
