
# this list also appears in src/js/pebble-js-app.js.in.
config_langs = [ 'en', 'es', 'de', 'fr', 'bg' ]

# this version number also appears in appinfo.json.in and in
# src/js/pebble-js-app.js.in.
version = '4_0'

source = open('rosewright_configure.html.in', 'r').read()

for lang in config_langs:
    dict = {
        'lang' : lang,
        'version' : version,
        }

    filename = 'rosewright_%(version)s_configure.%(lang)s.html' % dict
    print filename
    open(filename, 'w').write(source % dict)

