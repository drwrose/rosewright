# This is a tool to generate a_face.png.  If you already have this
# file and are happy with it, there's no reason to run this tool
# again.

import FaceMaker

# aplite, basalt
#face = FaceMaker.FaceMaker(zoom = 0.92, screenSize = (144, 168))

# chalk
face = FaceMaker.FaceMaker(zoom = 0.98, screenSize = (180, 180))

#face.drawRing(1.0)

face.drawTicks(60, 0.95, 1.0, width = 0.003); face.drawTicks(12, 0.95, 1.0, width = 0.009); face.drawRing(0.0512, 0.0225)

font = face.loadFont('effloresce.otf', 0.12)
print font
#face.drawCircularLabels(face.romanLabels, 0.95, font, align = 'i', rotate = True)
#face.drawCircularLabels(face.romanLabels, 0.92, font, align = 'i', rotate = False)

face.pasteImage((0.47, 0.53), 'corner_trim.png', (29, 29), None)
face.pasteImage((-0.47, 0.53), 'corner_trim.png', (29, 29), None, rotate = 90)
face.pasteImage((-0.47, -0.53), 'corner_trim.png', (29, 29), None, rotate = 180)
face.pasteImage((0.47, -0.53), 'corner_trim.png', (29, 29), None, rotate = 270)

face.save('a_face_working.png')
