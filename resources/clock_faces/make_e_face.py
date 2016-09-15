# This is a tool to generate e_face.png.  If you already have this
# file and are happy with it, there's no reason to run this tool
# again.

import FaceMaker
import PIL.Image

#platform = 'aplite'
#platform = 'basalt'
#platform = 'chalk'
platform = 'emery'
layer = 'ticks'

if platform in ['aplite', 'basalt']:
    face = FaceMaker.FaceMaker(zoom = 1.0, screenSize = (144, 168))
elif platform == 'chalk':
    face = FaceMaker.FaceMaker(zoom = 1.0, screenSize = (180, 180))
elif platform == 'emery':
    face = FaceMaker.FaceMaker(zoom = 1.0, screenSize = (200, 228))

if layer == 'ticks':
    face.drawTicks(60, 0.2, 1.7, width = 0.003); face.drawTicks(12, 0.2, 1.7, width = 0.009)

face.save('e_face_working.png')
