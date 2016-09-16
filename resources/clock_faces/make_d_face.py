# This is a tool to generate a_face.png.  If you already have this
# file and are happy with it, there's no reason to run this tool
# again.

import FaceMaker
import PIL.Image
import sys

#platform = 'aplite'
#platform = 'basalt'
#platform = 'chalk'
platform = 'emery'

#layer = 'ticks'
#layer = 'foreground'
layer = 'background'
#layer = 'numerals'

if platform in ['aplite', 'basalt']:
    screenSize = (144, 168); zoom = 0.92
elif platform == 'chalk':
    screenSize = (180, 180); zoom = 0.98
elif platform == 'emery':
    screenSize = (200, 228); zoom = 0.92

face = FaceMaker.FaceMaker(zoom = zoom, screenSize = screenSize)

# 1.0 = 634 ref pixels

if layer == 'ticks':
    face.drawTicks(60, 0.2, 1.7, width = 0.003)

elif layer == 'foreground':
    #face.setFg(255)
    #face.fillCircle(1.0)
    ## face.setFg(255)
    ## face.drawCircularSpots(12, 0.968, spotDiameter = 0.05, width = None)

    face.setFg(0)
    face.drawRing(1.6, width = 0.25)
    face.drawRing(1.0, width = None)

    #face.drawTicks(60, 0.876, 1.0, width = 0.003)
    #face.drawTicks(60, 0.2, 1.7, width = 0.003)
    face.drawCircularSpots(12, 0.95, spotDiameter = 0.05, width = 0.005)

elif layer == 'background':
    face.setFg(0)
    face.drawRing(1.0, width = 0.01)
    face.drawCircularSpots(12, 0.95, spotDiameter = 0.05, width = None)

elif layer == 'numerals':
    face.setFg(0)

    face.drawRing(0.084, width = 0.0347)
    font = face.loadFont('Multicolore.otf', 0.13)
    face.drawCircularLabels(face.standardLabels, 0.86, font, align = 'i')

face.save('d_face_working.png')
