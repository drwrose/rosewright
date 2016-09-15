# This is a tool to generate a_face.png.  If you already have this
# file and are happy with it, there's no reason to run this tool
# again.

import FaceMaker

#platform = 'aplite'
#platform = 'basalt'
#platform = 'chalk'
platform = 'emery'
layer = 'background'
#layer = 'numerals'
#layer = 'numerals_unrotated'

if platform in ['aplite', 'basalt']:
    face = FaceMaker.FaceMaker(zoom = 0.92, screenSize = (144, 168))
elif platform == 'chalk':
    face = FaceMaker.FaceMaker(zoom = 0.98, screenSize = (180, 180))
elif platform == 'emery':
    face = FaceMaker.FaceMaker(zoom = 0.92, screenSize = (200, 228))

font = face.loadFont('effloresce.otf', 0.12)
if layer == 'numerals':
    face.drawCircularLabels(face.romanLabels, 0.95, font, align = 'i', rotate = True)
if layer == 'numerals_unrotated':
    face.drawCircularLabels(face.romanLabels, 0.92, font, align = 'i', rotate = False)

if layer == 'background':
    if platform in ['aplite', 'basalt', 'emery']:
        face.drawRing(1.0)

    face.drawTicks(60, 0.95, 1.0, width = 0.003); face.drawTicks(12, 0.95, 1.0, width = 0.009); face.drawRing(0.0512, 0.0225)
    cornerPixelScale = 168
    face.pasteImage((0.47, 0.53), 'corner_trim.png', (29, 29), cornerPixelScale)
    face.pasteImage((-0.47, 0.53), 'corner_trim.png', (29, 29), cornerPixelScale, rotate = 90)
    face.pasteImage((-0.47, -0.53), 'corner_trim.png', (29, 29), cornerPixelScale, rotate = 180)
    face.pasteImage((0.47, -0.53), 'corner_trim.png', (29, 29), cornerPixelScale, rotate = 270)

face.save('a_face_working.png')
