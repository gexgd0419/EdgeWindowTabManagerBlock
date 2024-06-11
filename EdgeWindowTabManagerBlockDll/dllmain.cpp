#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wrl.h>
#include <detours.h>
#pragma comment (lib, "runtimeobject.lib")
#include <Shlwapi.h>
#pragma comment (lib, "shlwapi.lib")

#define DEFINE_HOOK(func) static decltype(func)* Real_##func = func
#define ATTACH_HOOK(func) DetourAttach((PVOID*)&Real_##func, My_##func)
#define DETACH_HOOK(func) DetourDetach((PVOID*)&Real_##func, My_##func)

DEFINE_HOOK(RoGetActivationFactory);
DEFINE_HOOK(RoActivateInstance);
DEFINE_HOOK(CreateProcessW);
DEFINE_HOOK(CreateProcessAsUserW);

static char szThisDllPath[MAX_PATH];

bool IsRoClassBlocked(HSTRING activatableClassId)
{
    PCWSTR pszClassId = ::WindowsGetStringRawBuffer(activatableClassId, nullptr);

    if (wcscmp(pszClassId, L"WindowsUdk.UI.Shell.WindowTabManager") == 0
        || wcscmp(pszClassId, L"Windows.UI.Shell.WindowTabManager") == 0) // case sensitive
        return true;

    return false;
}

HRESULT WINAPI My_RoGetActivationFactory(HSTRING activatableClassId, REFIID iid, void** factory)
{
    if (IsRoClassBlocked(activatableClassId))
        return E_ACCESSDENIED;
    return Real_RoGetActivationFactory(activatableClassId, iid, factory);
}

HRESULT WINAPI My_RoActivateInstance(HSTRING activatableClassId, IInspectable** instance)
{
    if (IsRoClassBlocked(activatableClassId))
        return E_ACCESSDENIED;
    return Real_RoActivateInstance(activatableClassId, instance);
}

bool IsEdge(LPWSTR lpCommandLine)
{
    LPWSTR path = _wcsdup(lpCommandLine);
    if (!path) return false;
    PathRemoveArgsW(path);
    PathRemoveBlanksW(path);
    PathUnquoteSpacesW(path);
    int ret = StrCmpIW(PathFindFileNameW(path), L"msedge.exe");
    free(path);
    return ret == 0;
}

BOOL WINAPI My_CreateProcessW(
    _In_opt_ LPCWSTR lpApplicationName,
    _Inout_opt_ LPWSTR lpCommandLine,
    _In_opt_ LPSECURITY_ATTRIBUTES lpProcessAttributes,
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ BOOL bInheritHandles,
    _In_ DWORD dwCreationFlags,
    _In_opt_ LPVOID lpEnvironment,
    _In_opt_ LPCWSTR lpCurrentDirectory,
    _In_ LPSTARTUPINFOW lpStartupInfo,
    _Out_ LPPROCESS_INFORMATION lpProcessInformation
)
{
    // We only care about Edge sub-processes
    if (!lpCommandLine || !IsEdge(lpCommandLine))
        return Real_CreateProcessW(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
            bInheritHandles, dwCreationFlags,
            lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);

    // Bypass IFEO for sub-process creation
    if (!Real_CreateProcessW(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
        bInheritHandles, dwCreationFlags | DEBUG_ONLY_THIS_PROCESS | CREATE_SUSPENDED,
        lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation))
        return FALSE;

    DebugActiveProcessStop(lpProcessInformation->dwProcessId);

    // Inject this DLL into sub-processes as well
    // but not into things like renderers because they are protected
    if (wcsstr(lpCommandLine, L"--type=") == nullptr)
    {
        LPCSTR pDllPath = szThisDllPath;
        DetourUpdateProcessWithDll(lpProcessInformation->hProcess, &pDllPath, 1);
    }

    if (!(dwCreationFlags & CREATE_SUSPENDED))
        ResumeThread(lpProcessInformation->hThread);

    return TRUE;
}

BOOL WINAPI My_CreateProcessAsUserW(
    _In_opt_ HANDLE hToken,
    _In_opt_ LPCWSTR lpApplicationName,
    _Inout_opt_ LPWSTR lpCommandLine,
    _In_opt_ LPSECURITY_ATTRIBUTES lpProcessAttributes,
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ BOOL bInheritHandles,
    _In_ DWORD dwCreationFlags,
    _In_opt_ LPVOID lpEnvironment,
    _In_opt_ LPCWSTR lpCurrentDirectory,
    _In_ LPSTARTUPINFOW lpStartupInfo,
    _Out_ LPPROCESS_INFORMATION lpProcessInformation
)
{
    // We only care about Edge sub-processes
    if (!lpCommandLine || !IsEdge(lpCommandLine))
        return Real_CreateProcessAsUserW(hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
            bInheritHandles, dwCreationFlags,
            lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);

    // Bypass IFEO for sub-process creation
    if (!Real_CreateProcessAsUserW(hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
        bInheritHandles, dwCreationFlags | DEBUG_ONLY_THIS_PROCESS | CREATE_SUSPENDED,
        lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation))
        return FALSE;

    DebugActiveProcessStop(lpProcessInformation->dwProcessId);

    // Inject this DLL into sub-processes as well
    // but not into things like renderers because they are protected
    if (wcsstr(lpCommandLine, L"--type=") == nullptr)
    {
        LPCSTR pDllPath = szThisDllPath;
        DetourUpdateProcessWithDll(lpProcessInformation->hProcess, &pDllPath, 1);
    }

    if (!(dwCreationFlags & CREATE_SUSPENDED))
        ResumeThread(lpProcessInformation->hThread);

    return TRUE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID)
{
    if (DetourIsHelperProcess())
        return TRUE;

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        GetModuleFileNameA(hModule, szThisDllPath, MAX_PATH);
        DetourRestoreAfterWith();
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        ATTACH_HOOK(RoGetActivationFactory);
        ATTACH_HOOK(RoActivateInstance);
        ATTACH_HOOK(CreateProcessW);
        ATTACH_HOOK(CreateProcessAsUserW);
        DetourTransactionCommit();
        break;

    case DLL_PROCESS_DETACH:
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DETACH_HOOK(RoGetActivationFactory);
        DETACH_HOOK(RoActivateInstance);
        DETACH_HOOK(CreateProcessW);
        DETACH_HOOK(CreateProcessAsUserW);
        DetourTransactionCommit();
        break;
    }

    return TRUE;
}

