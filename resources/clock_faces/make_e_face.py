# This is a tool to generate a_face.png.  If you already have this
# file and are happy with it, there's no reason to run this tool
# again.

import FaceMaker
import PIL.Image

# aplite, basalt
#face = FaceMaker.FaceMaker(zoom = 1.0, screenSize = (144, 168))

# chalk
face = FaceMaker.FaceMaker(zoom = 1.0, screenSize = (180, 180))

#face.drawTicks(60, 0.5, 2.0, width = 0.003)
#face.drawTicks(12, 0.5, 2.0, width = 0.01)
face.drawTicks(60, 0.2, 1.7, width = 0.003); face.drawTicks(12, 0.2, 1.7, width = 0.009)

# 1.0 = 317 ref pixels

face.save('e_face_ticks.png')
