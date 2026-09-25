#pragma once

#include <windows.h>
#include <msctf.h>
#include <olectl.h>
#include <string>

// CLSID for Weasel Fluent Text Service: {D7B1C618-9F3E-4E7B-B96E-5C13D1A5B4C2}
static const GUID CLSID_WeaselFluentTextService = 
    { 0xd7b1c618, 0x9f3e, 0x4e7b, { 0xb9, 0x6e, 0x5c, 0x13, 0xd1, 0xa5, 0xb4, 0xc2 } };

// GUID for Weasel Fluent Profile: {E8934A01-C349-4F8A-8512-14AE0293BD01}
static const GUID GUID_WeaselFluentProfile = 
    { 0xe8934a01, 0xc349, 0x4f8a, { 0x85, 0x12, 0x14, 0xae, 0x02, 0x93, 0xbd, 0x01 } };

// Original Weasel CLSID: {A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}
static const GUID CLSID_WeaselOriginalTextService = 
    { 0xa3f4cded, 0xb1e9, 0x41ee, { 0x9c, 0xa6, 0x7b, 0x4d, 0x0d, 0xe6, 0xcb, 0x0a } };

// Original Weasel Profile GUID: {3D02CAB6-2B8E-4781-BA20-1C9267529467}
static const GUID GUID_WeaselOriginalProfile = 
    { 0x3d02cab6, 0x2b8e, 0x4781, { 0xba, 0x20, 0x1c, 0x92, 0x67, 0x52, 0x94, 0x67 } };

#define TEXTSERVICE_DESC L"小狼毫 (Fluent)"
#define TEXTSERVICE_LANGID 0x0804 // Chinese (Simplified, PRC)

#define WEASEL_PIPE_NAME L"\\\\.\\pipe\\WeaselFluentNamedPipe"

#pragma pack(push, 1)
enum IpcMsgType : uint32_t {
    IPC_MSG_KEY = 1,
    IPC_MSG_CARET = 2,
    IPC_MSG_ACTIVATE = 3,
    IPC_MSG_DEACTIVATE = 4
};

struct IpcKeyRequest {
    uint32_t msgType;
    uint32_t vkCode;
    uint32_t isKeyDown;
    RECT caretRect;
    uint64_t hwnd;
};

struct IpcKeyResponse {
    uint32_t eaten;
    uint32_t isComposing;
    uint32_t commitLen;
    uint32_t isAsciiMode;
    wchar_t commitText[128];
};
#pragma pack(pop)

extern HINSTANCE g_hInst;
extern LONG g_cRefDll;

inline void DllAddRef() {
    InterlockedIncrement(&g_cRefDll);
}

inline void DllRelease() {
    InterlockedDecrement(&g_cRefDll);
}
