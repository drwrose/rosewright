# This is a tool to generate c_face.png.  If you already have this
# file and are happy with it, there's no reason to run this tool
# again.

import FaceMaker
import sys

# aplite, basalt
#face = FaceMaker.FaceMaker(zoom = 1.25, bg = 0, fg = 255, screenSize = (144, 168)) #, upscale = 2, format = 'L')

# chalk
face = FaceMaker.FaceMaker(zoom = 1.223, bg = 0, fg = 255, screenSize = (180, 180))

#face.drawTicks(60, 0.2, 1.7, width = 0.004)
#face.save('c_face_ticks.png')

#face.drawTicks(12, 0.2, 1.7, width = 0.01)
#face.save('c_face_ticks.png')
#sys.exit()


rings = [0.2933, 0.3600, 0.4273, 0.4920, 0.6500, 0.6713, 0.7047, 0.7180, 0.8027, 0.8173]

rings.reverse()
for diameter in rings:
    face.drawRing(diameter, width = 0.003)
rings.reverse()

face.fillCircle(0.048)
face.clearCircle(0.010)

# The main clock ticks.
ticks = range(0, 360, 30)[:]
ticks.remove(0)
ticks.remove(180)
face.drawTicks(ticks, rings[3], rings[4], width = 0.01)

font = face.loadFont('Multicolore.otf', 0.08)
face.drawCircularLabels([(0, '12'), (180, '6')], 0.5800, font)

face.drawTicks(60, rings[5], rings[6], width = 0.002)
face.drawTicks(60, rings[7], rings[8], width = 0.002)
face.drawTicks(240, 0.7907, rings[8], width = 0.002)
face.drawTicks(200, rings[9], 0.8200, width = 0.002)
face.drawTicks(20, rings[9], 0.8400, width = 0.002)

def drawChrono(c, smallTicks, bigTicks, labels, handFilename, handPivot):
    face.clearCircle(0.2720, center = c)
    face.drawRing(0.2520, center = c)
    face.drawRing(0.2293, center = c)
    face.fillCircle(0.048, center = c)
    face.clearCircle(0.010, center = c)
    face.drawTicks(smallTicks, 0.2293, 0.2520, width = 0.001, center = c)
    face.drawTicks(bigTicks, 0.2000, 0.2293, width = 0.002, center = c)

    font = face.loadFont('Multicolore.otf', 0.038)
    face.drawCircularLabels(labels, 0.2200, font, center = c, align = 'i')

    # Tell the developer where to place the chrono dials.
    print handFilename, face.p2s(*c)

# Draw the little chonograph dials.
drawChrono((-0.2033, 0.0), 60, 12,
           [(60, '10'), (120, '20'), (180, '30'),
            (240, '40'), (300, '50'), (0, '60')],
           'c_chrono1_hand.png', (32, 193))
drawChrono((0.2033, 0.0), 30, 0,
           [(60, '5'), (120, '10'), (180, '15'),
            (240, '20'), (300, '25'), (0, '30')],
           'c_chrono2_hand.png', (37, 195))

# tenths
#drawChrono((0, 0.2033), 0, 10, [(72, '2'), (144, '4'), (216, '6'), (288, '8'), (0, '0')], 'c_chrono2_hand.png', (37, 195))

# hours
#drawChrono((0, 0.2033), 0, 12, [(0, '12'), (180, '6')], 'c_chrono2_hand.png', (37, 195))


face.save('c_face_working.png')
#print "handScale = %s" % (face.pixelScaleToHandScale(1500))

