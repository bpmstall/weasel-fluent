#ifndef EMOJI_PICKER_WINDOW_H
#define EMOJI_PICKER_WINDOW_H

#include <QWidget>
#include <QString>
#include <QVector>
#include <QRectF>
#include <QPointF>

class EmojiPickerWindow : public QWidget {
    Q_OBJECT

public:
    enum class Category {
        Emoji = 0,
        Kaomoji,
        Symbols
    };

    explicit EmojiPickerWindow(QWidget* parent = nullptr);
    ~EmojiPickerWindow() override = default;

    void setCategory(Category cat);
    Category currentCategory() const { return m_category; }

    int currentPage() const { return m_pageNo; }
    int totalPages() const;

    QImage renderPreviewImage(qreal dpr = 1.0);

    static const QPixmap& getEmojiPixmap(const QString& str);

signals:
    void textSelected(const QString& text);
    void closed();

protected:
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void updateHitTargets();
    const QVector<QString>& currentDataset() const;
    int itemsPerPage() const;

    Category m_category = Category::Emoji;
    int m_pageNo = 0;

    int m_hoverItem = -1;
    int m_hoverTab = -1;
    bool m_hoverClose = false;
    bool m_hoverPrev = false;
    bool m_hoverNext = false;

    QRectF m_btnClose;
    QVector<QRectF> m_tabRects;
    QVector<QRectF> m_itemRects;
    QRectF m_btnPrev;
    QRectF m_btnNext;
    QRectF m_pageTextRect;

    static const QVector<QString> s_emojis;
    static const QVector<QString> s_kaomojis;
    static const QVector<QString> s_symbols;

    // Animated tab underline
    void updateTabAnimation(bool animate);
    QRectF m_animatedTabUnderline;
    QRectF m_tabUnderlineStart;
    QRectF m_tabUnderlineTarget;
    class QVariantAnimation* m_tabAnim = nullptr;
};

#endif // EMOJI_PICKER_WINDOW_H
