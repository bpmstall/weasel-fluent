import sys
from PySide6.QtGui import QGuiApplication, QFont, QPainter, QImage, QColor, QPainterPath, QFontMetricsF
from PySide6.QtCore import Qt, QPointF, QRectF

app = QGuiApplication(sys.argv)

def test_emoji():
    img = QImage(300, 60, QImage.Format.Format_ARGB32_Premultiplied)
    img.fill(QColor(32, 32, 32))
    p = QPainter(img)
    p.setRenderHint(QPainter.RenderHint.Antialiasing, True)
    p.setRenderHint(QPainter.RenderHint.TextAntialiasing, True)

    # Text via path
    f_cn = QFont("Microsoft YaHei UI", 12)
    fm_cn = QFontMetricsF(f_cn)
    base_y = (60.0 + fm_cn.ascent() - fm_cn.descent()) * 0.5
    path = QPainterPath()
    path.addText(QPointF(20, base_y), f_cn, "1 你好")
    p.fillPath(path, QColor(255, 255, 255))

    # Emoji via drawText
    f_em = QFont("Segoe UI Emoji", 14)
    p.setFont(f_em)
    p.drawText(QRectF(100, 10, 40, 40), Qt.AlignmentFlag.AlignCenter, "👋")

    p.end()
    img.save("test_emoji_render.png")
    print("Saved test_emoji_render.png")

if __name__ == '__main__':
    test_emoji()
