#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <detours.h>
#include <stdio.h>
#include <DbgHelp.h>
#include "resource.h"
#pragma comment (lib, "dbghelp.lib")

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

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR lpCmdLine, int nShowCmd)
{
	WCHAR szEdgePath[MAX_PATH] = { 0 };
	DWORD cb = sizeof szEdgePath;

	LSTATUS err = RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\msedge.exe", NULL,
		RRF_RT_REG_SZ, NULL, szEdgePath, &cb);
	if (err != ERROR_SUCCESS)
	{
		ReportError(IDS_GET_EDGE_PATH_FAILED, err);
		return HRESULT_FROM_WIN32(err);
	}

	WORD wEdgeBinaryType = GetExecutableMachineType(szEdgePath);
	if (wEdgeBinaryType == 0)
	{
		err = GetLastError();
		ReportError(IDS_GET_EDGE_BIN_TYPE_FAILED, err);
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
	PROCESS_INFORMATION pi;

	if (!DetourCreateProcessWithDllW(szEdgePath, szCmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi,
		szDllPath, NULL))
	{
		err = GetLastError();
		ReportError(IDS_INJECT_DLL_FAILED, err);
		return HRESULT_FROM_WIN32(err);
	}

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	return 0;
}