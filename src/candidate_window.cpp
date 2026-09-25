#include "candidate_window.h"
#include "emoji_picker_window.h"
#include "extension_panel_window.h"

#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QFontMetricsF>
#include <QScreen>
#include <QtMath>
#include <QDebug>
#include <QVariantAnimation>
#include <QEasingCurve>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#include <mmsystem.h>
#ifdef small
#undef small
#endif
#endif

#include <QAction>
#include <QActionGroup>
#include <QDesktopServices>
#include <QUrl>

#include "components/menus_toolbars/Menu.h"
#include "design/Typography.h"
#include "components/windowing/WindowBackdropMaterial.h"

#ifdef Q_OS_WIN
void playMechanicalClickSound() {
    static QByteArray s_clickWav;
    static bool s_inited = false;
    if (!s_inited) {
        s_inited = true;
        const int sampleRate = 44100;
        const int numSamples = 1200; // ~27ms
        QByteArray pcm;
        pcm.resize(numSamples * 2);
        qint16* samples = reinterpret_cast<qint16*>(pcm.data());
        for (int i = 0; i < numSamples; ++i) {
            double t = (double)i / sampleRate;
            double env = std::exp(-t * 220.0);
            double tone = std::sin(2.0 * M_PI * 1800.0 * t) * 0.6 + std::sin(2.0 * M_PI * 900.0 * t) * 0.4;
            double noise = ((double)rand() / RAND_MAX - 0.5) * 0.3 * std::exp(-t * 400.0);
            samples[i] = static_cast<qint16>(qBound(-32767.0, (tone + noise) * env * 24000.0, 32767.0));
        }

        s_clickWav.append("RIFF", 4);
        quint32 fileSize = 36 + pcm.size();
        s_clickWav.append(reinterpret_cast<const char*>(&fileSize), 4);
        s_clickWav.append("WAVEfmt ", 8);
        quint32 fmtChunkSize = 16;
        quint16 audioFormat = 1;
        quint16 numChannels = 1;
        quint32 byteRate = sampleRate * 2;
        quint16 blockAlign = 2;
        quint16 bitsPerSample = 16;
        s_clickWav.append(reinterpret_cast<const char*>(&fmtChunkSize), 4);
        s_clickWav.append(reinterpret_cast<const char*>(&audioFormat), 2);
        s_clickWav.append(reinterpret_cast<const char*>(&numChannels), 2);
        s_clickWav.append(reinterpret_cast<const char*>(&sampleRate), 4);
        s_clickWav.append(reinterpret_cast<const char*>(&byteRate), 4);
        s_clickWav.append(reinterpret_cast<const char*>(&blockAlign), 2);
        s_clickWav.append(reinterpret_cast<const char*>(&bitsPerSample), 2);
        s_clickWav.append("data", 4);
        quint32 dataSize = pcm.size();
        s_clickWav.append(reinterpret_cast<const char*>(&dataSize), 4);
        s_clickWav.append(pcm);
    }
    PlaySoundW(reinterpret_cast<LPCWSTR>(s_clickWav.constData()), NULL, SND_MEMORY | SND_ASYNC);
}
#else
void playMechanicalClickSound() {}
#endif

namespace {

QIcon createFluentMenuIcon(const QString& glyph, const QColor& color) {
    QIcon icon;
    for (int size : {16, 20, 24}) {
        QPixmap pm(size * 2, size * 2);
        pm.setDevicePixelRatio(2.0);
        pm.fill(Qt::transparent);
        {
            QPainter p(&pm);
            p.setRenderHint(QPainter::TextAntialiasing);
            p.setPen(color);
            Typography::Icons::paintGlyph(p, QRectF(0, 0, size, size), glyph, size, Qt::AlignCenter);
        }
        icon.addPixmap(pm, QIcon::Normal, QIcon::Off);
        icon.addPixmap(pm, QIcon::Active, QIcon::Off);
        icon.addPixmap(pm, QIcon::Selected, QIcon::Off);
        icon.addPixmap(pm, QIcon::Normal, QIcon::On);
        icon.addPixmap(pm, QIcon::Active, QIcon::On);
    }
    return icon;
}

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

void paintCleanMica(QPainter& p, const QPainterPath& path, const QRectF& r, bool isDark, qreal dpr) {
    p.save();
    p.setClipPath(path);

    // 1. Pure, uniform WinUI 3 / Windows 11 Mica base color (#202020 for Dark, #F3F3F3 for Light)
    // Completely eliminates artificial radial gradients and splotchy accent color fields!
    QColor base = isDark ? QColor(32, 32, 32) : QColor(243, 243, 243);
    p.fillPath(path, base);

    // 2. Authentic Win11 tactile micro-grain noise tile (Uniform, zero gradient)
    static QPixmap grain = createMicaGrainTile(dpr);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.setOpacity(isDark ? 0.022 : 0.015);
    p.drawTiledPixmap(r, grain, QPointF(0, 0));

    p.restore();
}

bool isEmojiText(const QString& str) {
    const auto ucs = str.toUcs4();
    for (uint val : ucs) {
        if ((val >= 0x1F000 && val <= 0x1FAFF) ||
            (val >= 0x2600 && val <= 0x27BF) ||
            (val >= 0x1F900 && val <= 0x1F9FF) ||
            (val >= 0x1F300 && val <= 0x1F5FF) ||
            (val >= 0x1F600 && val <= 0x1F64F)) {
            return true;
        }
    }
    return false;
}

QFont createCandidateChineseFont(int pointSize) {
    QFont font;
    auto& cfg = AppConfig::instance();
    QString pref = cfg.fontFamily();
    QStringList fams;
    if (!pref.isEmpty() && pref != QStringLiteral("auto")) {
        fams << pref;
    }
    fams << QStringLiteral("Microsoft YaHei UI")
         << QStringLiteral("Microsoft YaHei")
         << QStringLiteral("Segoe UI Variable Text")
         << QStringLiteral("Segoe UI");
    font.setFamilies(fams);
    font.setPointSize(pointSize);
    font.setWeight(QFont::Normal);
    font.setBold(false);
    font.setKerning(true);
    font.setHintingPreference(QFont::PreferVerticalHinting);
    font.setStyleStrategy(QFont::StyleStrategy(QFont::PreferQuality | QFont::PreferAntialias));
    return font;
}

QFont createCandidateNumberFont(int pointSize) {
    QFont font;
    font.setFamilies({
        QStringLiteral("Segoe UI"),
        QStringLiteral("Segoe UI Variable Text"),
        QStringLiteral("Microsoft YaHei UI")
    });
    font.setPointSize(pointSize);
    font.setWeight(QFont::Normal);
    font.setBold(false);
    font.setKerning(true);
    font.setHintingPreference(QFont::PreferVerticalHinting);
    font.setStyleStrategy(QFont::StyleStrategy(QFont::PreferQuality | QFont::PreferAntialias));
    return font;
}

QFont createCleanCandidateFont(int pointSize) {
    return createCandidateChineseFont(pointSize);
}

QFont createWindowsNativeEmojiFont(int pointSize) {
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

void drawCrispText(QPainter& p, const QRectF& rect, const QFont& font, const QString& text, const QColor& color, Qt::Alignment align = Qt::AlignVCenter | Qt::AlignLeft) {
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

inline qreal lerp(qreal from, qreal to, qreal progress) {
    return from + (to - from) * progress;
}

inline qreal indicatorLeadingProgress(qreal progress) {
    return qBound(0.0, progress * 1.35, 1.0);
}

inline qreal indicatorTrailingProgress(qreal progress) {
    return qBound(0.0, (progress - 0.18) / 0.82, 1.0);
}

} // namespace

CandidateWindow::CandidateWindow(RimeEngine* engine, QWidget* parent)
    : QWidget(parent), m_engine(engine) {
    setWindowFlags(Qt::Tool |
                   Qt::FramelessWindowHint |
                   Qt::WindowDoesNotAcceptFocus |
                   Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setMouseTracking(true);

    m_emojiPicker = new EmojiPickerWindow(nullptr);
    connect(m_emojiPicker, &EmojiPickerWindow::textSelected, this, [this](const QString& text) {
        if (m_engine) {
            m_engine->commitDirectText(text);
        }
    });

    m_extensionPanel = new ExtensionPanelWindow(m_engine, nullptr);
    connect(m_extensionPanel, &ExtensionPanelWindow::textCommitRequested, this, [this](const QString& text) {
        if (m_engine) {
            m_engine->commitDirectText(text);
        }
    });
    connect(m_extensionPanel, &ExtensionPanelWindow::openSettingsRequested, this, [this]() {
        emit openSettingsRequested();
    });

    if (m_engine) {
        connect(m_engine, &RimeEngine::stateChanged, this, &CandidateWindow::updateUiState);
        m_state = m_engine->getUiState();
    }
    connect(&AppConfig::instance(), &AppConfig::configChanged, this, &CandidateWindow::reloadConfig);

    reloadConfig();
}

CandidateWindow::~CandidateWindow() {
    if (m_emojiPicker) {
        delete m_emojiPicker;
        m_emojiPicker = nullptr;
    }
    if (m_extensionPanel) {
        delete m_extensionPanel;
        m_extensionPanel = nullptr;
    }
}

#ifdef Q_OS_WIN
bool CandidateWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_MOUSEACTIVATE) {
        *result = MA_NOACTIVATE;
        return true;
    }
    return QWidget::nativeEvent(eventType, message, result);
}
#endif

void CandidateWindow::reloadConfig() {
    if (m_engine) {
        m_state = m_engine->getUiState();
    }
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd) {
        // 1. Extend frame into client area with -1 margins
        MARGINS margins = {-1, -1, -1, -1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);

        // 2. Hardware 8px rounded corners via DWM
        int corner = 2; // DWMWCP_ROUND
        DwmSetWindowAttribute(hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &corner, sizeof(corner));

        // 3. Immersive Dark Mode
        BOOL dark = AppConfig::instance().isDarkTheme() ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));

        // 4. Disable DWM opaque system backdrop so transparent widget surface has zero square corner artifacts
        int backdropType = 1; // DWMSBT_NONE
        DwmSetWindowAttribute(hwnd, 38 /*DWMWA_SYSTEMBACKDROP_TYPE*/, &backdropType, sizeof(backdropType));
    }
#endif
    calculateLayout();
    updateSelectionAnimation(false);
    update();
}

void CandidateWindow::showEvent(QShowEvent* event) {
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

void CandidateWindow::startEntranceAnimation() {
    if (!m_fadeAnim) {
        m_fadeAnim = new QVariantAnimation(this);
        m_fadeAnim->setDuration(120);
        m_fadeAnim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_fadeAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
            m_windowOpacity = val.toReal();
            update();
        });
    }
    m_fadeAnim->stop();
    m_fadeAnim->setStartValue(0.3);
    m_fadeAnim->setEndValue(1.0);
    m_fadeAnim->start();
}

QRectF CandidateWindow::indicatorBaseRect(int index) const {
    for (const auto& cr : m_candidateRects) {
        if (cr.index == index) {
            return QRectF(cr.rect.left() + 4.0,
                          cr.rect.top() + (cr.rect.height() - 14.0) * 0.5,
                          2.5, 14.0);
        }
    }
    return QRectF();
}

QRectF CandidateWindow::currentIndicatorRect() const {
    const QRectF target = indicatorBaseRect(m_state.highlightedIndex);
    if (target.isEmpty()) return {};

    if (m_indicatorMotionDirection == 0 || m_indicatorPrevRect.isEmpty() ||
        qFuzzyCompare(m_indicatorProgress + 1.0, 2.0)) {
        return target;
    }
    if (qFuzzyCompare(m_indicatorProgress + 1.0, 1.0)) {
        return m_indicatorPrevRect;
    }

    const qreal clamped = qBound(0.0, m_indicatorProgress, 1.0);
    const qreal leading = indicatorLeadingProgress(clamped);
    const qreal trailing = indicatorTrailingProgress(clamped);

    bool isVertical = (AppConfig::instance().orientation() == QStringLiteral("vertical"));

    if (isVertical) {
        qreal top = target.top();
        qreal bottom = target.bottom();
        qreal left = target.left();
        qreal right = target.right();

        if (m_indicatorMotionDirection > 0) { // Down
            top = lerp(m_indicatorPrevRect.top(), target.top(), trailing);
            bottom = lerp(m_indicatorPrevRect.bottom(), target.bottom(), leading);
        } else { // Up
            top = lerp(m_indicatorPrevRect.top(), target.top(), leading);
            bottom = lerp(m_indicatorPrevRect.bottom(), target.bottom(), trailing);
        }
        const qreal normalizedTop = qMin(top, bottom);
        const qreal normalizedBottom = qMax(top, bottom);
        return QRectF(QPointF(left, normalizedTop), QPointF(right, normalizedBottom));
    } else {
        qreal left = target.left();
        qreal right = target.right();
        qreal top = target.top();
        qreal bottom = target.bottom();

        if (m_indicatorMotionDirection > 0) { // Right
            left = lerp(m_indicatorPrevRect.left(), target.left(), trailing);
            right = lerp(m_indicatorPrevRect.right(), target.right(), leading);
        } else { // Left
            left = lerp(m_indicatorPrevRect.left(), target.left(), leading);
            right = lerp(m_indicatorPrevRect.right(), target.right(), trailing);
        }
        const qreal normalizedLeft = qMin(left, right);
        const qreal normalizedRight = qMax(left, right);
        return QRectF(QPointF(normalizedLeft, top), QPointF(normalizedRight, bottom));
    }
}

void CandidateWindow::updateSelectionAnimation(bool animate) {
    int newIdx = m_state.highlightedIndex;
    const QRectF target = indicatorBaseRect(newIdx);
    if (target.isEmpty()) {
        m_indicatorProgress = 1.0;
        m_indicatorMotionDirection = 0;
        m_indicatorPrevRect = QRectF();
        m_indicatorTargetRect = QRectF();
        return;
    }

    if (!animate || !isVisible() || m_indicatorTargetRect.isEmpty()) {
        if (m_selectionAnim) m_selectionAnim->stop();
        m_indicatorProgress = 1.0;
        m_indicatorMotionDirection = 0;
        m_indicatorPrevRect = target;
        m_indicatorTargetRect = target;
        m_lastHighlightIndex = newIdx;
        return;
    }

    if (newIdx == m_lastHighlightIndex && target == m_indicatorTargetRect) {
        return;
    }

    const QRectF prevRect = indicatorBaseRect(m_lastHighlightIndex);
    if (m_selectionAnim && m_selectionAnim->state() == QAbstractAnimation::Running) {
        m_indicatorPrevRect = currentIndicatorRect();
    } else {
        m_indicatorPrevRect = prevRect.isEmpty() ? m_indicatorTargetRect : prevRect;
    }
    if (m_indicatorPrevRect.isEmpty()) {
        m_indicatorPrevRect = target;
    }

    m_indicatorTargetRect = target;
    m_indicatorMotionDirection = (newIdx > m_lastHighlightIndex) ? 1 : -1;
    m_lastHighlightIndex = newIdx;

    if (!m_selectionAnim) {
        m_selectionAnim = new QVariantAnimation(this);
        m_selectionAnim->setDuration(160);
        m_selectionAnim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_selectionAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
            m_indicatorProgress = val.toReal();
            update();
        });
        connect(m_selectionAnim, &QVariantAnimation::finished, this, [this]() {
            m_indicatorProgress = 1.0;
            m_indicatorMotionDirection = 0;
            m_indicatorPrevRect = m_indicatorTargetRect;
            update();
        });
    }

    m_selectionAnim->stop();
    m_indicatorProgress = 0.0;
    m_selectionAnim->setStartValue(0.0);
    m_selectionAnim->setEndValue(1.0);
    m_selectionAnim->start();
}

void CandidateWindow::updateUiState(const RimeUiState& state) {
    m_state = state;
    if (m_state.isComposing && !m_state.candidates.isEmpty()) {
        bool wasHidden = !isVisible();
        calculateLayout();
        updateSelectionAnimation(!wasHidden);
        if (wasHidden) {
            show();
            startEntranceAnimation();
        }
        update();
    } else {
        hide();
        m_indicatorProgress = 1.0;
        m_indicatorMotionDirection = 0;
        m_indicatorPrevRect = QRectF();
        m_indicatorTargetRect = QRectF();
        if (m_emojiPicker && m_emojiPicker->isVisible()) {
            m_emojiPicker->hide();
        }
        if (m_extensionPanel && m_extensionPanel->isVisible()) {
            m_extensionPanel->hide();
        }
    }
}

void CandidateWindow::moveToPosition(const QPoint& pt) {
    QScreen* screen = QGuiApplication::screenAt(pt);
    if (!screen) screen = QGuiApplication::primaryScreen();
    QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    int x = pt.x();
    int y = pt.y();
    int w = m_calculatedSize.width() > 0 ? m_calculatedSize.width() : width();
    int h = m_calculatedSize.height() > 0 ? m_calculatedSize.height() : height();

    // If candidate window overflows bottom of screen, flip to above the caret
    if (y + h > avail.bottom() - 4) {
        y = pt.y() - h - 28;
    }

    // Clamp coordinates to screen boundaries with padding
    x = qBound(avail.left() + 4, x, avail.right() - w - 4);
    y = qBound(avail.top() + 4, y, avail.bottom() - h - 4);

    move(x, y);
}

void CandidateWindow::calculateLayout() {
    auto& cfg = AppConfig::instance();
    bool isVertical = (cfg.orientation() == QStringLiteral("vertical"));
    int fontSize = cfg.fontSize();

    QFont cnFont = createCandidateChineseFont(fontSize);
    QFont numFont = createCandidateNumberFont(fontSize);
    QFont emojiFont = createWindowsNativeEmojiFont(fontSize + 2);
    QFontMetricsF fmCn(cnFont);
    QFontMetricsF fmNum(numFont);
    QFontMetricsF fmEmoji(emojiFont);

    m_candidateRects.clear();

    if (m_state.candidates.isEmpty()) {
        m_calculatedSize = QSize(1, 1);
        resize(1, 1);
        return;
    }

    int displayCount = qMin(static_cast<int>(m_state.candidates.size()), cfg.pageSize());

    if (!isVertical) {
        if (m_isExpanded) {
            // Horizontal Expanded Mode: Multi-row candidate matrix (380 x dynamic height)
            qreal width = 380.0;

            int maxMatrixCount = qMin(static_cast<int>(m_state.candidates.size()), 20);
            int numRows = qMax(1, (maxMatrixCount + 4) / 5);
            qreal matrixStartY = 8.0;
            qreal cellW = (width - 16.0) / 5.0; // 72.8px
            qreal cellH = 32.0;
            qreal cellGapY = 4.0;
            qreal matrixH = numRows * cellH + (numRows - 1) * cellGapY;

            for (int i = 0; i < maxMatrixCount; ++i) {
                int col = i % 5;
                int row = i / 5;
                qreal cx = 8.0 + col * cellW;
                qreal cy = matrixStartY + row * (cellH + cellGapY);
                QRectF r(cx + 2.0, cy, cellW - 4.0, cellH);
                m_candidateRects.append({m_state.candidates[i].index, r});
            }

            // Bottom bar: pagination & settings & collapse buttons
            qreal sepY = matrixStartY + matrixH + 6.0;
            qreal bottomBarY = sepY + 4.0;
            qreal bottomBarH = 22.0;

            qreal bottomBarCenter = width / 2.0;
            qreal navBtnW = 28.0;
            qreal navBtnGap = 8.0;
            qreal totalNavW = navBtnW * 2.0 + navBtnGap;

            m_btnPrevPage = QRectF(bottomBarCenter - totalNavW / 2.0, bottomBarY, navBtnW, bottomBarH);
            m_btnNextPage = QRectF(m_btnPrevPage.right() + navBtnGap, bottomBarY, navBtnW, bottomBarH);
            m_btnSettings = QRectF(width - 56.0, bottomBarY, 22.0, 22.0);
            m_btnCollapse = QRectF(width - 30.0, bottomBarY, 22.0, 22.0);

            qreal height = bottomBarY + bottomBarH + 6.0;
            m_calculatedSize = QSize(static_cast<int>(width), static_cast<int>(height));
            resize(m_calculatedSize);
            return;
        }

        // Horizontal layout (40.0px height, 32.0px pill, 4.0px top/bottom padding)
        qreal curX = 4.0;
        qreal height = 40.0;
        qreal pillH = 32.0;
        qreal pillY = 4.0;

        // If preedit exists and showPreedit is enabled, measure preedit
        if (cfg.showPreedit() && !m_state.preedit.isEmpty()) {
            qreal preeditWidth = fmCn.horizontalAdvance(m_state.preedit) + 12.0;
            curX += preeditWidth;
        }

        for (int i = 0; i < displayCount; ++i) {
            const auto& cand = m_state.candidates[i];
            QString numStr = QString::number(i + 1);
            qreal numW = cfg.showLabel() ? (fmNum.horizontalAdvance(numStr) + 5.0) : 0.0;
            qreal candW = isEmojiText(cand.text) ? fmEmoji.horizontalAdvance(cand.text) : fmCn.horizontalAdvance(cand.text);
            qreal commentW = cand.comment.isEmpty() ? 0.0 : (fmCn.horizontalAdvance(QStringLiteral(" ") + cand.comment));
            qreal textW = numW + candW + commentW;

            // Pill width: generous breathing margins
            qreal capsuleW = (i == 0) ? (textW + 22.0) : (textW + 16.0);
            QRectF r(curX, pillY, capsuleW, pillH);

            m_candidateRects.append({cand.index, r});
            curX += capsuleW + 8.0;
        }

        // Toolbar separator
        curX += 2.0;
        qreal toolY = (height - 24.0) * 0.5;

        // Buttons: < > | ♡ | ∨
        m_btnPrevPage = QRectF(curX, toolY, 16.0, 24.0);
        curX += 16.0;
        m_btnNextPage = QRectF(curX, toolY, 16.0, 24.0);
        curX += 16.0 + 8.0;

        m_btnSticker = QRectF(curX, toolY, 22.0, 24.0);
        curX += 22.0 + 8.0;

        m_btnSettings = QRectF(curX, toolY, 20.0, 24.0);
        curX += 20.0 + 6.0;

        m_calculatedSize = QSize(static_cast<int>(std::ceil(curX)), static_cast<int>(height));
        resize(m_calculatedSize);
    } else {
        // Vertical layout
        qreal width = 180.0;
        qreal rowH = 32.0;
        qreal curY = 6.0;

        if (cfg.showPreedit() && !m_state.preedit.isEmpty()) {
            qreal preW = fmCn.horizontalAdvance(m_state.preedit) + 28.0;
            width = qMax(width, preW);
            curY += 28.0;
        }

        qreal fixedNumColW = cfg.showLabel() ? (fmNum.horizontalAdvance(QStringLiteral("8")) + 6.0) : 0.0;
        qreal maxContentW = 0.0;

        for (int i = 0; i < displayCount; ++i) {
            const auto& cand = m_state.candidates[i];
            qreal candW = isEmojiText(cand.text) ? fmEmoji.horizontalAdvance(cand.text) : fmCn.horizontalAdvance(cand.text);
            qreal commentW = cand.comment.isEmpty() ? 0.0 : (fmCn.horizontalAdvance(cand.comment) + 16.0);
            maxContentW = qMax(maxContentW, candW + commentW);
        }

        width = qMax(width, 14.0 + fixedNumColW + maxContentW + 28.0);
        // Ensure enough width for toolbar (4 buttons x 24px + 3 gaps x 6px = 114px + 28px margins = 142px)
        width = qMax(width, 150.0);

        for (int i = 0; i < displayCount; ++i) {
            QRectF r(6.0, curY, width - 12.0, rowH);
            m_candidateRects.append({m_state.candidates[i].index, r});
            curY += rowH + 2.0;
        }

        curY += 6.0;
        qreal toolW = 114.0;
        qreal toolX = (width - toolW) * 0.5;
        qreal toolY = curY;
        m_btnPrevPage = QRectF(toolX, toolY, 24.0, 24.0);
        m_btnNextPage = QRectF(toolX + 30.0, toolY, 24.0, 24.0);
        m_btnSticker = QRectF(toolX + 60.0, toolY, 24.0, 24.0);
        m_btnSettings = QRectF(toolX + 90.0, toolY, 24.0, 24.0);
        curY += 28.0;

        m_calculatedSize = QSize(static_cast<int>(std::ceil(width)), static_cast<int>(std::ceil(curY + 4.0)));
        resize(m_calculatedSize);
    }
}

void CandidateWindow::setPreviewState(const RimeUiState& state) {
    m_state = state;
    calculateLayout();
    updateSelectionAnimation(false);
    resize(m_calculatedSize);
}

QImage CandidateWindow::renderPreviewImage(qreal dpr) {
    calculateLayout();
    updateSelectionAnimation(false);
    m_windowOpacity = 1.0;
    if (m_calculatedSize.isEmpty() || m_calculatedSize.width() <= 0 || m_calculatedSize.height() <= 0) {
        return QImage();
    }
    resize(m_calculatedSize);

    QSize pixelSize = m_calculatedSize * dpr;
    QImage result(pixelSize, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    result.setDevicePixelRatio(dpr);

    auto& cfg = AppConfig::instance();
    bool isDark = cfg.isDarkTheme();
    constexpr qreal kCornerRadius = 8.0;

    QRectF bgRect = QRectF(0.5, 0.5, m_calculatedSize.width() - 1.0, m_calculatedSize.height() - 1.0);
    QPainterPath bgPath;
    bgPath.addRoundedRect(bgRect, kCornerRadius, kCornerRadius);

    // 1. Offscreen opaque image for full ClearType subpixel text rendering
    QImage contentImage(pixelSize, QImage::Format_RGB32);
    contentImage.setDevicePixelRatio(dpr);

    {
        QPainter cp(&contentImage);
        cp.setRenderHint(QPainter::Antialiasing, true);
        cp.setRenderHint(QPainter::TextAntialiasing, true);

        // Fill background with Mica / solid color
        if (cfg.backdrop() == QStringLiteral("acrylic")) {
            auto options = fluent::windowing::WindowBackdropMaterialOptions::forTheme(
                isDark,
                isDark ? QColor(28, 28, 28) : QColor(248, 248, 248),
                cfg.effectiveAccentColor());
            options.effect = fluent::windowing::BackdropEffect::Acrylic;
            fluent::windowing::WindowBackdropMaterial::paintAcrylic(cp, bgRect, options);
        } else if (cfg.backdrop() == QStringLiteral("solid")) {
            QColor bgColor = isDark ? QColor(32, 32, 32) : QColor(248, 248, 248);
            cp.fillRect(QRect(0, 0, m_calculatedSize.width(), m_calculatedSize.height()), bgColor);
        } else {
            // Authentic Mica base and micro-grain
            QColor base = isDark ? QColor(32, 32, 32) : QColor(243, 243, 243);
            cp.fillRect(QRect(0, 0, m_calculatedSize.width(), m_calculatedSize.height()), base);
            static QPixmap grain = createMicaGrainTile(dpr);
            cp.setRenderHint(QPainter::SmoothPixmapTransform, false);
            cp.setOpacity(isDark ? 0.022 : 0.015);
            cp.drawTiledPixmap(QRect(0, 0, m_calculatedSize.width(), m_calculatedSize.height()), grain, QPointF(0, 0));
            cp.setOpacity(1.0);
        }

        // Draw candidates and icons in ClearType mode
        bool isVertical = (cfg.orientation() == QStringLiteral("vertical"));
        if (isVertical) {
            drawVertical(cp);
        } else {
            drawHorizontal(cp);
        }
    }

    // 2. Composite onto transparent surface with smooth rounded corners
    {
        QPainter p(&result);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        p.save();
        p.setClipPath(bgPath);
        p.drawImage(0, 0, contentImage);
        p.restore();

        // 3. Subtle outer border
        QColor borderColor = isDark ? QColor(255, 255, 255, 28) : QColor(0, 0, 0, 24);
        p.setPen(QPen(borderColor, 1.0));
        p.drawPath(bgPath);
    }

    return result;
}

void CandidateWindow::paintEvent(QPaintEvent* /*event*/) {
    if (width() <= 0 || height() <= 0) return;

    qreal dpr = devicePixelRatioF();
    QSize pixelSize = size() * dpr;
    if (pixelSize.isEmpty()) return;

    auto& cfg = AppConfig::instance();
    bool isDark = cfg.isDarkTheme();
    constexpr qreal kCornerRadius = 8.0;

    QRectF bgRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath bgPath;
    bgPath.addRoundedRect(bgRect, kCornerRadius, kCornerRadius);

    // 1. Offscreen opaque image for full ClearType subpixel text rendering
    QImage contentImage(pixelSize, QImage::Format_RGB32);
    contentImage.setDevicePixelRatio(dpr);

    {
        QPainter cp(&contentImage);
        cp.setRenderHint(QPainter::Antialiasing, true);
        cp.setRenderHint(QPainter::TextAntialiasing, true);

        // Fill background with Mica / solid color
        if (cfg.backdrop() == QStringLiteral("acrylic")) {
            auto options = fluent::windowing::WindowBackdropMaterialOptions::forTheme(
                isDark,
                isDark ? QColor(28, 28, 28) : QColor(248, 248, 248),
                cfg.effectiveAccentColor());
            options.effect = fluent::windowing::BackdropEffect::Acrylic;
            fluent::windowing::WindowBackdropMaterial::paintAcrylic(cp, bgRect, options);
        } else if (cfg.backdrop() == QStringLiteral("solid")) {
            QColor bgColor = isDark ? QColor(32, 32, 32) : QColor(248, 248, 248);
            cp.fillRect(rect(), bgColor);
        } else {
            // Authentic Mica base and micro-grain
            QColor base = isDark ? QColor(32, 32, 32) : QColor(243, 243, 243);
            cp.fillRect(rect(), base);
            static QPixmap grain = createMicaGrainTile(dpr);
            cp.setRenderHint(QPainter::SmoothPixmapTransform, false);
            cp.setOpacity(isDark ? 0.022 : 0.015);
            cp.drawTiledPixmap(rect(), grain, QPointF(0, 0));
            cp.setOpacity(1.0);
        }

        // Draw candidates and icons in ClearType mode
        bool isVertical = (cfg.orientation() == QStringLiteral("vertical"));
        if (isVertical) {
            drawVertical(cp);
        } else {
            drawHorizontal(cp);
        }
    }

    // 2. Composite onto transparent widget surface with smooth rounded corners
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.fillRect(rect(), Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    if (m_windowOpacity < 0.999) {
        p.setOpacity(m_windowOpacity);
    }

    p.save();
    p.setClipPath(bgPath);
    p.drawImage(0, 0, contentImage);
    p.restore();

    // 3. Subtle outer border
    QColor borderColor = isDark ? QColor(255, 255, 255, 28) : QColor(0, 0, 0, 24);
    p.setPen(QPen(borderColor, 1.0));
    p.drawPath(bgPath);
}

void CandidateWindow::drawHorizontalExpanded(QPainter& p) {
    auto& cfg = AppConfig::instance();
    bool isDark = cfg.isDarkTheme();
    QColor textPrimary = isDark ? QColor(255, 255, 255, 245) : QColor(25, 25, 25, 245);
    QColor textSecondary = isDark ? QColor(255, 255, 255, 140) : QColor(0, 0, 0, 135);
    QColor accentColor = cfg.effectiveAccentColor();
    QColor capsuleActive = isDark ? QColor(255, 255, 255, 12) : QColor(0, 0, 0, 10);
    QColor capsuleHover = isDark ? QColor(255, 255, 255, 7) : QColor(0, 0, 0, 5);
    QColor sepColor = isDark ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 18);

    QFont cnFont = createCandidateChineseFont(cfg.fontSize());
    QFont numFont = createCandidateNumberFont(cfg.fontSize());
    QFont emojiFont = createWindowsNativeEmojiFont(cfg.fontSize() + 2);
    QFontMetricsF fmCn(cnFont);
    QFontMetricsF fmNum(numFont);
    QFontMetricsF fmEmoji(emojiFont);

    // 1. Candidate Matrix Items (NO PINYIN - candidates start directly from top)
    for (int i = 0; i < m_candidateRects.size(); ++i) {
        const auto& cr = m_candidateRects[i];
        bool isHighlight = (cr.index == m_state.highlightedIndex);
        bool isHover = (cr.index == m_hoverCandidate);

        // A. Background capsule (Authentic Fluent subtle capsule - identical to unexpanded candidate bar)
        if (isHighlight) {
            QPainterPath path;
            path.addRoundedRect(cr.rect, 4.0, 4.0);
            p.fillPath(path, capsuleActive);
        } else if (isHover) {
            QPainterPath path;
            path.addRoundedRect(cr.rect, 4.0, 4.0);
            p.fillPath(path, capsuleHover);
        }

        // B. Active Accent Indicator (Windows 11 signature 2.5px vertical Accent bar on left edge)
        if (isHighlight) {
            QRectF indR(cr.rect.left() + 4.0,
                        cr.rect.top() + (cr.rect.height() - 14.0) * 0.5,
                        2.5, 14.0);
            QPainterPath barPath;
            barPath.addRoundedRect(indR, 1.25, 1.25);
            p.fillPath(barPath, accentColor);
        }

        // C. Text & Label inside cell
        p.save();
        p.setClipRect(cr.rect);

        QString labelStr;
        if (i < 9) labelStr = QString::number(i + 1);
        else if (i == 9) labelStr = QStringLiteral("0");
        else labelStr = QChar(QLatin1Char('a' + (i - 10)));

        qreal textX = cr.rect.left() + (isHighlight ? 12.0 : 8.0);

        if (cfg.showLabel()) {
            qreal nw = fmNum.horizontalAdvance(labelStr);
            QRectF labelR(textX, cr.rect.top(), nw, cr.rect.height());
            drawCrispText(p, labelR, numFont, labelStr, textSecondary, Qt::AlignVCenter | Qt::AlignLeft);
            textX += nw + 4.0;
        }

        if (i < m_state.candidates.size()) {
            const auto& cand = m_state.candidates[i];
            if (isEmojiText(cand.text)) {
                const QPixmap& pix = EmojiPickerWindow::getEmojiPixmap(cand.text);
                if (!pix.isNull()) {
                    qreal s = 20.0;
                    QRectF iconR(textX, cr.rect.top() + (cr.rect.height() - s) * 0.5, s, s);
                    qreal prevOp = p.opacity();
                    if (isDark) {
                        p.setOpacity((isHighlight || isHover) ? 0.98 : 0.88);
                    }
                    p.drawPixmap(iconR, pix, pix.rect());
                    if (isDark) {
                        p.setOpacity(prevOp);
                    }
                    textX += s + 4.0;
                } else {
                    p.setFont(emojiFont);
                    qreal ew = fmEmoji.horizontalAdvance(cand.text);
                    QRectF textR(textX, cr.rect.top(), ew, cr.rect.height());
                    p.drawText(textR, Qt::AlignVCenter | Qt::AlignLeft, cand.text);
                    textX += textR.width();
                }
            } else {
                qreal maxTextW = qMax(0.0, cr.rect.right() - textX - 4.0);
                QRectF textR(textX, cr.rect.top(), maxTextW, cr.rect.height());
                drawCrispText(p, textR, cnFont, cand.text, textPrimary, Qt::AlignVCenter | Qt::AlignLeft);
            }
        }
        p.restore();
    }

    // 2. Divider above bottom bar
    qreal sepY = m_btnSettings.top() - 4.0;
    p.setPen(QPen(sepColor, 1.0));
    p.drawLine(QPointF(8.0, sepY), QPointF(width() - 8.0, sepY));

    // 3. Bottom Bar (pageInfo removed as requested)
    auto drawBtnHover = [&](const QRectF& btnRect, bool isHovered) {
        if (isHovered) {
            p.setPen(Qt::NoPen);
            p.setBrush(isDark ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 14));
            p.drawRoundedRect(btnRect.adjusted(-1, -1, 1, 1), 4.0, 4.0);
        }
    };
    drawBtnHover(m_btnPrevPage, m_hoverPrev);
    drawBtnHover(m_btnNextPage, m_hoverNext);
    drawBtnHover(m_btnSettings, m_hoverSettings);
    drawBtnHover(m_btnCollapse, m_hoverCollapse);

    QColor prevColor = (m_state.pageNo == 0) ? (isDark ? QColor(255, 255, 255, 75) : QColor(0, 0, 0, 60))
                                           : (m_hoverPrev ? accentColor : textPrimary);
    QColor nextColor = m_hoverNext ? accentColor : (isDark ? QColor(255, 255, 255, 230) : QColor(0, 0, 0, 200));
    QColor toolIconColor = isDark ? QColor(255, 255, 255, 210) : QColor(0, 0, 0, 190);

    drawVectorTriangle(p, m_btnPrevPage.center(), 8.0, true, prevColor);
    drawVectorTriangle(p, m_btnNextPage.center(), 8.0, false, nextColor);

    // Settings gear icon (symmetric on right)
    drawVectorGear(p, m_btnSettings, m_hoverSettings ? accentColor : toolIconColor);

    // Collapse chevron up icon (outer right)
    drawVectorChevron(p, m_btnCollapse, m_hoverCollapse ? accentColor : toolIconColor, true /* pointsUp */);
}

void CandidateWindow::drawHorizontal(QPainter& p) {
    if (m_isExpanded) {
        drawHorizontalExpanded(p);
        return;
    }

    auto& cfg = AppConfig::instance();
    bool isDark = cfg.isDarkTheme();
    QColor textPrimary = isDark ? QColor(255, 255, 255, 245) : QColor(25, 25, 25, 245);
    QColor textSecondary = isDark ? QColor(255, 255, 255, 140) : QColor(0, 0, 0, 135);
    QColor accentColor = cfg.effectiveAccentColor();
    QColor capsuleActive = isDark ? QColor(255, 255, 255, 12) : QColor(0, 0, 0, 10);
    QColor capsuleHover = isDark ? QColor(255, 255, 255, 7) : QColor(0, 0, 0, 5);

    QFont cnFont = createCandidateChineseFont(cfg.fontSize());
    QFont numFont = createCandidateNumberFont(cfg.fontSize());
    QFont emojiFont = createWindowsNativeEmojiFont(cfg.fontSize() + 2);
    QFontMetricsF fmCn(cnFont);
    QFontMetricsF fmNum(numFont);
    QFontMetricsF fmEmoji(emojiFont);

    // Preedit (Only drawn when showPreedit is enabled)
    if (cfg.showPreedit() && !m_state.preedit.isEmpty()) {
        QRectF preeditR(8.0, 0, fmCn.horizontalAdvance(m_state.preedit) + 8.0, height());
        drawCrispText(p, preeditR, cnFont, m_state.preedit, accentColor, Qt::AlignVCenter | Qt::AlignLeft);
    }

    // 1. Candidate Background Capsules ("先选中")
    for (int i = 0; i < m_candidateRects.size(); ++i) {
        const auto& cr = m_candidateRects[i];
        bool isHighlight = (cr.index == m_state.highlightedIndex);
        bool isHover = (cr.index == m_hoverCandidate);

        if (isHighlight) {
            QPainterPath path;
            path.addRoundedRect(cr.rect, 4.0, 4.0);
            p.fillPath(path, capsuleActive);
        } else if (isHover) {
            QPainterPath path;
            path.addRoundedRect(cr.rect, 4.0, 4.0);
            p.fillPath(path, capsuleHover);
        }
    }

    // 2. Animated Theme-Colored Accent Indicator ("主题色的光标移动")
    const QRectF indR = currentIndicatorRect();
    if (!indR.isEmpty()) {
        QPainterPath barPath;
        barPath.addRoundedRect(indR, 1.25, 1.25);
        p.fillPath(barPath, accentColor);
    }

    // Candidates Text
    for (int i = 0; i < m_candidateRects.size(); ++i) {
        const auto& cr = m_candidateRects[i];
        bool isHighlight = (cr.index == m_state.highlightedIndex);
        bool isHover = (cr.index == m_hoverCandidate);

        // Draw candidate text
        const auto& cand = m_state.candidates[i];
        QString numStr = QString::number(i + 1);
        qreal textX = cr.rect.left() + (isHighlight ? 13.0 : 8.0);

        // 1. Number label (Segoe UI, softer muted secondary color)
        if (cfg.showLabel()) {
            qreal nw = fmNum.horizontalAdvance(numStr);
            QRectF labelR(textX, cr.rect.top(), nw, cr.rect.height());
            drawCrispText(p, labelR, numFont, numStr, textSecondary, Qt::AlignVCenter | Qt::AlignLeft);
            textX += nw + 5.0; // 5.0px comfortable gap between number and candidate
        }

        // 2. Candidate Character (Microsoft YaHei UI, crisp ClearType primary color)
        if (isEmojiText(cand.text)) {
            const QPixmap& pix = EmojiPickerWindow::getEmojiPixmap(cand.text);
            if (!pix.isNull()) {
                qreal s = 20.0;
                QRectF iconR(textX, cr.rect.top() + (cr.rect.height() - s) * 0.5, s, s);
                qreal prevOp = p.opacity();
                if (isDark) {
                    p.setOpacity((isHighlight || isHover) ? 0.98 : 0.88);
                }
                p.drawPixmap(iconR, pix, pix.rect());
                if (isDark) {
                    p.setOpacity(prevOp);
                }
                textX += s + 4.0;
            } else {
                p.setFont(emojiFont);
                qreal ew = fmEmoji.horizontalAdvance(cand.text);
                QRectF textR(textX, cr.rect.top(), ew, cr.rect.height());
                p.drawText(textR, Qt::AlignVCenter | Qt::AlignLeft, cand.text);
                textX += textR.width();
            }
        } else {
            qreal cw = fmCn.horizontalAdvance(cand.text);
            QRectF textR(textX, cr.rect.top(), cw, cr.rect.height());
            drawCrispText(p, textR, cnFont, cand.text, textPrimary, Qt::AlignVCenter | Qt::AlignLeft);
            textX += textR.width();
        }

        // 3. Comment
        if (!cand.comment.isEmpty()) {
            QRectF commentR(textX + 4.0, cr.rect.top(), fmCn.horizontalAdvance(cand.comment), cr.rect.height());
            drawCrispText(p, commentR, cnFont, cand.comment, textSecondary, Qt::AlignVCenter | Qt::AlignLeft);
        }
    }

    // Separators (16px high centered vertically, thin 1px subtle lines)
    qreal sepY = (height() - 16.0) * 0.5;
    QColor sepColor = isDark ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 18);
    p.setPen(QPen(sepColor, 1.0));
    if (!m_candidateRects.isEmpty()) {
        qreal sep1X = m_btnPrevPage.left() - 4.0;
        p.drawLine(QPointF(sep1X, sepY), QPointF(sep1X, sepY + 16.0));

        qreal sep2X = m_btnSticker.left() - 4.0;
        p.drawLine(QPointF(sep2X, sepY), QPointF(sep2X, sepY + 16.0));

        qreal sep3X = m_btnSettings.left() - 4.0;
        p.drawLine(QPointF(sep3X, sepY), QPointF(sep3X, sepY + 16.0));
    }

    // Button hover backgrounds
    auto drawBtnHover = [&](const QRectF& btnRect, bool isHovered) {
        if (isHovered) {
            p.setPen(Qt::NoPen);
            p.setBrush(isDark ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 14));
            p.drawRoundedRect(btnRect.adjusted(-1, -1, 1, 1), 4.0, 4.0);
        }
    };
    drawBtnHover(m_btnPrevPage, m_hoverPrev);
    drawBtnHover(m_btnNextPage, m_hoverNext);
    drawBtnHover(m_btnSticker, m_hoverSticker);
    drawBtnHover(m_btnSettings, m_hoverSettings);

    // Toolbar buttons
    QColor prevColor = (m_state.pageNo == 0) ? (isDark ? QColor(255, 255, 255, 75) : QColor(0, 0, 0, 60))
                                           : (m_hoverPrev ? accentColor : textPrimary);
    QColor nextColor = m_hoverNext ? accentColor : (isDark ? QColor(255, 255, 255, 230) : QColor(0, 0, 0, 200));
    QColor toolIconColor = isDark ? QColor(255, 255, 255, 210) : QColor(0, 0, 0, 190);

    drawVectorTriangle(p, m_btnPrevPage.center(), 6.0, true, prevColor);
    drawVectorTriangle(p, m_btnNextPage.center(), 6.0, false, nextColor);
    drawVectorSticker(p, m_btnSticker, m_hoverSticker ? accentColor : toolIconColor);
    drawVectorChevron(p, m_btnSettings, m_hoverSettings ? accentColor : toolIconColor);
}

void CandidateWindow::drawVertical(QPainter& p) {
    auto& cfg = AppConfig::instance();
    bool isDark = cfg.isDarkTheme();
    QColor textPrimary = isDark ? QColor(255, 255, 255, 240) : QColor(20, 20, 20, 240);
    QColor textSecondary = isDark ? QColor(160, 160, 160, 220) : QColor(100, 100, 100, 220);
    QColor accentColor = cfg.effectiveAccentColor();
    QColor capsuleActive = isDark ? QColor(255, 255, 255, 12) : QColor(0, 0, 0, 10);
    QColor capsuleHover = isDark ? QColor(255, 255, 255, 7) : QColor(0, 0, 0, 5);
    QColor sepColor = isDark ? QColor(255, 255, 255, 16) : QColor(0, 0, 0, 14);

    QFont cnFont = createCandidateChineseFont(cfg.fontSize());
    QFont numFont = createCandidateNumberFont(cfg.fontSize());
    QFont emojiFont = createWindowsNativeEmojiFont(cfg.fontSize() + 2);
    QFontMetricsF fmCn(cnFont);
    QFontMetricsF fmNum(numFont);
    QFontMetricsF fmEmoji(emojiFont);

    // Preedit header (Only drawn when showPreedit is enabled)
    if (cfg.showPreedit() && !m_state.preedit.isEmpty()) {
        QRectF preeditR(12.0, 4.0, width() - 24.0, 22.0);
        drawCrispText(p, preeditR, cnFont, m_state.preedit, accentColor, Qt::AlignVCenter | Qt::AlignLeft);

        // Divider under preedit
        p.setPen(QPen(sepColor, 1.0));
        p.drawLine(QPointF(8.0, 28.0), QPointF(width() - 8.0, 28.0));
    }

    qreal fixedNumColW = cfg.showLabel() ? (fmNum.horizontalAdvance(QStringLiteral("8")) + 6.0) : 0.0;

    // 1. Candidate Background Capsules ("先选中")
    for (int i = 0; i < m_candidateRects.size(); ++i) {
        const auto& cr = m_candidateRects[i];
        bool isHighlight = (cr.index == m_state.highlightedIndex);
        bool isHover = (cr.index == m_hoverCandidate);

        if (isHighlight) {
            QPainterPath path;
            path.addRoundedRect(cr.rect, 4.0, 4.0);
            p.fillPath(path, capsuleActive);
        } else if (isHover) {
            QPainterPath path;
            path.addRoundedRect(cr.rect, 4.0, 4.0);
            p.fillPath(path, capsuleHover);
        }
    }

    // 2. Animated Theme-Colored Accent Indicator ("主题色的光标移动")
    const QRectF indR = currentIndicatorRect();
    if (!indR.isEmpty()) {
        QPainterPath barPath;
        barPath.addRoundedRect(indR, 1.25, 1.25);
        p.fillPath(barPath, accentColor);
    }

    for (int i = 0; i < m_candidateRects.size(); ++i) {
        const auto& cr = m_candidateRects[i];
        bool isHighlight = (cr.index == m_state.highlightedIndex);
        bool isHover = (cr.index == m_hoverCandidate);

        const auto& cand = m_state.candidates[i];
        QString numStr = QString::number(i + 1);

        // Fixed X guidelines guarantee strictly straight vertical columns
        qreal numX = cr.rect.left() + 14.0;
        qreal candX = numX + fixedNumColW;

        if (cfg.showLabel()) {
            qreal nw = fmNum.horizontalAdvance(numStr);
            QRectF labelR(numX, cr.rect.top(), nw, cr.rect.height());
            drawCrispText(p, labelR, numFont, numStr, textSecondary, Qt::AlignVCenter | Qt::AlignLeft);
        }

        if (isEmojiText(cand.text)) {
            const QPixmap& pix = EmojiPickerWindow::getEmojiPixmap(cand.text);
            if (!pix.isNull()) {
                qreal s = 20.0;
                QRectF iconR(candX, cr.rect.top() + (cr.rect.height() - s) * 0.5, s, s);
                qreal prevOp = p.opacity();
                if (isDark) {
                    p.setOpacity((isHighlight || isHover) ? 0.98 : 0.88);
                }
                p.drawPixmap(iconR, pix, pix.rect());
                if (isDark) {
                    p.setOpacity(prevOp);
                }
            } else {
                p.setFont(emojiFont);
                qreal ew = fmEmoji.horizontalAdvance(cand.text);
                QRectF textR(candX, cr.rect.top(), ew, cr.rect.height());
                p.drawText(textR, Qt::AlignVCenter | Qt::AlignLeft, cand.text);
            }
        } else {
            qreal cw = fmCn.horizontalAdvance(cand.text);
            QRectF textR(candX, cr.rect.top(), cw, cr.rect.height());
            drawCrispText(p, textR, cnFont, cand.text, textPrimary, Qt::AlignVCenter | Qt::AlignLeft);
        }

        if (!cand.comment.isEmpty()) {
            qreal comW = fmCn.horizontalAdvance(cand.comment);
            QRectF commentR(cr.rect.right() - comW - 10.0, cr.rect.top(), comW, cr.rect.height());
            drawCrispText(p, commentR, cnFont, cand.comment, textSecondary, Qt::AlignVCenter | Qt::AlignRight);
        }
    }

    // Bottom toolbar divider
    p.setPen(QPen(sepColor, 1.0));
    p.drawLine(QPointF(8.0, m_btnPrevPage.top() - 4.0), QPointF(width() - 8.0, m_btnPrevPage.top() - 4.0));

    // Button hover backgrounds
    auto drawBtnHover = [&](const QRectF& btnRect, bool isHovered) {
        if (isHovered) {
            p.setPen(Qt::NoPen);
            p.setBrush(isDark ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 14));
            p.drawRoundedRect(btnRect.adjusted(-1, -1, 1, 1), 4.0, 4.0);
        }
    };
    drawBtnHover(m_btnPrevPage, m_hoverPrev);
    drawBtnHover(m_btnNextPage, m_hoverNext);
    drawBtnHover(m_btnSticker, m_hoverSticker);
    drawBtnHover(m_btnSettings, m_hoverSettings);

    QColor prevColor = (m_state.pageNo == 0) ? (isDark ? QColor(255, 255, 255, 75) : QColor(0, 0, 0, 60))
                                           : (m_hoverPrev ? accentColor : textPrimary);
    QColor nextColor = m_hoverNext ? accentColor : (isDark ? QColor(255, 255, 255, 230) : QColor(0, 0, 0, 200));
    QColor toolIconColor = isDark ? QColor(255, 255, 255, 210) : QColor(0, 0, 0, 190);

    drawVectorTriangle(p, m_btnPrevPage.center(), 6.0, true, prevColor);
    drawVectorTriangle(p, m_btnNextPage.center(), 6.0, false, nextColor);
    drawVectorSticker(p, m_btnSticker, m_hoverSticker ? accentColor : toolIconColor);
    drawVectorGear(p, m_btnSettings, m_hoverSettings ? accentColor : toolIconColor);
}

void CandidateWindow::drawVectorTriangle(QPainter& p, const QPointF& center, qreal size, bool pointsLeft, const QColor& color) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(color, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(color);

    QPainterPath path;
    qreal half = size * 0.5;
    if (pointsLeft) {
        path.moveTo(center.x() - half * 0.85, center.y());
        path.lineTo(center.x() + half * 0.85, center.y() - half);
        path.lineTo(center.x() + half * 0.85, center.y() + half);
    } else {
        path.moveTo(center.x() + half * 0.85, center.y());
        path.lineTo(center.x() - half * 0.85, center.y() - half);
        path.lineTo(center.x() - half * 0.85, center.y() + half);
    }
    path.closeSubpath();
    p.drawPath(path);
    p.restore();
}

void CandidateWindow::drawVectorGear(QPainter& p, const QRectF& rect, const QColor& color) {
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

void CandidateWindow::drawVectorSticker(QPainter& p, const QRectF& rect, const QColor& color) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    QPointF c = rect.center();

    // 1. Outline rounded card at bottom-left (matching Windows 11 Sticker icon)
    QRectF box(c.x() - 6.5, c.y() - 3.5, 11.0, 10.5);
    p.setPen(QPen(color, 1.35, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(box, 2.5, 2.5);

    // 2. Solid filled heart placed at upper-right
    qreal hx = c.x() + 3.0;
    qreal hy = c.y() - 4.2;
    qreal s = 0.95;

    QPainterPath heart;
    heart.moveTo(hx, hy + 1.2 * s);
    heart.cubicTo(hx - 1.5 * s, hy - 1.8 * s, hx - 4.2 * s, hy - 0.2 * s, hx - 3.2 * s, hy + 2.2 * s);
    heart.cubicTo(hx - 2.4 * s, hy + 4.0 * s, hx, hy + 5.2 * s, hx, hy + 6.0 * s);
    heart.cubicTo(hx, hy + 5.2 * s, hx + 2.4 * s, hy + 4.0 * s, hx + 3.2 * s, hy + 2.2 * s);
    heart.cubicTo(hx + 4.2 * s, hy - 0.2 * s, hx + 1.5 * s, hy - 1.8 * s, hx, hy + 1.2 * s);

    // Clean background cutout mask to cleanly erase the card stroke behind the heart
    auto& cfg = AppConfig::instance();
    QColor bgMask = cfg.isDarkTheme() ? QColor(32, 32, 32) : QColor(243, 243, 243);
    p.setPen(QPen(bgMask, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(bgMask);
    p.drawPath(heart);

    // Fill heart with theme icon color
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawPath(heart);

    p.restore();
}

void CandidateWindow::drawVectorChevron(QPainter& p, const QRectF& rect, const QColor& color, bool pointsUp) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(color, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    QPointF c = rect.center();
    QPainterPath path;
    if (pointsUp) {
        path.moveTo(c.x() - 4.0, c.y() + 2.0);
        path.lineTo(c.x(), c.y() - 2.0);
        path.lineTo(c.x() + 4.0, c.y() + 2.0);
    } else {
        path.moveTo(c.x() - 4.0, c.y() - 1.5);
        path.lineTo(c.x(), c.y() + 2.5);
        path.lineTo(c.x() + 4.0, c.y() - 1.5);
    }
    p.drawPath(path);
    p.restore();
}

void CandidateWindow::toggleExpanded() {
    auto& cfg = AppConfig::instance();
    bool isVertical = (cfg.orientation() == QStringLiteral("vertical"));
    if (isVertical) return; // Vertical mode does not expand

    m_isExpanded = !m_isExpanded;
    if (m_isExpanded && m_engine && m_state.candidates.isEmpty()) {
        m_state = m_engine->getUiState();
    }
    calculateLayout();
    update();
}

void CandidateWindow::toggleEmojiPicker() {
    showCustomEmojiPicker();
}

void CandidateWindow::showCustomEmojiPicker() {
    if (!m_emojiPicker) return;
    if (m_emojiPicker->isVisible()) {
        m_emojiPicker->hide();
        return;
    }

    QPoint globalPos = mapToGlobal(QPoint(0, 0));
    int pickerW = m_emojiPicker->width();
    int pickerH = m_emojiPicker->height();

    QScreen* screen = QGuiApplication::screenAt(globalPos);
    if (!screen) screen = QGuiApplication::primaryScreen();
    QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    auto& cfg = AppConfig::instance();
    bool isVertical = (cfg.orientation() == QStringLiteral("vertical"));

    int targetX = 0;
    int targetY = 0;

    if (!isVertical) {
        targetX = globalPos.x() + static_cast<int>(m_btnSticker.center().x()) - pickerW / 2;
        targetY = globalPos.y() - pickerH - 8;
        if (targetY < avail.top() + 8) {
            targetY = globalPos.y() + height() + 8;
        }
    } else {
        targetX = globalPos.x() + width() + 8;
        if (targetX + pickerW > avail.right() - 8) {
            targetX = globalPos.x() - pickerW - 8;
        }
        targetY = globalPos.y();
        if (targetY + pickerH > avail.bottom() - 8) {
            targetY = avail.bottom() - pickerH - 8;
        }
    }

    targetX = qBound(avail.left() + 8, targetX, avail.right() - pickerW - 8);
    targetY = qBound(avail.top() + 8, targetY, avail.bottom() - pickerH - 8);

    m_emojiPicker->move(targetX, targetY);
    m_emojiPicker->show();
    m_emojiPicker->raise();
}

fluent::menus_toolbars::FluentMenu* CandidateWindow::createExtensionMenu() {
    auto* menu = new fluent::menus_toolbars::FluentMenu(QString(), this);
    menu->setWindowFlag(Qt::WindowStaysOnTopHint, true);

    auto& cfg = AppConfig::instance();
    const bool isDark = cfg.isDarkTheme();
    const QColor iconColor = isDark ? QColor(220, 220, 220) : QColor(50, 50, 50);

    // 1. 中/英文模式 (Chinese / English)
    const bool isAscii = m_engine ? m_engine->isAsciiMode() : false;
    auto* actLang = menu->addAction(isAscii ? QStringLiteral("中/英文模式 (当前: 英文)") : QStringLiteral("中/英文模式 (当前: 中文)"));
    actLang->setIcon(createFluentMenuIcon(Typography::Icons::World, iconColor));
    actLang->setCheckable(true);
    actLang->setChecked(!isAscii);
    connect(actLang, &QAction::triggered, this, [this](bool checked) {
        if (m_engine) {
            m_engine->setAsciiMode(!checked);
        }
        if (AppConfig::instance().soundEnabled()) {
            playMechanicalClickSound();
        }
    });

    // 2. 简/繁体中文 (Simplified / Traditional)
    const bool isSimp = m_engine ? m_engine->isSimplified() : true;
    auto* actSimp = menu->addAction(isSimp ? QStringLiteral("简繁切换 (当前: 简体)") : QStringLiteral("简繁切换 (当前: 繁体)"));
    actSimp->setIcon(createFluentMenuIcon(Typography::Icons::Edit, iconColor));
    actSimp->setCheckable(true);
    actSimp->setChecked(isSimp);
    connect(actSimp, &QAction::triggered, this, [this](bool checked) {
        if (m_engine) {
            m_engine->setSimplified(checked);
        }
        if (AppConfig::instance().soundEnabled()) {
            playMechanicalClickSound();
        }
    });

    // 3. 全角/半角符号 (Full / Half Shape)
    const bool isFull = m_engine ? m_engine->isFullShape() : false;
    auto* actShape = menu->addAction(isFull ? QStringLiteral("全半角符号 (当前: 全角)") : QStringLiteral("全半角符号 (当前: 半角)"));
    actShape->setIcon(createFluentMenuIcon(Typography::Icons::Font, iconColor));
    actShape->setCheckable(true);
    actShape->setChecked(isFull);
    connect(actShape, &QAction::triggered, this, [this](bool checked) {
        if (m_engine) {
            m_engine->setFullShape(checked);
        }
        if (AppConfig::instance().soundEnabled()) {
            playMechanicalClickSound();
        }
    });

    // 4. 表情与颜文字面板
    auto* actEmoji = menu->addAction(QStringLiteral("表情与符号面板..."));
    actEmoji->setIcon(createFluentMenuIcon(Typography::Icons::Emoji, iconColor));
    connect(actEmoji, &QAction::triggered, this, [this]() {
        toggleEmojiPicker();
    });

    menu->addSeparator();

    // 5. 候选排版方向 (Candidate Orientation)
    auto* subOrientation = new fluent::menus_toolbars::FluentMenu(QStringLiteral("候选排版方向"), menu);
    subOrientation->setIcon(createFluentMenuIcon(Typography::Icons::List, iconColor));
    auto* grpOrientation = new QActionGroup(subOrientation);
    grpOrientation->setExclusive(true);

    auto* actHoriz = subOrientation->addAction(QStringLiteral("水平排版 (横排)"));
    actHoriz->setCheckable(true);
    actHoriz->setChecked(cfg.orientation() != QStringLiteral("vertical"));
    grpOrientation->addAction(actHoriz);

    auto* actVert = subOrientation->addAction(QStringLiteral("竖直排版 (竖排)"));
    actVert->setCheckable(true);
    actVert->setChecked(cfg.orientation() == QStringLiteral("vertical"));
    grpOrientation->addAction(actVert);

    connect(actHoriz, &QAction::triggered, this, [this]() {
        AppConfig::instance().setOrientation(QStringLiteral("horizontal"));
        AppConfig::instance().save();
        calculateLayout();
        update();
    });
    connect(actVert, &QAction::triggered, this, [this]() {
        AppConfig::instance().setOrientation(QStringLiteral("vertical"));
        AppConfig::instance().save();
        calculateLayout();
        update();
    });
    menu->addMenu(subOrientation);

    // 6. 候选翻页快捷键 (Page Keys)
    auto* subPageKeys = new fluent::menus_toolbars::FluentMenu(QStringLiteral("候选翻页快捷键"), menu);
    subPageKeys->setIcon(createFluentMenuIcon(Typography::Icons::Keyboard, iconColor));
    auto* grpPageKeys = new QActionGroup(subPageKeys);
    grpPageKeys->setExclusive(true);

    const QString currentPk = cfg.pageKeys();
    struct PageKeyOption { QString label; QString val; };
    const QVector<PageKeyOption> pkOpts = {
        {QStringLiteral(", . (逗号 / 句号)"), QStringLiteral(", .")},
        {QStringLiteral("- = (减号 / 等号)"), QStringLiteral("- =")},
        {QStringLiteral("[ ] (方括号)"), QStringLiteral("[ ]")},
        {QStringLiteral("PageUp / PageDown"), QStringLiteral("PageUp/PageDown")}
    };
    for (const auto& opt : pkOpts) {
        auto* actPk = subPageKeys->addAction(opt.label);
        actPk->setCheckable(true);
        actPk->setChecked(currentPk == opt.val);
        grpPageKeys->addAction(actPk);
        connect(actPk, &QAction::triggered, this, [opt]() {
            AppConfig::instance().setPageKeys(opt.val);
            AppConfig::instance().save();
        });
    }
    menu->addMenu(subPageKeys);

    // 7. 单页候选词数 (Page Size)
    auto* subPageSize = new fluent::menus_toolbars::FluentMenu(QStringLiteral("单页候选词数"), menu);
    subPageSize->setIcon(createFluentMenuIcon(Typography::Icons::AllApps, iconColor));
    auto* grpPageSize = new QActionGroup(subPageSize);
    grpPageSize->setExclusive(true);
    const int currentPs = cfg.pageSize();
    for (int count : {5, 7, 9}) {
        auto* actPs = subPageSize->addAction(QStringLiteral("%1 个候选词").arg(count));
        actPs->setCheckable(true);
        actPs->setChecked(currentPs == count);
        grpPageSize->addAction(actPs);
        connect(actPs, &QAction::triggered, this, [this, count]() {
            AppConfig::instance().setPageSize(count);
            AppConfig::instance().save();
            calculateLayout();
            update();
        });
    }
    menu->addMenu(subPageSize);

    // 8. 输入方案 (Rime Schema)
    auto* subSchema = new fluent::menus_toolbars::FluentMenu(QStringLiteral("输入法方案切换"), menu);
    subSchema->setIcon(createFluentMenuIcon(Typography::Icons::Document, iconColor));
    auto* grpSchema = new QActionGroup(subSchema);
    grpSchema->setExclusive(true);
    const QString curSchema = cfg.schema();
    struct SchemaOption { QString name; QString id; };
    const QVector<SchemaOption> schemaOpts = {
        {QStringLiteral("雾凇拼音 (全拼)"), QStringLiteral("rime_ice")},
        {QStringLiteral("自然码双拼"), QStringLiteral("double_pinyin")},
        {QStringLiteral("小鹤双拼"), QStringLiteral("double_pinyin_flypy")},
        {QStringLiteral("朙月拼音"), QStringLiteral("luna_pinyin")}
    };
    for (const auto& s : schemaOpts) {
        auto* actS = subSchema->addAction(s.name);
        actS->setCheckable(true);
        actS->setChecked(curSchema == s.id);
        grpSchema->addAction(actS);
        connect(actS, &QAction::triggered, this, [this, s]() {
            AppConfig::instance().setSchema(s.id);
            AppConfig::instance().save();
            if (m_engine) {
                m_engine->selectSchema(s.id);
            }
        });
    }
    menu->addMenu(subSchema);

    menu->addSeparator();

    // 9. 重新部署 Rime 词库
    auto* actDeploy = menu->addAction(QStringLiteral("重新部署 Rime 词库"));
    actDeploy->setIcon(createFluentMenuIcon(Typography::Icons::Sync, iconColor));
    connect(actDeploy, &QAction::triggered, this, [this]() {
        if (m_engine) {
            m_engine->deploy();
        }
        if (AppConfig::instance().soundEnabled()) {
            playMechanicalClickSound();
        }
    });

    // 10. 打开用户词库与配置目录
    auto* actFolder = menu->addAction(QStringLiteral("打开用户词库与配置目录..."));
    actFolder->setIcon(createFluentMenuIcon(Typography::Icons::Folder, iconColor));
    connect(actFolder, &QAction::triggered, this, []() {
        QString userDir = QStringLiteral("C:/Users/zheng/AppData/Roaming/Rime");
        QDesktopServices::openUrl(QUrl::fromLocalFile(userDir));
    });

    // 11. 机械键盘音效反馈
    auto* actSound = menu->addAction(QStringLiteral("机械键盘音效反馈"));
    actSound->setIcon(createFluentMenuIcon(Typography::Icons::Speaker, iconColor));
    actSound->setCheckable(true);
    actSound->setChecked(cfg.soundEnabled());
    connect(actSound, &QAction::triggered, this, [](bool checked) {
        AppConfig::instance().setSoundEnabled(checked);
        AppConfig::instance().save();
        if (checked) {
            playMechanicalClickSound();
        }
    });

    menu->addSeparator();

    // 12. 输入法设置... (Open Settings Window)
    auto* actSettings = menu->addAction(QStringLiteral("输入法设置..."));
    actSettings->setIcon(createFluentMenuIcon(Typography::Icons::Settings, iconColor));
    connect(actSettings, &QAction::triggered, this, [this]() {
        emit openSettingsRequested();
    });

    return menu;
}

void CandidateWindow::showExtensionMenu() {
    auto* menu = createExtensionMenu();
    if (!menu) return;

    m_hoverSettings = false;
    update();

    QPoint anchorPos = mapToGlobal(m_btnSettings.bottomRight().toPoint());
    int menuWidth = menu->sizeHint().width();
    int targetX = anchorPos.x() - menuWidth;
    int targetY = anchorPos.y() + 4;

    QScreen* screen = QGuiApplication::screenAt(anchorPos);
    if (!screen) screen = QGuiApplication::primaryScreen();
    QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
    if (targetX < avail.left() + 8) {
        targetX = avail.left() + 8;
    }
    if (targetX + menuWidth > avail.right() - 8) {
        targetX = avail.right() - menuWidth - 8;
    }

    menu->exec(QPoint(targetX, targetY));
    menu->deleteLater();
}

void CandidateWindow::toggleExtensionPanel() {
    if (!m_extensionPanel) return;
    if (m_extensionPanel->isVisible()) {
        m_extensionPanel->hide();
        return;
    }
    showExtensionPanel();
}

void CandidateWindow::showExtensionPanel() {
    if (!m_extensionPanel) return;
    m_extensionPanel->updateHitTargets();

    QPoint globalPos = mapToGlobal(QPoint(0, 0));
    int panelW = m_extensionPanel->width();
    int panelH = m_extensionPanel->height();

    QScreen* screen = QGuiApplication::screenAt(globalPos);
    if (!screen) screen = QGuiApplication::primaryScreen();
    QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    auto& cfg = AppConfig::instance();
    bool isVertical = (cfg.orientation() == QStringLiteral("vertical"));

    int targetX = 0;
    int targetY = 0;

    if (!isVertical) {
        targetX = globalPos.x() + width() - panelW;
        targetY = globalPos.y() + height() + 6;
        if (targetY + panelH > avail.bottom() - 8) {
            targetY = globalPos.y() - panelH - 6;
        }
    } else {
        targetX = globalPos.x() + width() + 6;
        targetY = globalPos.y();
        if (targetX + panelW > avail.right() - 8) {
            targetX = globalPos.x() - panelW - 6;
        }
        if (targetY + panelH > avail.bottom() - 8) {
            targetY = avail.bottom() - panelH - 8;
        }
    }

    targetX = qBound(avail.left() + 8, targetX, avail.right() - panelW - 8);
    targetY = qBound(avail.top() + 8, targetY, avail.bottom() - panelH - 8);

    m_extensionPanel->move(targetX, targetY);
    m_extensionPanel->show();
    m_extensionPanel->raise();
}

void CandidateWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (AppConfig::instance().soundEnabled()) {
            playMechanicalClickSound();
        }
        QPointF pos = event->position();

        // 1. Collapse button in expanded mode
        if (m_isExpanded && m_btnCollapse.contains(pos)) {
            toggleExpanded();
            return;
        }

        // 2. Settings button / Expand button
        if (m_btnSettings.contains(pos)) {
            bool isVertical = (AppConfig::instance().orientation() == QStringLiteral("vertical"));
            if (!isVertical && !m_isExpanded) {
                toggleExpanded();
                return;
            }
            emit openSettingsRequested();
            return;
        }

        // 3. Sticker button
        if (!m_isExpanded && m_btnSticker.contains(pos)) {
            toggleEmojiPicker();
            return;
        }

        // 4. Page buttons
        if (m_btnPrevPage.contains(pos)) {
            if (m_engine) m_engine->processKey(RIME_KEY_PAGE_UP);
            return;
        }
        if (m_btnNextPage.contains(pos)) {
            if (m_engine) m_engine->processKey(RIME_KEY_PAGE_DOWN);
            return;
        }

        // 5. Candidate selection
        for (const auto& cr : m_candidateRects) {
            if (cr.rect.contains(pos)) {
                if (m_engine) {
                    m_engine->selectCandidate(cr.index);
                }
                emit candidateSelected(cr.index);
                if (m_isExpanded) {
                    m_isExpanded = false;
                    calculateLayout();
                    update();
                }
                return;
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void CandidateWindow::mouseMoveEvent(QMouseEvent* event) {
    QPointF pos = event->position();
    int oldHover = m_hoverCandidate;
    m_hoverCandidate = -1;
    for (const auto& cr : m_candidateRects) {
        if (cr.rect.contains(pos)) {
            m_hoverCandidate = cr.index;
            break;
        }
    }

    bool oldPrev = m_hoverPrev;
    bool oldNext = m_hoverNext;
    bool oldSticker = m_hoverSticker;
    bool oldSettings = m_hoverSettings;
    bool oldCollapse = m_hoverCollapse;

    m_hoverPrev = m_btnPrevPage.contains(pos);
    m_hoverNext = m_btnNextPage.contains(pos);
    m_hoverSticker = !m_isExpanded && m_btnSticker.contains(pos);
    m_hoverSettings = m_btnSettings.contains(pos);
    m_hoverCollapse = m_isExpanded && m_btnCollapse.contains(pos);

    if (oldHover != m_hoverCandidate || oldPrev != m_hoverPrev ||
        oldNext != m_hoverNext || oldSticker != m_hoverSticker || oldSettings != m_hoverSettings ||
        oldCollapse != m_hoverCollapse) {
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void CandidateWindow::leaveEvent(QEvent* event) {
    m_hoverCandidate = -1;
    m_hoverPrev = false;
    m_hoverNext = false;
    m_hoverSticker = false;
    m_hoverSettings = false;
    m_hoverCollapse = false;
    update();
    QWidget::leaveEvent(event);
}

void CandidateWindow::keyPressEvent(QKeyEvent* event) {
    auto& cfg = AppConfig::instance();
    if (cfg.soundEnabled()) {
        playMechanicalClickSound();
    }

    if (!m_engine) return;

    if (event->key() == Qt::Key_Tab) {
        toggleExpanded();
        return;
    }

    if (m_isExpanded && event->key() == Qt::Key_Escape) {
        m_isExpanded = false;
        calculateLayout();
        update();
        return;
    }

    if (event->key() == Qt::Key_Up) {
        m_engine->processKey(RIME_KEY_UP);
    } else if (event->key() == Qt::Key_Down) {
        m_engine->processKey(RIME_KEY_DOWN);
    } else if (event->key() == Qt::Key_PageUp) {
        m_engine->processKey(RIME_KEY_PAGE_UP);
    } else if (event->key() == Qt::Key_PageDown) {
        m_engine->processKey(RIME_KEY_PAGE_DOWN);
    } else if (event->key() == Qt::Key_Space) {
        m_engine->processKey(RIME_KEY_SPACE);
        if (m_isExpanded) {
            m_isExpanded = false;
            calculateLayout();
            update();
        }
    } else if (event->key() == Qt::Key_Escape) {
        m_engine->processKey(RIME_KEY_ESCAPE);
    } else if (event->key() == Qt::Key_Backspace) {
        m_engine->processKey(RIME_KEY_BACKSPACE);
    } else if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_9) {
        int idx = event->key() - Qt::Key_1;
        m_engine->selectCandidate(idx);
        if (m_isExpanded) {
            m_isExpanded = false;
            calculateLayout();
            update();
        }
    } else if (m_isExpanded && event->key() == Qt::Key_0) {
        m_engine->selectCandidate(9);
        m_isExpanded = false;
        calculateLayout();
        update();
    } else if (m_isExpanded && event->key() >= Qt::Key_A && event->key() <= Qt::Key_J) {
        int idx = 10 + (event->key() - Qt::Key_A);
        m_engine->selectCandidate(idx);
        m_isExpanded = false;
        calculateLayout();
        update();
    } else {
        // Page keys configuration
        QString pk = cfg.pageKeys();
        if (pk == QStringLiteral("comma_period")) {
            if (event->key() == Qt::Key_Comma) {
                m_engine->processKey(RIME_KEY_PAGE_UP);
                return;
            } else if (event->key() == Qt::Key_Period) {
                m_engine->processKey(RIME_KEY_PAGE_DOWN);
                return;
            }
        } else if (pk == QStringLiteral("minus_equal")) {
            if (event->key() == Qt::Key_Minus) {
                m_engine->processKey(RIME_KEY_PAGE_UP);
                return;
            } else if (event->key() == Qt::Key_Equal) {
                m_engine->processKey(RIME_KEY_PAGE_DOWN);
                return;
            }
        } else if (pk == QStringLiteral("bracket")) {
            if (event->key() == Qt::Key_BracketLeft) {
                m_engine->processKey(RIME_KEY_PAGE_UP);
                return;
            } else if (event->key() == Qt::Key_BracketRight) {
                m_engine->processKey(RIME_KEY_PAGE_DOWN);
                return;
            }
        }

        QString text = event->text();
        if (!text.isEmpty()) {
            m_engine->processChar(text.at(0));
        }
    }
}
