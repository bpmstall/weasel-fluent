#include "settings_window.h"

#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFontMetricsF>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QParallelAnimationGroup>
#include <QScrollBar>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

#include "components/foundation/UserTheme.h"
#include "components/windowing/WindowBackdropMaterial.h"
#include "components/windowing/WindowBackdrop.h"
#include "components/windowing/TitleBar.h"
#include "design/IconCatalog.h"

namespace {

constexpr qreal kFixedCornerRadius = 6.0;

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

    // Uniform WinUI 3 / Windows 11 Mica base color (#202020 for Dark, #F3F3F3 for Light)
    QColor base = isDark ? QColor(32, 32, 32) : QColor(243, 243, 243);
    p.fillPath(path, base);

    // Authentic Win11 tactile micro-grain noise tile (Uniform, zero gradient)
    static QPixmap grain = createMicaGrainTile(dpr);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.setOpacity(isDark ? 0.022 : 0.015);
    p.drawTiledPixmap(r, grain, QPointF(0, 0));

    p.restore();
}

inline void revealWindowMaterial(QWidget* widget) {
    if (!widget) return;
    widget->setAutoFillBackground(false);
    widget->setAttribute(Qt::WA_NoSystemBackground, true);
    widget->setStyleSheet(QStringLiteral("background: transparent;"));
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

enum class IndicatorMotionDirection {
    None,
    Up,
    Down
};

class SettingsScrollView final : public fluent::scrolling::ScrollView {
public:
    explicit SettingsScrollView(QWidget* parent = nullptr) : fluent::scrolling::ScrollView(parent) {
        setFrameShape(QFrame::NoFrame);
        applyTransparentSurface();
    }

    void onThemeUpdated() override {
        fluent::scrolling::ScrollView::onThemeUpdated();
        applyTransparentSurface();
    }

    void applyTransparentSurface() {
        setAutoFillBackground(false);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));
        if (viewport()) {
            viewport()->setAutoFillBackground(false);
            viewport()->setAttribute(Qt::WA_NoSystemBackground, true);
            viewport()->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        }
    }

protected:
    void showEvent(QShowEvent* event) override {
        fluent::scrolling::ScrollView::showEvent(event);
        applyTransparentSurface();
    }
};

class SettingsTitleBarContent final : public QWidget {
public:
    explicit SettingsTitleBarContent(QWidget* parent = nullptr) : QWidget(parent) {
        revealWindowMaterial(this);
    }

    void setControls(QWidget* backBtn, QWidget* titleLbl, QWidget* searchBox) {
        m_backBtn = backBtn;
        m_titleLbl = titleLbl;
        m_searchBox = searchBox;
        relayout();
    }

    void relayout() {
        if (!parentWidget()) return;
        int barW = parentWidget()->width();
        int contentX = x();
        int h = height();
        if (h <= 0) h = 48;

        // 1. Back button shifted far left to x = 0 (which is x=8 in the window)
        if (m_backBtn) {
            m_backBtn->setGeometry(0, (h - 32) / 2, 32, 32);
        }

        // 2. Title "设置" next to back button
        if (m_titleLbl) {
            m_titleLbl->setGeometry(40, (h - 32) / 2, 40, 32);
        }

        // 3. Search box: mathematically centered in the entire window (barW)
        if (m_searchBox) {
            int sw = 320;
            int targetX = (barW - sw) / 2 - contentX;
            m_searchBox->setGeometry(targetX, (h - 32) / 2, sw, 32);
        }

        if (auto* bar = qobject_cast<fluent::windowing::TitleBar*>(parentWidget())) {
            bar->refreshChromeExclusions();
        }
    }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        relayout();
    }

private:
    QWidget* m_backBtn = nullptr;
    QWidget* m_titleLbl = nullptr;
    QWidget* m_searchBox = nullptr;
};

} // namespace

// ============================================================================
// SettingsCard Implementation (WinUI 3 Gallery Card)
// ============================================================================

SettingsCard::SettingsCard(const QString& iconGlyph,
                           const QString& title,
                           const QString& subtitle,
                           QWidget* trailingWidget,
                           const QColor& iconColor,
                           QWidget* parent)
    : fluent::layout::Card(parent), m_trailing(trailingWidget) {
    setObjectName(QStringLiteral("settingsCard"));
    setMinimumHeight(68);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAppearance(fluent::layout::Card::Layer);
    setBorderVisible(true);

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(20, 12, 20, 12);
    m_layout->setSpacing(16);

    // Fluent Font Icon (30x30 standard slot)
    m_icon = new fluent::FontIcon(iconGlyph, this);
    m_icon->setIconSize(Typography::IconSize::Standard);
    m_icon->setFixedSize(30, 30);
    if (iconColor.isValid()) {
        m_icon->setStyleSheet(QStringLiteral("color: %1;").arg(iconColor.name()));
    }

    // Text Column
    auto* textCol = new QWidget(this);
    revealWindowMaterial(textCol);
    auto* textLayout = new QVBoxLayout(textCol);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);

    m_titleLabel = new fluent::textfields::Label(title, textCol);
    m_titleLabel->setFluentTypography(Typography::FontRole::Body);
    m_titleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);

    m_subLabel = new fluent::textfields::Label(subtitle, textCol);
    m_subLabel->setFluentTypography(Typography::FontRole::Caption);
    m_subLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
    m_subLabel->setWordWrap(true);

    textLayout->addWidget(m_titleLabel);
    textLayout->addWidget(m_subLabel);

    m_layout->addWidget(m_icon, 0, Qt::AlignVCenter);
    m_layout->addWidget(textCol, 1, Qt::AlignVCenter);

    if (m_trailing) {
        m_layout->addWidget(m_trailing, 0, Qt::AlignRight | Qt::AlignVCenter);
    }
}

void SettingsCard::setTrailingWidget(QWidget* trailingWidget) {
    if (m_trailing) {
        m_layout->removeWidget(m_trailing);
        m_trailing->deleteLater();
    }
    m_trailing = trailingWidget;
    if (m_trailing) {
        m_layout->addWidget(m_trailing, 0, Qt::AlignRight | Qt::AlignVCenter);
    }
}

QString SettingsCard::title() const {
    return m_titleLabel ? m_titleLabel->text() : QString();
}

QString SettingsCard::subtitle() const {
    return m_subLabel ? m_subLabel->text() : QString();
}

void SettingsCard::setMatchesSearch(bool matches) {
    setVisible(matches);
}

// ============================================================================
// SettingsNavDelegate (WinUI 3 Navigation Delegate aligned with Fluent-Qt)
// ============================================================================

class SettingsNavDelegate : public QStyledItemDelegate {
public:
    explicit SettingsNavDelegate(fluent::FluentElement* themeHost, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), m_themeHost(themeHost) {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(index);
        int w = option.widget ? option.widget->width() : 200;
        return QSize(w, 40);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (!index.isValid() || !m_themeHost)
            return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);

        const auto& colors = m_themeHost->themeColorsRef();
        const auto radius = m_themeHost->themeRadius();

        // WinUI 3 row background rectangle with standard margins
        const QRectF backgroundRect(
            option.rect.left() + 4.0,
            option.rect.top() + 2.0,
            option.rect.width() - 8.0,
            option.rect.height() - 4.0
        );

        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const bool pressed = (option.state & QStyle::State_Sunken) && hovered;

        // "先选中": immediate background plate feedback
        QColor background = Qt::transparent;
        if (pressed) {
            background = colors.subtleTertiary;
        } else if (selected || hovered) {
            background = colors.subtleSecondary;
        }

        if (background.alpha() > 0) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(background);
            painter->drawRoundedRect(backgroundRect, radius.control, radius.control);
        }

        // Icon (Typography::Icons::paintGlyph handles optical size, grid snapping, and restores font)
        const QString iconGlyph = index.data(Qt::UserRole + 2).toString();
        const qreal contentLeft = backgroundRect.left() + 12.0;
        const qreal iconWidth = 20.0;
        if (!iconGlyph.isEmpty()) {
            painter->setPen(selected ? colors.textPrimary : (hovered ? colors.textPrimary : colors.textSecondary));
            const QRectF iconRect(contentLeft, backgroundRect.top(), iconWidth, backgroundRect.height());
            Typography::Icons::paintGlyph(*painter, iconRect, iconGlyph, Typography::IconSize::Standard, Qt::AlignCenter);
        }

        // Title label ("先选中": semi-bold on selected, zero latency)
        QFont textFont = m_themeHost->themeFont(Typography::FontRole::Body).toQFont();
        textFont.setBold(selected);
        textFont.setWeight(selected ? QFont::DemiBold : QFont::Normal);
        painter->setFont(textFont);
        painter->setPen(selected ? colors.textPrimary : (hovered ? colors.textPrimary : colors.textSecondary));

        const qreal textX = iconGlyph.isEmpty() ? (backgroundRect.left() + 14.0) : (contentLeft + iconWidth + 12.0);
        const QRectF textRect(textX, backgroundRect.top(), backgroundRect.right() - textX - 8.0, backgroundRect.height());

        const QString text = index.data(Qt::DisplayRole).toString();
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text);

        painter->restore();
    }

private:
    fluent::FluentElement* m_themeHost = nullptr;
};

// ============================================================================
// SettingsNavPane Implementation (Aligned with GalleryNavigationPane.cpp)
// ============================================================================

SettingsNavPane::SettingsNavPane(fluent::FluentElement* themeHost, QWidget* parent)
    : QWidget(parent), m_themeHost(themeHost)
{
    setObjectName(QStringLiteral("settingsNavPane"));
    setFixedWidth(210);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_NoSystemBackground, true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 12, 8, 16);
    layout->setSpacing(0);

    m_treeView = new fluent::collections::TreeView(this);
    m_treeView->setObjectName(QStringLiteral("settingsNavTreeView"));
    m_treeView->setBorderVisible(false);
    m_treeView->setBackgroundVisible(false);
    m_treeView->setProperty("fluentPreserveParentSurface", true);
    if (m_treeView->viewport()) {
        m_treeView->viewport()->setProperty("fluentPreserveParentSurface", true);
        m_treeView->viewport()->setAttribute(Qt::WA_NoSystemBackground, true);
    }
    m_treeView->setHorizontalFluentScrollBarEnabled(false);
    m_treeView->setOverscrollEnabled(false);
    m_treeView->setIndentation(0);
    m_treeView->setIndicatorMotionAnimationEnabled(true);
    m_treeView->setSelectionIndicatorVisible(true);
    m_treeView->setFocusPolicy(Qt::NoFocus);

    fluent::collections::TreeView::SelectionIndicatorStyle indicatorStyle;
    indicatorStyle.inset = 7.0; // 4.0 margin + 3.0 pill left margin
    indicatorStyle.width = 3.0;
    indicatorStyle.height = 16.0;
    indicatorStyle.insetRole = -1;
    m_treeView->setSelectionIndicatorStyle(indicatorStyle);
    m_treeView->setSelectionMode(fluent::collections::TreeView::SelectionMode::Single);

    m_model = new QStandardItemModel(m_treeView);

    struct NavEntry {
        QString title;
        QString iconGlyph;
    };
    const NavEntry entries[] = {
        {QStringLiteral("个性化与排版"), Typography::Icons::Brush},
        {QStringLiteral("输入与按键"), Typography::Icons::Keyboard},
        {QStringLiteral("词库与维护"), Typography::Icons::Folder},
        {QStringLiteral("关于与系统"), Typography::Icons::Info}
    };

    for (const auto& entry : entries) {
        auto* item = new QStandardItem(entry.title);
        item->setData(entry.iconGlyph, Qt::UserRole + 2);
        item->setEditable(false);
        m_model->appendRow(item);
    }

    m_treeView->setModel(m_model);
    m_treeView->setItemDelegate(new SettingsNavDelegate(m_themeHost, m_treeView));

    // Connect itemPressed for instant selection on mouse press ("先选中")
    connect(m_treeView, &fluent::collections::TreeView::itemPressed, this, [this](const QModelIndex& index) {
        if (index.isValid()) {
            emit rowActivated(index.row());
        }
    });

    // Also connect activated for keyboard Return/Enter
    connect(m_treeView, &QAbstractItemView::activated, this, [this](const QModelIndex& index) {
        if (index.isValid()) {
            emit rowActivated(index.row());
        }
    });

    layout->addWidget(m_treeView);
}

void SettingsNavPane::onThemeUpdated() {
    if (m_treeView && m_treeView->viewport()) {
        m_treeView->viewport()->update();
    }
    update();
}

int SettingsNavPane::selectedRow() const {
    if (!m_treeView) return -1;
    QModelIndex idx = m_treeView->currentIndex();
    return idx.isValid() ? idx.row() : -1;
}

void SettingsNavPane::setSelectedRow(int row) {
    if (!m_model || row < 0 || row >= m_model->rowCount())
        return;
    QModelIndex idx = m_model->index(row, 0);
    if (!idx.isValid())
        return;

    if (m_treeView->currentIndex() == idx &&
        m_treeView->selectionModel() &&
        m_treeView->selectionModel()->isSelected(idx))
    {
        return;
    }

    m_treeView->doItemsLayout();
    m_treeView->selectionModel()->setCurrentIndex(
        idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_treeView->setCurrentIndex(idx);
    m_treeView->viewport()->update();
}

QRectF SettingsNavPane::currentIndicatorRect(qreal progress) const {
    if (!m_treeView || !m_model) return {};
    const_cast<fluent::collections::TreeView*>(m_treeView)->doItemsLayout();
    QRectF r = (progress >= 0.0) ? m_treeView->selectedIndicatorRect(progress)
                                 : m_treeView->selectedIndicatorRect();
    if (!r.isEmpty()) {
        QPoint p = m_treeView->viewport()->mapTo(this, QPoint(qRound(r.left()), qRound(r.top())));
        return QRectF(QPointF(p.x(), p.y()), r.size());
    }

    // High fidelity fallback matching FluentQt leading/trailing fluid dynamic formula
    int curRow = selectedRow();
    if (curRow < 0) curRow = 0;
    qreal itemH = 40.0;
    qreal indicatorH = 16.0;
    qreal indicatorW = 3.0;
    qreal insetX = 15.0; // 8px left margin + 7px inset

    qreal targetTop = 12.0 + curRow * itemH + (itemH - indicatorH) / 2.0;
    qreal targetBottom = targetTop + indicatorH;

    if (progress < 0.0 || qFuzzyCompare(progress + 1.0, 2.0)) {
        return QRectF(insetX, targetTop, indicatorW, indicatorH);
    }

    int prevRow = (curRow == 1) ? 0 : (curRow == 0 ? 1 : curRow - 1);
    qreal prevTop = 12.0 + prevRow * itemH + (itemH - indicatorH) / 2.0;
    qreal prevBottom = prevTop + indicatorH;

    if (qFuzzyCompare(progress + 1.0, 1.0)) {
        return QRectF(insetX, prevTop, indicatorW, indicatorH);
    }

    qreal clamped = qBound(0.0, progress, 1.0);
    qreal leading = qBound(0.0, clamped * 1.35, 1.0);
    qreal trailing = qBound(0.0, (clamped - 0.18) / 0.82, 1.0);

    qreal top = (curRow >= prevRow) ? lerp(prevTop, targetTop, trailing)
                                    : lerp(prevTop, targetTop, leading);
    qreal bottom = (curRow >= prevRow) ? lerp(prevBottom, targetBottom, leading)
                                       : lerp(prevBottom, targetBottom, trailing);

    return QRectF(insetX, qMin(top, bottom), indicatorW, qAbs(bottom - top));
}

void SettingsNavPane::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const auto colors = themeColors();
    p.setPen(QPen(colors.strokeDivider, 1.0));
    p.drawLine(width() - 1, 0, width() - 1, height());
}

// ============================================================================
// HeroPreviewCard Implementation (Live IME Simulator & Quick Themes)
// ============================================================================

HeroPreviewCard::HeroPreviewCard(QWidget* parent) : fluent::layout::Card(parent) {
    setObjectName(QStringLiteral("heroPreviewCard"));
    setFixedHeight(195);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAppearance(fluent::layout::Card::Layer);
    setBorderVisible(true);
    setMouseTracking(true);
}

void HeroPreviewCard::refresh() {
    update();
}

void HeroPreviewCard::paintEvent(QPaintEvent* event) {
    fluent::layout::Card::paintEvent(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    auto& cfg = AppConfig::instance();
    bool isDark = cfg.isDarkTheme();
    QColor accent = cfg.effectiveAccentColor();

    qreal fullW = width();
    qreal rightW = 250.0;
    qreal leftW = fullW - rightW - 32.0;

    // ------------------------------------------------------------------------
    // Left: Wallpaper Backdrop + Live 1:1 Candidate Window Simulator
    // ------------------------------------------------------------------------
    QRectF canvasRect(16.0, 14.0, leftW, 167.0);
    QPainterPath canvasPath;
    canvasPath.addRoundedRect(canvasRect, 8.0, 8.0);

    // Simulated Win11 desktop wallpaper backdrop
    QLinearGradient bg(canvasRect.topLeft(), canvasRect.bottomRight());
    bg.setColorAt(0.0, isDark ? QColor(26, 32, 44) : QColor(228, 235, 245));
    bg.setColorAt(1.0, isDark ? QColor(16, 20, 28) : QColor(210, 220, 235));
    p.fillPath(canvasPath, bg);

    p.setPen(QPen(isDark ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 18), 1.0));
    p.drawPath(canvasPath);

    // Maintain live CandidateWindow simulator
    if (!m_candSimulator) {
        m_candSimulator = new CandidateWindow(nullptr, this);
        m_candSimulator->hide();
    }

    // Build authentic RimeUiState based on current configuration
    RimeUiState state;
    state.isComposing = true;
    state.preedit = QStringLiteral("nihao");
    state.highlightedIndex = 0;
    state.pageNo = 0;

    const struct { QString text; QString comment; } kSampleCands[] = {
        {QStringLiteral("你好"), QString()},
        {QStringLiteral("你"), QString()},
        {QStringLiteral("拟好"), QStringLiteral("nǐ hǎo")},
        {QStringLiteral("呢"), QString()},
        {QStringLiteral("尼"), QString()},
        {QStringLiteral("泥"), QString()},
        {QStringLiteral("倪"), QString()},
        {QStringLiteral("拟"), QString()},
        {QStringLiteral("逆"), QString()}
    };

    int count = qMax(1, qMin(9, cfg.pageSize()));
    for (int i = 0; i < count; ++i) {
        CandidateItem c;
        c.index = i;
        c.text = kSampleCands[i].text;
        c.comment = kSampleCands[i].comment;
        state.candidates.append(c);
    }

    m_candSimulator->setPreviewState(state);
    qreal dpr = devicePixelRatioF();
    QImage candImg = m_candSimulator->renderPreviewImage(dpr);

    if (!candImg.isNull()) {
        qreal candW = candImg.width() / candImg.devicePixelRatio();
        qreal candH = candImg.height() / candImg.devicePixelRatio();

        // If vertical layout or large size, scale down proportionally to fit beautifully in preview canvas
        qreal maxW = canvasRect.width() - 32.0;
        qreal maxH = canvasRect.height() - 44.0;
        qreal scale = 1.0;
        if (candW > maxW || candH > maxH) {
            scale = qMin(maxW / candW, maxH / candH);
        }

        qreal drawW = candW * scale;
        qreal drawH = candH * scale;
        qreal candX = canvasRect.left() + (canvasRect.width() - drawW) * 0.5;
        qreal candY = canvasRect.top() + (canvasRect.height() - 28.0 - drawH) * 0.5;
        QRectF targetRect(candX, candY, drawW, drawH);

        // Windows 11 natural drop shadow
        QPainterPath shadowPath;
        shadowPath.addRoundedRect(targetRect.translated(0, 3.0), 6.0 * scale, 6.0 * scale);
        p.fillPath(shadowPath, QColor(0, 0, 0, isDark ? 65 : 28));

        // Draw the exact 1:1 CandidateWindow image
        p.drawImage(targetRect, candImg);
    }

    // Subtle Badge at the bottom of canvas
    QRectF badgeRect(canvasRect.left() + 14.0, canvasRect.bottom() - 26.0, 290.0, 20.0);
    QPainterPath badgeP;
    badgeP.addRoundedRect(badgeRect, 4.0, 4.0);
    p.fillPath(badgeP, isDark ? QColor(0, 0, 0, 80) : QColor(255, 255, 255, 140));

    p.setPen(isDark ? QColor(180, 180, 180) : QColor(80, 80, 80));
    QFont bf = p.font();
    bf.setPixelSize(10);
    p.setFont(bf);
    p.drawText(badgeRect, Qt::AlignCenter, QStringLiteral("Windows 11 Mica 材质 · 雾凇拼音 (rime-ice) · 全拼"));

    // ------------------------------------------------------------------------
    // Right: "选择要应用的主题" Quick Preset Tiles (Win11 Settings Style)
    // ------------------------------------------------------------------------
    qreal rx = fullW - rightW - 12.0;
    p.setPen(isDark ? Qt::white : QColor(20, 20, 20));
    QFont hf = themeFont(Typography::FontRole::Body).toQFont();
    hf.setBold(false);
    hf.setWeight(QFont::Normal);
    p.setFont(hf);
    p.drawText(QRectF(rx, 14.0, rightW, 20.0), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("选择要应用的主题方案"));

    struct PresetInfo {
        QString name;
        QColor bg;
        QColor acc;
        bool isLight;
    };
    const PresetInfo presets[] = {
        {QStringLiteral("Windows 默认"), QColor(32, 32, 32), QColor(0, 120, 212), false},
        {QStringLiteral("曜黑极简"), QColor(18, 18, 18), QColor(0, 183, 195), false},
        {QStringLiteral("罗兰雅致"), QColor(30, 26, 38), QColor(135, 100, 184), false},
        {QStringLiteral("晨曦浅色"), QColor(243, 243, 243), QColor(16, 124, 65), true}
    };

    qreal tileW = (rightW - 10.0) * 0.5;
    qreal tileH = 54.0;
    qreal startY = 42.0;

    for (int i = 0; i < 4; ++i) {
        int row = i / 2;
        int col = i % 2;
        QRectF tr(rx + col * (tileW + 8.0), startY + row * (tileH + 8.0), tileW, tileH);
        m_presetRects[i] = tr;

        const auto& pr = presets[i];
        QPainterPath tp;
        tp.addRoundedRect(tr, 6.0, 6.0);
        p.fillPath(tp, pr.bg);

        // Accent indicator stripe
        QRectF stripe(tr.left() + 6.0, tr.bottom() - 10.0, tr.width() - 12.0, 3.0);
        QPainterPath sp;
        sp.addRoundedRect(stripe, 1.5, 1.5);
        p.fillPath(sp, pr.acc);

        // Tile Title
        p.setPen(pr.isLight ? QColor(20, 20, 20) : Qt::white);
        QFont tf = p.font();
        tf.setFamilies({QStringLiteral("Segoe UI Variable Text"), QStringLiteral("Segoe UI"), QStringLiteral("Microsoft YaHei UI")});
        tf.setPixelSize(11);
        tf.setBold(false);
        tf.setWeight(QFont::Normal);
        p.setFont(tf);
        p.drawText(QRectF(tr.left() + 6.0, tr.top() + 6.0, tr.width() - 12.0, 24.0),
                   Qt::AlignLeft | Qt::AlignTop, pr.name);

        // Border
        bool isHover = (m_hoveredPreset == i);
        QColor bColor = isHover ? accent : (isDark ? QColor(255, 255, 255, 24) : QColor(0, 0, 0, 24));
        p.setPen(QPen(bColor, isHover ? 1.5 : 1.0));
        p.drawPath(tp);
    }
}

void HeroPreviewCard::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QPointF pos = event->position();
        for (int i = 0; i < 4; ++i) {
            if (m_presetRects[i].contains(pos)) {
                emit presetRequested(i);
                return;
            }
        }
    }
    fluent::layout::Card::mousePressEvent(event);
}

void HeroPreviewCard::mouseMoveEvent(QMouseEvent* event) {
    QPointF pos = event->position();
    int oldH = m_hoveredPreset;
    m_hoveredPreset = -1;
    for (int i = 0; i < 4; ++i) {
        if (m_presetRects[i].contains(pos)) {
            m_hoveredPreset = i;
            break;
        }
    }
    if (oldH != m_hoveredPreset) {
        update();
    }
    fluent::layout::Card::mouseMoveEvent(event);
}

void HeroPreviewCard::leaveEvent(QEvent* event) {
    m_hoveredPreset = -1;
    update();
    fluent::layout::Card::leaveEvent(event);
}

// ============================================================================
// SettingsWindow Implementation (Windows 11 Settings Center Architecture)
// ============================================================================

SettingsWindow::SettingsWindow(RimeEngine* engine, QWidget* parent)
    : fluent::windowing::Window(parent), m_engine(engine) {
    setWindowTitle(QStringLiteral("设置"));
    resize(1000, 720);
    setMinimumSize(850, 560);

    setupUi();
}

void SettingsWindow::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    auto& cfg = AppConfig::instance();
    bool isDark = cfg.isDarkTheme();

    QRectF r(rect());
    QPainterPath bgPath;
    bgPath.addRect(r);

    if (cfg.backdrop() == QStringLiteral("solid")) {
        p.fillRect(rect(), isDark ? QColor(32, 32, 32) : QColor(248, 248, 248));
    } else if (cfg.backdrop() == QStringLiteral("acrylic")) {
        auto options = fluent::windowing::WindowBackdropMaterialOptions::forTheme(
            isDark,
            isDark ? QColor(28, 28, 28) : QColor(248, 248, 248),
            cfg.effectiveAccentColor());
        options.effect = fluent::windowing::BackdropEffect::Acrylic;
        fluent::windowing::WindowBackdropMaterial::paintAcrylic(p, r, options);
    } else {
        paintCleanMica(p, bgPath, r, isDark, devicePixelRatioF());
    }

    // High contrast subtle window border
    QColor borderColor = isDark ? QColor(255, 255, 255, 24) : QColor(0, 0, 0, 24);
    p.setPen(QPen(borderColor, 1.0));
    p.drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
}

bool SettingsWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == titleBar() && event->type() == QEvent::Paint) {
        QPainter p(titleBar());
        p.setRenderHint(QPainter::Antialiasing, true);
        auto& cfg = AppConfig::instance();
        bool isDark = cfg.isDarkTheme();
        QRectF r(titleBar()->rect());
        QPainterPath path;
        path.addRect(r);

        if (cfg.backdrop() == QStringLiteral("solid")) {
            p.fillRect(titleBar()->rect(), isDark ? QColor(32, 32, 32) : QColor(248, 248, 248));
        } else if (cfg.backdrop() == QStringLiteral("acrylic")) {
            auto options = fluent::windowing::WindowBackdropMaterialOptions::forTheme(
                isDark,
                isDark ? QColor(28, 28, 28) : QColor(248, 248, 248),
                cfg.effectiveAccentColor());
            options.effect = fluent::windowing::BackdropEffect::Acrylic;
            fluent::windowing::WindowBackdropMaterial::paintAcrylic(p, r, options);
        } else {
            paintCleanMica(p, path, r, isDark, devicePixelRatioF());
        }
        return true;
    }
    return fluent::windowing::Window::eventFilter(watched, event);
}

void SettingsWindow::applyThemeAndBackdrop() {
    auto& cfg = AppConfig::instance();

    bool isDark = cfg.isDarkTheme();
    fluent::FluentElement::setTheme(isDark ? fluent::FluentElement::Dark : fluent::FluentElement::Light);
    fluent::UserTheme::applyAccentOverride(cfg.effectiveAccentColor());

    if (cfg.backdrop() == QStringLiteral("acrylic")) {
        setBackdropEffect(fluent::windowing::BackdropEffect::Acrylic);
    } else if (cfg.backdrop() == QStringLiteral("solid")) {
        setBackdropEffect(fluent::windowing::BackdropEffect::Solid);
    } else {
        setBackdropEffect(fluent::windowing::BackdropEffect::Mica);
    }

    // Publish PaintedOpaque backdrop state so TitleBar respects continuous window mica paint
    // and does NOT clear with CompositionMode_Clear to transparent.
    fluent::windowing::BackdropState bState;
    bState.requestedEffect = fluent::windowing::BackdropEffect::Mica;
    bState.effectiveEffect = fluent::windowing::BackdropEffect::Mica;
    bState.backend = fluent::windowing::BackdropBackend::PaintedMaterial;
    bState.fidelity = fluent::windowing::BackdropFidelity::Emulated;
    bState.surfaceMode = fluent::windowing::BackdropSurfaceMode::PaintedOpaque;
    fluent::windowing::publishWindowBackdropState(this, bState);

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd) {
        BOOL dark = isDark ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
        int backdropType = 2; // DWMSBT_MAINWINDOW (Mica)
        DwmSetWindowAttribute(hwnd, 38 /*DWMWA_SYSTEMBACKDROP_TYPE*/, &backdropType, sizeof(backdropType));
    }
#endif

    if (m_searchBox) {
        if (isDark) {
            m_searchBox->setStyleSheet(QStringLiteral(
                "QLineEdit { background: rgba(255, 255, 255, 0.07); border: 1px solid rgba(255, 255, 255, 0.08); border-radius: 6px; color: #FFFFFF; font-size: 13px; }"
                "QLineEdit:hover { background: rgba(255, 255, 255, 0.09); border: 1px solid rgba(255, 255, 255, 0.12); }"
                "QLineEdit:focus { background: rgba(255, 255, 255, 0.12); border: 1px solid rgba(255, 255, 255, 0.20); color: #FFFFFF; }"
            ));
        } else {
            m_searchBox->setStyleSheet(QStringLiteral(
                "QLineEdit { background: rgba(0, 0, 0, 0.04); border: 1px solid rgba(0, 0, 0, 0.08); border-radius: 6px; color: #1F1F1F; font-size: 13px; }"
                "QLineEdit:hover { background: rgba(0, 0, 0, 0.06); border: 1px solid rgba(0, 0, 0, 0.12); }"
                "QLineEdit:focus { background: #FFFFFF; border: 1px solid rgba(0, 0, 0, 0.20); color: #1F1F1F; }"
            ));
        }
    }
    if (m_iconSearch) {
        m_iconSearch->setStyleSheet(isDark ? QStringLiteral("color: rgba(255, 255, 255, 0.65);")
                                           : QStringLiteral("color: rgba(0, 0, 0, 0.55);"));
    }
    if (m_lblTitle) {
        m_lblTitle->setStyleSheet(isDark ? QStringLiteral("color: #FFFFFF;")
                                         : QStringLiteral("color: #1A1A1A;"));
    }

    if (m_heroPreview) {
        m_heroPreview->refresh();
    }
    update();
}

void SettingsWindow::setupTitleBar() {
    auto* bar = titleBar();
    if (!bar) return;

    bar->setVisible(true);
    bar->setTitleBarHeight(48);
    bar->installEventFilter(this);

    auto* titleBarContent = new SettingsTitleBarContent(bar);

    m_btnBack = new fluent::basicinput::Button(titleBarContent);
    m_btnBack->setFluentStyle(fluent::basicinput::Button::Subtle);
    m_btnBack->setIconGlyph(Typography::Icons::ChevronLeft, Typography::IconSize::Standard);
    m_btnBack->setFixedSize(32, 32);
    m_btnBack->setToolTip(QStringLiteral("返回"));
    m_btnBack->setEnabled(false);
    connect(m_btnBack, &QPushButton::clicked, this, [this]() {
        if (m_navHistory.size() > 1) {
            m_navHistory.pop_back(); // pop current
            int prev = m_navHistory.takeLast();
            onNavigationChanged(prev);
        } else {
            onNavigationChanged(0);
        }
    });

    m_lblTitle = new fluent::textfields::Label(QStringLiteral("设置"), titleBarContent);
    m_lblTitle->setFluentTypography(Typography::FontRole::Body);
    m_lblTitle->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
    m_lblTitle->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_lblTitle->setFixedHeight(32);

    m_searchBox = new fluent::textfields::LineEdit(titleBarContent);
    m_searchBox->setPlaceholderText(QStringLiteral("查找设置"));
    m_searchBox->setTextMargins(32, 0, 8, 0);
    m_searchBox->setFixedWidth(320);
    m_searchBox->setFixedHeight(32);
    connect(m_searchBox, &QLineEdit::textChanged, this, &SettingsWindow::onSearchFilterChanged);

    m_iconSearch = new fluent::FontIcon(Typography::Icons::Search, m_searchBox);
    m_iconSearch->setIconSize(Typography::IconSize::Standard);
    m_iconSearch->setFixedSize(16, 16);
    m_iconSearch->move(10, 8);

    titleBarContent->setControls(m_btnBack, m_lblTitle, m_searchBox);

    bar->setContentWidget(titleBarContent);
    bar->refreshChromeExclusions();
}

void SettingsWindow::setupUi() {
    setCustomWindowChromeEnabled(true);
    setCaptionButtonToolTips(
        QStringLiteral("最小化"), QStringLiteral("最大化"),
        QStringLiteral("关闭"), QStringLiteral("还原"));
    setCaptionButtonAccessibleNames(
        QStringLiteral("最小化"), QStringLiteral("最大化"),
        QStringLiteral("关闭"), QStringLiteral("还原"));

    setupTitleBar();
    applyThemeAndBackdrop();

    // Body: Left Navigation Pane + Right Content Pages
    auto* bodyWidget = new QWidget();
    revealWindowMaterial(bodyWidget);
    auto* bodyLayout = new QHBoxLayout(bodyWidget);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    bodyLayout->addWidget(createNavigationPane());

    m_pagesHost = new fluent::navigation::StackContentHost(bodyWidget);
    m_pagesHost->setTransitionEffect(fluent::navigation::StackContentHost::TransitionEffect::SlideFromBottom);
    m_pagesHost->setTransitionAnimationEnabled(true);
    revealWindowMaterial(m_pagesHost);

    m_pagesHost->insertPage(0, createPersonalizationPage());
    m_pagesHost->insertPage(1, createInputPage());
    m_pagesHost->insertPage(2, createMaintenancePage());
    m_pagesHost->insertPage(3, createAboutPage());

    bodyLayout->addWidget(m_pagesHost, 1);

    setContentWidget(bodyWidget);

    if (m_navPane) {
        m_navPane->setSelectedRow(0);
    }
    m_pagesHost->setCurrentIndex(0, 1, false);
}

QWidget* SettingsWindow::createNavigationPane() {
    auto* navWidget = new SettingsNavPane(this, this);
    m_navPane = navWidget;
    connect(m_navPane, &SettingsNavPane::rowActivated, this, &SettingsWindow::onNavigationChanged);
    return navWidget;
}

void SettingsWindow::onNavigationChanged(int row) {
    if (row < 0 || !m_pagesHost || row >= m_pagesHost->count()) return;
    int oldRow = m_pagesHost->currentIndex();

    if (m_navHistory.isEmpty() || m_navHistory.last() != row) {
        m_navHistory.append(row);
    }
    if (m_btnBack) {
        m_btnBack->setEnabled(row > 0);
    }

    if (oldRow == row) {
        if (m_navPane && m_navPane->selectedRow() != row) {
            m_navPane->setSelectedRow(row);
        }
        return;
    }

    if (m_navPane && m_navPane->selectedRow() != row) {
        m_navPane->setSelectedRow(row);
    }

    int direction = (row > oldRow) ? 1 : -1;
    m_pagesHost->setCurrentIndex(row, direction, true);

    if (auto* scroll = qobject_cast<QScrollArea*>(m_pagesHost->pageWidget(row))) {
        scroll->verticalScrollBar()->setValue(0);
    }
}

QWidget* SettingsWindow::createPageHeader(const QString& title, const QString& subtitle) {
    auto* header = new QWidget();
    revealWindowMaterial(header);
    auto* l = new QVBoxLayout(header);
    l->setContentsMargins(0, 0, 0, 12);
    l->setSpacing(4);

    auto* lblTitle = new fluent::textfields::Label(title, header);
    lblTitle->setFluentTypography(Typography::FontRole::Subtitle);
    QFont pf = lblTitle->font();
    pf.setBold(false);
    pf.setWeight(QFont::Normal);
    lblTitle->setFont(pf);
    lblTitle->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);

    auto* lblSub = new fluent::textfields::Label(subtitle, header);
    lblSub->setFluentTypography(Typography::FontRole::Caption);
    lblSub->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
    lblSub->setWordWrap(true);

    l->addWidget(lblTitle);
    l->addWidget(lblSub);
    return header;
}

QWidget* SettingsWindow::createSectionHeader(const QString& title) {
    auto* sec = new QWidget();
    revealWindowMaterial(sec);
    auto* l = new QVBoxLayout(sec);
    l->setContentsMargins(0, 14, 0, 6);

    auto* lbl = new fluent::textfields::Label(title, sec);
    lbl->setFluentTypography(Typography::FontRole::BodyLarge);
    QFont sf = lbl->font();
    sf.setBold(false);
    sf.setWeight(QFont::Normal);
    lbl->setFont(sf);
    lbl->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
    l->addWidget(lbl);
    return sec;
}

// ----------------------------------------------------------------------------
// Page 0: 个性化与排版 (Matching Figure 2 & Figure 3)
// ----------------------------------------------------------------------------
QWidget* SettingsWindow::createPersonalizationPage() {
    auto* scroll = new SettingsScrollView();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);

    auto* container = new QWidget();
    revealWindowMaterial(container);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(36, 20, 36, 36);
    layout->setSpacing(6);

    // Page Header
    layout->addWidget(createPageHeader(
        QStringLiteral("个性化与排版"),
        QStringLiteral("选择要应用的输入法外观材质、系统主题色与候选词流式排版")));

    // Top Hero Card (Visual CandidateWindow Preview & Preset Themes)
    m_heroPreview = new HeroPreviewCard(container);
    connect(m_heroPreview, &HeroPreviewCard::presetRequested, this, &SettingsWindow::onPresetApplied);
    layout->addWidget(m_heroPreview);

    auto& cfg = AppConfig::instance();

    // ------------------------------------------------------------------------
    // Section 1: 排版与候选显示
    // ------------------------------------------------------------------------
    layout->addWidget(createSectionHeader(QStringLiteral("排版与候选显示")));

    // 1. Orientation Card (AlignLeft icon)
    m_comboOrientation = new fluent::basicinput::ComboBox();
    m_comboOrientation->setFixedWidth(190);
    m_comboOrientation->addItem(QStringLiteral("水平排版 (Horizontal)"));
    m_comboOrientation->addItem(QStringLiteral("竖直排版 (Vertical)"));
    m_comboOrientation->setCurrentIndex(cfg.orientation() == QStringLiteral("vertical") ? 1 : 0);
    connect(m_comboOrientation, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWindow::onOrientationChanged);

    layout->addWidget(new SettingsCard(
        Typography::Icons::AlignLeft,
        QStringLiteral("候选窗排版方向"),
        QStringLiteral("选择候选词水平自适应药丸流式排版或传统垂直纵向排列"),
        m_comboOrientation, QColor(0, 164, 239), container));

    // 2. Page Size Card (List icon)
    auto* pageW = new QWidget();
    revealWindowMaterial(pageW);
    auto* pageL = new QHBoxLayout(pageW);
    pageL->setContentsMargins(0, 0, 0, 0);
    pageL->setSpacing(12);

    m_sliderPageSize = new fluent::basicinput::Slider(Qt::Horizontal);
    m_sliderPageSize->setRange(3, 9);
    m_sliderPageSize->setValue(cfg.pageSize());
    m_sliderPageSize->setFixedWidth(140);

    m_lblPageSizeVal = new fluent::textfields::Label(QString::number(cfg.pageSize()) + QStringLiteral(" 个"), pageW);
    m_lblPageSizeVal->setFixedWidth(40);
    m_lblPageSizeVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_lblPageSizeVal->setFluentTypography(Typography::FontRole::Body);

    pageL->addWidget(m_sliderPageSize);
    pageL->addWidget(m_lblPageSizeVal);

    connect(m_sliderPageSize, &fluent::basicinput::Slider::valueChanged,
            this, &SettingsWindow::onPageSizeChanged);

    layout->addWidget(new SettingsCard(
        Typography::Icons::List,
        QStringLiteral("单页候选词数量"),
        QStringLiteral("设置每页最多展示的候选词个数 (推荐 5~9 个)"),
        pageW, QColor(0, 164, 239), container));

    // 3. Font Size Card (Font icon)
    auto* fontW = new QWidget();
    revealWindowMaterial(fontW);
    auto* fontL = new QHBoxLayout(fontW);
    fontL->setContentsMargins(0, 0, 0, 0);
    fontL->setSpacing(12);

    m_sliderFontSize = new fluent::basicinput::Slider(Qt::Horizontal);
    m_sliderFontSize->setRange(10, 24);
    m_sliderFontSize->setValue(cfg.fontSize());
    m_sliderFontSize->setFixedWidth(140);

    m_lblFontSizeVal = new fluent::textfields::Label(QString::number(cfg.fontSize()) + QStringLiteral(" pt"), fontW);
    m_lblFontSizeVal->setFixedWidth(44);
    m_lblFontSizeVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_lblFontSizeVal->setFluentTypography(Typography::FontRole::Body);

    fontL->addWidget(m_sliderFontSize);
    fontL->addWidget(m_lblFontSizeVal);

    connect(m_sliderFontSize, &fluent::basicinput::Slider::valueChanged,
            this, &SettingsWindow::onFontSizeChanged);

    layout->addWidget(new SettingsCard(
        Typography::Icons::Font,
        QStringLiteral("候选词字号大小"),
        QStringLiteral("调整候选字词的显示字号，界面将自动等比缩放"),
        fontW, QColor(0, 164, 239), container));

    // Font Family Card
    m_comboFontFamily = new fluent::basicinput::ComboBox();
    m_comboFontFamily->setFixedWidth(230);
    m_comboFontFamily->addItem(QStringLiteral("Microsoft YaHei UI (系统推荐)"));
    m_comboFontFamily->addItem(QStringLiteral("思源黑体 (Source Han Sans SC)"));
    m_comboFontFamily->addItem(QStringLiteral("霞鹜文楷 (LXGW WenKai)"));
    m_comboFontFamily->addItem(QStringLiteral("微软雅黑 (Microsoft YaHei)"));
    m_comboFontFamily->addItem(QStringLiteral("Segoe UI Variable Text"));

    QString curFont = cfg.fontFamily();
    if (curFont == QStringLiteral("Source Han Sans SC")) m_comboFontFamily->setCurrentIndex(1);
    else if (curFont == QStringLiteral("LXGW WenKai")) m_comboFontFamily->setCurrentIndex(2);
    else if (curFont == QStringLiteral("Microsoft YaHei")) m_comboFontFamily->setCurrentIndex(3);
    else if (curFont == QStringLiteral("Segoe UI Variable Text")) m_comboFontFamily->setCurrentIndex(4);
    else m_comboFontFamily->setCurrentIndex(0);

    connect(m_comboFontFamily, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWindow::onFontFamilyChanged);

    layout->addWidget(new SettingsCard(
        Typography::Icons::Font,
        QStringLiteral("候选词字体族"),
        QStringLiteral("选择候选字词呈现的中文字体，自动适配系统 ClearType 平滑抗锯齿"),
        m_comboFontFamily, QColor(0, 164, 239), container));

    // 4. Show Label Card (CheckMark icon)
    m_switchShowLabel = new fluent::basicinput::ToggleSwitch();
    m_switchShowLabel->setOnContent(QStringLiteral("开"));
    m_switchShowLabel->setOffContent(QStringLiteral("关"));
    m_switchShowLabel->setIsOn(cfg.showLabel());
    connect(m_switchShowLabel, &fluent::basicinput::ToggleSwitch::toggled,
            this, &SettingsWindow::onShowLabelToggled);

    layout->addWidget(new SettingsCard(
        Typography::Icons::CheckMark,
        QStringLiteral("显示数字快捷序号"),
        QStringLiteral("在候选词左侧绘制 1 2 3 序号标签（纯净间距无点）"),
        m_switchShowLabel, QColor(0, 164, 239), container));

    // 5. Show Preedit Card (Keyboard icon)
    m_switchShowPreedit = new fluent::basicinput::ToggleSwitch();
    m_switchShowPreedit->setOnContent(QStringLiteral("开"));
    m_switchShowPreedit->setOffContent(QStringLiteral("关"));
    m_switchShowPreedit->setIsOn(cfg.showPreedit());
    connect(m_switchShowPreedit, &fluent::basicinput::ToggleSwitch::toggled,
            this, &SettingsWindow::onShowPreeditToggled);

    layout->addWidget(new SettingsCard(
        Typography::Icons::Keyboard,
        QStringLiteral("显示拼音输入串 (Preedit)"),
        QStringLiteral("在候选窗中展示正在键入的拼音字符（关闭后更紧凑纯净）"),
        m_switchShowPreedit, QColor(0, 164, 239), container));

    // ------------------------------------------------------------------------
    // Section 2: 材质与外观个性化 (Corner radius custom slider is removed!)
    // ------------------------------------------------------------------------
    layout->addWidget(createSectionHeader(QStringLiteral("材质与外观个性化")));

    // 6. Theme Mode (Brightness icon)
    m_comboTheme = new fluent::basicinput::ComboBox();
    m_comboTheme->setFixedWidth(190);
    m_comboTheme->addItem(QStringLiteral("深色模式 (Dark)"));
    m_comboTheme->addItem(QStringLiteral("浅色模式 (Light)"));
    m_comboTheme->addItem(QStringLiteral("跟随系统 (System)"));
    if (cfg.themeMode() == QStringLiteral("light")) m_comboTheme->setCurrentIndex(1);
    else if (cfg.themeMode() == QStringLiteral("system")) m_comboTheme->setCurrentIndex(2);
    else m_comboTheme->setCurrentIndex(0);

    connect(m_comboTheme, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWindow::onThemeModeChanged);

    layout->addWidget(new SettingsCard(
        Typography::Icons::Brightness,
        QStringLiteral("应用主题模式"),
        QStringLiteral("切换输入法面板与设置界面的深色曜黑或明亮浅色风格"),
        m_comboTheme, QColor(255, 140, 0), container));

    // 7. Backdrop Material (Desktop icon)
    m_comboBackdrop = new fluent::basicinput::ComboBox();
    m_comboBackdrop->setFixedWidth(190);
    m_comboBackdrop->addItem(QStringLiteral("云母材质 (Mica - 推荐)"));
    m_comboBackdrop->addItem(QStringLiteral("亚克力 (Acrylic)"));
    m_comboBackdrop->addItem(QStringLiteral("经典纯色 (Solid)"));
    if (cfg.backdrop() == QStringLiteral("acrylic")) m_comboBackdrop->setCurrentIndex(1);
    else if (cfg.backdrop() == QStringLiteral("solid")) m_comboBackdrop->setCurrentIndex(2);
    else m_comboBackdrop->setCurrentIndex(0);

    connect(m_comboBackdrop, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWindow::onBackdropChanged);

    layout->addWidget(new SettingsCard(
        Typography::Icons::Desktop,
        QStringLiteral("背景材质特效"),
        QStringLiteral("启用 Windows 11 DWM 硬件级云母材质 (Mica)，清晰且极具质感"),
        m_comboBackdrop, QColor(0, 164, 239), container));

    // 8. Accent Color (Color palette icon)
    m_comboAccent = new fluent::basicinput::ComboBox();
    m_comboAccent->setFixedWidth(230);
    QColor sysAccent = AppConfig::systemAccentColor();
    m_comboAccent->addItem(QStringLiteral("跟随系统主题色 (自动: %1)").arg(sysAccent.name().toUpper()));
    m_comboAccent->addItem(QStringLiteral("经典极客蓝 (#0078D4)"));
    m_comboAccent->addItem(QStringLiteral("清新松柏绿 (#107C41)"));
    m_comboAccent->addItem(QStringLiteral("优雅罗兰紫 (#8764B8)"));
    m_comboAccent->addItem(QStringLiteral("活力珊瑚橙 (#CA5010)"));

    QString curAccent = cfg.accentColor();
    if (curAccent.compare(QStringLiteral("auto"), Qt::CaseInsensitive) == 0 || curAccent.isEmpty()) {
        m_comboAccent->setCurrentIndex(0);
    } else if (curAccent.compare(QStringLiteral("#0078D4"), Qt::CaseInsensitive) == 0) {
        m_comboAccent->setCurrentIndex(1);
    } else if (curAccent.compare(QStringLiteral("#107C41"), Qt::CaseInsensitive) == 0) {
        m_comboAccent->setCurrentIndex(2);
    } else if (curAccent.compare(QStringLiteral("#8764B8"), Qt::CaseInsensitive) == 0) {
        m_comboAccent->setCurrentIndex(3);
    } else if (curAccent.compare(QStringLiteral("#CA5010"), Qt::CaseInsensitive) == 0) {
        m_comboAccent->setCurrentIndex(4);
    } else {
        m_comboAccent->setCurrentIndex(0);
    }

    connect(m_comboAccent, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWindow::onAccentColorChanged);

    layout->addWidget(new SettingsCard(
        Typography::Icons::Color,
        QStringLiteral("系统强调色"),
        QStringLiteral("用于指示候选选中高亮与左侧药丸竖条的品牌强调色"),
        m_comboAccent, QColor(171, 71, 188), container));

    layout->addStretch(1);
    scroll->setWidget(container);
    return scroll;
}

// ----------------------------------------------------------------------------
// Page 1: 输入与按键
// ----------------------------------------------------------------------------
QWidget* SettingsWindow::createInputPage() {
    auto* scroll = new SettingsScrollView();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);

    auto* container = new QWidget();
    revealWindowMaterial(container);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(36, 20, 36, 36);
    layout->setSpacing(6);

    layout->addWidget(createPageHeader(
        QStringLiteral("输入与按键"),
        QStringLiteral("个性化输入交互习惯、空格上屏键位与按键音效反馈")));

    layout->addWidget(createSectionHeader(QStringLiteral("输入习惯与反馈")));

    auto& cfg = AppConfig::instance();

    // 1. Space commit first
    m_switchSpaceCommit = new fluent::basicinput::ToggleSwitch();
    m_switchSpaceCommit->setOnContent(QStringLiteral("开"));
    m_switchSpaceCommit->setOffContent(QStringLiteral("关"));
    m_switchSpaceCommit->setIsOn(cfg.spaceCommitFirst());
    connect(m_switchSpaceCommit, &fluent::basicinput::ToggleSwitch::toggled,
            this, &SettingsWindow::onSpaceCommitToggled);

    layout->addWidget(new SettingsCard(
        Typography::Icons::Send,
        QStringLiteral("空格键直接上屏首选词"),
        QStringLiteral("按下 Space 空格键直接确认首选词并送至前台宿主程序"),
        m_switchSpaceCommit, QColor(171, 71, 188), container));

    // 2. Sound Feedback
    m_switchSound = new fluent::basicinput::ToggleSwitch();
    m_switchSound->setOnContent(QStringLiteral("开"));
    m_switchSound->setOffContent(QStringLiteral("关"));
    m_switchSound->setIsOn(cfg.soundEnabled());
    connect(m_switchSound, &fluent::basicinput::ToggleSwitch::toggled,
            this, &SettingsWindow::onSoundToggled);

    layout->addWidget(new SettingsCard(
        Typography::Icons::Volume,
        QStringLiteral("按键机械音效反馈"),
        QStringLiteral("击键时播放清脆机械键盘按键声 (Windows 原生音频)"),
        m_switchSound, QColor(0, 164, 239), container));

    // 3. Switch Key (Left Shift / Right Shift / Ctrl / None)
    m_comboSwitchKey = new fluent::basicinput::ComboBox();
    m_comboSwitchKey->setFixedWidth(210);
    m_comboSwitchKey->addItem(QStringLiteral("左 Shift 键 (Left Shift - 默认)"));
    m_comboSwitchKey->addItem(QStringLiteral("右 Shift 键 (Right Shift)"));
    m_comboSwitchKey->addItem(QStringLiteral("Ctrl 键 (Control)"));
    m_comboSwitchKey->addItem(QStringLiteral("无 (禁用单键切换)"));

    QString curSwitch = cfg.switchKey();
    if (curSwitch == QStringLiteral("RightShift")) m_comboSwitchKey->setCurrentIndex(1);
    else if (curSwitch == QStringLiteral("Ctrl")) m_comboSwitchKey->setCurrentIndex(2);
    else if (curSwitch == QStringLiteral("None")) m_comboSwitchKey->setCurrentIndex(3);
    else m_comboSwitchKey->setCurrentIndex(0);

    connect(m_comboSwitchKey, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWindow::onSwitchKeyChanged);

    layout->addWidget(new SettingsCard(
        Typography::Icons::Sync,
        QStringLiteral("中英文快捷切换按键"),
        QStringLiteral("单击选择切换中文拼音与半角英文输入状态的键盘按键 (推荐左 Shift)"),
        m_comboSwitchKey, QColor(0, 164, 239), container));

    // 4. Page Navigation Keys (, . / - = / [ ])
    m_comboPageKeys = new fluent::basicinput::ComboBox();
    m_comboPageKeys->setFixedWidth(210);
    m_comboPageKeys->addItem(QStringLiteral(", . (逗号 / 句号 - 推荐)"));
    m_comboPageKeys->addItem(QStringLiteral("- = (减号 / 等号)"));
    m_comboPageKeys->addItem(QStringLiteral("[ ] (左右方括号)"));
    m_comboPageKeys->addItem(QStringLiteral("PageUp / PageDown"));

    QString curPk = cfg.pageKeys();
    if (curPk == QStringLiteral("minus_equal")) m_comboPageKeys->setCurrentIndex(1);
    else if (curPk == QStringLiteral("bracket")) m_comboPageKeys->setCurrentIndex(2);
    else if (curPk == QStringLiteral("page_up_down")) m_comboPageKeys->setCurrentIndex(3);
    else m_comboPageKeys->setCurrentIndex(0);

    connect(m_comboPageKeys, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWindow::onPageKeysChanged);

    layout->addWidget(new SettingsCard(
        Typography::Icons::ChevronLeft,
        QStringLiteral("候选词翻页快捷键"),
        QStringLiteral("在长选单中通过符号键进行前翻一页、后翻一页快速选词"),
        m_comboPageKeys, QColor(0, 164, 239), container));

    // 5. CJK Spacing
    m_switchCjkSpacing = new fluent::basicinput::ToggleSwitch();
    m_switchCjkSpacing->setOnContent(QStringLiteral("开"));
    m_switchCjkSpacing->setOffContent(QStringLiteral("关"));
    m_switchCjkSpacing->setIsOn(cfg.cjkSpacing());
    connect(m_switchCjkSpacing, &fluent::basicinput::ToggleSwitch::toggled,
            this, &SettingsWindow::onCjkSpacingToggled);

    layout->addWidget(new SettingsCard(
        Typography::Icons::Font,
        QStringLiteral("中英文混输智能空格"),
        QStringLiteral("在汉字与英文字符/数字之间输入时自动追加微距空格，使排版更工整"),
        m_switchCjkSpacing, QColor(16, 124, 65), container));

    layout->addStretch(1);
    scroll->setWidget(container);
    return scroll;
}

// ----------------------------------------------------------------------------
// Page 2: 词库与维护
// ----------------------------------------------------------------------------
QWidget* SettingsWindow::createMaintenancePage() {
    auto* scroll = new SettingsScrollView();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);

    auto* container = new QWidget();
    revealWindowMaterial(container);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(36, 20, 36, 36);
    layout->setSpacing(6);

    layout->addWidget(createPageHeader(
        QStringLiteral("词库与维护"),
        QStringLiteral("管理 Rime 引擎核心依赖、雾凇拼音词库与执行方案重新部署")));

    layout->addWidget(createSectionHeader(QStringLiteral("Rime 引擎状态与方案")));

    auto& cfg = AppConfig::instance();

    // 1. Engine Status
    auto* lblEngineStatus = new fluent::textfields::Label(
        m_engine && m_engine->isLoaded() ? QStringLiteral("rime.dll 已就绪 (动态加载)")
                                         : QStringLiteral("仿真模拟模式 (Simulation)"));
    lblEngineStatus->setFluentTypography(Typography::FontRole::Body);
    lblEngineStatus->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);

    layout->addWidget(new SettingsCard(
        Typography::Icons::CheckMark,
        QStringLiteral("Rime 引擎状态"),
        QStringLiteral("当前加载的 Rime 核心 C API 库与会话连接状态"),
        lblEngineStatus, QColor(16, 124, 65), container));

    // 2. User directory
    auto* btnOpenDict = new fluent::basicinput::Button(QStringLiteral("打开目录"));
    btnOpenDict->setFluentStyle(fluent::basicinput::Button::Standard);
    connect(btnOpenDict, &fluent::basicinput::Button::clicked, this, []() {
        QString path = QStringLiteral("C:/Users/zheng/AppData/Roaming/Rime");
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });

    layout->addWidget(new SettingsCard(
        Typography::Icons::Folder,
        QStringLiteral("雾凇拼音用户词库"),
        QStringLiteral("词库位于 C:/Users/zheng/AppData/Roaming/Rime (rime-ice)"),
        btnOpenDict, QColor(16, 124, 65), container));

    // 3. Deploy
    auto* deployBox = new QWidget();
    revealWindowMaterial(deployBox);
    auto* deployLayout = new QHBoxLayout(deployBox);
    deployLayout->setContentsMargins(0, 0, 0, 0);
    deployLayout->setSpacing(12);

    m_lblDeployStatus = new fluent::textfields::Label(QStringLiteral("就绪"));
    m_lblDeployStatus->setFluentTypography(Typography::FontRole::Caption);
    m_lblDeployStatus->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);

    m_btnDeploy = new fluent::basicinput::Button(QStringLiteral("立即重新部署"));
    m_btnDeploy->setFluentStyle(fluent::basicinput::Button::Accent);
    connect(m_btnDeploy, &fluent::basicinput::Button::clicked, this, &SettingsWindow::onDeployClicked);

    deployLayout->addWidget(m_lblDeployStatus);
    deployLayout->addWidget(m_btnDeploy);

    layout->addWidget(new SettingsCard(
        Typography::Icons::Sync,
        QStringLiteral("重新部署词库与配置"),
        QStringLiteral("重新编译并加载方案字典与自定义配置 (Rime Deploy)"),
        deployBox, QColor(0, 164, 239), container));

    // 4. Active Schema Card
    m_comboSchema = new fluent::basicinput::ComboBox();
    m_comboSchema->setFixedWidth(240);
    m_comboSchema->addItem(QStringLiteral("雾凇拼音全拼 (rime-ice) [默认]"));
    m_comboSchema->addItem(QStringLiteral("雾凇双拼 · 自然码方案"));
    m_comboSchema->addItem(QStringLiteral("雾凇双拼 · 小鹤双拼方案"));
    m_comboSchema->addItem(QStringLiteral("朙月拼音 · 传统全拼 (luna-pinyin)"));

    QString curSch = cfg.schema();
    if (curSch == QStringLiteral("zrm")) m_comboSchema->setCurrentIndex(1);
    else if (curSch == QStringLiteral("flypy")) m_comboSchema->setCurrentIndex(2);
    else if (curSch == QStringLiteral("luna_pinyin")) m_comboSchema->setCurrentIndex(3);
    else m_comboSchema->setCurrentIndex(0);

    connect(m_comboSchema, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWindow::onSchemaChanged);

    layout->addWidget(new SettingsCard(
        Typography::Icons::Document,
        QStringLiteral("输入法方案 (Rime Schema)"),
        QStringLiteral("选择当前生效的雾凇拼音或双拼输入方案，更改后将自动重新部署"),
        m_comboSchema, QColor(0, 164, 239), container));

    // 5. Backup User Dict Card
    auto* backupBox = new QWidget();
    revealWindowMaterial(backupBox);
    auto* backupLayout = new QHBoxLayout(backupBox);
    backupLayout->setContentsMargins(0, 0, 0, 0);
    backupLayout->setSpacing(12);

    m_lblBackupStatus = new fluent::textfields::Label(QStringLiteral("未备份"));
    m_lblBackupStatus->setFluentTypography(Typography::FontRole::Caption);
    m_lblBackupStatus->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);

    m_btnBackupDict = new fluent::basicinput::Button(QStringLiteral("立即备份词库"));
    m_btnBackupDict->setFluentStyle(fluent::basicinput::Button::Standard);
    connect(m_btnBackupDict, &fluent::basicinput::Button::clicked, this, &SettingsWindow::onBackupDictClicked);

    backupLayout->addWidget(m_lblBackupStatus);
    backupLayout->addWidget(m_btnBackupDict);

    layout->addWidget(new SettingsCard(
        Typography::Icons::Save,
        QStringLiteral("用户词库备份与归档"),
        QStringLiteral("将个人自造词与字词使用频次快照归档至本地 Rime/backup 目录"),
        backupBox, QColor(16, 124, 65), container));

    // 6. Weasel Install Directory
    auto* btnOpenInstallDir = new fluent::basicinput::Button(QStringLiteral("浏览安装路径"));
    btnOpenInstallDir->setFluentStyle(fluent::basicinput::Button::Standard);
    connect(btnOpenInstallDir, &fluent::basicinput::Button::clicked, this, []() {
        QString path = QStringLiteral("C:/Program Files/Rime/weasel-0.17.4");
        if (!QFileInfo::exists(path)) {
            path = QStringLiteral("C:/Program Files/Rime");
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });

    layout->addWidget(new SettingsCard(
        Typography::Icons::Storage,
        QStringLiteral("小狼毫安装程序目录"),
        QStringLiteral("查看位于 Program Files 的 Weasel 核心组件与系统字典资源"),
        btnOpenInstallDir, QColor(0, 164, 239), container));

    layout->addStretch(1);
    scroll->setWidget(container);
    return scroll;
}

// ----------------------------------------------------------------------------
// Page 3: 关于与系统
// ----------------------------------------------------------------------------
QWidget* SettingsWindow::createAboutPage() {
    auto* scroll = new SettingsScrollView();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);

    auto* container = new QWidget();
    revealWindowMaterial(container);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(36, 20, 36, 36);
    layout->setSpacing(6);

    layout->addWidget(createPageHeader(
        QStringLiteral("关于与系统"),
        QStringLiteral("Weasel Fluent 输入法前端体系版本与配置重置恢复")));

    layout->addWidget(createSectionHeader(QStringLiteral("关于 Weasel Fluent")));

    // 1. Version Card
    auto* lblVer = new fluent::textfields::Label(QStringLiteral("v1.0.0 (WinUI 3 / Fluent 2)"));
    lblVer->setFluentTypography(Typography::FontRole::Body);
    lblVer->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);

    layout->addWidget(new SettingsCard(
        Typography::Icons::Info,
        QStringLiteral("版本与技术栈"),
        QStringLiteral("C++17 + Qt 6.8.2 + Fluent-Qt + Windows 11 Mica 架构"),
        lblVer, QColor(255, 140, 0), container));

    // 2. Reset Defaults Card
    m_btnReset = new fluent::basicinput::Button(QStringLiteral("恢复默认"));
    m_btnReset->setFluentStyle(fluent::basicinput::Button::Standard);
    connect(m_btnReset, &fluent::basicinput::Button::clicked, this, &SettingsWindow::onResetDefaultsClicked);

    layout->addWidget(new SettingsCard(
        Typography::Icons::Undo,
        QStringLiteral("恢复出厂默认设置"),
        QStringLiteral("重置所有排版、材质、快捷键配置回滚到默认状态"),
        m_btnReset, QColor(230, 80, 80), container));

    // 3. Open Source Component Card
    auto* btnOpenLicense = new fluent::basicinput::Button(QStringLiteral("访问项目仓库"));
    btnOpenLicense->setFluentStyle(fluent::basicinput::Button::Standard);
    connect(btnOpenLicense, &fluent::basicinput::Button::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/rime/weasel")));
    });

    layout->addWidget(new SettingsCard(
        Typography::Icons::World,
        QStringLiteral("开源核心与第三方库"),
        QStringLiteral("librime (GPLv3) · Qt 6.8.2 (LGPLv3) · Fluent-Qt (MIT) · 雾凇拼音 (CC-BY-SA)"),
        btnOpenLicense, QColor(0, 164, 239), container));

    // 4. Export Config Card
    auto* btnExportConfig = new fluent::basicinput::Button(QStringLiteral("打开配置目录"));
    btnExportConfig->setFluentStyle(fluent::basicinput::Button::Standard);
    connect(btnExportConfig, &fluent::basicinput::Button::clicked, this, []() {
        QString path = QFileInfo(AppConfig::instance().configPath()).absolutePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });

    layout->addWidget(new SettingsCard(
        Typography::Icons::Download,
        QStringLiteral("配置文件管理与导出"),
        QStringLiteral("直接访问当前个性化排版与快捷键存储文件 (config.json)"),
        btnExportConfig, QColor(16, 124, 65), container));

    layout->addStretch(1);
    scroll->setWidget(container);
    return scroll;
}

// ============================================================================
// Slots Implementation
// ============================================================================

void SettingsWindow::onOrientationChanged(int index) {
    AppConfig::instance().setOrientation(index == 1 ? QStringLiteral("vertical") : QStringLiteral("horizontal"));
    if (m_heroPreview) m_heroPreview->refresh();
}

void SettingsWindow::onPageSizeChanged(int value) {
    m_lblPageSizeVal->setText(QString::number(value) + QStringLiteral(" 个"));
    AppConfig::instance().setPageSize(value);
    if (m_heroPreview) m_heroPreview->refresh();
}

void SettingsWindow::onFontSizeChanged(int value) {
    m_lblFontSizeVal->setText(QString::number(value) + QStringLiteral(" pt"));
    AppConfig::instance().setFontSize(value);
    if (m_heroPreview) m_heroPreview->refresh();
}

void SettingsWindow::onThemeModeChanged(int index) {
    if (index == 1) AppConfig::instance().setThemeMode(QStringLiteral("light"));
    else if (index == 2) AppConfig::instance().setThemeMode(QStringLiteral("system"));
    else AppConfig::instance().setThemeMode(QStringLiteral("dark"));
    applyThemeAndBackdrop();
}

void SettingsWindow::onBackdropChanged(int index) {
    if (index == 1) AppConfig::instance().setBackdrop(QStringLiteral("acrylic"));
    else if (index == 2) AppConfig::instance().setBackdrop(QStringLiteral("solid"));
    else AppConfig::instance().setBackdrop(QStringLiteral("mica"));
    applyThemeAndBackdrop();
}

void SettingsWindow::onAccentColorChanged(int index) {
    QString c = QStringLiteral("auto");
    if (index == 1) c = QStringLiteral("#0078D4");
    else if (index == 2) c = QStringLiteral("#107C41");
    else if (index == 3) c = QStringLiteral("#8764B8");
    else if (index == 4) c = QStringLiteral("#CA5010");
    AppConfig::instance().setAccentColor(c);
    applyThemeAndBackdrop();
}

void SettingsWindow::onSoundToggled(bool on) {
    AppConfig::instance().setSoundEnabled(on);
    if (on) {
        playMechanicalClickSound();
    }
}

void SettingsWindow::onSpaceCommitToggled(bool on) {
    AppConfig::instance().setSpaceCommitFirst(on);
}

void SettingsWindow::onShowLabelToggled(bool on) {
    AppConfig::instance().setShowLabel(on);
    if (m_heroPreview) m_heroPreview->refresh();
}

void SettingsWindow::onShowPreeditToggled(bool on) {
    AppConfig::instance().setShowPreedit(on);
    if (m_heroPreview) m_heroPreview->refresh();
}

void SettingsWindow::onFontFamilyChanged(int index) {
    QString fam = QStringLiteral("Microsoft YaHei UI");
    if (index == 1) fam = QStringLiteral("Source Han Sans SC");
    else if (index == 2) fam = QStringLiteral("LXGW WenKai");
    else if (index == 3) fam = QStringLiteral("Microsoft YaHei");
    else if (index == 4) fam = QStringLiteral("Segoe UI Variable Text");
    AppConfig::instance().setFontFamily(fam);
    if (m_heroPreview) m_heroPreview->refresh();
}

void SettingsWindow::onSwitchKeyChanged(int index) {
    QString k = QStringLiteral("Shift");
    if (index == 1) k = QStringLiteral("RightShift");
    else if (index == 2) k = QStringLiteral("Ctrl");
    else if (index == 3) k = QStringLiteral("None");
    AppConfig::instance().setSwitchKey(k);
}

void SettingsWindow::onPageKeysChanged(int index) {
    QString k = QStringLiteral("comma_period");
    if (index == 1) k = QStringLiteral("minus_equal");
    else if (index == 2) k = QStringLiteral("bracket");
    else if (index == 3) k = QStringLiteral("page_up_down");
    AppConfig::instance().setPageKeys(k);
}

void SettingsWindow::onCjkSpacingToggled(bool on) {
    AppConfig::instance().setCjkSpacing(on);
}

void SettingsWindow::onSchemaChanged(int index) {
    QString s = QStringLiteral("rime_ice");
    if (index == 1) s = QStringLiteral("zrm");
    else if (index == 2) s = QStringLiteral("flypy");
    else if (index == 3) s = QStringLiteral("luna_pinyin");
    AppConfig::instance().setSchema(s);
    if (m_heroPreview) m_heroPreview->refresh();
    onDeployClicked();
}

void SettingsWindow::onBackupDictClicked() {
    QString rimeDir = QStringLiteral("C:/Users/zheng/AppData/Roaming/Rime");
    QString backupDir = rimeDir + QStringLiteral("/backup/") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"));
    QDir().mkpath(backupDir);

    QDir src(rimeDir);
    QStringList files = src.entryList(QStringList() << QStringLiteral("*.userdb") << QStringLiteral("*.yaml"), QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    int copied = 0;
    for (const QString& f : files) {
        QFile::copy(src.filePath(f), backupDir + QStringLiteral("/") + f);
        copied++;
    }

    if (m_lblBackupStatus) {
        m_lblBackupStatus->setText(QStringLiteral("已备份至 backup 目录"));
    }
}

void SettingsWindow::onSearchFilterChanged(const QString& query) {
    QString q = query.trimmed().toLower();
    if (q.isEmpty()) {
        auto allCards = findChildren<SettingsCard*>();
        for (auto* card : allCards) {
            card->setVisible(true);
        }
        if (m_heroPreview) m_heroPreview->setVisible(true);
        return;
    }

    if (m_heroPreview) m_heroPreview->setVisible(false);

    int curPage = m_pagesHost ? m_pagesHost->currentIndex() : 0;
    int curPageMatches = 0;
    int bestOtherPage = -1;
    int bestOtherMatches = 0;

    for (int p = 0; p < (m_pagesHost ? m_pagesHost->count() : 4); ++p) {
        QWidget* pageWidget = m_pagesHost->pageWidget(p);
        if (!pageWidget) continue;
        auto cards = pageWidget->findChildren<SettingsCard*>();
        int count = 0;
        for (auto* card : cards) {
            bool matches = card->title().toLower().contains(q) ||
                           card->subtitle().toLower().contains(q);
            card->setVisible(matches);
            if (matches) count++;
        }
        if (p == curPage) {
            curPageMatches = count;
        } else if (count > bestOtherMatches) {
            bestOtherMatches = count;
            bestOtherPage = p;
        }
    }

    if (curPageMatches == 0 && bestOtherPage >= 0) {
        onNavigationChanged(bestOtherPage);
    }
}

void SettingsWindow::onDeployClicked() {
    m_lblDeployStatus->setText(QStringLiteral("部署中..."));
    if (m_engine) {
        m_engine->deploy();
    }
    m_lblDeployStatus->setText(QStringLiteral("部署完成"));
}

void SettingsWindow::onPresetApplied(int index) {
    auto& cfg = AppConfig::instance();
    cfg.setCornerRadius(6); // Fixed inner & outer corner radius 6.0px

    if (index == 0) {
        // Windows Default
        cfg.setThemeMode(QStringLiteral("dark"));
        cfg.setBackdrop(QStringLiteral("mica"));
        cfg.setAccentColor(QStringLiteral("auto"));
    } else if (index == 1) {
        // Obsidian Minimal
        cfg.setThemeMode(QStringLiteral("dark"));
        cfg.setBackdrop(QStringLiteral("solid"));
        cfg.setAccentColor(QStringLiteral("#00B7C3"));
    } else if (index == 2) {
        // Elegant Violet
        cfg.setThemeMode(QStringLiteral("dark"));
        cfg.setBackdrop(QStringLiteral("mica"));
        cfg.setAccentColor(QStringLiteral("#8764B8"));
    } else if (index == 3) {
        // Morning Light
        cfg.setThemeMode(QStringLiteral("light"));
        cfg.setBackdrop(QStringLiteral("mica"));
        cfg.setAccentColor(QStringLiteral("#107C41"));
    }

    syncControlsFromConfig();
    applyThemeAndBackdrop();
}

void SettingsWindow::syncControlsFromConfig() {
    auto& cfg = AppConfig::instance();

    if (m_comboOrientation) {
        m_comboOrientation->setCurrentIndex(cfg.orientation() == QStringLiteral("vertical") ? 1 : 0);
    }
    if (m_sliderPageSize) {
        m_sliderPageSize->setValue(cfg.pageSize());
    }
    if (m_sliderFontSize) {
        m_sliderFontSize->setValue(cfg.fontSize());
    }
    if (m_comboFontFamily) {
        QString f = cfg.fontFamily();
        if (f == QStringLiteral("Source Han Sans SC")) m_comboFontFamily->setCurrentIndex(1);
        else if (f == QStringLiteral("LXGW WenKai")) m_comboFontFamily->setCurrentIndex(2);
        else if (f == QStringLiteral("Microsoft YaHei")) m_comboFontFamily->setCurrentIndex(3);
        else if (f == QStringLiteral("Segoe UI Variable Text")) m_comboFontFamily->setCurrentIndex(4);
        else m_comboFontFamily->setCurrentIndex(0);
    }
    if (m_comboTheme) {
        if (cfg.themeMode() == QStringLiteral("light")) m_comboTheme->setCurrentIndex(1);
        else if (cfg.themeMode() == QStringLiteral("system")) m_comboTheme->setCurrentIndex(2);
        else m_comboTheme->setCurrentIndex(0);
    }
    if (m_comboBackdrop) {
        if (cfg.backdrop() == QStringLiteral("acrylic")) m_comboBackdrop->setCurrentIndex(1);
        else if (cfg.backdrop() == QStringLiteral("solid")) m_comboBackdrop->setCurrentIndex(2);
        else m_comboBackdrop->setCurrentIndex(0);
    }
    if (m_comboAccent) {
        QString cur = cfg.accentColor();
        if (cur.compare(QStringLiteral("#0078D4"), Qt::CaseInsensitive) == 0) m_comboAccent->setCurrentIndex(1);
        else if (cur.compare(QStringLiteral("#107C41"), Qt::CaseInsensitive) == 0) m_comboAccent->setCurrentIndex(2);
        else if (cur.compare(QStringLiteral("#8764B8"), Qt::CaseInsensitive) == 0) m_comboAccent->setCurrentIndex(3);
        else if (cur.compare(QStringLiteral("#CA5010"), Qt::CaseInsensitive) == 0) m_comboAccent->setCurrentIndex(4);
        else m_comboAccent->setCurrentIndex(0);
    }
    if (m_switchShowLabel) {
        m_switchShowLabel->setIsOn(cfg.showLabel());
    }
    if (m_switchShowPreedit) {
        m_switchShowPreedit->setIsOn(cfg.showPreedit());
    }
    if (m_switchSound) {
        m_switchSound->setIsOn(cfg.soundEnabled());
    }
    if (m_switchSpaceCommit) {
        m_switchSpaceCommit->setIsOn(cfg.spaceCommitFirst());
    }
    if (m_comboSwitchKey) {
        QString s = cfg.switchKey();
        if (s == QStringLiteral("RightShift")) m_comboSwitchKey->setCurrentIndex(1);
        else if (s == QStringLiteral("Ctrl")) m_comboSwitchKey->setCurrentIndex(2);
        else if (s == QStringLiteral("None")) m_comboSwitchKey->setCurrentIndex(3);
        else m_comboSwitchKey->setCurrentIndex(0);
    }
    if (m_comboPageKeys) {
        QString p = cfg.pageKeys();
        if (p == QStringLiteral("minus_equal")) m_comboPageKeys->setCurrentIndex(1);
        else if (p == QStringLiteral("bracket")) m_comboPageKeys->setCurrentIndex(2);
        else if (p == QStringLiteral("page_up_down")) m_comboPageKeys->setCurrentIndex(3);
        else m_comboPageKeys->setCurrentIndex(0);
    }
    if (m_switchCjkSpacing) {
        m_switchCjkSpacing->setIsOn(cfg.cjkSpacing());
    }
    if (m_comboSchema) {
        QString sch = cfg.schema();
        if (sch == QStringLiteral("zrm")) m_comboSchema->setCurrentIndex(1);
        else if (sch == QStringLiteral("flypy")) m_comboSchema->setCurrentIndex(2);
        else if (sch == QStringLiteral("luna_pinyin")) m_comboSchema->setCurrentIndex(3);
        else m_comboSchema->setCurrentIndex(0);
    }
}

void SettingsWindow::onResetDefaultsClicked() {
    auto& cfg = AppConfig::instance();
    cfg.setOrientation(QStringLiteral("horizontal"));
    cfg.setPageSize(5);
    cfg.setFontSize(12);
    cfg.setFontFamily(QStringLiteral("Microsoft YaHei UI"));
    cfg.setThemeMode(QStringLiteral("dark"));
    cfg.setBackdrop(QStringLiteral("mica"));
    cfg.setCornerRadius(6); // Fixed 6.0px
    cfg.setAccentColor(QStringLiteral("auto"));
    cfg.setSoundEnabled(false);
    cfg.setSpaceCommitFirst(true);
    cfg.setShowLabel(true);
    cfg.setShowPreedit(false);
    cfg.setSwitchKey(QStringLiteral("Shift"));
    cfg.setPageKeys(QStringLiteral("comma_period"));
    cfg.setSchema(QStringLiteral("rime_ice"));
    cfg.setCjkSpacing(false);

    syncControlsFromConfig();
    applyThemeAndBackdrop();
}

void SettingsWindow::selectNavigationRow(int row) {
    onNavigationChanged(row);
}

QRectF SettingsWindow::currentNavIndicatorRect(qreal progress) const {
    if (!m_navPane) return {};
    return m_navPane->currentIndicatorRect(progress);
}
