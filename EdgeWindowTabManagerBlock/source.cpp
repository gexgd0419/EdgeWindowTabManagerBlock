#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <detours.h>
#include <stdio.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR lpCmdLine, int nShowCmd)
{
	WCHAR szEdgePath[MAX_PATH] = { 0 };
	DWORD cb = sizeof szEdgePath;

	RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\msedge.exe", NULL,
		RRF_RT_REG_SZ, NULL, szEdgePath, &cb);

	char szDllPath[MAX_PATH];
	GetModuleFileNameA(NULL, szDllPath, MAX_PATH);
	char* pCh = strrchr(szDllPath, '\\');
	if (pCh)
		*pCh = '\0';
	else
		szDllPath[0] = '\0';
	strcat_s(szDllPath, "\\EdgeWindowTabManagerBlockDll.dll");
	
	WCHAR szCmdLine[8192] = { 0 };
	swprintf_s(szCmdLine, L"\"%s\" %s", szEdgePath, lpCmdLine);

	STARTUPINFOW si = { sizeof si };
	PROCESS_INFORMATION pi;

	DetourCreateProcessWithDllW(szEdgePath, szCmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi,
		szDllPath, NULL);

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	return 0;
}