#include "pch.h"
#include "CMinifilterController.h"


CMinifilterController::CMinifilterController()
{
}


CMinifilterController::~CMinifilterController()
{
}


//======================================== 动态加载/卸载sys驱动 ======================================
// SYS文件跟程序放在同个目录下
// 如果产生的SYS名为HelloDDK.sys,那么安装驱动InstallDriver("HelloDDK",".\\HelloDDK.sys","370030"/*Altitude*/);
// 启动驱动服务 StartDriver("HelloDDK");
// 停止驱动服务 StopDriver("HelloDDK");
// 卸载SYS也是类似的调用过程， DeleteDriver("HelloDDK");
//====================================================================================================

BOOL CMinifilterController::InstallDriver(const WCHAR* lpszDriverName, const WCHAR* lpszDriverPath, const WCHAR* lpszAltitude,const WCHAR* lpwszInstPath)
{
	WCHAR szTempStr[MAX_PATH];
	HKEY hKey;
	DWORD dwData;
	  
	if (NULL == lpszDriverName || NULL == lpszDriverPath)
	{
		return FALSE;
	}
	//得到完整的驱动路径
	//GetFullPathName(lpszDriverPath, MAX_PATH, szDriverImagePath, NULL);

	SC_HANDLE hServiceMgr = NULL;// SCM管理器的句柄
	SC_HANDLE hService = NULL;// NT驱动程序的服务句柄

	//打开服务控制管理器
	hServiceMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hServiceMgr == NULL)
	{
		// OpenSCManager失败
		CloseServiceHandle(hServiceMgr);
		return FALSE;
	}

	// OpenSCManager成功
 
	//创建驱动所对应的服务
	hService = CreateService(hServiceMgr,
		lpszDriverName, // 驱动程序的在注册表中的名字
		lpszDriverName, // 注册表驱动程序的DisplayName 值
		SERVICE_ALL_ACCESS, // 加载驱动程序的访问权限
		SERVICE_FILE_SYSTEM_DRIVER, // 表示加载的服务是文件系统驱动程序
		SERVICE_SYSTEM_START, // 注册表驱动程序的Start 值
		SERVICE_ERROR_IGNORE, // 注册表驱动程序的ErrorControl 值
		lpszDriverPath, // 注册表驱动程序的ImagePath 值
		L"FSFilter Activity Monitor",// 注册表驱动程序的Group 值
		NULL,
		L"FltMgr", // 注册表驱动程序的DependOnService 值
		NULL,
		NULL);

	if (hService == NULL)
	{
		if (GetLastError() == ERROR_SERVICE_EXISTS)
		{
			//服务创建失败，是由于服务已经创立过
			CloseServiceHandle(hService); // 服务句柄
			CloseServiceHandle(hServiceMgr); // SCM句柄
			return TRUE;
		}
		else
		{
			CloseServiceHandle(hService); // 服务句柄
			CloseServiceHandle(hServiceMgr); // SCM句柄
			return FALSE;
		}
	}
	CloseServiceHandle(hService); // 服务句柄
	CloseServiceHandle(hServiceMgr); // SCM句柄

	//-------------------------------------------------------------------------------------------------------
	// SYSTEM\\CurrentControlSet\\Services\\DriverName\\Instances子健下的键值项
	//-------------------------------------------------------------------------------------------------------
	wcscpy_s(szTempStr,260, L"SYSTEM\\CurrentControlSet\\Services\\");
	wcscat_s(szTempStr,260, lpszDriverName);
	
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szTempStr, 0, L"", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	
	if (RegSetValueEx(hKey, L"InstallPath", 0, REG_SZ, (CONST BYTE*)lpwszInstPath, (DWORD)wcslen(lpwszInstPath)*sizeof(WCHAR)) != ERROR_SUCCESS)
	{
		return FALSE;
	}

	RegFlushKey(hKey);//刷新注册表
	RegCloseKey(hKey);

 	wcscat_s(szTempStr, 260,L"\\Instances");
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szTempStr, 0, L"", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	// 注册表驱动程序的DefaultInstance 值
	wcscpy_s(szTempStr, 260,lpszDriverName);
	wcscat_s(szTempStr, 260, L" Instance");
	if (RegSetValueEx(hKey, L"DefaultInstance", 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)wcslen(szTempStr)*sizeof(WCHAR)) != ERROR_SUCCESS)
	{ 
		return FALSE;
	}
	RegFlushKey(hKey);//刷新注册表
	RegCloseKey(hKey);
	//-------------------------------------------------------------------------------------------------------

	//-------------------------------------------------------------------------------------------------------
	// SYSTEM\\CurrentControlSet\\Services\\DriverName\\Instances\\DriverName Instance子健下的键值项
	//-------------------------------------------------------------------------------------------------------
	wcscpy_s(szTempStr,260, L"SYSTEM\\CurrentControlSet\\Services\\");
	wcscat_s(szTempStr, 260, lpszDriverName);
	wcscat_s(szTempStr, 260, L"\\Instances\\");
	wcscat_s(szTempStr, 260, lpszDriverName);
	wcscat_s(szTempStr,260, L" Instance");
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szTempStr, 0, L"", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	// 注册表驱动程序的Altitude 值
	wcscpy_s(szTempStr,260, lpszAltitude);
	if (RegSetValueEx(hKey, L"Altitude", 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)wcslen(szTempStr) * sizeof(WCHAR)) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	// 注册表驱动程序的Flags 值
	dwData = 0x0;
	if (RegSetValueEx(hKey, L"Flags", 0, REG_DWORD, (CONST BYTE*)&dwData, sizeof(DWORD)) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	RegFlushKey(hKey);//刷新注册表
	RegCloseKey(hKey);
	//-------------------------------------------------------------------------------------------------------

	return TRUE;
}

BOOL CMinifilterController::StartDriver(const WCHAR* lpszDriverName)
{
	SC_HANDLE schManager;
	SC_HANDLE schService;
  
	if (NULL == lpszDriverName)
	{
		printf("NULL == lpszDriverName \r\n");
		return FALSE;
	}

	schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == schManager)
	{
		printf("NULL == schManager \r\n");
		CloseServiceHandle(schManager);
		return FALSE;
	}
	schService = OpenService(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
	if (NULL == schService)
	{
		printf("NULL == schService \r\n");
		CloseServiceHandle(schService);
		CloseServiceHandle(schManager);
		return FALSE;
	}

	if (!StartService(schService, 0, NULL))
	{
		printf("!StartService(schService, 0, NULL) LastError:%d\r\n", GetLastError());
		CloseServiceHandle(schService);
		CloseServiceHandle(schManager);
		
		if (GetLastError() == ERROR_SERVICE_ALREADY_RUNNING)
		{
			printf("服务已经开启 \r\n");
			// 服务已经开启
			return TRUE;
		}
		return FALSE;
	}

	CloseServiceHandle(schService);
	CloseServiceHandle(schManager);

	return TRUE;
}

BOOL CMinifilterController::StopDriver(const WCHAR* lpszDriverName)
{
	SC_HANDLE schManager;
	SC_HANDLE schService;
	SERVICE_STATUS svcStatus;
	bool bStopped = false;

	schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == schManager)
	{
		return FALSE;
	}
	schService = OpenService(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
	if (NULL == schService)
	{
		CloseServiceHandle(schManager);
		return FALSE;
	}
	if (!ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus) && (svcStatus.dwCurrentState != SERVICE_STOPPED))
	{
		CloseServiceHandle(schService);
		CloseServiceHandle(schManager);
		return FALSE;
	}

	CloseServiceHandle(schService);
	CloseServiceHandle(schManager);

	return TRUE;
}

BOOL CMinifilterController::DeleteDriver(const WCHAR* lpszDriverName)
{
	SC_HANDLE schManager;
	SC_HANDLE schService;
	SERVICE_STATUS svcStatus;

	schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == schManager)
	{
		return FALSE;
	}
	schService = OpenService(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
	if (NULL == schService)
	{
		CloseServiceHandle(schManager);
		return FALSE;
	}
	ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus);
	if (!DeleteService(schService))
	{
		CloseServiceHandle(schService);
		CloseServiceHandle(schManager);
		return FALSE;
	}
	CloseServiceHandle(schService);
	CloseServiceHandle(schManager);

	return TRUE;
}
