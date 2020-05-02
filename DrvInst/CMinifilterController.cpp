#include "pch.h"
#include "CMinifilterController.h"


CMinifilterController::CMinifilterController()
{
}


CMinifilterController::~CMinifilterController()
{
}


//======================================== ��̬����/ж��sys���� ======================================
// SYS�ļ����������ͬ��Ŀ¼��
// ���������SYS��ΪHelloDDK.sys,��ô��װ����InstallDriver("HelloDDK",".\\HelloDDK.sys","370030"/*Altitude*/);
// ������������ StartDriver("HelloDDK");
// ֹͣ�������� StopDriver("HelloDDK");
// ж��SYSҲ�����Ƶĵ��ù��̣� DeleteDriver("HelloDDK");
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
	//�õ�����������·��
	//GetFullPathName(lpszDriverPath, MAX_PATH, szDriverImagePath, NULL);

	SC_HANDLE hServiceMgr = NULL;// SCM�������ľ��
	SC_HANDLE hService = NULL;// NT��������ķ�����

	//�򿪷�����ƹ�����
	hServiceMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hServiceMgr == NULL)
	{
		// OpenSCManagerʧ��
		CloseServiceHandle(hServiceMgr);
		return FALSE;
	}

	// OpenSCManager�ɹ�
 
	//������������Ӧ�ķ���
	hService = CreateService(hServiceMgr,
		lpszDriverName, // �����������ע����е�����
		lpszDriverName, // ע������������DisplayName ֵ
		SERVICE_ALL_ACCESS, // ������������ķ���Ȩ��
		SERVICE_FILE_SYSTEM_DRIVER, // ��ʾ���صķ������ļ�ϵͳ��������
		SERVICE_SYSTEM_START, // ע������������Start ֵ
		SERVICE_ERROR_IGNORE, // ע������������ErrorControl ֵ
		lpszDriverPath, // ע������������ImagePath ֵ
		L"FSFilter Activity Monitor",// ע������������Group ֵ
		NULL,
		L"FltMgr", // ע������������DependOnService ֵ
		NULL,
		NULL);

	if (hService == NULL)
	{
		if (GetLastError() == ERROR_SERVICE_EXISTS)
		{
			//���񴴽�ʧ�ܣ������ڷ����Ѿ�������
			CloseServiceHandle(hService); // ������
			CloseServiceHandle(hServiceMgr); // SCM���
			return TRUE;
		}
		else
		{
			CloseServiceHandle(hService); // ������
			CloseServiceHandle(hServiceMgr); // SCM���
			return FALSE;
		}
	}
	CloseServiceHandle(hService); // ������
	CloseServiceHandle(hServiceMgr); // SCM���

	//-------------------------------------------------------------------------------------------------------
	// SYSTEM\\CurrentControlSet\\Services\\DriverName\\Instances�ӽ��µļ�ֵ��
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

	RegFlushKey(hKey);//ˢ��ע���
	RegCloseKey(hKey);

 	wcscat_s(szTempStr, 260,L"\\Instances");
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szTempStr, 0, L"", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	// ע������������DefaultInstance ֵ
	wcscpy_s(szTempStr, 260,lpszDriverName);
	wcscat_s(szTempStr, 260, L" Instance");
	if (RegSetValueEx(hKey, L"DefaultInstance", 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)wcslen(szTempStr)*sizeof(WCHAR)) != ERROR_SUCCESS)
	{ 
		return FALSE;
	}
	RegFlushKey(hKey);//ˢ��ע���
	RegCloseKey(hKey);
	//-------------------------------------------------------------------------------------------------------

	//-------------------------------------------------------------------------------------------------------
	// SYSTEM\\CurrentControlSet\\Services\\DriverName\\Instances\\DriverName Instance�ӽ��µļ�ֵ��
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
	// ע������������Altitude ֵ
	wcscpy_s(szTempStr,260, lpszAltitude);
	if (RegSetValueEx(hKey, L"Altitude", 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)wcslen(szTempStr) * sizeof(WCHAR)) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	// ע������������Flags ֵ
	dwData = 0x0;
	if (RegSetValueEx(hKey, L"Flags", 0, REG_DWORD, (CONST BYTE*)&dwData, sizeof(DWORD)) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	RegFlushKey(hKey);//ˢ��ע���
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
			printf("�����Ѿ����� \r\n");
			// �����Ѿ�����
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
