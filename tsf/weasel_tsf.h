#pragma once

#include "globals.h"
#include <msctf.h>
#include <string>

class CWeaselFluentTextService : 
    public ITfTextInputProcessorEx,
    public ITfKeyEventSink,
    public ITfThreadMgrEventSink,
    public ITfCompositionSink {
public:
    CWeaselFluentTextService();
    ~CWeaselFluentTextService();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfTextInputProcessor
    STDMETHODIMP Activate(ITfThreadMgr* ptim, TfClientId tid) override;
    STDMETHODIMP Deactivate() override;

    // ITfTextInputProcessorEx
    STDMETHODIMP ActivateEx(ITfThreadMgr* ptim, TfClientId tid, DWORD dwFlags) override;

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL fForeground) override;
    STDMETHODIMP OnTestKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext* pic, REFGUID rguid, BOOL* pfEaten) override;

    // ITfThreadMgrEventSink
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr* pdim) override;
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* pdim) override;
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* pdimFocus, ITfDocumentMgr* pdimPrevFocus) override;
    STDMETHODIMP OnPushContext(ITfContext* pic) override;
    STDMETHODIMP OnPopContext(ITfContext* pic) override;

    // ITfCompositionSink
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition* pComposition) override;

    TfClientId getClientId() const { return m_tid; }

private:
    bool initKeySinks();
    void uninitKeySinks();
    RECT queryCaretRect(ITfContext* pic);
    void commitString(ITfContext* pic, const std::wstring& text);

    void setCompartmentOpen(bool open);

    LONG m_refCount{1};
    ITfThreadMgr* m_pThreadMgr{nullptr};
    TfClientId m_tid{TF_CLIENTID_NULL};
    DWORD m_threadMgrEventSinkCookie{TF_INVALID_COOKIE};
    DWORD m_keyEventSinkCookie{TF_INVALID_COOKIE};

    bool m_isComposing{false};
    bool m_isAsciiMode{false};
};
