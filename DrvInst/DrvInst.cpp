// DrvInst.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
//#include <iostream> 
#include "CMinifilterController.h"

#pragma comment(lib,"shlwapi")
#pragma comment(lib,"setupapi")

typedef BOOL(WINAPI * pf_Wow64DisableWow64FsRedirection)(__out PVOID *OldValue);
typedef BOOL(WINAPI * pf_Wow64RevertWow64FsRedirection)(__in PVOID OlValue);

pf_Wow64RevertWow64FsRedirection pfn_Revert = NULL;
pf_Wow64DisableWow64FsRedirection pfn_Disable = NULL;

TCHAR g_tszProtectPath[MAX_PATH] = { 0 };
TCHAR g_tszProtectPathDos[MAX_PATH] = { 0 };

VOID ErrMsg(HRESULT hr,
	LPCWSTR  lpFmt,
	...)
{

	LPWSTR   lpSysMsg;
	WCHAR    buf[400];
	size_t   offset;
	va_list  vArgList;


	if (hr != 0) {
		StringCchPrintfW(buf,
			celems(buf),
			L"Error %#lx: ",
			hr);
	}
	else {
		buf[0] = 0;
	}

	offset = wcslen(buf);

	va_start(vArgList,
		lpFmt);
	StringCchVPrintfW(buf + offset,
		celems(buf) - offset,
		lpFmt,
		vArgList);

	va_end(vArgList);

	if (hr != 0) {
		FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			hr,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&lpSysMsg,
			0,
			NULL);
		if (lpSysMsg) {

			offset = wcslen(buf);

			StringCchPrintfW(buf + offset,
				celems(buf) - offset,
				L"\n\nPossible cause:\n\n");

			offset = wcslen(buf);

			StringCchCatW(buf + offset,
				celems(buf) - offset,
				lpSysMsg);

			LocalFree((HLOCAL)lpSysMsg);
		}

		MessageBoxW(NULL,
			buf,
			L"Error",
			MB_ICONERROR | MB_OK);
	}
	else {
		MessageBoxW(NULL,
			buf,
			L"BindView",
			MB_ICONINFORMATION | MB_OK);
	}

	return;
}

HRESULT GetKeyValue( 
	HINF hInf,
	__in LPCWSTR lpszSection,
	__in_opt LPCWSTR lpszKey,
	DWORD  dwIndex,
	__deref_out_opt LPWSTR *lppszValue)
{
	INFCONTEXT  infCtx;
	__range(0, 512) DWORD       dwSizeNeeded;
	HRESULT     hr;

	*lppszValue = NULL;

	if (SetupFindFirstLineW(hInf,
		lpszSection,
		lpszKey,
		&infCtx) == FALSE)
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}

	if (SetupGetStringFieldW(&infCtx,
		dwIndex,
		NULL,
		0,
		&dwSizeNeeded))
	{
		*lppszValue = (LPWSTR)CoTaskMemAlloc(sizeof(WCHAR) * dwSizeNeeded);

		if (!*lppszValue)
		{
			return HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY);
		}

		if (SetupGetStringFieldW(&infCtx,
			dwIndex,
			*lppszValue,
			dwSizeNeeded,
			NULL) == FALSE)
		{

			hr = HRESULT_FROM_WIN32(GetLastError());

			CoTaskMemFree(*lppszValue);
			*lppszValue = NULL;
		}
		else
		{
			hr = S_OK;
		}
	}
	else
	{
		DWORD dwErr = GetLastError();
		hr = HRESULT_FROM_WIN32(dwErr);
	}

	return hr;
}

HRESULT GetPnpID(
	__in LPWSTR lpszInfFile,
	__deref_out_opt LPWSTR *lppszPnpID)
{
	HINF    hInf;
	LPWSTR  lpszModelSection;
	HRESULT hr;

	*lppszPnpID = NULL;

	hInf = SetupOpenInfFileW(lpszInfFile,
		NULL,
		INF_STYLE_WIN4,
		NULL);

	if (hInf == INVALID_HANDLE_VALUE)
	{

		return HRESULT_FROM_WIN32(GetLastError());
	}

	//
	// Read the Model section name from Manufacturer section.
	//

	hr = GetKeyValue(hInf,
		L"Manufacturer",
		NULL,
		1,
		&lpszModelSection);

	if (hr == S_OK)
	{

		//
		// Read PnpID from the Model section.
		//

		hr = GetKeyValue(hInf,
			lpszModelSection,
			NULL,
			2,
			lppszPnpID);

		CoTaskMemFree(lpszModelSection);
	}

	SetupCloseInfFile(hInf);

	return hr;
}

HRESULT InstallSpecifiedComponent(__in LPWSTR lpszInfFile,
	__in LPWSTR lpszPnpID,
	const GUID *pguidClass)
{
	INetCfg    *pnc;
	LPWSTR     lpszApp;
	HRESULT    hr;

	hr = HrGetINetCfg(TRUE,
		L"MyFirewall",
		&pnc,
		&lpszApp);

	if (hr == S_OK) {

		//
		// Install the network component.
		//

		hr = HrInstallNetComponent(pnc,
			lpszPnpID,
			pguidClass,
			lpszInfFile);
		if ((hr == S_OK) || (hr == NETCFG_S_REBOOT)) {

			hr = pnc->Apply();
		}
		else {
			printf("error 1 \n");
			if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
				 
			}
		}

		HrReleaseINetCfg(pnc,
			TRUE);
	}
	else {
		printf("error 2 \n");
		if ((hr == NETCFG_E_NO_WRITE_LOCK) && lpszApp) {
			 
			printf("lock \n");
			CoTaskMemFree(lpszApp);
		}
		else {
			printf("%d", hr);
		}
	}

	return hr;
}
  
HRESULT UninstallComponent(__in LPWSTR lpszInfId)
{
	INetCfg              *pnc;
	INetCfgComponent     *pncc;
	INetCfgClass         *pncClass;
	INetCfgClassSetup    *pncClassSetup;
	LPWSTR               lpszApp;
	GUID                 guidClass;
	OBO_TOKEN            obo;
	HRESULT              hr;

	hr = HrGetINetCfg(TRUE,
		L"MyFirewall",
		&pnc,
		&lpszApp);

	if (hr == S_OK) {

		//
		// Get a reference to the network component to uninstall.
		//

		hr = pnc->FindComponent(lpszInfId,
			&pncc);

		if (hr == S_OK) {

			//
			// Get the class GUID.
			//

			hr = pncc->GetClassGuid(&guidClass);

			if (hr == S_OK) {

				//
				// Get a reference to component's class.
				//

				hr = pnc->QueryNetCfgClass(&guidClass,
					IID_INetCfgClass,
					(PVOID *)&pncClass);
				if (hr == S_OK) {

					//
					// Get the setup interface.
					//

					hr = pncClass->QueryInterface(IID_INetCfgClassSetup,
						(LPVOID *)&pncClassSetup);

					if (hr == S_OK) {

						//
						// Uninstall the component.
						//

						ZeroMemory(&obo,
							sizeof(OBO_TOKEN));

						obo.Type = OBO_USER;

						hr = pncClassSetup->DeInstall(pncc,
							&obo,
							NULL);
						if ((hr == S_OK) || (hr == NETCFG_S_REBOOT)) {

							hr = pnc->Apply();

							if ((hr != S_OK) && (hr != NETCFG_S_REBOOT)) {
								ErrMsg(hr,
									L"Couldn't apply the changes after"
									L" uninstalling %s.",
									lpszInfId);
							}
						}
						else {
							ErrMsg(hr,
								L"Failed to uninstall %s.",
								lpszInfId);
						}

						ReleaseRef(pncClassSetup);
					}
					else {
						ErrMsg(hr,
							L"Couldn't get an interface to setup class.");
					}

					ReleaseRef(pncClass);
				}
				else {
					ErrMsg(hr,
						L"Couldn't get a pointer to class interface "
						L"of %s.",
						lpszInfId);
				}
			}
			else {
				ErrMsg(hr,
					L"Couldn't get the class guid of %s.",
					lpszInfId);
			}

			ReleaseRef(pncc);
		}
		else {
			ErrMsg(hr,
				L"Couldn't get an interface pointer to %s.",
				lpszInfId);
		}

		HrReleaseINetCfg(pnc,
			TRUE);
	}
	else {
		if ((hr == NETCFG_E_NO_WRITE_LOCK) && lpszApp) {
			ErrMsg(hr,
				L"%s currently holds the lock, try later.",
				lpszApp);

			CoTaskMemFree(lpszApp);
		}
		else {
			ErrMsg(hr,
				L"Couldn't get the notify object interface.");
		}
	}

	return hr;
}

BOOL IsWow64()
{
	typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
	LPFN_ISWOW64PROCESS fnIsWow64Process;
	BOOL bIsWow64 = FALSE;
	fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandleA("kernel32"), "IsWow64Process");
	if (NULL != fnIsWow64Process)
	{
		fnIsWow64Process(GetCurrentProcess(), &bIsWow64);
	}
	return bIsWow64;
}

BOOL InstallNdis6Driver()
{
	WCHAR wszCurrentDirectory[MAX_PATH] = { 0 };
	if (GetCurrentDirectoryW(MAX_PATH, wszCurrentDirectory) == 0)
	{
		return FALSE;
	}

	WCHAR cmd[260] = {0};
	WCHAR dir[20];

	if (IsWow64())
		wcscpy_s(dir, 20, L"x64");
	else
		wcscpy_s(dir, 20, L"x86");

	HRESULT hr = StringCbPrintf(cmd, 260, L" -l \"%s\\driver\\%s\\Ndis6Firewall.inf\" -c s -i %s", wszCurrentDirectory,dir, L"MS_Ndis6Firewall");

	if (hr != S_OK)
		return FALSE;
	 
	STARTUPINFO         si = { sizeof(STARTUPINFO) };
	PROCESS_INFORMATION pi = { 0 };
	TCHAR               cmdline[0x1000]; 
  
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	
	PVOID OldValue = NULL;
	if(pfn_Disable)
		pfn_Disable(&OldValue);
	 
	WCHAR wcsProgPath[MAX_PATH];
	wcscpy_s(wcsProgPath, MAX_PATH, L"netcfg.exe");
	DWORD dwRet = SearchPath(NULL, L"netcfg.exe", NULL, MAX_PATH, wcsProgPath, NULL);
	 
	if(pfn_Revert)
		pfn_Revert(&OldValue);
	 
	if (dwRet == 0)
		return FALSE;

	_stprintf_s(cmdline, TEXT("\"%s\" %s"), wcsProgPath, cmd);
	 
	if (CreateProcess(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, wszCurrentDirectory, &si, &pi))
	{
		CloseHandle(pi.hThread);
		if (WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0)
			return TRUE;
	} 

	return FALSE;
}

BOOL UninstallNdis6Driver()
{
	WCHAR wszCurrentDirectory[MAX_PATH] = { 0 };
	if (GetCurrentDirectoryW(MAX_PATH, wszCurrentDirectory) == 0)
	{
		return FALSE;
	}

	WCHAR cmd[260] = { 0 };

	HRESULT hr = StringCbPrintf(cmd, 260, L" -u MS_Ndis6Firewall");
	if (hr != S_OK)
		return FALSE;

	STARTUPINFO         si = { sizeof(STARTUPINFO) };
	PROCESS_INFORMATION pi = { 0 };
	TCHAR               cmdline[0x1000];

	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	PVOID OldValue = NULL;
	if (pfn_Disable)
		pfn_Disable(&OldValue);

	WCHAR wcsProgPath[MAX_PATH];
	wcscpy_s(wcsProgPath, MAX_PATH, L"netcfg.exe");
	DWORD dwRet = SearchPath(NULL, L"netcfg.exe", NULL, MAX_PATH, wcsProgPath, NULL);

	if (pfn_Revert)
		pfn_Revert(&OldValue);

	if (dwRet == 0)
		return FALSE;

	_stprintf_s(cmdline, TEXT("\"%s\" %s"), wcsProgPath, cmd);

	if (CreateProcess(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, wszCurrentDirectory, &si, &pi))
	{
		CloseHandle(pi.hThread);
		if (WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0)
			return TRUE;
	}

	return FALSE;
}

void AdjustPrivilege()
{
	HANDLE hToken;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
	{
		TOKEN_PRIVILEGES tp;
		tp.PrivilegeCount = 1;
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid))
		{
			AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
		}
		CloseHandle(hToken);
	}
}
 
BOOL GetOSVersion(DWORD &dwMajorVersion, DWORD &dwMinorVersion)
{
	HMODULE hModule = LoadLibrary(L"ntdll.dll");
	if (hModule)
	{
		typedef void (WINAPI *GetNtVersionNumbers_PROC)(DWORD*, DWORD*, DWORD*);
		GetNtVersionNumbers_PROC getNtVersionNumbers = NULL;
		getNtVersionNumbers = (GetNtVersionNumbers_PROC)GetProcAddress(hModule, "RtlGetNtVersionNumbers");

		if (getNtVersionNumbers)
		{
			dwMajorVersion = 0;
			dwMinorVersion = 0;
			DWORD dwBuildNumber = 0;

			getNtVersionNumbers(&dwMajorVersion, &dwMinorVersion, &dwBuildNumber);

			dwMajorVersion &= 0xFFFF;
			dwMinorVersion &= 0xFFFF;
			dwBuildNumber &= 0xFFFF;
		}
		FreeLibrary(hModule);
		return TRUE;
	}

	return FALSE;
}

BOOL IsXP()
{
	BOOL bRet = FALSE;

	static DWORD dwMajorVersion = 0;
	static DWORD dwMinorVersion = 0;

	if (dwMajorVersion == 0 && dwMinorVersion == 0)
	{
		GetOSVersion(dwMajorVersion, dwMinorVersion);
	}

	if (dwMajorVersion == 5)
	{
		bRet = TRUE;
	}

	return bRet;
}

VOID GetProtectPath()
{
	TCHAR szDosPath[MAX_PATH] = { 0 };
	TCHAR tszDriver[5] = { 0 };
	TCHAR tszPath[MAX_PATH] = { 0 };
	TCHAR tszFilename[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, szDosPath, MAX_PATH);
	if (PathRemoveFileSpec(szDosPath))
	{
		_tcscpy_s(g_tszProtectPathDos,MAX_PATH, szDosPath);
		_wsplitpath_s(szDosPath, tszDriver, 5, tszPath, MAX_PATH, tszFilename, MAX_PATH, NULL, 0);
		if (0 != QueryDosDevice(tszDriver, g_tszProtectPath, MAX_PATH))
		{
			_tcscat_s(g_tszProtectPath, MAX_PATH, tszPath);
			_tcscat_s(g_tszProtectPath, MAX_PATH, tszFilename);
			
		}
	}
}

void CreateRun()
{
	//添加以下代码
	HKEY   hKey;
	WCHAR pFileName[MAX_PATH] = { 0 };
	//得到程序自身的全路径 
	DWORD dwRet = GetModuleFileNameW(NULL, (LPWCH)pFileName, MAX_PATH);
	PathRemoveFileSpec(pFileName);
	_tcscat_s(pFileName, _T("\\client.exe"));
	 
	//找到系统的启动项 
	LPCTSTR lpRun = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run");
	//打开启动项Key 
	long lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, lpRun, 0, KEY_WRITE, &hKey);
	if (lRet == ERROR_SUCCESS)
	{
		//添加注册
		RegSetValueEx(hKey, _T("FirewallClient"), 0, REG_SZ, (const BYTE*)(LPCSTR)pFileName, MAX_PATH);
		RegCloseKey(hKey);
	}
	  
}
 
void DeleteRun()
{
	//添加以下代码
	HKEY   hKey;
	char pFileName[MAX_PATH] = { 0 };
	//得到程序自身的全路径 
	DWORD dwRet = GetModuleFileNameW(NULL, (LPWCH)pFileName, MAX_PATH);
	//找到系统的启动项 
	LPCTSTR lpRun = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run");
	//打开启动项Key 
	long lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, lpRun, 0, KEY_WRITE, &hKey);
	if (lRet == ERROR_SUCCESS)
	{
		//删除注册
		RegDeleteValue(hKey, _T("FirewallClient"));
		RegCloseKey(hKey);
	}
	 
}

int ControlNdisDriver(int iMode)
{
	if (!IsXP())
	{
		HMODULE hModule = LoadLibrary(_T("Kernel32.dll"));
		pfn_Disable = (pf_Wow64DisableWow64FsRedirection)GetProcAddress(hModule, "Wow64DisableWow64FsRedirection");
		pfn_Revert = (pf_Wow64RevertWow64FsRedirection)GetProcAddress(hModule, "Wow64RevertWow64FsRedirection");

		if (iMode == 0)
		{
			if (InstallNdis6Driver())
			{
				printf("Install success!! \n");
				return 1;
			}
			return 0;
		}
		else
		{
			if (UninstallNdis6Driver())
			{
				printf("UnInstall success!! \n");
				return 1;
			}
			return 0;
		}

	}
	else
	{
		if (iMode == 0)
		{
			LPWSTR  lpszPnpID;
			WCHAR lpszInfFile[260];
			wcscpy_s(lpszInfFile, 260, L"./driver/x86/netsf.inf");

			HRESULT hr = GetPnpID(lpszInfFile, &lpszPnpID);

			if (!SetupCopyOEMInf(L"./driver/x86/netsf_m.inf", L"./", SPOST_PATH, 0, NULL, 0, NULL, NULL))
			{
				printf("SetupCopyOEMInf failed ! \n");
				return 0;
			}

			if (hr == S_OK)
			{

				hr = InstallSpecifiedComponent(lpszInfFile,
					lpszPnpID,
					&GUID_DEVCLASS_NETSERVICE);

				CoTaskMemFree(lpszPnpID);
				return 1;
			}
			else
			{
				printf("GetPnpId Error \n");
				return 0;
			}
		}
		else
		{
			if (0 == UninstallComponent(L"ms_passthru"))
				return 1;
			return 0;
		}

	}

	return 0;
}
 
int ControlMinifilterDriver(int iMode)
{
#define DRIVER_INSTANCE L"FileProtector"
	CMinifilterController MFController;
	if (iMode == 0)
	{
		WCHAR wszDriverPath[MAX_PATH];
		wcscpy_s(wszDriverPath, MAX_PATH, g_tszProtectPathDos);
		wcscat_s(wszDriverPath, MAX_PATH, IsWow64() ? L"\\Driver\\X64\\FileProtector.sys" : L"\\Driver\\X86\\FileProtector.sys");
		
		if (MFController.InstallDriver(DRIVER_INSTANCE, wszDriverPath, L"370030", g_tszProtectPath))
		{
			printf("InstallMinifilterDriver Success \r\n");
			if (MFController.StartDriver(DRIVER_INSTANCE))
			{ 
				return 1;
			}
			else
			{
				printf("Start MinifilterDriver Failed \r\n");
				MFController.DeleteDriver(DRIVER_INSTANCE);
			}
		} 
	}
	else
	{
		MFController.StopDriver(DRIVER_INSTANCE);
		
		if (MFController.DeleteDriver(DRIVER_INSTANCE))
			return 1; 
	}

	return 0;
}

int GetRunMode(int argc, char ** argv)
{
	int iMode = -1;

	if (argc == 2)
	{
		if (strcmp(argv[1], "-i") == 0)
		{
			iMode = 0;
		}
		else if (strcmp(argv[1], "-u") == 0)
		{
			iMode = 1;
		}
	}

	return iMode;
}

void InitializeDrvInst()
{ 
	AdjustPrivilege(); 
	GetProtectPath();
}

void UninitalizeDrvInst()
{
	DeleteRun();
}

void ControlInit(int iMode)
{
	InitializeDrvInst();
	if (iMode == 0)
	{
		CreateRun();
	}
	else
	{
		DeleteRun();
	}

}

int main(int argc, char **argv)
{	  
	int iMode = -1;
	// 0为安装，1为卸载，-1代表出错 
	if ((iMode = GetRunMode(argc,argv)) == -1)
	{
		printf("参数错误！\n -u 卸载驱动 \n -i 安装驱动 \n\n");
		system("pause");
		return 0;
	}
	 
	ControlInit(iMode);
 
	int iRet = ControlNdisDriver(iMode);
	
	if (iRet == 0)
	{
		printf("InstallNdis Error \r\n");
		return 0;
	}
	
	printf("InstallNdis Success \r\n");
	        
	iRet = ControlMinifilterDriver(iMode);

	if (iRet == 0)
	{
		printf("Install Minifilter Error \r\n");
		ControlNdisDriver(1);
		return 0; 
	}
	 
	return 0;
}

 