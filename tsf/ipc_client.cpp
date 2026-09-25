#include "ipc_client.h"

IpcClient& IpcClient::instance() {
    static IpcClient s_inst;
    return s_inst;
}

void IpcClient::tryLaunchServer() {
    DWORD now = GetTickCount();
    if (now - m_lastLaunchTick < 5000) {
        return; // Don't spam process launches
    }
    m_lastLaunchTick = now;

    // Check registry or standard path for server executable
    HKEY hKey = nullptr;
    wchar_t serverPath[MAX_PATH] = {0};
    DWORD pathSize = sizeof(serverPath);

    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExW(hKey, L"WeaselFluent", nullptr, nullptr, reinterpret_cast<LPBYTE>(serverPath), &pathSize);
        RegCloseKey(hKey);
    }

    if (serverPath[0] == L'\0') {
        wcscpy_s(serverPath, L"C:\\Users\\zheng\\.gemini\\antigravity\\scratch\\weasel-fluent-cpp-qt\\dist\\weasel-fluent\\weasel-fluent-cpp-qt.exe");
    }

    // Clean quotes if present
    std::wstring cmd = serverPath;
    if (!cmd.empty() && cmd.front() == L'"' && cmd.back() == L'"') {
        cmd = cmd.substr(1, cmd.size() - 2);
    }

    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    if (CreateProcessW(cmd.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

HANDLE IpcClient::getPipeHandle() {
    if (m_hPipe != INVALID_HANDLE_VALUE) {
        return m_hPipe;
    }

    m_hPipe = CreateFileW(
        WEASEL_PIPE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (m_hPipe == INVALID_HANDLE_VALUE) {
        tryLaunchServer();
        // Retry once after brief wait
        if (WaitNamedPipeW(WEASEL_PIPE_NAME, 100)) {
            m_hPipe = CreateFileW(
                WEASEL_PIPE_NAME,
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr
            );
        }
    }

    if (m_hPipe != INVALID_HANDLE_VALUE) {
        DWORD dwMode = PIPE_READMODE_MESSAGE;
        SetNamedPipeHandleState(m_hPipe, &dwMode, nullptr, nullptr);
    }

    return m_hPipe;
}

void IpcClient::closePipe() {
    if (m_hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hPipe);
        m_hPipe = INVALID_HANDLE_VALUE;
    }
}

bool IpcClient::sendKey(DWORD vkCode, bool isKeyDown, const RECT& caretRect, HWND hwnd, IpcKeyResponse* pResponse) {
    if (!pResponse) return false;
    memset(pResponse, 0, sizeof(IpcKeyResponse));

    IpcKeyRequest req = {};
    req.msgType = IPC_MSG_KEY;
    req.vkCode = vkCode;
    req.isKeyDown = isKeyDown ? 1 : 0;
    req.caretRect = caretRect;
    req.hwnd = reinterpret_cast<uint64_t>(hwnd);

    // Use fast CallNamedPipeW with 150ms timeout and busy retry
    DWORD bytesRead = 0;
    BOOL ok = CallNamedPipeW(
        WEASEL_PIPE_NAME,
        &req,
        sizeof(req),
        pResponse,
        sizeof(IpcKeyResponse),
        &bytesRead,
        150
    );

    if (!ok && GetLastError() == ERROR_PIPE_BUSY) {
        if (WaitNamedPipeW(WEASEL_PIPE_NAME, 100)) {
            ok = CallNamedPipeW(
                WEASEL_PIPE_NAME,
                &req,
                sizeof(req),
                pResponse,
                sizeof(IpcKeyResponse),
                &bytesRead,
                150
            );
        }
    }

    if (ok && bytesRead >= sizeof(uint32_t) * 3) {
        return true;
    }

    // Only try to launch server if the pipe truly does not exist
    if (GetLastError() == ERROR_FILE_NOT_FOUND) {
        tryLaunchServer();
    }
    return false;
}

void IpcClient::notifyActivate(HWND hwnd) {
    IpcKeyRequest req = {};
    req.msgType = IPC_MSG_ACTIVATE;
    req.hwnd = reinterpret_cast<uint64_t>(hwnd);

    DWORD bytesRead = 0;
    IpcKeyResponse resp = {};
    CallNamedPipeW(WEASEL_PIPE_NAME, &req, sizeof(req), &resp, sizeof(resp), &bytesRead, 50);
}

void IpcClient::notifyDeactivate() {
    IpcKeyRequest req = {};
    req.msgType = IPC_MSG_DEACTIVATE;

    DWORD bytesRead = 0;
    IpcKeyResponse resp = {};
    CallNamedPipeW(WEASEL_PIPE_NAME, &req, sizeof(req), &resp, sizeof(resp), &bytesRead, 50);
}
