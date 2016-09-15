# This is a tool to generate b_face.png.  If you already have this
# file and are happy with it, there's no reason to run this tool
# again.

import FaceMaker

#platform = 'aplite'
#platform = 'basalt'
#platform = 'chalk'
platform = 'emery'
#layer = 'ticks'
#layer = 'axle'
layer = 'numerals'

if platform in ['aplite', 'basalt']:
    face = FaceMaker.FaceMaker(zoom = 1.0, screenSize = (144, 168))
elif platform == 'chalk':
    face = FaceMaker.FaceMaker(zoom = 0.98, screenSize = (180, 180))
elif platform == 'emery':
    face = FaceMaker.FaceMaker(zoom = 1.0, screenSize = (200, 228))

if layer == 'numerals':
    font = face.loadFont('Barkentina 1.otf', 0.15)
    face.drawCircularLabels(face.standardLabels, 0.92, font, align = 'i')

#face.setFg(128)
#face.drawRing(1.1, width = 0.05)
#face.setFg(0)
#face.drawRing(1.1)
#face.drawRing(1.0)

if layer == 'axle':
    face.drawRing(0.093, width = 0.028)

if layer == 'ticks':
    if platform == 'chalk':
        face.drawTicks(60, 0.95, 1.0, width = 0.003)
        face.drawTicks(12, 0.95, 1.0, width = 0.009)
    else:
        face.drawTicks(60, 0.2, 1.5, width = 0.003)
        face.drawTicks(12, 0.2, 1.5, width = 0.009)

face.save('b_face_working.png')
