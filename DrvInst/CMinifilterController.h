#pragma once
class CMinifilterController
{
public:
	CMinifilterController();
	~CMinifilterController();

	BOOL InstallDriver(const WCHAR* lpszDriverName, const WCHAR* lpszDriverPath, const WCHAR* lpszAltitude,const WCHAR* lpwszInstPath);
	BOOL StartDriver(const WCHAR* lpszDriverName);
	BOOL StopDriver(const WCHAR* lpszDriverName);
	BOOL DeleteDriver(const WCHAR* lpszDriverName);
};

