#!/usr/bin/env dls-python2.4

import sys
from qt import *
from threading import Thread
from Queue import Queue, Empty
from PIL import Image

buf = Queue()

class Widget(QLabel):
    def __init__(self, *args):
        QLabel.__init__(self,  *args)
    def timerEvent(self, e):
        try:
            val = buf.get_nowait()
            img = Image.fromstring("L", (640, 480), val)
            rgb = img.convert("RGB").tostring()
            self.setPixmap(QPixmap(QImage(rgb, 640, 480, 24, None, 1, QImage.IgnoreEndian)))
        except Empty:
            pass

q = QApplication(sys.argv)
w = Widget("hello", None)
w.startTimer(100)

q.setMainWidget(w)
w.show()

def worker():
    n = 0
    while True:
        data = sys.stdin.read(640*480)
        if len(data) == 0:
            break
        n += 1
        buf.put(data)

Thread(target = worker).start()

q.exec_loop()
