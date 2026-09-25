#ifndef SETTINGS_WINDOW_H
#define SETTINGS_WINDOW_H

#include <QWidget>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QVector>

#include "components/windowing/Window.h"
#include "components/layout/Card.h"
#include "components/scrolling/ScrollView.h"
#include "components/navigation/StackContentHost.h"
#include "components/collections/TreeView.h"
#include <QStandardItemModel>
#include "components/basicinput/Button.h"
#include "components/basicinput/ComboBox.h"
#include "components/basicinput/Slider.h"
#include "components/basicinput/ToggleSwitch.h"
#include "components/foundation/FontIcon.h"
#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"
#include "design/Typography.h"
#include "compatibility/QtCompat.h"
#include "config.h"
#include "rime_engine.h"
#include "candidate_window.h"

/**
 * @brief WinUI 3 / Windows 11 Settings-style Setting Row Card.
 */
class SettingsCard : public fluent::layout::Card {
    Q_OBJECT

public:
    SettingsCard(const QString& iconGlyph,
                 const QString& title,
                 const QString& subtitle,
                 QWidget* trailingWidget,
                 const QColor& iconColor = QColor(),
                 QWidget* parent = nullptr);

    void setTrailingWidget(QWidget* trailingWidget);
    QString title() const;
    QString subtitle() const;
    void setMatchesSearch(bool matches);

private:
    fluent::FontIcon* m_icon = nullptr;
    fluent::textfields::Label* m_titleLabel = nullptr;
    fluent::textfields::Label* m_subLabel = nullptr;
    QWidget* m_trailing = nullptr;
    QHBoxLayout* m_layout = nullptr;
};

/**
 * @brief WinUI 3 / Windows 11 Settings navigation pane powered by Fluent-Qt TreeView.
 */
class SettingsNavPane : public QWidget, public fluent::FluentElement {
    Q_OBJECT

public:
    explicit SettingsNavPane(fluent::FluentElement* themeHost, QWidget* parent = nullptr);
    ~SettingsNavPane() override = default;

    void setSelectedRow(int row);
    int selectedRow() const;
    QRectF currentIndicatorRect(qreal progress = -1.0) const;

    fluent::collections::TreeView* treeView() const { return m_treeView; }

signals:
    void rowActivated(int row);

protected:
    void paintEvent(QPaintEvent* event) override;
    void onThemeUpdated() override;

private:
    fluent::FluentElement* m_themeHost = nullptr;
    fluent::collections::TreeView* m_treeView = nullptr;
    QStandardItemModel* m_model = nullptr;
};

/**
 * @brief Top Hero Card featuring live CandidateWindow preview and theme presets.
 */
class HeroPreviewCard : public fluent::layout::Card {
    Q_OBJECT

public:
    explicit HeroPreviewCard(QWidget* parent = nullptr);
    void refresh();

signals:
    void presetRequested(int presetIndex);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QRectF m_presetRects[4];
    int m_hoveredPreset = -1;
    CandidateWindow* m_candSimulator = nullptr;
};

/**
 * @brief WinUI 3 Settings Window matching native Windows 11 Settings Center layout.
 */
class SettingsWindow : public fluent::windowing::Window {
    Q_OBJECT

public:
    explicit SettingsWindow(RimeEngine* engine = nullptr, QWidget* parent = nullptr);
    ~SettingsWindow() override = default;

    void selectNavigationRow(int row);
    QRectF currentNavIndicatorRect(qreal progress = -1.0) const;
    fluent::navigation::StackContentHost* contentHost() const { return m_pagesHost; }

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onNavigationChanged(int row);
    void onOrientationChanged(int index);
    void onPageSizeChanged(int value);
    void onFontSizeChanged(int value);
    void onThemeModeChanged(int index);
    void onBackdropChanged(int index);
    void onAccentColorChanged(int index);
    void onSoundToggled(bool on);
    void onSpaceCommitToggled(bool on);
    void onShowLabelToggled(bool on);
    void onShowPreeditToggled(bool on);
    void onFontFamilyChanged(int index);
    void onSwitchKeyChanged(int index);
    void onPageKeysChanged(int index);
    void onCjkSpacingToggled(bool on);
    void onSchemaChanged(int index);
    void onBackupDictClicked();
    void onSearchFilterChanged(const QString& query);
    void onDeployClicked();
    void onResetDefaultsClicked();
    void onPresetApplied(int index);

private:
    void setupUi();
    void setupTitleBar();
    void applyThemeAndBackdrop();
    void syncControlsFromConfig();

    QWidget* createNavigationPane();
    QWidget* createPersonalizationPage();
    QWidget* createInputPage();
    QWidget* createMaintenancePage();
    QWidget* createAboutPage();

    QWidget* createSectionHeader(const QString& title);
    QWidget* createPageHeader(const QString& title, const QString& subtitle);

    RimeEngine* m_engine = nullptr;

    SettingsNavPane* m_navPane = nullptr;
    fluent::navigation::StackContentHost* m_pagesHost = nullptr;
    HeroPreviewCard* m_heroPreview = nullptr;

    // Controls
    fluent::basicinput::ComboBox* m_comboOrientation = nullptr;
    fluent::basicinput::Slider* m_sliderPageSize = nullptr;
    fluent::textfields::Label* m_lblPageSizeVal = nullptr;
    fluent::basicinput::Slider* m_sliderFontSize = nullptr;
    fluent::textfields::Label* m_lblFontSizeVal = nullptr;
    fluent::basicinput::ComboBox* m_comboFontFamily = nullptr;
    fluent::basicinput::ToggleSwitch* m_switchShowLabel = nullptr;
    fluent::basicinput::ToggleSwitch* m_switchShowPreedit = nullptr;

    fluent::basicinput::ComboBox* m_comboTheme = nullptr;
    fluent::basicinput::ComboBox* m_comboBackdrop = nullptr;
    fluent::basicinput::ComboBox* m_comboAccent = nullptr;

    fluent::basicinput::ToggleSwitch* m_switchSpaceCommit = nullptr;
    fluent::basicinput::ToggleSwitch* m_switchSound = nullptr;
    fluent::basicinput::ComboBox* m_comboSwitchKey = nullptr;
    fluent::basicinput::ComboBox* m_comboPageKeys = nullptr;
    fluent::basicinput::ToggleSwitch* m_switchCjkSpacing = nullptr;

    fluent::basicinput::ComboBox* m_comboSchema = nullptr;
    fluent::basicinput::Button* m_btnDeploy = nullptr;
    fluent::textfields::Label* m_lblDeployStatus = nullptr;
    fluent::basicinput::Button* m_btnBackupDict = nullptr;
    fluent::textfields::Label* m_lblBackupStatus = nullptr;
    fluent::basicinput::Button* m_btnReset = nullptr;

    // Title bar controls
    fluent::basicinput::Button* m_btnBack = nullptr;
    fluent::textfields::Label* m_lblTitle = nullptr;
    fluent::textfields::LineEdit* m_searchBox = nullptr;
    fluent::FontIcon* m_iconSearch = nullptr;

    QVector<int> m_navHistory;
};

#endif // SETTINGS_WINDOW_H
