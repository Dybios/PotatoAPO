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
#include <thread>
#include <chrono>

bool StopWinService(const std::wstring& serviceName);
bool StartWinService(const std::wstring& serviceName);

// --- Structure to hold device information ---
struct AudioDevice {
    std::wstring id;
    std::wstring name;
};

constexpr const wchar_t* POTATOAPO_GUID = L"{46BB25C9-3D22-4ECE-9481-148C12B0B577}";
constexpr const wchar_t* SFX_GUID = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},5";
constexpr const wchar_t* MFX_GUID = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},6";
constexpr const wchar_t* EFX_GUID = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},7";
constexpr const wchar_t* COMPOSITESFX_GUID = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},13";
constexpr const wchar_t* COMPOSITEMFX_GUID = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},14";
constexpr const wchar_t* COMPOSITEEFX_GUID = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},15";
constexpr const wchar_t* COMPOSITEOSFX_GUID = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},19";
constexpr const wchar_t* COMPOSITEOMFX_GUID = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},20";
constexpr const wchar_t* BACKUP_REGPATH = L"SOFTWARE\\PotatoAPO\\";
constexpr const wchar_t* FXPROP_REGPATH = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\";
constexpr const wchar_t* FXPROP_REGKEY = L"\\FxProperties";
constexpr const char* DLL_NAME = "PotatoAPO.dll";
constexpr const char* DLL_INSTALL_PATH = "C:\\ProgramData\\PotatoAPO\\PotatoAPO.dll";
constexpr const wchar_t* disableAudioDgPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Audio";
constexpr const wchar_t* disableAudioDgKey = L"DisableProtectedAudioDG";

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
    int disableAudioDGvalue = 0;
    HRESULT ret = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    if (SUCCEEDED(ret)) {
        std::string audioEndpointGuidStr;
        std::vector<std::wstring> endpointsList;

        HKEY hKeyFxProp = NULL;
        HKEY hKeyBackup = NULL;
        LONG lResult;
        bool isCapture = false; // Choose either render or capture
        std::wstring apoChainInfo = L"PotatoApoChain";

        // Open backup registry path
        lResult = RegOpenKeyEx(
            HKEY_CURRENT_USER,
            BACKUP_REGPATH,
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
            std::wstring wSubKeyPath = FXPROP_REGPATH;
            if (isCapture) {
                wSubKeyPath += L"Capture\\";
            }
            else {
                wSubKeyPath += L"Render\\";
            }
            wSubKeyPath += endpoint + FXPROP_REGKEY;
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
                std::wstring wTargetApoChain, wTargetApoOffloadChain;
                if (!apoChain.compare("SFX")) {
                    wTargetApoChain = COMPOSITESFX_GUID;
                    wTargetApoOffloadChain = COMPOSITEOSFX_GUID;
                }
                else if (!apoChain.compare("MFX")) {
                    wTargetApoChain = COMPOSITEMFX_GUID;
                    wTargetApoOffloadChain = COMPOSITEOMFX_GUID;
                }
                else if (!apoChain.compare("EFX")) {
                    wTargetApoChain = COMPOSITEEFX_GUID;
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

                    // Restore the offload path chain information as well if available
                    if (!wTargetApoOffloadChain.empty()) {
                        lResult = RegQueryValueEx(
                            hKeyBackup,
                            wTargetApoOffloadChain.c_str(),
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
                                wTargetApoOffloadChain.c_str(),
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
                                wTargetApoOffloadChain.c_str(),
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
                    
                    if (!wTargetApoOffloadChain.empty()) {
                        lResult = RegDeleteValue(
                            hKeyFxProp,
                            wTargetApoOffloadChain.c_str()
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
            }
            else {
                RegCloseKey(hKeyFxProp);
                RegCloseKey(hKeyBackup);
                CoUninitialize();
                system("pause");
                return 1;
            }
        }

        // Re-enable the protected APO registry key
        lResult = RegCreateKeyEx(
            HKEY_LOCAL_MACHINE,
            disableAudioDgPath,
            0,
            NULL,
            REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE,
            NULL,
            &hKeyFxProp,
            NULL
        );
        if (lResult != ERROR_SUCCESS) {
            RegCloseKey(hKeyFxProp);
            CoUninitialize();
            return 1;
        }

        lResult = RegDeleteValueW(hKeyFxProp, disableAudioDgKey);
        if (lResult != ERROR_SUCCESS) {
            RegCloseKey(hKeyFxProp);
            CoUninitialize();
            return 1;
        }

        // Close handles
        if (hKeyFxProp != NULL) RegCloseKey(hKeyFxProp);
        RegCloseKey(hKeyBackup);

        // Delete the backup key after successful restoration
        std::cout << std::endl << "Deleting backups..." << std::endl;
        lResult = RegDeleteTreeW(HKEY_CURRENT_USER, BACKUP_REGPATH);
        if (lResult == ERROR_SUCCESS) {
            std::cout << "Backups deleted successfully.\n";
        }
        else {
            std::cout << "Error deleting backups. You can remove the entries manually.\n";
        }

        // Unregister PotatoAPO.dll
        std::wstring command = L"regsvr32.exe";
        std::wstring params = L"/s /u \"" + s2ws((DLL_INSTALL_PATH)) + L"\"";
        HINSTANCE hInst = ShellExecuteW(
            NULL,
            NULL,
            command.c_str(),
            params.c_str(),
            NULL,
            SW_HIDE
        );

        CoUninitialize();

        // Stop the Windows Audio Service
        const std::wstring serviceName = L"AudioSrv";
        if (!StopWinService(serviceName)) {
            std::wcerr << L"\nFailed to stop Windows Audio service." << std::endl;
        }

        // Delete all installed remnants from ProgramData
        std::filesystem::path installPath = DLL_INSTALL_PATH;
        std::filesystem::path installPathParent = installPath.parent_path();
        try {
            if (!std::filesystem::exists(installPathParent)) {
                return 1;
            }
            std::filesystem::remove_all(installPathParent);
        }
        catch (const std::filesystem::filesystem_error& ex) {
            return 1;
        }

        // Restart Windows Audio Service
        if (!StartWinService(serviceName)) {
            std::wcerr << L"\nFailed to restart Windows Audio service." << std::endl;
        }
    }

    std::cout << "PotatoAPO uninstalled successfully." << std::endl;
    system("pause");
    return 0;
}


std::string GetLastErrorAsString() {
    // Get the error message ID, if any.
    DWORD errorMessageID = GetLastError();
    if (errorMessageID == 0) {
        return std::string(); // No error message has been recorded
    }

    LPSTR messageBuffer = nullptr;
    // Ask Windows to get the corresponding error string
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    std::string message(messageBuffer, size);

    // Free the buffer allocated by FormatMessage
    LocalFree(messageBuffer);

    return message;
}

bool StopWinService(const std::wstring& serviceName) {
    SC_HANDLE schSCManager = NULL;
    SC_HANDLE schService = NULL;
    SERVICE_STATUS_PROCESS ssStatus;
    DWORD dwBytesNeeded;
    DWORD dwTimeout = 30000; // 30 seconds timeout
    DWORD dwStartTime = GetTickCount();

    std::wcout << L"Attempting to stop service: " << serviceName << std::endl;

    // Open a handle to the SCM database
    schSCManager = OpenSCManager(
        NULL,                 // Local computer
        SERVICES_ACTIVE_DATABASE, // Services active database
        SC_MANAGER_ALL_ACCESS // All access to SCM
    );

    if (NULL == schSCManager) {
        std::wcerr << L"OpenSCManager failed: " << GetLastErrorAsString().c_str() << std::endl;
        return false;
    }

    // Open a handle to the service
    schService = OpenService(
        schSCManager,           // SCM database
        serviceName.c_str(),    // Service name
        SERVICE_STOP | SERVICE_QUERY_STATUS | SERVICE_ENUMERATE_DEPENDENTS
    );

    if (NULL == schService) {
        std::wcerr << L"OpenService failed (service: " << serviceName << L"): " << GetLastErrorAsString().c_str() << std::endl;
        CloseServiceHandle(schSCManager);
        return false;
    }

    // Check if the service is already stopped
    if (!QueryServiceStatusEx(
        schService,
        SC_STATUS_PROCESS_INFO,
        (LPBYTE)&ssStatus,
        sizeof(SERVICE_STATUS_PROCESS),
        &dwBytesNeeded))
    {
        std::wcerr << L"QueryServiceStatusEx failed (service: " << serviceName << L"): " << GetLastErrorAsString().c_str() << std::endl;
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return false;
    }

    if (ssStatus.dwCurrentState == SERVICE_STOPPED) {
        std::wcout << L"Service " << serviceName << L" is already stopped." << std::endl;
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return true;
    }

    // Send a stop code to the service
    if (!ControlService(
        schService,
        SERVICE_CONTROL_STOP,
        (LPSERVICE_STATUS)&ssStatus))
    {
        std::wcerr << L"ControlService (stop) failed (service: " << serviceName << L"): " << GetLastErrorAsString().c_str() << std::endl;
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return false;
    }

    // Wait for the service to stop
    while (ssStatus.dwCurrentState != SERVICE_STOPPED) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ssStatus.dwWaitHint)); // Wait suggested by the service

        if (!QueryServiceStatusEx(
            schService,
            SC_STATUS_PROCESS_INFO,
            (LPBYTE)&ssStatus,
            sizeof(SERVICE_STATUS_PROCESS),
            &dwBytesNeeded))
        {
            std::wcerr << L"QueryServiceStatusEx failed during stop wait (service: " << serviceName << L"): " << GetLastErrorAsString().c_str() << std::endl;
            CloseServiceHandle(schService);
            CloseServiceHandle(schSCManager);
            return false;
        }

        if (ssStatus.dwCurrentState == SERVICE_STOPPED) {
            std::wcout << L"Service " << serviceName << L" stopped successfully." << std::endl;
            break;
        }

        if (GetTickCount() - dwStartTime > dwTimeout) {
            std::wcerr << L"Service " << serviceName << L" stop timed out." << std::endl;
            CloseServiceHandle(schService);
            CloseServiceHandle(schSCManager);
            return false;
        }
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return true;
}

// Function to start a Windows service
bool StartWinService(const std::wstring& serviceName) {
    SC_HANDLE schSCManager = NULL;
    SC_HANDLE schService = NULL;
    SERVICE_STATUS_PROCESS ssStatus;
    DWORD dwBytesNeeded;
    DWORD dwTimeout = 30000; // 30 seconds timeout
    DWORD dwStartTime = GetTickCount();

    std::wcout << L"Attempting to start service: " << serviceName << std::endl;

    // Open a handle to the SCM database
    schSCManager = OpenSCManager(
        NULL,                 // Local computer
        SERVICES_ACTIVE_DATABASE, // Services active database
        SC_MANAGER_ALL_ACCESS // All access to SCM
    );

    if (NULL == schSCManager) {
        std::wcerr << L"OpenSCManager failed: " << GetLastErrorAsString().c_str() << std::endl;
        return false;
    }

    // Open a handle to the service
    schService = OpenService(
        schSCManager,           // SCM database
        serviceName.c_str(),    // Service name
        SERVICE_START | SERVICE_QUERY_STATUS
    );

    if (NULL == schService) {
        std::wcerr << L"OpenService failed (service: " << serviceName << L"): " << GetLastErrorAsString().c_str() << std::endl;
        CloseServiceHandle(schSCManager);
        return false;
    }

    // Check if the service is already running
    if (!QueryServiceStatusEx(
        schService,
        SC_STATUS_PROCESS_INFO,
        (LPBYTE)&ssStatus,
        sizeof(SERVICE_STATUS_PROCESS),
        &dwBytesNeeded))
    {
        std::wcerr << L"QueryServiceStatusEx failed (service: " << serviceName << L"): " << GetLastErrorAsString().c_str() << std::endl;
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return false;
    }

    if (ssStatus.dwCurrentState == SERVICE_RUNNING) {
        std::wcout << L"Service " << serviceName << L" is already running." << std::endl;
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return true;
    }

    // Start the service
    if (!StartService(
        schService,  // Handle to service
        0,           // No arguments
        NULL))       // No arguments
    {
        std::wcerr << L"StartService failed (service: " << serviceName << L"): " << GetLastErrorAsString().c_str() << std::endl;
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return false;
    }

    // Wait for the service to start
    while (ssStatus.dwCurrentState != SERVICE_RUNNING) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ssStatus.dwWaitHint)); // Wait suggested by the service

        if (!QueryServiceStatusEx(
            schService,
            SC_STATUS_PROCESS_INFO,
            (LPBYTE)&ssStatus,
            sizeof(SERVICE_STATUS_PROCESS),
            &dwBytesNeeded))
        {
            std::wcerr << L"QueryServiceStatusEx failed during start wait (service: " << serviceName << L"): " << GetLastErrorAsString().c_str() << std::endl;
            CloseServiceHandle(schService);
            CloseServiceHandle(schSCManager);
            return false;
        }

        if (ssStatus.dwCurrentState == SERVICE_RUNNING) {
            std::wcout << L"Service " << serviceName << L" started successfully." << std::endl;
            break;
        }

        if (GetTickCount() - dwStartTime > dwTimeout) {
            std::wcerr << L"Service " << serviceName << L" start timed out." << std::endl;
            CloseServiceHandle(schService);
            CloseServiceHandle(schSCManager);
            return false;
        }
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return true;
}