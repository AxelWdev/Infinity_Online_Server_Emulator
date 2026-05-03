#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static DWORD FindProcessIdByName(const char* exeName, bool* multipleMatches)
{
    if (multipleMatches)
    {
        *multipleMatches = false;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        printf("[-] CreateToolhelp32Snapshot failed error=%lu\n", GetLastError());
        return 0;
    }

    PROCESSENTRY32 entry = {};
    entry.dwSize = sizeof(entry);

    DWORD foundPid = 0;
    if (Process32First(snapshot, &entry))
    {
        do
        {
            if (_stricmp(entry.szExeFile, exeName) == 0)
            {
                if (!foundPid)
                {
                    foundPid = entry.th32ProcessID;
                }
                else if (multipleMatches)
                {
                    *multipleMatches = true;
                }
            }
        } while (Process32Next(snapshot, &entry));
    }
    else
    {
        printf("[-] Process32First failed error=%lu\n", GetLastError());
    }

    CloseHandle(snapshot);
    return foundPid;
}

static bool ResolveDllPath(int argc, char* argv[], char* dllPath, size_t dllPathSize)
{
    if (argc >= 2)
    {
        strncpy(dllPath, argv[1], dllPathSize - 1);
        dllPath[dllPathSize - 1] = '\0';
    }
    else
    {
        DWORD len = GetModuleFileNameA(nullptr, dllPath, (DWORD)dllPathSize);
        if (!len || len >= dllPathSize)
        {
            printf("[-] GetModuleFileNameA failed error=%lu\n", GetLastError());
            return false;
        }

        char* slash = strrchr(dllPath, '\\');
        if (!slash)
        {
            printf("[-] Failed to resolve injector directory\n");
            return false;
        }

        strcpy(slash + 1, "hook.dll");
    }

    char fullPath[MAX_PATH] = {};
    DWORD fullLen = GetFullPathNameA(dllPath, MAX_PATH, fullPath, nullptr);
    if (!fullLen || fullLen >= MAX_PATH)
    {
        printf("[-] GetFullPathNameA failed for DLL path\n");
        return false;
    }

    DWORD attrs = GetFileAttributesA(fullPath);
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY))
    {
        printf("[-] DLL not found: %s\n", fullPath);
        return false;
    }

    strncpy(dllPath, fullPath, dllPathSize - 1);
    dllPath[dllPathSize - 1] = '\0';
    return true;
}

// ==========================================================
// DLL INJECTION
// ==========================================================

bool InjectAndRun(DWORD pid, const char* dllPath) {

    HANDLE hProc = OpenProcess(
        PROCESS_CREATE_THREAD |
        PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE |
        PROCESS_VM_READ,
        FALSE,
        pid
    );

    if (!hProc) {
        printf("[-] OpenProcess failed (PID %lu) error=%lu\n", pid, GetLastError());
        return false;
    }

    size_t len = strlen(dllPath) + 1;
    void* remotePath = VirtualAllocEx(
        hProc, nullptr, len,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );

    if (!remotePath) {
        printf("[-] VirtualAllocEx failed\n");
        CloseHandle(hProc);
        return false;
    }

    if (!WriteProcessMemory(hProc, remotePath, dllPath, len, nullptr)) {
        printf("[-] WriteProcessMemory failed\n");
        VirtualFreeEx(hProc, remotePath, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }

    FARPROC loadLib = GetProcAddress(
        GetModuleHandleA("kernel32.dll"),
        "LoadLibraryA"
    );

    HANDLE hThread = CreateRemoteThread(
        hProc,
        nullptr,
        0,
        (LPTHREAD_START_ROUTINE)loadLib,
        remotePath,
        0,
        nullptr
    );

    if (!hThread) {
        printf("[-] CreateRemoteThread (LoadLibrary) failed\n");
        VirtualFreeEx(hProc, remotePath, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);

    DWORD remoteBase = 0;
    GetExitCodeThread(hThread, &remoteBase);
    CloseHandle(hThread);

    VirtualFreeEx(hProc, remotePath, 0, MEM_RELEASE);

    if (!remoteBase) {
        printf("[-] LoadLibrary failed inside target\n");
        CloseHandle(hProc);
        return false;
    }

    printf("[+] DLL loaded at 0x%08X\n", remoteBase);
    printf("[+] Remote LoadLibrary completed; DllMain performed hook initialization\n");
    CloseHandle(hProc);
    return true;
}

// ==========================================================
// ENTRY POINT
// ==========================================================

int main(int argc, char* argv[]) {

    char dllPath[MAX_PATH] = {0};
    const char* processName = "xclient.exe";
    bool multipleMatches = false;

    printf("========================================\n");
    printf("   DLL Injector (xclient.exe auto mode)\n");
    printf("========================================\n\n");
    printf("[*] Default DLL location: hook.dll next to injector.exe\n");
    if (argc >= 2) {
        printf("[*] DLL override supplied on command line\n");
    }

    if (!ResolveDllPath(argc, argv, dllPath, sizeof(dllPath))) {
        return 1;
    }

    DWORD pid = FindProcessIdByName(processName, &multipleMatches);
    if (!pid) {
        printf("[-] Could not find %s. Start the client first.\n", processName);
        return 1;
    }

    if (multipleMatches) {
        printf("[!] Multiple %s processes detected; using PID %lu\n", processName, pid);
    }

    printf("[*] Target process: %s\n", processName);
    printf("[*] Target PID: %lu\n", pid);
    printf("[*] DLL: %s\n", dllPath);

    if (!InjectAndRun(pid, dllPath)) {
        printf("[-] Injection failed\n");
        return 1;
    }

    printf("[+] Injection complete\n");
    return 0;
}

// injector.exe
// injector.exe [optional-dll-path]
