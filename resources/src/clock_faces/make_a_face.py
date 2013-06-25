# This is a tool to generate a_face.png.  If you already have this
# file and are happy with it, there's no reason to run this tool
# again.

import FaceMaker

face = FaceMaker.FaceMaker(zoom = 0.92)

face.drawRing(1.0)
face.drawTicks(60, 0.95, 1.0, width = 0.003)
face.drawTicks(12, 0.95, 1.0, width = 0.009)

font = face.loadFont('effloresce.otf', 0.12)
face.drawCircularLabels(face.romanLabels, 0.95, font, align = 'i')

face.pasteImage((0, -0.2), 'pebble_label.png', (13, 4), None)
face.pasteImage((0.47, 0.53), 'corner_trim.png', (29, 29), None)
face.pasteImage((-0.47, 0.53), 'corner_trim.png', (29, 29), None, rotate = 90)
face.pasteImage((-0.47, -0.53), 'corner_trim.png', (29, 29), None, rotate = 180)
face.pasteImage((0.47, -0.53), 'corner_trim.png', (29, 29), None, rotate = 270)

cx, cy = 0.220, -0.010
face.pasteImage((cx, cy), 'date_card.png', (15, 8), None)
print "date card = %s" % (face.p2s(cx, cy))

face.save('a_face.png')

if False:
    # Add the hands for illustrative purposes.
    pixelScale = face.handScaleToPixelScale(0.12)
    face.pasteImage((0, 0), '../clock_hands/a_hour_hand.png', (78, 410), pixelScale, rotate = 255)
    face.pasteImage((0, 0), '../clock_hands/a_minute_hand.png', (37, 557), pixelScale, rotate = 168)
    face.save('a_face_sample.png')
    

