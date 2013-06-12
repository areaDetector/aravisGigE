#!/usr/bin/env dls-python2.6

"IP stuffing utility for GigE cameras"

import socket, struct, os, sys

GVCP = 3956
CAMIP = "192.168.0.100"
CAMMAC = "00111cf01676"
CAMMASK = "255.255.255.0"
CAMGW = "0.0.0.0"
HOSTIP = "192.168.0.1"

def gige(typ, op, seq, data = ""):
    sz = len(data)
    return struct.pack(">HHHH", typ, op, sz, seq) + data

def forceip(CAMMAC, CAMIP, CAMMASK, CAMGW):
    mac = int(CAMMAC, 16)
    mach = (mac >> 32) & 0xffff
    macl = mac & 0xffffffff
    ips = socket.inet_aton(CAMIP)
    mask = socket.inet_aton(CAMMASK)
    gw = socket.inet_aton(CAMGW)
    fmt = ">xxHIxxxxxxxxxxxx4sxxxxxxxxxxxx4sxxxxxxxxxxxx4s"
    data = struct.pack(fmt, mach, macl, ips, mask, gw)
    return gige(0x4201, 0x0004, 0xffff, data)

# this detects connected cameras
# discovery = gige(0x4201, 0x0002, 0xffff)
# sock.sendto(discovery, ("255.255.255.255", GVCP))

if len(sys.argv) != 6:
    print "usage: forceip.py HOSTIP CAMMAC CAMIP CAMMASK CAMGW"
    print "HOSTIP is your PC's IP on the interface the camera is connected, use /sbin/ifconfig"
    print "example: forceip.py 192.168.0.1 00111cf01676 192.168.0.10 255.255.255.0 0.0.0.0"
    sys.exit(1)

HOSTIP  = sys.argv[1]
CAMMAC  = sys.argv[2]
CAMIP   = sys.argv[3]
CAMMASK = sys.argv[4]
CAMGW   = sys.argv[5]

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.settimeout(1.0)
sock.bind((HOSTIP, GVCP))

setup = forceip(CAMMAC, CAMIP, CAMMASK, CAMGW)
sock.sendto(setup, ("255.255.255.255", GVCP))
print "camera replies", struct.unpack(">HHHH", sock.recv(2048))
os.system("ping -c 5 -s 32 %s" % CAMIP)
print "You might have to run this script twice..."
