#include <windows.h>
#include <string>
#include <iostream>
#include <vector>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <filesystem>

// --- Structure to hold device information ---
struct AudioDevice {
    std::wstring id;
    std::wstring name;
};

HRESULT EnumerateAllEndpoints(std::vector<AudioDevice>& devices, bool isCapture);
std::string ExtractGuidFromEndpointId(const std::wstring& fullEndpointId);

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
constexpr const char* DLL_NAME = "PotatoAPO.dll";
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
    HKEY hKeyFxProp = NULL;
    HKEY hKeyBackup = NULL;
    DWORD dwDisposition;
    DWORD dataSize = 0;
    LONG lResult;
    int disableAudioDGvalue = 1;
    std::wstring command, params;
    HINSTANCE hInst;

    HRESULT ret = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(ret)) {
        int choice = -1;
        bool isCapture = false;
        std::cout << "1. Output (Render)\n2. Input (Capture)\n";
        std::cout << "\nSelect which endpoint type to apply effects to: " << std::endl;
        std::cin >> choice;

        switch (choice) {
        case 1:
            isCapture = false; // Render endpoint type selected
            break;
        case 2:
            isCapture = true; // Capture endpoint type selected
            break;
        default:
            std::cerr << "Invalid selection." << std::endl;
            CoUninitialize();
            return 1;
        }

        // Enumerate all capture endpoints
        std::vector<AudioDevice> devices;
        std::cout << "\nAvailable Audio Endpoints:" << std::endl;
        auto res = EnumerateAllEndpoints(devices, isCapture);
        if (FAILED(res)) {
            std::cerr << "Error enumerating capture endpoints: " << res << std::endl;
            CoUninitialize();
            return 1;
        }

        // List all and get user input of their desired endpoint
        for (size_t i = 0; i < devices.size(); ++i) {
            wprintf(L"%zu: %ls\n", i + 1, devices[i].name.c_str());
        }

        choice = -1;
        std::cout << "Select the endpoint you want to install PotatoAPO on: ";
        std::cin >> choice;

        if (std::cin.fail() || choice < 1 || choice > static_cast<int>(devices.size())) {
            std::cerr << "Invalid selection." << std::endl;
            CoUninitialize();
            return 1;
        }

        if (devices.empty()) {
            std::cout << "No audio devices found." << std::endl;
            CoUninitialize();
            return 1;
        }

        AudioDevice selectedDevice = devices[choice - 1];
        std::string audioEndpointGuidStr = ExtractGuidFromEndpointId(selectedDevice.id);
        std::cout << "Endpoint selected: " << ws2s(selectedDevice.name) << std::endl;
        std::cout << "Device ID: " << audioEndpointGuidStr << std::endl << std::endl;

        // Get user input for the APO chain they want to install in
        int apoChoice = -1;
        std::cout << "1. SFX\n2. MFX\n3. EFX" << std::endl;
        std::cout << "Choose with APO chain to install at: ";
        std::cin >> apoChoice;

        if (std::cin.fail() || apoChoice < 1 || apoChoice > 3) {
            std::cerr << "Invalid selection." << std::endl;
            CoUninitialize();
            return 1;
        }

        std::wstring targetApoGuid, targetApoGuidOffload;
        std::string targetApo;
        switch (apoChoice) {
        case 1:
            targetApo = "SFX";
            targetApoGuid = COMPOSITESFX_GUID;
            targetApoGuidOffload = COMPOSITEOSFX_GUID;
            break;
        case 2:
            targetApo = "MFX";
            targetApoGuid = COMPOSITEMFX_GUID;
            targetApoGuidOffload = COMPOSITEOMFX_GUID;
            break;
        case 3:
            targetApo = "EFX";
            targetApoGuid = EFX_GUID;
            break;
        default:
            break;
        }

        // Construct the full Capture registry path
        std::string subKeyPath = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\";
        if (isCapture) {
            subKeyPath += "Capture\\";
        }
        else {
            subKeyPath += "Render\\";
        }
        subKeyPath += audioEndpointGuidStr; // Append the user-provided audio endpoint GUID
        subKeyPath += "\\FxProperties";
        std::wstring wSubKeyPath = s2ws(subKeyPath);
        std::wcout << L"Selected Endpoint Registry: " << wSubKeyPath << std::endl;

        // Construct the full backup registry path
        std::wstring backupRegPath = BACKUP_REGPATH + s2ws(audioEndpointGuidStr);
        DWORD originalDataType = 0;
        std::vector<BYTE> originalData;

        /** Prep: Done doing housekeeping, now copy all deps to ProgramData for installation **/
        std::filesystem::path sourceFile = DLL_NAME;
        std::filesystem::path destinationDir = "C:\\ProgramData\\PotatoAPO\\";

        try {
            if (!std::filesystem::exists(sourceFile)) {
                return 1;
            }
            if (!std::filesystem::exists(destinationDir)) {
                std::filesystem::create_directories(destinationDir);
            }
            std::filesystem::path destinationFile = destinationDir / sourceFile.filename();
            std::filesystem::copy(sourceFile, destinationFile, std::filesystem::copy_options::overwrite_existing);
        }
        catch (const std::filesystem::filesystem_error& ex) {
            system("pause");
            return 1;
        }

        // Register the PotatoAPO DLL
        command = L"regsvr32.exe";
        params = L"/s \"" + s2ws((destinationDir.string() + DLL_NAME)) + L"\"";
        hInst = ShellExecuteW(
            NULL,
            NULL,
            command.c_str(),
            params.c_str(),
            NULL,
            SW_HIDE
        );

        /** 1. Create/Open the new backup registry key in **/
        lResult = RegCreateKeyEx(
            HKEY_CURRENT_USER,
            backupRegPath.c_str(),
            0,
            NULL,
            REG_OPTION_NON_VOLATILE,
            KEY_WRITE | KEY_READ,
            NULL,
            &hKeyBackup,
            &dwDisposition
        );
        if (lResult != ERROR_SUCCESS) {
            RegCloseKey(hKeyBackup);
            CoUninitialize();
            goto exit;
        }

        /** 2. Open the FxProperties of the chosen endpoint **/
        lResult = RegOpenKeyEx(
            HKEY_LOCAL_MACHINE,
            wSubKeyPath.c_str(),
            0,
            KEY_READ,
            &hKeyFxProp
        );
        if (lResult != ERROR_SUCCESS) {
            RegCloseKey(hKeyFxProp);
            CoUninitialize();
            goto exit;
        }

        /** 3. Backup existing GUID keys from the the selected APO of the selected endpoint GUID **/
        std::cout << "Attempting to backup existing registry value..." << std::endl;
        lResult = RegQueryValueEx(
            hKeyFxProp,
            targetApoGuid.c_str(),
            NULL,
            &originalDataType,
            NULL,
            &dataSize
        );
        if (lResult == ERROR_SUCCESS) {
            // Allocate buffer and get the data
            originalData.resize(dataSize);
            lResult = RegQueryValueEx(
                hKeyFxProp,
                targetApoGuid.c_str(),
                NULL,
                &originalDataType,
                originalData.data(),
                &dataSize
            );
            if (lResult != ERROR_SUCCESS) {
                RegCloseKey(hKeyFxProp);
                CoUninitialize();
                goto exit;
            }

            // Set the data to the backup registry path **/
            lResult = RegSetValueEx(
                hKeyBackup,
                targetApoGuid.c_str(),
                0,
                originalDataType,
                originalData.data(),
                originalData.size()
            );
            if (lResult != ERROR_SUCCESS) {
                RegCloseKey(hKeyBackup); // Close the registry key before exiting
                CoUninitialize();
                goto exit;
            }

            if (!targetApoGuidOffload.empty()) {
                // Backup the offload registries as well if not empty
                originalData.clear();
                lResult = RegQueryValueEx(
                    hKeyFxProp,
                    targetApoGuidOffload.c_str(),
                    NULL,
                    &originalDataType,
                    NULL,
                    &dataSize
                );
                if (lResult == ERROR_SUCCESS) {
                    // Allocate buffer and get the data
                    originalData.resize(dataSize);
                    lResult = RegQueryValueEx(
                        hKeyFxProp,
                        targetApoGuidOffload.c_str(),
                        NULL,
                        &originalDataType,
                        originalData.data(),
                        &dataSize
                    );
                    if (lResult != ERROR_SUCCESS) {
                        RegCloseKey(hKeyFxProp);
                        CoUninitialize();
                        goto exit;
                    }

                    // Set the data to the backup registry path **/
                    lResult = RegSetValueEx(
                        hKeyBackup,
                        targetApoGuidOffload.c_str(),
                        0,
                        originalDataType,
                        originalData.data(),
                        originalData.size()
                    );
                    if (lResult != ERROR_SUCCESS) {
                        RegCloseKey(hKeyBackup); // Close the registry key before exiting
                        CoUninitialize();
                        goto exit;
                    }
                }
            }

            std::cout << std::endl << "Backed up original APO keys to \"HKEY_CURRENT_USER\\" << ws2s(BACKUP_REGPATH) << "\" successfully." << std::endl;
        }
        else {
            std::cout << std::endl << "No previous " << targetApo << " value found. Directly setting the registry value." << std::endl;
        }

        // Set which APO key was the effect installed to (used for uninstallation)
        std::wstring apoChainInfo = L"PotatoApoChain";
        std::wstring wTargetApo = s2ws(targetApo);
        lResult = RegSetValueEx(
            hKeyBackup,
            apoChainInfo.c_str(),
            0,
            REG_SZ,
            (const BYTE*)wTargetApo.c_str(),
            (wTargetApo.size() + 1) * sizeof(WCHAR)
        );
        if (lResult != ERROR_SUCCESS) {
            RegCloseKey(hKeyBackup);
            CoUninitialize();
            goto exit;
        }

        /** Write the registry key with our GUID value **/
        std::vector<std::wstring> multiStrings;
        multiStrings.push_back(POTATOAPO_GUID); // Future-proof for when we want to append the GUIDs

        size_t totalBufferSize = 0;
        for (const auto& s : multiStrings) {
            totalBufferSize += (s.size() + 1); // +1 for the null terminator of each string
        }
        totalBufferSize += 1; // +1 for the final double-null terminator

        // Buffer to hold the combined multi-string data
        std::vector<WCHAR> multiSzBuffer(totalBufferSize);
        WCHAR* currentPtr = multiSzBuffer.data();

        for (const auto& s : multiStrings) {
            wcscpy_s(currentPtr, multiSzBuffer.size() - (currentPtr - multiSzBuffer.data()), s.c_str());
            currentPtr += (s.size() + 1);
        }
        *currentPtr = L'\0';

        std::cout << std::endl << "Installing PotatoAPO..." << std::endl;
        lResult = RegCreateKeyEx(
            HKEY_LOCAL_MACHINE,
            wSubKeyPath.c_str(),
            0,
            NULL,
            REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE,
            NULL,
            &hKeyFxProp,
            &dwDisposition
        );
        if (lResult != ERROR_SUCCESS) {
            RegCloseKey(hKeyBackup);
            CoUninitialize();
            goto exit;
        }

        lResult = RegSetValueEx(
            hKeyFxProp,
            targetApoGuid.c_str(),
            0,
            REG_MULTI_SZ,
            (const BYTE*)multiSzBuffer.data(),
            multiSzBuffer.size() * sizeof(WCHAR)
        );
        if (lResult != ERROR_SUCCESS) {
            RegCloseKey(hKeyFxProp); // Close the registry key before exiting
            CoUninitialize();
            goto exit;
        }

        // Set our GUID to its offload pin as well
        lResult = RegSetValueEx(
            hKeyFxProp,
            targetApoGuidOffload.c_str(),
            0,
            REG_MULTI_SZ,
            (const BYTE*)multiSzBuffer.data(),
            multiSzBuffer.size() * sizeof(WCHAR)
        );
        if (lResult != ERROR_SUCCESS) {
            RegCloseKey(hKeyFxProp); // Close the registry key before exiting
            CoUninitialize();
            goto exit;
        }
        std::cout << std::endl << "Successfully installed PotatoAPO." << std::endl;

        // Disable the protected key to run unsigned APOs
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
            goto exit;
        }

        lResult = RegSetValueEx(
            hKeyFxProp,
            disableAudioDgKey,
            0,
            REG_DWORD,
            (const BYTE*)&disableAudioDGvalue,
            sizeof(disableAudioDGvalue)
        );
        if (lResult != ERROR_SUCCESS) {
            RegCloseKey(hKeyFxProp);
            CoUninitialize();
            goto exit;
        }

        RegCloseKey(hKeyFxProp);
        RegCloseKey(hKeyBackup);
        CoUninitialize();
    }

    // Reconfigure the audio service to allow starting it in a new thread after reboot
    command = L"sc.exe ";
    params = L"config audiosrv type= own";
    hInst = ShellExecuteW(
        NULL,
        NULL,
        command.c_str(),
        params.c_str(),
        NULL,
        SW_HIDE
    );

    std::cout << std::endl << "It is recommended to reboot your system for all changes to take full effect." << std::endl;
exit:
    system("pause");
    return 0;
}

HRESULT EnumerateAllEndpoints(std::vector<AudioDevice>& devices, bool isCapture) {
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDeviceCollection* pCollection = NULL;
    IMMDevice* pDevice = NULL;
    IPropertyStore* pProps = NULL;
    LPWSTR wszId = NULL;
    PROPVARIANT varName;
    HRESULT hr;

    // Create the device enumerator
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        NULL,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator
    );

    if (FAILED(hr)) {
        std::cerr << "CoCreateInstance failed: " << hr << std::endl;
        return hr;
    }

    if (isCapture) {
        // Get the collection of audio capture endpoints
        hr = pEnumerator->EnumAudioEndpoints(
            eCapture,         // Enumerate capture devices
            DEVICE_STATE_ACTIVE, // Only active devices
            &pCollection
        );
    }
    else {
        // Get the collection of audio render endpoints
        hr = pEnumerator->EnumAudioEndpoints(
            eRender,         // Enumerate render devices
            DEVICE_STATE_ACTIVE, // Only active devices
            &pCollection
        );
    }

    if (FAILED(hr)) {
        std::cerr << "EnumAudioEndpoints failed: " << hr << std::endl;
        pEnumerator->Release();
        return hr;
    }

    UINT count;
    pCollection->GetCount(&count);

    if (count == 0) {
        std::cout << "(No active capture devices found)" << std::endl;
    }

    // Iterate through each device in the collection
    for (UINT i = 0; i < count; ++i) {
        // Get the i-th device
        hr = pCollection->Item(i, &pDevice);
        if (FAILED(hr)) { continue; }

        // Get the device ID string
        hr = pDevice->GetId(&wszId);
        if (FAILED(hr)) { pDevice->Release(); continue; }

        // Open the property store for the device
        hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
        if (FAILED(hr)) { CoTaskMemFree(wszId); pDevice->Release(); continue; }

        // Initialize PROPVARIANT for the device friendly name
        PropVariantInit(&varName);

        // Get the friendly name property
        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
        if (FAILED(hr)) {
            // If friendly name not available, use the ID as a fallback
            devices.push_back({ wszId, std::wstring(wszId) });
        }
        else {
            // Store the device ID and friendly name
            devices.push_back({ wszId, varName.pwszVal });
        }

        // Clean up COM objects for the current device
        PropVariantClear(&varName);
        if (pProps) pProps->Release();
        CoTaskMemFree(wszId);
        if (pDevice) pDevice->Release();
    }

    // Clean up COM objects for the collection and enumerator
    if (pCollection) pCollection->Release();
    if (pEnumerator) pEnumerator->Release();

    return S_OK;
}

std::string ExtractGuidFromEndpointId(const std::wstring& fullEndpointId) {
    std::string epIdStr = ws2s(fullEndpointId);

    size_t firstBrace1 = fullEndpointId.find('{');
    size_t lastBrace1 = fullEndpointId.find('}', firstBrace1);
    if (lastBrace1 == std::string::npos || lastBrace1 < firstBrace1) {
        return "";
    }

    size_t firstBrace2 = fullEndpointId.find('{', lastBrace1);
    size_t lastBrace2 = fullEndpointId.find('}', firstBrace2);

    // Extract the substring starting from the second opening brace
    // and with the calculated length of 38 characters.
    std::string extractedGuid = epIdStr.substr(firstBrace2, 38);

    return extractedGuid;
}