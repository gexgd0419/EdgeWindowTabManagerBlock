#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <detours.h>
#include <stdio.h>
#include <DbgHelp.h>
#include <shellapi.h>
#include <Shlwapi.h>
#include <string>
#include <CommCtrl.h>
#include "resource.h"
#pragma comment (lib, "dbghelp.lib")
#pragma comment (lib, "shlwapi.lib")

void ReportError(UINT idMessage, DWORD errorCode = 0)
{
	WCHAR buffer[1024] = { 0 };
	LoadStringW(NULL, idMessage, buffer, 1024);
	
	if (errorCode != 0)
	{
		LPWSTR pStr;
		if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
			errorCode, LANG_USER_DEFAULT, (LPWSTR)&pStr, 0, NULL) != 0)
		{
			wcscat_s(buffer, L"\r\n\r\n");
			wcscat_s(buffer, pStr);
			LocalFree(pStr);
		}
	}

	MessageBoxW(NULL, buffer, L"EdgeWindowTabManagerBlock", MB_OK + MB_ICONEXCLAMATION);
}

WORD GetExecutableMachineType(HANDLE hFile)
{
	WORD result = 0;

	HANDLE hMap = CreateFileMappingW(hFile, NULL, PAGE_READONLY | SEC_IMAGE, 0, 0, NULL);
	if (hMap)
	{
		LPVOID pData = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
		if (pData)
		{
			PIMAGE_NT_HEADERS pHdr = ImageNtHeader(pData);
			if (pHdr)
				result = pHdr->FileHeader.Machine;
			UnmapViewOfFile(pData);
		}
		CloseHandle(hMap);
	}

	return result;
}

WORD GetExecutableMachineType(LPCWSTR lpFile)
{
	HANDLE hFile = CreateFileW(lpFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return 0;
	WORD result = GetExecutableMachineType(hFile);
	CloseHandle(hFile);
	return result;
}

WORD GetExecutableMachineType(LPCSTR lpFile)
{
	HANDLE hFile = CreateFileA(lpFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return 0;
	WORD result = GetExecutableMachineType(hFile);
	CloseHandle(hFile);
	return result;
}

DWORD FindEdgeProcessWithWindowTabManager();
bool ShowChooseEdgeVersionDlg();
std::wstring GetEdgePath();

inline LSTATUS WriteRegSZ(HKEY hKey, LPCWSTR lpValueName, LPCWSTR szData)
{
	return RegSetValueExW(hKey, lpValueName, 0, REG_SZ, (const BYTE*)szData, (wcslen(szData) + 1) * sizeof(WCHAR));
}

LSTATUS AddUninstallRegistryKey()
{
	HKEY hKey;
	LSTATUS err;
	err = RegCreateKeyExW(HKEY_LOCAL_MACHINE,
		L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\EdgeWindowTabManagerBlock",
		0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr);
	if (err != ERROR_SUCCESS)
		return err;

	WCHAR uninstallCmdLine[MAX_PATH + 11];
	GetModuleFileNameW(nullptr, uninstallCmdLine, MAX_PATH);
	PathQuoteSpacesW(uninstallCmdLine);
	wcscat_s(uninstallCmdLine, L" --uninstall-ifeo-debugger");

	if (err == ERROR_SUCCESS) err = WriteRegSZ(hKey, L"DisplayName", L"EdgeWindowTabManagerBlock (as Edge debugger)");
	if (err == ERROR_SUCCESS) err = WriteRegSZ(hKey, L"DisplayVersion", L"0.1");
	if (err == ERROR_SUCCESS) err = WriteRegSZ(hKey, L"Publisher", L"gexgd0419 on GitHub");
	if (err == ERROR_SUCCESS) err = WriteRegSZ(hKey, L"UninstallString", uninstallCmdLine);
	if (err == ERROR_SUCCESS) err = WriteRegSZ(hKey, L"HelpLink", L"https://github.com/gexgd0419/EdgeWindowTabManagerBlock");
	if (err == ERROR_SUCCESS) err = WriteRegSZ(hKey, L"URLInfoAbout", L"https://github.com/gexgd0419/EdgeWindowTabManagerBlock");
	if (err == ERROR_SUCCESS) err = WriteRegSZ(hKey, L"URLUpdateInfo", L"https://github.com/gexgd0419/EdgeWindowTabManagerBlock/releases");

	RegCloseKey(hKey);
	return err;
}

bool ShowInstallDebuggerDlg(bool force)
{
	DWORD cb = 0;
	if (RegGetValueW(HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\msedge.exe",
		L"Debugger", RRF_RT_REG_SZ, nullptr, nullptr, &cb) == ERROR_SUCCESS)
	{
		// already installed
		return true;
	}

	DWORD fDoNotAsk = 0;
	cb = sizeof(DWORD);
	if (!force)
	{
		RegGetValueW(HKEY_CURRENT_USER, L"Software\\EdgeWindowTabManagerBlock", L"DebuggerInstallDoNotAsk", RRF_RT_DWORD,
			nullptr, &fDoNotAsk, &cb);
		if (fDoNotAsk)
			return true;
	}

	TASKDIALOGCONFIG cfg = { sizeof cfg };
	cfg.dwFlags = TDF_USE_COMMAND_LINKS | TDF_SIZE_TO_CONTENT;
	cfg.pszWindowTitle = MAKEINTRESOURCEW(IDS_PROGRAMNAME);
	cfg.pszMainInstruction = MAKEINTRESOURCEW(IDS_REGISTER_DEBUGGER_ASK);
	cfg.pszContent = MAKEINTRESOURCEW(IDS_REGISTER_DEBUGGER_REASON);
	cfg.pszFooter = MAKEINTRESOURCEW(IDS_REGISTER_DLG_TIP);
	cfg.pszFooterIcon = TD_INFORMATION_ICON;

	TASKDIALOG_BUTTON btns[] =
	{
		{ IDYES, MAKEINTRESOURCEW(IDS_REGISTER_AS_DEBUGGER) },
		{ IDNO, MAKEINTRESOURCEW(IDS_DO_NOT_REGISTER) },
		{ IDIGNORE, MAKEINTRESOURCEW(IDS_ASK_ME_LATER) },
	};

	cfg.cButtons = std::size(btns);
	cfg.pButtons = btns;

	cfg.pfCallback = [](HWND hwnd, UINT msg, WPARAM, LPARAM, LONG_PTR) -> HRESULT
		{
			if (msg == TDN_DIALOG_CONSTRUCTED)
			{
				SendMessageW(hwnd, TDM_SET_BUTTON_ELEVATION_REQUIRED_STATE, IDYES, TRUE);
			}
			return S_OK;
		};

	int sel = 0;
	TaskDialogIndirect(&cfg, &sel, nullptr, nullptr);

	switch (sel)
	{
	case IDYES:
		// launch itself elevated to perform installation
		WCHAR path[MAX_PATH];
		GetModuleFileNameW(nullptr, path, MAX_PATH);
		ShellExecuteW(nullptr, L"runas", path, L"--install-ifeo-debugger", nullptr, SW_SHOWNORMAL);
		fDoNotAsk = FALSE;
		RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\EdgeWindowTabManagerBlock", L"DebuggerInstallDoNotAsk", REG_DWORD,
			&fDoNotAsk, sizeof(DWORD));
		return false;

	case IDNO:
		fDoNotAsk = TRUE;
		RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\EdgeWindowTabManagerBlock", L"DebuggerInstallDoNotAsk", REG_DWORD,
			&fDoNotAsk, sizeof(DWORD));
		return true; // continue launching Edge

	case IDIGNORE:
		fDoNotAsk = FALSE;
		RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\EdgeWindowTabManagerBlock", L"DebuggerInstallDoNotAsk", REG_DWORD,
			&fDoNotAsk, sizeof(DWORD));
		return true; // continue launching Edge

	case IDCANCEL:
	default:
		return false;
	}
}

int DoInstall()
{
	WCHAR cmdline[MAX_PATH + 13];
	GetModuleFileNameW(nullptr, cmdline, MAX_PATH);
	PathQuoteSpacesW(cmdline);
	wcscat_s(cmdline, L" --ifeo-debug");

	LSTATUS err = AddUninstallRegistryKey();

	if (err == ERROR_SUCCESS)
		err = RegSetKeyValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\msedge.exe",
			L"Debugger", REG_SZ, cmdline, (wcslen(cmdline) + 1) * sizeof(WCHAR));

	if (err != ERROR_SUCCESS)
	{
		RegDeleteKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\EdgeWindowTabManagerBlock", 0, 0);
		ReportError(IDS_REGISTRATION_FAILED, err);
		return HRESULT_FROM_WIN32(err);
	}

	WCHAR msg[256] = { 0 };
	LoadStringW(NULL, IDS_REGISTRATION_COMPLETED, msg, 256);
	MessageBoxW(NULL, msg, L"EdgeWindowTabManagerBlock", MB_ICONINFORMATION);
	return 0;
}

int DoUninstall()
{
	HKEY hKey;
	LSTATUS err = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\msedge.exe",
		0, KEY_QUERY_VALUE | KEY_SET_VALUE, &hKey);
	if (err == ERROR_SUCCESS)
	{
		RegDeleteValueW(hKey, L"Debugger");
		DWORD valueCount = 1;
		RegQueryInfoKeyW(hKey, nullptr, nullptr, nullptr, &valueCount, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		RegCloseKey(hKey);
		if (valueCount == 0)  // when there's no value, remove the entire key
			RegDeleteKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\msedge.exe", 0, 0);
	}
	else if (err != ERROR_FILE_NOT_FOUND) // Ignore when key not found
	{
		ReportError(IDS_UNREGISTRATION_FAILED, err);
		return HRESULT_FROM_WIN32(err);
	}

	err = RegDeleteKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\EdgeWindowTabManagerBlock", 0, 0);
	if (err != 0 && err != ERROR_FILE_NOT_FOUND) // Ignore when key not found
	{
		ReportError(IDS_UNREGISTRATION_FAILED, err);
		return HRESULT_FROM_WIN32(err);
	}

	WCHAR msg[256] = { 0 };
	LoadStringW(NULL, IDS_UNREGISTRATION_COMPLETED, msg, 256);
	MessageBoxW(NULL, msg, L"EdgeWindowTabManagerBlock", MB_ICONINFORMATION);
	return 0;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR lpCmdLine, int nShowCmd)
{
	WCHAR szEdgePath[MAX_PATH] = { 0 };	
	DWORD cb = sizeof szEdgePath;

	// The first parameter can be a msedge.exe path. Check whether it is or not

	bool edgePathInCmdLine = false;
	bool ifeoDebug = false;
	LPWSTR lpRemainCmdLine = PathGetArgsW(lpCmdLine); // points to the 2nd parameter

	wcsncpy_s(szEdgePath, lpCmdLine, lpRemainCmdLine - lpCmdLine); // copy the first parameter
	PathRemoveBlanksW(szEdgePath);
	PathUnquoteSpacesW(szEdgePath);

	if (StrCmpIW(szEdgePath, L"--install-ifeo-debugger") == 0)
	{
		return DoInstall();
	}
	else if (StrCmpIW(szEdgePath, L"--uninstall-ifeo-debugger") == 0)
	{
		return DoUninstall();
	}
	else if (StrCmpIW(szEdgePath, L"--ifeo-debug") == 0)
	{
		ifeoDebug = true;
		// shifts all arguments
		lpCmdLine = lpRemainCmdLine;
		lpRemainCmdLine = PathGetArgsW(lpCmdLine);
		wcsncpy_s(szEdgePath, lpCmdLine, lpRemainCmdLine - lpCmdLine);
		PathRemoveBlanksW(szEdgePath);
		PathUnquoteSpacesW(szEdgePath);
	}
	
	// Check if the first parameter is a path to msedge.exe
	if (StrCmpIW(PathFindFileNameW(szEdgePath), L"msedge.exe") == 0)
	{
		lpCmdLine = lpRemainCmdLine;
		edgePathInCmdLine = true;
	}

	bool ctrlDown = false;
	if (!ifeoDebug && (GetAsyncKeyState(VK_CONTROL) & 0x8000))  // check CTRL key state only when launched directly
	{
		Sleep(1000); // if CTRL pressed now, wait for a second to test again
		if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
			ctrlDown = true;
	}

	if (!edgePathInCmdLine)
	{
		// try to fetch Edge path from the registry
		std::wstring edgePath;

		if (!ctrlDown)
			edgePath = GetEdgePath(); // if CTRL not pressed, fetch Edge path from the registry

		while (edgePath.empty()) // loop until a valid path is selected
		{
			if (!ShowChooseEdgeVersionDlg())
				return HRESULT_FROM_WIN32(ERROR_CANCELLED); // quit on dialog cancellation
			edgePath = GetEdgePath();
		}
		wcscpy_s(szEdgePath, edgePath.c_str());
	}

	LSTATUS err;
	WORD wEdgeBinaryType = GetExecutableMachineType(szEdgePath);
	if (wEdgeBinaryType == 0)
	{
		err = GetLastError();
		ReportError(IDS_GET_EDGE_BIN_TYPE_FAILED, err);
		return HRESULT_FROM_WIN32(err);
	}

	char szDllPath[MAX_PATH];
	GetModuleFileNameA(NULL, szDllPath, MAX_PATH);
	PathRemoveFileSpecA(szDllPath);
	PathAppendA(szDllPath, "EdgeWindowTabManagerBlockDll.dll");

	WORD wDllBinaryType = GetExecutableMachineType(szDllPath);
	if (wDllBinaryType == 0)
	{
		err = GetLastError();
		if (err == ERROR_FILE_NOT_FOUND)
			ReportError(IDS_DLL_NOT_FOUND);
		else
			ReportError(IDS_GET_DLL_BIN_TYPE_FAILED, err);
		return HRESULT_FROM_WIN32(err);
	}
	if (wEdgeBinaryType != wDllBinaryType)
	{
		if (wEdgeBinaryType == IMAGE_FILE_MACHINE_AMD64)
			ReportError(IDS_EDGE_64BIT_NOT_MATCH);
		else if (wEdgeBinaryType == IMAGE_FILE_MACHINE_I386)
			ReportError(IDS_EDGE_32BIT_NOT_MATCH);
		else
			ReportError(IDS_EDGE_UNKNOWN_BIN_TYPE);
		return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
	}

	// Show install debugger dialog after checking bitness
	if (!ifeoDebug && !ShowInstallDebuggerDlg(ctrlDown))
		return 0;

	if (FindEdgeProcessWithWindowTabManager() != 0)
	{
		WCHAR msg[256] = { 0 };
		LoadStringW(NULL, IDS_EDGE_WITH_WTM_RUNNING, msg, 256);
		do
		{
			int ret = MessageBoxW(NULL, msg, L"EdgeWindowTabManagerBlock", MB_CANCELTRYCONTINUE + MB_ICONEXCLAMATION);
			if (ret == IDCANCEL)
				return HRESULT_FROM_WIN32(ERROR_CANCELLED);
			else if (ret == IDCONTINUE)
				break;
		} while (FindEdgeProcessWithWindowTabManager() != 0);
	}

	WCHAR szCmdLine[8192] = { 0 };
	swprintf_s(szCmdLine, L"\"%s\" %s", szEdgePath, lpCmdLine);

	STARTUPINFOW si = { sizeof si };
	GetStartupInfoW(&si);
	si.lpTitle = nullptr;
	si.lpReserved = nullptr;
	si.cbReserved2 = 0;
	si.lpReserved2 = 0;
	PROCESS_INFORMATION pi;

	if (!DetourCreateProcessWithDllW(szEdgePath, szCmdLine, NULL, NULL, TRUE,
		DEBUG_ONLY_THIS_PROCESS,  // avoid recursively launching itself when set as the IFEO debugger of msedge
		NULL, NULL, &si, &pi,
		szDllPath, NULL))
	{
		err = GetLastError();
		ReportError(IDS_INJECT_DLL_FAILED, err);
		return HRESULT_FROM_WIN32(err);
	}

	// we don't want to actually debug, so detach it immediately
	DebugActiveProcessStop(pi.dwProcessId);

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	return 0;
}