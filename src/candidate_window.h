#ifndef CANDIDATE_WINDOW_H
#define CANDIDATE_WINDOW_H

#include <QWidget>
#include <QVector>
#include <QRect>
#include "rime_engine.h"
#include "config.h"

class EmojiPickerWindow;
class ExtensionPanelWindow;

namespace fluent::menus_toolbars {
class FluentMenu;
}

class CandidateWindow : public QWidget {
    Q_OBJECT

public:
    explicit CandidateWindow(RimeEngine* engine, QWidget* parent = nullptr);
    ~CandidateWindow() override;

    void updateUiState(const RimeUiState& state);
    void reloadConfig();
    void setPreviewState(const RimeUiState& state);
    QImage renderPreviewImage(qreal dpr = 1.0);
    QSize calculatedSize() const { return m_calculatedSize; }
    void moveToPosition(const QPoint& pt);

    void toggleEmojiPicker();
    void showCustomEmojiPicker();
    class EmojiPickerWindow* emojiPicker() const { return m_emojiPicker; }

    void toggleExpanded();
    bool isExpanded() const { return m_isExpanded; }

    void toggleExtensionPanel();
    void showExtensionPanel();
    class ExtensionPanelWindow* extensionPanel() const { return m_extensionPanel; }

    fluent::menus_toolbars::FluentMenu* createExtensionMenu();
    void showExtensionMenu();

signals:
    void openSettingsRequested();
    void candidateSelected(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;

#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif

private:
    void calculateLayout();
    void drawHorizontal(QPainter& p);
    void drawHorizontalExpanded(QPainter& p);
    void drawVertical(QPainter& p);
    void drawVectorTriangle(QPainter& p, const QPointF& center, qreal size, bool pointsLeft, const QColor& color);
    void drawVectorChevron(QPainter& p, const QRectF& rect, const QColor& color, bool pointsUp = false);
    void drawVectorGear(QPainter& p, const QRectF& rect, const QColor& color);
    void drawVectorSticker(QPainter& p, const QRectF& rect, const QColor& color);

    RimeEngine* m_engine = nullptr;
    RimeUiState m_state;
    EmojiPickerWindow* m_emojiPicker = nullptr;
    ExtensionPanelWindow* m_extensionPanel = nullptr;

    bool m_isExpanded = false;

    // Hit-test rects
    struct CandidateRect {
        int index = 0;
        QRectF rect;
    };
    QVector<CandidateRect> m_candidateRects;
    QRectF m_btnPrevPage;
    QRectF m_btnNextPage;
    QRectF m_btnSticker;
    QRectF m_btnSettings;
    QRectF m_btnCollapse;

    int m_hoverCandidate = -1;
    bool m_hoverPrev = false;
    bool m_hoverNext = false;
    bool m_hoverSticker = false;
    bool m_hoverSettings = false;
    bool m_hoverCollapse = false;

    qreal m_dpiScale = 1.0;
    QSize m_calculatedSize;

    // Animated selection indicator
    void updateSelectionAnimation(bool animate);
    void startEntranceAnimation();
    QRectF indicatorBaseRect(int index) const;
    QRectF currentIndicatorRect() const;
    QRectF m_indicatorPrevRect;
    QRectF m_indicatorTargetRect;
    int m_lastHighlightIndex = 0;
    int m_indicatorMotionDirection = 0;
    qreal m_indicatorProgress = 1.0;
    class QVariantAnimation* m_selectionAnim = nullptr;

    // Window entrance fade
    qreal m_windowOpacity = 1.0;
    class QVariantAnimation* m_fadeAnim = nullptr;
};

void playMechanicalClickSound();

#endif // CANDIDATE_WINDOW_H
