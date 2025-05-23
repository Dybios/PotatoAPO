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

HRESULT EnumerateCaptureEndpoints(std::vector<AudioDevice>& devices);
std::string ExtractGuidFromEndpointId(const std::wstring& fullEndpointId);

const std::wstring POTATOAPO_GUID = L"{46BB25C9-3D22-4ECE-9481-148C12B0B577}";
const std::wstring SFX_GUID = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},5";
const std::wstring MFX_GUID = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},6";
const std::wstring EFX_GUID = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},7";
const std::wstring BACKUP_REGPATH = L"SOFTWARE\\PotatoAPO\\Backup\\";
const std::string DLL_NAME = "PotatoAPO.dll";

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
    LONG lResult;

    /** Prep: Copy all deps to ProgramData for installation **/
    std::filesystem::path sourceFile = ".\\" + DLL_NAME;
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
        return 1;
    }

    // Register the PotatoAPO DLL
    std::wstring command = L"regsvr32.exe";
    std::wstring params = L"/s \"" + s2ws((destinationDir.string() + DLL_NAME)) + L"\"";
    HINSTANCE hInst = ShellExecuteW(
        NULL,
        NULL,
        command.c_str(),
        params.c_str(),
        NULL,
        SW_HIDE
    );

    HRESULT ret = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(ret)) {
        // Enumerate all capture endpoints
        std::vector<AudioDevice> captureDevices;
        std::cout << "\nAvailable Audio Capture Endpoints:" << std::endl;
        auto res = EnumerateCaptureEndpoints(captureDevices);
        if (FAILED(res)) {
            std::cerr << "Error enumerating capture endpoints: " << res << std::endl;
            CoUninitialize();
            return 1;
        }

        // List all and get user input of their desired endpoint
        for (size_t i = 0; i < captureDevices.size(); ++i) {
            wprintf(L"%zu: %ls\n", i + 1, captureDevices[i].name.c_str());
        }

        int choice = -1;
        std::cout << "Select the endpoint you want to install PotatoAPO on: ";
        std::cin >> choice;

        if (std::cin.fail() || choice < 1 || choice > static_cast<int>(captureDevices.size())) {
            std::cerr << "Invalid selection." << std::endl;
            CoUninitialize();
            return 1;
        }

        if (captureDevices.empty()) {
            std::cout << "No audio capture devices found." << std::endl;
            CoUninitialize();
            return 1;
        }

        AudioDevice selectedDevice = captureDevices[choice - 1];
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

        std::wstring targetApoGuid = SFX_GUID;
        std::string targetApo;
        switch (apoChoice) {
        case 1:
            targetApo = "SFX";
            targetApoGuid = SFX_GUID;
            break;
        case 2:
            targetApo = "MFX";
            targetApoGuid = MFX_GUID;
            break;
        case 3:
            targetApo = "EFX";
            targetApoGuid = EFX_GUID;
            break;
        default:
            break;
        }

        // Construct the full Capture registry path
        std::string subKeyPath = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\Capture\\";
        subKeyPath += audioEndpointGuidStr; // Append the user-provided audio endpoint GUID
        subKeyPath += "\\FxProperties";
        std::wstring wSubKeyPath = s2ws(subKeyPath);

        // Construct the full backup registry path
        std::wstring backupRegPath = BACKUP_REGPATH + s2ws(audioEndpointGuidStr);
        DWORD originalDataType = 0;
        std::vector<BYTE> originalData;

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
        DWORD dataSize = 0;
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

            // Check if writing the value was successful
            if (lResult != ERROR_SUCCESS) {
                RegCloseKey(hKeyBackup); // Close the registry key before exiting
                CoUninitialize();
                goto exit;
            }

            std::cout << std::endl << "Backed up original APO keys to \"HKEY_CURRENT_USER\\" << ws2s(BACKUP_REGPATH) << "\" successfully." << std::endl;
        }
        else {
            std::cout << std::endl << "No SFX value present. Continuing to set the SFX registry value." << std::endl;
        }

        /** Write the registry key with our GUID value **/
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
            REG_SZ,
            (const BYTE*)POTATOAPO_GUID.c_str(),
            (POTATOAPO_GUID.size() + 1) * sizeof(WCHAR)
        );

        // Check if writing the value was successful
        if (lResult != ERROR_SUCCESS) {
            RegCloseKey(hKeyFxProp); // Close the registry key before exiting
            CoUninitialize();
            goto exit;
        }
        std::cout << std::endl << "Successfully installed PotatoAPO." << std::endl;

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

HRESULT EnumerateCaptureEndpoints(std::vector<AudioDevice>& devices) {
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

    // Get the collection of audio capture endpoints
    hr = pEnumerator->EnumAudioEndpoints(
        eCapture,         // Enumerate capture devices
        DEVICE_STATE_ACTIVE, // Only active devices
        &pCollection
    );

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