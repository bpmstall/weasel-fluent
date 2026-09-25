#include "globals.h"
#include <msctf.h>
#include <shlwapi.h>

static const wchar_t* CLSID_STRING = L"{D7B1C618-9F3E-4E7B-B96E-5C13D1A5B4C2}";

static bool setRegString(HKEY hRoot, const wchar_t* subKey, const wchar_t* valueName, const wchar_t* data) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(hRoot, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    RegSetValueExW(hKey, valueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(data), static_cast<DWORD>((wcslen(data) + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return true;
}

static void deleteRegTree(HKEY hRoot, const wchar_t* subKey) {
    RegDeleteTreeW(hRoot, subKey);
}

STDAPI DllRegisterServer(void) {
    wchar_t dllPath[MAX_PATH] = {0};
    if (GetModuleFileNameW(g_hInst, dllPath, MAX_PATH) == 0) {
        return E_FAIL;
    }

    // 1. Register COM InProcServer32 (both HKCU and HKLM if allowed)
    std::wstring clsidSubKey = L"Software\\Classes\\CLSID\\";
    clsidSubKey += CLSID_STRING;
    std::wstring inprocKey = clsidSubKey + L"\\InprocServer32";

    setRegString(HKEY_CURRENT_USER, clsidSubKey.c_str(), nullptr, TEXTSERVICE_DESC);
    setRegString(HKEY_CURRENT_USER, inprocKey.c_str(), nullptr, dllPath);
    setRegString(HKEY_CURRENT_USER, inprocKey.c_str(), L"ThreadingModel", L"Apartment");

    // Also register override for original Weasel CLSID {A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}
    static const wchar_t* ORIG_CLSID = L"{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}";
    std::wstring origClsidKey = L"Software\\Classes\\CLSID\\";
    origClsidKey += ORIG_CLSID;
    std::wstring origInprocKey = origClsidKey + L"\\InprocServer32";
    setRegString(HKEY_CURRENT_USER, origClsidKey.c_str(), nullptr, TEXTSERVICE_DESC);
    setRegString(HKEY_CURRENT_USER, origInprocKey.c_str(), nullptr, dllPath);
    setRegString(HKEY_CURRENT_USER, origInprocKey.c_str(), L"ThreadingModel", L"Apartment");

    // Also try HKLM in case of elevated registration
    setRegString(HKEY_LOCAL_MACHINE, clsidSubKey.c_str(), nullptr, TEXTSERVICE_DESC);
    setRegString(HKEY_LOCAL_MACHINE, inprocKey.c_str(), nullptr, dllPath);
    setRegString(HKEY_LOCAL_MACHINE, inprocKey.c_str(), L"ThreadingModel", L"Apartment");

    CoInitialize(nullptr);

    // 2. Register TSF Input Processor Profile
    ITfInputProcessorProfiles* pProfiles = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_TF_InputProcessorProfiles,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfiles,
        reinterpret_cast<void**>(&pProfiles)
    );

    if (SUCCEEDED(hr) && pProfiles) {
        // Register Weasel Fluent Profile
        pProfiles->Register(CLSID_WeaselFluentTextService);
        pProfiles->AddLanguageProfile(
            CLSID_WeaselFluentTextService,
            TEXTSERVICE_LANGID,
            GUID_WeaselFluentProfile,
            TEXTSERVICE_DESC,
            static_cast<ULONG>(wcslen(TEXTSERVICE_DESC)),
            dllPath,
            static_cast<ULONG>(wcslen(dllPath)),
            0
        );
        pProfiles->EnableLanguageProfile(
            CLSID_WeaselFluentTextService,
            TEXTSERVICE_LANGID,
            GUID_WeaselFluentProfile,
            TRUE
        );

        // Register Original Weasel Profile compatibility
        pProfiles->Register(CLSID_WeaselOriginalTextService);
        pProfiles->AddLanguageProfile(
            CLSID_WeaselOriginalTextService,
            TEXTSERVICE_LANGID,
            GUID_WeaselOriginalProfile,
            TEXTSERVICE_DESC,
            static_cast<ULONG>(wcslen(TEXTSERVICE_DESC)),
            dllPath,
            static_cast<ULONG>(wcslen(dllPath)),
            0
        );
        pProfiles->EnableLanguageProfile(
            CLSID_WeaselOriginalTextService,
            TEXTSERVICE_LANGID,
            GUID_WeaselOriginalProfile,
            TRUE
        );

        pProfiles->Release();
    }

    // 3. Register TSF Categories for both CLSIDs
    ITfCategoryMgr* pCatMgr = nullptr;
    hr = CoCreateInstance(
        CLSID_TF_CategoryMgr,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_ITfCategoryMgr,
        reinterpret_cast<void**>(&pCatMgr)
    );

    if (SUCCEEDED(hr) && pCatMgr) {
        pCatMgr->RegisterCategory(CLSID_WeaselFluentTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_WeaselFluentTextService);
        pCatMgr->RegisterCategory(CLSID_WeaselFluentTextService, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, CLSID_WeaselFluentTextService);

        pCatMgr->RegisterCategory(CLSID_WeaselOriginalTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_WeaselOriginalTextService);
        pCatMgr->RegisterCategory(CLSID_WeaselOriginalTextService, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, CLSID_WeaselOriginalTextService);

        pCatMgr->Release();
    }

    CoUninitialize();
    return S_OK;
}

STDAPI DllUnregisterServer(void) {
    CoInitialize(nullptr);

    // 1. Unregister TSF Profiles
    ITfInputProcessorProfiles* pProfiles = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, reinterpret_cast<void**>(&pProfiles))) && pProfiles) {
        pProfiles->Unregister(CLSID_WeaselFluentTextService);
        pProfiles->Unregister(CLSID_WeaselOriginalTextService);
        pProfiles->Release();
    }

    // 2. Unregister Categories
    ITfCategoryMgr* pCatMgr = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, reinterpret_cast<void**>(&pCatMgr))) && pCatMgr) {
        pCatMgr->UnregisterCategory(CLSID_WeaselFluentTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_WeaselFluentTextService);
        pCatMgr->UnregisterCategory(CLSID_WeaselFluentTextService, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, CLSID_WeaselFluentTextService);

        pCatMgr->UnregisterCategory(CLSID_WeaselOriginalTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_WeaselOriginalTextService);
        pCatMgr->UnregisterCategory(CLSID_WeaselOriginalTextService, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, CLSID_WeaselOriginalTextService);

        pCatMgr->Release();
    }

    CoUninitialize();

    // 3. Remove COM Registration
    std::wstring clsidSubKey = L"Software\\Classes\\CLSID\\";
    clsidSubKey += CLSID_STRING;

    deleteRegTree(HKEY_CURRENT_USER, clsidSubKey.c_str());
    deleteRegTree(HKEY_LOCAL_MACHINE, clsidSubKey.c_str());

    return S_OK;
}
