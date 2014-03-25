#! /usr/bin/env python

import sys
import os
import getopt
import locale

help = """
make_lang.py

Re-run this script to generate lang_table.c and lang_data.json in
the resources directory, based on the system language data.

make_lang.py [opts]
"""

fontChoices = [ 'latin', 'extended', 'zh', 'ja', 'ko' ]

# Duplicated from lang_table.h.
MAX_DAY_NAME = 7
NUM_DAY_NAMES = 12

# Font filenames and pixel sizes.
fontNames = {
    'latin' : ('ArchivoNarrow-Bold.ttf', 16),
    'extended' : ('DejaVuSansCondensed-Bold.ttf', 14),
    'zh' : ('wqy-microhei.ttc', 16),
    'ja' : ('TakaoPGothic.ttf', 16),
    'ko' : ('UnDotum.ttf', 16),
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
    ];

# Non-standard font required for these:
langs += [
    [ 'el_GR', 'Greek', 'extended' ],
    [ 'hu_HU', 'Hungarian', 'latin' ],
    [ 'ru_RU', 'Russian', 'extended' ],
    [ 'pl_PL', 'Polish', 'latin' ],
    [ 'cs_CZ', 'Czech', 'latin' ],
    [ 'hy_AM', 'Armenian', 'extended' ],
    ]

# CJK is its own set of categories.
langs += [
    [ 'zh_CN', 'Chinese', 'zh' ],
    [ 'ja_JP', 'Japanese', 'ja' ],
    [ 'ko_KR', 'Korean', 'ko' ],
    ]

# Attempt to determine the directory in which we're operating.
rootDir = os.path.dirname(__file__) or '.'
resourcesDir = rootDir

neededChars = {}
maxNumChars = 0

def writeResourceFile(generatedJson, nameType, localeName, nameList):
    filename = '%s_%s.raw' % (nameType, localeName)
    resourceId = '%s_%s_NAMES' % (nameType.upper(), localeName.upper())

    data = ''
    for i in range(len(nameList)):
        name = (nameList[i] + '\0' * MAX_DAY_NAME)[:MAX_DAY_NAME]
        data += name
    for i in range(len(nameList), NUM_DAY_NAMES):
        data += '\0' * MAX_DAY_NAME

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
    

def makeDates(generatedTable, generatedJson, li):
    global maxNumChars
    localeName, langName, fontKey = langs[li]
    fontIndex = fontChoices.index(fontKey)
    print localeName
    locale.setlocale(locale.LC_ALL, localeName + '.UTF-8')

    neededChars.setdefault(fontKey, set())
    showNames = []
    for sym in [locale.ABDAY_1, locale.ABDAY_2, locale.ABDAY_3, locale.ABDAY_4, locale.ABDAY_5, locale.ABDAY_6, locale.ABDAY_7,
                locale.ABMON_1, locale.ABMON_2, locale.ABMON_3, locale.ABMON_4, locale.ABMON_5, locale.ABMON_6, locale.ABMON_7, locale.ABMON_8, locale.ABMON_9, locale.ABMON_10, locale.ABMON_11, locale.ABMON_12]:

        # Get the day or month text from the system locale table, and decode from UTF-8.
        name = locale.nl_langinfo(sym)
        name = name.decode('utf-8')

        # Clean up the text as needed:
        if name[-1] == '.':
            # Sometimes the abbreviation ends with a dot.
            name = name[:-1]
        # Strip out excess whitespace on one side or the other.
        name = name.strip()
        # Ensure the first letter is uppercase for consistency.
        name = name[0].upper() + name[1:]

        # Record the total set of Unicode characters required in the font.
        for char in name:
            neededChars[fontKey].add(ord(char))

        # And finally, re-encode to UTF-8 for the Pebble.
        name = name.encode('utf-8')
        maxNumChars = max(maxNumChars, len(name))

        showNames.append(name)

    weekdayNames = showNames[:7]
    monthNames = showNames[7:]
    weekdayNameId = writeResourceFile(generatedJson, 'weekday', localeName, weekdayNames)
    monthNameId = writeResourceFile(generatedJson, 'month', localeName, monthNames)
    
    print >> generatedTable, """  { "%s", %s, %s, %s }, // %s = %s""" % (localeName, fontIndex, weekdayNameId, monthNameId, li, langName)

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
    print >> generatedTable, "// maximum characters: %s\n" % (maxNumChars)

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
