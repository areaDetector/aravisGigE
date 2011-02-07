#!/usr/bin/env python

import xml, re, sys

from xml.dom import minidom

memmap = {}

genixml = sys.argv[1]
capture = sys.argv[2]

doc = minidom.parse(genixml)
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

for line in file(capture):
    line = line.strip()
    pre = re.search("write request (.*?)$", line)
    if pre:
        vals = pre.group(1)
        vals = vals.split(",")
        for v in vals:
            m = re.search("[*]0x(.*?)\s=\s0x(.*?)$", v)
            if m:
                (address, value) = m.groups()
                address = int(address, 16)
                value = int(value, 16)
                #print "%-30s(%04x) = %08x" % (memmap.get(address, "unknown"), address, value)
                print template % (address, value)
    



