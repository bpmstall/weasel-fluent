#pragma once

#include <QObject>
#include <QPoint>
#include <QString>
#include <windows.h>

class RimeEngine;
class CandidateWindow;

class InputHook : public QObject {
    Q_OBJECT
public:
    static InputHook& instance();

    void setEngine(RimeEngine* engine);
    void setCandidateWindow(CandidateWindow* window);

    bool install();
    void uninstall();
    bool isInstalled() const { return m_hook != nullptr; }

    bool isChineseMode() const { return m_chineseMode; }
    void setChineseMode(bool chinese);
    void toggleChineseMode();

    static void sendUnicodeString(const QString& text);
    static QPoint getCaretPosition();

signals:
    void chineseModeChanged(bool isChinese);
    void textCommitted(const QString& text);

private:
    explicit InputHook(QObject* parent = nullptr);
    ~InputHook() override;

    static LRESULT CALLBACK lowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    bool handleKey(DWORD vkCode, bool isKeyDown, bool isKeyUp);

    HHOOK m_hook{nullptr};
    RimeEngine* m_engine{nullptr};
    CandidateWindow* m_candWin{nullptr};

    bool m_chineseMode{true};
    bool m_shiftPressedAlone{false};
    DWORD m_lastKeyDownTime{0};
};
