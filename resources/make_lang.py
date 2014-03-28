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

specialCases = {
    ('es_ES', 'ampm') : ['am', 'pm'],    # Removed silly space
    ('he_IL', 'ampm') : ['ma', 'mp'],    # Reversed for rtl re-reversal
    ('de_DE', 'ampm') : ['vorm', 'nach'],
    ('ru_RU', 'month') : [u'\u042f\u043d\u0432', u'\u0424\u0435\u0432', u'\u041c\u0430\u0440', u'\u0410\u043f\u0440', u'\u041c\u0430\u0439', u'\u0418\u044e\u043d', u'\u0418\u044e\u043b', u'\u0410\u0432\u0433', u'\u0421\u0435\u043d', u'\u041e\u043a\u0442', u'\u041d\u043e\u044f', u'\u0414\u0435\u043a'],
    ('ru_RU', 'ampm') : [u'\u0434\u043e', u'\u043f\u043e\u0441\u043b'],
    ('hy_AM', 'ampm') : [u'\u0561\u057c\u0561\u057b', u'\u0570\u0565\u057f\u0578'],
    ('fa_IR', 'ampm') : [u'\u0635', u'\u0645'],  # copied from Arabic, does that work?
    ('ta_IN', 'ampm') : [u'\u0bae\u0bc1', u'\u0baa\u0bbf'], # Shortened to unique prefix
    ('th_TH', 'ampm') : [u'\u0e01\u0e48\u0e2d\u0e19', u'\u0e2b\u0e25\u0e31\u0e07'], # Shortened to unique prefix
    ('hi_IN', 'ampm') : [u'\u092a\u0942\u0930\u094d\u0935', u'\u0905\u092a\u0930'],
    }

# Attempt to determine the directory in which we're operating.
rootDir = os.path.dirname(__file__) or '.'
resourcesDir = rootDir

nameTypes = [ 'weekday', 'month', 'ampm' ]

neededChars = {}
maxNumChars = {}
maxTotalLen = {}

def writeResourceFile(generatedJson, nameType, localeName, nameList):
    filename = '%s_%s.raw' % (localeName, nameType)
    resourceId = '%s_%s_NAMES' % (localeName.upper(), nameType.upper())

    data = '\0'.join(nameList)
    maxTotalLen[nameType] = max(maxTotalLen.get(nameType, 0), len(data))

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

def getDfsNames(dfs, localeName, nameType, *funcs):
    """ Extracts out either the abbreviated or the narrow names from
    the dfs, as appropriate. """

    special = specialCases.get((localeName, nameType), None)
    if special:
        return special

    for func in funcs:
        for width in [dfs.ABBREVIATED, dfs.NARROW]:
            try:
                names = func(dfs.STANDALONE, width)
            except TypeError:
                names = func()

            # Remove a period for tightness.
            for i in range(len(names)):
                if '.' in names[i]:
                    names[i] = names[i].replace('.', '')

            # If the resulting names are short enough (we arbitrarily
            # declare 4 letters or less is short enough) then return
            # them.
            if max(map(len, names)) <= 4:
                return names

            # Otherwise, carry on to the next function attempt.

    return names

def getDefaultAmPm():
    """ Returns the default am/pm strings if the language-specific strings are too long. """
    return ['am', 'pm']

def makeDates(generatedTable, generatedJson, li):
    localeName, langName, fontKey = langs[li]
    fontIndex = fontChoices.index(fontKey)

    locale = icu.Locale(localeName)
    print '%s/%s/%s' % (localeName, langName, locale.getDisplayName())
    dfs = icu.DateFormatSymbols(locale)

    names = {
        'weekday': getDfsNames(dfs, localeName, 'weekday', dfs.getWeekdays)[1:],
        'month' : getDfsNames(dfs, localeName, 'month', dfs.getShortMonths, dfs.getMonths),
        'ampm' : getDfsNames(dfs, localeName, 'ampm', dfs.getAmPmStrings, getDefaultAmPm),
        }
    
    neededChars.setdefault(fontKey, set())
    showNames = {}
    showNamesUnicode = {}
    nameIds = {}
    for nameType in nameTypes:
        showNames[nameType] = []
        showNamesUnicode[nameType] = []
        for name in names[nameType]:
            if nameType == 'ampm':
                # For am/pm, force lowercase because I like that.
                name = name.lower()
            else:
                # For month and weekday, ensure the first letter is
                # uppercase for consistency.
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
            showNamesUnicode[nameType].append(uname)

            # And finally, re-encode to UTF-8 for the Pebble.
            name = name.encode('utf-8')
            maxNumChars[nameType] = max(maxNumChars.get(nameType, 0), len(name))

            showNames[nameType].append(name)

        nameIds[nameType] = writeResourceFile(generatedJson, nameType, localeName, showNames[nameType])

    print >> generatedTable, """  { "%s", %s, %s, %s, %s }, // %s = %s""" % (localeName, fontIndex, nameIds['weekday'], nameIds['month'], nameIds['ampm'], li, langName)
    for nameType in nameTypes:
        print >> generatedTable, """ //   %s: %s""" % (nameType, repr(showNamesUnicode[nameType]))
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
    for nameType in nameTypes:
        print >> generatedTable, "// %s maximum characters: %s" % (nameType, maxNumChars[nameType])
        print >> generatedTable, "#define %s_NAMES_MAX_BUFFER %s\n" % (nameType.upper(), maxTotalLen[nameType])

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
