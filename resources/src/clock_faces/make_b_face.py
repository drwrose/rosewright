# This is a tool to generate b_face.png.  If you already have this
# file and are happy with it, there's no reason to run this tool
# again.

import FaceMaker

face = FaceMaker.FaceMaker(zoom = 1.0)

face.setFg(128)
face.drawRing(1.1, width = 0.05)
face.setFg(0)
face.drawRing(1.1)
face.drawRing(1.0)
face.drawTicks(60, 0.95, 1.0, width = 0.003)
face.drawTicks(12, 0.95, 1.0, width = 0.009)

font = face.loadFont('Barkentina 1.otf', 0.15)
face.drawCircularLabels(face.standardLabels, 0.92, font, align = 'i')

face.pasteImage((0, -0.2), 'pebble_label.png', (13, 4), None)

cx, cy = 0.120, 0.15
face.pasteImage((-cx, cy), 'date_card.png', (15, 8), None)
face.pasteImage((cx, cy), 'date_card.png', (15, 8), None)

face.save('b_face.png')

print "date cards = %s" % (face.p2s(-cx, cy, cx, cy))

if False:
    # Add the hands for illustrative purposes.
    pixelScale = face.handScaleToPixelScale(0.27)
    face.pasteImage((0, 0), '../clock_hands/b_hour_hand.png', (33, 211), pixelScale, rotate = 255)
    face.pasteImage((0, 0), '../clock_hands/b_minute_hand.png', (24, 280), pixelScale, rotate = 168)
    face.save('b_face_sample.png')
    

