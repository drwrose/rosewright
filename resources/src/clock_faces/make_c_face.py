# This is a tool to generate c_face.png.  If you already have this
# file and are happy with it, there's no reason to run this tool
# again.

import FaceMaker

# Set true to generate c1_face.png, without the chrono dials baked in.
forChronograph = True

face = FaceMaker.FaceMaker(zoom = 1.25, bg = 0, fg = 255) #, upscale = 2, format = 'L')

rings = [0.2933, 0.3600, 0.4273, 0.4920, 0.6500, 0.6713, 0.7047, 0.7180, 0.8027, 0.8173]
    
rings.reverse()
for diameter in rings:
    face.drawRing(diameter, width = 0.003)
rings.reverse()

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
    face.drawTicks(smallTicks, 0.2293, 0.2520, width = 0.001, center = c)
    face.drawTicks(bigTicks, 0.2000, 0.2293, width = 0.002, center = c)

    font = face.loadFont('Multicolore.otf', 0.038)
    face.drawCircularLabels(labels, 0.2200, font, center = c, align = 'i')

    if not forChronograph:
        # If the dials won't be functional, paint them in.
        face.pasteImage(c, '../clock_hands/%s' % (handFilename), handPivot, 1500)
    else:
        # If they will be functional, we want to know where to place them.
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

if not forChronograph:
    face.save('c_face.png')
else:
    face.save('c1_face.png')
print "handScale = %s" % (face.pixelScaleToHandScale(1500))

