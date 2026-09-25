import sys
from PySide6.QtGui import QGuiApplication, QFont, QPainter, QImage, QColor
from PySide6.QtCore import Qt, QRectF
import numpy as np

app = QGuiApplication(sys.argv)

def test_flag(name, font_fn, painter_fn=None):
    img = QImage(60, 40, QImage.Format.Format_ARGB32_Premultiplied)
    img.fill(QColor(32, 32, 32))
    p = QPainter(img)
    p.setRenderHint(QPainter.RenderHint.Antialiasing, True)
    p.setRenderHint(QPainter.RenderHint.TextAntialiasing, True)
    if painter_fn:
        painter_fn(p)

    font = QFont("Microsoft YaHei UI", 12)
    font_fn(font)
    p.setFont(font)
    p.setPen(QColor(255, 255, 255))
    p.drawText(QRectF(10, 5, 40, 30), Qt.AlignmentFlag.AlignVCenter | Qt.AlignmentFlag.AlignLeft, "你")
    p.end()

    arr = np.array(img.toImage()) if hasattr(img, 'toImage') else np.array(img)
    # in PySide6, QImage can be accessed or saved
    img.save("tmp.png")
    from PIL import Image
    im = Image.open("tmp.png").convert("RGB")
    arr = np.array(im)
    diff = np.abs(arr[:, :, 0].astype(int) - arr[:, :, 2].astype(int))
    max_d = np.max(diff)
    print(f"{name:50s}: max |R-B| = {max_d}")

def main():
    def f_default(f): pass
    test_flag("default", f_default)

    def f_no_subpixel(f):
        f.setStyleStrategy(QFont.StyleStrategy.NoSubpixelAntialias)
    test_flag("StyleStrategy.NoSubpixelAntialias", f_no_subpixel)

    def f_prefer_antialias(f):
        f.setStyleStrategy(QFont.StyleStrategy.PreferAntialias | QFont.StyleStrategy.NoSubpixelAntialias)
    test_flag("PreferAntialias | NoSubpixelAntialias", f_prefer_antialias)

    def f_prefer_match(f):
        f.setStyleStrategy(QFont.StyleStrategy.PreferMatch | QFont.StyleStrategy.NoSubpixelAntialias)
    test_flag("PreferMatch | NoSubpixelAntialias", f_prefer_match)

    def f_prefer_bitmap(f):
        f.setStyleStrategy(QFont.StyleStrategy.PreferBitmap)
    test_flag("PreferBitmap", f_prefer_bitmap)

    def f_prefer_outline(f):
        f.setStyleStrategy(QFont.StyleStrategy.PreferOutline)
    test_flag("PreferOutline", f_prefer_outline)

    def f_prefer_quality(f):
        f.setStyleStrategy(QFont.StyleStrategy.PreferQuality)
    test_flag("PreferQuality", f_prefer_quality)

    def f_force_outline(f):
        f.setStyleStrategy(QFont.StyleStrategy.ForceOutline)
    test_flag("ForceOutline", f_force_outline)

    def f_hinting_none(f):
        f.setHintingPreference(QFont.HintingPreference.PreferNoHinting)
    test_flag("HintingPreference.PreferNoHinting", f_hinting_none)

    def f_hinting_vertical(f):
        f.setHintingPreference(QFont.HintingPreference.PreferVerticalHinting)
    test_flag("HintingPreference.PreferVerticalHinting", f_hinting_vertical)

    def f_hinting_full(f):
        f.setHintingPreference(QFont.HintingPreference.PreferFullHinting)
    test_flag("HintingPreference.PreferFullHinting", f_hinting_full)

    # QPainter hints
    def p_no_subpixel_pos(p):
        p.setRenderHint(QPainter.RenderHint.SubpixelPositioning, False)
    test_flag("QPainter SubpixelPositioning=False", f_default, p_no_subpixel_pos)

    def p_lossless_text(p):
        p.setRenderHint(QPainter.RenderHint.LosslessImageRendering, True)
    test_flag("LosslessImageRendering", f_default, p_lossless_text)

if __name__ == '__main__':
    main()
