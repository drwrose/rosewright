#! /usr/bin/env python

import PIL.Image
import PIL.ImageChops
import sys
import os
import getopt
from resources.make_rle import make_rle, make_rle_trans
from resources.peb_platform import getPlatformShape, getPlatformColor, getPlatformFilename, getPlatformFilenameAndVariant, screenSizes

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

    -x
        Perform RLE compression of images.

    -p platform[,platform...]
        Specifies the build platform (aplite, basalt, chalk, diorite, emery).

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

# [fill_rect, bar_rect, (font, vshift)] where rect is (x, y, w, h)
batteryGaugeSizes = {
    'rect' : [(6, 0, 18, 10), (10, 3, 10, 4), ('GOTHIC_14', -4)],
    'round' : [(6, 0, 18, 10), (10, 3, 10, 4), ('GOTHIC_14', -4)],
    'emery' : [(8, 0, 25, 14), (13, 3, 15, 8), ('GOTHIC_18', -5)],
    }

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
    'c' : ('Rosewright C', 'c', 'c', [0xA4, 0x9C, 0x82, 0xFD, 0x83, 0x0E, 0x48, 0xB4, 0xA8, 0x2E, 0x9C, 0xF8, 0xDA, 0x77, 0xF4, 0xCA]),
    'c2' : ('Rosewright Chronograph', 'c2', 'c2', [0xA4, 0x9C, 0x82, 0xFD, 0x83, 0x0E, 0x48, 0xB4, 0xA8, 0x2E, 0x9C, 0xF8, 0xDA, 0x77, 0xF4, 0xC7]),
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
#       1    - foreground pixels are forced to color 1 (red channel)
#       2    - foreground pixels are forced to color 2 (green channel)
#       3    - foreground pixels are forced to color 3 (blue channel)
#       't'  - foreground pixels are drawn in their own color.
#              This doubles the resource cost on B&W platforms.
#       't%' - As above, but dither the grayscale levels on B&W.
#   asymmetric - false if the hand is left/right symmetric and can be
#       drawn mirrored, or false if it must be drawn without
#       mirroring, which doubles the resource cost again.
#   pivot - the (x, y) pixel point of the center of rotation of the
#       hand in its image.
#   scale - a scale factor for reducing the hand to its final size.
#
#  For vectorParams:
#     [(fillType, points, scale), (fillType, points, scale), ...]
#
#   fillType - Deprecated.
#   points - a list of points in the vector.  Draw the hand in the
#       vertical position, from the pivot at (0, 0).
#

hands = {
    'a_hands' : [('hour', ('a_hour_hand', 't', False, (90, 410), 0.12), None),
                 ('minute', ('a_minute_hand', 't', True, (49, 565), 0.12), None),
                 ('second', ('a_second_hand', 2, False, (37, 37), 0.12),
                  [(2, [(0, -5), (0, -70)], 1.0),
                   ]),
                 ],
    'a_scale' : { 'rect' : 1.0,
                  'round' : 1.19,
                  'emery' : 1.36,
                  },
    'b_hands' : [('hour', ('b_hour_hand', 't', False, (33, 211), 0.27), None),
                 ('minute', ('b_minute_hand', 't', False, (24, 292), 0.27), None),
                 ('second', ('b_second_hand', 2, False, (32, 21), 0.27),
                  [(2, [(0, -5), (0, -75)], 1.0),
                   ]),
                 ],
    'b_scale' : { 'rect' : 1.0,
                  'round' : 1.11,
                  'emery' : 1.36,
                  },
    'c_hands' : [('hour', ('c_hour_hand', 't%', False, (59, 434), 0.14), None),
                 ('minute', ('c_minute_hand', 't%', False, (38, 584), 0.14), None),
                 ('second', ('c_chrono1_hand', 2, False, (32, -27), 0.14),
                  [(2, [(0, -2), (0, -26)], 1.0),
                   ]),
                 ('chrono_minute', ('c_chrono2_hand', 2, False, (37, 195), 0.14), None),
                 ('chrono_tenth', ('c_chrono2_hand', 2, False, (37, 195), 0.14), None),
                 ],
    'c_scale' : { 'rect' : 1.0,
                  'round' : 1.05,
                  'emery' : 1.36,
                  },
    'c2_hands' : [('hour', ('c_hour_hand', 't%', False, (59, 434), 0.14), None),
                  ('minute', ('c_minute_hand', 't%', False, (38, 584), 0.14), None),
                  ('second', ('c_chrono1_hand', 2, False, (32, -27), 0.14),
                   [(2, [(0, -2), (0, -26)], 1.0),
                    ]),
                  ('chrono_minute', ('c_chrono2_hand', 2, False, (37, 195), 0.14), None),
                  ('chrono_second', ('c_second_hand', 1, False, (41, -29), 0.14),
                   [(1, [(0, -4), (0, -88)], 1.0),
                    ]),
                  ('chrono_tenth', ('c_chrono2_hand', 2, False, (37, 195), 0.14), None),
                  ],
    'c2_scale' : { 'rect' : 1.0,
                   'round' : 1.05,
                   'emery' : 1.36,
                   },
    'd_hands' : [('hour', ('d_hour_hand', 't', False, (24, 193), 0.24), None),
                 ('minute', ('d_minute_hand', 't', False, (27, 267), 0.24), None),
                 ('second', ('d_second_hand', 1, False, (14, -8), 0.24),
                  [(1, [(0, -3), (0, -64)], 1.0),
                   ]),
                 ],
    'd_scale' : { 'rect' : 1.0,
                  'round' : 1.17,
                  'emery' : 1.36,
                  },
    'e_hands' : [('hour', ('e_hour_hand', 't%', False, (28, 99), 0.53), None),
                 ('minute', ('e_minute_hand', 't%', False, (22, 142), 0.53), None),
                 ('second', ('e_second_hand', 't%', False, (18, 269), 0.24), None),
                 ],
    'e_scale' : { 'rect' : 1.0,
                  'round' : 1.13,
                  'emery' : 1.36,
                  },
    }

# Table of face styles.  For each style, specify the following:
#
#   filename  - the background image for the face, or a list of optional faces.
#   bw_invert - True to invert the color scheme by default on B&W.
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
#               only on B&W platforms; see the colors field, above, for
#               color platforms.  This may be a list of tuples to define a
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
        'bw_invert' : True,
        'colors' : [ (('Yellow', 'Black', 'DarkCandyAppleRed', 'Black'), ('PastelYellow', 'Black')),
                     (('White', 'Black', 'DarkCandyAppleRed', 'Black'), ('White', 'Black')),
                     (('ElectricBlue', 'DukeBlue', 'DarkCandyAppleRed', 'ElectricUltramarine'), ('BabyBlueEyes', 'Black')),
                     (('MediumSpringGreen', 'Black', 'DarkCandyAppleRed', 'DarkGreen'), ('DarkGreen', 'White')),
                     ],
        'date_window_a_rect': (2, 75, 'b'),
        'date_window_b_rect': (106, 75, 'b'),
        'date_window_c_rect': (35, 101, 'b'),
        'date_window_d_rect': (75, 101, 'b'),
        'date_window_a_round': (4, 79, 'b'),
        'date_window_b_round': (134, 79, 'b'),
        'date_window_c_round': (46, 116, 'b'),
        'date_window_d_round': (92, 116, 'b'),
        'date_window_a_emery' : (3, 102, 'b'),
        'date_window_b_emery' : (149, 102, 'b'),
        'date_window_c_emery' : (50, 137, 'b'),
        'date_window_d_emery' : (104, 137, 'b'),
        'date_window_filename' : ('date_window.png', 'date_window_mask.png'),
        'top_subdial_rect' : (32, 32, 'b'),
        'top_subdial_round' : (50, 32, 'b'),
        'top_subdial_emery' : (50, 43, 'b'),
        'bluetooth_rect' : [ (37, 47, 'b'), (17, 29, 'b'),
                             (37, 47, 'b'), (17, 29, 'b'),
                             (37, 47, 'b'), (17, 29, 'b'),
                             ],
        'battery_rect' : [ (86, 51, 'b'), (104, 32, 'b'),
                           (86, 51, 'b'), (104, 32, 'b'),
                           (86, 51, 'b'), (104, 32, 'b'),
                           ],
        'bluetooth_round' : [ (55, 53, 'b'), (30, 30, 'b'),
                              (55, 53, 'b'), (30, 30, 'b'),
                              (55, 53, 'b'), (30, 30, 'b'),
                              ],
        'battery_round' : [ (104, 57, 'b'), (126, 33, 'b'),
                            (104, 57, 'b'), (126, 33, 'b'),
                            (104, 57, 'b'), (126, 33, 'b'),
                            ],
        'bluetooth_emery' : [ (50, 64, 'b'), (23, 39, 'b'),
                              (50, 64, 'b'), (23, 39, 'b'),
                              (50, 64, 'b'), (23, 39, 'b'),
                              ],
        'battery_emery' : [ (121, 69, 'b'), (145, 43, 'b'),
                            (121, 69, 'b'), (145, 43, 'b'),
                            (121, 69, 'b'), (145, 43, 'b'),
                            ],
        'defaults' : [ 'date:b', 'moon_phase', 'moon_dark', 'second', 'hour_minute_overlap', 'sweep' ],
        },
    'b' : {
        'filename' : ['b_face.png', 'b_face_clean.png'],
        'bw_invert' : True,
        'colors' : [ (('PictonBlue', 'Black', 'DarkCandyAppleRed', 'Black'), ('BabyBlueEyes', 'Black')),
                     (('White', 'Black', 'DarkCandyAppleRed', 'DukeBlue'), ('White', 'DukeBlue')),
                     (('Yellow', 'BulgarianRose', 'DukeBlue', 'OxfordBlue'), ('PastelYellow', 'Black')),
                     (('BrilliantRose', 'Blue', 'DarkCandyAppleRed', 'BulgarianRose'), ('RichBrilliantLavender', 'Black')),
                     ],
        'date_window_a_rect': (2, 75, 'b'),
        'date_window_b_rect': (106, 75, 'b'),
        'date_window_c_rect': [ (35, 101, 'b'), (35, 101, 'b'),
                                (35, 109, 'b'), (35, 109, 'b'),
                                ],
        'date_window_d_rect': [ (75, 101, 'b'), (75, 101, 'b'),
                                (75, 109, 'b'), (75, 109, 'b'),
                                ],
        'date_window_a_round': (4, 79, 'b'),
        'date_window_b_round': (134, 79, 'b'),
        'date_window_c_round': (46, 116, 'b'),
        'date_window_d_round': (92, 116, 'b'),
        'date_window_a_emery' : (3, 102, 'b'),
        'date_window_b_emery' : (149, 102, 'b'),
        'date_window_c_emery' : [ (50, 137, 'b'), (50, 137, 'b'),
                                  (50, 148, 'b'), (50, 148, 'b'),
                                  ],
        'date_window_d_emery' : [ (104, 137, 'b'), (104, 137, 'b'),
                                  (104, 148, 'b'), (104, 148, 'b'),
                                  ],
        'top_subdial_rect' : [ (32, 33, 'b'), (32, 25, 'b') ],
        'top_subdial_round' : (50, 34, 'b'),
        'top_subdial_emery' : [(50, 45, 'b'), (50, 34, 'b')],
        'date_window_filename' : ('date_window.png', 'date_window_mask.png'),
        'bluetooth_rect' : (14, 18, 'b'),
        'battery_rect' : (110, 22, 'b'),
        'bluetooth_round' : [ (55, 53, 'b'), (30, 30, 'b'),
                              (55, 53, 'b'), (30, 30, 'b'),
                              ],
        'battery_round' : [ (104, 57, 'b'), (126, 33, 'b'),
                            (104, 57, 'b'), (126, 33, 'b'),
                            ],
        'bluetooth_emery' : (19, 24, 'b'),
        'battery_emery' : (153, 30, 'b'),
        'defaults' : [ 'day:c', 'date:d', 'second', 'hour_minute_overlap', 'pebble_label', 'sweep' ],
        },
    'c' : {
        'filename' : ['c_face.png'],
        'colors' : [ (('Black', 'White', 'White', 'Yellow'), ('Black', 'White')),
                     (('Yellow', 'Black', 'Blue', 'Red'), ('PastelYellow', 'Black')),
                     (('ElectricUltramarine', 'Black', 'Yellow', 'White'), ('VividCerulean', 'Black')),
                     ],
        'chrono' : ('c_face_chrono_tenths.png', 'c_face_chrono_hours.png'),
        'centers' : { 'rect' : { 'chrono_minute': (115, 84),
                                 'chrono_tenth' : (72, 126),
                                 'second': (29, 84) },
                      'round' : { 'chrono_minute' : (135, 90),
                                  'chrono_tenth' : (90, 135),
                                  'second' : (45, 90), },
                      'emery' : { 'chrono_minute' : (158, 114),
                                  'chrono_tenth' : (100, 171),
                                  'second' : (42, 114), },
                      },
        'date_window_a_rect' : [ (35, 37, 'b'), (5, 128, 'b') ],
        'date_window_b_rect' : [ (75, 37, 'b'), (102, 128, 'b') ],
        'date_window_a_round' : [ (45, 36, 'b'), (19, 120, 'b') ],
        'date_window_b_round' : [ (94, 36, 'b'), (119, 120, 'b') ],
        'date_window_a_emery' : [ (50, 50, 'b'), (7, 174, 'b') ],
        'date_window_b_emery' : [ (104, 50, 'b'), (143, 174, 'b') ],
        'date_window_filename' : ('date_window.png', 'date_window_mask.png'),
        'top_subdial_rect' : (32, 16, 'b'),
        'top_subdial_round' : (50, 20, 'b'),
        'top_subdial_emery' : (50, 22, 'b'),
        'bluetooth_rect' : (16, 18, 'b'),
        'battery_rect' : (103, 21, 'b'),
        'bluetooth_round' : (33, 41, 'b'),
        'battery_round' : (121, 49, 'b'),
        'bluetooth_emery' : (22, 24, 'b'),
        'battery_emery' : (144, 29, 'b'),
        'defaults' : [ 'second', 'pebble_label' ],
        },
    'c2' : {
        'filename' : ['c_face.png'],
        'colors' : [ (('Black', 'White', 'White', 'Yellow'), ('Black', 'White')),
                     (('Yellow', 'Black', 'Blue', 'Red'), ('PastelYellow', 'Black')),
                     (('ElectricUltramarine', 'Black', 'Yellow', 'White'), ('VividCerulean', 'Black')),
                     ],
        'chrono' : ('c_face_chrono_tenths.png', 'c_face_chrono_hours.png'),
        'centers' : { 'rect' : { 'chrono_minute': (115, 84),
                                 'chrono_tenth' : (72, 126),
                                 'second': (29, 84) },
                      'round' : { 'chrono_minute' : (135, 90),
                                  'chrono_tenth' : (90, 135),
                                  'second' : (45, 90), },
                      'emery' : { 'chrono_minute' : (158, 114),
                                  'chrono_tenth' : (100, 171),
                                  'second' : (42, 114), },
                      },
        'date_window_a_rect' : [ (35, 37, 'b'), (5, 128, 'b') ],
        'date_window_b_rect' : [ (75, 37, 'b'), (102, 128, 'b') ],
        'date_window_a_round' : [ (45, 36, 'b'), (19, 120, 'b') ],
        'date_window_b_round' : [ (94, 36, 'b'), (119, 120, 'b') ],
        'date_window_a_emery' : [ (50, 50, 'b'), (7, 174, 'b') ],
        'date_window_b_emery' : [ (104, 50, 'b'), (143, 174, 'b') ],
        'date_window_filename' : ('date_window.png', 'date_window_mask.png'),
        'top_subdial_rect' : (32, 16, 'b'),
        'top_subdial_round' : (50, 20, 'b'),
        'top_subdial_emery' : (50, 22, 'b'),
        'bluetooth_rect' : (16, 18, 'b'),
        'battery_rect' : (103, 21, 'b'),
        'bluetooth_round' : (33, 41, 'b'),
        'battery_round' : (121, 49, 'b'),
        'bluetooth_emery' : (22, 24, 'b'),
        'battery_emery' : (144, 29, 'b'),
        'defaults' : [ 'second', 'limit_cache_aplite', 'prebake_label', 'pebble_label' ],
        },
    'd' : {
        'filename' : ['d_face.png', 'd_face_white.png', 'd_face_clean.png'],
        'bw_invert' : True,
        'colors' : [ (('White', 'BulgarianRose', 'White', 'ElectricBlue'), ('OxfordBlue', 'White')),
                     (('Black', 'DarkGreen', 'BrightGreen', 'PastelYellow'), ('White', 'Black')),
                     (('White', 'DukeBlue', 'Yellow', 'FashionMagenta'), ('DukeBlue', 'White')),
                     ],
        'date_window_a_rect': [ (32, 94, 'w'), (32, 94, 'b'), (32, 94, 'w'),],
        'date_window_b_rect': [ (78, 94, 'w'), (78, 94, 'b'), (78, 94, 'w'),],
        'date_window_c_rect' : [ (32, 117, 'w'), (32, 117, 'b'), (32, 117, 'w'),],
        'date_window_d_rect' : [ (78, 117, 'w'), (78, 117, 'b'), (78, 117, 'w'),],
        'date_window_a_round': [ (38, 92, 'w'), (38, 92, 'b'), (38, 92, 'w'),],
        'date_window_b_round': [ (100, 92, 'w'), (100, 92, 'b'), (100, 92, 'w'),],
        'date_window_c_round' : [ (46, 115, 'w'), (46, 115, 'b'), (46, 115, 'w'),],
        'date_window_d_round' : [ (92, 115, 'w'), (92, 115, 'b'), (92, 115, 'w'),],
        'date_window_a_emery' : [ (46, 127, 'w'), (46, 127, 'b'), (46, 127, 'w') ],
        'date_window_b_emery' : [ (108, 127, 'w'), (108, 127, 'b'), (108, 127, 'w') ],
        'date_window_c_emery' : [ (46, 159, 'w'), (46, 159, 'b'), (46, 159, 'w') ],
        'date_window_d_emery' : [ (108, 159, 'w'), (108, 159, 'b'), (108, 159, 'w') ],
        'date_window_filename' : ('date_window.png', 'date_window_mask.png'),
        'top_subdial_rect' : [ (32, 34, 'w'), (32, 34, 'b'), (32, 34, 'w') ],
        'top_subdial_round' : [ (50, 34, 'w'), (50, 34, 'b'), (50, 34, 'w') ],
        'top_subdial_emery' : [ (50, 46, 'w'), (50, 46, 'b'), (50, 46, 'w') ],
        'bluetooth_rect' : [ (43, 34, 'b'), (36, 29, 'b'),
                             (43, 34, 'b'), (36, 29, 'b'),
                             (43, 34, 'b'), (36, 29, 'b'),
                             ],
        'battery_rect' : [ (77, 39, 'b'), (89, 33, 'b'),
                           (77, 39, 'b'), (89, 33, 'b'),
                           (77, 39, 'b'), (89, 33, 'b'),
                           ],
        'bluetooth_round' : [ (60, 48, 'b'), (46, 43, 'b'),
                              (60, 48, 'b'), (46, 43, 'b'),
                              (60, 48, 'b'), (46, 43, 'b'),
                              ],
        'battery_round' : [ (97, 52, 'b'), (115, 47, 'b'),
                            (97, 52, 'b'), (115, 47, 'b'),
                            (97, 52, 'b'), (115, 47, 'b'),
                            ],
        'bluetooth_emery' : [ (61, 46, 'b'), (49, 39, 'b'),
                              (61, 46, 'b'), (49, 39, 'b'),
                              (61, 46, 'b'), (49, 39, 'b'),
                              ],
        'battery_emery' : [ (107, 53, 'b'), (125, 45, 'b'),
                            (107, 53, 'b'), (125, 45, 'b'),
                            (107, 53, 'b'), (125, 45, 'b'),
                            ],
        'defaults' : [ 'day:c', 'date:d', 'bluetooth', 'battery', 'sweep' ],
        },
    'e' : {
        'filename' : ['e_face.png', 'e_face_white.png', 'e_face_clean.png'],
        'colors' : [ (('Black', 'White', 'Yellow', 'PastelYellow'), ('PastelYellow', 'Black')),
                     (('Black', 'Yellow', 'PastelYellow', 'Yellow'), ('White', 'Black')),
                     (('Black', 'BlueMoon', 'ShockingPink', 'White'), ('RichBrilliantLavender', 'Black')),
                     (('Black', 'PastelYellow', 'BabyBlueEyes', 'White'), ('Celeste', 'Black')),
                     (('Black', 'Red', 'Melon', 'White'), ('White', 'Black')),
                     (('Black', 'Magenta', 'Rajah', 'Orange'), ('ImperialPurple', 'White')),
                     ],
        'date_window_a_rect' : (55, 13, 'w'),
        'date_window_b_rect' : (4, 74, 'w'),
        'date_window_c_rect' : (106, 74, 'w'),
        'date_window_d_rect' : (55, 138, 'w'),
        'date_window_a_round' : (69, 11, 'w'),
        'date_window_b_round' : (11, 78, 'w'),
        'date_window_c_round' : (128, 78, 'w'),
        'date_window_d_round' : (69, 144, 'w'),
        'date_window_a_emery' : (77, 16, 'w'),
        'date_window_b_emery' : (5, 100, 'w'),
        'date_window_c_emery' : (149, 100, 'w'),
        'date_window_d_emery' : (77, 187, 'w'),
        'date_window_filename' : ('date_window.png', 'date_window_mask.png'),
        'top_subdial_rect' : (32, 32, 'b'),
        'top_subdial_round' : (50, 32, 'b'),
        'top_subdial_emery' : (50, 43, 'b'),
        'bluetooth_rect' : [ (11, 12, 'b'), (11, 12, 'w'), (11, 12, 'b'), ],
        'battery_rect' : [ (109, 16, 'b'), (109, 16, 'w'), (109, 16, 'b'), ],
        'bluetooth_round' : [ (26, 26, 'b'), (26, 26, 'w'), (26, 26, 'b'), ],
        'battery_round' : [ (133, 30, 'b'), (133, 30, 'w'), (133, 30, 'b'), ],
        'bluetooth_emery' : [ (15, 16, 'b'), (15, 16, 'w'), (15, 16, 'b'), ],
        'battery_emery' : [ (150, 22, 'b'), (150, 22, 'w'), (150, 22, 'b'), ],
        'defaults' : [ 'date:c', 'moon_dark', 'limit_cache_aplite', 'limit_cache_basalt', 'limit_cache_chalk' ],
        },
    }


"""
def scaleIndicatorCoord(v, oldIndicatorSize, oldScreenSize, newIndicatorSize, newScreenSize, scale):
    if v + oldIndicatorSize / 2 < oldScreenSize / 3:
        # Close to the left (top) wall.
        v *= scale
    elif v + oldIndicatorSize / 2 > oldScreenSize * 2 / 3:
        # Close to the right (bottom) wall.
        v = (oldScreenSize - (v + oldIndicatorSize))
        v *= scale
        v = (newScreenSize - (v + newIndicatorSize))
    else:
        # Somewhere centered.
        v += (oldIndicatorSize / 2.0 - oldScreenSize / 2.0)
        v *= scale
        v -= (newIndicatorSize / 2.0 - newScreenSize / 2.0)

    v = int(v + 0.5)
    return v


def scaleIndicatorRect(list_shape, oldIndicatorSize, oldScreenSize, newIndicatorSize, newScreenSize):
    x, y, c = list_shape
    scale = newScreenSize[1] / float(oldScreenSize[1])
    x = scaleIndicatorCoord(x, oldIndicatorSize[0], oldScreenSize[0], newIndicatorSize[0], newScreenSize[0], scale)
    y = scaleIndicatorCoord(y, oldIndicatorSize[1], oldScreenSize[1], newIndicatorSize[1], newScreenSize[1], scale)
    return (x, y, c)

def scaleIndicator(key, list_shape, oldIndicatorSize, oldScreenSize, newIndicatorSize, newScreenSize):
    if not isinstance(list_shape, type([])):
        list_shape = scaleIndicatorRect(list_shape, oldIndicatorSize, oldScreenSize, newIndicatorSize, newScreenSize)
        print "  '%s' : %s," % (key, list_shape)
    else:
        result = []
        for indicator in list_shape:
            result.append(scaleIndicatorRect(indicator, oldIndicatorSize, oldScreenSize, newIndicatorSize, newScreenSize))
        print "  '%s' : %s," % (key, result)


def scaleIndicators():
    oldScreenSize = (144, 168)
    newScreenSize = (200, 228)

    for faceStyle in ['a', 'b', 'c', 'c2', 'd', 'e']:
        print faceStyle
        fd = faces[faceStyle]

        # Date windows
        oldIndicatorSize = (37, 19)
        newIndicatorSize = (50, 26)
        for key in 'abcd':
            dw = fd.get('date_window_%s_rect' % (key), None)
            if dw:
                scaleIndicator('date_window_%s_emery' % (key), dw, oldIndicatorSize, oldScreenSize, newIndicatorSize, newScreenSize)

        # Top subdial
        oldIndicatorSize = (80, 41)
        newIndicatorSize = (100, 53)
        top_subdial = fd.get('top_subdial_rect')
        scaleIndicator('top_subdial_emery', top_subdial, oldIndicatorSize, oldScreenSize, newIndicatorSize, newScreenSize)

        # Bluetooth indicator
        oldIndicatorSize = (18, 18)
        newIndicatorSize = (24, 24)
        bluetooth = fd.get('bluetooth_rect')
        scaleIndicator('bluetooth_emery', bluetooth, oldIndicatorSize, oldScreenSize, newIndicatorSize, newScreenSize)

        # Battery gauge
        oldIndicatorSize = (24, 10)
        newIndicatorSize = (33, 14)
        battery = fd.get('battery_rect')
        scaleIndicator('battery_emery', battery, oldIndicatorSize, oldScreenSize, newIndicatorSize, newScreenSize)

    print "c2_chrono_hands"
    oldIndicatorSize = (1, 1)
    newIndicatorSize = (1, 1)
    for hand in ['chrono_minute', 'chrono_tenth', 'second']:
        x, y = faces['c2']['centers']['rect'][hand]
        tuple = (x, y, 'b')
        scaleIndicator(hand, tuple, oldIndicatorSize, oldScreenSize, newIndicatorSize, newScreenSize)
"""

enableSecondHand = False
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

def applyLabel(outputFilename, faceIndex, platforms = None):
    """ Applies the "Pebble" label to the clock face image.  Reads
    faceFilename and applies pebble_label.png onto it, writing the
    result to outputFilename. """

    sourceFaceFilename = 'clock_faces/' + faceFilenames[faceIndex]
    outputBasename, outputExt = os.path.splitext(outputFilename)

    prefix = resourcesDir + '/'

    for platform in platforms:
        color = getPlatformColor(platform)
        faceFilename, faceVariant = getPlatformFilenameAndVariant(prefix + sourceFaceFilename, platform)
        face = PIL.Image.open(faceFilename)

        labelFilename = getPlatformFilename(prefix + 'clock_faces/pebble_label.png', platform)
        label = PIL.Image.open(labelFilename)

        top_subdial_placement = expandIndicatorList(top_subdial[platform], numIndicatorFaces)[faceIndex]
        pebble_label_offset_x = ((subdialSizes[platform][0] - pebbleLabelSizes[platform][0]) / 2)
        pebble_label_offset_y = ((subdialSizes[platform][1] - pebbleLabelSizes[platform][1]) / 2)

        x = top_subdial_placement[0] + pebble_label_offset_x
        y = top_subdial_placement[1] + pebble_label_offset_y

        if color == 'bw':
            # For bw platforms, load the mask separately.
            maskFilename = getPlatformFilename(prefix + 'clock_faces/pebble_label_mask.png', platform)
            mask = PIL.Image.open(maskFilename)
        else:
            # For color platforms, extract the mask from the alpha channel.
            r, g, b, mask = label.convert('RGBA').split()
            label = PIL.Image.merge('RGB', [r, g, b])

        face.paste(label, box = (x, y), mask = mask)

        outputFilename = prefix + outputBasename + faceVariant + outputExt
        face.save(outputFilename)

def makeFaces(generatedTable, generatedDefs):

    resourceStr = ''

    fd = faces[faceStyle]
    faceFilenames = fd.get('filename')
    if isinstance(faceFilenames, type('')):
        faceFilenames = [faceFilenames]

    targetChronoTenths = None
    targetChronoHours = None
    chronoFilenames = fd.get('chrono')
    if chronoFilenames:
        targetChronoTenths, targetChronoHours = chronoFilenames

    if date_windows and date_window_filename:
        window, mask = date_window_filename

        for platform in targetPlatforms:
            im = PIL.Image.open(getPlatformFilename('resources/clock_faces/' + window, platform))
            dateWindowSizes[platform] = im.size

        resourceStr += make_rle('clock_faces/' + window, name = 'DATE_WINDOW', useRle = supportRle, platforms = targetPlatforms, compress = True)

        if mask and bwPlatforms:
            resourceStr += make_rle('clock_faces/' + mask, name = 'DATE_WINDOW_MASK', useRle = supportRle, platforms = bwPlatforms, compress = True)

    if targetChronoTenths:
        resourceStr += make_rle_trans('clock_faces/' + targetChronoTenths, name = 'CHRONO_DIAL_TENTHS', useRle = supportRle, platforms = targetPlatforms, compress = True)
        resourceStr += make_rle_trans('clock_faces/' + targetChronoHours, name = 'CHRONO_DIAL_HOURS', useRle = supportRle, platforms = targetPlatforms, compress = True)

        for platform in targetPlatforms:
            chronoDialFilename = getPlatformFilename('resources/clock_faces/' + targetChronoTenths, platform)
            im = PIL.Image.open(chronoDialFilename)
            chronoDialSizes[platform] = im.size

    for platform in targetPlatforms:
        labelFilename = getPlatformFilename('resources/clock_faces/pebble_label.png', platform)
        im = PIL.Image.open(labelFilename)
        pebbleLabelSizes[platform] = im.size

        bluetoothFilename = getPlatformFilename('resources/bluetooth_connected.png', platform)
        im = PIL.Image.open(bluetoothFilename)
        bluetoothSizes[platform] = im.size

    print >> generatedTable, "struct FaceDef clock_face_table[NUM_FACES] = {"
    for i in range(len(faceFilenames)):
        print >> generatedTable, "  { RESOURCE_ID_CLOCK_FACE_%s }," % (i)

        faceFilename = 'clock_faces/' + faceFilenames[i]
        resourceStr += make_rle(faceFilename, name = 'CLOCK_FACE_%s' % (i), useRle = supportRle, platforms = targetPlatforms, compress = True)

        if prebakeLabel:
            # Also generate a pre-labeled clock face.
            basename = os.path.split(faceFilename)[1]
            basename = os.path.splitext(basename)[0]
            outputFilename = 'build/%s_label.png' % (basename)
            applyLabel(outputFilename, i, platforms = targetPlatforms)
            resourceStr += make_rle(outputFilename, name = 'CLOCK_FACE_%s_LABEL' % (i), useRle = supportRle, platforms = targetPlatforms, compress = True)


    print >> generatedTable, "};\n"

    faceColors = fd.get('colors')
    print >> generatedTable, "#ifndef PBL_BW"
    print >> generatedTable, "struct FaceColorDef clock_face_color_table[NUM_FACE_COLORS] = {"
    for i in range(len(faceColors)):
        cb, db = faceColors[i]
        print >> generatedTable, "  { GColor%sARGB8, GColor%sARGB8, GColor%sARGB8, GColor%sARGB8, GColor%sARGB8, GColor%sARGB8 }," % (cb[0], cb[1], cb[2], cb[3], db[0], db[1])
    print >> generatedTable, "};"
    print >> generatedTable, "#endif  // PBL_BW\n"

    return resourceStr

def makeVectorHands(generatedTable, paintChannel, generatedDefs, hand, scaleFactors, groupList):
    resourceStr = ''

    colorMap = {
        'b' : '1',
        'w' : '2',
        '' : '0',
        }

    print >> generatedTable, "struct VectorHand %s_hand_vector_table = {" % (hand)
    print >> generatedTable, "  %s," % (paintChannel)
    print >> generatedTable, "  %s, (struct VectorHandGroup[]){" % (len(groupList))
    for platform in targetPlatforms:
        shape = getPlatformShape(platform)
        scaleFactor = scaleFactors.get(shape, 1.0)

        print >> generatedTable, "#ifdef PBL_PLATFORM_%s" % (platform.upper())
        for fillType, points, scale in groupList:
            scale *= scaleFactor
            print >> generatedTable, "  { { %s, (GPoint[]){" % (len(points))
            for px, py in points:
                print >> generatedTable, "    { %d, %d }," % (px * scale, py * scale)
            print >> generatedTable, "  } } },"
        print >> generatedTable, "#endif  // PBL_PLATFORM_%s" % (platform.upper())

    print >> generatedTable, "  }"
    print >> generatedTable, "};\n"

    return resourceStr

def getNumSteps(hand, platform):
    # Get the number of subdivisions for the hand.
    numStepsHand = numSteps[hand]

    # If we're building support for sweep-second hands, we might need
    # more subdivisions.
    if 'sweep' in defaults and platform != 'aplite':
        if enableChronoSecondHand:
            if hand == 'chrono_second':
                numStepsHand = numStepsSweep[hand]
        else:
            if hand == 'second':
                numStepsHand = numStepsSweep[hand]

    return numStepsHand

def getResourceCacheSize(hand, platform):
    if platform == 'aplite' and hand != 'second':
        # On Aplite, we only ever cache the second hand--not enough
        # RAM to play around with.
        return 0

    return resourceCacheSize.get((hand, platform), [0, 0])[0]

def getMaskResourceCacheSize(hand, platform):
    if platform not in bwPlatforms:
        # When we're building for a color platform, we don't bother
        # cachings masks.
        return 0

    return resourceCacheSize.get((hand, platform), [0, 0])[1]

def makeBitmapHands(generatedTable, generatedDefs, useRle, hand, scaleFactors, sourceBasename, colorMode, asymmetric, pivot, scale):
    resourceStr = ''

    for platform in targetPlatforms:
        shape = getPlatformShape(platform)
        color = getPlatformColor(platform)
        scaleFactor = scaleFactors.get(shape, 1.0)

        print >> generatedTable, "#ifdef PBL_PLATFORM_%s" % (platform.upper())

        if color == 'bw':
            resourceStr += makeBitmapHandsBW(generatedTable, useRle, hand, sourceBasename, colorMode, asymmetric, pivot, scale * scaleFactor, platform)
        else:
            resourceStr += makeBitmapHandsColor(generatedTable, useRle, hand, sourceBasename, colorMode, asymmetric, pivot, scale * scaleFactor, platform)

        print >> generatedTable, "#endif  // PBL_PLATFORM_%s" % (platform.upper())

    return resourceStr

def makeBitmapHandsBW(generatedTable, useRle, hand, sourceBasename, colorMode, asymmetric, pivot, scale, platform):
    resourceStr = ''
    maskResourceStr = ''

    compress = (hand not in ['second', 'chrono_second'])

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

    # In the B&W case, we don't need to make a distinction between the
    # explicit and implicit masks.  An explicit mask completely
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

    numStepsHand = getNumSteps(hand, platform)
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

            # Now make the 1-bit version for B&W platforms.
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
            # multiple of bytes, but the B&W build is the lowest
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
                # In the non-transparency case, the B&W mask is the
                # B&W image we actually write out.
                p1 = pm1
            else:
                # In the transparency case, we need to write the mask
                # image separately.
                targetMaskBasename = 'build/flat_%s_%s_%s_mask_%s' % (handStyle, hand, i, platform)

                # Save the B&W mask.
                pm1.save('%s/%s.png' % (resourcesDir, targetMaskBasename))

                maskResourceStr += make_rle(targetMaskBasename + '.png', name = symbolMaskName, useRle = useRle, platforms = [platform], compress = compress)

            targetBasename = 'build/flat_%s_%s_%s_%s' % (handStyle, hand, i, platform)
            p1.save('%s/%s.png' % (resourcesDir, targetBasename))
            resourceStr += make_rle(targetBasename + '.png', name = symbolName, useRle = useRle, platforms = [platform], compress = compress)

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
    resourceCacheSize[hand, platform] = numBitmaps, numMaskBitmaps

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

def makeBitmapHandsColor(generatedTable, useRle, hand, sourceBasename, colorMode, asymmetric, pivot, scale, platform):
    resourceStr = ''
    maskResourceStr = ''

    compress = (hand not in ['second', 'chrono_second'])

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

    numStepsHand = getNumSteps(hand, platform)
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
            # multiple of bytes, but the B&W build is the lowest
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
                targetMaskBasename = 'build/flat_%s_%s_%s_mask_%s' % (handStyle, hand, i, platform)
                if sourceMaskExplicit:
                    # An explicit color mask.
                    pme2 = pme2.convert("P", palette = PIL.Image.ADAPTIVE, colors = 16)
                    pme2.save('%s/%s.png' % (resourcesDir, targetMaskBasename))

                    maskResourceStr += make_rle(targetMaskBasename + '.png', name = symbolMaskName, useRle = useRle, platforms = [platform], compress = compress)

                else:
                    # With only an implicit color mask, we won't be
                    # using the mask image in color.
                    pass

            targetBasename = 'build/flat_%s_%s_%s_%s' % (handStyle, hand, i, platform)
            p2.save('%s/%s.png' % (resourcesDir, targetBasename))
            resourceStr += make_rle(targetBasename + '.png', name = symbolName, useRle = useRle, platforms = [platform], compress = compress)

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
    resourceCacheSize[hand, platform] = numBitmaps, numMaskBitmaps

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

    handDefEntry = """
#ifdef PBL_PLATFORM_%(platformUpper)s
struct HandDef %(hand)s_hand_def = {
    NUM_STEPS_%(handUpper)s,
    %(resourceId)s, %(resourceMaskId)s,
    %(placeX)s, %(placeY)s,
    %(useRle)s,
    %(bitmapCenters)s,
    %(bitmapTable)s,
    %(vectorTable)s,
};
#endif  // PBL_PLATFORM_%(platformUpper)s
"""

    scaleFactors = hands[handStyle + '_scale']
    for hand, bitmapParams, vectorParams in hands[handStyle + '_hands']:
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

        paintChannel = 0
        resourceId = '0'
        bitmapCenters = 'NULL'
        bitmapTable = 'NULL'
        vectorTable = 'NULL'

        if bitmapParams:
            colorMode = bitmapParams[1]
            paintChannel, useTransparency, dither = parseColorMode(colorMode)
            resourceId = 'RESOURCE_ID_%s_0' % (hand.upper())
            bitmapCenters = '%s_hand_bitmap_lookup' % (hand)
            bitmapTable = '%s_hand_bitmap_table' % (hand)
            resourceStr += makeBitmapHands(generatedTable, generatedDefs, useRle, hand, scaleFactors, *bitmapParams)

        if vectorParams:
            vectorTable = '&%s_hand_vector_table' % (hand)
            resourceStr += makeVectorHands(generatedTable, paintChannel, generatedDefs, hand, scaleFactors, vectorParams)

        for platform in targetPlatforms:
            resourceMaskId = resourceId

            shape = getPlatformShape(platform)
            color = getPlatformColor(platform)
            hourMinuteOverlap = ('hour_minute_overlap' in defaults)

            screenSize = screenSizes[shape]
            placeX = screenSize[0] / 2
            placeY = screenSize[1] / 2

            if centers and shape in centers and hand in centers[shape]:
                placeX, placeY = centers[shape][hand]

            if bitmapParams:
                resourceMaskId = resourceId
                if useTransparency and (color == 'bw' or hourMinuteOverlap):
                    resourceMaskId = 'RESOURCE_ID_%s_0_MASK' % (hand.upper())

            handDef = handDefEntry % {
                'hand' : hand,
                'handUpper' : hand.upper(),
                'resourceId' : resourceId,
                'resourceMaskId' : resourceMaskId,
                'placeX' : placeX,
                'placeY' : placeY,
                'useRle' : int(bool(useRle)),
                'bitmapCenters' : bitmapCenters,
                'bitmapTable' : bitmapTable,
                'vectorTable' : vectorTable,
                'platformUpper' : platform.upper(),
            }
            print >> generatedTable, handDef

        print >> generatedDefs, 'extern struct HandDef %s_hand_def;' % (hand)

    return resourceStr

def getIndicator(fd, token):
    """ Gets an indicator tuple from the config dictionary.  Finds
    either a tuple or a list of tuples; in either case returns a
    dictionary of lists of platform to tuples. """

    list = fd.get(token, None)
    indicator = {}
    for platform in targetPlatforms:
        shape = getPlatformShape(platform)
        list_shape = fd.get(token + '_' + shape, list)
        if not isinstance(list_shape, type([])):
            list_shape = [list_shape]
        indicator[platform] = list_shape

    return indicator

def expandIndicatorList(list_shape, numFaces):
    """ Given an indicator list, e.g. getIndicator()[platform], return
    a list whose length is the number of faces. """

    if len(list_shape) == 1 and numFaces != 1:
        list_shape = [list_shape[0]] * numFaces
    if len(list_shape) == numFaces / 2:
      # Implicitly expand it by doubling each face.
      i2 = list_shape
      list_shape = []
      for i in i2:
        list_shape.append(i)
        list_shape.append(i)

    assert len(list_shape) == numFaces

    return list_shape

def makeIndicatorTable(generatedTable, generatedDefs, name, indicator, numFaces, anonymous = False):
    """ Makes an array of IndicatorTable values to define how a given
    indicator (that is, a bluetooth or battery indicator, or a
    day/date window) is meant to be rendered for each of the alternate
    faces. """

    if not indicator.values()[0][0]:
        return

    if anonymous:
        # Anonymous structure within a table
        print >> generatedTable, "  { // %s" % (name)
    else:
        # Standalone named structure
        print >> generatedDefs, "extern struct IndicatorTable %s[%s];" % (name, numFaces)
        print >> generatedTable, "struct IndicatorTable %s[%s] = {" % (name, numFaces)

    def writeTable(generatedTable, list_shape, numFaces):
        list_shape = expandIndicatorList(list_shape, numFaces)
        for x, y, c in list_shape:
            print >> generatedTable, "   { %s, %s, %s }," % (
                x, y, int(c[0] == 'b'))

    for platform in targetPlatforms:
        print >> generatedTable, "#ifdef PBL_PLATFORM_%s" % (platform.upper())
        writeTable(generatedTable, indicator[platform], numFaces)
        print >> generatedTable, "#endif  // PBL_PLATFORM_%s" % (platform.upper())

    if anonymous:
        # Anonymous structure within a table
        print >> generatedTable, "  },";
    else:
        print >> generatedTable, "};\n";

def makeMoonWheel(platform):
    """ Returns the resource strings needed to include the moon wheel
    icons, for the optional moon wheel window. """

    resourceStr = ''

    # moon_wheel_white_*.png is for when the moon is to be drawn as white pixels on black (used on b&w platforms only).
    # moon_wheel_black_*.png is for when the moon is to be drawn as black pixels on white (b&w and color platforms).

    numStepsMoon = getNumSteps('moon', platform)
    color = getPlatformColor(platform)

    subdialMaskPathname = getPlatformFilename(resourcesDir + '/clock_faces/top_subdial_internal_mask.png', platform)
    subdialMask = PIL.Image.open(subdialMaskPathname)
    subdialSize = subdialMask.size
    subdialSizes[platform] = subdialSize

    subdialMask = subdialMask.convert('L')
    subdialMaskBinary = subdialMask.point(thresholdMask)
    black = PIL.Image.new('L', subdialMask.size, 0)
    white = PIL.Image.new('L', subdialMask.size, 255)

    if color == 'bw':
        cats = ['white', 'black']
    else:
        # We need only the white image on color platforms.
        cats = ['black']

    for cat in cats:
        wheelSourcePathname = getPlatformFilename('%s/clock_faces/moon_wheel_%s.png' % (resourcesDir, cat), platform)
        wheelSource = PIL.Image.open(wheelSourcePathname)
        wheelSize = wheelSource.size

        for i in range(numStepsMoon):
            angle = i * 180.0 / numStepsMoon

            # Rotate the moon wheel to the appropriate angle.
            p = wheelSource.rotate(-angle, PIL.Image.BICUBIC, True)

            cx, cy = p.size[0] / 2, p.size[1] / 2
            px, py = cx - wheelSize[0] / 2, cy - wheelSize[1] / 2
            cropbox = (px, py, px + subdialSize[0], py + subdialSize[1])
            p = p.crop(cropbox)

            # Now make the 1-bit version for B&W platforms and
            # the 2-bit version for color platforms.
            if color == 'bw':
                p = p.convert('1')
                if cat == 'white':
                    # Invert the image for moon_wheel_white.
                    p = PIL.Image.composite(black, white, p)

                # Now apply the mask.
                p = PIL.Image.composite(p, black, subdialMask)

            else:
                p = p.convert('RGB')
                r, g, b = p.split()
                r = r.point(threshold2Bit).convert('L')
                g = g.point(threshold2Bit).convert('L')
                b = b.point(threshold2Bit).convert('L')

                # Now apply the mask.
                r = PIL.Image.composite(r, black, subdialMaskBinary)
                g = PIL.Image.composite(g, black, subdialMaskBinary)
                b = PIL.Image.composite(b, black, subdialMaskBinary)

                p = PIL.Image.merge('RGBA', [r, g, b, subdialMask])

                # And quantize to 16 colors.
                p = p.convert("P", palette = PIL.Image.ADAPTIVE, colors = 16)

            targetBasename = 'build/rot_moon_wheel_%s_%s_%s.png' % (cat, i, platform)
            p.save('%s/%s' % (resourcesDir, targetBasename))
            resourceStr += make_rle(targetBasename, name = 'MOON_WHEEL_%s_%s' % (cat.upper(), i), useRle = supportRle, platforms = [platform], compress = True)

    # Let's also throw in the other subdial and clock-face decorations here.
    if not prebakeLabel:
        resourceStr += make_rle('clock_faces/pebble_label.png', name = 'PEBBLE_LABEL', useRle = supportRle, platforms = [platform], compress = True)
        if platform in bwPlatforms:
            resourceStr += make_rle('clock_faces/pebble_label_mask.png', name = 'PEBBLE_LABEL_MASK', useRle = supportRle, platforms = [platform], compress = True)

    resourceStr += make_rle('clock_faces/top_subdial.png', name = 'TOP_SUBDIAL', useRle = supportRle, platforms = [platform], compress = True)
    if platform in bwPlatforms:
        resourceStr += make_rle('clock_faces/top_subdial_mask.png', name = 'TOP_SUBDIAL_MASK', useRle = supportRle, platforms = [platform], compress = True)
        resourceStr += make_rle('clock_faces/top_subdial_frame_mask.png', name = 'TOP_SUBDIAL_FRAME_MASK', useRle = supportRle, platforms = [platform], compress = True)

    resourceStr += make_rle('bluetooth_connected.png', name = 'BLUETOOTH_CONNECTED', useRle = supportRle, platforms = [platform], compress = True)
    resourceStr += make_rle('bluetooth_disconnected.png', name = 'BLUETOOTH_DISCONNECTED', useRle = supportRle, platforms = [platform], compress = True)
    if platform != 'aplite':
        resourceStr += make_rle('quiet_time.png', name = 'QUIET_TIME', useRle = supportRle, platforms = [platform], compress = True)
    if platform in bwPlatforms:
        resourceStr += make_rle('bluetooth_mask.png', name = 'BLUETOOTH_MASK', useRle = supportRle, platforms = [platform], compress = True)
        if platform != 'aplite':
            resourceStr += make_rle('quiet_time_mask.png', name = 'QUIET_TIME_MASK', useRle = supportRle, platforms = [platform], compress = True)

    resourceStr += make_rle('battery_gauge_empty.png', name = 'BATTERY_GAUGE_EMPTY', useRle = supportRle, platforms = [platform], compress = True)
    resourceStr += make_rle('battery_gauge_charged.png', name = 'BATTERY_GAUGE_CHARGED', useRle = supportRle, platforms = [platform], compress = True)
    resourceStr += make_rle('charging.png', name = 'CHARGING', useRle = supportRle, platforms = [platform], compress = True)
    if platform in bwPlatforms:
        resourceStr += make_rle('battery_gauge_mask.png', name = 'BATTERY_GAUGE_MASK', useRle = supportRle, platforms = [platform], compress = True)
    if platform in bwPlatforms:
        resourceStr += make_rle('charging_mask.png', name = 'CHARGING_MASK', useRle = supportRle, platforms = [platform], compress = True)


    return resourceStr

def enquoteStrings(strings):
    """ Accepts a list of strings, returns a list of strings with
    embedded quotation marks. """
    quoted = []
    for str in strings:
        quoted.append('"%s"' % (str))
    return quoted

def configWatch():
    import version
    versionStr = version.version
    versionMajor, versionMinor = map(int, versionStr.split('.')[:2])
    configVersionStr = version.configVersion
    configVersionMajor, configVersionMinor = map(int, configVersionStr.split('.')[:2])

    generatedTable = open('%s/generated_table.c' % (resourcesDir), 'w')
    generatedDefs = open('%s/generated_defs.h' % (resourcesDir), 'w')

    resourceStr = ''

    if top_subdial.values()[0][0]:
        for platform in targetPlatforms:
            resourceStr += makeMoonWheel(platform)

    resourceStr += makeFaces(generatedTable, generatedDefs)
    resourceStr += makeHands(generatedTable, generatedDefs)

    print >> generatedDefs, "extern struct IndicatorTable date_windows[NUM_DATE_WINDOWS][NUM_INDICATOR_FACES];"
    print >> generatedTable, "struct IndicatorTable date_windows[NUM_DATE_WINDOWS][NUM_INDICATOR_FACES] = {"
    for i in range(len(date_window_keys)):
        ch = chr(97 + i)
        makeIndicatorTable(generatedTable, generatedDefs, ch, date_windows[i], numIndicatorFaces, anonymous = True)
    print >> generatedTable, "};\n"

    makeIndicatorTable(generatedTable, generatedDefs, 'battery_table', battery, numIndicatorFaces)
    makeIndicatorTable(generatedTable, generatedDefs, 'bluetooth_table', bluetooth, numIndicatorFaces)
    makeIndicatorTable(generatedTable, generatedDefs, 'top_subdial', top_subdial, numFaces)

    resourceIn = open('%s/package.json.in' % (rootDir), 'r').read()
    resource = open('%s/package.json' % (rootDir), 'w')

    watchface = 'true'
    if screenshotBuild:
        watchface = 'false'
    elif enableChronoSecondHand:
        watchface = 'false'

    langData = open('%s/lang_data.json' % (resourcesDir), 'r').read()
    generatedMedia = resourceStr[:-1]

    print >> resource, resourceIn % {
        'versionMajor' : versionMajor,
        'versionMinor' : versionMinor,
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
        'versionStr' : versionStr,
        'configVersionMajor' : configVersionMajor,
        'configVersionMinor' : configVersionMinor,
        'formattedConfigLangs' : repr(config_langs),
        'numFaces' : numFaces,
        'numFaceColors' : numFaceColors,
        'defaultFaceIndex' : defaultFaceIndex,
        'dateWindowKeys' : date_window_keys,
        'enableChronoDial' : int('chrono' in fd),
        'defaultBluetooth' : defaultBluetooth,
        'defaultBattery' : defaultBattery,
        'defaultSecondHand' : int('second' in defaults),
        'defaultHourBuzzer' : int('buzzer' in defaults),
        'enableSweepSeconds' : int('sweep' in defaults),
        'defaultDateWindows' : repr(defaultDateWindows),
        'displayLangLookup' : displayLangLookup,
        'enableTopSubdial' : int(bool(top_subdial.values()[0][0])),
        'enableDebug' : int(compileDebugging),
        'defaultTopSubdial' : defaultTopSubdial,
        'defaultLunarBackground' : defaultLunarBackground,
        'pebbleLabel' : int('pebble_label' in defaults),
        }

    configPerPlatformIn = open('%s/generated_config.h.per_platform_in' % (resourcesDir), 'r').read()
    configIn = open('%s/generated_config.h.in' % (resourcesDir), 'r').read()
    config = open('%s/generated_config.h' % (resourcesDir), 'w')

    for platform in targetPlatforms:
        shape = getPlatformShape(platform)
        print >> config, configPerPlatformIn % {
            'platformUpper' : platform.upper(),
            'screenWidth' : screenSizes[shape][0],
            'screenHeight' : screenSizes[shape][1],
            'batteryGaugeFillX' : batteryGaugeSizes[shape][0][0],
            'batteryGaugeFillY' : batteryGaugeSizes[shape][0][1],
            'batteryGaugeFillW' : batteryGaugeSizes[shape][0][2],
            'batteryGaugeFillH' : batteryGaugeSizes[shape][0][3],
            'batteryGaugeBarX' : batteryGaugeSizes[shape][1][0],
            'batteryGaugeBarY' : batteryGaugeSizes[shape][1][1],
            'batteryGaugeBarW' : batteryGaugeSizes[shape][1][2],
            'batteryGaugeBarH' : batteryGaugeSizes[shape][1][3],
            'batteryGaugeFont' : batteryGaugeSizes[shape][2][0],
            'batteryGaugeVshift' : batteryGaugeSizes[shape][2][1],
            'chronoDialSizeX' : chronoDialSizes.get(platform, (0, 0))[0],
            'chronoDialSizeY' : chronoDialSizes.get(platform, (0, 0))[1],
            'bluetoothSizeX' : bluetoothSizes[platform][0],
            'bluetoothSizeY' : bluetoothSizes[platform][1],
            'numStepsHour' : getNumSteps('hour', platform),
            'numStepsMinute' : getNumSteps('minute', platform),
            'numStepsSecond' : getNumSteps('second', platform),
            'numStepsChronoMinute' : getNumSteps('chrono_minute', platform),
            'numStepsChronoSecond' : getNumSteps('chrono_second', platform),
            'numStepsChronoTenth' : getNumSteps('chrono_tenth', platform),
            'numStepsMoon' : getNumSteps('moon', platform),
            'subdialSizeX' : subdialSizes[platform][0],
            'subdialSizeY' : subdialSizes[platform][1],
            'dateWindowSizeX' : dateWindowSizes[platform][0],
            'dateWindowSizeY' : dateWindowSizes[platform][1],
            'pebbleLabelSizeX' : pebbleLabelSizes[platform][0],
            'pebbleLabelSizeY' : pebbleLabelSizes[platform][1],
            'limitResourceCache' : int('limit_cache' in defaults or 'limit_cache_' + platform in defaults),
            'secondResourceCacheSize' : getResourceCacheSize('second', platform),
            'chronoSecondResourceCacheSize' : getResourceCacheSize('chrono_second', platform),
            'secondMaskResourceCacheSize' : getMaskResourceCacheSize('second', platform),
            'chronoSecondMaskResourceCacheSize' : getMaskResourceCacheSize('chrono_second', platform),
            }

    print >> config, configIn % {
        'persistKey' : 0x5151 + uuId[-1],
        'supportRle' : int(bool(supportRle)),
        'bwInvert' : int(bool(bwInvert)),
        'numFaces' : numFaces,
        'prebakeLabel' : int(bool(prebakeLabel)),
        'numIndicatorFaces' : numIndicatorFaces,
        'numFaceColors' : numFaceColors,
        'defaultFaceIndex' : defaultFaceIndex,
        'numDateWindows' : len(date_window_keys),
        'dateWindowKeys' : date_window_keys,
        'compileDebugging' : int(compileDebugging),
        'screenshotBuild' : int(screenshotBuild),
        'defaultDateWindows' : repr(defaultDateWindows)[1:-1],
        'enableBluetooth' : int(bool(bluetooth.values()[0][0])),
        'defaultBluetooth' : defaultBluetooth,
        'enableBatteryGauge' : int(bool(battery.values()[0][0])),
        'defaultBattery' : defaultBattery,
        'defaultSecondHand' : int('second' in defaults),
        'defaultHourBuzzer' : int('buzzer' in defaults),
        'enableSweepSeconds' : int('sweep' in defaults),
        'makeChronograph' : int(enableChronoSecondHand),
        'enableChronoDial' : int('chrono' in fd),
        'enableTopSubdial' : int(bool(top_subdial.values()[0][0])),
        'defaultTopSubdial' : defaultTopSubdial,
        'defaultLunarBackground' : defaultLunarBackground,
        'enableChronoMinuteHand' : int(enableChronoMinuteHand),
        'enableChronoSecondHand' : int(enableChronoSecondHand),
        'enableChronoTenthHand' : int(enableChronoTenthHand),
        'hourMinuteOverlap' : int('hour_minute_overlap' in defaults),
        }

    # Also generate the html pages for this version, if needed.

    source = open('%s/html/rosewright_configure.html.in' % (rootDir), 'r').read()
    for lang in config_langs:
        dict = {
            'rootDir' : rootDir,
            'lang' : lang,
            'configVersionMajor' : configVersionMajor,
            'configVersionMinor' : configVersionMinor,
            }

        filename = '%(rootDir)s/html/rosewright_%(configVersionMajor)s_%(configVersionMinor)s_configure.%(lang)s.html' % dict
        print filename
        open(filename, 'w').write(source % dict)


# Main.
try:
    opts, args = getopt.getopt(sys.argv[1:], 's:H:F:iwm:xp:dDh')
except getopt.error, msg:
    usage(1, msg)

# The set of languages supported by our config pages.
config_langs = [ 'en', 'es', 'de', 'fr', 'bg' ]

watchStyle = None
handStyle = None
faceStyle = None
compileDebugging = False
screenshotBuild = False
supportRle = False
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
    elif opt == '-x':
        supportRle = True
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
    targetPlatforms = [ 'aplite', 'basalt', 'chalk', 'diorite', 'emery' ]

bwPlatforms = set(targetPlatforms) & set(['aplite', 'diorite'])

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

bwInvert = fd.get('bw_invert', False)

faceColors = fd.get('colors')
numFaceColors = len(faceColors)

defaultFaceIndex = fd.get('default_face', 0)

top_subdial = getIndicator(fd, 'top_subdial')
if top_subdial.values()[0][0]:
    numIndicatorFaces = numFaces * 2
else:
    numIndicatorFaces = numFaces

# Get the set of date_windows defined for the watchface.
date_windows = []
date_window_keys = ''
for key in 'abcd':
    dw = getIndicator(fd, 'date_window_%s' % (key))
    if dw.values()[0][0]:
        date_windows.append(dw)
        date_window_keys += key

date_window_filename = fd.get('date_window_filename', None)
bluetooth = getIndicator(fd, 'bluetooth')
battery = getIndicator(fd, 'battery')
defaults = fd.get('defaults', [])
centers = fd.get('centers', [])

subdialSizes = {}  # filled in by makeMoonWheel()
dateWindowSizes = {} # filled in by makeFaces()
pebbleLabelSizes = {} # filled in by makeFaces()
chronoDialSizes = {} # filled in by makeFaces()
bluetoothSizes = {} # filled in by makeFaces()


# Look for 'day' and 'date' prefixes in the defaults.
defaultDateWindows = [0] * len(date_window_keys)
for keyword in defaults:
    for token, value in [('day', 6), ('date', 2)]:
        if keyword.startswith(token + ':'):
            ch = keyword.split(':', 1)[1]
            i = ord(ch) - 97
            defaultDateWindows[i] = value
            break

prebakeLabel = ('prebake_label' in defaults)

defaultTopSubdial = 0
if 'moon_phase' in defaults:
    defaultTopSubdial = 2
elif 'pebble_label' in defaults:
    defaultTopSubdial = 1

defaultLunarBackground = 0
if 'moon_dark' in defaults:
    defaultLunarBackground = 1

configWatch()
#scaleIndicators()
