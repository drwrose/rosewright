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

fontChoices = [ 'latin', 'el', 'ru', 'hy', 'rtl_he', 'rtl_ar', 'zh', 'ja', 'ko', 'th', 'ta', 'hi' ]

# Font (rect, round) filenames and (rect, round) pixel sizes.  Font
# sizes are encoded into the resource names, which appear in wright.c.
fontNames = {
    #'latin' : ('ArchivoNarrow-Bold.ttf', (16, 18)),
    #'latin' : (('9x18B.pcf.gz', '10x20.pcf.gz'), (18, 20)),
    'latin' : (('ArchivoNarrow-Bold.ttf', '10x20.pcf.gz'), (16, 20)),
    'el' : ('DejaVuSansCondensed-Bold.ttf', (14, 16)),
    'ru' : ('DejaVuSansCondensed-Bold.ttf', (14, 16)),
    'hy' : ('DejaVuSansCondensed-Bold.ttf', (14, 16)),
    'rtl_he' : ('DejaVuSansCondensed-Bold.ttf', (14, 16)),
    'rtl_ar' : ('DejaVuSansCondensed-Bold.ttf', (14, 16)),
    'zh' : ('wqy-microhei.ttc', (16, 18)),
    'ja' : ('TakaoPGothic.ttf', (16, 18)),
    'ko' : ('UnDotum.ttf', (16, 18)),
    'th' : ('Waree.ttf', (16, 18)),
    'ta' : ('TAMu_Kalyani.ttf', (16, 18)),
    'hi' : ('lohit_hi.ttf', (16, 18)),
    }

# This list is duplicated in html/rosewright_X_configure.js, and the
# language names also appear in text_strings.js.*.
langs = [
    [ 'en_US', 'English', 'latin', 0 ],
    [ 'fr_FR', 'French', 'latin', 1 ],
    [ 'it_IT', 'Italian', 'latin', 2 ],
    [ 'es_ES', 'Spanish', 'latin', 3 ],
    [ 'pt_PT', 'Portuguese', 'latin', 4 ],
    [ 'de_DE', 'German', 'latin', 5 ],
    [ 'nl_NL', 'Dutch', 'latin', 6 ],
    [ 'da_DK', 'Danish', 'latin', 7 ],
    [ 'sv_SE', 'Swedish', 'latin', 8 ],
    [ 'is_IS', 'Icelandic', 'latin', 9 ],
    [ 'tl', 'Tagalog', 'latin', 10 ],
    [ 'el_GR', 'Greek', 'el', 11 ],
    [ 'hu_HU', 'Hungarian', 'latin', 12 ],
    [ 'ru_RU', 'Russian', 'ru', 13 ],
    [ 'pl_PL', 'Polish', 'latin', 14 ],
    [ 'cs_CZ', 'Czech', 'latin', 15 ],
    [ 'hy_AM', 'Armenian', 'hy', 16 ],
    [ 'tr_TR', 'Turkish', 'latin', 17 ],
    [ 'he_IL', 'Hebrew', 'rtl_he', 18 ],
    [ 'fa_IR', 'Farsi', 'rtl_ar', 19 ],
    [ 'ar_SA', 'Arabic', 'rtl_ar', 20 ],
    [ 'zh_CN', 'Chinese', 'zh', 21 ],
    [ 'ja_JP', 'Japanese', 'ja', 22 ],
    [ 'ko_KR', 'Korean', 'ko', 23 ],
    [ 'th_TH', 'Thai', 'th', 24 ],
    [ 'ta_IN', 'Tamil', 'ta', 25 ],
    [ 'hi_IN', 'Hindi', 'hi', 26 ],
    [ 'bg_BG', 'Bulgarian', 'ru', 27 ],
    ]

# In the above list, the third parameter is the font code:

# 'latin' is the normal Latin font that ships on the Pebble.

# The remaining codes are language-specific fonts, though some that
# share the same alphabet are grouped together.  Some may use the same
# source font but the resource has a different subset of glyphs.

# 'rtl_*' are written right-to-left and require special handling for
# this reason.

# The remaining codes are for language-specific fonts.

# The fourth parameter is the index into lang_table, which should be
# unique to each language and probably shouldn't change for a
# particular language over the lifetime of this app.

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

# We generate the date_names array with three sets of strings, in the
# order: all weekday names, then all month names, then all ampm names.
nameTypes = [ 'weekday', 'month', 'ampm' ]

neededChars = {}
maxNumChars = 0
maxTotalLen = 0

def writeResourceFile(generatedJson, localeName, nameList):
    global maxTotalLen
    filename = '%s.raw' % (localeName)
    resourceId = '%s_NAMES' % (localeName.upper())

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

def makeDates(generatedTable, generatedJson, langRow, li):
    global maxNumChars
    localeName, langName, fontKey = langRow
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
    showNames = []
    showNamesUnicode = []
    nameIds = None
    for nameType in nameTypes:
        for name in names[nameType]:
            if nameType == 'ampm':
                # For am/pm, force lowercase because I like that.
                name = name.lower()
            else:
                # For month and weekday, ensure the first letter is
                # uppercase for consistency.
                name = name[0].upper() + name[1:]

            # Reverse text meant to be written right-to-left.
            if fontKey.startswith('rtl'):
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

    nameIds = writeResourceFile(generatedJson, localeName, showNames)

    print >> generatedTable, """ // %s""" % (localeName)
    print >> generatedTable, """  { %s, %s }, // %s = %s""" % (fontIndex, nameIds, li, langName)
    print >> generatedTable, """ // %s""" % (repr(showNamesUnicode))
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
    # Generate the displayLangLookup table for pebble-js-app.js.
    displayLangLookup = open('%s/displayLangLookup.txt' % (resourcesDir), 'w')
    print >> displayLangLookup, "var display_lang_lookup = {"
    for localeName, langName, fontKey, index in langs:
        print >> displayLangLookup, "  '%s' : %s," % (localeName, index)
        if '_' in localeName:
            prefix = localeName.split('_')[0]
            print >> displayLangLookup, "  '%s' : %s," % (prefix, index)
            hyphened = localeName.replace('_', '-')
            print >> displayLangLookup, "  '%s' : %s," % (hyphened, index)
            
    print >> displayLangLookup, "};"
    print >> displayLangLookup, "var display_lang_reverse = {"
    for localeName, langName, fontKey, index in langs:
        print >> displayLangLookup, "  %s : '%s'," % (index, localeName)
    print >> displayLangLookup, "};"
    displayLangLookup.close()

    # Generate lang_table.c.
    generatedTable = open('%s/lang_table.c' % (resourcesDir), 'w')
    print >> generatedTable, '// Generated by make_lang.py\n'
    generatedJson = open('%s/lang_data.json' % (resourcesDir), 'w')

    numLangs = len(langs)
    print >> generatedTable, "LangDef lang_table[%s] = {" % (numLangs)

    # Reorder the langs table and write it out in order by index.
    langDict = {}
    for localeName, langName, fontKey, index in langs:
        langDict[index] = localeName, langName, fontKey
    for li in range(numLangs):
        makeDates(generatedTable, generatedJson, langDict[li], li)

    print >> generatedTable, "};\n"

    print >> generatedTable, "int num_langs = %s;" % (numLangs)
    print >> generatedTable, "// maximum characters: %s" % (maxNumChars)
    print >> generatedTable, "#define DATE_NAMES_MAX_BUFFER %s\n" % (maxTotalLen)

    # Ensure the latin font includes the digits, plus whatever other
    # characters we might need there.
    for char in '0123456789ABCDk!':
        neededChars['latin'].add(ord(char))

    fontEntry = """    {
      "type": "font",
      "characterRegex": "[%(regex)s]",
      "name": "DAY_FONT_%(upperKey)s_%(size_rect)s",
      "file": "%(filename_rect)s",
      "targetPlatforms" : [
        "aplite", "basalt"
      ]
    },
    {
      "type": "font",
      "characterRegex": "[%(regex)s]",
      "name": "DAY_FONT_%(upperKey)s_%(size_round)s",
      "file": "%(filename_round)s",
      "targetPlatforms" : [
        "chalk"
      ]
    },"""    

    for fontKey in fontChoices:
        filenames, (size_rect, size_round) = fontNames[fontKey]
        if isinstance(filenames, type(())):
            filename_rect, filename_round = filenames
        else:
            filename_rect = filenames
            filename_round = filenames
        print >> generatedJson, fontEntry % {
            'regex' : makeCharacterRegex(neededChars[fontKey]),
            'upperKey' : fontKey.upper(),
            'size_rect' : size_rect,
            'size_round' : size_round,
            'filename_rect' : filename_rect,
            'filename_round' : filename_round,
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
