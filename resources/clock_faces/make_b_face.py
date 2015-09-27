# This is a tool to generate b_face.png.  If you already have this
# file and are happy with it, there's no reason to run this tool
# again.

import FaceMaker

# aplite, basalt
#face = FaceMaker.FaceMaker(zoom = 1.0, screenSize = (144, 168))

# chalk
face = FaceMaker.FaceMaker(zoom = 1.01, screenSize = (180, 180))

#face.setFg(128)
#face.drawRing(1.1, width = 0.05)
#face.setFg(0)
#face.drawRing(1.1)
#face.drawRing(1.0)

face.drawRing(0.093, width = 0.028)

face.drawTicks(60, 0.95, 1.0, width = 0.003)
face.drawTicks(12, 0.95, 1.0, width = 0.009)

font = face.loadFont('Barkentina 1.otf', 0.15)
face.drawCircularLabels(face.standardLabels, 0.92, font, align = 'i')

face.pasteImage((0, -0.2), 'pebble_label.png', (13, 4), None)

face.save('b_face_working.png')
