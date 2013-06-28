# This is a tool to generate a_face.png.  If you already have this
# file and are happy with it, there's no reason to run this tool
# again.

import FaceMaker

face = FaceMaker.FaceMaker(zoom = 0.92)

# 1.0 = 634 ref pixels

#face.setFg(128); face.fill()

face.setFg(0)
face.drawRing(1.1, width = 0.05)
face.drawTicks(60, 0.938, 1.0, width = 0.003)
#face.drawTicks(300, 0.957, 1.0, width = 0.003)
face.drawCircularSpots(12, 0.968, spotDiameter = 0.0268, width = 0.005)
face.drawRing(0.084, width = 0.0347)

font = face.loadFont('Multicolore.otf', 0.09)
face.drawCircularLabels(face.standardLabels, 0.93, font, align = 'i')

face.save('d_face.png')
print "handScale = %s" % (face.pixelScaleToHandScale(634))
