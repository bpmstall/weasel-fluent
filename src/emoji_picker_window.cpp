#include "emoji_picker_window.h"
#include "config.h"
#include "components/windowing/WindowBackdropMaterial.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QFontMetricsF>
#include <QtMath>
#include <QHash>
#include <QVariantAnimation>
#include <QEasingCurve>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

namespace {

QPixmap createMicaGrainTile(qreal dpr) {
    const int ratioKey = qRound((dpr > 0.0 ? dpr : 1.0) * 4.0);
    const qreal quantizedRatio = ratioKey / 4.0;
    const int size = qMax(1, qRound(96 * quantizedRatio));
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);

    quint32 state = 0x8f3d7a21U ^ static_cast<quint32>(ratioKey);
    for (int y = 0; y < size; ++y) {
        auto* scanLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < size; ++x) {
            state ^= state << 13U;
            state ^= state >> 17U;
            state ^= state << 5U;
            const int val = static_cast<int>((state >> 24U) & 0xffU);
            scanLine[x] = qRgb(val, val, val);
        }
    }
    QPixmap pm = QPixmap::fromImage(image);
    pm.setDevicePixelRatio(quantizedRatio);
    return pm;
}

QFont createEmojiFont(int pointSize) {
    QFont font;
    font.setFamilies({
        QStringLiteral("Segoe UI Emoji"),
        QStringLiteral("Apple Color Emoji"),
        QStringLiteral("Noto Color Emoji")
    });
    font.setPointSize(pointSize);
    font.setWeight(QFont::Normal);
    font.setBold(false);
    font.setStyleStrategy(QFont::StyleStrategy(QFont::PreferQuality | QFont::PreferAntialias));
    return font;
}

QFont createTextFont(int pointSize, bool bold = false) {
    QFont font;
    font.setFamilies({
        QStringLiteral("Microsoft YaHei UI"),
        QStringLiteral("Segoe UI Variable Text"),
        QStringLiteral("Segoe UI")
    });
    font.setPointSize(pointSize);
    font.setWeight(bold ? QFont::DemiBold : QFont::Normal);
    font.setBold(bold);
    return font;
}

void drawCrispText(QPainter& p, const QRectF& rect, const QFont& font, const QString& text, const QColor& color, Qt::Alignment align = Qt::AlignCenter) {
    if (text.isEmpty()) return;
    QFontMetricsF fm(font);
    qreal baselineY = rect.top() + (rect.height() + fm.ascent() - fm.descent()) * 0.5;
    qreal x = rect.left();
    if (align & Qt::AlignHCenter) {
        x = rect.left() + (rect.width() - fm.horizontalAdvance(text)) * 0.5;
    } else if (align & Qt::AlignRight) {
        x = rect.right() - fm.horizontalAdvance(text);
    }
    QPainterPath path;
    path.addText(QPointF(x, baselineY), font, text);
    p.fillPath(path, color);
}

QString emojiToHex(const QString& str) {
    const auto ucs4 = str.toUcs4();
    QStringList parts;
    for (uint cp : ucs4) {
        if (cp != 0xfe0f) {
            parts.append(QString::number(cp, 16));
        }
    }
    return parts.join(QLatin1Char('-'));
}

} // namespace

const QPixmap& EmojiPickerWindow::getEmojiPixmap(const QString& str) {
    static QHash<QString, QPixmap> cache;
    auto it = cache.find(str);
    if (it != cache.end()) {
        return it.value();
    }

    QString hex = emojiToHex(str);
    QPixmap pm;
    // 1. Try resource path
    pm.load(QStringLiteral(":/emojis/emojis/%1.png").arg(hex));
    if (pm.isNull()) {
        // Fallback with fe0f
        const auto ucs4 = str.toUcs4();
        QStringList fullParts;
        for (uint cp : ucs4) {
            fullParts.append(QString::number(cp, 16));
        }
        pm.load(QStringLiteral(":/emojis/emojis/%1.png").arg(fullParts.join(QLatin1Char('-'))));
    }
    // 2. Fallback to direct file path
    if (pm.isNull()) {
        pm.load(QStringLiteral("assets/emojis/%1.png").arg(hex));
    }

    auto inserted = cache.insert(str, pm);
    return inserted.value();
}

const QVector<QString> EmojiPickerWindow::s_emojis = {
    // 128 Curated high-frequency Emojis (32 per page x 4 pages)
    // Page 1: Smileys & Faces
    QStringLiteral("😀"), QStringLiteral("😃"), QStringLiteral("😄"), QStringLiteral("😁"),
    QStringLiteral("😆"), QStringLiteral("😅"), QStringLiteral("🤣"), QStringLiteral("😂"),
    QStringLiteral("🙂"), QStringLiteral("🙃"), QStringLiteral("😉"), QStringLiteral("😊"),
    QStringLiteral("😇"), QStringLiteral("🥰"), QStringLiteral("😍"), QStringLiteral("🤩"),
    QStringLiteral("😘"), QStringLiteral("😗"), QStringLiteral("😚"), QStringLiteral("😋"),
    QStringLiteral("😛"), QStringLiteral("😜"), QStringLiteral("🤪"), QStringLiteral("😝"),
    QStringLiteral("🤗"), QStringLiteral("🤭"), QStringLiteral("🤫"), QStringLiteral("🤔"),
    QStringLiteral("🤐"), QStringLiteral("🤨"), QStringLiteral("😐"), QStringLiteral("😑"),

    // Page 2: Moods & Reactions
    QStringLiteral("😶"), QStringLiteral("😏"), QStringLiteral("😒"), QStringLiteral("🙄"),
    QStringLiteral("😬"), QStringLiteral("🤥"), QStringLiteral("😌"), QStringLiteral("😔"),
    QStringLiteral("😪"), QStringLiteral("🤤"), QStringLiteral("😴"), QStringLiteral("😷"),
    QStringLiteral("🤒"), QStringLiteral("🤕"), QStringLiteral("🤢"), QStringLiteral("🤮"),
    QStringLiteral("🤧"), QStringLiteral("🥵"), QStringLiteral("🥶"), QStringLiteral("🥴"),
    QStringLiteral("😵"), QStringLiteral("🤯"), QStringLiteral("🤠"), QStringLiteral("🥳"),
    QStringLiteral("😎"), QStringLiteral("🤓"), QStringLiteral("🧐"), QStringLiteral("😕"),
    QStringLiteral("😟"), QStringLiteral("🙁"), QStringLiteral("😮"), QStringLiteral("😯"),

    // Page 3: Gestures & Hands
    QStringLiteral("👋"), QStringLiteral("🤚"), QStringLiteral("🖐️"), QStringLiteral("✋"),
    QStringLiteral("🖖"), QStringLiteral("👌"), QStringLiteral("🤌"), QStringLiteral("🤏"),
    QStringLiteral("✌️"), QStringLiteral("🤞"), QStringLiteral("🤟"), QStringLiteral("🤘"),
    QStringLiteral("🤙"), QStringLiteral("👈"), QStringLiteral("👉"), QStringLiteral("👆"),
    QStringLiteral("🖕"), QStringLiteral("👇"), QStringLiteral("☝️"), QStringLiteral("👍"),
    QStringLiteral("👎"), QStringLiteral("✊"), QStringLiteral("👊"), QStringLiteral("🤛"),
    QStringLiteral("🤜"), QStringLiteral("👏"), QStringLiteral("🙌"), QStringLiteral("👐"),
    QStringLiteral("🤲"), QStringLiteral("🤝"), QStringLiteral("🙏"), QStringLiteral("✍️"),

    // Page 4: Hearts, Magic & Celebrations
    QStringLiteral("❤️"), QStringLiteral("🧡"), QStringLiteral("💛"), QStringLiteral("💚"),
    QStringLiteral("💙"), QStringLiteral("💜"), QStringLiteral("🖤"), QStringLiteral("🤍"),
    QStringLiteral("🤎"), QStringLiteral("💔"), QStringLiteral("❣️"), QStringLiteral("💕"),
    QStringLiteral("💞"), QStringLiteral("💓"), QStringLiteral("💗"), QStringLiteral("💖"),
    QStringLiteral("💘"), QStringLiteral("💝"), QStringLiteral("💟"), QStringLiteral("💯"),
    QStringLiteral("✨"), QStringLiteral("🌟"), QStringLiteral("⭐"), QStringLiteral("💥"),
    QStringLiteral("🔥"), QStringLiteral("🎉"), QStringLiteral("🎊"), QStringLiteral("🎁"),
    QStringLiteral("🎈"), QStringLiteral("🏆"), QStringLiteral("🥇"), QStringLiteral("🎯")
};

const QVector<QString> EmojiPickerWindow::s_kaomojis = {
    // 45 Curated Japanese Kaomojis (15 per page x 3 pages)
    // Page 1
    QStringLiteral("(*/ω＼*)"), QStringLiteral("(^_−)☆"), QStringLiteral("(ง •_•)ง"),
    QStringLiteral("(๑•̀ㅂ•́)و✧"), QStringLiteral("(・ω・)"), QStringLiteral("(>_<)"),
    QStringLiteral("(╯°□°)╯︵ ┻━┻"), QStringLiteral("(ง •̀_•́)ง"), QStringLiteral("(=^･ω･^=)"),
    QStringLiteral("(๑>؂<๑)"), QStringLiteral("(⊙_⊙)?"), QStringLiteral("(*/∇＼*)"),
    QStringLiteral("ヾ(≧▽≦*)o"), QStringLiteral("(*^▽^*)"), QStringLiteral("(T_T)"),

    // Page 2
    QStringLiteral("(✿◡‿◡)"), QStringLiteral("(•̀ᴗ•́)و ̑̑"), QStringLiteral("(￣▽￣)\""),
    QStringLiteral("╮(╯▽╰)╭"), QStringLiteral("(¬_¬ )"), QStringLiteral("(๑¯◡¯๑)"),
    QStringLiteral("(o゜▽゜)o☆"), QStringLiteral("(っ*´Д`)っ"), QStringLiteral("ε=ε=(~￣▽￣)~"),
    QStringLiteral("( ﾟдﾟ)つ Bye"), QStringLiteral("(๑•́ ₃ •̀๑)"), QStringLiteral("(；′⌒`)"),
    QStringLiteral("(/▽＼)"), QStringLiteral("o(*////▽////*)q"), QStringLiteral("(´･ω･`)"),

    // Page 3
    QStringLiteral("눈_눈"), QStringLiteral("ಠ_ಠ"), QStringLiteral("¯\\_(ツ)_/¯"),
    QStringLiteral("(づ｡◕‿‿◕｡)づ"), QStringLiteral("(*•̀ㅂ•́)و"), QStringLiteral("(,,•́ . •̀,,)"),
    QStringLiteral("(oﾟvﾟ)ノ"), QStringLiteral("(￣y▽,￣)╭"), QStringLiteral("(/ω＼)"),
    QStringLiteral("੭ ᐕ)੭*⁾⁾"), QStringLiteral("(ง ˙ω˙)ว"), QStringLiteral("Σ( ° △ °|||)"),
    QStringLiteral("(｡•́︿•̀｡)"), QStringLiteral("(・-・*)"), QStringLiteral("(・∀・)")
};

const QVector<QString> EmojiPickerWindow::s_symbols = {
    // 96 Curated Symbols (32 per page x 3 pages)
    // Page 1: Punctuation & Quotes
    QStringLiteral("“"), QStringLiteral("”"), QStringLiteral("‘"), QStringLiteral("’"),
    QStringLiteral("《"), QStringLiteral("》"), QStringLiteral("〈"), QStringLiteral("〉"),
    QStringLiteral("「"), QStringLiteral("」"), QStringLiteral("『"), QStringLiteral("』"),
    QStringLiteral("【"), QStringLiteral("】"), QStringLiteral("〔"), QStringLiteral("〕"),
    QStringLiteral("—"), QStringLiteral("…"), QStringLiteral("·"), QStringLiteral("～"),
    QStringLiteral("¡"), QStringLiteral("¿"), QStringLiteral("•"), QStringLiteral("※"),
    QStringLiteral("§"), QStringLiteral("¶"), QStringLiteral("†"), QStringLiteral("‡"),
    QStringLiteral("★"), QStringLiteral("☆"), QStringLiteral("▲"), QStringLiteral("△"),

    // Page 2: Math & Units
    QStringLiteral("+"), QStringLiteral("-"), QStringLiteral("×"), QStringLiteral("÷"),
    QStringLiteral("="), QStringLiteral("≠"), QStringLiteral("≈"), QStringLiteral("≡"),
    QStringLiteral("≤"), QStringLiteral("≥"), QStringLiteral("±"), QStringLiteral("∓"),
    QStringLiteral("∞"), QStringLiteral("∑"), QStringLiteral("∏"), QStringLiteral("√"),
    QStringLiteral("∫"), QStringLiteral("‰"), QStringLiteral("℅"), QStringLiteral("°"),
    QStringLiteral("℃"), QStringLiteral("℉"), QStringLiteral("㎎"), QStringLiteral("㎏"),
    QStringLiteral("㎜"), QStringLiteral("㎝"), QStringLiteral("㎞"), QStringLiteral("㎡"),
    QStringLiteral("㏄"), QStringLiteral("㏕"), QStringLiteral("㏖"), QStringLiteral("㏔"),

    // Page 3: Arrows, Currencies & Geometry
    QStringLiteral("←"), QStringLiteral("↑"), QStringLiteral("→"), QStringLiteral("↓"),
    QStringLiteral("↖"), QStringLiteral("↗"), QStringLiteral("↘"), QStringLiteral("↙"),
    QStringLiteral("⇐"), QStringLiteral("⇑"), QStringLiteral("⇒"), QStringLiteral("⇓"),
    QStringLiteral("↔"), QStringLiteral("↕"), QStringLiteral("➔"), QStringLiteral("➜"),
    QStringLiteral("¥"), QStringLiteral("$"), QStringLiteral("€"), QStringLiteral("£"),
    QStringLiteral("₩"), QStringLiteral("₽"), QStringLiteral("₹"), QStringLiteral("฿"),
    QStringLiteral("■"), QStringLiteral("□"), QStringLiteral("◆"), QStringLiteral("◇"),
    QStringLiteral("●"), QStringLiteral("○"), QStringLiteral("▲"), QStringLiteral("▼")
};

EmojiPickerWindow::EmojiPickerWindow(QWidget* parent)
    : QWidget(parent) {
    setWindowFlags(Qt::Tool |
                   Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);
    setFixedSize(360, 240);
    updateHitTargets();
}

void EmojiPickerWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd) {
        MARGINS margins = {-1, -1, -1, -1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);

        int corner = 2; // DWMWCP_ROUND
        DwmSetWindowAttribute(hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &corner, sizeof(corner));

        BOOL dark = AppConfig::instance().isDarkTheme() ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));

        int backdropType = 1; // DWMSBT_NONE
        DwmSetWindowAttribute(hwnd, 38 /*DWMWA_SYSTEMBACKDROP_TYPE*/, &backdropType, sizeof(backdropType));
    }
#endif
}

int EmojiPickerWindow::itemsPerPage() const {
    switch (m_category) {
        case Category::Emoji: return 32;   // 8 cols x 4 rows
        case Category::Kaomoji: return 15; // 3 cols x 5 rows
        case Category::Symbols: return 32; // 8 cols x 4 rows
    }
    return 32;
}

const QVector<QString>& EmojiPickerWindow::currentDataset() const {
    switch (m_category) {
        case Category::Emoji: return s_emojis;
        case Category::Kaomoji: return s_kaomojis;
        case Category::Symbols: return s_symbols;
    }
    return s_emojis;
}

int EmojiPickerWindow::totalPages() const {
    const auto& ds = currentDataset();
    int per = itemsPerPage();
    return qMax(1, static_cast<int>(std::ceil(static_cast<double>(ds.size()) / per)));
}

void EmojiPickerWindow::updateTabAnimation(bool animate) {
    int idx = static_cast<int>(m_category);
    if (idx < 0 || idx >= m_tabRects.size()) return;
    QRectF tr = m_tabRects[idx];
    QRectF target(tr.left() + 16.0, tr.bottom() - 2.5, tr.width() - 32.0, 2.5);

    if (!animate || m_animatedTabUnderline.isNull() || !isVisible()) {
        m_animatedTabUnderline = target;
        m_tabUnderlineTarget = target;
        if (m_tabAnim) m_tabAnim->stop();
        return;
    }

    if (target == m_tabUnderlineTarget) return;

    m_tabUnderlineStart = m_animatedTabUnderline;
    m_tabUnderlineTarget = target;

    if (!m_tabAnim) {
        m_tabAnim = new QVariantAnimation(this);
        m_tabAnim->setDuration(180);
        m_tabAnim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_tabAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
            qreal t = val.toReal();
            qreal x = m_tabUnderlineStart.x() + (m_tabUnderlineTarget.x() - m_tabUnderlineStart.x()) * t;
            qreal y = m_tabUnderlineStart.y() + (m_tabUnderlineTarget.y() - m_tabUnderlineStart.y()) * t;
            qreal w = m_tabUnderlineStart.width() + (m_tabUnderlineTarget.width() - m_tabUnderlineStart.width()) * t;
            qreal h = m_tabUnderlineStart.height() + (m_tabUnderlineTarget.height() - m_tabUnderlineStart.height()) * t;
            m_animatedTabUnderline = QRectF(x, y, w, h);
            update();
        });
    }

    m_tabAnim->stop();
    m_tabAnim->setStartValue(0.0);
    m_tabAnim->setEndValue(1.0);
    m_tabAnim->start();
}

void EmojiPickerWindow::setCategory(Category cat) {
    if (m_category != cat) {
        m_category = cat;
        m_pageNo = 0;
        m_hoverItem = -1;
        updateHitTargets();
        updateTabAnimation(true);
        update();
    }
}

void EmojiPickerWindow::updateHitTargets() {
    qreal w = width();
    qreal h = height();

    // 1. Top Header & Close button
    m_btnClose = QRectF(w - 32.0, 7.0, 24.0, 24.0);

    // 2. Tabs
    m_tabRects.clear();
    qreal tabX = 12.0;
    qreal tabY = 6.0;
    qreal tabH = 28.0;
    qreal tabW = 86.0;

    for (int i = 0; i < 3; ++i) {
        m_tabRects.append(QRectF(tabX, tabY, tabW, tabH));
        tabX += tabW + 6.0;
    }

    // 3. Content grid
    m_itemRects.clear();
    qreal gridLeft = 12.0;
    qreal gridW = w - 24.0;

    int per = itemsPerPage();
    if (m_category == Category::Kaomoji) {
        // 3 cols x 5 rows
        int cols = 3;
        int rows = 5;
        qreal cellW = (gridW - (cols - 1) * 6.0) / cols;
        qreal cellH = 27.5;
        qreal gridTop = 46.5;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                int idx = r * cols + c;
                if (idx < per) {
                    QRectF rect(gridLeft + c * (cellW + 6.0), gridTop + r * (cellH + 3.5), cellW, cellH);
                    m_itemRects.append(rect);
                }
            }
        }
    } else {
        // 8 cols x 4 rows
        int cols = 8;
        int rows = 4;
        qreal cellW = (gridW - (cols - 1) * 4.0) / cols;
        qreal cellH = 36.0;
        qreal gridTop = 46.0;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                int idx = r * cols + c;
                if (idx < per) {
                    QRectF rect(gridLeft + c * (cellW + 4.0), gridTop + r * (cellH + 4.0), cellW, cellH);
                    m_itemRects.append(rect);
                }
            }
        }
    }

    // 4. Footer navigation
    qreal footerDividerY = h - 32.0; // 208.0
    m_pageTextRect = QRectF(14.0, footerDividerY, 200.0, 32.0);
    m_btnNext = QRectF(w - 36.0, footerDividerY + 4.0, 24.0, 24.0);
    m_btnPrev = QRectF(w - 68.0, footerDividerY + 4.0, 24.0, 24.0);
}

void EmojiPickerWindow::paintEvent(QPaintEvent* /*event*/) {
    qreal dpr = devicePixelRatioF();
    QSize pixelSize = size() * dpr;
    if (pixelSize.isEmpty()) return;

    auto& cfg = AppConfig::instance();
    bool isDark = cfg.isDarkTheme();
    constexpr qreal kCornerRadius = 8.0;

    QRectF bgRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath bgPath;
    bgPath.addRoundedRect(bgRect, kCornerRadius, kCornerRadius);

    // 1. Offscreen image
    QImage contentImage(pixelSize, QImage::Format_RGB32);
    contentImage.setDevicePixelRatio(dpr);

    {
        QPainter cp(&contentImage);
        cp.setRenderHint(QPainter::Antialiasing, true);
        cp.setRenderHint(QPainter::TextAntialiasing, true);

        // Fill Mica background
        QColor baseColor = isDark ? QColor(32, 32, 32) : QColor(243, 243, 243);
        cp.fillRect(rect(), baseColor);
        static QPixmap grain = createMicaGrainTile(dpr);
        cp.setRenderHint(QPainter::SmoothPixmapTransform, false);
        cp.setOpacity(isDark ? 0.022 : 0.015);
        cp.drawTiledPixmap(rect(), grain, QPointF(0, 0));
        cp.setOpacity(1.0);

        QColor textPrimary = isDark ? QColor(255, 255, 255, 245) : QColor(20, 20, 20, 245);
        QColor textSecondary = isDark ? QColor(255, 255, 255, 140) : QColor(0, 0, 0, 135);
        QColor accentColor = cfg.effectiveAccentColor();

        // 1. Draw Tabs
        static const QString tabTitles[3] = {
            QStringLiteral("😀 表情"),
            QStringLiteral("(^_−) 颜文字"),
            QStringLiteral("Ω 符号")
        };

        QFont tabFont = createTextFont(10, false);
        for (int i = 0; i < 3; ++i) {
            bool isActive = (static_cast<int>(m_category) == i);
            bool isHover = (m_hoverTab == i);
            QRectF tr = m_tabRects[i];

            if (isHover && !isActive) {
                cp.setPen(Qt::NoPen);
                cp.setBrush(isDark ? QColor(255, 255, 255, 12) : QColor(0, 0, 0, 8));
                cp.drawRoundedRect(tr, 4.0, 4.0);
            }

            QColor tabColor = isActive ? textPrimary : (isHover ? textPrimary : textSecondary);
            drawCrispText(cp, tr, tabFont, tabTitles[i], tabColor, Qt::AlignCenter);
        }

        // Sliding active underline indicator
        if (m_animatedTabUnderline.isNull() && static_cast<int>(m_category) < m_tabRects.size()) {
            QRectF tr = m_tabRects[static_cast<int>(m_category)];
            m_animatedTabUnderline = QRectF(tr.left() + 16.0, tr.bottom() - 2.5, tr.width() - 32.0, 2.5);
            m_tabUnderlineTarget = m_animatedTabUnderline;
        }
        if (!m_animatedTabUnderline.isNull()) {
            cp.setPen(Qt::NoPen);
            cp.setBrush(accentColor);
            cp.drawRoundedRect(m_animatedTabUnderline, 1.25, 1.25);
        }

        // Close button (✕)
        if (m_hoverClose) {
            cp.setPen(Qt::NoPen);
            cp.setBrush(isDark ? QColor(255, 255, 255, 16) : QColor(0, 0, 0, 12));
            cp.drawRoundedRect(m_btnClose, 4.0, 4.0);
        }
        QFont closeFont = createTextFont(10, false);
        drawCrispText(cp, m_btnClose, closeFont, QStringLiteral("✕"), m_hoverClose ? textPrimary : textSecondary, Qt::AlignCenter);

        // Divider under header
        QColor sepColor = isDark ? QColor(255, 255, 255, 16) : QColor(0, 0, 0, 14);
        cp.setPen(QPen(sepColor, 1.0));
        cp.drawLine(QPointF(12.0, 40.0), QPointF(width() - 12.0, 40.0));

        // 2. Draw Content Grid
        const auto& ds = currentDataset();
        int per = itemsPerPage();
        int startIdx = m_pageNo * per;

        QFont emojiFont = createEmojiFont(18);
        QFont kaomojiFont = createTextFont(10, false);
        QFont symbolFont = createTextFont(14, false);

        for (int i = 0; i < m_itemRects.size(); ++i) {
            int dataIdx = startIdx + i;
            if (dataIdx >= ds.size()) break;

            const QString& item = ds[dataIdx];
            const QRectF& r = m_itemRects[i];
            bool isHover = (m_hoverItem == i);

            // Item hover / background
            if (isHover) {
                cp.setPen(Qt::NoPen);
                cp.setBrush(isDark ? QColor(255, 255, 255, 16) : QColor(0, 0, 0, 10));
                cp.drawRoundedRect(r, 4.0, 4.0);
            } else if (m_category == Category::Kaomoji) {
                // Kaomoji has subtle pill background
                cp.setPen(Qt::NoPen);
                cp.setBrush(isDark ? QColor(255, 255, 255, 6) : QColor(0, 0, 0, 4));
                cp.drawRoundedRect(r, 4.0, 4.0);
            }

            // Draw glyph/text
            if (m_category == Category::Emoji) {
                const QPixmap& pix = getEmojiPixmap(item);
                if (!pix.isNull()) {
                    constexpr qreal s = 24.0;
                    QRectF destRect(r.center().x() - s * 0.5, r.center().y() - s * 0.5, s, s);
                    qreal prevOp = cp.opacity();
                    if (isDark) {
                        cp.setOpacity(isHover ? 1.0 : 0.86);
                    }
                    cp.drawPixmap(destRect, pix, pix.rect());
                    if (isDark) {
                        cp.setOpacity(prevOp);
                    }
                } else {
                    cp.setFont(emojiFont);
                    cp.setPen(textPrimary);
                    cp.drawText(r, Qt::AlignCenter, item);
                }
            } else if (m_category == Category::Kaomoji) {
                drawCrispText(cp, r, kaomojiFont, item, textPrimary, Qt::AlignCenter);
            } else {
                drawCrispText(cp, r, symbolFont, item, textPrimary, Qt::AlignCenter);
            }
        }

        // 3. Footer Divider & Controls
        cp.setPen(QPen(sepColor, 1.0));
        cp.drawLine(QPointF(12.0, height() - 32.0), QPointF(width() - 12.0, height() - 32.0));

        // Page info text
        QString pageStr = QStringLiteral("第 %1 / %2 页 (共 %3 项)")
                              .arg(m_pageNo + 1)
                              .arg(totalPages())
                              .arg(ds.size());
        QFont footFont = createTextFont(9, false);
        drawCrispText(cp, m_pageTextRect, footFont, pageStr, textSecondary, Qt::AlignVCenter | Qt::AlignLeft);

        // Prev / Next button hover
        auto drawBtn = [&](const QRectF& btnRect, bool isHover, bool canClick, bool pointsLeft) {
            if (isHover && canClick) {
                cp.setPen(Qt::NoPen);
                cp.setBrush(isDark ? QColor(255, 255, 255, 16) : QColor(0, 0, 0, 12));
                cp.drawRoundedRect(btnRect, 4.0, 4.0);
            }

            QColor arrowColor = canClick ? (isHover ? accentColor : textPrimary)
                                         : (isDark ? QColor(255, 255, 255, 60) : QColor(0, 0, 0, 50));
            cp.save();
            cp.setPen(Qt::NoPen);
            cp.setBrush(arrowColor);
            QPainterPath tri;
            QPointF c = btnRect.center();
            qreal s = 6.0;
            if (pointsLeft) {
                tri.moveTo(c.x() + s * 0.4, c.y() - s * 0.5);
                tri.lineTo(c.x() - s * 0.4, c.y());
                tri.lineTo(c.x() + s * 0.4, c.y() + s * 0.5);
            } else {
                tri.moveTo(c.x() - s * 0.4, c.y() - s * 0.5);
                tri.lineTo(c.x() + s * 0.4, c.y());
                tri.lineTo(c.x() - s * 0.4, c.y() + s * 0.5);
            }
            tri.closeSubpath();
            cp.drawPath(tri);
            cp.restore();
        };

        drawBtn(m_btnPrev, m_hoverPrev, m_pageNo > 0, true);
        drawBtn(m_btnNext, m_hoverNext, m_pageNo < totalPages() - 1, false);
    }

    // 2. Render onto transparent window with smooth rounded border
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.fillRect(rect(), Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    p.save();
    p.setClipPath(bgPath);
    p.drawImage(0, 0, contentImage);
    p.restore();

    // 3. Subtle 1px border
    QColor borderColor = isDark ? QColor(255, 255, 255, 24) : QColor(0, 0, 0, 20);
    p.setPen(QPen(borderColor, 1.0));
    p.drawPath(bgPath);
}

QImage EmojiPickerWindow::renderPreviewImage(qreal dpr) {
    updateHitTargets();
    QSize pixelSize = size() * dpr;
    QImage result(pixelSize, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    result.setDevicePixelRatio(dpr);

    auto& cfg = AppConfig::instance();
    bool isDark = cfg.isDarkTheme();
    constexpr qreal kCornerRadius = 8.0;

    QRectF bgRect = QRectF(0.5, 0.5, width() - 1.0, height() - 1.0);
    QPainterPath bgPath;
    bgPath.addRoundedRect(bgRect, kCornerRadius, kCornerRadius);

    {
        QPainter p(&result);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.save();
        p.setClipPath(bgPath);

        // Draw offscreen content
        QImage content(pixelSize, QImage::Format_RGB32);
        content.setDevicePixelRatio(dpr);

        QPainter cp(&content);
        cp.setRenderHint(QPainter::Antialiasing, true);
        cp.setRenderHint(QPainter::TextAntialiasing, true);

        QColor baseColor = isDark ? QColor(32, 32, 32) : QColor(243, 243, 243);
        cp.fillRect(rect(), baseColor);
        static QPixmap grain = createMicaGrainTile(dpr);
        cp.setOpacity(isDark ? 0.022 : 0.015);
        cp.drawTiledPixmap(rect(), grain, QPointF(0, 0));
        cp.setOpacity(1.0);

        // Call paint logic
        // Paint content
        QColor textPrimary = isDark ? QColor(255, 255, 255, 245) : QColor(20, 20, 20, 245);
        QColor textSecondary = isDark ? QColor(255, 255, 255, 140) : QColor(0, 0, 0, 135);
        QColor accentColor = cfg.effectiveAccentColor();

        static const QString tabTitles[3] = {
            QStringLiteral("😀 表情"),
            QStringLiteral("(^_−) 颜文字"),
            QStringLiteral("Ω 符号")
        };

        QFont tabFont = createTextFont(10, false);
        for (int i = 0; i < 3; ++i) {
            bool isActive = (static_cast<int>(m_category) == i);
            QRectF tr = m_tabRects[i];
            drawCrispText(cp, tr, tabFont, tabTitles[i], isActive ? textPrimary : textSecondary, Qt::AlignCenter);
            if (isActive) {
                QRectF lineR(tr.left() + 16.0, tr.bottom() - 2.5, tr.width() - 32.0, 2.5);
                cp.setPen(Qt::NoPen);
                cp.setBrush(accentColor);
                cp.drawRoundedRect(lineR, 1.25, 1.25);
            }
        }

        drawCrispText(cp, m_btnClose, createTextFont(10, false), QStringLiteral("✕"), textSecondary, Qt::AlignCenter);

        QColor sepColor = isDark ? QColor(255, 255, 255, 16) : QColor(0, 0, 0, 14);
        cp.setPen(QPen(sepColor, 1.0));
        cp.drawLine(QPointF(12.0, 40.0), QPointF(width() - 12.0, 40.0));

        const auto& ds = currentDataset();
        int per = itemsPerPage();
        int startIdx = m_pageNo * per;

        QFont emojiFont = createEmojiFont(18);
        QFont kaomojiFont = createTextFont(10, false);
        QFont symbolFont = createTextFont(14, false);

        for (int i = 0; i < m_itemRects.size(); ++i) {
            int dataIdx = startIdx + i;
            if (dataIdx >= ds.size()) break;
            const QString& item = ds[dataIdx];
            const QRectF& r = m_itemRects[i];

            if (m_category == Category::Kaomoji) {
                cp.setPen(Qt::NoPen);
                cp.setBrush(isDark ? QColor(255, 255, 255, 6) : QColor(0, 0, 0, 4));
                cp.drawRoundedRect(r, 4.0, 4.0);
                drawCrispText(cp, r, kaomojiFont, item, textPrimary, Qt::AlignCenter);
            } else if (m_category == Category::Emoji) {
                const QPixmap& pix = getEmojiPixmap(item);
                if (!pix.isNull()) {
                    constexpr qreal s = 24.0;
                    QRectF destRect(r.center().x() - s * 0.5, r.center().y() - s * 0.5, s, s);
                    qreal prevOp = cp.opacity();
                    if (isDark) {
                        cp.setOpacity(0.86);
                    }
                    cp.drawPixmap(destRect, pix, pix.rect());
                    if (isDark) {
                        cp.setOpacity(prevOp);
                    }
                } else {
                    cp.setFont(emojiFont);
                    cp.setPen(textPrimary);
                    cp.drawText(r, Qt::AlignCenter, item);
                }
            } else {
                drawCrispText(cp, r, symbolFont, item, textPrimary, Qt::AlignCenter);
            }
        }

        cp.setPen(QPen(sepColor, 1.0));
        cp.drawLine(QPointF(12.0, height() - 32.0), QPointF(width() - 12.0, height() - 32.0));

        QString pageStr = QStringLiteral("第 %1 / %2 页 (共 %3 项)")
                              .arg(m_pageNo + 1)
                              .arg(totalPages())
                              .arg(ds.size());
        drawCrispText(cp, m_pageTextRect, createTextFont(9, false), pageStr, textSecondary, Qt::AlignVCenter | Qt::AlignLeft);

        auto drawBtn = [&](const QRectF& btnRect, bool canClick, bool pointsLeft) {
            QColor arrowColor = canClick ? textPrimary : (isDark ? QColor(255, 255, 255, 60) : QColor(0, 0, 0, 50));
            cp.save();
            cp.setPen(Qt::NoPen);
            cp.setBrush(arrowColor);
            QPainterPath tri;
            QPointF c = btnRect.center();
            qreal s = 6.0;
            if (pointsLeft) {
                tri.moveTo(c.x() + s * 0.4, c.y() - s * 0.5);
                tri.lineTo(c.x() - s * 0.4, c.y());
                tri.lineTo(c.x() + s * 0.4, c.y() + s * 0.5);
            } else {
                tri.moveTo(c.x() - s * 0.4, c.y() - s * 0.5);
                tri.lineTo(c.x() + s * 0.4, c.y());
                tri.lineTo(c.x() - s * 0.4, c.y() + s * 0.5);
            }
            tri.closeSubpath();
            cp.drawPath(tri);
            cp.restore();
        };

        drawBtn(m_btnPrev, m_pageNo > 0, true);
        drawBtn(m_btnNext, m_pageNo < totalPages() - 1, false);

        p.drawImage(0, 0, content);
        p.restore();

        p.setPen(QPen(isDark ? QColor(255, 255, 255, 24) : QColor(0, 0, 0, 20), 1.0));
        p.drawPath(bgPath);
    }

    return result;
}

void EmojiPickerWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QPointF pos = event->position();

        // 1. Close button
        if (m_btnClose.contains(pos)) {
            hide();
            emit closed();
            return;
        }

        // 2. Tabs
        for (int i = 0; i < m_tabRects.size(); ++i) {
            if (m_tabRects[i].contains(pos)) {
                setCategory(static_cast<Category>(i));
                return;
            }
        }

        // 3. Page prev / next
        if (m_btnPrev.contains(pos)) {
            if (m_pageNo > 0) {
                m_pageNo--;
                update();
            }
            return;
        }
        if (m_btnNext.contains(pos)) {
            if (m_pageNo < totalPages() - 1) {
                m_pageNo++;
                update();
            }
            return;
        }

        // 4. Item grid
        const auto& ds = currentDataset();
        int per = itemsPerPage();
        int startIdx = m_pageNo * per;

        for (int i = 0; i < m_itemRects.size(); ++i) {
            if (m_itemRects[i].contains(pos)) {
                int dataIdx = startIdx + i;
                if (dataIdx < ds.size()) {
                    emit textSelected(ds[dataIdx]);
                    hide();
                    return;
                }
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void EmojiPickerWindow::mouseMoveEvent(QMouseEvent* event) {
    QPointF pos = event->position();
    int oldItem = m_hoverItem;
    int oldTab = m_hoverTab;
    bool oldClose = m_hoverClose;
    bool oldPrev = m_hoverPrev;
    bool oldNext = m_hoverNext;

    m_hoverClose = m_btnClose.contains(pos);
    m_hoverPrev = m_btnPrev.contains(pos);
    m_hoverNext = m_btnNext.contains(pos);

    m_hoverTab = -1;
    for (int i = 0; i < m_tabRects.size(); ++i) {
        if (m_tabRects[i].contains(pos)) {
            m_hoverTab = i;
            break;
        }
    }

    m_hoverItem = -1;
    const auto& ds = currentDataset();
    int per = itemsPerPage();
    int startIdx = m_pageNo * per;

    for (int i = 0; i < m_itemRects.size(); ++i) {
        if (m_itemRects[i].contains(pos)) {
            int dataIdx = startIdx + i;
            if (dataIdx < ds.size()) {
                m_hoverItem = i;
            }
            break;
        }
    }

    if (oldItem != m_hoverItem || oldTab != m_hoverTab || oldClose != m_hoverClose ||
        oldPrev != m_hoverPrev || oldNext != m_hoverNext) {
        update();
    }
}

void EmojiPickerWindow::leaveEvent(QEvent* event) {
    m_hoverItem = -1;
    m_hoverTab = -1;
    m_hoverClose = false;
    m_hoverPrev = false;
    m_hoverNext = false;
    update();
    QWidget::leaveEvent(event);
}

void EmojiPickerWindow::wheelEvent(QWheelEvent* event) {
    int delta = event->angleDelta().y();
    if (delta > 0) {
        // Scroll up -> prev page
        if (m_pageNo > 0) {
            m_pageNo--;
            update();
        }
    } else if (delta < 0) {
        // Scroll down -> next page
        if (m_pageNo < totalPages() - 1) {
            m_pageNo++;
            update();
        }
    }
    event->accept();
}

void EmojiPickerWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        hide();
        emit closed();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_PageUp) {
        if (m_pageNo > 0) {
            m_pageNo--;
            update();
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Right || event->key() == Qt::Key_PageDown) {
        if (m_pageNo < totalPages() - 1) {
            m_pageNo++;
            update();
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Tab) {
        int nextCat = (static_cast<int>(m_category) + 1) % 3;
        setCategory(static_cast<Category>(nextCat));
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}
