#! /usr/bin/env python

import sys
import os
import getopt
import locale

help = """
make_lang.py

Re-run this script to generate lang_table.c in the resources directory,
based on the system language data.

make_lang.py [opts]
"""

# This list is duplicated in rosewright_X_configure.html.
langs = [
    [ 'en_US', 'English' ],
    [ 'fr_FR', 'French' ],
    [ 'it_IT', 'Italian' ],
    [ 'es_ES', 'Spanish' ],
    [ 'pt_PT', 'Portuguese' ],
    [ 'de_DE', 'German' ],
    [ 'nl_NL', 'Dutch' ],
    [ 'da_DK', 'Danish' ],
    [ 'sv_SE', 'Swedish' ],
    [ 'no_NO', 'Norwegian' ],
    [ 'is_IS' ,'Icelandic' ],
    ];

# Non-standard font required for these:
langs += [
    [ 'el_GR', 'Greek' ],
    [ 'hu_HU', 'Hungarian' ],
    [ 'ru_RU', 'Russian' ],
    [ 'pl_PL', 'Polish' ],
    [ 'cs_CZ', 'Czech' ],
    ]

# Attempt to determine the directory in which we're operating.
rootDir = os.path.dirname(__file__) or '.'
resourcesDir = rootDir
neededChars = set()

def makeDates(generatedTable, li):
    localeName, langName = langs[li]    
    print localeName
    locale.setlocale(locale.LC_ALL, localeName)

    weekdayNames = []
    for sym in [locale.ABDAY_1, locale.ABDAY_2, locale.ABDAY_3, locale.ABDAY_4, locale.ABDAY_5, locale.ABDAY_6, locale.ABDAY_7]:
        name = locale.nl_langinfo(sym)
        #name = name.decode('utf-8').upper().encode('utf-8')
        for char in name.decode('utf-8'):
            neededChars.add(char)

        if '"' in name or name.encode('string_escape') != name:
            # The text has some fancy characters.  We can't just pass
            # string_escape, since that's not 100% C compatible.
            # Instead, we'll aggressively hexify every character.
            name = ''.join(map(lambda c: '\\x%02x' % (ord(c)), name))
        weekdayNames.append(' \"%s\",' % (name))
        
    print >> generatedTable, """  { "%s", {%s } }, // %s = %s""" % (localeName, ''.join(weekdayNames), li, langName)

def makeLang():
    generatedTable = open('%s/lang_table.c' % (resourcesDir), 'w')

    numLangs = len(langs)
    print >> generatedTable, "LangDef lang_table[%s] = {" % (numLangs)

    for li in range(numLangs):
        makeDates(generatedTable, li)

    print >> generatedTable, "};\n"

    print >> generatedTable, "int num_langs = %s;\n" % (numLangs)

    chars = list(neededChars)
    chars.sort()
    print >> generatedTable, "// Requires %s chars: %s" % (len(chars), map(ord, chars))
    

# Main.
try:
    opts, args = getopt.getopt(sys.argv[1:], 'h')
except getopt.error, msg:
    usage(1, msg)

for opt, arg in opts:
    if opt == '-h':
        usage(0)

makeLang()
