#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "../wrappers.h"
#include "resource.h"
#include <DbgHelp.h>
#pragma comment (lib, "dbghelp.lib")
#include <Shlwapi.h>
#pragma comment (lib, "shlwapi.lib")
#include <CommCtrl.h>
#pragma comment (lib, "comctl32.lib")
#include <commdlg.h>
#pragma comment (lib, "comdlg32.lib")
#include <string>
#include <sstream>
#include <memory>
#include <vector>

#if defined _M_IX86
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

static std::wstring GetRegString(HKEY hKey, LPCWSTR lpSubKey, LPCWSTR lpValue)
{
	DWORD cb = 0;
	LSTATUS err = RegGetValueW(hKey, lpSubKey, lpValue, RRF_RT_REG_SZ, nullptr, nullptr, &cb);
	if (err != ERROR_SUCCESS)
		return {};

	auto pBuf = std::make_unique<BYTE[]>(cb);
	err = RegGetValueW(hKey, lpSubKey, lpValue, RRF_RT_REG_SZ, nullptr, pBuf.get(), &cb);
	if (err != ERROR_SUCCESS)
		return {};

	return std::wstring(reinterpret_cast<LPWSTR>(pBuf.get()));
}

static bool IsEdgePath(LPCWSTR lpPath)
{
	return StrCmpIW(PathFindFileNameW(lpPath), L"msedge.exe") == 0;
}

static std::wstring GetDefaultBrowserPath()
{
	DWORD cch = 0;
	if (AssocQueryStringW(ASSOCF_IS_PROTOCOL, ASSOCSTR_EXECUTABLE, L"https", L"open", nullptr, &cch) != S_FALSE)
		return {};

	auto pBuf = std::make_unique<WCHAR[]>(cch);
	if (FAILED(AssocQueryStringW(ASSOCF_IS_PROTOCOL, ASSOCSTR_EXECUTABLE, L"https", L"open", pBuf.get(), &cch)))
		return {};

	return std::wstring(pBuf.get());
}

static std::wstring GetBrowserPath(HKEY hKeyBrowser)
{
	DWORD cch = 0;
	if (AssocQueryStringByKeyW(0, ASSOCSTR_EXECUTABLE, hKeyBrowser, L"open", nullptr, &cch) != S_FALSE)
		return {};

	auto pBuf = std::make_unique<WCHAR[]>(cch);
	if (FAILED(AssocQueryStringByKeyW(0, ASSOCSTR_EXECUTABLE, hKeyBrowser, L"open", pBuf.get(), &cch)))
		return {};

	return std::wstring(pBuf.get());
}

struct BrowserInfo
{
	std::wstring title;
	std::wstring path;
};

static std::vector<BrowserInfo> GetEdgeVersionList()
{
	std::vector<BrowserInfo> list;
	HKey hKeyBrowsers;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Clients\\StartMenuInternet", 0, KEY_ENUMERATE_SUB_KEYS, &hKeyBrowsers)
		!= ERROR_SUCCESS)
		return {};

	for (int index = 0; ; index++)
	{
		WCHAR szKeyName[256];
		DWORD cchKeyName = 256;
		LRESULT err = RegEnumKeyExW(hKeyBrowsers, index, szKeyName, &cchKeyName, nullptr, nullptr, nullptr, nullptr);
		if (err == ERROR_NO_MORE_ITEMS)
			break;

		HKey hKeyBrowser;
		if (RegOpenKeyExW(hKeyBrowsers, szKeyName, 0, KEY_QUERY_VALUE, &hKeyBrowser) != ERROR_SUCCESS)
			continue;

		BrowserInfo info;

		info.title = GetRegString(hKeyBrowser, nullptr, nullptr);
		if (info.title.empty())
			continue;

		info.path = GetBrowserPath(hKeyBrowser);
		if (info.path.empty() || !IsEdgePath(info.path.c_str()))
			continue;

		list.push_back(std::move(info));
	}

	return list;
}

static bool HasNewEdgeVersions()
{
	DWORD cb = 0;
	LSTATUS err = RegGetValueW(HKEY_CURRENT_USER, L"Software\\EdgeWindowTabManagerBlock", L"LastEdgeVersionList", RRF_RT_REG_MULTI_SZ,
		nullptr, nullptr, &cb);
	if (err != ERROR_SUCCESS)
		return false; // skip checking and silently continue if LastEdgeVersionList does not exist

	std::unique_ptr<BYTE> pBuf(new BYTE[cb]);
	err = RegGetValueW(HKEY_CURRENT_USER, L"Software\\EdgeWindowTabManagerBlock", L"LastEdgeVersionList",
		RRF_RT_REG_MULTI_SZ, nullptr, pBuf.get(), &cb);
	if (err != ERROR_SUCCESS)
		return false;

	LPCWSTR pEdgeVersions = reinterpret_cast<LPCWSTR>(pBuf.get());

	for (auto& edgeVersion : GetEdgeVersionList())
	{
		bool found = false;
		for (LPCWSTR pEdgeVersion = pEdgeVersions; *pEdgeVersion != '\0'; pEdgeVersion += wcslen(pEdgeVersion) + 1)
		{
			if (edgeVersion.title == pEdgeVersion)
			{
				found = true;
				break;
			}
		}
		if (!found)
			return true;
	}

	return false;
}

bool ShowChooseEdgeVersionDlg()
{
	TASKDIALOGCONFIG cfg = { sizeof cfg };
	cfg.dwFlags = TDF_USE_COMMAND_LINKS | TDF_SIZE_TO_CONTENT;
	cfg.dwCommonButtons = TDCBF_CANCEL_BUTTON;
	cfg.pszWindowTitle = MAKEINTRESOURCEW(IDS_PROGRAMNAME);
	cfg.pszMainInstruction = MAKEINTRESOURCEW(IDS_CHOOSE_EDGE_VER);
	cfg.pszFooter = MAKEINTRESOURCEW(IDS_CHOOSE_EDGE_DLG_TIP);
	cfg.pszFooterIcon = TD_INFORMATION_ICON;

	TASKDIALOG_BUTTON btn;
	std::vector<TASKDIALOG_BUTTON> btns;
	std::vector<std::wstring> btntexts; // store button strings to be passed to the API

	btn.nButtonID = 50;
	btn.pszButtonText = MAKEINTRESOURCEW(IDS_FOLLOW_DEFAULT_BROWSER);
	btns.push_back(btn);
	
	btn.nButtonID = 100; // Edge version button IDs start from 100

	auto edgeVersions = GetEdgeVersionList();

	for (auto& edgeVersion : edgeVersions)
	{
		btntexts.push_back(edgeVersion.title + L"\r\n" + edgeVersion.path);
		btn.pszButtonText = btntexts.back().c_str();
		btns.push_back(btn);
		btn.nButtonID++; // increase button ID
	}

	btn.nButtonID = 51;
	btn.pszButtonText = MAKEINTRESOURCEW(IDS_BROWSE_FOR_EDGE_EXE);
	btns.push_back(btn);

	cfg.cButtons = btns.size();
	cfg.pButtons = btns.data();

	int sel = 0;
	TaskDialogIndirect(&cfg, &sel, nullptr, nullptr);

	if (sel >= 100) // an Edge version button
	{
		auto& path = edgeVersions[sel - 100].path;
		RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\EdgeWindowTabManagerBlock", L"EdgePath", REG_SZ,
			path.c_str(), (path.size() + 1) * sizeof(wchar_t));

		// store the current installed Edge version list
		std::wstring edgeVerStr;
		for (auto& edgeVersion : edgeVersions)
		{
			edgeVerStr += edgeVersion.title;
			edgeVerStr += L'\0';
		}

		RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\EdgeWindowTabManagerBlock", L"LastEdgeVersionList", REG_MULTI_SZ,
			edgeVerStr.c_str(), (edgeVerStr.size() + 1) * sizeof(wchar_t));
	}
	else if (sel == 50) // follow default button
	{
		RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\EdgeWindowTabManagerBlock", L"EdgePath", REG_SZ,
			L"<default>", sizeof(L"<default>")); // a special value for default
		RegDeleteKeyValueW(HKEY_CURRENT_USER, L"Software\\EdgeWindowTabManagerBlock", L"LastEdgeVersionList");
	}
	else if (sel == 51) // browse... button
	{
		OPENFILENAMEW ofn = { sizeof ofn };
		WCHAR filename[MAX_PATH] = { 0 };
		ofn.lpstrFilter = L"msedge.exe\0msedge.exe\0*.exe\0*.exe\0";
		ofn.nFilterIndex = 1;
		ofn.lpstrFile = filename;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrInitialDir = L"%ProgramFiles%";
		ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

		if (!GetOpenFileNameW(&ofn))
			return false;

		RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\EdgeWindowTabManagerBlock", L"EdgePath", REG_SZ,
			ofn.lpstrFile, (wcslen(ofn.lpstrFile) + 1) * sizeof(wchar_t));
		RegDeleteKeyValueW(HKEY_CURRENT_USER, L"Software\\EdgeWindowTabManagerBlock", L"LastEdgeVersionList");
	}
	else
	{
		return false;
	}
	return true;
}

std::wstring GetEdgePath()
{
	auto userSelectedPath = GetRegString(HKEY_CURRENT_USER, L"Software\\EdgeWindowTabManagerBlock", L"EdgePath");
	auto edgeVers = GetEdgeVersionList();

	if (!userSelectedPath.empty())
	{
		if (userSelectedPath == L"<default>")
		{
			auto defaultBrowser = GetDefaultBrowserPath();
			if (IsEdgePath(defaultBrowser.c_str()))
				return defaultBrowser;
		}
		else if (PathFileExistsW(userSelectedPath.c_str()) && !HasNewEdgeVersions())
			return userSelectedPath;
		else
			return {}; // make user select another one
	}

	// exactly one Edge version, use it
	if (edgeVers.size() == 1)
		return edgeVers[0].path;

	return {};
}