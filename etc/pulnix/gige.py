#!/usr/bin/env dls-python2.6

import socket, struct

seq = 1

CAMIP = "192.168.0.55"
CAMMAC = "00111cf01676"
CAMMASK = "255.255.255.0"

GVCP = 3956
GVCP_MSG = 0x4200
GVCP_MSG_ACK = 0x4201
GVCP_FORCEIP_CMD = 4
GVCP_READREG_CMD = 0x80
GVCP_WRITEREG_CMD = 0x82

def gige(typ, op, seq, data = ""):
    return struct.pack(">HHHH", typ, op, len(data), seq) + data

def gigesetup():
    mac = int(CAMMAC, 16)
    mach = (mac >> 32) & 0xffff
    macl = mac & 0xffffffff
    ips = socket.inet_aton(CAMIP)
    mask = socket.inet_aton(CAMMASK)
    gw = "\x00\x00\x00\x00"
    fmt = ">xxHIxxxxxxxxxxxx4sxxxxxxxxxxxx4sxxxxxxxxxxxx4s"
    data = struct.pack(fmt, mach, macl, ips, mask, gw)
    return gige(GVCP_MSG, GVCP_FORCEIP_CMD, 0xffff, data)

def gvcprecv(address):
    global seq
    data = struct.pack(">I", address)
    packet = gige(0x4201, 0x0080, seq, data)
    sock.sendto(packet, (CAMIP, GVCP))
    return sock.recv(2048)
    seq += 1

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.settimeout(1.0)

sock.bind(("192.168.0.1", GVCP))

print repr(gvcprecv(0x93c))
print repr(gvcprecv(0x940))

