#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <QObject>
#include <QString>
#include <QColor>
#include <QJsonObject>

class QFileSystemWatcher;

class AppConfig : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString orientation READ orientation WRITE setOrientation NOTIFY configChanged)
    Q_PROPERTY(int pageSize READ pageSize WRITE setPageSize NOTIFY configChanged)
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY configChanged)
    Q_PROPERTY(QString backdrop READ backdrop WRITE setBackdrop NOTIFY configChanged)
    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY configChanged)
    Q_PROPERTY(int cornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY configChanged)
    Q_PROPERTY(QString accentColor READ accentColor WRITE setAccentColor NOTIFY configChanged)
    Q_PROPERTY(bool soundEnabled READ soundEnabled WRITE setSoundEnabled NOTIFY configChanged)
    Q_PROPERTY(bool spaceCommitFirst READ spaceCommitFirst WRITE setSpaceCommitFirst NOTIFY configChanged)
    Q_PROPERTY(bool showLabel READ showLabel WRITE setShowLabel NOTIFY configChanged)
    Q_PROPERTY(bool showPreedit READ showPreedit WRITE setShowPreedit NOTIFY configChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY configChanged)
    Q_PROPERTY(QString switchKey READ switchKey WRITE setSwitchKey NOTIFY configChanged)
    Q_PROPERTY(QString pageKeys READ pageKeys WRITE setPageKeys NOTIFY configChanged)
    Q_PROPERTY(QString schema READ schema WRITE setSchema NOTIFY configChanged)
    Q_PROPERTY(bool cjkSpacing READ cjkSpacing WRITE setCjkSpacing NOTIFY configChanged)

public:
    static AppConfig& instance();

    void load(const QString& path = QString());
    bool save(const QString& path = QString()) const;

    QString orientation() const { return m_orientation; }
    void setOrientation(const QString& v);

    int pageSize() const { return m_pageSize; }
    void setPageSize(int v);

    QString themeMode() const { return m_themeMode; }
    void setThemeMode(const QString& v);

    QString backdrop() const { return m_backdrop; }
    void setBackdrop(const QString& v);

    int fontSize() const { return m_fontSize; }
    void setFontSize(int v);

    int cornerRadius() const { return m_cornerRadius; }
    void setCornerRadius(int v);

    QString accentColor() const { return m_accentColor; }
    void setAccentColor(const QString& v);

    bool soundEnabled() const { return m_soundEnabled; }
    void setSoundEnabled(bool v);

    bool spaceCommitFirst() const { return m_spaceCommitFirst; }
    void setSpaceCommitFirst(bool v);

    bool showLabel() const { return m_showLabel; }
    void setShowLabel(bool v);

    bool showPreedit() const { return m_showPreedit; }
    void setShowPreedit(bool v);

    QString fontFamily() const { return m_fontFamily; }
    void setFontFamily(const QString& v);

    QString switchKey() const { return m_switchKey; }
    void setSwitchKey(const QString& v);

    QString pageKeys() const { return m_pageKeys; }
    void setPageKeys(const QString& v);

    QString schema() const { return m_schema; }
    void setSchema(const QString& v);

    bool cjkSpacing() const { return m_cjkSpacing; }
    void setCjkSpacing(bool v);

    QString configPath() const;

    // Automatic Windows System Theme & Accent Color detection
    static QColor systemAccentColor();
    static bool systemThemeIsDark();

    // Effective resolved values
    QColor effectiveAccentColor() const;
    bool isDarkTheme() const;

signals:
    void configChanged();

private:
    explicit AppConfig(QObject* parent = nullptr);
    ~AppConfig() override = default;

    void setupWatcher();

    QString m_orientation = QStringLiteral("horizontal");
    int m_pageSize = 5;
    QString m_themeMode = QStringLiteral("dark");
    QString m_backdrop = QStringLiteral("mica");
    int m_fontSize = 12;
    int m_cornerRadius = 7;
    QString m_accentColor = QStringLiteral("auto");
    bool m_soundEnabled = false;
    bool m_spaceCommitFirst = true;
    bool m_showLabel = true;
    bool m_showPreedit = false;
    QString m_fontFamily = QStringLiteral("Microsoft YaHei UI");
    QString m_switchKey = QStringLiteral("Shift");
    QString m_pageKeys = QStringLiteral("comma_period");
    QString m_schema = QStringLiteral("rime_ice");
    bool m_cjkSpacing = false;

    mutable QString m_customPath;
    mutable bool m_isSaving = false;
    QFileSystemWatcher* m_watcher = nullptr;
};

#endif // APP_CONFIG_H
