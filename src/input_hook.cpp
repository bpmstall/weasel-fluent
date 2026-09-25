#include "input_hook.h"
#include "rime_engine.h"
#include "candidate_window.h"
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <vector>

static InputHook* s_instance = nullptr;

InputHook& InputHook::instance() {
    if (!s_instance) {
        s_instance = new InputHook();
    }
    return *s_instance;
}

InputHook::InputHook(QObject* parent) : QObject(parent) {
    s_instance = this;
}

InputHook::~InputHook() {
    uninstall();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void InputHook::setEngine(RimeEngine* engine) {
    m_engine = engine;
    if (m_engine) {
        connect(m_engine, &RimeEngine::committed, this, [this](const QString& text) {
            sendUnicodeString(text);
            if (m_candWin) {
                m_candWin->hide();
            }
            emit textCommitted(text);
        });
    }
}

void InputHook::setCandidateWindow(CandidateWindow* window) {
    m_candWin = window;
}

bool InputHook::install() {
    if (m_hook) {
        return true;
    }
    m_hook = ::SetWindowsHookExW(WH_KEYBOARD_LL, lowLevelKeyboardProc, ::GetModuleHandle(NULL), 0);
    return m_hook != nullptr;
}

void InputHook::uninstall() {
    if (m_hook) {
        ::UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }
}

void InputHook::setChineseMode(bool chinese) {
    if (m_chineseMode != chinese) {
        m_chineseMode = chinese;
        if (!m_chineseMode && m_engine) {
            m_engine->clear();
            if (m_candWin) {
                m_candWin->hide();
            }
        }
        emit chineseModeChanged(m_chineseMode);
    }
}

void InputHook::toggleChineseMode() {
    setChineseMode(!m_chineseMode);
}

void InputHook::sendUnicodeString(const QString& text) {
    if (text.isEmpty()) return;

    std::vector<INPUT> inputs;
    inputs.reserve(text.length() * 2);

    for (int i = 0; i < text.length(); ++i) {
        QChar qc = text.at(i);
        WORD utf16 = qc.unicode();

        INPUT inDown = {};
        inDown.type = INPUT_KEYBOARD;
        inDown.ki.wVk = 0;
        inDown.ki.wScan = utf16;
        inDown.ki.dwFlags = KEYEVENTF_UNICODE;
        inputs.push_back(inDown);

        INPUT inUp = {};
        inUp.type = INPUT_KEYBOARD;
        inUp.ki.wVk = 0;
        inUp.ki.wScan = utf16;
        inUp.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        inputs.push_back(inUp);
    }

    if (!inputs.empty()) {
        ::SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    }
}

QPoint InputHook::getCaretPosition() {
    GUITHREADINFO gti = {};
    gti.cbSize = sizeof(GUITHREADINFO);
    HWND fg = ::GetForegroundWindow();
    if (fg) {
        DWORD threadId = ::GetWindowThreadProcessId(fg, nullptr);
        if (::GetGUIThreadInfo(threadId, &gti)) {
            if (gti.hwndCaret && (gti.rcCaret.bottom > 0 || gti.rcCaret.right > 0)) {
                POINT pt = { gti.rcCaret.left, gti.rcCaret.bottom };
                ::ClientToScreen(gti.hwndCaret, &pt);
                return QPoint(pt.x, pt.y + 4);
            } else if (gti.hwndFocus) {
                RECT rc = {};
                ::GetWindowRect(gti.hwndFocus, &rc);
                if (rc.bottom - rc.top < 120) {
                    return QPoint(rc.left + 16, rc.bottom + 4);
                }
            }
        }
    }

    // Fallback: mouse cursor position
    POINT mousePt = {};
    ::GetCursorPos(&mousePt);
    return QPoint(mousePt.x + 12, mousePt.y + 24);
}

LRESULT CALLBACK InputHook::lowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && s_instance) {
        auto* kbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        bool isKeyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

        if (s_instance->handleKey(kbd->vkCode, isKeyDown, isKeyUp)) {
            return 1; // Intercept event
        }
    }
    return ::CallNextHookEx(nullptr, nCode, wParam, lParam);
}

bool InputHook::handleKey(DWORD vkCode, bool isKeyDown, bool isKeyUp) {
    if (!m_engine) return false;

    // Check modifiers
    bool ctrl = (::GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool alt = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    bool win = (::GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 || (::GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;

    // If Ctrl/Alt/Win is pressed, do not intercept standard shortcuts (Ctrl+C, Alt+Tab, Win+R, etc.)
    if (ctrl || alt || win) {
        m_shiftPressedAlone = false;
        return false;
    }

    // Shift key toggle
    if (vkCode == VK_SHIFT || vkCode == VK_LSHIFT || vkCode == VK_RSHIFT) {
        if (isKeyDown) {
            m_shiftPressedAlone = true;
        } else if (isKeyUp) {
            if (m_shiftPressedAlone) {
                toggleChineseMode();
            }
            m_shiftPressedAlone = false;
        }
        return false;
    }

    // Any other key clears shift-alone status
    m_shiftPressedAlone = false;

    // English mode: pass all keys
    if (!m_chineseMode) {
        return false;
    }

    RimeUiState st = m_engine->getUiState();
    bool composing = st.isComposing;

    if (composing) {
        // While composing:
        if (vkCode >= 'A' && vkCode <= 'Z') {
            if (isKeyDown) {
                char ch = static_cast<char>('a' + (vkCode - 'A'));
                m_engine->processChar(ch);
                if (m_candWin) {
                    m_candWin->move(getCaretPosition());
                    m_candWin->show();
                }
            }
            return true;
        }

        if (vkCode == VK_SPACE) {
            if (isKeyDown) {
                // Space commits highlighted candidate
                if (!st.candidates.isEmpty()) {
                    int idx = (st.highlightedIndex >= 0 && st.highlightedIndex < st.candidates.size())
                              ? st.highlightedIndex : 0;
                    m_engine->selectCandidate(idx);
                } else {
                    m_engine->processChar(' ');
                }
            }
            return true;
        }

        if (vkCode >= '1' && vkCode <= '9') {
            if (isKeyDown) {
                int idx = vkCode - '1';
                m_engine->selectCandidate(idx);
            }
            return true;
        }

        if (vkCode == VK_RETURN) {
            if (isKeyDown) {
                // Enter commits raw preedit
                QString raw = st.preedit;
                m_engine->clear();
                sendUnicodeString(raw);
                if (m_candWin) {
                    m_candWin->hide();
                }
            }
            return true;
        }

        if (vkCode == VK_BACK) {
            if (isKeyDown) {
                m_engine->processKey(RIME_KEY_BACKSPACE);
                RimeUiState newSt = m_engine->getUiState();
                if (!newSt.isComposing) {
                    if (m_candWin) {
                        m_candWin->hide();
                    }
                }
            }
            return true;
        }

        if (vkCode == VK_ESCAPE) {
            if (isKeyDown) {
                m_engine->clear();
                if (m_candWin) {
                    m_candWin->hide();
                }
            }
            return true;
        }

        if (vkCode == VK_DOWN) {
            if (isKeyDown) {
                m_engine->processKey(RIME_KEY_DOWN);
            }
            return true;
        }

        if (vkCode == VK_UP) {
            if (isKeyDown) {
                m_engine->processKey(RIME_KEY_UP);
            }
            return true;
        }

        if (vkCode == VK_TAB) {
            if (isKeyDown && m_candWin) {
                m_candWin->toggleExpanded();
            }
            return true;
        }
    } else {
        // Not composing yet:
        if (vkCode >= 'A' && vkCode <= 'Z') {
            if (isKeyDown) {
                char ch = static_cast<char>('a' + (vkCode - 'A'));
                m_engine->processChar(ch);
                if (m_candWin) {
                    m_candWin->move(getCaretPosition());
                    m_candWin->show();
                }
            }
            return true;
        }
    }

    return false;
}
