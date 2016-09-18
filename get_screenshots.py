#! /usr/bin/env python

import sys
import os
import glob
import getopt
import zipfile
import json
import subprocess
import shutil

help = """
get_screenshots.py

Captures screenshots from an already-build pbw (or series of pbw's) as
generated by make_all.sh, and stores them into resources/clock_faces.

get_screenshots.py [opts]

Options:

    -b file.pbw[,file.pbw...]
        Specifies the pbw or pbw's that will be used.  The default is
        build/rosewright_*.pbw.

    -p platform[,platform...]
        Specifies the build platform.  The default is all platforms
        supported by the named pbw's.

"""

def usage(code, msg = ''):
    print >> sys.stderr, help
    print >> sys.stderr, msg
    sys.exit(code)

watches = {
    'Rosewright A' : 'a',
    'Rosewright B' : 'b',
    'Rosewright C' : 'c',
    'Rosewright Chronograph' : 'c2',
    'Rosewright D' : 'd',
    'Rosewright E' : 'e',
    }

def send_pebble_command(*args):
    args = ['pebble'] + list(args)
    status = subprocess.call(args)
    if status == 0:
        return True
    print "Command failed: %s" % (args,)
    return False

def get_screenshots_for_platform(pbw, id, platform):
    print pbw, id, platform
    if not send_pebble_command('wipe'):
        return False

    if not send_pebble_command('install', '--emulator', platform, pbw):
        return False

    for i in range(5):
        filename = '%s_%s_%s.png' % (id, platform, i + 1)
        print filename
        if os.path.exists(filename):
            os.unlink(filename)
        if not send_pebble_command('screenshot', '--emulator', platform, filename):
            return False
        if not os.path.exists(filename):
            print "Screenshot file not created: %s" % (filename)
            return False
        targetFilename = 'resources/clock_faces/%s_screenshots/%s' % (id, filename)
        print targetFilename
        if os.path.exists(targetFilename):
            os.unlink(targetFilename)
        shutil.move(filename, targetFilename)

        if not send_pebble_command('emu-tap', '--emulator', platform):
            return False

    if not send_pebble_command('kill'):
        return False

    return True

def get_screenshots(pbw, platforms):
    try:
        zip = zipfile.ZipFile(pbw, 'r')
    except IOError:
        print "Could not open %s" % (pbw)
        return False
    try:
        appinfoData = zip.read('appinfo.json')
    except KeyError:
        print "Could not find appinfo.json in %s" % (pbw)
        return False

    zip = None
    appinfo = json.loads(appinfoData)
    targetPlatforms = appinfo['targetPlatforms']
    name = appinfo['name']
    if name not in watches:
        print "Unknown watch name '%s' in %s" % (name, pbw)
        return False
    id = watches[name]

    if platforms:
        unsupported = set(platforms) - set(targetPlatforms)
        if unsupported:
            print "Platform %s not supported by %s" % (','.join(sorted(unsupported)), pbw)
            return False
    else:
        platforms = targetPlatforms

    for platform in platforms:
        if not get_screenshots_for_platform(pbw, id, platform):
            return False

    return True


# Main.
try:
    opts, args = getopt.getopt(sys.argv[1:], 'b:p:h')
except getopt.error, msg:
    usage(1, msg)

pbws = []
targetPlatforms = []
for opt, arg in opts:
    if opt == '-b':
        pbws += arg.split(',')
    elif opt == '-p':
        targetPlatforms += arg.split(',')
    elif opt == '-h':
        usage(0)

if not pbws:
    pbws = glob.glob('build/rosewright_*.pbw')
    pbws.sort()

if not pbws:
    print >> sys.stderr, "No pbw's found."
    sys.exit(1)

for pbw in pbws:
    if not get_screenshots(pbw, targetPlatforms):
        sys.exit(1)