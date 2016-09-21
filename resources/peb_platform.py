""" This module contains some functions and definitions for dealing
with the different combinations of Pebble hardware platforms. """

import os

# Screen sizes by shape token.
screenSizes = {
    'rect' : (144, 168),
    'round' : (180, 180),
    'emery' : (200, 228),
    }

def getPlatformShape(platform):
    """ Returns the shape token for a particular platform. """

    if platform in ['aplite', 'basalt', 'diorite']:
        shape = 'rect'
    elif platform in ['chalk']:
        shape = 'round'
    elif platform in ['emery']:
        shape = 'emery'
    else:
        raise StandardError
    return shape

def getPlatformColor(platform):
    """ Returns the color token for a particular platform. """

    if platform in ['aplite', 'diorite']:
        color = 'bw'
    elif platform in ['chalk', 'basalt', 'emery']:
        color = 'color'
    else:
        raise StandardError
    return color

def getVariantsForPlatform(platform):
    """ Returns the filename variants that might be applied to a file
    based on the platform. """

    variants = []

    shapeVariants = []
    colorVariants = []

    shape = getPlatformShape(platform)
    color = getPlatformColor(platform)

    if color == 'bw':
        colorVariants.append('~bw')
    elif color == 'color':
        colorVariants.append('~color')

    if shape == 'rect':
        shapeVariants.append('~rect')
    elif shape == 'round':
        shapeVariants.append('~round')
    elif shape == 'emery':
        shapeVariants.append('~emery')

    for sv in shapeVariants:
        for cv in colorVariants:
            variants.append(cv + sv)

    for sv in shapeVariants:
        variants.append(sv)
    for cv in colorVariants:
        variants.append(cv)

    return variants

def getPlatformFilenameAndVariant(filename, platform, prefix = ''):
    """ Returns the (filename, variant) pair, after finding the
    appropriate filename modified with the ~variant for the
    platform. """

    basename, ext = os.path.splitext(filename)

    for variant in getVariantsForPlatform(platform) + ['']:
        if os.path.exists(prefix + basename + variant + ext):
            return basename + variant + ext, variant

    raise StandardError, 'No filename for %s, platform %s' % (filename, platform)


def getPlatformFilename(filename, platform, prefix = ''):
    """ Returns the filename modified with the ~variant for the
    platform. """

    return getPlatformFilenameAndVariant(filename, platform, prefix = prefix)[0]
