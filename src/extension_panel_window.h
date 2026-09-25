#ifndef EXTENSION_PANEL_WINDOW_H
#define EXTENSION_PANEL_WINDOW_H

#include <QWidget>
#include <QString>
#include <QVector>
#include <QRectF>
#include <QPointF>
#include <QTimer>
#include <QVariantAnimation>
#include "rime_engine.h"
#include "config.h"

class ExtensionPanelWindow : public QWidget {
    Q_OBJECT

public:
    enum class Category {
        Snippets = 0, // 快捷短语
        Tools,        // 文本工具
        Modes         // 输入状态
    };

    explicit ExtensionPanelWindow(RimeEngine* engine, QWidget* parent = nullptr);
    ~ExtensionPanelWindow() override = default;

    void setCategory(Category cat);
    Category currentCategory() const { return m_category; }

    void updateHitTargets();
    QImage renderPreviewImage(qreal dpr = 1.0);

    // Generators and utilities
    static QString generateRandomPassword(int length = 16);
    static QString evaluateExpression(const QString& expr);
    static QString formatCase(const QString& text, int mode);
    static QString numberToChineseAmount(double amount);

signals:
    void textCommitRequested(const QString& text);
    void openSettingsRequested();
    void closed();

protected:
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif

private:
    void updateTabAnimation(bool animate);
    void drawVectorGear(QPainter& p, const QRectF& rect, const QColor& color);

    RimeEngine* m_engine = nullptr;
    Category m_category = Category::Snippets;

    // Item structure for list rows & mode tiles
    struct ItemRow {
        QRectF rect;
        QString icon;
        QString title;
        QString subtitle;
        QString actionData;
        bool isMode = false;
        bool modeActive = false;
    };
    QVector<ItemRow> m_items;

    // Bottom Bar Hit targets
    QVector<QRectF> m_tabRects;
    QRectF m_btnSettings;

    // Animated tab underline / pill
    QRectF m_animatedTabUnderline;
    QRectF m_tabUnderlineStart;
    QRectF m_tabUnderlineTarget;
    QVariantAnimation* m_tabAnim = nullptr;

    // Hover states
    int m_hoverItem = -1;
    int m_hoverTab = -1;
    bool m_hoverSettings = false;

    // Deploy status feedback
    QString m_deployStatusText;
    QTimer* m_deployStatusTimer = nullptr;

    // Dynamic password cache
    QString m_currentSamplePassword;
};

#endif // EXTENSION_PANEL_WINDOW_H
