#include "config.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSettings>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>
#include <QFileSystemWatcher>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#ifdef small
#undef small
#endif
#endif

AppConfig& AppConfig::instance() {
    static AppConfig s_instance;
    return s_instance;
}

AppConfig::AppConfig(QObject* parent) : QObject(parent) {
    load();
    setupWatcher();
}

QString AppConfig::configPath() const {
    if (!m_customPath.isEmpty()) {
        return m_customPath;
    }
    QString appDir = QCoreApplication::applicationDirPath();
    QString localFile = appDir + QStringLiteral("/config.json");
    if (QFileInfo::exists(localFile)) {
        return localFile;
    }
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dataDir.isEmpty()) {
        dataDir = appDir;
    }
    QDir().mkpath(dataDir);
    return dataDir + QStringLiteral("/config.json");
}

void AppConfig::setupWatcher() {
    if (!m_watcher) {
        m_watcher = new QFileSystemWatcher(this);
        connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString& path) {
            if (m_isSaving) return;
            // Delay slightly because QSaveFile replaces the file inode on Windows
            QTimer::singleShot(80, this, [this, path]() {
                if (QFileInfo::exists(path)) {
                    if (!m_watcher->files().contains(path)) {
                        m_watcher->addPath(path);
                    }
                }
                load(path);
                emit configChanged();
            });
        });
    }

    QString target = configPath();
    if (QFileInfo::exists(target) && !m_watcher->files().contains(target)) {
        m_watcher->addPath(target);
    }
}

void AppConfig::load(const QString& path) {
    if (!path.isEmpty()) {
        m_customPath = path;
    }
    QString targetPath = configPath();
    QFile file(targetPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return;
    }

    QJsonObject obj = doc.object();
    if (obj.contains(QStringLiteral("orientation")))
        m_orientation = obj.value(QStringLiteral("orientation")).toString(m_orientation);
    if (obj.contains(QStringLiteral("page_size")))
        m_pageSize = obj.value(QStringLiteral("page_size")).toInt(m_pageSize);
    if (obj.contains(QStringLiteral("theme_mode")))
        m_themeMode = obj.value(QStringLiteral("theme_mode")).toString(m_themeMode);
    if (obj.contains(QStringLiteral("backdrop")))
        m_backdrop = obj.value(QStringLiteral("backdrop")).toString(m_backdrop);
    if (obj.contains(QStringLiteral("font_size")))
        m_fontSize = obj.value(QStringLiteral("font_size")).toInt(m_fontSize);
    if (obj.contains(QStringLiteral("corner_radius")))
        m_cornerRadius = obj.value(QStringLiteral("corner_radius")).toInt(m_cornerRadius);
    if (obj.contains(QStringLiteral("accent_color")))
        m_accentColor = obj.value(QStringLiteral("accent_color")).toString(m_accentColor);
    if (obj.contains(QStringLiteral("sound_enabled")))
        m_soundEnabled = obj.value(QStringLiteral("sound_enabled")).toBool(m_soundEnabled);
    if (obj.contains(QStringLiteral("space_commit_first")))
        m_spaceCommitFirst = obj.value(QStringLiteral("space_commit_first")).toBool(m_spaceCommitFirst);
    if (obj.contains(QStringLiteral("show_label")))
        m_showLabel = obj.value(QStringLiteral("show_label")).toBool(m_showLabel);
    if (obj.contains(QStringLiteral("show_preedit")))
        m_showPreedit = obj.value(QStringLiteral("show_preedit")).toBool(m_showPreedit);
    if (obj.contains(QStringLiteral("font_family")))
        m_fontFamily = obj.value(QStringLiteral("font_family")).toString(m_fontFamily);
    if (obj.contains(QStringLiteral("switch_key")))
        m_switchKey = obj.value(QStringLiteral("switch_key")).toString(m_switchKey);
    if (obj.contains(QStringLiteral("page_keys")))
        m_pageKeys = obj.value(QStringLiteral("page_keys")).toString(m_pageKeys);
    if (obj.contains(QStringLiteral("schema")))
        m_schema = obj.value(QStringLiteral("schema")).toString(m_schema);
    if (obj.contains(QStringLiteral("cjk_spacing")))
        m_cjkSpacing = obj.value(QStringLiteral("cjk_spacing")).toBool(m_cjkSpacing);
}

bool AppConfig::save(const QString& path) const {
    m_isSaving = true;
    if (!path.isEmpty()) {
        m_customPath = path;
    }
    QString targetPath = configPath();
    QJsonObject obj;
    obj[QStringLiteral("orientation")] = m_orientation;
    obj[QStringLiteral("page_size")] = m_pageSize;
    obj[QStringLiteral("theme_mode")] = m_themeMode;
    obj[QStringLiteral("backdrop")] = m_backdrop;
    obj[QStringLiteral("font_size")] = m_fontSize;
    obj[QStringLiteral("corner_radius")] = m_cornerRadius;
    obj[QStringLiteral("accent_color")] = m_accentColor;
    obj[QStringLiteral("sound_enabled")] = m_soundEnabled;
    obj[QStringLiteral("space_commit_first")] = m_spaceCommitFirst;
    obj[QStringLiteral("show_label")] = m_showLabel;
    obj[QStringLiteral("show_preedit")] = m_showPreedit;
    obj[QStringLiteral("font_family")] = m_fontFamily;
    obj[QStringLiteral("switch_key")] = m_switchKey;
    obj[QStringLiteral("page_keys")] = m_pageKeys;
    obj[QStringLiteral("schema")] = m_schema;
    obj[QStringLiteral("cjk_spacing")] = m_cjkSpacing;

    QJsonDocument doc(obj);
    QSaveFile saveFile(targetPath);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        m_isSaving = false;
        return false;
    }
    saveFile.write(doc.toJson(QJsonDocument::Indented));
    bool ok = saveFile.commit();

    if (m_watcher && QFileInfo::exists(targetPath) && !m_watcher->files().contains(targetPath)) {
        m_watcher->addPath(targetPath);
    }

    m_isSaving = false;
    return ok;
}

QColor AppConfig::systemAccentColor() {
#ifdef Q_OS_WIN
    bool isDark = systemThemeIsDark();

    // 1. Preferred: Read Windows 11 AccentPalette from Explorer\Accent
    // In Dark Mode, Windows uses Color 1 (Light 2: luminous soft accent, e.g. #ED958A)
    // In Light Mode, Windows uses Color 3 (Base: saturated deep accent, e.g. #D44340)
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        BYTE palette[32] = {0};
        DWORD size = sizeof(palette);
        DWORD type = 0;
        if (RegQueryValueExW(hKey, L"AccentPalette", nullptr, &type, palette, &size) == ERROR_SUCCESS && size >= 32) {
            int offset = isDark ? 4 : 12;
            int r = palette[offset];
            int g = palette[offset + 1];
            int b = palette[offset + 2];
            RegCloseKey(hKey);
            if (r != 0 || g != 0 || b != 0) {
                return QColor(r, g, b);
            }
        } else {
            RegCloseKey(hKey);
        }
    }

    // 2. Query Windows DWM AccentColor (ABGR)
    QSettings dwm(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\DWM"), QSettings::NativeFormat);
    if (dwm.contains(QStringLiteral("AccentColor"))) {
        quint32 abgr = dwm.value(QStringLiteral("AccentColor")).toUInt();
        int r = abgr & 0xFF;
        int g = (abgr >> 8) & 0xFF;
        int b = (abgr >> 16) & 0xFF;
        if (r != 0 || g != 0 || b != 0) {
            return QColor(r, g, b);
        }
    }
    // 3. Query Windows DWM ColorizationColor (ARGB)
    if (dwm.contains(QStringLiteral("ColorizationColor"))) {
        quint32 argb = dwm.value(QStringLiteral("ColorizationColor")).toUInt();
        int r = (argb >> 16) & 0xFF;
        int g = (argb >> 8) & 0xFF;
        int b = argb & 0xFF;
        if (r != 0 || g != 0 || b != 0) {
            return QColor(r, g, b);
        }
    }
#endif
    // 4. Fallback to Qt Accent Palette
    QColor qAccent = QGuiApplication::palette().color(QPalette::Accent);
    if (qAccent.isValid() && qAccent != Qt::black) {
        return qAccent;
    }
    return QColor(QStringLiteral("#0078D4")); // Win11 default blue
}

bool AppConfig::systemThemeIsDark() {
#ifdef Q_OS_WIN
    QSettings personalize(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"), QSettings::NativeFormat);
    if (personalize.contains(QStringLiteral("AppsUseLightTheme"))) {
        return personalize.value(QStringLiteral("AppsUseLightTheme")).toInt() == 0;
    }
#endif
    if (QGuiApplication::styleHints()) {
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    }
    return true; // Default dark
}

QColor AppConfig::effectiveAccentColor() const {
    if (m_accentColor.isEmpty() ||
        m_accentColor.compare(QStringLiteral("auto"), Qt::CaseInsensitive) == 0 ||
        m_accentColor.compare(QStringLiteral("system"), Qt::CaseInsensitive) == 0) {
        return systemAccentColor();
    }
    // Popular presets matching Rust & Fluent presets
    QString lower = m_accentColor.trimmed().toLower();
    if (lower == QStringLiteral("teal")) return QColor(QStringLiteral("#009999"));
    if (lower == QStringLiteral("blue")) return QColor(QStringLiteral("#0078D4"));
    if (lower == QStringLiteral("purple")) return QColor(QStringLiteral("#8764B8"));
    if (lower == QStringLiteral("green")) return QColor(QStringLiteral("#107C41"));
    if (lower == QStringLiteral("orange")) return QColor(QStringLiteral("#CA5010"));
    if (lower == QStringLiteral("red")) return QColor(QStringLiteral("#D13438"));

    QColor c(m_accentColor);
    return c.isValid() ? c : systemAccentColor();
}

bool AppConfig::isDarkTheme() const {
    if (m_themeMode.compare(QStringLiteral("system"), Qt::CaseInsensitive) == 0) {
        return systemThemeIsDark();
    }
    return m_themeMode.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0;
}

void AppConfig::setOrientation(const QString& v) {
    if (m_orientation != v) {
        m_orientation = v;
        save();
        emit configChanged();
    }
}

void AppConfig::setPageSize(int v) {
    if (m_pageSize != v) {
        m_pageSize = v;
        save();
        emit configChanged();
    }
}

void AppConfig::setThemeMode(const QString& v) {
    if (m_themeMode != v) {
        m_themeMode = v;
        save();
        emit configChanged();
    }
}

void AppConfig::setBackdrop(const QString& v) {
    if (m_backdrop != v) {
        m_backdrop = v;
        save();
        emit configChanged();
    }
}

void AppConfig::setFontSize(int v) {
    if (m_fontSize != v) {
        m_fontSize = v;
        save();
        emit configChanged();
    }
}

void AppConfig::setCornerRadius(int v) {
    if (m_cornerRadius != v) {
        m_cornerRadius = v;
        save();
        emit configChanged();
    }
}

void AppConfig::setAccentColor(const QString& v) {
    if (m_accentColor != v) {
        m_accentColor = v;
        save();
        emit configChanged();
    }
}

void AppConfig::setSoundEnabled(bool v) {
    if (m_soundEnabled != v) {
        m_soundEnabled = v;
        save();
        emit configChanged();
    }
}

void AppConfig::setSpaceCommitFirst(bool v) {
    if (m_spaceCommitFirst != v) {
        m_spaceCommitFirst = v;
        save();
        emit configChanged();
    }
}

void AppConfig::setShowLabel(bool v) {
    if (m_showLabel != v) {
        m_showLabel = v;
        save();
        emit configChanged();
    }
}

void AppConfig::setShowPreedit(bool v) {
    if (m_showPreedit != v) {
        m_showPreedit = v;
        save();
        emit configChanged();
    }
}

void AppConfig::setFontFamily(const QString& v) {
    if (m_fontFamily != v) {
        m_fontFamily = v;
        save();
        emit configChanged();
    }
}

void AppConfig::setSwitchKey(const QString& v) {
    if (m_switchKey != v) {
        m_switchKey = v;
        save();
        emit configChanged();
    }
}

void AppConfig::setPageKeys(const QString& v) {
    if (m_pageKeys != v) {
        m_pageKeys = v;
        save();
        emit configChanged();
    }
}

void AppConfig::setSchema(const QString& v) {
    if (m_schema != v) {
        m_schema = v;
        save();
        emit configChanged();
    }
}

void AppConfig::setCjkSpacing(bool v) {
    if (m_cjkSpacing != v) {
        m_cjkSpacing = v;
        save();
        emit configChanged();
    }
}

