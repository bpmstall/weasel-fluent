#include "ipc_server.h"
#include "rime_engine.h"
#include "candidate_window.h"
#include <QCoreApplication>
#include <QDebug>
#include <iostream>
#include <sddl.h>

IpcServer::IpcServer(RimeEngine* engine, CandidateWindow* candWin, QObject* parent)
    : QObject(parent), m_engine(engine), m_candWin(candWin) {
    if (m_engine) {
        connect(m_engine, &RimeEngine::committed, this, [this](const QString& text) {
            m_lastCommit = text;
        });
    }
}

IpcServer::~IpcServer() {
    stop();
}

bool IpcServer::start() {
    if (m_running) return true;

    m_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    m_running = true;

    m_hThread = CreateThread(
        nullptr,
        0,
        [](LPVOID param) -> DWORD {
            auto* self = static_cast<IpcServer*>(param);
            self->serverLoop();
            return 0;
        },
        this,
        0,
        nullptr
    );

    return (m_hThread != nullptr);
}

void IpcServer::stop() {
    if (!m_running) return;
    m_running = false;

    if (m_hStopEvent) {
        SetEvent(m_hStopEvent);
    }

    // Ping the pipe to unblock any pending ConnectNamedPipe
    HANDLE hPing = CreateFileW(WEASEL_PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hPing != INVALID_HANDLE_VALUE) {
        CloseHandle(hPing);
    }

    if (m_hThread) {
        WaitForSingleObject(m_hThread, 1000);
        CloseHandle(m_hThread);
        m_hThread = nullptr;
    }

    if (m_hStopEvent) {
        CloseHandle(m_hStopEvent);
        m_hStopEvent = nullptr;
    }
}

void IpcServer::serverLoop() {
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, FALSE };
    PSECURITY_DESCRIPTOR pSD = nullptr;
    if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:(A;;GRGW;;;WD)(A;;GRGW;;;AC)S:(ML;;NW;;;LW)",
            SDDL_REVISION_1,
            &pSD,
            nullptr)) {
        sa.lpSecurityDescriptor = pSD;
    }

    while (m_running) {
        HANDLE hPipe = CreateNamedPipeW(
            WEASEL_PIPE_NAME,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            sizeof(IpcKeyResponse) * 2,
            sizeof(IpcKeyRequest) * 2,
            200,
            sa.lpSecurityDescriptor ? &sa : nullptr
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            Sleep(50);
            continue;
        }

        BOOL connected = ConnectNamedPipe(hPipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (connected && m_running) {
            IpcKeyRequest req = {};
            DWORD bytesRead = 0;
            BOOL readOk = ReadFile(hPipe, &req, sizeof(req), &bytesRead, nullptr);

            if (readOk && bytesRead >= sizeof(IpcKeyRequest)) {
                IpcKeyResponse resp = {};

                if (req.msgType == IPC_MSG_ACTIVATE) {
                    resp.eaten = 1;
                } else if (req.msgType == IPC_MSG_DEACTIVATE) {
                    QMetaObject::invokeMethod(this, [this]() {
                        if (m_candWin) m_candWin->hide();
                        if (m_engine) m_engine->clear();
                    }, Qt::BlockingQueuedConnection);
                    resp.eaten = 1;
                } else if (req.msgType == IPC_MSG_KEY) {
                    // Synchronously execute on Qt main/GUI thread
                    QMetaObject::invokeMethod(this, [this, &req, &resp]() {
                        m_lastCommit.clear();

                        DWORD vk = req.vkCode;
                        bool isKeyDown = (req.isKeyDown != 0);

                        bool asciiMode = m_engine ? m_engine->isAsciiMode() : false;
                        resp.isAsciiMode = asciiMode ? 1 : 0;

                        RimeUiState st = m_engine ? m_engine->getUiState() : RimeUiState{};
                        bool composing = st.isComposing;

                        if (isKeyDown) {
                            if (asciiMode) {
                                resp.eaten = 0;
                                resp.isComposing = 0;
                                return;
                            }
                            if (composing) {
                                if (vk >= 'A' && vk <= 'Z') {
                                    if (m_candWin && m_candWin->isExpanded()) {
                                        int candIdx = 10 + (vk - 'A');
                                        if (candIdx < st.candidates.size()) {
                                            m_engine->selectCandidate(candIdx);
                                            resp.eaten = 1;
                                            return;
                                        }
                                    }
                                    char ch = static_cast<char>('a' + (vk - 'A'));
                                    m_engine->processChar(ch);
                                    resp.eaten = 1;
                                } else if (vk == VK_SPACE) {
                                    if (!st.candidates.isEmpty()) {
                                        int idx = (st.highlightedIndex >= 0 && st.highlightedIndex < st.candidates.size())
                                                  ? st.highlightedIndex : 0;
                                        m_engine->selectCandidate(idx);
                                    } else {
                                        m_engine->processChar(' ');
                                    }
                                    resp.eaten = 1;
                                } else if (vk >= '1' && vk <= '9') {
                                    int idx = vk - '1';
                                    m_engine->selectCandidate(idx);
                                    resp.eaten = 1;
                                } else if (vk == '0') {
                                    m_engine->selectCandidate(9);
                                    resp.eaten = 1;
                                } else if (vk == VK_RETURN) {
                                    m_lastCommit = st.preedit;
                                    m_engine->clear();
                                    if (m_candWin) m_candWin->hide();
                                    resp.eaten = 1;
                                } else if (vk == VK_BACK) {
                                    m_engine->processKey(RIME_KEY_BACKSPACE);
                                    resp.eaten = 1;
                                } else if (vk == VK_ESCAPE) {
                                    if (m_candWin && m_candWin->isExpanded()) {
                                        m_candWin->toggleExpanded();
                                    } else {
                                        m_engine->clear();
                                        if (m_candWin) m_candWin->hide();
                                    }
                                    resp.eaten = 1;
                                } else if (vk == VK_TAB) {
                                    if (m_candWin) m_candWin->toggleExpanded();
                                    resp.eaten = 1;
                                } else if (vk == VK_UP) {
                                    m_engine->processKey(RIME_KEY_UP);
                                    resp.eaten = 1;
                                } else if (vk == VK_DOWN) {
                                    m_engine->processKey(RIME_KEY_DOWN);
                                    resp.eaten = 1;
                                } else if (vk == VK_LEFT) {
                                    m_engine->processKey(RIME_KEY_LEFT);
                                    resp.eaten = 1;
                                } else if (vk == VK_RIGHT) {
                                    m_engine->processKey(RIME_KEY_RIGHT);
                                    resp.eaten = 1;
                                } else if (vk == VK_OEM_MINUS || vk == VK_PRIOR || vk == VK_OEM_4) {
                                    m_engine->processKey(RIME_KEY_PAGE_UP);
                                    resp.eaten = 1;
                                } else if (vk == VK_OEM_PLUS || vk == VK_NEXT || vk == VK_OEM_6) {
                                    m_engine->processKey(RIME_KEY_PAGE_DOWN);
                                    resp.eaten = 1;
                                }
                            } else {
                                // Not composing: letter 'A'..'Z' starts composition
                                if (vk >= 'A' && vk <= 'Z') {
                                    char ch = static_cast<char>('a' + (vk - 'A'));
                                    m_engine->processChar(ch);
                                    resp.eaten = 1;
                                }
                            }

                            // Update Caret Position & Candidate Window visibility
                            RimeUiState newSt = m_engine ? m_engine->getUiState() : RimeUiState{};
                            resp.isComposing = newSt.isComposing ? 1 : 0;

                            if (m_candWin) {
                                if (newSt.isComposing && !newSt.candidates.isEmpty()) {
                                    // Use TSF pixel-perfect caret coordinates!
                                    int cx = req.caretRect.left;
                                    int cy = req.caretRect.bottom > 0 ? req.caretRect.bottom + 4 : req.caretRect.top + 20;
                                    m_candWin->moveToPosition(QPoint(cx, cy));
                                    m_candWin->show();
                                } else {
                                    m_candWin->hide();
                                }
                            }

                            // If text was committed
                            if (!m_lastCommit.isEmpty()) {
                                int len = qMin(static_cast<int>(m_lastCommit.length()), 127);
                                resp.commitLen = len;
                                for (int i = 0; i < len; ++i) {
                                    resp.commitText[i] = m_lastCommit.at(i).unicode();
                                }
                                resp.commitText[len] = L'\0';
                                if (m_candWin) {
                                    if (m_candWin->isExpanded()) m_candWin->toggleExpanded();
                                    m_candWin->hide();
                                }
                            }
                        } else {
                            // KeyUp event
                            if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) {
                                if (m_engine) {
                                    m_engine->setAsciiMode(!m_engine->isAsciiMode());
                                }
                            }
                            resp.isAsciiMode = (m_engine && m_engine->isAsciiMode()) ? 1 : 0;
                            resp.eaten = composing ? 1 : 0;
                            resp.isComposing = composing ? 1 : 0;
                        }
                    }, Qt::BlockingQueuedConnection);
                }

                DWORD bytesWritten = 0;
                WriteFile(hPipe, &resp, sizeof(resp), &bytesWritten, nullptr);
            }
        }

        FlushFileBuffers(hPipe);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }

    if (pSD) {
        LocalFree(pSD);
    }
}
