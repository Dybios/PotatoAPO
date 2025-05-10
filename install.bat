@echo off
title PotatoAPO Installer

REM Check for Administrator rights
openfiles >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: This script requires Administrator privileges.
    echo Please right-click the script file and select "Run as administrator".
    goto :end
)

setlocal enabledelayedexpansion

REM --- Configuration ---
set "DLL_PATH=C:\\PotatoAPO\\PotatoVoiceAPO.dll"
set "NEW_SFX_GUID={46BB25C9-3D22-4ECE-9481-148C12B0B577}"
set "AUDIO_DEVICE_TYPE=Capture"
set "SFX_REG_BASE=HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\%AUDIO_DEVICE_TYPE%"
set "SFX_REG_RELATIVE_PATH=FxProperties"
set "SFX_VALUE_NAME={d04e05a6-594b-4fb6-a80d-01af5eed7d1d},5"
set "BACKUP_REG_BASE=HKEY_CURRENT_USER\Software\PotatoAPO\EffectsBackup"
set "APO_REG_PATH=HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Audio"
set "APO_VALUE_NAME=DisableProtectedAudioDG"
set "APO_VALUE_DATA=1"

set "FRIENDLY_NAME_FAMILY_REG_RELATIVE_PATH=Properties"
set "FRIENDLY_NAME_ENDPOINT_REG_RELATIVE_PATH=Properties"
set "FRIENDLY_NAME_FAMILY_VALUE_NAME={b3f8fa53-0004-438e-9003-51a46e139bfc},6"
set "FRIENDLY_NAME_ENDPOINT_VALUE_NAME={a45c254e-df1c-4efd-8020-67d146a850e0},2"


REM --- Step 0: Configure Audio Service for Unsigned APOs ---
echo.
echo Configuring Audio Service to run unsigned APOs...
reg add "%APO_REG_PATH%" /v "%APO_VALUE_NAME%" /t REG_DWORD /d %APO_VALUE_DATA% /f >nul
if %errorlevel% neq 0 (
    echo WARNING: Failed to configure to allow unsigned APOs to run. Error level: %errorlevel%
	goto :end
) else (
    echo Succesfully configured to allow unsigned APOs to run.
)

REM --- Step 1: Register the DLL ---
mkdir C:\\PotatoAPO
copy .\x64\\Release\\PotatoVoiceAPO.dll %DLL_PATH%

echo.
echo Registering %DLL_PATH%...
regsvr32 "%DLL_PATH%" /s
if %errorlevel% neq 0 (
    echo ERROR: Failed to register %DLL_PATH%. Error level: %errorlevel%
    goto :end
)
echo %DLL_PATH% registered successfully.

REM --- Step 2 & 3: Get and Display Endpoints, Get User Choice ---
echo.
echo Getting available audio %AUDIO_DEVICE_TYPE% endpoints...
set endpoint_count=0
set "endpoint_ids="

REM Loop through subkeys (endpoint GUIDs) under the audio device path
for /f "tokens=*" %%K in ('reg query "%SFX_REG_BASE%" 2^>nul ^| findstr /i "%SFX_REG_BASE%\\"') do (
    REM Extract the endpoint ID (the GUID) from the key path
    set "full_key=%%K"
    set "endpoint_id=!full_key:%SFX_REG_BASE%\=!"
    set "endpoint_id=!endpoint_id:~0,38!" REM Assuming GUIDs are 38 chars including braces

    if not "!endpoint_id!"=="" (
        set "current_endpoint_id=!endpoint_id!"
        set "family_name="
		set "endpoint_name="

        REM Construct the path to the friendly name value for this endpoint
        set "FRIENDLY_NAME_FAMILY_PATH=%SFX_REG_BASE%\!current_endpoint_id!\%FRIENDLY_NAME_FAMILY_REG_RELATIVE_PATH%"
        set "FRIENDLY_NAME_ENDPOINT_PATH=%SFX_REG_BASE%\!current_endpoint_id!\%FRIENDLY_NAME_ENDPOINT_REG_RELATIVE_PATH%"

        REM Query the friendly name family value
        REM Use findstr to reliably parse the value data after the type (REG_SZ)
        for /f "tokens=2*" %%A in ('reg query "!FRIENDLY_NAME_FAMILY_PATH!" /v "%FRIENDLY_NAME_FAMILY_VALUE_NAME%" 2^>nul ^| findstr /i "%FRIENDLY_NAME_FAMILY_VALUE_NAME%"') do (
            set "family_name=%%B"
        )
		
		for /f "tokens=2*" %%A in ('reg query "!FRIENDLY_NAME_ENDPOINT_PATH!" /v "%FRIENDLY_NAME_ENDPOINT_VALUE_NAME%" 2^>nul ^| findstr /i "%FRIENDLY_NAME_ENDPOINT_VALUE_NAME%"') do (
            set "endpoint_name=%%B"
        )
		
		set friendly_name=!family_name! !endpoint_name!

        set "endpoint_id[!endpoint_count!]=!current_endpoint_id!"
        set "endpoint_name[!endpoint_count!]=!friendly_name!"
        echo   [!endpoint_count!] !friendly_name! (ID: !current_endpoint_id!)
        set /a endpoint_count+=1
    )
)

if %endpoint_count% == 0 (
    echo No active %AUDIO_DEVICE_TYPE% endpoints found.
    goto :end
)

echo.
set /p selection="Enter the number of the endpoint to modify: "

if not defined selection goto :invalid_selection
if %selection% LSS 0 goto :invalid_selection
if %selection% GEQ %endpoint_count% goto :invalid_selection

set "selected_endpoint_id=!endpoint_id[%selection%]!"
set "selected_endpoint_name=!endpoint_name[%selection%]!"

echo.
echo Selected endpoint: !selected_endpoint_name! (ID: !selected_endpoint_id!)

REM --- Step 4: Query endpoint's SFX mode guid in fxproperties ---
echo.
echo Querying SFX data for endpoint !selected_endpoint_name!...
set "CURRENT_SFX_REG_PATH=%SFX_REG_BASE%\%selected_endpoint_id%\%SFX_REG_RELATIVE_PATH%"
set ORIGINAL_SFX_DATA=

reg query "%CURRENT_SFX_REG_PATH%" /v "%SFX_VALUE_NAME%" >nul 2>&1
if %errorlevel% equ 0 (
	REM Use findstr to reliably parse the value data after the type (REG_SZ)
	for /f "tokens=3*" %%A in ('reg query "%CURRENT_SFX_REG_PATH%" /v "%SFX_VALUE_NAME%" 2^>nul ^| findstr /i "%SFX_VALUE_NAME%"') do (
		set "ORIGINAL_SFX_DATA=%%A"
	)
	goto :backup_and_update_sfx_reg_path	
) else (
	goto :backup_and_update_sfx_reg_path
)

REM --- Step 6 (If data found) OR Step 7 (Else) ---
:backup_and_update_sfx_reg_path
echo Existing SFX GUID found: !ORIGINAL_SFX_DATA!
echo Backing up original data and overwriting with %NEW_SFX_GUID%...

REM Step 6: Add new registry entry in another location (backup)
set "BACKUP_REG_PATH=%BACKUP_REG_BASE%\%selected_endpoint_id%"
echo Backing up to %BACKUP_REG_PATH%\%SFX_VALUE_NAME%...
reg add "%BACKUP_REG_PATH%" /v "%SFX_VALUE_NAME%" /t REG_SZ /d "!ORIGINAL_SFX_DATA!" /f >nul
if %errorlevel% neq 0 (
    echo WARNING: Failed to create backup registry entry. Error level: %errorlevel%
) else (
    echo Backup successful.
)

REM Step 6 (cont.): Overwrite with our guid data
echo Overwriting original SFX data at %CURRENT_SFX_REG_PATH%\%SFX_VALUE_NAME%...
reg add "%CURRENT_SFX_REG_PATH%" /v "%SFX_VALUE_NAME%" /t REG_SZ /d "%NEW_SFX_GUID%" /f >nul
if %errorlevel% neq 0 (
    echo ERROR: Failed to overwrite original SFX data. Error level: %errorlevel%
    goto :end
)
echo Original SFX data overwritten successfully with %NEW_SFX_GUID%.
goto :reconfigure_service

:create_and_update_sfx_reg_path
REM Step 7: Add key with our guid data directly
echo No existing SFX data found at %CURRENT_SFX_REG_PATH%\%SFX_VALUE_NAME%.
echo Adding new SFX registry entry with %NEW_SFX_GUID%...

REM Ensure the key path exists before adding the value
for /f "delims=\" %%A in ("%CURRENT_SFX_REG_PATH%") do set "PARENT_KEY=%%A"
for /f "tokens=1* delims=\" %%A in ("%CURRENT_SFX_REG_PATH%") do set "CURRENT_PATH=%%A"
:build_path
for /f "tokens=1* delims=\" %%B in ("%%CURRENT_PATH%%") do (
    set "CURRENT_PATH_PART=%%B"
    set "REMAINING_PATH=%%C"
    set "PARENT_KEY=!PARENT_KEY!\!CURRENT_PATH_PART!"
    echo Creating path if necessary: "!PARENT_KEY!"
    reg add "!PARENT_KEY!" /f >nul 2>&1 REM Add key, ignore errors if it exists
    if not defined REMAINING_PATH goto :path_built
    set "CURRENT_PATH=%%C"
    goto :build_path
)
:path_built

reg add "%CURRENT_SFX_REG_PATH%" /v "%SFX_VALUE_NAME%" /t REG_SZ /d "%NEW_SFX_GUID%" /f >nul
 if %errorlevel% neq 0 (
    echo ERROR: Failed to add new SFX data entry. Error level: %errorlevel%
    goto :end
)
echo New SFX data entry added successfully with %NEW_SFX_GUID%.
goto :reconfigure_service

REM --- Step 8: Restart the audio service ---
:reconfigure_service
echo.
echo Reconfiguring the Audio Service (audiosrv)...
sc config audiosrv type= own

REM --- Step 9: Prompt user to reboot system ---
echo.
echo Registry changes have been made and the audio service has been restarted.
echo For changes to take full effect, it is highly recommended to reboot your system.
echo.

goto :end

:invalid_selection
echo.
echo Invalid selection. Please run the script again and enter a valid number.
goto :end

:end
echo.
echo Script finished.
endlocal
pause
exit /b