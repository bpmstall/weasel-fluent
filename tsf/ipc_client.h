#pragma once

#include "globals.h"

class IpcClient {
public:
    static IpcClient& instance();

    bool sendKey(DWORD vkCode, bool isKeyDown, const RECT& caretRect, HWND hwnd, IpcKeyResponse* pResponse);
    void notifyActivate(HWND hwnd);
    void notifyDeactivate();

private:
    IpcClient() = default;
    ~IpcClient() = default;

    HANDLE getPipeHandle();
    void closePipe();
    void tryLaunchServer();

    HANDLE m_hPipe{INVALID_HANDLE_VALUE};
    DWORD m_lastLaunchTick{0};
};
