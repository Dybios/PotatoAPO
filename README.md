# PotatoAPO
This repo creates a Windows APO DLL which can be registered in SFX chain of the capture (or render) endpoint to process any audio data. The Visual Studio solution is based on the sample example of MinimalAPO provided with EqualizerAPO's application [developer documentation](https://sourceforge.net/p/equalizerapo/wiki/Developer%20documentation/). 

>**IMPORTANT**
>
>This DLL does not apply any effects and acts only as an interface. You will need to build and install separate processing plugin DLLs to appropriate paths to hear the audible effects. For a simple example of silencing an audio output, refer to the [silence-example](https://github.com/Dybios/PotatoPlugins/tree/silence-example)
>demo provided in the PotatoPlugins repository. Follow the steps provided there to build and deploy the DLLs.

Following steps detail the way the PotatoAPO DLL can be registered and unregistered. 

NOTE: Please make sure to backup the registry key and its values before making any changes.

**Preparation steps:**
1. Build the project for your OS architecture (Win32/x64).
2. Register the APO by opening a command shell with Administrator privileges
   in the corresponding build directory (e.g. x64\Debug) and typing `regsvr32 PotatoVoiceAPO.dll`
3. Open regedit, go to `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Audio`
   and create a DWORD value named `DisableProtectedAudioDG` with value 1 to allow unsigned APOs to run.
4. Go to `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Capture` and
   find your default audio device by looking at the Properties subkeys. In the FxProperties subkey of the device
   create or change the value for `{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},5` (SFX endpoint) to `{46BB25C9-3D22-4ECE-9481-148C12B0B577}`.
5. Go to the Sound control panel and make sure that audio enhancements are enabled for your device.
6. Open a command shell with Administrator privileges, type `sc config audiosrv type= own`
   and reboot to make the system start the audio service in a separate process. Otherwise the APO DLL will likely be
   permanently locked until reboot even when the audio service has been stopped.

**Development steps:**
1. Launch an audio application and verify that the APO is effective. Without changes to the processing method,
   you should hear some noticeable distortion.
2. Change the APO code. You might want to start with the method PotatoVoiceAPO::APOProcess, where the actual audio
   processing happens. You can debug by attaching to audiodg.exe.
3. Stop the Windows Audio service (AudioSrv) to unlock the PotatoVoiceAPO.dll.
4. Build the Visual Studio project to overwrite it with the new DLL.
5. Start the Windows Audio service again and continue with step 1.

**Necessary steps before release (just some hints):**
1. Change the class name PotatoVoiceAPO.
2. Change the registration properties at the top of MinimalAPO.cpp (change name and insert your copyright statement).
3. Use the Visual Studio GUID generator to generate your own GUID and place it into the `__declspec (uuid(...))` declaration in PotatoVoiceAPO.h.

**Removal steps:**
1. Revert the changes you made to the FxProperties subkey of the default audio device (see preparation step 4).
   If you can't, you can also just reinstall the audio device driver.
2. Open regedit, go to `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Audio`
   and remove the value named DisableProtectedAudioDG to reenable APO signature checking.
3. Unregister the APO by opening a command shell with Administrator privileges
   in the build directory (e.g. x64\Debug) and typing `regsvr32 /u PotatoVoiceAPO.dll`
