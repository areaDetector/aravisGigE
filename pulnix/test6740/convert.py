#!/usr/bin/env dls-python2.4

import sys, os, re
from PIL import Image

files = os.listdir(".")

for f in files:

    if re.match("frame(.*?)[.]raw$", f):
        print f
        bytes = file(f).read()
        img = Image.fromstring("L", (640, 480), bytes)
        img.save(f + ".jpg")


