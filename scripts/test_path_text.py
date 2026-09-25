import sys
from PySide6.QtGui import QGuiApplication, QFont, QPainter, QImage, QColor, QPainterPath
from PySide6.QtCore import Qt, QPointF
from PIL import Image
import numpy as np

app = QGuiApplication(sys.argv)

def test_add_text():
    img = QImage(60, 40, QImage.Format.Format_ARGB32_Premultiplied)
    img.fill(QColor(32, 32, 32))
    p = QPainter(img)
    p.setRenderHint(QPainter.RenderHint.Antialiasing, True)

    font = QFont("Microsoft YaHei UI", 12)
    path = QPainterPath()
    path.addText(QPointF(10, 26), font, "你")
    p.fillPath(path, QColor(255, 255, 255))
    p.end()

    img.save("path_text.png")
    im = Image.open("path_text.png").convert("RGB")
    arr = np.array(im)
    diff = np.abs(arr[:, :, 0].astype(int) - arr[:, :, 2].astype(int))
    max_d = np.max(diff)
    print("addText max |R-B|:", max_d)

if __name__ == '__main__':
    test_add_text()
