#!/usr/bin/env dls-python2.6

"""IP stuffing utility for Pulnix RM-6740GE
Based on sniffing the Windows Coyote app"""

import socket, struct

GVCP = 3956
CAMIP = "192.168.0.50"
CAMMAC = "00111cf01676"
CAMMASK = "255.255.255.0"

# sequence number must not be zero, must increment
seq = 1

def gige(typ, op, seq, data = ""):
    sz = len(data)
    return struct.pack(">HHHH", typ, op, sz, seq) + data

def gigesetup():
    mac = int(CAMMAC, 16)
    mach = (mac >> 32) & 0xffff
    macl = mac & 0xffffffff
    ips = socket.inet_aton(CAMIP)
    mask = socket.inet_aton(CAMMASK)
    fmt = ">xxHIxxxxxxxxxxxx4sxxxxxxxxxxxx4sxxxxxxxxxxxxxxxx"
    data = struct.pack(fmt, mach, macl, ips, mask)
    return gige(0x4200, 0x0004, 0xffff, data)

def showhex(xs):
    for i, x in enumerate(xs):
        print "%02x" % ord(x),
        if (i + 1) % 16 == 0:
            print

def gvcpsend(address, value):
    global seq
    data = struct.pack(">II", address, value)
    packet = gige(0x4201, 0x0082, seq, data)
    showhex(packet)
    sock.sendto(packet, ("192.168.0.10", GVCP))
    # print repr(sock.recv(2048))
    time.sleep(0.01)
    seq += 1

discovery = gige(0x4201, 0x0002, 0xffff)
setup = gigesetup()

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.settimeout(1.0)

sock.bind(("192.168.0.1", GVCP))

# who is a GigE camera?
# (can't get the reply yet because they don't have a good IP address!)
# sock.sendto(discovery, ("255.255.255.255", GVCP))

# set the IP address
sock.sendto(setup, ("255.255.255.255", GVCP))
