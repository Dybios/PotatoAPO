#include <windows.h>
#include <string>
#include <iostream>
#include <vector>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <filesystem>
#include <strsafe.h>

BOOL RegDeleteRecurse(HKEY hKeyRoot, LPTSTR lpSubKey);

// --- Structure to hold device information ---
struct AudioDevice {
    std::wstring id;
    std::wstring name;
};

const std::wstring POTATOAPO_GUID = L"{46BB25C9-3D22-4ECE-9481-148C12B0B577}";
const std::wstring SFX_GUID = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},5";
const std::wstring MFX_GUID = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},6";
const std::wstring EFX_GUID = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},7";
const std::wstring BACKUP_REGPATH = L"SOFTWARE\\PotatoAPO\\";
const std::wstring FXPROP_REGPATH = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\Capture\\";
const std::wstring FXPROP_REGKEY = L"\\FxProperties";
const std::string DLL_NAME = "PotatoAPO.dll";
const std::string DLL_INSTALL_PATH = "C:\\ProgramData\\PotatoAPO\\";

std::wstring s2ws(const std::string& str) {
    std::wstring temp;
    int wcharsNum = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    temp.resize(wcharsNum);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &temp[0], wcharsNum);
    return temp;
}

std::string ws2s(const std::wstring& wStr) {
    std::string temp;
    int len = WideCharToMultiByte(CP_UTF8, 0, wStr.c_str(), wStr.size(), NULL, 0, 0, 0);
    temp.resize(len);
    WideCharToMultiByte(CP_UTF8, 0, wStr.c_str(), wStr.size(), &temp[0], len, 0, 0);
    return temp;
}

int main() {
    HRESULT ret = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    if (SUCCEEDED(ret)) {
        std::string audioEndpointGuidStr;
        std::vector<std::wstring> endpointsList;

        HKEY hKeyFxProp = NULL;
        HKEY hKeyBackup = NULL;
        LONG lResult;

        std::wstring apoChainInfo = L"PotatoApoChain";

        // Open backup registry path
        lResult = RegOpenKeyEx(
            HKEY_CURRENT_USER,
            BACKUP_REGPATH.c_str(),
            0,
            KEY_READ,
            &hKeyBackup
        );
        if (lResult == ERROR_SUCCESS) {
            std::cout << "Backup found. Restoring values from backup...\n";
            TCHAR subkeyName[256];
            DWORD subkeyNameSize = sizeof(subkeyName) / sizeof(subkeyName[0]);
            DWORD index = 0;

            // Enumerate subkeys
            std::cout << "Endpoints PotatoAPO is applied to: " << std::endl;
            while (RegEnumKeyEx(hKeyBackup, index, subkeyName, &subkeyNameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                std::wcout << subkeyName << std::endl;
                index++;
                subkeyNameSize = sizeof(subkeyName) / sizeof(subkeyName[0]); // Reset the size for next call
                endpointsList.push_back(subkeyName);
            }
        }
        else {
            // How are we here?
            RegCloseKey(hKeyBackup);
            CoUninitialize();
            std::cout << "PotatoAPO is not installed. Exiting uninstaller." << std::endl;
            system("pause");
            return 1;
        }

        for (auto& endpoint : endpointsList) {
            // Build the target subkey path in FxProperties
            std::wstring wSubKeyPath = FXPROP_REGPATH + endpoint + FXPROP_REGKEY;
            std::wstring wBackupSubKeyPath = BACKUP_REGPATH + endpoint;

            // Open backup key path for reading
            lResult = RegOpenKeyEx(
                HKEY_CURRENT_USER,
                wBackupSubKeyPath.c_str(),
                0,
                KEY_READ,
                &hKeyBackup
            );
            if (lResult != ERROR_SUCCESS) {
                RegCloseKey(hKeyBackup);
                CoUninitialize();
                system("pause");
                return 1;
            }

            // Open fxProp key for writing (to restore values)
            lResult = RegCreateKeyEx(
                HKEY_LOCAL_MACHINE,
                wSubKeyPath.c_str(),
                0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL,
                &hKeyFxProp, NULL
            );
            if (lResult != ERROR_SUCCESS) {
                RegCloseKey(hKeyFxProp);
                RegCloseKey(hKeyBackup);
                CoUninitialize();
                system("pause");
                return 1;
            }

            // Read which APO chain it was installed in
            DWORD dataSize = 0;
            DWORD originalDataType = 0;
            std::vector<BYTE> originalData;
            lResult = RegQueryValueEx(
                hKeyBackup,
                apoChainInfo.c_str(),
                NULL,
                &originalDataType,
                NULL,
                &dataSize
            );
            if (lResult == ERROR_SUCCESS) {
                // Allocate buffer and get the data
                originalData.resize(dataSize);
                lResult = RegQueryValueEx(
                    hKeyBackup,
                    apoChainInfo.c_str(),
                    NULL,
                    &originalDataType,
                    originalData.data(),
                    &dataSize
                );
                if (lResult != ERROR_SUCCESS) {
                    RegCloseKey(hKeyFxProp);
                    RegCloseKey(hKeyBackup);
                    CoUninitialize();
                    system("pause");
                    return 1;
                }

                std::wstring wApoChain(reinterpret_cast<LPCWSTR>(originalData.data()));
                std::string apoChain = ws2s(wApoChain);
                std::wstring wTargetApoChain;
                if (!apoChain.compare("SFX")) {
                    wTargetApoChain = SFX_GUID;
                }
                else if (!apoChain.compare("MFX")) {
                    wTargetApoChain = MFX_GUID;
                }
                else if (!apoChain.compare("EFX")) {
                    wTargetApoChain = EFX_GUID;
                }

                // Get the target APO GUID data and set to the same FxProperties registry path **/
                lResult = RegQueryValueEx(
                    hKeyBackup,
                    wTargetApoChain.c_str(),
                    NULL,
                    &originalDataType,
                    NULL,
                    &dataSize
                );
                if (lResult == ERROR_SUCCESS) {
                    originalData.clear();
                    originalData.resize(dataSize);
                    lResult = RegQueryValueEx(
                        hKeyBackup,
                        wTargetApoChain.c_str(),
                        NULL,
                        &originalDataType,
                        originalData.data(),
                        &dataSize
                    );
                    if (lResult != ERROR_SUCCESS) {
                        RegCloseKey(hKeyFxProp);
                        RegCloseKey(hKeyBackup);
                        CoUninitialize();
                        system("pause");
                        return 1;
                    }

                    lResult = RegSetValueEx(
                        hKeyFxProp,
                        wTargetApoChain.c_str(),
                        0,
                        originalDataType,
                        originalData.data(),
                        originalData.size()
                    );
                    if (lResult != ERROR_SUCCESS) {
                        RegCloseKey(hKeyFxProp);
                        RegCloseKey(hKeyBackup);
                        CoUninitialize();
                        system("pause");
                        return 1;
                    }
                }
                else {
                    lResult = RegDeleteValue(
                        hKeyFxProp,
                        wTargetApoChain.c_str()
                    );
                    if (lResult != ERROR_SUCCESS) {
                        RegCloseKey(hKeyFxProp);
                        RegCloseKey(hKeyBackup);
                        CoUninitialize();
                        system("pause");
                        return 1;
                    }
                }
            }
            else {
                RegCloseKey(hKeyFxProp);
                RegCloseKey(hKeyBackup);
                CoUninitialize();
                system("pause");
                return 1;
            }
        }

        // Close handles
        if (hKeyFxProp != NULL) RegCloseKey(hKeyFxProp);
        RegCloseKey(hKeyBackup);

        // Delete the backup key after successful restoration
        std::cout << std::endl << "Deleting backups..." << std::endl;
        LPTSTR tempPath = (LPTSTR)BACKUP_REGPATH.c_str();
        if (RegDeleteRecurse(HKEY_CURRENT_USER, tempPath)) {
            std::cout << "Backups deleted successfully.\n";
        }

        // Unregister PotatoAPO.dll
        std::wstring command = L"regsvr32.exe";
        std::wstring params = L"/s /u \"" + s2ws((DLL_INSTALL_PATH + DLL_NAME)) + L"\"";
        HINSTANCE hInst = ShellExecuteW(
            NULL,
            NULL,
            command.c_str(),
            params.c_str(),
            NULL,
            SW_HIDE
        );

        CoUninitialize();
    }

    std::cout << "PotatoAPO uninstalled successfully." << std::endl;
    system("pause");
    return 0;
}

BOOL RegDeleteRecurse(HKEY hKeyRoot, LPTSTR lpSubKey)
{
    LPTSTR lpEnd;
    LONG lResult;
    DWORD dwSize;
    TCHAR szName[MAX_PATH];
    HKEY hKey;
    FILETIME ftWrite;

    lResult = RegDeleteKey(hKeyRoot, lpSubKey);
    if (lResult == ERROR_SUCCESS)
        return TRUE;

    lResult = RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_READ, &hKey);
    if (lResult != ERROR_SUCCESS)
    {
        if (lResult == ERROR_FILE_NOT_FOUND) {
            printf("Key not found.\n");
            return TRUE;
        }
        else {
            printf("Error opening key.\n");
            return FALSE;
        }
    }

    // Check for an ending slash and add one if it is missing.
    lpEnd = lpSubKey + lstrlen(lpSubKey);
    if (*(lpEnd - 1) != TEXT('\\'))
    {
        *lpEnd = TEXT('\\');
        lpEnd++;
        *lpEnd = TEXT('\0');
    }

    // Enumerate the keys
    dwSize = MAX_PATH;
    lResult = RegEnumKeyEx(hKey, 0, szName, &dwSize, NULL,
        NULL, NULL, &ftWrite);
    if (lResult == ERROR_SUCCESS)
    {
        do {
            *lpEnd = TEXT('\0');
            StringCchCat(lpSubKey, MAX_PATH * 2, szName);
            if (!RegDeleteRecurse(hKeyRoot, lpSubKey)) {
                break;
            }
            dwSize = MAX_PATH;
            lResult = RegEnumKeyEx(hKey, 0, szName, &dwSize, NULL,
                NULL, NULL, &ftWrite);
        } while (lResult == ERROR_SUCCESS);
    }
    lpEnd--;
    *lpEnd = TEXT('\0');

    RegCloseKey(hKey);

    // Try again to delete the key.
    lResult = RegDeleteKey(hKeyRoot, lpSubKey);
    if (lResult == ERROR_SUCCESS)
        return TRUE;

    return FALSE;
}
