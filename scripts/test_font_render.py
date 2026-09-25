import sys
from PySide6.QtGui import QGuiApplication, QFont, QPainter, QImage, QColor, QFontMetricsF
from PySide6.QtCore import Qt, QPointF, QRectF

app = QGuiApplication(sys.argv)

def render_sample(font_family, point_size, no_subpixel=True, weight=QFont.Weight.Normal):
    img = QImage(120, 50, QImage.Format.Format_ARGB32_Premultiplied)
    img.fill(QColor(32, 32, 32))

    p = QPainter(img)
    p.setRenderHint(QPainter.RenderHint.Antialiasing, True)
    p.setRenderHint(QPainter.RenderHint.TextAntialiasing, True)

    font = QFont(font_family, point_size)
    font.setWeight(weight)
    flags = QFont.StyleStrategy.PreferQuality | QFont.StyleStrategy.PreferAntialias
    if no_subpixel:
        flags |= QFont.StyleStrategy.NoSubpixelAntialias
    font.setStyleStrategy(flags)

    num_font = QFont("Segoe UI", point_size)
    num_font.setWeight(weight)
    num_font.setStyleStrategy(flags)

    p.setFont(num_font)
    p.setPen(QColor(160, 160, 160))
    p.drawText(QRectF(10, 5, 20, 40), Qt.AlignmentFlag.AlignVCenter | Qt.AlignmentFlag.AlignLeft, "1")

    p.setFont(font)
    p.setPen(QColor(255, 255, 255, 240))
    p.drawText(QRectF(25, 5, 40, 40), Qt.AlignmentFlag.AlignVCenter | Qt.AlignmentFlag.AlignLeft, "你")

    p.end()
    return img

def main():
    configs = [
        ("Microsoft YaHei UI", 10, True),
        ("Microsoft YaHei UI", 11, True),
        ("Microsoft YaHei UI", 11, False),
        ("Microsoft YaHei UI", 12, True),
        ("Microsoft YaHei UI", 12, False),
        ("Microsoft YaHei UI", 13, True),
        ("Microsoft YaHei UI", 14, True),
    ]

    for fam, sz, no_sub in configs:
        img = render_sample(fam, sz, no_sub)
        sub_str = "nosub" if no_sub else "subpixel"
        out_name = f"sample_{fam}_{sz}_{sub_str}.png".replace(" ", "_")
        img.save(out_name)
        print(f"Generated {out_name}")

if __name__ == '__main__':
    main()
