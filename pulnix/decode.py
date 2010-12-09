#!/usr/bin/env python

import xml, re

from xml.dom import minidom

memmap = {}

doc = minidom.parse("geni.xml")
for a in doc.getElementsByTagName("Address"):
    name = a.parentNode.getAttribute("Name")
    a.normalize()
    address = a.firstChild.nodeValue
    memmap[int(address, 16)] = name

items = memmap.items()
items.sort()

## for k, v in items:
##     print "%08x: %s" % (k, v)

template = "arv_device_write_register(arv_camera_get_device(camera), 0x%04x, 0x%08x);"

for line in file("gige.txt"):
    line = line.strip()
    m = re.search("write request [*]0x(.*?)\s=\s0x(.*?)$", line)
    if m:
        (address, value) = m.groups()
        address = int(address, 16)
        value = int(value, 16)
        print "%-30s(%04x) = %08x" % (memmap[address], address, value)
        # print template % (address, value)
    



