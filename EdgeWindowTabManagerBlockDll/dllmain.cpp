#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wrl.h>
#include <detours.h>
#pragma comment (lib, "runtimeobject.lib")

static HRESULT (WINAPI *Real_RoGetActivationFactory)(HSTRING activatableClassId, REFIID iid, void** factory) = RoGetActivationFactory;
static HRESULT (WINAPI *Real_RoActivateInstance)(HSTRING activatableClassId, IInspectable** instance) = RoActivateInstance;

bool IsRoClassBlocked(HSTRING activatableClassId)
{
    PCWSTR pszClassId = ::WindowsGetStringRawBuffer(activatableClassId, nullptr);

    if (wcsstr(pszClassId, L"WindowTabManager") != NULL)
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

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID)
{
    if (DetourIsHelperProcess())
        return TRUE;

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DetourRestoreAfterWith();
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach((PVOID*)&Real_RoGetActivationFactory, My_RoGetActivationFactory);
        DetourAttach((PVOID*)&Real_RoActivateInstance, My_RoActivateInstance);
        DetourTransactionCommit();
        break;

    case DLL_PROCESS_DETACH:
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach((PVOID*)&Real_RoGetActivationFactory, My_RoGetActivationFactory);
        DetourDetach((PVOID*)&Real_RoActivateInstance, My_RoActivateInstance);
        DetourTransactionCommit();
        break;
    }

    return TRUE;
}

