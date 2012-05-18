#!/usr/bin/env dls-python2.6

"""IP stuffing utility for Pulnix RM-6740GE
Based on sniffing the Windows Coyote app"""

import socket, struct, time, traceback

from PIL import Image

GVCP = 3956
GevCCPReg = 0x00000a00
GevHeartbeatTimeoutReg = 0x00000938
GevSCDAReg = 0x00000d18
GevSCPDReg = 0x00000d08
GevSCPHostPortReg = 0x00000d00
GevSCPSPacketSizeReg = 0x00000d04
GevMCDAReg = 0x00000b10
GevMCPHostPortReg = 0x00000b00
AcquisitionModeReg = 0x0000d310
AcquisitionStartReg = 0x0000d314

OffsetXReg = 0x0000d31c
OffsetYReg = 0x0000d320

def gige(typ, op, seq, data = ""):
    sz = len(data)
    return struct.pack(">HHHH", typ, op, sz, seq) + data

def gigesetup():
    mach = 0x0011
    macl = 0x1cf01676
    ip = 0xc0a8000a
    mask = 0xffffff00
    pad2 = "\x00" * 2
    pad12 = "\x00" * 12
    pad16 = "\x00" * 16
    data = struct.pack(">2sHI12sI12sI16s",
                       pad2, mach, macl, pad12, ip, pad12, mask, pad16)
    return gige(0x4200, 0x0004, 0xffff, data)

def showhex(xs):
    for i, x in enumerate(xs):
        print "%02x" % ord(x),
        if (i + 1) % 16 == 0:
            print

# sequence number must not be zero, must increment
seq = 1

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
sock.settimeout(0.5)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

sock.bind(("192.168.0.1", GVCP))

# who is a GigE camera? (can't get the reply because they don't have a good IP address!)
sock.sendto(discovery, ("255.255.255.255", GVCP))
# set the IP address
sock.sendto(setup, ("255.255.255.255", GVCP))
# print repr(sock.recv(2048))

hostip = 0xc0a80001

time.sleep(1.0)

gvcpsend(GevCCPReg, 2)

# gvcpsendm(OffsetXReg, 0)
# gvcpsendm(OffsetYReg, 0)

gvcpsend(GevSCPSPacketSizeReg, 0x5dc)
gvcpsend(GevSCDAReg, hostip)
gvcpsend(GevSCPDReg, 10000)
gvcpsend(GevSCPHostPortReg, GVCP)

gvcpsend(AcquisitionModeReg, 1)
gvcpsend(AcquisitionStartReg, 1)

bufs = [0] * 1024
maxidx = 0

while True:
    try:
        (a, b) = sock.recvfrom(2048)
        if b[1] == 20202:
            header = struct.unpack(">HHBBH", a[:8])
            idx = header[-1]
            maxidx = idx
            print "Frame", header, idx
            if header[2] == 3:
                # image
                bufs[idx - 1] = a[8:]
            elif header[2] == 1:
                fmt = ">HHBBHHHIIIIIIIHH"
                sz = struct.calcsize(fmt)
                print struct.unpack(fmt, a[:sz])
    except socket.timeout:
        traceback.print_exc()
        break

gvcpsend(GevCCPReg, 0)

bufs = "".join(bufs[:maxidx-1])
print len(bufs), 640*480

img = Image.fromstring("L", (640, 480), bufs)

img.show()
