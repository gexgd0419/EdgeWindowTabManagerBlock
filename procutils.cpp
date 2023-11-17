#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>
#include <memory>
#include <string>
#include "wrappers.h"

typedef LONG NTSTATUS;

typedef enum _PROCESSINFOCLASS
{
    ProcessCommandLineInformation = 60, // q: UNICODE_STRING
} PROCESSINFOCLASS;

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING;
typedef UNICODE_STRING* PUNICODE_STRING;
typedef const UNICODE_STRING* PCUNICODE_STRING;

#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)

typedef NTSTATUS (NTAPI *NtQueryInformationProcess_t)(
    HANDLE ProcessHandle,
    PROCESSINFOCLASS ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength
);

static NtQueryInformationProcess_t NtQueryInformationProcess
    = (NtQueryInformationProcess_t)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess");

// Gets the command line of a process. Returns empty string on failure.
static std::wstring GetProcessCommandLine(DWORD pid)
{
    if (!NtQueryInformationProcess)
        return std::wstring();

    Handle hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc)
        return std::wstring();

    ULONG cb = 0;
    NTSTATUS stat = NtQueryInformationProcess(hProc, ProcessCommandLineInformation, nullptr, 0, &cb);
    if (stat != STATUS_INFO_LENGTH_MISMATCH)
        return std::wstring();

    std::unique_ptr<UNICODE_STRING> pBuffer(static_cast<PUNICODE_STRING>(operator new(cb)));
    stat = NtQueryInformationProcess(hProc, ProcessCommandLineInformation, pBuffer.get(), cb, nullptr);
    if (!NT_SUCCESS(stat))
        return std::wstring();

    return std::wstring(pBuffer->Buffer, pBuffer->Length / sizeof(WCHAR));
}

static bool DoesProcessHaveWindowTabManager(DWORD pid)
{
    HFile hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (hSnap == INVALID_HANDLE_VALUE)
        return false;
    MODULEENTRY32W me = { sizeof me };
    if (Module32FirstW(hSnap, &me))
    {
        do
        {
            if (_wcsicmp(me.szModule, L"Windows.Internal.UI.Shell.WindowTabManager.dll") == 0)
                return true;
        } while (Module32NextW(hSnap, &me));
    }
    return false;
}

static BOOL CALLBACK EnumThreadWindowsProc(HWND hwnd, LPARAM lparam)
{
    // either the window is visible, or the window class name is Chrome_WidgetWin_1
    if (!IsWindowVisible(hwnd))
    {
        WCHAR szClass[32];
        GetClassNameW(hwnd, szClass, 32);
        if (wcscmp(szClass, L"Chrome_WidgetWin_1") == 0)
            return TRUE; // continue enumeration
    }

    *reinterpret_cast<bool*>(lparam) = true;
    return FALSE; // stop enumeration
}

static bool HasVisibleEdgeWindows(DWORD pid)
{
    HFile hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0); // all threads are included
    if (hSnap == INVALID_HANDLE_VALUE)
        return false;
    THREADENTRY32 te = { sizeof te };
    if (!Thread32First(hSnap, &te))
        return false;
    do
    {
        if (te.th32OwnerProcessID != pid)
            continue;
        bool flag = false;
        EnumThreadWindows(te.th32ThreadID, EnumThreadWindowsProc, reinterpret_cast<LPARAM>(&flag));
        if (flag)
            return true;
    } while (Thread32Next(hSnap, &te));

    return false;
}

// Returns the main Edge process which is using WindowTabManager. Returns 0 if not found.
DWORD FindEdgeProcessWithWindowTabManager()
{
    HFile hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32W pe = { sizeof pe };
    if (!Process32FirstW(hSnap, &pe))
        return 0;

    do
    {
        if (_wcsicmp(pe.szExeFile, L"msedge.exe") != 0)
            continue;

        std::wstring cmdLine = GetProcessCommandLine(pe.th32ProcessID);
        if (cmdLine.empty())
            continue;
        if (cmdLine.find(L"--type=") != std::wstring::npos) // do not include Edge subprocesses
            continue;

        if (DoesProcessHaveWindowTabManager(pe.th32ProcessID))
        {
            // if it's launched by Startup Boost, and is running in the background
            // (there are no visible Edge windows open),
            // we can safely kill it to make Edge start from scratch
            if (cmdLine.find(L"--no-startup-window") != std::wstring::npos
                && !HasVisibleEdgeWindows(pe.th32ProcessID))
            {
                Handle hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProc)
                    TerminateProcess(hProc, 1);
            }
            else
            {
                return pe.th32ProcessID;
            }
        }
    } while (Process32NextW(hSnap, &pe));

    return 0;
}