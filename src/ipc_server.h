#pragma once

#include <QObject>
#include <QThread>
#include <QString>
#include <QPoint>
#include <QRect>
#include <windows.h>
#include "../tsf/globals.h"

class RimeEngine;
class CandidateWindow;

class IpcServer : public QObject {
    Q_OBJECT
public:
    explicit IpcServer(RimeEngine* engine, CandidateWindow* candWin, QObject* parent = nullptr);
    ~IpcServer() override;

    bool start();
    void stop();

signals:
    void keyProcessed(DWORD vkCode, bool eaten, const QString& commitText);

private:
    void serverLoop();

    RimeEngine* m_engine{nullptr};
    CandidateWindow* m_candWin{nullptr};

    HANDLE m_hStopEvent{nullptr};
    HANDLE m_hThread{nullptr};
    bool m_running{false};

    QString m_lastCommit;
};
