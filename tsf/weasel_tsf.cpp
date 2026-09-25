#include "weasel_tsf.h"
#include "ipc_client.h"
#include <vector>

// EditSession to query exact caret bounding box from TSF
class CCaretEditSession : public ITfEditSession {
public:
    CCaretEditSession(ITfContext* pContext, RECT* pRect)
        : m_refCount(1), m_pContext(pContext), m_pRect(pRect) {
        if (m_pContext) m_pContext->AddRef();
    }
    ~CCaretEditSession() {
        if (m_pContext) m_pContext->Release();
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override {
        if (!ppvObj) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
            *ppvObj = this;
            AddRef();
            return S_OK;
        }
        *ppvObj = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_refCount); }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) delete this;
        return count;
    }

    STDMETHODIMP DoEditSession(TfEditCookie ec) override {
        if (!m_pContext || !m_pRect) return E_FAIL;
        m_pRect->left = m_pRect->top = m_pRect->right = m_pRect->bottom = 0;

        ITfContextView* pView = nullptr;
        if (SUCCEEDED(m_pContext->GetActiveView(&pView)) && pView) {
            TF_SELECTION sel = {};
            ULONG cFetched = 0;
            if (SUCCEEDED(m_pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &cFetched)) && cFetched > 0 && sel.range) {
                BOOL fClipped = FALSE;
                HRESULT hr = pView->GetTextExt(ec, sel.range, m_pRect, &fClipped);
                if (FAILED(hr) || (m_pRect->right <= m_pRect->left && m_pRect->bottom <= m_pRect->top)) {
                    // Reset to 0 so proper fallback is used
                    m_pRect->left = m_pRect->top = m_pRect->right = m_pRect->bottom = 0;
                }
                sel.range->Release();
            }
            pView->Release();
        }
        return S_OK;
    }

private:
    LONG m_refCount;
    ITfContext* m_pContext;
    RECT* m_pRect;
};

// EditSession to commit text into document via ITfInsertAtSelection or range SetText
class CCommitEditSession : public ITfEditSession {
public:
    CCommitEditSession(ITfContext* pContext, const std::wstring& text)
        : m_refCount(1), m_pContext(pContext), m_text(text) {
        if (m_pContext) m_pContext->AddRef();
    }
    ~CCommitEditSession() {
        if (m_pContext) m_pContext->Release();
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override {
        if (!ppvObj) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
            *ppvObj = this;
            AddRef();
            return S_OK;
        }
        *ppvObj = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_refCount); }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) delete this;
        return count;
    }

    STDMETHODIMP DoEditSession(TfEditCookie ec) override {
        if (!m_pContext || m_text.empty()) return E_FAIL;
        bool committed = false;

        ITfInsertAtSelection* pInsert = nullptr;
        if (SUCCEEDED(m_pContext->QueryInterface(IID_ITfInsertAtSelection, reinterpret_cast<void**>(&pInsert))) && pInsert) {
            ITfRange* pRange = nullptr;
            HRESULT hr = pInsert->InsertTextAtSelection(ec, 0, m_text.c_str(), static_cast<LONG>(m_text.length()), &pRange);
            if (SUCCEEDED(hr)) {
                committed = true;
            }
            if (pRange) pRange->Release();
            pInsert->Release();
        }

        if (!committed) {
            TF_SELECTION sel = {};
            ULONG cFetched = 0;
            if (SUCCEEDED(m_pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &cFetched)) && cFetched > 0 && sel.range) {
                HRESULT hr = sel.range->SetText(ec, 0, m_text.c_str(), static_cast<LONG>(m_text.length()));
                if (SUCCEEDED(hr)) {
                    sel.range->Collapse(ec, TF_ANCHOR_END);
                    m_pContext->SetSelection(ec, 1, &sel);
                    committed = true;
                }
                sel.range->Release();
            }
        }
        return committed ? S_OK : E_FAIL;
    }

private:
    LONG m_refCount;
    ITfContext* m_pContext;
    std::wstring m_text;
};

// CWeaselFluentTextService Implementation
CWeaselFluentTextService::CWeaselFluentTextService() {
    DllAddRef();
}

CWeaselFluentTextService::~CWeaselFluentTextService() {
    Deactivate();
    DllRelease();
}

STDMETHODIMP CWeaselFluentTextService::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_POINTER;
    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ITfTextInputProcessor) ||
        IsEqualIID(riid, IID_ITfTextInputProcessorEx)) {
        *ppvObj = static_cast<ITfTextInputProcessorEx*>(this);
    } else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
        *ppvObj = static_cast<ITfKeyEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
        *ppvObj = static_cast<ITfThreadMgrEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
        *ppvObj = static_cast<ITfCompositionSink*>(this);
    }

    if (*ppvObj) {
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CWeaselFluentTextService::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) CWeaselFluentTextService::Release() {
    ULONG count = InterlockedDecrement(&m_refCount);
    if (count == 0) {
        delete this;
    }
    return count;
}

STDMETHODIMP CWeaselFluentTextService::Activate(ITfThreadMgr* ptim, TfClientId tid) {
    return ActivateEx(ptim, tid, 0);
}

STDMETHODIMP CWeaselFluentTextService::ActivateEx(ITfThreadMgr* ptim, TfClientId tid, DWORD dwFlags) {
    m_pThreadMgr = ptim;
    if (m_pThreadMgr) {
        m_pThreadMgr->AddRef();
    }
    m_tid = tid;

    initKeySinks();
    setCompartmentOpen(!m_isAsciiMode);

    HWND fg = GetForegroundWindow();
    IpcClient::instance().notifyActivate(fg);

    return S_OK;
}

void CWeaselFluentTextService::setCompartmentOpen(bool open) {
    if (!m_pThreadMgr) return;
    ITfCompartmentMgr* pCompMgr = nullptr;
    if (SUCCEEDED(m_pThreadMgr->QueryInterface(IID_ITfCompartmentMgr, reinterpret_cast<void**>(&pCompMgr))) && pCompMgr) {
        ITfCompartment* pComp = nullptr;
        if (SUCCEEDED(pCompMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &pComp)) && pComp) {
            VARIANT var;
            var.vt = VT_I4;
            var.lVal = open ? 1 : 0;
            pComp->SetValue(m_tid, &var);
            pComp->Release();
        }
        pCompMgr->Release();
    }
}

STDMETHODIMP CWeaselFluentTextService::Deactivate() {
    uninitKeySinks();

    IpcClient::instance().notifyDeactivate();

    if (m_pThreadMgr) {
        m_pThreadMgr->Release();
        m_pThreadMgr = nullptr;
    }
    m_tid = TF_CLIENTID_NULL;
    return S_OK;
}

bool CWeaselFluentTextService::initKeySinks() {
    if (!m_pThreadMgr) return false;

    // 1. ThreadMgr Event Sink
    ITfSource* pSource = nullptr;
    if (SUCCEEDED(m_pThreadMgr->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&pSource))) && pSource) {
        pSource->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink*>(this), &m_threadMgrEventSinkCookie);
        pSource->Release();
    }

    // 2. Key Event Sink
    ITfKeystrokeMgr* pKeyMgr = nullptr;
    if (SUCCEEDED(m_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, reinterpret_cast<void**>(&pKeyMgr))) && pKeyMgr) {
        pKeyMgr->AdviseKeyEventSink(m_tid, static_cast<ITfKeyEventSink*>(this), TRUE);
        pKeyMgr->Release();
    }

    return true;
}

void CWeaselFluentTextService::uninitKeySinks() {
    if (!m_pThreadMgr) return;

    if (m_threadMgrEventSinkCookie != TF_INVALID_COOKIE) {
        ITfSource* pSource = nullptr;
        if (SUCCEEDED(m_pThreadMgr->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&pSource))) && pSource) {
            pSource->UnadviseSink(m_threadMgrEventSinkCookie);
            pSource->Release();
        }
        m_threadMgrEventSinkCookie = TF_INVALID_COOKIE;
    }

    ITfKeystrokeMgr* pKeyMgr = nullptr;
    if (SUCCEEDED(m_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, reinterpret_cast<void**>(&pKeyMgr))) && pKeyMgr) {
        pKeyMgr->UnadviseKeyEventSink(m_tid);
        pKeyMgr->Release();
    }
}

RECT CWeaselFluentTextService::queryCaretRect(ITfContext* pic) {
    RECT rc = {0, 0, 0, 0};
    if (!pic) return rc;

    CCaretEditSession* pSession = new CCaretEditSession(pic, &rc);
    HRESULT hrSession = S_OK;
    pic->RequestEditSession(m_tid, pSession, TF_ES_SYNC | TF_ES_READ, &hrSession);
    pSession->Release();

    // Fallback: If TSF returned empty or degenerate rect (e.g. at document start or legacy app)
    if (rc.left == 0 && rc.right == 0 && rc.top == 0 && rc.bottom == 0) {
        GUITHREADINFO gti = {sizeof(gti)};
        HWND fg = GetForegroundWindow();
        if (fg && GetGUIThreadInfo(GetWindowThreadProcessId(fg, nullptr), &gti)) {
            if (gti.hwndCaret && (gti.rcCaret.right > 0 || gti.rcCaret.bottom > 0)) {
                POINT pt = {gti.rcCaret.left, gti.rcCaret.bottom};
                ClientToScreen(gti.hwndCaret, &pt);
                rc.left = pt.x;
                rc.top = pt.y - 16;
                rc.right = pt.x + 2;
                rc.bottom = pt.y;
            }
        }
        if (rc.left == 0 && rc.right == 0) {
            POINT cur = {};
            GetCursorPos(&cur);
            rc.left = cur.x;
            rc.top = cur.y - 16;
            rc.right = cur.x + 2;
            rc.bottom = cur.y;
        }
    }
    return rc;
}

void CWeaselFluentTextService::commitString(ITfContext* pic, const std::wstring& text) {
    if (text.empty()) return;

    bool committed = false;
    if (pic) {
        CCommitEditSession* pSession = new CCommitEditSession(pic, text);
        HRESULT hrSession = S_OK;
        HRESULT hr = pic->RequestEditSession(m_tid, pSession, TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
        if (SUCCEEDED(hr) && SUCCEEDED(hrSession)) {
            committed = true;
        }
        pSession->Release();
    }

    if (!committed) {
        // Fallback: SendInput with KEYEVENTF_UNICODE ensures committed text is never lost
        std::vector<INPUT> inputs;
        inputs.reserve(text.length() * 2);
        for (wchar_t ch : text) {
            INPUT down = {};
            down.type = INPUT_KEYBOARD;
            down.ki.wScan = ch;
            down.ki.dwFlags = KEYEVENTF_UNICODE;
            inputs.push_back(down);

            INPUT up = down;
            up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
            inputs.push_back(up);
        }
        if (!inputs.empty()) {
            SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
        }
    }
}

// ITfKeyEventSink Implementation
STDMETHODIMP CWeaselFluentTextService::OnSetFocus(BOOL fForeground) {
    return S_OK;
}

STDMETHODIMP CWeaselFluentTextService::OnTestKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) return E_POINTER;

    if (m_isAsciiMode) {
        *pfEaten = FALSE;
        return S_OK;
    }

    DWORD vk = static_cast<DWORD>(wParam);

    // If composing, intercept all editing & navigation keys
    if (m_isComposing) {
        if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9') ||
            vk == VK_SPACE || vk == VK_RETURN || vk == VK_BACK || vk == VK_ESCAPE ||
            vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT ||
            vk == VK_TAB || vk == VK_PRIOR || vk == VK_NEXT ||
            vk == VK_OEM_MINUS || vk == VK_OEM_PLUS || vk == VK_OEM_4 || vk == VK_OEM_6) {
            *pfEaten = TRUE;
            return S_OK;
        }
    } else {
        // Not composing: intercept letters 'A'..'Z' to begin composition
        if (vk >= 'A' && vk <= 'Z') {
            // Check modifier keys: Ctrl, Alt, Win bypass IME
            bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            bool win = (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 || (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;
            if (!ctrl && !alt && !win) {
                *pfEaten = TRUE;
                return S_OK;
            }
        }
    }

    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CWeaselFluentTextService::OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) return E_POINTER;

    DWORD vk = static_cast<DWORD>(wParam);

    // Check modifiers
    bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    bool win = (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 || (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;
    if (ctrl || alt || win) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // Shift key toggle
    if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // Query exact caret coordinates from TSF context
    RECT caretRect = queryCaretRect(pic);
    HWND fg = GetForegroundWindow();

    IpcKeyResponse resp = {};
    bool ok = IpcClient::instance().sendKey(vk, true, caretRect, fg, &resp);

    if (ok) {
        if (resp.commitLen > 0) {
            std::wstring commitStr(resp.commitText, resp.commitLen);
            commitString(pic, commitStr);
        }
        m_isComposing = (resp.isComposing != 0);
        bool newAscii = (resp.isAsciiMode != 0);
        if (newAscii != m_isAsciiMode) {
            m_isAsciiMode = newAscii;
            setCompartmentOpen(!m_isAsciiMode);
        }
        *pfEaten = (resp.eaten != 0);
        return S_OK;
    }

    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CWeaselFluentTextService::OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) return E_POINTER;
    *pfEaten = m_isComposing ? TRUE : FALSE;
    return S_OK;
}

STDMETHODIMP CWeaselFluentTextService::OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) return E_POINTER;

    DWORD vk = static_cast<DWORD>(wParam);
    if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) {
        RECT r = {};
        IpcKeyResponse resp = {};
        if (IpcClient::instance().sendKey(VK_SHIFT, false, r, GetForegroundWindow(), &resp)) {
            bool newAscii = (resp.isAsciiMode != 0);
            if (newAscii != m_isAsciiMode) {
                m_isAsciiMode = newAscii;
                setCompartmentOpen(!m_isAsciiMode);
            }
            m_isComposing = (resp.isComposing != 0);
        }
        *pfEaten = FALSE;
        return S_OK;
    }

    *pfEaten = m_isComposing ? TRUE : FALSE;
    return S_OK;
}

STDMETHODIMP CWeaselFluentTextService::OnPreservedKey(ITfContext* pic, REFGUID rguid, BOOL* pfEaten) {
    if (pfEaten) *pfEaten = FALSE;
    return S_OK;
}

// ITfThreadMgrEventSink Implementation
STDMETHODIMP CWeaselFluentTextService::OnInitDocumentMgr(ITfDocumentMgr* pdim) { return S_OK; }
STDMETHODIMP CWeaselFluentTextService::OnUninitDocumentMgr(ITfDocumentMgr* pdim) { return S_OK; }
STDMETHODIMP CWeaselFluentTextService::OnSetFocus(ITfDocumentMgr* pdimFocus, ITfDocumentMgr* pdimPrevFocus) { return S_OK; }
STDMETHODIMP CWeaselFluentTextService::OnPushContext(ITfContext* pic) { return S_OK; }
STDMETHODIMP CWeaselFluentTextService::OnPopContext(ITfContext* pic) { return S_OK; }

// ITfCompositionSink Implementation
STDMETHODIMP CWeaselFluentTextService::OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition* pComposition) {
    m_isComposing = false;
    return S_OK;
}
