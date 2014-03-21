# This is a tool to generate a_face.png.  If you already have this
# file and are happy with it, there's no reason to run this tool
# again.

import FaceMaker
import PIL.Image

face = FaceMaker.FaceMaker(zoom = 0.92)

# 1.0 = 634 ref pixels

#face.setFg(128)
face.setFg(255)
face.fillCircle(1.0)

face.setFg(255)
face.drawCircularSpots(12, 0.968, spotDiameter = 0.05, width = None)

face.setFg(0)
face.drawRing(1.6, width = 0.25)

face.flush()
ring = PIL.Image.open('d_face_ring.png')
color, mask = ring.split()
face.target.paste(color, (0, 0), mask)

face.drawRing(1.0, width = 0.01)
face.setFg(0)
face.drawTicks(60, 0.876, 1.0, width = 0.003)
#face.drawTicks(300, 0.957, 1.0, width = 0.003)
face.drawCircularSpots(12, 0.95, spotDiameter = 0.05, width = 0.005)
face.drawRing(0.084, width = 0.0347)

font = face.loadFont('Multicolore.otf', 0.13)
face.drawCircularLabels(face.standardLabels, 0.86, font, align = 'i')

cx, cy = 0.120, 0.15
#face.pasteImage((-cx, cy), 'date_card_black.png', (15, 8), None)
#face.pasteImage((cx, cy), 'date_card_black.png', (15, 8), None)

face.save('d_face.png')
print "handScale = %s" % (face.pixelScaleToHandScale(634))

print "date cards = %s" % (face.p2s(-cx, cy, cx, cy))

if False:
    # Add the hands for illustrative purposes.
    pixelScale = face.handScaleToPixelScale(0.24)
    face.pasteImage((0, 0), '../clock_hands/d_hour_hand.png', (24, 193), pixelScale, rotate = 255)
    face.pasteImage((0, 0), '../clock_hands/d_minute_hand.png', (27, 267), pixelScale, rotate = 168)
    face.save('d_face_sample.png')
    

