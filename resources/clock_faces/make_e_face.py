# This is a tool to generate a_face.png.  If you already have this
# file and are happy with it, there's no reason to run this tool
# again.

import FaceMaker
import PIL.Image

face = FaceMaker.FaceMaker(zoom = 1.0)

face.drawTicks(60, 0.5, 2.0, width = 0.003)
#face.drawTicks(12, 0.5, 2.0, width = 0.01)

# 1.0 = 317 ref pixels

print "handScale = %s" % (face.pixelScaleToHandScale(317))
face.save('d_face_ticks.png')
