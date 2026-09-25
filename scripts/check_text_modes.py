import sys
from PySide6.QtGui import QGuiApplication, QFont, QPainter, QImage, QColor, QRawFont, QGlyphRun
from PySide6.QtCore import Qt, QRectF, QPointF
import numpy as np
from PIL import Image

app = QGuiApplication(sys.argv)

def check_one(name, fn):
    img = QImage(60, 40, QImage.Format.Format_ARGB32_Premultiplied)
    img.fill(QColor(32, 32, 32))
    p = QPainter(img)
    p.setRenderHint(QPainter.RenderHint.Antialiasing, True)
    fn(p)
    p.end()

    img.save(f"test_{name}.png")
    im = Image.open(f"test_{name}.png").convert("RGB")
    arr = np.array(im)
    diff = np.abs(arr[:, :, 0].astype(int) - arr[:, :, 2].astype(int))
    print(f"{name:35s}: max |R-B| = {np.max(diff)}")

def main():
    def t_drawtext_default(p):
        p.setRenderHint(QPainter.RenderHint.TextAntialiasing, True)
        f = QFont("Microsoft YaHei UI", 12)
        p.setFont(f)
        p.setPen(QColor(255, 255, 255))
        p.drawText(QRectF(10, 5, 40, 30), Qt.AlignmentFlag.AlignVCenter, "你")

    def t_drawtext_no_ta(p):
        p.setRenderHint(QPainter.RenderHint.TextAntialiasing, False)
        f = QFont("Microsoft YaHei UI", 12)
        p.setFont(f)
        p.setPen(QColor(255, 255, 255))
        p.drawText(QRectF(10, 5, 40, 30), Qt.AlignmentFlag.AlignVCenter, "你")

    def t_path(p):
        f = QFont("Microsoft YaHei UI", 12)
        from PySide6.QtGui import QPainterPath
        path = QPainterPath()
        path.addText(QPointF(10, 26), f, "你")
        p.fillPath(path, QColor(255, 255, 255))

    check_one("drawtext_default", t_drawtext_default)
    check_one("drawtext_no_ta", t_drawtext_no_ta)
    check_one("path", t_path)

if __name__ == '__main__':
    main()
