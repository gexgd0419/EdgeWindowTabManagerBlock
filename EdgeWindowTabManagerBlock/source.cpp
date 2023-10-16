#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <detours.h>
#include <stdio.h>
#include <DbgHelp.h>
#pragma comment (lib, "dbghelp.lib")

void ReportError(LPCWSTR lpMessage, DWORD errorCode = 0)
{
	WCHAR buffer[1024] = { 0 };
	wcscpy_s(buffer, lpMessage);
	
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

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR lpCmdLine, int nShowCmd)
{
	WCHAR szEdgePath[MAX_PATH] = { 0 };
	DWORD cb = sizeof szEdgePath;

	LSTATUS err = RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\msedge.exe", NULL,
		RRF_RT_REG_SZ, NULL, szEdgePath, &cb);
	if (err != ERROR_SUCCESS)
	{
		ReportError(L"Cannot query the installation path of Microsoft Edge.", err);
		return HRESULT_FROM_WIN32(err);
	}

	WORD wEdgeBinaryType = GetExecutableMachineType(szEdgePath);
	if (wEdgeBinaryType == 0)
	{
		err = GetLastError();
		ReportError(L"Cannot query the executable type of Microsoft Edge.", err);
		return HRESULT_FROM_WIN32(err);
	}

	char szDllPath[MAX_PATH];
	GetModuleFileNameA(NULL, szDllPath, MAX_PATH);
	char* pCh = strrchr(szDllPath, '\\');
	if (pCh)
		*pCh = '\0';
	else
		szDllPath[0] = '\0';
	strcat_s(szDllPath, "\\EdgeWindowTabManagerBlockDll.dll");

	WORD wDllBinaryType = GetExecutableMachineType(szDllPath);
	if (wDllBinaryType == 0)
	{
		err = GetLastError();
		if (err == ERROR_FILE_NOT_FOUND)
			ReportError(L"EdgeWindowTabManagerBlockDll.dll does not exist. It should be in the same directory as this program.");
		else
			ReportError(L"Cannot query the executable type of EdgeWindowTabManagerBlockDll.dll.", err);
		return HRESULT_FROM_WIN32(err);
	}
	if (wEdgeBinaryType != wDllBinaryType)
	{
		if (wEdgeBinaryType == IMAGE_FILE_MACHINE_AMD64)
			ReportError(L"The Microsoft Edge on your system is 64-bit. Please use the 64-bit version of this program.");
		else if (wEdgeBinaryType == IMAGE_FILE_MACHINE_I386)
			ReportError(L"The Microsoft Edge on your system is 32-bit. Please use the 32-bit version of this program.");
		else
			ReportError(L"The Microsoft Edge on your system has an unsupported binary type.");
		return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
	}

	WCHAR szCmdLine[8192] = { 0 };
	swprintf_s(szCmdLine, L"\"%s\" %s", szEdgePath, lpCmdLine);

	STARTUPINFOW si = { sizeof si };
	PROCESS_INFORMATION pi;

	if (!DetourCreateProcessWithDllW(szEdgePath, szCmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi,
		szDllPath, NULL))
	{
		err = GetLastError();
		ReportError(L"Cannot launch Microsoft Edge process with injected DLL.", err);
		return HRESULT_FROM_WIN32(err);
	}

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	return 0;
}