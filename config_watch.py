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

    -c
        Enable chronograph mode (if the selected hand style includes
        chrono hands).  This builds the watch as a standard app,
        instead of as a watch face, to activate the chronograph
        buttons.

    -x
        Perform no RLE compression of images.

    -p platform[,platform...]
        Specifies the build platform (aplite, basalt, and/or chalk).

    -d
        Compile for debugging.  Specifically this enables "fast time",
        so the hands move quickly about the face of the watch.  It
        also enables logging.

    -D
        Make a "screenshot build".  This is a nonfunctional watch for
        the purpose of capturing screenshots.  The hands are frozen at
        10:09, and the buttons are active for scrolling through
        different configuration options.

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
rectCenterX, rectCenterY = 144 / 2, 168 / 2
roundCenterX, roundCenterY = 180 / 2, 180 / 2

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
#     (filename, colorMode, asymmetric, pivot, (scale_rect, scale_round))
#
#   hand - the hand type being defined.
#   filename - the png image that defines this hand, pointing upward.
#   colorMode - indicate how the colors in the png file are to be interpreted.
#       1    - foreground pixels are forced to color 1 (red channel)
#       2    - foreground pixels are forced to color 2 (green channel)
#       3    - foreground pixels are forced to color 3 (blue channel)
#       't'  - foreground pixels are drawn in their own color.
#              This doubles the resource cost on Aplite.
#       't%' - As above, but dither the grayscale levels on Aplite.
#   asymmetric - false if the hand is left/right symmetric and can be
#       drawn mirrored, or false if it must be drawn without
#       mirroring, which doubles the resource cost again.
#   pivot - the (x, y) pixel point of the center of rotation of the
#       hand in its image.
#   (scale_rect, scale_round) - a scale factor for reducing the hand to its final size.
#
#  For vectorParams:
#     [(fillType, points, (scale_rect, scale_round)), (fillType, points, (scale_rect, scale_round)), ...]
#
#   fillType - Deprecated.
#   points - a list of points in the vector.  Draw the hand in the
#       vertical position, from the pivot at (0, 0).
#

hands = {
    'a' : [('hour', ('a_hour_hand', 't', False, (90, 410), (0.12, 0.15)), None),
           ('minute', ('a_minute_hand', 't', True, (49, 565), (0.12, 0.15)), None),
           ('second', ('a_second_hand', 2, False, (37, 37), (0.12, 0.15)),
            [(2, [(0, -5), (0, -70)], (1.0, 1.25)),
             ]),
           ],
    'b' : [('hour', ('b_hour_hand', 't', False, (33, 211), (0.27, 0.30)), None),
           ('minute', ('b_minute_hand', 't', False, (24, 292), (0.27, 0.30)), None),
           ('second', ('b_second_hand', 2, False, (32, 21), (0.27, 0.30)),
            [(2, [(0, -5), (0, -75)], (1.0, 1.25)),
             ]),
           ],
    'c' : [('hour', ('c_hour_hand', 't%', False, (59, 434), 0.14), None),
           ('minute', ('c_minute_hand', 't%', False, (38, 584), 0.14), None),
           ('second', ('c_chrono1_hand', 2, False, (32, -27), 0.14),
            [(2, [(0, -2), (0, -26)], (1.0, 1.25)),
             ]),
           ('chrono_minute', ('c_chrono2_hand', 2, False, (37, 195), 0.14), None),
           ('chrono_second', ('c_second_hand', 1, False, (41, -29), 0.14),
            [(1, [(0, -4), (0, -88), (1.0, 1.25)]),
             ]),
           ('chrono_tenth', ('c_chrono2_hand', 2, False, (37, 195), 0.14), None),
           ],
    'd' : [('hour', ('d_hour_hand', 't', False, (24, 193), 0.24), None),
           ('minute', ('d_minute_hand', 't', False, (27, 267), 0.24), None),
           ('second', ('d_second_hand', 1, False, (14, -8), 0.24),
            [(1, [(0, -3), (0, -64), (1.0, 1.25)]),
             ]),
           ],
    'e' : [('hour', ('e_hour_hand', 't%', False, (28, 99), 0.53), None),
           ('minute', ('e_minute_hand', 't%', False, (22, 142), 0.53), None),
           ('second', ('e_second_hand', 't%', False, (18, 269), 0.24), None),
           ],
    }

# Table of face styles.  For each style, specify the following:
#
#   filename  - the background image for the face, or a list of optional faces.
#   aplite_invert - True to invert the color scheme by default on Aplite.
#               Needed to render dark hands on a white background;
#               otherwise, hands will be drawn white (unless they have
#               't' as their color).
#   colors    - for Basalt only, the list of ((bg, c1, c2, c3), (db, d1))
#               colors that define each color scheme.  The first four
#               colors are the bg and three fg colors for the face and
#               hands; the next two colors are the bg and fg colors
#               for the date windows.
#   chrono    - the (tenths, hours) images for the two chrono dials, if used.
#   date_window_a_rect/round - the (x, y, c) position, background of the first date window.
#               The background is either 'b' or 'w', and is relevant
#               only on Aplite; see the colors field, above, for
#               Basalt.  This may be a list of tuples to define a
#               different value for each optional face.
#   date_window_b_rect/round - the (x, y, c) position, background of the second date window.
#   date_window_c_rect/round - etc.
#   date_window_filename - the (window, mask) images shared by all date windows.
#   battery   - the (x, y, c) position and color of the battery gauge, or None.
#   bluetooth - the (x, y, c) position and color of the bluetooth indicator, or None.
#   defaults  - a list of things enabled by default: one or more of 'date:X', 'day:X', 'battery', 'bluetooth', 'second'
#   centers   - a tuple of ((hand, x, y), ...) to indicate the position for
#               each kind of watch hand.  If the tuple is empty or a
#               hand is omitted, the default is the center of the screen.
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
        'filename': ['a_face.png', 'a_face_unrotated.png', 'a_face_clean.png'],
        'aplite_invert' : True,
        'colors' : [ (('Yellow', 'Black', 'DarkCandyAppleRed', 'Black'), ('PastelYellow', 'Black')),
                     (('White', 'Black', 'DarkCandyAppleRed', 'Black'), ('White', 'Black')),
                     (('ElectricBlue', 'DukeBlue', 'DarkCandyAppleRed', 'ElectricUltramarine'), ('BabyBlueEyes', 'Black')),
                     (('MediumSpringGreen', 'Black', 'DarkCandyAppleRed', 'DarkGreen'), ('DarkGreen', 'White')),
                     ],
        'date_window_a_rect': (19, 83, 'b'),
        'date_window_a_round': (21, 87, 'b'),
        'date_window_b_rect': (123, 83, 'b'),
        'date_window_b_round': (151, 87, 'b'),
        'date_window_c_rect' : (52, 109, 'b'),
        'date_window_c_round' : (63, 124, 'b'),
        'date_window_d_rect' : (92, 109, 'b'),
        'date_window_d_round' : (109, 124, 'b'),
        'date_window_filename' : ('date_window.png', 'date_window_mask.png'),
        'top_subdial_rect' : (32, 32, 'b'),
        'top_subdial_round' : (50, 32, 'b'),
        'bluetooth_rect' : [ (37, 47, 'b'), (26, 0, 'b'),
                             (37, 47, 'b'), (26, 0, 'b'),
                             (37, 47, 'b'), (26, 0, 'b'),
                             ],
        'bluetooth_round' : [ (55, 53, 'b'), (30, 30, 'b'),
                              (55, 53, 'b'), (30, 30, 'b'),
                              (55, 53, 'b'), (30, 30, 'b'),
                              ],
        'battery_rect' : [ (92, 51, 'b'), (98, 1, 'b'),
                           (92, 51, 'b'), (98, 1, 'b'),
                           (92, 51, 'b'), (98, 1, 'b'),
                           ],
        'battery_round' : [ (110, 57, 'b'), (132, 33, 'b'),
                            (110, 57, 'b'), (132, 33, 'b'),
                            (110, 57, 'b'), (132, 33, 'b'),
                            ],
        'defaults' : [ 'date:b', 'moon_phase', 'moon_dark', 'second', 'hour_minute_overlap' ],
        },
    'b' : {
        'filename' : ['b_face.png', 'b_face_clean.png'],
        'aplite_invert' : True,
        'colors' : [ (('PictonBlue', 'Black', 'DarkCandyAppleRed', 'Black'), ('BabyBlueEyes', 'Black')),
                     (('White', 'Black', 'DarkCandyAppleRed', 'DukeBlue'), ('White', 'DukeBlue')),
                     (('Yellow', 'BulgarianRose', 'DukeBlue', 'OxfordBlue'), ('PastelYellow', 'Black')),
                     (('BrilliantRose', 'Blue', 'DarkCandyAppleRed', 'BulgarianRose'), ('RichBrilliantLavender', 'Black')),
                     ],
        'date_window_a_rect': (19, 83, 'b'),
        'date_window_a_round': (21, 87, 'b'),
        'date_window_b_rect' : (52, 109, 'b'),
        'date_window_b_round' : (63, 124, 'b'),
        'date_window_c_rect' : (92, 109, 'b'),
        'date_window_c_round' : (109, 124, 'b'),
        'date_window_d_rect': (123, 83, 'b'),
        'date_window_d_round': (151, 87, 'b'),
        'top_subdial_rect' : (32, 33, 'b'),
        'top_subdial_round' : (50, 34, 'b'),
        'date_window_filename' : ('date_window.png', 'date_window_mask.png'),
        'bluetooth_rect' : (0, 0, 'b'),
        'bluetooth_round' : [ (55, 53, 'b'), (30, 30, 'b'),
                              (55, 53, 'b'), (30, 30, 'b'),
                              ],
        'battery_rect' : (125, 3, 'b'),
        'battery_round' : [ (110, 57, 'b'), (132, 33, 'b'),
                            (110, 57, 'b'), (132, 33, 'b'),
                            ],
        'defaults' : [ 'day:b', 'date:c', 'moon_phase', 'second', 'hour_minute_overlap' ],
        },
    'c' : {
        'filename' : ['c_face_rect.png', 'c_face.png'],
        'colors' : [ (('Black', 'White', 'White', 'Yellow'), ('Black', 'White')),
                     (('Yellow', 'Black', 'Blue', 'Red'), ('PastelYellow', 'Black')),
                     (('ElectricUltramarine', 'Black', 'Yellow', 'White'), ('VividCerulean', 'Black')),
                     ],
        'chrono' : ('c_face_chrono_tenths.png', 'c_face_chrono_hours.png'),
        'centers_rect' : (('chrono_minute', 115, 84), ('chrono_tenth', 72, 126), ('second', 29, 84)),
        'centers_round' : (('chrono_minute', 115, 84), ('chrono_tenth', 72, 126), ('second', 29, 84)),
        'date_window_a' : (52, 45, 'b'),
        'date_window_b' : (92, 45, 'b'),
        'date_window_filename' : ('date_window.png', 'date_window_mask.png'),
        'bluetooth' : [ (16, 18, 'b'),
                        (0, 0, 'b'), ],
        'battery' : [ (109, 21, 'b'),
                      (123, 3, 'b'), ],
        'defaults' : [ 'second', 'limit_cache_aplite' ],
        },
    'd' : {
        'filename' : ['d_face.png', 'd_face_white.png'],
        'aplite_invert' : True,
        'colors' : [ (('White', 'BulgarianRose', 'White', 'ElectricBlue'), ('OxfordBlue', 'White')),
                     (('Black', 'DarkGreen', 'BrightGreen', 'PastelYellow'), ('White', 'Black')),
                     (('White', 'DukeBlue', 'Yellow', 'FashionMagenta'), ('DukeBlue', 'White')),
                     ],
        'date_window_a': [ (49, 102, 'w'), (49, 102, 'b') ],
        'date_window_b': [ (95, 102, 'w'), (95, 102, 'b') ],
        'date_window_c' : [ (49, 125, 'w'), (49, 125, 'b') ],
        'date_window_d' : [ (95, 125, 'w'), (95, 125, 'b') ],
        'date_window_filename' : ('date_window.png', 'date_window_mask.png'),
        'top_subdial' : [ (32, 34, 'w'), (32, 34, 'b'), ],
        'bluetooth' : [ (49, 45, 'b'), (36, 29, 'b'),
                        (49, 45, 'b'), (36, 29, 'b') ],
        'battery' : [ (79, 49, 'b'), (95, 33, 'b'),
                      (79, 49, 'b'), (95, 33, 'b') ],
        'defaults' : [ 'day:c', 'date:d', 'bluetooth', 'battery' ],
        },
    'e' : {
        'filename' : ['e_face.png', 'e_face_white.png'],
        'colors' : [ (('Black', 'White', 'Yellow', 'PastelYellow'), ('PastelYellow', 'Black')),
                     (('Black', 'Yellow', 'PastelYellow', 'Yellow'), ('White', 'Black')),                     
                     (('Black', 'BlueMoon', 'ShockingPink', 'White'), ('RichBrilliantLavender', 'Black')),
                     (('Black', 'PastelYellow', 'BabyBlueEyes', 'White'), ('Celeste', 'Black')),
                     (('Black', 'Red', 'Melon', 'White'), ('BrilliantRose', 'Black')),
                     (('Black', 'Magenta', 'Rajah', 'Orange'), ('ImperialPurple', 'White')),                     
                     ],
        'date_window_a' : (72, 21, 'w'),
        'date_window_b' : (21, 82, 'w'),
        'date_window_c' : (123, 82, 'w'),
        'date_window_d' : (72, 146, 'w'),
        'date_window_filename' : ('date_window.png', 'date_window_mask.png'),
        'top_subdial' : (32, 32, 'w'),
        'bluetooth' : [ (11, 12, 'b'), (11, 12, 'b'),
                        (11, 12, 'w'), (11, 12, 'w'), ],
        'battery' : [ (115, 16, 'b'), (115, 16, 'b'),
                      (115, 16, 'w'), (115, 16, 'w'), ],
        'defaults' : [ 'date:c', 'moon_dark', 'second', 'limit_cache_aplite' ],
        },
    }

makeChronograph = False
enableSecondHand = False
enableChronoMinuteHand = False
enableChronoSecondHand = False
enableChronoTenthHand = False
date_windows_rect = []
date_windows_round = []
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
    'moon' : 30,
    }

# If you use the -w option to enable sweep seconds, it means we need a
# greater number of second-hand subdivisions.
numStepsSweep = {
    'second' : 180,
    'chrono_second' : 180,
    }

# This gets populated with the number of bitmap images for each hand
# type, if we are enabling caching.
resourceCacheSize = {}

thresholdMask = [0] + [255] * 255
threshold1Bit = [0] * 128 + [255] * 128
threshold2Bit = [0] * 64 + [85] * 64 + [170] * 64 + [255] * 64

trivialImage = PIL.Image.new('L', (1, 1), 255)

# Attempt to determine the directory in which we're operating.
rootDir = os.path.dirname(__file__) or '.'
resourcesDir = os.path.join(rootDir, 'resources')
buildDir = os.path.join(resourcesDir, 'build')
if not os.path.isdir(buildDir):
    os.mkdir(buildDir)

def formatUuId(uuId):
    return '%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x' % tuple(uuId)

def parseColorMode(colorMode):
    paintChannel = 0
    useTransparency = False
    dither = False

    if colorMode in [1, 2, 3]:
        paintChannel = colorMode
    elif colorMode[0] == 't':
        useTransparency = True
        if colorMode.endswith('%'):
            dither = True

    return paintChannel, useTransparency, dither

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
      "name": "DATE_WINDOW",
      "file": "%(rleFilename)s",
      "type": "%(ptype)s"
    },"""

    dateWindowMaskEntry = """
    {
      "name": "DATE_WINDOW_MASK",
      "file": "%(rleFilename)s",
      "type": "%(ptype)s",
      "targetPlatforms": [
        "aplite"
      ]
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
        
    print >> generatedTable, "struct FaceDef clock_face_table[NUM_FACES] = {"
    for i in range(len(faceFilenames)):
        print >> generatedTable, "  { RESOURCE_ID_CLOCK_FACE_%s }," % (i)

        rleFilename, ptype = make_rle('clock_faces/' + faceFilenames[i], useRle = supportRle)
        resourceStr += faceResourceEntry % {
            'index' : i,
            'rleFilename' : rleFilename,
            'ptype' : ptype,
            }
    print >> generatedTable, "};\n"

    faceColors = fd.get('colors')
    print >> generatedTable, "#ifndef PBL_PLATFORM_APLITE"
    print >> generatedTable, "struct FaceColorDef clock_face_color_table[NUM_FACE_COLORS] = {"
    for i in range(len(faceColors)):
        cb, db = faceColors[i]
        print >> generatedTable, "  { GColor%sARGB8, GColor%sARGB8, GColor%sARGB8, GColor%sARGB8, GColor%sARGB8, GColor%sARGB8 }," % (cb[0], cb[1], cb[2], cb[3], db[0], db[1])
    print >> generatedTable, "};"
    print >> generatedTable, "#endif  // PBL_PLATFORM_APLITE\n"

    if date_windows_rect and date_window_filename:
        window, mask = date_window_filename

        rleFilename, ptype = make_rle('clock_faces/' + window, useRle = supportRle)
        resourceStr += dateWindowEntry % {
            'rleFilename' : rleFilename,
            'ptype' : ptype,
            }

        if mask:
            rleFilename, ptype = make_rle('clock_faces/' + mask, useRle = supportRle)
            resourceStr += dateWindowMaskEntry % {
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

def makeVectorHands(generatedTable, paintChannel, generatedDefs, hand, groupList):
    resourceStr = ''

    colorMap = {
        'b' : '1',
        'w' : '2',
        '' : '0',
        }

    print >> generatedTable, "struct VectorHand %s_hand_vector_table = {" % (hand)
    print >> generatedTable, "  %s," % (paintChannel)
    print >> generatedTable, "  %s, (struct VectorHandGroup[]){" % (len(groupList))
    for fillType, points, (scale_rect, scale_round) in groupList:
        print >> generatedTable, "  { { %s, (GPoint[]){" % (len(points))
        print >> generatedTable, "#ifdef PBL_ROUND"
        for px, py in points:
            print >> generatedTable, "    { %d, %d }," % (px * scale_round, py * scale_round)
        print >> generatedTable, "#else  // PBL_ROUND"
        for px, py in points:
            print >> generatedTable, "    { %d, %d }," % (px * scale_rect, py * scale_rect)
        print >> generatedTable, "#endif  // PBL_ROUND"
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

def getResourceCacheSize(hand):
    return resourceCacheSize.get(hand, [0, 0])[0]

def getMaskResourceCacheSize(hand):
    return resourceCacheSize.get(hand, [0, 0])[1]

def makeBitmapHands(generatedTable, generatedDefs, useRle, hand, sourceBasename, colorMode, asymmetric, pivot, scale):
    if isinstance(scale, type(())):
        scale_rect, scale_round = scale
    else:
        scale_rect = scale
        scale_round = scale

    if 'basalt' in targetPlatforms:
        print >> generatedTable, "#ifdef PBL_PLATFORM_BASALT"
        resourceStr = makeBitmapHandsColor(generatedTable, useRle, hand, sourceBasename, colorMode, asymmetric, pivot, scale_rect, '~color~rect')
        print >> generatedTable, "#endif  // PBL_PLATFORM_BASALT"

    if 'chalk' in targetPlatforms:
        print >> generatedTable, "#ifdef PBL_PLATFORM_CHALK"
        resourceStr = makeBitmapHandsColor(generatedTable, useRle, hand, sourceBasename, colorMode, asymmetric, pivot, scale_round, '~color~round')
        print >> generatedTable, "#endif  // PBL_PLATFORM_CHALK"

    if 'aplite' in targetPlatforms:
        # Aplite comes last, because its resourceStr might include masks.
        print >> generatedTable, "#ifdef PBL_PLATFORM_APLITE"
        resourceStr = makeBitmapHandsAplite(generatedTable, useRle, hand, sourceBasename, colorMode, asymmetric, pivot, scale_rect)
        print >> generatedTable, "#endif  // PBL_PLATFORM_APLITE"

    return resourceStr

def makeBitmapHandsAplite(generatedTable, useRle, hand, sourceBasename, colorMode, asymmetric, pivot, scale):
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

    paintChannel, useTransparency, dither = parseColorMode(colorMode)

    source1Pathname = '%s/clock_hands/%s~bw.png' % (resourcesDir, sourceBasename)
    if os.path.exists(source1Pathname):
        # If there's an explicit ~bw source file, use it for the 1-bit
        # version.
        source1 = PIL.Image.open(source1Pathname)
    else:
        # Otherwise, use the original for the 1-bit version.
        sourcePathname = '%s/clock_hands/%s.png' % (resourcesDir, sourceBasename)
        source1 = PIL.Image.open(sourcePathname)

    r, g, b, source1Mask = source1.convert('RGBA').split()
    source1 = PIL.Image.merge('RGB', [r, g, b])

    # Ensure that the source image is black anywhere its alpha channel
    # is black.
    black = PIL.Image.new('L', source1.size, 0)
    r, g, b = source1.split()
    mask = source1Mask.point(thresholdMask)
    r = PIL.Image.composite(r, black, mask)
    g = PIL.Image.composite(g, black, mask)
    b = PIL.Image.composite(b, black, mask)
    source1 = PIL.Image.merge('RGB', [r, g, b])

    # In the aplite case, we don't need to make a distinction between
    # the explicit and implicit masks.  An explicit mask completely
    # replaces the implicit mask.
    source1MaskPathname = '%s/clock_hands/%s_mask~bw.png' % (resourcesDir, sourceBasename)
    if os.path.exists(source1MaskPathname):
        source1Mask = PIL.Image.open(source1MaskPathname).convert('L')

    # Center the source image on its pivot, and pad it with black.
    border = (pivot[0], pivot[1], source1.size[0] - pivot[0], source1.size[1] - pivot[1])
    size = (max(border[0], border[2]) * 2, max(border[1], border[3]) * 2)
    center = (size[0] / 2, size[1] / 2)
    large1 = PIL.Image.new('RGB', size, 0)
    large1.paste(source1, (center[0] - pivot[0], center[1] - pivot[1]))

    large1Mask = PIL.Image.new('L', size, 0)
    large1Mask.paste(source1Mask, (center[0] - pivot[0], center[1] - pivot[1]))

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
            symbolMaskName = '%s_%s_MASK' % (hand.upper(), i)

        if i not in handLookupLines:
            # Here we have a new rotation of the bitmap image to
            # generate.  We might have decided to flip this image from
            # another image i, but we still need to scale and rotate
            # the source image now, if for no other reason than to
            # compute cx, cy.

            # We expect to encounter each i the first time in an
            # unflipped state, because we visit quadrant I first.
            assert not flip_x and not flip_y

            p1 = large1.rotate(-angle, PIL.Image.BICUBIC, True)
            scaledSize = (int(p1.size[0] * scale + 0.5), int(p1.size[1] * scale + 0.5))
            p1 = p1.resize(scaledSize, PIL.Image.ANTIALIAS)

            # Now make the 1-bit version for Aplite.
            r, g, b = p1.split()
            if not dither:
                p1 = b.point(threshold1Bit).convert('1')
            else:
                p1 = b.convert('1')

            cx, cy = p1.size[0] / 2, p1.size[1] / 2
            cropbox = p1.getbbox()

            # Mask.
            pm1 = large1Mask.rotate(-angle, PIL.Image.BICUBIC, True)
            pm1 = pm1.resize(scaledSize, PIL.Image.ANTIALIAS)

            # And the 1-bit version of the mask.
            if not dither or useTransparency:
                pm1 = pm1.point(threshold1Bit).convert('1')
            else:
                pm1 = pm1.convert('1')

            # It's important to take the crop from the alpha mask, not
            # from the color.
            cropbox = pm1.getbbox()
            p1 = p1.crop(cropbox)
            pm1 = pm1.crop(cropbox)

            cx, cy = cx - cropbox[0], cy - cropbox[1]

            # Now that we have scaled and rotated image i, write it
            # out.

            # We require our images to be an even multiple of 8 pixels
            # wide, to make it easier to reverse the bits
            # horizontally.  (Actually we only need it to be an even
            # multiple of bytes, but the Aplite build is the lowest
            # common denominator with 8 pixels per byte.)
            w = 8 * ((p1.size[0] + 7) / 8)
            if w != p1.size[0]:
                pt = PIL.Image.new('1', (w, p1.size[1]), 0)
                pt.paste(p1, (0, 0))
                p1 = pt

                pt = PIL.Image.new('1', (w, pm1.size[1]), 0)
                pt.paste(pm1, (0, 0))
                pm1 = pt

            if not useTransparency:
                # In the non-transparency case, the aplite mask is the
                # aplite image we actually write out.
                p1 = pm1
            else:
                # In the transparency case, we need to write the mask
                # image separately.
                targetMaskBasename = 'build/flat_%s_%s_%s_mask' % (handStyle, hand, i)

                # Save the aplite mask.
                pm1.save('%s/%s~bw.png' % (resourcesDir, targetMaskBasename))

                rleFilename, ptype = make_rle(targetMaskBasename + '.png', useRle = useRle, modes = ['~bw'])
                maskResourceStr += resourceEntry % {
                    'defName' : symbolMaskName,
                    'targetFilename' : rleFilename,
                    'ptype' : ptype,
                    }

            targetBasename = 'build/flat_%s_%s_%s' % (handStyle, hand, i)
            p1.save('%s/%s~bw.png' % (resourcesDir, targetBasename))
            rleFilename, ptype = make_rle(targetBasename + '.png', useRle = useRle, modes = ['~bw'])

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

    numBitmaps = maxLookupIndex + 1
    if useTransparency:
        numMaskBitmaps = numBitmaps
    else:
        numMaskBitmaps = 0
    resourceCacheSize[hand] = numBitmaps, numMaskBitmaps
    
    print >> generatedTable, "struct BitmapHandCenterRow %s_hand_bitmap_lookup[] = {" % (hand)
    for i in range(numBitmaps):
        line = handLookupLines.get(i, "  {},");
        print >> generatedTable, line
    print >> generatedTable, "};\n"

    print >> generatedTable, "struct BitmapHandTableRow %s_hand_bitmap_table[NUM_STEPS_%s] = {" % (hand, hand.upper())
    for line in handTableLines:
        print >> generatedTable, line
    print >> generatedTable, "};\n"

    return resourceStr + maskResourceStr

def makeBitmapHandsColor(generatedTable, useRle, hand, sourceBasename, colorMode, asymmetric, pivot, scale, mode):
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

    paintChannel, useTransparency, dither = parseColorMode(colorMode)

    sourcePathname = '%s/clock_hands/%s.png' % (resourcesDir, sourceBasename)
    source = PIL.Image.open(sourcePathname)

    r, g, b, sourceMask = source.convert('RGBA').split()
    source = PIL.Image.merge('RGB', [r, g, b])

    # Ensure that the source image is black anywhere its alpha channel
    # is black.
    black = PIL.Image.new('L', source.size, 0)
    r, g, b = source.split()
    mask = sourceMask.point(thresholdMask)
    r = PIL.Image.composite(r, black, mask)
    g = PIL.Image.composite(g, black, mask)
    b = PIL.Image.composite(b, black, mask)
    source = PIL.Image.merge('RGB', [r, g, b])

    # Now check for an explicit mask image.  In the color case, an
    # explicit (separate) mask image is maintained in addition to the
    # implicit (alpha channel) mask above.
    sourceMaskPathname = '%s/clock_hands/%s_mask.png' % (resourcesDir, sourceBasename)
    sourceMaskExplicit = None
    if os.path.exists(sourceMaskPathname):
        sourceMaskExplicit = PIL.Image.open(sourceMaskPathname).convert('RGBA')

    # Center the source image on its pivot, and pad it with black.
    border = (pivot[0], pivot[1], source.size[0] - pivot[0], source.size[1] - pivot[1])
    size = (max(border[0], border[2]) * 2, max(border[1], border[3]) * 2)
    center = (size[0] / 2, size[1] / 2)
    large = PIL.Image.new('RGB', size, 0)
    large.paste(source, (center[0] - pivot[0], center[1] - pivot[1]))

    largeMask = PIL.Image.new('L', size, 0)
    largeMask.paste(sourceMask, (center[0] - pivot[0], center[1] - pivot[1]))

    if sourceMaskExplicit:
        largeMaskExplicit = PIL.Image.new('RGBA', size, (0, 0, 0, 0))
        largeMaskExplicit.paste(sourceMaskExplicit, (center[0] - pivot[0], center[1] - pivot[1]))

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
            symbolMaskName = '%s_%s_MASK' % (hand.upper(), i)

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

            # Now make the 2-bit version for Basalt and Chalk.
            r, g, b = p.split()
            r = r.point(threshold2Bit).convert('L')
            g = g.point(threshold2Bit).convert('L')
            b = b.point(threshold2Bit).convert('L')
            p2 = PIL.Image.merge('RGB', [r, g, b])

            cx, cy = p2.size[0] / 2, p2.size[1] / 2
            cropbox = p2.getbbox()

            # Mask.
            pm = largeMask.rotate(-angle, PIL.Image.BICUBIC, True)
            pm = pm.resize(scaledSize, PIL.Image.ANTIALIAS)

            # And the 2-bit version of the mask.
            pm2 = pm.point(threshold2Bit).convert('L')

            # It's important to take the crop from the alpha mask, not
            # from the color.
            cropbox = pm2.getbbox()
            p2 = p2.crop(cropbox)
            pm2 = pm2.crop(cropbox)

            if sourceMaskExplicit:
                pme = largeMaskExplicit.rotate(-angle, PIL.Image.BICUBIC, True)
                pme = pme.resize(scaledSize, PIL.Image.ANTIALIAS)
                r, g, b, a = pme.split()
                r = r.point(threshold2Bit).convert('L')
                g = g.point(threshold2Bit).convert('L')
                b = b.point(threshold2Bit).convert('L')
                a = a.point(threshold2Bit).convert('L')
                pme2 = PIL.Image.merge('RGBA', [r, g, b, a])
                pme2 = pme2.crop(cropbox)

            if not useTransparency:
                # Force the foreground pixels to the appropriate color
                if paintChannel == 1:
                    p2 = PIL.Image.new('RGB', p2.size, (255, 0, 0))
                elif paintChannel == 2:
                    p2 = PIL.Image.new('RGB', p2.size, (0, 255, 0))
                elif paintChannel == 3:
                    p2 = PIL.Image.new('RGB', p2.size, (0, 0, 255))

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
                pt = PIL.Image.new('RGB', (w, p2.size[1]), 0)
                pt.paste(p2, (0, 0))
                p2 = pt

                pt = PIL.Image.new('L', (w, pm2.size[1]), 0)
                pt.paste(pm2, (0, 0))
                pm2 = pt
                if sourceMaskExplicit:
                    pt = PIL.Image.new('RGBA', (w, pme2.size[1]), (0, 0, 0, 0))
                    pt.paste(pme2, (0, 0))
                    pme2 = pt

            # Apply the mask as the alpha channel.
            r, g, b = p2.split()
            p2 = PIL.Image.merge('RGBA', [r, g, b, pm2])

            # And quantize to 16 colors, which looks almost as good
            # for half the RAM.
            p2 = p2.convert("P", palette = PIL.Image.ADAPTIVE, colors = 16)

            if useTransparency:
                # In the transparency case, we need to write the mask
                # image separately.
                targetMaskBasename = 'build/flat_%s_%s_%s_mask' % (handStyle, hand, i)
                if sourceMaskExplicit:
                    # An explicit color mask.
                    pme2 = pme2.convert("P", palette = PIL.Image.ADAPTIVE, colors = 16)
                    pme2.save('%s/%s%s.png' % (resourcesDir, targetMaskBasename, mode))
                else:
                    # With only an implicit color mask, we won't be
                    # using the mask image in color, so save a
                    # trivial image.
                    trivialImage.save('%s/%s%s.png' % (resourcesDir, targetMaskBasename, mode))

                rleFilename, ptype = make_rle(targetMaskBasename + '.png', useRle = useRle, modes = [mode])
                maskResourceStr += resourceEntry % {
                    'defName' : symbolMaskName,
                    'targetFilename' : rleFilename,
                    'ptype' : ptype,
                    }

            targetBasename = 'build/flat_%s_%s_%s' % (handStyle, hand, i)
            p2.save('%s/%s%s.png' % (resourcesDir, targetBasename, mode))
            rleFilename, ptype = make_rle(targetBasename + '.png', useRle = useRle, modes = [mode])

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

    numBitmaps = maxLookupIndex + 1
    if useTransparency:
        numMaskBitmaps = numBitmaps
    else:
        numMaskBitmaps = 0
    resourceCacheSize[hand] = numBitmaps, numMaskBitmaps
    
    print >> generatedTable, "struct BitmapHandCenterRow %s_hand_bitmap_lookup[] = {" % (hand)
    for i in range(numBitmaps):
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
    #ifdef PBL_ROUND
    %(roundPlaceX)s, %(roundPlaceY)s,
    #else
    %(rectPlaceX)s, %(rectPlaceY)s,
    #endif  // PBL_ROUND
    %(useRle)s,
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
        paintChannel = 0
        bitmapCenters = 'NULL'
        bitmapTable = 'NULL'
        vectorTable = 'NULL'

        if bitmapParams:
            resourceStr += makeBitmapHands(generatedTable, generatedDefs, useRle, hand, *bitmapParams)
            colorMode = bitmapParams[1]
            paintChannel, useTransparency, dither = parseColorMode(colorMode)
            resourceId = 'RESOURCE_ID_%s_0' % (hand.upper())
            resourceMaskId = resourceId
            if useTransparency:
                resourceMaskId = 'RESOURCE_ID_%s_0_MASK' % (hand.upper())
            bitmapCenters = '%s_hand_bitmap_lookup' % (hand)
            bitmapTable = '%s_hand_bitmap_table' % (hand)

        if vectorParams:
            resourceStr += makeVectorHands(generatedTable, paintChannel, generatedDefs, hand, vectorParams)
            vectorTable = '&%s_hand_vector_table' % (hand)

        handDef = handDefEntry % {
            'hand' : hand,
            'handUpper' : hand.upper(),
            'resourceId' : resourceId,
            'resourceMaskId' : resourceMaskId,
            'rectPlaceX' : cxdRect.get(hand, rectCenterX),
            'rectPlaceY' : cydRect.get(hand, rectCenterY),
            'roundPlaceX' : cxdRound.get(hand, roundCenterX),
            'roundPlaceY' : cydRound.get(hand, roundCenterY),
            'useRle' : int(bool(useRle)),
            'bitmapCenters' : bitmapCenters,
            'bitmapTable' : bitmapTable,
            'vectorTable' : vectorTable,
        }
        print >> generatedTable, handDef
        print >> generatedDefs, 'extern struct HandDef %s_hand_def;' % (hand)

    return resourceStr

def getIndicator(fd, indicator):
    """ Gets an indicator tuple from the config dictionary.  Finds
    either a tuple or a list of tuples; in either case returns a pair
    of lists of tuples: rect, round. """

    list = fd.get(indicator, None)
    list_rect = fd.get(indicator + '_rect', list)
    list_round = fd.get(indicator + '_round', list)
    if not isinstance(list_rect, type([])):
        list_rect = [list_rect]
    if not isinstance(list_round, type([])):
        list_round = [list_round]

    return list_rect, list_round

def makeIndicatorTable(generatedTable, generatedDefs, name, (indicator_rect, indicator_round), numFaces, anonymous = False):
    """ Makes an array of IndicatorTable values to define how a given
    indicator (that is, a bluetooth or battery indicator, or a
    day/date window) is meant to be rendered for each of the alternate
    faces. """

    if not indicator_rect[0]:
        return

    if anonymous:
        # Anonymous structure within a table
        print >> generatedTable, "  { // %s" % (name)
    else:
        # Standalone named structure
        print >> generatedDefs, "extern struct IndicatorTable %s[%s];" % (name, numFaces)
        print >> generatedTable, "struct IndicatorTable %s[%s] = {" % (name, numFaces)

    def writeTable(generatedTable, indicator, numFaces):
        if len(indicator) == 1 and numFaces != 1:
            indicator = [indicator[0]] * numFaces

        assert len(indicator) == numFaces

        for x, y, c in indicator:
            print >> generatedTable, "   { %s, %s, %s }," % (
                x, y, int(c[0] == 'b'))

    print >> generatedTable, "#ifdef PBL_ROUND"
    writeTable(generatedTable, indicator_round, numFaces)
    print >> generatedTable, "#else  // PBL_ROUND"
    writeTable(generatedTable, indicator_rect, numFaces)
    print >> generatedTable, "#endif  // PBL_ROUND"

    if anonymous:
        # Anonymous structure within a table
        print >> generatedTable, "  },";
    else:
        print >> generatedTable, "};\n";

def makeMoonDateWindow():
    """ Returns the resource strings needed to include the moon phase
    icons, for the moon date window. """

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

def makeMoonWheel():
    """ Returns the resource strings needed to include the moon wheel
    icons, for the optional moon wheel window. """

    topSubdialEntry = """
    {
      "name": "%(name)s",
      "file": "%(rleFilename)s",
      "type": "%(ptype)s"
    },"""

    moonWheelEntry = """
    {
      "name": "MOON_WHEEL_%(cat)s_%(index)s",
      "file": "%(rleFilename)s",
      "type": "%(ptype)s"
    },"""

    # moon_wheel_white_*.png is for when the moon is to be drawn as white pixels on black (not used on Basalt).
    # moon_wheel_black_*.png is for when the moon is to be drawn as black pixels on white.

    numStepsMoon = getNumSteps('moon')
    wheelSize = 80, 80
    subdialSize = 80, 41

    for mode in [ '', '~bw' ]:
        subdialMaskPathname = '%s/clock_faces/top_subdial_mask%s.png' % (resourcesDir, mode)
        subdialMask = PIL.Image.open(subdialMaskPathname)
        assert subdialMask.size == subdialSize
        subdialMask = subdialMask.convert('L')
        subdialMaskBinary = subdialMask.point(thresholdMask)
        black = PIL.Image.new('L', subdialMask.size, 0)
        white = PIL.Image.new('L', subdialMask.size, 255)

        for cat in ['white', 'black']:
            wheelSourcePathname = '%s/clock_faces/moon_wheel_%s%s.png' % (resourcesDir, cat, mode)
            wheelSource = PIL.Image.open(wheelSourcePathname)

            for i in range(numStepsMoon):
                if cat == 'white' and mode != '~bw':
                    # Ignore the white image on Basalt.
                    p = wheelSource
                else:
                    # Process the image normally.
                    angle = i * 180.0 / numStepsMoon

                    # Rotate the moon wheel to the appropriate angle.
                    p = wheelSource.rotate(-angle, PIL.Image.BICUBIC, True)

                    cx, cy = p.size[0] / 2, p.size[1] / 2
                    px, py = cx - wheelSize[0] / 2, cy - wheelSize[1] / 2
                    cropbox = (px, py, px + subdialSize[0], py + subdialSize[1])
                    p = p.crop(cropbox)

                    # Now make the 1-bit version for Aplite and the 2-bit
                    # version for Basalt.
                    if mode == '~bw':
                        p = p.convert('1')
                        if cat == 'white':
                            # Invert the image for moon_wheel_white.
                            p = PIL.Image.composite(black, white, p)

                        # Now apply the mask.
                        p = PIL.Image.composite(p, black, subdialMask)

                    else:
                        r, g, b = p.split()
                        r = r.point(threshold2Bit).convert('L')
                        g = g.point(threshold2Bit).convert('L')
                        b = b.point(threshold2Bit).convert('L')

                        # Now apply the mask.
                        r = PIL.Image.composite(r, black, subdialMaskBinary)
                        g = PIL.Image.composite(g, black, subdialMaskBinary)
                        b = PIL.Image.composite(b, black, subdialMaskBinary)

                        p = PIL.Image.merge('RGBA', [r, g, b, subdialMask])

                    if mode == '':
                        # And quantize to 16 colors.
                        p = p.convert("P", palette = PIL.Image.ADAPTIVE, colors = 16)

                targetBasename = 'build/rot_moon_wheel_%s_%s%s.png' % (cat, i, mode)
                p.save('%s/%s' % (resourcesDir, targetBasename))


    # Now encode to RLE and build the resource string (which is global
    # rather than dependent on the platform).
    resourceStr = ''

    rleFilename, ptype = make_rle('clock_faces/top_subdial.png', useRle = supportRle)
    resourceStr += topSubdialEntry % {
        'name' : 'TOP_SUBDIAL',
        'rleFilename' : rleFilename,
        'ptype' : ptype,
        }
    rleFilename, ptype = make_rle('clock_faces/top_subdial_mask.png', useRle = supportRle)
    resourceStr += topSubdialEntry % {
        'name' : 'TOP_SUBDIAL_MASK',
        'rleFilename' : rleFilename,
        'ptype' : ptype,
        }
    for cat in ['white', 'black']:
        for i in range(numStepsMoon):
            targetBasename = 'build/rot_moon_wheel_%s_%s.png' % (cat, i)
            rleFilename, ptype = make_rle(targetBasename, useRle = supportRle)
            resourceStr += moonWheelEntry % {
                'cat' : cat.upper(),
                'index' : i,
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
    resourceStr += makeMoonDateWindow()

    if top_subdial_rect[0]:
        resourceStr += makeMoonWheel()

    print >> generatedDefs, "extern struct IndicatorTable date_windows[NUM_DATE_WINDOWS][NUM_FACES];"
    print >> generatedTable, "struct IndicatorTable date_windows[NUM_DATE_WINDOWS][NUM_FACES] = {"
    for i in range(len(date_window_keys)):
        ch = chr(97 + i)
        makeIndicatorTable(generatedTable, generatedDefs, ch, (date_windows_rect[i], date_windows_round[i]), numFaces, anonymous = True)
    print >> generatedTable, "};\n"

    makeIndicatorTable(generatedTable, generatedDefs, 'battery_table', (battery_rect, battery_round), numIndicatorFaces)
    makeIndicatorTable(generatedTable, generatedDefs, 'bluetooth_table', (bluetooth_rect, bluetooth_round), numIndicatorFaces)
    makeIndicatorTable(generatedTable, generatedDefs, 'top_subdial', (top_subdial_rect, top_subdial_round), numFaces)

    resourceIn = open('%s/appinfo.json.in' % (rootDir), 'r').read()
    resource = open('%s/appinfo.json' % (rootDir), 'w')

    watchface = 'true'
    if screenshotBuild:
        watchface = 'false'
    elif makeChronograph and enableChronoSecondHand:
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
        'numFaceColors' : numFaceColors,
        'defaultFaceIndex' : defaultFaceIndex,
        'dateWindowKeys' : date_window_keys,
        'enableChronoDial' : int(makeChronograph),
        'defaultBluetooth' : defaultBluetooth,
        'defaultBattery' : defaultBattery,
        'defaultSecondHand' : int('second' in defaults),
        'defaultHourBuzzer' : int('buzzer' in defaults),
        'enableSweepSeconds' : int(supportSweep),
        'defaultDateWindows' : repr(defaultDateWindows),
        'displayLangLookup' : displayLangLookup,
        'enableTopSubdial' : int(bool(top_subdial_rect[0])),
        'enableDebug' : int(compileDebugging),
        'defaultTopSubdial' : defaultTopSubdial,
        'defaultLunarBackground' : defaultLunarBackground,
        }

    configIn = open('%s/generated_config.h.in' % (resourcesDir), 'r').read()
    config = open('%s/generated_config.h' % (resourcesDir), 'w')

    print >> config, configIn % {
        'persistKey' : 0x5151 + uuId[-1],
        'supportRle' : int(bool(supportRle)),
        'apliteInvert' : int(bool(apliteInvert)),
        'numFaces' : numFaces,
        'numIndicatorFaces' : numIndicatorFaces,
        'numFaceColors' : numFaceColors,
        'defaultFaceIndex' : defaultFaceIndex,
        'numDateWindows' : len(date_window_keys),
        'dateWindowKeys' : date_window_keys,
        'numStepsHour' : numSteps['hour'],
        'numStepsMinute' : numSteps['minute'],
        'numStepsSecond' : getNumSteps('second'),
        'numStepsChronoMinute' : numSteps['chrono_minute'],
        'numStepsChronoSecond' : getNumSteps('chrono_second'),
        'numStepsChronoTenth' : numSteps['chrono_tenth'],
        'numStepsMoon' : numSteps['moon'],
        'hourResourceCacheSize' : getResourceCacheSize('hour'),
        'minuteResourceCacheSize' : getResourceCacheSize('minute'),
        'secondResourceCacheSize' : getResourceCacheSize('second'),
        'chronoMinuteResourceCacheSize' : getResourceCacheSize('chrono_minute'),
        'chronoSecondResourceCacheSize' : getResourceCacheSize('chrono_second'),
        'chronoTenthResourceCacheSize' : getResourceCacheSize('chrono_tenth'),
        'hourMaskResourceCacheSize' : getMaskResourceCacheSize('hour'),
        'minuteMaskResourceCacheSize' : getMaskResourceCacheSize('minute'),
        'secondMaskResourceCacheSize' : getMaskResourceCacheSize('second'),
        'chronoMinuteMaskResourceCacheSize' : getMaskResourceCacheSize('chrono_minute'),
        'chronoSecondMaskResourceCacheSize' : getMaskResourceCacheSize('chrono_second'),
        'chronoTenthMaskResourceCacheSize' : getMaskResourceCacheSize('chrono_tenth'),
        'compileDebugging' : int(compileDebugging),
        'screenshotBuild' : int(screenshotBuild),
        'defaultDateWindows' : repr(defaultDateWindows)[1:-1],
        'enableBluetooth' : int(bool(bluetooth[0])),
        'defaultBluetooth' : defaultBluetooth,
        'enableBatteryGauge' : int(bool(battery[0])),
        'defaultBattery' : defaultBattery,
        'defaultSecondHand' : int('second' in defaults),
        'defaultHourBuzzer' : int('buzzer' in defaults),
        'enableSweepSeconds' : int(supportSweep),
        'makeChronograph' : int(makeChronograph and enableChronoSecondHand),
        'enableTopSubdial' : int(bool(top_subdial_rect[0])),
        'defaultTopSubdial' : defaultTopSubdial,
        'defaultLunarBackground' : defaultLunarBackground,
        'enableChronoMinuteHand' : int(enableChronoMinuteHand),
        'enableChronoSecondHand' : int(enableChronoSecondHand),
        'enableChronoTenthHand' : int(enableChronoTenthHand),
        'hourMinuteOverlap' : int('hour_minute_overlap' in defaults),
        'limitResourceCacheAplite' : int('limit_cache_aplite' in defaults),
        }


# Main.
try:
    opts, args = getopt.getopt(sys.argv[1:], 's:H:F:ciwm:xp:dDh')
except getopt.error, msg:
    usage(1, msg)

watchStyle = None
handStyle = None
faceStyle = None
supportSweep = False
compileDebugging = False
screenshotBuild = False
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
    elif opt == '-c':
        makeChronograph = True
    elif opt == '-w':
        supportSweep = True
    elif opt == '-x':
        supportRle = False
    elif opt == '-p':
        targetPlatforms += arg.split(',')
    elif opt == '-d':
        compileDebugging = True
    elif opt == '-D':
        screenshotBuild = True
    elif opt == '-h':
        usage(0)

if not watchStyle:
    print >> sys.stderr, "You must specify a desired watch style."
    sys.exit(1)

if not targetPlatforms:
    targetPlatforms = [ "aplite", "basalt", "chalk" ]

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

apliteInvert = fd.get('aplite_invert', False)

faceColors = fd.get('colors')
numFaceColors = len(faceColors)

defaultFaceIndex = fd.get('default_face', 0)

top_subdial_rect, top_subdial_round = getIndicator(fd, 'top_subdial')
if top_subdial_rect[0]:
    numIndicatorFaces = numFaces * 2
else:
    numIndicatorFaces = numFaces

# Get the set of date_windows defined for the watchface.
date_windows_rect = []
date_windows_round = []
date_window_keys = ''
for key in 'abcd':
    dw_rect, dw_round = getIndicator(fd, 'date_window_%s' % (key))
    if dw_rect[0] and dw_round[0]:
        date_windows_rect.append(dw_rect)
        date_windows_round.append(dw_round)
        date_window_keys += key

date_window_filename = fd.get('date_window_filename', None)
bluetooth_rect, bluetooth_round = getIndicator(fd, 'bluetooth')
battery_rect, battery_round = getIndicator(fd, 'battery')
defaults = fd.get('defaults', [])
centers_rect = fd.get('centers_rect', ())
centers_round = fd.get('centers_round', ())

# Look for 'day' and 'date' prefixes in the defaults.
defaultDateWindows = [0] * len(date_window_keys)
for keyword in defaults:
    for token, value in [('day', 4), ('date', 2), ('moon', 7)]:
        if keyword.startswith(token + ':'):
            ch = keyword.split(':', 1)[1]
            i = ord(ch) - 97
            defaultDateWindows[i] = value
            break

defaultTopSubdial = 0
if 'moon_phase' in defaults:
    defaultTopSubdial = 1

defaultLunarBackground = 0
if 'moon_dark' in defaults:
    defaultLunarBackground = 1

# Map the centers tuple into a dictionary of points for x and y.
cxdRect = dict(map(lambda (hand, x, y): (hand, x), centers_rect))
cydRect = dict(map(lambda (hand, x, y): (hand, y), centers_rect))
cxdRound = dict(map(lambda (hand, x, y): (hand, x), centers_round))
cydRound = dict(map(lambda (hand, x, y): (hand, y), centers_round))

configWatch()
