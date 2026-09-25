import sys
from PySide6.QtGui import QGuiApplication, QFont, QPainter, QImage, QColor, QPainterPath, QFontMetricsF
from PySide6.QtCore import Qt, QPointF, QRectF
from PIL import Image

app = QGuiApplication(sys.argv)

def test_full_candidate():
    # Let's render the first candidate pill:
    # Pill rect: (4, 4, 76, 32)
    # Background: #202020
    # Pill: #323232 with radius 4
    # Left accent bar: #387b95, 2.5x14, radius 1.2
    # Number: Segoe UI, color #a0a0a0
    # Text: Microsoft YaHei UI, color #ffffff

    # Scale 1.5x (to match display DPR 1.5x)
    dpr = 1.5
    w = int(500 * dpr)
    h = int(40 * dpr)

    img = QImage(w, h, QImage.Format.Format_ARGB32_Premultiplied)
    img.fill(QColor(0, 0, 0, 0)) # transparent

    p = QPainter(img)
    p.scale(dpr, dpr)
    p.setRenderHint(QPainter.RenderHint.Antialiasing, True)

    # 1. Background
    bg_rect = QRectF(0, 0, 500, 40)
    p.setPen(Qt.PenStyle.NoPen)
    p.setBrush(QColor(32, 32, 32))
    p.drawRoundedRect(bg_rect, 8.0, 8.0)

    # 2. Pill 1
    pill_rect = QRectF(4.0, 4.0, 76.0, 32.0)
    p.setBrush(QColor(255, 255, 255, 26))
    p.drawRoundedRect(pill_rect, 4.0, 4.0)

    # 3. Accent bar
    bar_rect = QRectF(7.0, 4.0 + (32.0 - 13.0) * 0.5, 2.5, 13.0)
    p.setBrush(QColor(56, 123, 149)) # #387b95
    p.drawRoundedRect(bar_rect, 1.25, 1.25)

    # Fonts
    point_size = 11.0
    num_font = QFont("Segoe UI", point_size)
    num_font.setWeight(QFont.Weight.Normal)

    cn_font = QFont("Microsoft YaHei UI", point_size)
    cn_font.setWeight(QFont.Weight.Normal)

    fm_num = QFontMetricsF(num_font)
    fm_cn = QFontMetricsF(cn_font)

    # Candidates:
    cands = [("1", "你"), ("2", "呢"), ("3", "泥"), ("4", "尼"), ("5", "拟"), ("6", "妮"), ("7", "倪")]

    cur_x = 4.0
    for i, (num, txt) in enumerate(cands):
        is_highlight = (i == 0)
        # compute width
        nw = fm_num.horizontalAdvance(num)
        cw = fm_cn.horizontalAdvance(txt)
        item_w = (nw + cw + 14.0)
        if is_highlight:
            item_w += 8.0

        if not is_highlight:
            pill_r = QRectF(cur_x, 4.0, item_w, 32.0)
            # no hover

        # Draw number
        text_x = cur_x + (14.0 if is_highlight else 6.0)
        baseline_y = 4.0 + (32.0 + fm_cn.ascent() - fm_cn.descent()) * 0.5

        # Path for number
        path_num = QPainterPath()
        path_num.addText(QPointF(text_x, baseline_y), num_font, num)
        p.fillPath(path_num, QColor(160, 160, 160, 220))

        text_x += nw + 5.0

        # Path for text
        path_cn = QPainterPath()
        path_cn.addText(QPointF(text_x, baseline_y), cn_font, txt)
        p.fillPath(path_cn, QColor(255, 255, 255, 240))

        cur_x += item_w + 2.0

    p.end()

    img.save("sim_cand_path.png")
    print("Saved sim_cand_path.png")

if __name__ == '__main__':
    test_full_candidate()
