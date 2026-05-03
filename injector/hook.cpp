#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LOG(msg) OutputDebugStringA(msg)

static const bool kEnableLzssBypass = false;

// Image base: 0x00400000
// ============================================
// OUTGOING PATCHES
// ============================================
#define ENC_OUT_OFFSET   0x00126ADD   // Encryption XOR -> MOV

// ============================================
// LZSS PATCHES (DISABLED FOR NOW)
// ============================================
#define LZSS_SEND_CALL_OFFSET  0x001269AD   // CALL CompressPacketLZSS inside Packet_CompressAndFlushSendBuffer
#define LZSS_RECV_CALL_OFFSET  0x00127981   // CALL FUN_00527ee0 inside PacketStreamBufferOutput

// ============================================
// INCOMING PATCHES
// ============================================
#define DEC_IN1_OFFSET   0x001266AC   // Decryption XOR1 -> MOV
#define DEC_IN2_OFFSET   0x001266F3   // Decryption XOR2 -> MOV
#define DEC_IN3_OFFSET   0x0012673A   // Decryption XOR3 -> MOV
#define DEC_IN4_OFFSET   0x0012677D   // Decryption XOR4 -> MOV

// --------------------------------------------------
// OUTGOING: Encryption patch (XOR -> MOV)
// 32 1C 31  -> 8A 1C 31
// --------------------------------------------------
BYTE enc_out_patch[3] = { 0x8A, 0x1C, 0x31 };

// --------------------------------------------------
// INCOMING: Decryption patches (XOR -> MOV)
// --------------------------------------------------
BYTE dec_in1_patch[3] = { 0x8A, 0x14, 0x1E };
BYTE dec_in2_patch[3] = { 0x8A, 0x0C, 0x1E };
BYTE dec_in3_patch[3] = { 0x8A, 0x14, 0x1E };
BYTE dec_in4_patch[3] = { 0x8A, 0x0C, 0x1E };

static volatile LONG g_patchThreadStarted = 0;
static volatile LONG g_udpLogInitialized = 0;
static CRITICAL_SECTION g_udpLogLock;
static HANDLE g_udpLogFile = INVALID_HANDLE_VALUE;

typedef int (WSAAPI* SendToFn)(SOCKET, const char*, int, int, const sockaddr*, int);
typedef int (WSAAPI* RecvFromFn)(SOCKET, char*, int, int, sockaddr*, int*);

static SendToFn g_realSendTo = nullptr;
static RecvFromFn g_realRecvFrom = nullptr;

DWORD WINAPI PatchThread(LPVOID);

extern "C" void __stdcall RawLzssCompressBypass_Impl(uintptr_t lzObj, const void* buffer, int size);
extern "C" int __stdcall RawLzssDecodeBypass_Impl(uintptr_t packetSubobject, const void* buffer, int size);
extern "C" void RawLzssCompressBypass_Entry();
extern "C" void RawLzssDecodeBypass_Entry();

void LogPacketPreview(const char* tag, const void* buffer, int size)
{
    char buf[256];
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(buffer);
    int count = size < 8 ? size : 8;
    int written = sprintf(buf, "[HOOK] %s size=%d bytes=", tag, size);

    for (int i = 0; i < count && written < static_cast<int>(sizeof(buf)) - 4; ++i)
    {
        written += sprintf(buf + written, "%02X", bytes[i]);
        if (i + 1 < count)
        {
            written += sprintf(buf + written, " ");
        }
    }

    if (count < size && written < static_cast<int>(sizeof(buf)) - 5)
    {
        sprintf(buf + written, " ...\n");
    }
    else
    {
        sprintf(buf + written, "\n");
    }

    LOG(buf);
}

bool IsPatchRegionAccessible(const void* addr, size_t size)
{
    MEMORY_BASIC_INFORMATION mbi = {};
    if (!VirtualQuery(addr, &mbi, sizeof(mbi)))
    {
        return false;
    }

    if (mbi.State != MEM_COMMIT)
    {
        return false;
    }

    if ((mbi.Protect & PAGE_GUARD) || mbi.Protect == PAGE_NOACCESS)
    {
        return false;
    }

    uintptr_t regionStart = (uintptr_t)mbi.BaseAddress;
    uintptr_t regionEnd = regionStart + mbi.RegionSize;
    uintptr_t patchStart = (uintptr_t)addr;
    uintptr_t patchEnd = patchStart + size;
    return patchStart >= regionStart && patchEnd <= regionEnd;
}

void ResolveClientSideLogPath(char* path, size_t pathSize)
{
    DWORD len = GetModuleFileNameA(nullptr, path, (DWORD)pathSize);
    if (!len || len >= pathSize)
    {
        strncpy(path, "udp_hook_traffic.log", pathSize - 1);
        path[pathSize - 1] = '\0';
        return;
    }

    char* slash = strrchr(path, '\\');
    if (!slash)
    {
        strncpy(path, "udp_hook_traffic.log", pathSize - 1);
        path[pathSize - 1] = '\0';
        return;
    }

    strcpy(slash + 1, "udp_hook_traffic.log");
}

void InitUdpLogOnce()
{
    if (InterlockedCompareExchange(&g_udpLogInitialized, 1, 0) != 0)
    {
        return;
    }

    InitializeCriticalSection(&g_udpLogLock);

    char path[MAX_PATH] = {};
    ResolveClientSideLogPath(path, sizeof(path));
    g_udpLogFile = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_udpLogFile == INVALID_HANDLE_VALUE)
    {
        char buf[256];
        sprintf(buf, "[HOOK] Failed to open UDP traffic log: %s error=%lu\n", path, GetLastError());
        LOG(buf);
        return;
    }

    char buf[320];
    sprintf(buf, "[HOOK] UDP traffic log: %s\n", path);
    LOG(buf);
    DWORD written = 0;
    WriteFile(g_udpLogFile, buf, (DWORD)strlen(buf), &written, nullptr);
}

void WriteUdpLogText(const char* text)
{
    InitUdpLogOnce();
    if (g_udpLogFile == INVALID_HANDLE_VALUE)
    {
        return;
    }

    EnterCriticalSection(&g_udpLogLock);
    DWORD written = 0;
    WriteFile(g_udpLogFile, text, (DWORD)strlen(text), &written, nullptr);
    LeaveCriticalSection(&g_udpLogLock);
}

void FormatSockaddr(const sockaddr* address, int addressLen, char* out, size_t outSize)
{
    if (!address || addressLen < (int)sizeof(sockaddr_in) || address->sa_family != AF_INET)
    {
        strncpy(out, "(unknown)", outSize - 1);
        out[outSize - 1] = '\0';
        return;
    }

    const sockaddr_in* ipv4 = reinterpret_cast<const sockaddr_in*>(address);
    const unsigned char* ip = reinterpret_cast<const unsigned char*>(&ipv4->sin_addr.s_addr);
    const unsigned char* port = reinterpret_cast<const unsigned char*>(&ipv4->sin_port);
    const unsigned int portValue = ((unsigned int)port[0] << 8) | port[1];
    _snprintf(out, outSize - 1, "%u.%u.%u.%u:%u", ip[0], ip[1], ip[2], ip[3], portValue);
    out[outSize - 1] = '\0';
}

void WriteUdpHexLine(const char* tag, SOCKET socket, const sockaddr* address, int addressLen, const char* data, int size)
{
    if (!data || size <= 0)
    {
        return;
    }

    char endpoint[64] = {};
    FormatSockaddr(address, addressLen, endpoint, sizeof(endpoint));

    char header[192];
    _snprintf(header, sizeof(header) - 1, "[UDP-HOOK] %s socket=%p endpoint=%s len=%d hex=", tag, (void*)socket,
              endpoint, size);
    header[sizeof(header) - 1] = '\0';
    WriteUdpLogText(header);

    char hex[4];
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data);
    for (int i = 0; i < size; ++i)
    {
        _snprintf(hex, sizeof(hex), "%02X%s", bytes[i], (i + 1 < size) ? " " : "");
        WriteUdpLogText(hex);
    }
    WriteUdpLogText("\n");
}

int WSAAPI HookedSendTo(SOCKET socket, const char* buffer, int length, int flags, const sockaddr* to, int toLength)
{
    if (!g_realSendTo)
    {
        return SOCKET_ERROR;
    }
    const int result = g_realSendTo(socket, buffer, length, flags, to, toLength);
    if (result >= 0)
    {
        WriteUdpHexLine("sendto", socket, to, toLength, buffer, length);
    }
    return result;
}

int WSAAPI HookedRecvFrom(SOCKET socket, char* buffer, int length, int flags, sockaddr* from, int* fromLength)
{
    if (!g_realRecvFrom)
    {
        return SOCKET_ERROR;
    }
    const int result = g_realRecvFrom(socket, buffer, length, flags, from, fromLength);
    if (result > 0)
    {
        WriteUdpHexLine("recvfrom", socket, from, fromLength ? *fromLength : 0, buffer, result);
    }
    return result;
}

bool HookIatFunction(HMODULE module, const char* importedModuleName, const char* functionName, void* replacement,
                     void** original)
{
    if (!module || !importedModuleName || !functionName || !replacement || !original)
    {
        return false;
    }

    auto* dos = reinterpret_cast<PIMAGE_DOS_HEADER>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return false;
    }

    auto* nt = reinterpret_cast<PIMAGE_NT_HEADERS>((BYTE*)module + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
    {
        return false;
    }

    const auto& importDirectory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!importDirectory.VirtualAddress)
    {
        return false;
    }

    auto* descriptor = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>((BYTE*)module + importDirectory.VirtualAddress);
    for (; descriptor->Name; ++descriptor)
    {
        const char* dllName = reinterpret_cast<const char*>((BYTE*)module + descriptor->Name);
        if (_stricmp(dllName, importedModuleName) != 0)
        {
            continue;
        }

        auto* thunk = reinterpret_cast<PIMAGE_THUNK_DATA>((BYTE*)module + descriptor->FirstThunk);
        if (!descriptor->OriginalFirstThunk)
        {
            continue;
        }
        auto* originalThunk = reinterpret_cast<PIMAGE_THUNK_DATA>((BYTE*)module + descriptor->OriginalFirstThunk);
        for (; originalThunk->u1.AddressOfData; ++thunk, ++originalThunk)
        {
            if (originalThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)
            {
                continue;
            }

            auto* importByName = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>((BYTE*)module + originalThunk->u1.AddressOfData);
            if (strcmp(reinterpret_cast<const char*>(importByName->Name), functionName) != 0)
            {
                continue;
            }

            DWORD oldProtect = 0;
            if (!VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), PAGE_READWRITE, &oldProtect))
            {
                return false;
            }

            *original = reinterpret_cast<void*>(thunk->u1.Function);
            thunk->u1.Function = reinterpret_cast<ULONG_PTR>(replacement);
            VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), &thunk->u1.Function, sizeof(thunk->u1.Function));
            return true;
        }
    }

    return false;
}

bool HookIatAddress(HMODULE module, void* target, void* replacement, void** original)
{
    if (!module || !target || !replacement || !original)
    {
        return false;
    }

    auto* dos = reinterpret_cast<PIMAGE_DOS_HEADER>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return false;
    }

    auto* nt = reinterpret_cast<PIMAGE_NT_HEADERS>((BYTE*)module + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
    {
        return false;
    }

    const auto& importDirectory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!importDirectory.VirtualAddress)
    {
        return false;
    }

    auto* descriptor = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>((BYTE*)module + importDirectory.VirtualAddress);
    for (; descriptor->Name; ++descriptor)
    {
        auto* thunk = reinterpret_cast<PIMAGE_THUNK_DATA>((BYTE*)module + descriptor->FirstThunk);
        for (; thunk->u1.Function; ++thunk)
        {
            if (reinterpret_cast<void*>(thunk->u1.Function) != target)
            {
                continue;
            }

            DWORD oldProtect = 0;
            if (!VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), PAGE_READWRITE, &oldProtect))
            {
                return false;
            }

            *original = reinterpret_cast<void*>(thunk->u1.Function);
            thunk->u1.Function = reinterpret_cast<ULONG_PTR>(replacement);
            VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), &thunk->u1.Function, sizeof(thunk->u1.Function));
            return true;
        }
    }

    return false;
}

void InstallUdpIatHooks()
{
    HMODULE module = GetModuleHandleA(nullptr);
    void* originalSendTo = nullptr;
    void* originalRecvFrom = nullptr;

    const bool sendHooked = HookIatFunction(module, "WS2_32.dll", "sendto", reinterpret_cast<void*>(HookedSendTo),
                                            &originalSendTo) ||
                            HookIatFunction(module, "ws2_32.dll", "sendto", reinterpret_cast<void*>(HookedSendTo),
                                            &originalSendTo) ||
                            HookIatFunction(module, "WSOCK32.dll", "sendto", reinterpret_cast<void*>(HookedSendTo),
                                            &originalSendTo);
    const bool recvHooked = HookIatFunction(module, "WS2_32.dll", "recvfrom", reinterpret_cast<void*>(HookedRecvFrom),
                                            &originalRecvFrom) ||
                            HookIatFunction(module, "ws2_32.dll", "recvfrom", reinterpret_cast<void*>(HookedRecvFrom),
                                            &originalRecvFrom) ||
                            HookIatFunction(module, "WSOCK32.dll", "recvfrom", reinterpret_cast<void*>(HookedRecvFrom),
                                            &originalRecvFrom);

    HMODULE ws2 = GetModuleHandleA("WS2_32.dll");
    if (!ws2)
    {
        ws2 = LoadLibraryA("WS2_32.dll");
    }
    HMODULE wsock32 = GetModuleHandleA("WSOCK32.dll");
    if (!wsock32)
    {
        wsock32 = LoadLibraryA("WSOCK32.dll");
    }

    bool sendAddressHooked = false;
    bool recvAddressHooked = false;
    if (!sendHooked)
    {
        void* ws2Send = ws2 ? reinterpret_cast<void*>(GetProcAddress(ws2, "sendto")) : nullptr;
        void* wsockSend = wsock32 ? reinterpret_cast<void*>(GetProcAddress(wsock32, "sendto")) : nullptr;
        sendAddressHooked = (ws2Send && HookIatAddress(module, ws2Send, reinterpret_cast<void*>(HookedSendTo),
                                                       &originalSendTo)) ||
                            (wsockSend && HookIatAddress(module, wsockSend, reinterpret_cast<void*>(HookedSendTo),
                                                         &originalSendTo));
    }
    if (!recvHooked)
    {
        void* ws2Recv = ws2 ? reinterpret_cast<void*>(GetProcAddress(ws2, "recvfrom")) : nullptr;
        void* wsockRecv = wsock32 ? reinterpret_cast<void*>(GetProcAddress(wsock32, "recvfrom")) : nullptr;
        recvAddressHooked = (ws2Recv && HookIatAddress(module, ws2Recv, reinterpret_cast<void*>(HookedRecvFrom),
                                                       &originalRecvFrom)) ||
                            (wsockRecv && HookIatAddress(module, wsockRecv, reinterpret_cast<void*>(HookedRecvFrom),
                                                         &originalRecvFrom));
    }

    if (sendHooked || sendAddressHooked)
    {
        g_realSendTo = reinterpret_cast<SendToFn>(originalSendTo);
    }
    if (recvHooked || recvAddressHooked)
    {
        g_realRecvFrom = reinterpret_cast<RecvFromFn>(originalRecvFrom);
    }

    char buf[192];
    sprintf(buf, "[HOOK] UDP IAT hooks: sendto=%s recvfrom=%s\n",
            (sendHooked || sendAddressHooked) ? "installed" : "missing",
            (recvHooked || recvAddressHooked) ? "installed" : "missing");
    LOG(buf);
    WriteUdpLogText(buf);
}

// --------------------------------------------------
// LZSS JMP patch (E9 rel32 + NOP)
// (currently disabled)
// --------------------------------------------------
//BYTE lzss_jmp_patch[6] = { 0 };

// --------------------------------------------------
void EnforcePatch(BYTE* addr, BYTE* patch, size_t size, const char* name)
{
    if (!IsPatchRegionAccessible(addr, size))
    {
        char buf[128];
        sprintf(buf, "[HOOK] Patch region unavailable for %s @ %p\n", name, addr);
        LOG(buf);
        return;
    }

    if (memcmp(addr, patch, size) != 0)
    {
        DWORD old;
        VirtualProtect(addr, size, PAGE_EXECUTE_READWRITE, &old);
        memcpy(addr, patch, size);
        VirtualProtect(addr, size, old, &old);
        FlushInstructionCache(GetCurrentProcess(), addr, size);

        char buf[128];
        sprintf(buf, "[HOOK] Re-patched %s @ %p\n", name, addr);
        LOG(buf);
    }
}

void EnforceCallPatch(BYTE* addr, const void* target, const char* name)
{
    if (!IsPatchRegionAccessible(addr, 5))
    {
        char buf[128];
        sprintf(buf, "[HOOK] Patch region unavailable for %s @ %p\n", name, addr);
        LOG(buf);
        return;
    }

    BYTE patch[5] = { 0 };
    patch[0] = 0xE8;
    int32_t rel = (int32_t)((uintptr_t)target - ((uintptr_t)addr + 5));
    memcpy(&patch[1], &rel, sizeof(rel));

    if (memcmp(addr, patch, sizeof(patch)) != 0)
    {
        DWORD old;
        VirtualProtect(addr, sizeof(patch), PAGE_EXECUTE_READWRITE, &old);
        memcpy(addr, patch, sizeof(patch));
        VirtualProtect(addr, sizeof(patch), old, &old);
        FlushInstructionCache(GetCurrentProcess(), addr, sizeof(patch));

        char buf[160];
        sprintf(buf, "[HOOK] Re-patched %s CALL @ %p -> %p\n", name, addr, target);
        LOG(buf);
    }
}

extern "C" void __stdcall RawLzssCompressBypass_Impl(uintptr_t lzObj, const void* buffer, int size)
{
    if (!lzObj || !buffer || size <= 0)
    {
        return;
    }

    uintptr_t packet = lzObj - 0x1048;
    memcpy(reinterpret_cast<void*>(packet + 0x6900), buffer, size);
    *reinterpret_cast<int*>(packet + 0x6980) = size;
    *reinterpret_cast<int*>(packet + 0x6988) = 0;
    LogPacketPreview("LZSS-SEND-RAW", buffer, size);
}

extern "C" int __stdcall RawLzssDecodeBypass_Impl(uintptr_t packetSubobject, const void* buffer, int size)
{
    if (!packetSubobject || !buffer || size <= 0)
    {
        return 0;
    }

    LogPacketPreview("LZSS-RECV-RAW", buffer, size);
    memcpy(reinterpret_cast<void*>(packetSubobject + 0x6844), buffer, size);
    *reinterpret_cast<int*>(packetSubobject + 0x68C8) = size;
    *reinterpret_cast<int*>(packetSubobject + 0x6954) = 0;
    return size;
}

extern "C" void __declspec(naked) RawLzssCompressBypass_Entry()
{
    __asm
    {
        push [esp + 8]
        push [esp + 8]
        push esi
        call RawLzssCompressBypass_Impl
        add esp, 12
        ret 8
    }
}

extern "C" void __declspec(naked) RawLzssDecodeBypass_Entry()
{
    __asm
    {
        push [esp + 4]
        push ecx
        push esi
        call RawLzssDecodeBypass_Impl
        add esp, 12
        ret 4
    }
}

bool StartPatchThreadOnce()
{
    if (InterlockedCompareExchange(&g_patchThreadStarted, 1, 0) != 0)
    {
        LOG("[HOOK] Patch thread already running\n");
        return false;
    }

    HANDLE hThread = CreateThread(nullptr, 0, PatchThread, nullptr, 0, nullptr);
    if (!hThread)
    {
        InterlockedExchange(&g_patchThreadStarted, 0);

        char buf[128];
        sprintf(buf, "[HOOK] Failed to create patch thread error=%lu\n", GetLastError());
        LOG(buf);
        return false;
    }

    CloseHandle(hThread);
    return true;
}

// --------------------------------------------------
DWORD WINAPI PatchThread(LPVOID)
{
    Sleep(2000); // Wait for full initialization

    uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);

    // Resolve addresses
    BYTE* enc_out_addr = (BYTE*)(base + ENC_OUT_OFFSET);

    BYTE* dec_in1_addr = (BYTE*)(base + DEC_IN1_OFFSET);
    BYTE* dec_in2_addr = (BYTE*)(base + DEC_IN2_OFFSET);
    BYTE* dec_in3_addr = (BYTE*)(base + DEC_IN3_OFFSET);
    BYTE* dec_in4_addr = (BYTE*)(base + DEC_IN4_OFFSET);
    BYTE* lzss_send_call_addr = (BYTE*)(base + LZSS_SEND_CALL_OFFSET);
    BYTE* lzss_recv_call_addr = (BYTE*)(base + LZSS_RECV_CALL_OFFSET);

    char buf[256];
    sprintf(buf,
            "[HOOK] Patch enforcement thread started\n"
            "[HOOK] Base: 0x%p\n"
            "[HOOK] Outgoing Encryption: 0x%p\n"
            "[HOOK] Incoming Decrypt 1: 0x%p\n"
            "[HOOK] Incoming Decrypt 2: 0x%p\n"
            "[HOOK] Incoming Decrypt 3: 0x%p\n"
            "[HOOK] Incoming Decrypt 4: 0x%p\n"
            "[HOOK] LZSS Send CALL Bypass: 0x%p (%s)\n"
            "[HOOK] LZSS Recv CALL Bypass: 0x%p (%s)\n",
            (void*)base,
            enc_out_addr,
            dec_in1_addr,
            dec_in2_addr,
            dec_in3_addr,
            dec_in4_addr,
            lzss_send_call_addr,
            kEnableLzssBypass ? "enabled" : "disabled",
            lzss_recv_call_addr,
            kEnableLzssBypass ? "enabled" : "disabled");
    LOG(buf);

    InitUdpLogOnce();
    InstallUdpIatHooks();

    LOG("[HOOK] Applying initial patches...\n");

    EnforcePatch(enc_out_addr, enc_out_patch, sizeof(enc_out_patch), "ENC-OUT");
    EnforcePatch(dec_in1_addr, dec_in1_patch, sizeof(dec_in1_patch), "DEC-IN1");
    EnforcePatch(dec_in2_addr, dec_in2_patch, sizeof(dec_in2_patch), "DEC-IN2");
    EnforcePatch(dec_in3_addr, dec_in3_patch, sizeof(dec_in3_patch), "DEC-IN3");
    EnforcePatch(dec_in4_addr, dec_in4_patch, sizeof(dec_in4_patch), "DEC-IN4");
    if (kEnableLzssBypass)
    {
        EnforceCallPatch(lzss_send_call_addr, reinterpret_cast<void*>(RawLzssCompressBypass_Entry), "LZSS-SEND");
        EnforceCallPatch(lzss_recv_call_addr, reinterpret_cast<void*>(RawLzssDecodeBypass_Entry), "LZSS-RECV");
    }
    LOG("[HOOK] Initial patches complete!\n");

    while (true)
    {
        EnforcePatch(enc_out_addr, enc_out_patch, sizeof(enc_out_patch), "ENC-OUT");
        EnforcePatch(dec_in1_addr, dec_in1_patch, sizeof(dec_in1_patch), "DEC-IN1");
        EnforcePatch(dec_in2_addr, dec_in2_patch, sizeof(dec_in2_patch), "DEC-IN2");
        EnforcePatch(dec_in3_addr, dec_in3_patch, sizeof(dec_in3_patch), "DEC-IN3");
        EnforcePatch(dec_in4_addr, dec_in4_patch, sizeof(dec_in4_patch), "DEC-IN4");
        if (kEnableLzssBypass)
        {
            EnforceCallPatch(lzss_send_call_addr, reinterpret_cast<void*>(RawLzssCompressBypass_Entry), "LZSS-SEND");
            EnforceCallPatch(lzss_recv_call_addr, reinterpret_cast<void*>(RawLzssDecodeBypass_Entry), "LZSS-RECV");
        }
        Sleep(100);
    }

    return 0;
}

// --------------------------------------------------
extern "C" __declspec(dllexport) void StartHook()
{
    LOG("[HOOK] StartHook called\n");
    StartPatchThreadOnce();
}

// --------------------------------------------------
BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hInst);
        LOG("[HOOK] DLL_PROCESS_ATTACH\n");

        if (StartPatchThreadOnce())
        {
            LOG("[HOOK] Background patch thread started without popup dialog\n");
        }
    }
    return TRUE;
}
