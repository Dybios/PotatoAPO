#include "stdafx.h"
#include <string>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "PotatoVoiceAPO.h"
#include "ClassFactory.h"

using namespace std;

static HINSTANCE hModule;

BOOL WINAPI DllMain(HINSTANCE hModule, DWORD dwReason, void* lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
		::hModule = hModule;

	return TRUE;
}

STDAPI DllCanUnloadNow()
{
	if (PotatoVoiceAPO::instCount == 0 && ClassFactory::lockCount == 0)
		return S_OK;
	else
		return S_FALSE;
}

STDAPI DllGetClassObject(const CLSID& clsid, const IID& iid, void** ppv)
{
	if (clsid != __uuidof(PotatoVoiceAPO))
		return CLASS_E_CLASSNOTAVAILABLE;

	ClassFactory* factory = new ClassFactory();
	if (factory == NULL)
		return E_OUTOFMEMORY;

	HRESULT hr = factory->QueryInterface(iid, ppv);
	factory->Release();

	return hr;
}

STDAPI DllRegisterServer()
{
	wchar_t filename[1024];
	GetModuleFileNameW(hModule, filename, sizeof(filename) / sizeof(wchar_t));

	HRESULT hr = RegisterAPO(PotatoVoiceAPO::regProperties);
	if (FAILED(hr))
	{
		UnregisterAPO(__uuidof(PotatoVoiceAPO));
		return hr;
	}

	wchar_t* guid;
	StringFromCLSID(__uuidof(PotatoVoiceAPO), &guid);
	wstring guidString(guid);
	CoTaskMemFree(guid);

	HKEY keyHandle;
	RegCreateKeyExW(HKEY_LOCAL_MACHINE, (L"SOFTWARE\\Classes\\CLSID\\" + guidString).c_str(), 0, NULL, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, NULL, &keyHandle, NULL);
	const wchar_t* value = L"MinimalAPO";
	RegSetValueExW(keyHandle, L"", 0, REG_SZ, (const BYTE*)value, (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
	RegCloseKey(keyHandle);

	RegCreateKeyExW(HKEY_LOCAL_MACHINE, (L"SOFTWARE\\Classes\\CLSID\\" + guidString + L"\\InprocServer32").c_str(), 0, NULL, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, NULL, &keyHandle, NULL);
	value = filename;
	RegSetValueExW(keyHandle, L"", 0, REG_SZ, (const BYTE*)value, (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
	value = L"Both";
	RegSetValueExW(keyHandle, L"ThreadingModel", 0, REG_SZ, (const BYTE*)value, (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
	RegCloseKey(keyHandle);

	return S_OK;
}

STDAPI DllUnregisterServer()
{
	wchar_t* guid;
	StringFromCLSID(__uuidof(PotatoVoiceAPO), &guid);
	wstring guidString(guid);
	CoTaskMemFree(guid);

	RegDeleteKeyExW(HKEY_LOCAL_MACHINE, (L"SOFTWARE\\Classes\\CLSID\\" + guidString + L"\\InprocServer32").c_str(), KEY_WOW64_64KEY, 0);
	RegDeleteKeyExW(HKEY_LOCAL_MACHINE, (L"SOFTWARE\\Classes\\CLSID\\" + guidString).c_str(), KEY_WOW64_64KEY, 0);

	HRESULT hr = UnregisterAPO(__uuidof(PotatoVoiceAPO));

	return hr;
}
