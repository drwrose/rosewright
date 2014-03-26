#! /usr/bin/env python

# This script requires PyICU; install it from
# https://pypi.python.org/pypi/PyICU .  This may in turn require
# ICU4C; install this from http://site.icu-project.org/download/ .

import sys
import os
import getopt
import icu

help = """
make_lang.py

Re-run this script to generate lang_table.c and lang_data.json in
the resources directory, based on the system language data.

make_lang.py [opts]
"""

fontChoices = [ 'latin', 'extended', 'rtl', 'zh', 'ja', 'ko', 'th', 'ta', 'hi' ]

# Enough for 12 months.
NUM_DAY_NAMES = 12

# Font filenames and pixel sizes.
fontNames = {
    'latin' : ('ArchivoNarrow-Bold.ttf', 16),
    'extended' : ('DejaVuSansCondensed-Bold.ttf', 14),
    'rtl' : ('DejaVuSansCondensed-Bold.ttf', 14),
    'zh' : ('wqy-microhei.ttc', 16),
    'ja' : ('TakaoPGothic.ttf', 16),
    'ko' : ('UnDotum.ttf', 16),
    'th' : ('Waree.ttf', 16),
    'ta' : ('TAMu_Kalyani.ttf', 16),
    'hi' : ('lohit_hi.ttf', 16),
    }

# This list is duplicated in html/rosewright_X_configure.js.
langs = [
    [ 'en_US', 'English', 'latin' ],
    [ 'fr_FR', 'French', 'latin' ],
    [ 'it_IT', 'Italian', 'latin' ],
    [ 'es_ES', 'Spanish', 'latin' ],
    [ 'pt_PT', 'Portuguese', 'latin' ],
    [ 'de_DE', 'German', 'latin' ],
    [ 'nl_NL', 'Dutch', 'latin' ],
    [ 'da_DK', 'Danish', 'latin' ],
    [ 'sv_SE', 'Swedish', 'latin' ],
    [ 'is_IS', 'Icelandic', 'latin' ],
    [ 'tl', 'Tagalog', 'latin' ],
    ];

# Non-standard font required for these:
langs += [
    [ 'el_GR', 'Greek', 'extended' ],
    [ 'hu_HU', 'Hungarian', 'latin' ],
    [ 'ru_RU', 'Russian', 'extended' ],
    [ 'pl_PL', 'Polish', 'latin' ],
    [ 'cs_CZ', 'Czech', 'latin' ],
    [ 'hy_AM', 'Armenian', 'extended' ],
    [ 'tr_TR', 'Turkish', 'latin' ],
    ]

# These are written right-to-left.
langs += [
    [ 'he_IL', 'Hebrew', 'rtl' ],
    [ 'fa_IR', 'Farsi', 'rtl' ],
    [ 'ar_SA', 'Arabic', 'rtl' ],
    ]

# Each of these requires its own unique source font.
langs += [
    [ 'zh_CN', 'Chinese', 'zh' ],
    [ 'ja_JP', 'Japanese', 'ja' ],
    [ 'ko_KR', 'Korean', 'ko' ],
    [ 'th_TH', 'Thai', 'th' ],
    [ 'ta_IN', 'Tamil', 'ta' ],
    [ 'hi_IN', 'Hindi', 'hi' ],
    ]

# Attempt to determine the directory in which we're operating.
rootDir = os.path.dirname(__file__) or '.'
resourcesDir = rootDir

neededChars = {}
maxNumChars = 0
maxTotalLen = 0

def writeResourceFile(generatedJson, nameType, localeName, nameList):
    global maxTotalLen
    filename = '%s_%s.raw' % (localeName, nameType)
    resourceId = '%s_%s_NAMES' % (localeName.upper(), nameType.upper())

    data = '\0'.join(nameList)
    maxTotalLen = max(maxTotalLen, len(data))

    open('%s/%s' % (resourcesDir, filename), 'w').write(data)
    
    nameEntry = """    {
      "type": "raw",
      "name": "%(id)s",
      "file": "%(filename)s"
    },"""    
    print >> generatedJson, nameEntry % {
        'id' : resourceId,
        'filename' : filename,
        }

    return 'RESOURCE_ID_%s' % (resourceId)

def getDfsNames(dfs, func):
    """ Extracts out either the abbreviated or the narrow names from
    the dfs, as appropriate. """

    names = func(dfs.STANDALONE, dfs.ABBREVIATED)

    # Remove a period for tightness.
    for i in range(len(names)):
        if '.' in names[i]:
            names[i] = names[i].replace('.', '')

    # If the abbreviated names go over four letters each, switch to
    # the narrow names instead.
    if max(map(len, names)) > 4:
        names = func(dfs.FORMAT, dfs.NARROW)

    return names

def makeDates(generatedTable, generatedJson, li):
    global maxNumChars
    localeName, langName, fontKey = langs[li]
    fontIndex = fontChoices.index(fontKey)

    locale = icu.Locale(localeName)
    print '%s/%s/%s' % (localeName, langName, locale.getDisplayName())
    dfs = icu.DateFormatSymbols(locale)

    weekdays = getDfsNames(dfs, dfs.getWeekdays)
    months = getDfsNames(dfs, dfs.getMonths)
    
    neededChars.setdefault(fontKey, set())
    showNames = []
    showNamesUnicode = []
    for name in weekdays[1:] + months:
        # Ensure the first letter is uppercase for consistency.
        name = name[0].upper() + name[1:]

        # Reverse text meant to be written right-to-left.
        if fontKey == 'rtl':
            ls = list(name)
            ls.reverse()
            name = ''.join(ls)

        # Record the total set of Unicode characters required in the font.
        for char in name:
            neededChars[fontKey].add(ord(char))

        try:
            uname = name.encode('ascii')
        except UnicodeEncodeError:
            uname = name
        showNamesUnicode.append(uname)

        # And finally, re-encode to UTF-8 for the Pebble.
        name = name.encode('utf-8')
        maxNumChars = max(maxNumChars, len(name))

        showNames.append(name)

    weekdayNames = showNames[:7]
    monthNames = showNames[7:]
    weekdayNameId = writeResourceFile(generatedJson, 'weekday', localeName, weekdayNames)
    monthNameId = writeResourceFile(generatedJson, 'month', localeName, monthNames)

    weekdayNamesUnicode = showNamesUnicode[:7]
    monthNamesUnicode = showNamesUnicode[7:]
    
    print >> generatedTable, """  { "%s", %s, %s, %s }, // %s = %s""" % (localeName, fontIndex, weekdayNameId, monthNameId, li, langName)
    print >> generatedTable, """ //   Weekdays: %s""" % (repr(weekdayNamesUnicode))
    print >> generatedTable, """ //   Months: %s""" % (repr(monthNamesUnicode))
    print >> generatedTable, ""

def makeHex(ch):
    return '\\u%04x' % (ch)

def makeCharacterRegex(chars):
    chars = list(chars)
    chars.sort()

    i = 0
    r = [[chars[i], chars[i]]]
    i += 1
    while i < len(chars):
        if chars[i] == r[-1][1] + 1:
            # This character extends the current range.
            r[-1][1] = chars[i]
        else:
            # This character begins a new range.
            r.append([chars[i], chars[i]])
        i += 1

    # Now build a sequence of strings of the form \uXXXX or \uXXXX-\uXXXX.
    s = ''
    for a, b in r:
        if a == b:
            s += makeHex(a)
        elif a + 1 == b:
            s += makeHex(a) + makeHex(b)
        else:
            s += makeHex(a) + '-' + makeHex(b)

    return s

def makeLang():
    generatedTable = open('%s/lang_table.c' % (resourcesDir), 'w')
    print >> generatedTable, '// Generated by make_lang.py\n'
    generatedJson = open('%s/lang_data.json' % (resourcesDir), 'w')

    numLangs = len(langs)
    print >> generatedTable, "LangDef lang_table[%s] = {" % (numLangs)

    for li in range(numLangs):
        makeDates(generatedTable, generatedJson, li)

    print >> generatedTable, "};\n"

    print >> generatedTable, "int num_langs = %s;" % (numLangs)
    print >> generatedTable, "// maximum characters: %s" % (maxNumChars)
    print >> generatedTable, "#define DAY_NAMES_MAX_BUFFER %s\n" % (maxTotalLen)

    fontEntry = """    {
      "type": "font",
      "characterRegex": "[%(regex)s]",
      "name": "DAY_FONT_%(upperKey)s_%(size)s",
      "file": "%(filename)s"
    },"""    

    for fontKey in fontChoices:
        filename, size = fontNames[fontKey]
        print >> generatedJson, fontEntry % {
            'regex' : makeCharacterRegex(neededChars[fontKey]),
            'upperKey' : fontKey.upper(),
            'size' : size,
            'filename' : filename,
            }

# Main.
try:
    opts, args = getopt.getopt(sys.argv[1:], 'h')
except getopt.error, msg:
    usage(1, msg)

for opt, arg in opts:
    if opt == '-h':
        usage(0)

makeLang()
