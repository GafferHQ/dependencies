#!/usr/bin/env python 

import OpenImageIO as oiio

# Make a ramp image, left half is increating r at rate dr/ds = 0.5, dr/dt = 0,
# right half is increasing r at rate dr/ds = 0, dr/dt = 0.5
b = oiio.ImageBuf (oiio.ImageSpec(128,128,3,oiio.FLOAT))
for y in range(128) :
    for x in range(128) :
        if x < 64 :
            b.setpixel (x, y, 0, (float(0.5*x)/128.0, 0.0, 0.1))
        else:
            b.setpixel (x, y, 0, (float(0.5*y)/128.0, 0.0, 0.1))
b.write ("ramp.exr")

command = testtex_command ("-res 128 128 -nowarp -wrap clamp -derivs ramp.exr")
outputs = [ "out.exr", "out.exr-ds.exr", "out.exr-dt.exr" ]
