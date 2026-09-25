#include "globals.h"
#include "weasel_tsf.h"

HINSTANCE g_hInst = nullptr;
LONG g_cRefDll = 0;

class CClassFactory : public IClassFactory {
public:
    CClassFactory() : m_refCount(1) { DllAddRef(); }
    ~CClassFactory() { DllRelease(); }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override {
        if (!ppvObj) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppvObj = static_cast<IClassFactory*>(this);
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

    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj) override {
        if (!ppvObj) return E_POINTER;
        *ppvObj = nullptr;
        if (pUnkOuter != nullptr) return CLASS_E_NOAGGREGATION;

        CWeaselFluentTextService* pService = new CWeaselFluentTextService();
        HRESULT hr = pService->QueryInterface(riid, ppvObj);
        pService->Release();
        return hr;
    }

    STDMETHODIMP LockServer(BOOL fLock) override {
        if (fLock) DllAddRef();
        else DllRelease();
        return S_OK;
    }

private:
    LONG m_refCount;
};

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        g_hInst = hInst;
        DisableThreadLibraryCalls(hInst);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (IsEqualCLSID(rclsid, CLSID_WeaselFluentTextService) ||
        IsEqualCLSID(rclsid, CLSID_WeaselOriginalTextService)) {
        CClassFactory* pFactory = new CClassFactory();
        HRESULT hr = pFactory->QueryInterface(riid, ppv);
        pFactory->Release();
        return hr;
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow(void) {
    return (g_cRefDll <= 0) ? S_OK : S_FALSE;
}
