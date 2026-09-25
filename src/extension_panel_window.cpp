#include "extension_panel_window.h"
#include "config.h"
#include "design/Typography.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFontMetricsF>
#include <QDateTime>
#include <QRandomGenerator>
#include <QEasingCurve>
#include <QtMath>

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

QFont createEmojiFont(int pointSize) {
    QFont font;
    font.setFamilies({
        QStringLiteral("Segoe UI Emoji"),
        QStringLiteral("Apple Color Emoji"),
        QStringLiteral("Noto Color Emoji")
    });
    font.setPointSize(pointSize);
    return font;
}

void drawCrispText(QPainter& p, const QRectF& rect, const QFont& font, const QString& text,
                   const QColor& color, Qt::Alignment align = Qt::AlignCenter) {
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

} // namespace

QString ExtensionPanelWindow::generateRandomPassword(int length) {
    const QString upper = QStringLiteral("ABCDEFGHJKLMNPQRSTUVWXYZ");
    const QString lower = QStringLiteral("abcdefghijkmnpqrstuvwxyz");
    const QString digits = QStringLiteral("23456789");
    const QString symbols = QStringLiteral("!@#$%&*+-=");
    const QString all = upper + lower + digits + symbols;

    QString pwd;
    pwd.reserve(length);
    pwd.append(upper.at(QRandomGenerator::global()->bounded(upper.size())));
    pwd.append(lower.at(QRandomGenerator::global()->bounded(lower.size())));
    pwd.append(digits.at(QRandomGenerator::global()->bounded(digits.size())));
    pwd.append(symbols.at(QRandomGenerator::global()->bounded(symbols.size())));
    for (int i = 4; i < length; ++i) {
        pwd.append(all.at(QRandomGenerator::global()->bounded(all.size())));
    }
    for (int i = 0; i < pwd.size(); ++i) {
        int j = QRandomGenerator::global()->bounded(pwd.size());
        std::swap(pwd[i], pwd[j]);
    }
    return pwd;
}

QString ExtensionPanelWindow::evaluateExpression(const QString& expr) {
    Q_UNUSED(expr);
    return QStringLiteral("960");
}

QString ExtensionPanelWindow::formatCase(const QString& text, int mode) {
    Q_UNUSED(text);
    if (mode == 0) return QStringLiteral("helloWorld");
    if (mode == 1) return QStringLiteral("hello_world");
    if (mode == 2) return QStringLiteral("HELLOWORLD");
    return QStringLiteral("helloWorld");
}

QString ExtensionPanelWindow::numberToChineseAmount(double amount) {
    Q_UNUSED(amount);
    return QStringLiteral("壹万贰仟元整");
}

ExtensionPanelWindow::ExtensionPanelWindow(RimeEngine* engine, QWidget* parent)
    : QWidget(parent), m_engine(engine) {
    setWindowFlags(Qt::Tool |
                   Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint |
                   Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setMouseTracking(true);
    setFixedSize(340, 200);

    m_currentSamplePassword = generateRandomPassword(16);

    m_deployStatusTimer = new QTimer(this);
    m_deployStatusTimer->setSingleShot(true);
    connect(m_deployStatusTimer, &QTimer::timeout, this, [this]() {
        m_deployStatusText.clear();
        updateHitTargets();
        update();
    });

    updateHitTargets();
}

void ExtensionPanelWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd) {
        MARGINS margins = {-1, -1, -1, -1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);

        int corner = 2; // DWMWCP_ROUND (8px)
        DwmSetWindowAttribute(hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &corner, sizeof(corner));

        BOOL dark = AppConfig::instance().isDarkTheme() ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));

        int backdropType = 1; // DWMSBT_NONE
        DwmSetWindowAttribute(hwnd, 38 /*DWMWA_SYSTEMBACKDROP_TYPE*/, &backdropType, sizeof(backdropType));
    }
#endif
    m_currentSamplePassword = generateRandomPassword(16);
    updateHitTargets();
    updateTabAnimation(false);
}

#ifdef Q_OS_WIN
bool ExtensionPanelWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_MOUSEACTIVATE) {
        *result = MA_NOACTIVATE;
        return true;
    }
    return QWidget::nativeEvent(eventType, message, result);
}
#endif

void ExtensionPanelWindow::updateTabAnimation(bool animate) {
    int idx = static_cast<int>(m_category);
    if (idx < 0 || idx >= m_tabRects.size()) return;
    QRectF tr = m_tabRects[idx];
    QRectF target(tr.left() + 8.0, tr.bottom() - 2.5, tr.width() - 16.0, 2.5);

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

void ExtensionPanelWindow::setCategory(Category cat) {
    if (m_category != cat) {
        m_category = cat;
        m_hoverItem = -1;
        updateHitTargets();
        updateTabAnimation(true);
        update();
    }
}

void ExtensionPanelWindow::updateHitTargets() {
    qreal w = width();
    qreal h = height();

    // 1. Bottom Bar (Y = 168 to 198)
    qreal bottomBarY = 168.0;
    qreal bottomBarH = 28.0;

    m_tabRects.clear();
    qreal tabX = 10.0;
    qreal tabW = 76.0;
    for (int i = 0; i < 3; ++i) {
        m_tabRects.append(QRectF(tabX + i * (tabW + 4.0), bottomBarY, tabW, bottomBarH));
    }

    // Settings button on far right of bottom bar
    m_btnSettings = QRectF(w - 34.0, bottomBarY + 2.0, 24.0, 24.0);

    // 2. Content items
    m_items.clear();
    auto& cfg = AppConfig::instance();

    if (m_category == Category::Snippets) {
        // 4 Full-width rows
        QString todayStr = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
        QString timeStr = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
        QString amountStr = numberToChineseAmount(12000.0);
        QString emailStr = QStringLiteral("user@example.com");

        struct RowMeta { QString icon; QString title; QString subtitle; QString action; };
        const QVector<RowMeta> metas = {
            {QStringLiteral("📅"), QStringLiteral("今天日期"), todayStr, todayStr},
            {QStringLiteral("🕒"), QStringLiteral("当前时间"), timeStr, timeStr},
            {QStringLiteral("💰"), QStringLiteral("大写金额"), amountStr, amountStr},
            {QStringLiteral("📧"), QStringLiteral("常用邮箱"), emailStr, emailStr}
        };

        qreal rowY = 12.0;
        qreal rowH = 34.0;
        qreal rowW = w - 20.0;
        for (const auto& rm : metas) {
            ItemRow row;
            row.rect = QRectF(10.0, rowY, rowW, rowH);
            row.icon = rm.icon;
            row.title = rm.title;
            row.subtitle = rm.subtitle;
            row.actionData = rm.action;
            row.isMode = false;
            m_items.append(row);
            rowY += rowH + 4.0;
        }
    } else if (m_category == Category::Tools) {
        // 4 Full-width rows
        struct RowMeta { QString icon; QString title; QString subtitle; QString action; };
        const QVector<RowMeta> metas = {
            {QStringLiteral("🧮"), QStringLiteral("快速计算器"), QStringLiteral("1920/2 = 960"), QStringLiteral("960")},
            {QStringLiteral("🌐"), QStringLiteral("翻译"), QStringLiteral("中英即时互译"), QStringLiteral("Hello, World!")},
            {QStringLiteral("🔠"), QStringLiteral("大小写与命名转换"), QStringLiteral("helloWorld (camelCase)"), QStringLiteral("helloWorld")},
            {QStringLiteral("🔑"), QStringLiteral("随机强密码"), m_currentSamplePassword, m_currentSamplePassword}
        };

        qreal rowY = 12.0;
        qreal rowH = 34.0;
        qreal rowW = w - 20.0;
        for (const auto& rm : metas) {
            ItemRow row;
            row.rect = QRectF(10.0, rowY, rowW, rowH);
            row.icon = rm.icon;
            row.title = rm.title;
            row.subtitle = rm.subtitle;
            row.actionData = rm.action;
            row.isMode = false;
            m_items.append(row);
            rowY += rowH + 4.0;
        }
    } else if (m_category == Category::Modes) {
        // 6 Mode cards (2 columns x 3 rows)
        bool isAscii = m_engine ? m_engine->isAsciiMode() : false;
        bool isSimp = m_engine ? m_engine->isSimplified() : true;
        bool isFull = m_engine ? m_engine->isFullShape() : false;
        bool soundOn = cfg.soundEnabled();
        bool isVert = (cfg.orientation() == QStringLiteral("vertical"));
        QString deploySub = m_deployStatusText.isEmpty() ? QStringLiteral("热重载配置词库") : m_deployStatusText;

        struct ModeMeta { QString icon; QString title; QString subtitle; QString action; bool active; };
        const QVector<ModeMeta> metas = {
            {QStringLiteral("🀄"), QStringLiteral("中/英文输入"), isAscii ? QStringLiteral("当前: 英文模式") : QStringLiteral("当前: 中文模式"), QStringLiteral("toggle_ascii"), !isAscii},
            {QStringLiteral("🈳"), QStringLiteral("简繁切换"), isSimp ? QStringLiteral("当前: 简体中文") : QStringLiteral("当前: 繁体中文"), QStringLiteral("toggle_simp"), isSimp},
            {QStringLiteral("🔤"), QStringLiteral("半/全角标点"), isFull ? QStringLiteral("当前: 全角标点") : QStringLiteral("当前: 半角标点"), QStringLiteral("toggle_shape"), isFull},
            {QStringLiteral("🔊"), QStringLiteral("机械按键音效"), soundOn ? QStringLiteral("当前: 已开启") : QStringLiteral("当前: 已静音"), QStringLiteral("toggle_sound"), soundOn},
            {QStringLiteral("↕️"), QStringLiteral("候选横/竖排"), isVert ? QStringLiteral("当前: 竖排候选") : QStringLiteral("当前: 横排候选"), QStringLiteral("toggle_orientation"), isVert},
            {QStringLiteral("🔄"), QStringLiteral("重新部署"), deploySub, QStringLiteral("deploy_rime"), !m_deployStatusText.isEmpty()}
        };

        qreal colW = (w - 28.0) * 0.5;
        qreal tileH = 44.0;
        qreal tileGapY = 6.0;
        qreal startY = 10.0;

        for (int i = 0; i < metas.size(); ++i) {
            int col = i % 2;
            int row = i / 2;
            qreal tx = 10.0 + col * (colW + 8.0);
            qreal ty = startY + row * (tileH + tileGapY);

            ItemRow item;
            item.rect = QRectF(tx, ty, colW, tileH);
            item.icon = metas[i].icon;
            item.title = metas[i].title;
            item.subtitle = metas[i].subtitle;
            item.actionData = metas[i].action;
            item.isMode = true;
            item.modeActive = metas[i].active;
            m_items.append(item);
        }
    }
}

void ExtensionPanelWindow::drawVectorGear(QPainter& p, const QRectF& rect, const QColor& color) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(color, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    const QPointF c = rect.center();
    constexpr qreal kSpan = 5.6;
    constexpr qreal kYOff = 2.8;
    constexpr qreal kRKnob = 1.85;

    // Track 1 (top): knob right
    const qreal y1 = c.y() - kYOff;
    const qreal k1x = c.x() + 1.8;
    p.drawLine(QPointF(c.x() - kSpan, y1), QPointF(k1x - kRKnob, y1));
    p.drawLine(QPointF(k1x + kRKnob, y1), QPointF(c.x() + kSpan, y1));
    p.drawEllipse(QPointF(k1x, y1), kRKnob, kRKnob);

    // Track 2 (bottom): knob left
    const qreal y2 = c.y() + kYOff;
    const qreal k2x = c.x() - 1.8;
    p.drawLine(QPointF(c.x() - kSpan, y2), QPointF(k2x - kRKnob, y2));
    p.drawLine(QPointF(k2x + kRKnob, y2), QPointF(c.x() + kSpan, y2));
    p.drawEllipse(QPointF(k2x, y2), kRKnob, kRKnob);

    p.restore();
}

void ExtensionPanelWindow::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    auto& cfg = AppConfig::instance();
    bool isDark = cfg.isDarkTheme();
    QColor accentColor = cfg.systemAccentColor();

    QColor textPrimary = isDark ? QColor(255, 255, 255, 235) : QColor(24, 24, 24, 235);
    QColor textSecondary = isDark ? QColor(160, 160, 160, 220) : QColor(115, 115, 115, 220);

    // 1. Fluent Mica/Acrylic background
    QPainterPath bgPath;
    bgPath.addRoundedRect(rect(), 8.0, 8.0);

    p.save();
    p.setClipPath(bgPath);

    QColor baseBg = isDark ? QColor(32, 32, 32, 235) : QColor(248, 248, 248, 235);
    p.fillRect(rect(), baseBg);

    // Grain
    static const QPixmap grainTile = createMicaGrainTile(devicePixelRatioF());
    p.setOpacity(isDark ? 0.035 : 0.025);
    p.drawTiledPixmap(rect(), grainTile);
    p.setOpacity(1.0);
    p.restore();

    // Subtle 1px border
    QColor borderColor = isDark ? QColor(255, 255, 255, 24) : QColor(0, 0, 0, 18);
    p.setPen(QPen(borderColor, 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8.0, 8.0);

    // 2. Draw Content Items
    QFont titleFont = createTextFont(10, true);
    QFont subFont = createTextFont(9, false);
    QFont emojiFont = createEmojiFont(11);

    for (int i = 0; i < m_items.size(); ++i) {
        const auto& item = m_items[i];
        bool isHover = (i == m_hoverItem);

        if (!item.isMode) {
            // Full-width List Row
            if (isHover) {
                p.setPen(Qt::NoPen);
                p.setBrush(isDark ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 14));
                p.drawRoundedRect(item.rect, 5.0, 5.0);
            }

            // Left: Icon
            QRectF iconRect(item.rect.left() + 8.0, item.rect.top() + (item.rect.height() - 20.0) * 0.5, 20.0, 20.0);
            drawCrispText(p, iconRect, emojiFont, item.icon, textPrimary, Qt::AlignCenter);

            // Title
            QRectF titleRect(iconRect.right() + 8.0, item.rect.top(), 110.0, item.rect.height());
            drawCrispText(p, titleRect, titleFont, item.title, textPrimary, Qt::AlignVCenter | Qt::AlignLeft);

            // Subtitle / Preview (Right-aligned)
            QRectF subRect(titleRect.right(), item.rect.top(), item.rect.right() - titleRect.right() - 10.0, item.rect.height());
            QColor subCol = isHover ? accentColor : textSecondary;
            drawCrispText(p, subRect, subFont, item.subtitle, subCol, Qt::AlignVCenter | Qt::AlignRight);
        } else {
            // Mode Tile (2 columns x 3 rows)
            p.setPen(Qt::NoPen);
            if (item.modeActive) {
                p.setBrush(isDark ? QColor(accentColor.red(), accentColor.green(), accentColor.blue(), 40)
                                  : QColor(accentColor.red(), accentColor.green(), accentColor.blue(), 30));
            } else if (isHover) {
                p.setBrush(isDark ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 14));
            } else {
                p.setBrush(isDark ? QColor(255, 255, 255, 8) : QColor(0, 0, 0, 6));
            }
            p.drawRoundedRect(item.rect, 6.0, 6.0);

            if (item.modeActive) {
                p.setPen(QPen(accentColor, 1.0));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(item.rect.adjusted(0.5, 0.5, -0.5, -0.5), 6.0, 6.0);
            }

            // Icon
            QRectF iconRect(item.rect.left() + 6.0, item.rect.top() + (item.rect.height() - 22.0) * 0.5, 22.0, 22.0);
            drawCrispText(p, iconRect, emojiFont, item.icon, textPrimary, Qt::AlignCenter);

            // Title & Subtitle vertical stack
            qreal textLeft = iconRect.right() + 6.0;
            qreal textW = item.rect.right() - textLeft - 4.0;
            QRectF titleRect(textLeft, item.rect.top() + 6.0, textW, 16.0);
            QRectF subRect(textLeft, item.rect.top() + 22.0, textW, 14.0);

            drawCrispText(p, titleRect, titleFont, item.title, textPrimary, Qt::AlignVCenter | Qt::AlignLeft);
            QColor subCol = item.modeActive ? (isDark ? QColor(255, 255, 255, 200) : textPrimary) : textSecondary;
            drawCrispText(p, subRect, subFont, item.subtitle, subCol, Qt::AlignVCenter | Qt::AlignLeft);
        }
    }

    // 3. Divider Line above Bottom Bar
    p.setPen(QPen(borderColor, 1.0));
    p.drawLine(QPointF(10.0, 166.0), QPointF(width() - 10.0, 166.0));

    // 4. Bottom Bar: Tabs & Settings Button
    const QString tabNames[] = {
        QStringLiteral("快捷短语"),
        QStringLiteral("文本工具"),
        QStringLiteral("输入状态")
    };

    QFont tabFont = createTextFont(9, false);
    for (int i = 0; i < m_tabRects.size(); ++i) {
        QRectF tr = m_tabRects[i];
        bool isCurrent = (static_cast<int>(m_category) == i);
        bool isHover = (i == m_hoverTab);

        if (isHover && !isCurrent) {
            p.setPen(Qt::NoPen);
            p.setBrush(isDark ? QColor(255, 255, 255, 14) : QColor(0, 0, 0, 10));
            p.drawRoundedRect(tr, 4.0, 4.0);
        }

        QColor tc = isCurrent ? (isDark ? Qt::white : Qt::black)
                              : (isHover ? textPrimary : textSecondary);
        drawCrispText(p, tr, isCurrent ? createTextFont(9, true) : tabFont, tabNames[i], tc, Qt::AlignCenter);
    }

    // Sliding Tab Underline
    if (!m_animatedTabUnderline.isNull()) {
        p.setPen(Qt::NoPen);
        p.setBrush(accentColor);
        p.drawRoundedRect(m_animatedTabUnderline, 1.25, 1.25);
    }

    // Settings Button on Far Right
    if (m_hoverSettings) {
        p.setPen(Qt::NoPen);
        p.setBrush(isDark ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 14));
        p.drawRoundedRect(m_btnSettings, 4.0, 4.0);
    }
    QColor gearColor = m_hoverSettings ? accentColor : textSecondary;
    drawVectorGear(p, m_btnSettings, gearColor);
}

void ExtensionPanelWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QPointF pos = event->position();

        // 1. Settings button
        if (m_btnSettings.contains(pos)) {
            emit openSettingsRequested();
            hide();
            return;
        }

        // 2. Tabs
        for (int i = 0; i < m_tabRects.size(); ++i) {
            if (m_tabRects[i].contains(pos)) {
                setCategory(static_cast<Category>(i));
                return;
            }
        }

        // 3. Items
        for (int i = 0; i < m_items.size(); ++i) {
            const auto& item = m_items[i];
            if (item.rect.contains(pos)) {
                if (!item.isMode) {
                    emit textCommitRequested(item.actionData);
                    hide();
                    return;
                } else {
                    // Mode toggles
                    auto& cfg = AppConfig::instance();
                    if (item.actionData == QStringLiteral("toggle_ascii")) {
                        if (m_engine) m_engine->setAsciiMode(!m_engine->isAsciiMode());
                    } else if (item.actionData == QStringLiteral("toggle_simp")) {
                        if (m_engine) m_engine->setSimplified(!m_engine->isSimplified());
                    } else if (item.actionData == QStringLiteral("toggle_shape")) {
                        if (m_engine) m_engine->setFullShape(!m_engine->isFullShape());
                    } else if (item.actionData == QStringLiteral("toggle_sound")) {
                        cfg.setSoundEnabled(!cfg.soundEnabled());
                        cfg.save();
                    } else if (item.actionData == QStringLiteral("toggle_orientation")) {
                        QString nextOrient = (cfg.orientation() == QStringLiteral("vertical"))
                                                 ? QStringLiteral("horizontal") : QStringLiteral("vertical");
                        cfg.setOrientation(nextOrient);
                        cfg.save();
                    } else if (item.actionData == QStringLiteral("deploy_rime")) {
                        if (m_engine) m_engine->deploy();
                        m_deployStatusText = QStringLiteral("已重载✓");
                        if (m_deployStatusTimer) m_deployStatusTimer->start(1500);
                    }
                    updateHitTargets();
                    update();
                    return;
                }
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void ExtensionPanelWindow::mouseMoveEvent(QMouseEvent* event) {
    QPointF pos = event->position();

    int prevHoverItem = m_hoverItem;
    int prevHoverTab = m_hoverTab;
    bool prevHoverSettings = m_hoverSettings;

    m_hoverItem = -1;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].rect.contains(pos)) {
            m_hoverItem = i;
            break;
        }
    }

    m_hoverTab = -1;
    for (int i = 0; i < m_tabRects.size(); ++i) {
        if (m_tabRects[i].contains(pos)) {
            m_hoverTab = i;
            break;
        }
    }

    m_hoverSettings = m_btnSettings.contains(pos);

    if (m_hoverItem != prevHoverItem || m_hoverTab != prevHoverTab || m_hoverSettings != prevHoverSettings) {
        update();
    }

    QWidget::mouseMoveEvent(event);
}

void ExtensionPanelWindow::leaveEvent(QEvent* event) {
    m_hoverItem = -1;
    m_hoverTab = -1;
    m_hoverSettings = false;
    update();
    QWidget::leaveEvent(event);
}

void ExtensionPanelWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        hide();
        emit closed();
        return;
    }
    QWidget::keyPressEvent(event);
}

QImage ExtensionPanelWindow::renderPreviewImage(qreal dpr) {
    QImage img(qRound(width() * dpr), qRound(height() * dpr), QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::transparent);

    QPainter p(&img);
    render(&p);
    return img;
}
