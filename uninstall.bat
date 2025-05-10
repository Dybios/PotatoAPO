@echo off
title PotatoAPO Installer Uninstaller

REM Check for Administrator rights
openfiles >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: This script requires Administrator privileges.
    echo Please right-click the script file and select "Run as administrator".
    goto :end
)

setlocal enabledelayedexpansion

REM --- Configuration (MUST MATCH INSTALLER SCRIPT) ---
set "DLL_PATH=C:\\PotatoAPO\\PotatoVoiceAPO.dll"
set "AUDIO_DEVICE_TYPE=Capture"
set "SFX_REG_BASE=HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\%AUDIO_DEVICE_TYPE%"
set "SFX_REG_RELATIVE_PATH=FxProperties"
set "SFX_VALUE_NAME={d04e05a6-594b-4fb6-a80d-01af5eed7d1d},5"
set "BACKUP_REG_BASE=HKEY_CURRENT_USER\Software\PotatoAPO\EffectsBackup"
set "APO_REG_PATH=HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Audio"
set "APO_VALUE_NAME=DisableProtectedAudioDG"
set "APO_RESET_DATA=0"

REM --- Step 0: Reset Audio Service Configuration for Unsigned APOs ---
echo.
echo Resetting Audio Service configuration for unsigned APOs...
reg query "%APO_REG_PATH%" /v "%APO_VALUE_NAME%" >nul 2>&1
if %errorlevel% equ 0 (
    echo Registry value "%APO_VALUE_NAME%" found. Setting data back to %APO_RESET_DATA%.
    reg add "%APO_REG_PATH%" /v "%APO_VALUE_NAME%" /t REG_DWORD /d %APO_RESET_DATA% /f >nul
    if %errorlevel% neq 0 (
        echo WARNING: Failed to reenable the APO signature checking. Error level: %errorlevel%
    ) else (
        echo Succesfully reenabled the APO signature checking.
    )
) else (
    echo Registry value "%APO_VALUE_NAME%" not found. No action needed for this value.
)

REM --- Step 1: Check if our custom backup location exists ---
echo.
echo Checking for backup data at %BACKUP_REG_BASE%...

reg query "%BACKUP_REG_BASE%" >nul 2>&1
if %errorlevel% equ 0 (
    echo Backup location found. Processing backed-up endpoints...

    REM --- Step 2: Iterate through endpoint IDs stored as subkeys in backup ---
    REM This loop parses the output of reg query to find subkey names (which are the endpoint IDs)
    for /f "tokens=*" %%K in ('reg query "%BACKUP_REG_BASE%" 2^>nul ^| findstr /i "HKEY_CURRENT_USER\\Software\\PotatoAPO\\EffectsBackup\\{"') do (
        REM Extract the endpoint ID from the key path
        set "full_key=%%K"
        set "endpoint_id=!full_key:HKEY_CURRENT_USER\Software\PotatoAPO\PotatoAPO\=!"
        set "endpoint_id=!endpoint_id:~0,38!" REM Assuming GUIDs are 38 chars including braces

        if not "!endpoint_id!"=="" (
            echo.
            echo Processing endpoint ID found in backup: !endpoint_id!

            set "BACKUP_REG_PATH=!BACKUP_REG_BASE!\!endpoint_id!"
            set "CURRENT_SFX_REG_PATH=!SFX_REG_BASE!\!endpoint_id!\!SFX_REG_RELATIVE_PATH!"

            REM Check if the SFX data value exists in the backup for this endpoint
            reg query "!BACKUP_REG_PATH!" /v "%SFX_VALUE_NAME%" >nul 2>&1
            if %errorlevel% equ 0 (
                REM --- Step 3 (If backup data exists): Copy backup data and delete custom path ---
                echo Backup SFX data found for endpoint !endpoint_id!. Restoring original SFX GUID...

                set ORIGINAL_SFX_DATA=
                REM Use findstr to reliably parse the value data after the type (REG_SZ)
                for /f "tokens=3*" %%A in ('reg query "!BACKUP_REG_PATH!" /v "%SFX_VALUE_NAME%" 2^>nul ^| findstr /i "%SFX_VALUE_NAME%"') do (
                    set "ORIGINAL_SFX_DATA=%%A"
                )

                if defined ORIGINAL_SFX_DATA (
                    echo Restoring original SFX GUID: !ORIGINAL_SFX_DATA! to !CURRENT_SFX_REG_PATH!\!SFX_VALUE_NAME!...
                    reg add "!CURRENT_SFX_REG_PATH!" /v "%SFX_VALUE_NAME%" /t REG_SZ /d "!ORIGINAL_SFX_DATA!" /f >nul
                    if %errorlevel% neq 0 (
                        echo ERROR: Failed to restore original SFX data for endpoint !endpoint_id!. Error level: %errorlevel%
                        echo You may need to manually fix the registry entry at !CURRENT_SFX_REG_PATH!\!SFX_VALUE_NAME!.
						goto :end
                    ) else (
                        echo Original SFX data restored successfully for endpoint !endpoint_id!.
                    )
                ) else (
                    echo WARNING: Backup value found for endpoint !endpoint_id!, but data could not be read or not found.
                )

                echo Deleting backup registry path for endpoint !endpoint_id! at !BACKUP_REG_BASE!\!endpoint_id!...
                REM Delete the endpoint-specific backup subkey (which is the endpoint ID itself)
                reg delete "!BACKUP_REG_BASE!\!endpoint_id!" /f >nul
                if %errorlevel% neq 0 (
                    echo WARNING: Failed to delete backup registry path for endpoint !endpoint_id!. Error level: %errorlevel%
                ) else (
                    echo Backup path deleted for endpoint !endpoint_id!.
                )

            ) else (
                REM --- Step 4 (If backup data does not exist): Delete the values in sfx guid directly ---
                echo No specific backup SFX data found for endpoint !endpoint_id!. Deleting the current SFX value.
                goto :delete_sfx_value_for_endpoint
            )
        )
    )

    REM Optional: Clean up the base backup key if it's now empty
    reg query "%BACKUP_REG_BASE%" >nul 2>&1
    if %errorlevel% neq 0 (
        echo Base backup location %BACKUP_REG_BASE% is now empty or deleted.
    ) else (
        echo Base backup location %BACKUP_REG_BASE% may still contain other data.
    )

) else (
    echo No backup location found at %BACKUP_REG_BASE%.
	:delete_sfx_value_for_endpoint
    echo Deleting SFX value at !CURRENT_SFX_REG_PATH!\!SFX_VALUE_NAME! for endpoint !endpoint_id!...
    reg delete "!CURRENT_SFX_REG_PATH!" /v "%SFX_VALUE_NAME%" /f >nul
    if %errorlevel% equ 0 (
        echo SFX value deleted successfully for endpoint !endpoint_id!.
    ) else (
        echo WARNING: Failed to delete SFX value or value did not exist for endpoint !endpoint_id!. Error level: %errorlevel%
    )
)

REM --- Step 5: Unregister the DLL ---
echo.
echo Unregistering %DLL_PATH%...
regsvr32 "%DLL_PATH%" /u /s
if %errorlevel% neq 0 (
    echo WARNING: Failed to unregister %DLL_PATH%. Error level: %errorlevel%
    echo You may need to manually unregister it using "regsvr32 /u %DLL_PATH%".
) else (
    echo %DLL_PATH% unregistered successfully.
)

REM --- Step 6: Prompt user to reboot system ---
echo.
echo Registry changes have been made and the audio service has been restarted.
echo For changes to take full effect, it is highly recommended to reboot your system.
echo.
pause

:end
echo.
echo Script finished.
endlocal
exit /b
