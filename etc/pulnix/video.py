#!/usr/bin/env dls-python2.6

"""IP stuffing utility for Pulnix RM-6740GE
Based on sniffing the Windows Coyote app"""

from PyQt4.QtCore import *
from PyQt4.QtGui import *

import socket, struct, time, traceback, sys, threading, ctypes, numpy

lib = ctypes.CDLL("./gvsp.so")

from Queue import Queue, Full

from xml.dom import minidom

from PIL import Image

regs = {}
genixml = "RM-6740GE.xml"
doc = minidom.parse(genixml)
for a in doc.getElementsByTagName("Address"):
    name = a.parentNode.getAttribute("Name")
    a.normalize()
    address = a.firstChild.nodeValue
    regs[name] = int(address, 16)

for k, v in regs.items():
    globals()[k] = v

TICKRATE = 2083333.0

CAMIP = "192.168.0.99"

GVCP = 3956

WIDTH = 640
HEIGHT = 480

scansel = 0
binsel = 0

scantable = [[0, 1, 1],
             [1, 3, 1],
             [2, 1, 2.8571428571428572],
             [3, 3, 2.8571428571428572]]

bintable = [[0, 1, 1],
            [4, 2, 2],
            [8, 4, 4]]

Scan, SCANY, SCANX = scantable[scansel]
Binning, XBIN, YBIN = bintable[binsel]

def gige(typ, op, seq, data = ""):
    sz = len(data)
    return struct.pack(">HHHH", typ, op, sz, seq) + data

def gigesetup():
    mach = 0x0011
    macl = 0x1cf01676
    ipl = struct.unpack(">I", socket.inet_aton(CAMIP))[0]
    mask = 0xffffff00
    pad2 = "\x00" * 2
    pad12 = "\x00" * 12
    pad16 = "\x00" * 16
    data = struct.pack(">2sHI12sI12sI16s",
                       pad2, mach, macl, pad12, ipl, pad12, mask, pad16)
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
    # showhex(packet)
    sock.sendto(packet, (CAMIP, GVCP))
    # print repr(sock.recv(2048))
    time.sleep(0.01)
    seq += 1
    if seq == 65536:
        seq = 1
    
discovery = gige(0x4201, 0x0002, 0xffff)
setup = gigesetup()

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(0.5)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
sock.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
sock.bind(("192.168.0.1", GVCP))

video = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
video.bind(("192.168.0.1", 0))
# video.settimeout(0.5)

# who is a GigE camera? (can't get the reply because they don't have a good IP address!)
sock.sendto(discovery, ("255.255.255.255", GVCP))
# set the IP address
sock.sendto(setup, ("255.255.255.255", GVCP))

hostip = struct.unpack(">I", socket.inet_aton(video.getsockname()[0]))[0]

gvcpsend(GevCCPReg, 2)

gvcpsend(BinningModeReg, Binning)
gvcpsend(ScanModeReg, Scan)

gvcpsend(WidthReg, int(WIDTH / SCANX / XBIN))
gvcpsend(HeightReg, int(HEIGHT / SCANY / YBIN))

# gvcpsend(GainRawChannelAReg, 400)
# gvcpsend(GainRawChannelBReg, 405)

gvcpsend(GevSCPSPacketSizeReg, 1500)
gvcpsend(GevSCDAReg, hostip)

gvcpsend(GevSCPDReg, 0) # no limits!
gvcpsend(GevSCPHostPortReg, video.getsockname()[1])

mbofs = 0
missed = 0
megabuffer = bytearray(1000000000)
packetbuf = ctypes.create_string_buffer(1000000)
fragments = [""] * 1000

def cgetframe():
    global missed
    while True:
        width = ctypes.c_int()
        height = ctypes.c_int()
        ts = ctypes.c_uint64()
        bytes = lib.ReadFrame(video.fileno(), packetbuf, len(packetbuf),
                              ctypes.byref(width), ctypes.byref(height),
                              ctypes.byref(ts))
        if width.value * height.value != bytes:
            missed += 1
        else:
            break
    image = packetbuf[:bytes]
    return (width.value, height.value, image)

def getframe():
    global missed
    gige_header = ["status", "block id", "packet format",
                   "packet id high", "packet id low", "reserved", "payload type",
                   "timestamp high", "timestamp low", "pixel type", "size x", "size y",
                   "offset x", "offset y", "padding x", "padding y"]

    tshi = gige_header.index("timestamp high")
    tsli = gige_header.index("timestamp low")

    header = ">HHBBHHHIIIIIIIHH"
    hsz = struct.calcsize(header)

    while True:
        width = 0
        height = 0
        ofs = 0
        gotHeader = False
        n = 0
        while True:
            packet = video.recv(2048)
            packetformat = ord(packet[4])
            if packetformat == 1:
                # header
                hdr = struct.unpack(header, packet[:hsz])
                th = hdr[tshi]
                tl = hdr[tsli]
                ts = th << 32 | tl
                # print zip(gige_header, hdr)
                (width, height) = hdr[10:12]
                # print width, height
                ofs = 0
                n = 0
                gotHeader = True
            elif packetformat == 2:
                # footer
                break
            elif packetformat == 3 and gotHeader:
                datalen = len(packet) - 8
                fragments[n] = packet[8:]
                n += 1
                ofs += datalen

        if width * height == ofs:
            break
        else:
            missed += 1

    data = "".join(fragments[:n])
    
    return (width, height, data)

nf = 0

def frames(callback):
    global mbofs, nf
    while True:
        (width, height, data) = cgetframe()
        try:
            # megabuffer[mbofs:mbofs+len(data)] = data
            mbofs += len(data)
            nf += 1
            q.put((width, height, data), block = False)
            callback()
        except Full:
            # print "video queue full, frame dropped"
            pass
    
def heartbeat():
    t0 = time.time()
    nf0 = nf
    while True:
        nf1 = nf
        t1 = time.time()
        time.sleep(1.0)
        gvcpsend(GevCCPReg, 2)
        rate = (nf1 - nf0) / (t1 - t0)
        print "fps:%f frames: %d (missed %d) %g MB" % (rate, nf, missed, mbofs * 1e-6)

class Video(QWidget):
    grey = [qRgb(n,n,n) for n in range(256)]
    frameChanged = pyqtSignal()
    def __init__(self, *args):
        super(Video, self).__init__(*args)
        self.img = QImage()
    @pyqtSlot()
    def frame(self):
        # print threading.current_thread()
        (width, height, data) = q.get()
        self.img = QImage(data, width, height, QImage.Format_Indexed8)
        self.img.setColorTable(self.grey)
        # keep data for lifetime of QImage
        self.data = data
        self.update()
    def paintEvent(self, event):
        p = QPainter(self)
        p.drawImage(self.rect(), self.img, self.img.rect())
        p.end()
    def sizeHint(self):
        return QSize(WIDTH, HEIGHT)

# main program
        
app = QApplication(sys.argv)

b = QWidget()
b.setWindowTitle("gigE")
l = QVBoxLayout()
v = Video(b)
v.frameChanged.connect(v.frame)
l.addWidget(v)

def setP1():
    gvcpsend(GainRawChannelAReg, int(p1.text()))

def setP2():
    gvcpsend(GainRawChannelBReg, int(p2.text()))

p1 = QLineEdit(b)
p1.returnPressed.connect(setP1)
p2 = QLineEdit(b)
p2.returnPressed.connect(setP2)
l.addWidget(p1)
l.addWidget(p2)
b.setLayout(l)
b.show()

q = Queue(10)

threading.Thread(target = heartbeat).start()
threading.Thread(target = frames, args = (v.frameChanged.emit,)).start()

gvcpsend(AcquisitionModeReg, 0)
gvcpsend(AcquisitionStartReg, 1)

app.exec_()

gvcpsend(GevCCPReg, 0)

sys.exit(0)

