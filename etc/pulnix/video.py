#!/usr/bin/env dls-python2.6

"""IP stuffing utility for Pulnix RM-6740GE
Based on sniffing the Windows Coyote app"""

from PyQt4.QtCore import *
from PyQt4.QtGui import *

import socket, struct, time, traceback, sys, threading

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

discovery = gige(0x4201, 0x0002, 0xffff)
setup = gigesetup()

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(0.5)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.bind(("192.168.0.1", GVCP))

video = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
video.bind(("192.168.0.1", 0))
video.settimeout(0.5)

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

tslast = 0

# new in python2.6
bytes = bytearray(100000)
mbofs = 0
megabuffer = bytearray(1000000000)
rate = 0

packetbuf = bytearray(10000)

def getframe():

    ofs = 0

    global tslast, tlast, rate

    gige_header = ["status", "block id", "packet format", "packet id high", "packet id low", "reserved", "payload type",
                   "timestamp high", "timestamp low", "pixel type", "size x", "size y",
                   "offset x", "offset y", "padding x", "padding y"]

    header1 = ">HHBBH"
    header2 = ">HHBBHHHIIIIIIIHH"
    hsz1 = struct.calcsize(header1)
    hsz2 = struct.calcsize(header2)

    width = 0
    height = 0
    maxidx = 0
    while True:
        # (a, b) = video.recvfrom(2048)
        a = video.recv(2048)
        header = struct.unpack(header1, a[:8])
        idx = header[-1]
        maxidx = idx
        if header[2] == 3:
            # image
            datalen = len(a) - 8
            bytes[ofs:ofs+datalen] = a[8:]
            ofs += datalen
        elif header[2] == 1:
            # header
            hdr = struct.unpack(header2, a[:hsz2])
            th = hdr[gige_header.index("timestamp high")]
            tl = hdr[gige_header.index("timestamp low")]
            ts = th << 32 | tl
            # print ts
            rate = 1.0 / (ts - tslast) * TICKRATE
            tslast = ts
            # print zip(gige_header, hdr)
            (width, height) = hdr[10:12]
            # print width, height
        elif header[2] == 2:
            # footer
            break
        else:
            print "unknown packet", header

    # copy before passing to other thread
    data = bytes[:ofs]
    
    # return (width / XBIN / SCANX, height / YBIN / SCANY, data)
    return (width, height, data)

nf = 0

def frames(callback):
    global mbofs, nf
    while True:
        f = getframe()
        try:
            (width, height, data) = f
            if width * height != len(data):
                print "frame missed a block, skipping", width * height, len(data)
                continue
            # megabuffer[mbofs:mbofs+len(data)] = data
            mbofs += len(data)
            nf += 1
            q.put(f, block = False)
            callback()
        except Full:
            # print "video queue full, frame dropped"
            pass
            
    
def heartbeat():
    while True:
        print "%f fps %d frames %g MB" % (rate, nf, mbofs * 1e-6)
        gvcpsend(GevCCPReg, 2)
        time.sleep(2.0)

class Video(QWidget):
    frameChanged = pyqtSignal()
    def __init__(self, *args):
        super(Video, self).__init__(*args)
        self.img = QImage()
    @pyqtSlot()
    def frame(self):
        # print threading.current_thread()
        (width, height, data) = q.get()
        img = QImage(data, width, height, QImage.Format_Indexed8)
        img.setColorTable(grey)
        self.img = img
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
#.resize(640, 480)
b.show()

grey = [qRgb(n,n,n) for n in range(256)]

q = Queue(10)

threading.Thread(target = heartbeat).start()
threading.Thread(target = frames, args = (v.frameChanged.emit,)).start()

gvcpsend(AcquisitionModeReg, 0)
gvcpsend(AcquisitionStartReg, 1)

app.exec_()

gvcpsend(GevCCPReg, 0)

sys.exit(0)

# discover binning
