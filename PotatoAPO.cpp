#include "stdafx.h"
#include "PotatoAPO.h"
#include <algorithm>

using namespace std;

long PotatoAPO::instCount = 0;
const CRegAPOProperties<1> PotatoAPO::regProperties(
	__uuidof(PotatoAPO), L"PotatoAPO", L"", 1, 0, __uuidof(IAudioProcessingObject),
	(APO_FLAG)(APO_FLAG_SAMPLESPERFRAME_MUST_MATCH | APO_FLAG_FRAMESPERSECOND_MUST_MATCH | APO_FLAG_BITSPERSAMPLE_MUST_MATCH | APO_FLAG_INPLACE));

PotatoAPO::PotatoAPO(IUnknown* pUnkOuter)
	: CBaseAudioProcessingObject(regProperties)
{
	refCount = 1;
	if (pUnkOuter != NULL)
		this->pUnkOuter = pUnkOuter;
	else
		this->pUnkOuter = reinterpret_cast<IUnknown*>(static_cast<INonDelegatingUnknown*>(this));

	InterlockedIncrement(&instCount);
}

PotatoAPO::~PotatoAPO()
{
	InterlockedDecrement(&instCount);
}

HRESULT PotatoAPO::QueryInterface(const IID& iid, void** ppv)
{
	return pUnkOuter->QueryInterface(iid, ppv);
}

ULONG PotatoAPO::AddRef()
{
	return pUnkOuter->AddRef();
}

ULONG PotatoAPO::Release()
{
	return pUnkOuter->Release();
}

HRESULT PotatoAPO::GetLatency(HNSTIME* pTime)
{
	if (!pTime)
		return E_POINTER;

	if (!m_bIsLocked)
		return APOERR_ALREADY_UNLOCKED;

	*pTime = 0;

	return S_OK;
}

HRESULT PotatoAPO::Initialize(UINT32 cbDataSize, BYTE* pbyData)
{
	if ((NULL == pbyData) && (0 != cbDataSize))
		return E_INVALIDARG;
	if ((NULL != pbyData) && (0 == cbDataSize))
		return E_POINTER;
	if (cbDataSize != sizeof(APOInitSystemEffects))
		return E_INVALIDARG;

	return S_OK;
}

HRESULT PotatoAPO::IsInputFormatSupported(IAudioMediaType* pOutputFormat,
	IAudioMediaType* pRequestedInputFormat, IAudioMediaType** ppSupportedInputFormat)
{
	if (!pRequestedInputFormat)
		return E_POINTER;

	UNCOMPRESSEDAUDIOFORMAT inFormat;
	HRESULT hr = pRequestedInputFormat->GetUncompressedAudioFormat(&inFormat);
	if (FAILED(hr))
	{
		return hr;
	}

	UNCOMPRESSEDAUDIOFORMAT outFormat;
	hr = pOutputFormat->GetUncompressedAudioFormat(&outFormat);
	if (FAILED(hr))
	{
		return hr;
	}

	hr = CBaseAudioProcessingObject::IsInputFormatSupported(pOutputFormat, pRequestedInputFormat, ppSupportedInputFormat);

	return hr;
}

HRESULT PotatoAPO::LockForProcess(UINT32 u32NumInputConnections,
	APO_CONNECTION_DESCRIPTOR** ppInputConnections, UINT32 u32NumOutputConnections,
	APO_CONNECTION_DESCRIPTOR** ppOutputConnections)
{
	HRESULT hr;

	UNCOMPRESSEDAUDIOFORMAT outFormat;
	hr = ppOutputConnections[0]->pFormat->GetUncompressedAudioFormat(&outFormat);
	if (FAILED(hr))
		return hr;

	hr = CBaseAudioProcessingObject::LockForProcess(u32NumInputConnections, ppInputConnections,
			u32NumOutputConnections, ppOutputConnections);
	if (FAILED(hr))
		return hr;

	channelCount = outFormat.dwSamplesPerFrame;

	// Find the effects DLL in the C:\Users\Public\PotatoEffects directory
	HANDLE hFind;
	WIN32_FIND_DATAA data;
	std::string dllFilename = "*.dll";
	std::string dynamicProcessDllFullPath = dynamicProcessDllPath + dllFilename;

	hFind = FindFirstFileA(dynamicProcessDllFullPath.c_str(), &data);
	if (hFind != INVALID_HANDLE_VALUE) {
		dynamicProcessDllFullPath.clear();
		dynamicProcessDllFullPath = dynamicProcessDllPath;
		dynamicProcessDllFullPath += data.cFileName;
		ATLTRACE("\nLockForProcess: PotatoEffect: Found DLL at %s\n", dynamicProcessDllFullPath.c_str());
		FindClose(hFind);
	}

	// Load the effects DLL before locking for processing
	std::wstring effectsDll;
	int wideCharSize = MultiByteToWideChar(CP_UTF8, 0, dynamicProcessDllFullPath.c_str(), -1, NULL, 0);
	wchar_t* wideCharString = (wchar_t*)malloc(wideCharSize * sizeof(wchar_t));
	if (wideCharString) {
		MultiByteToWideChar(CP_UTF8, 0, dynamicProcessDllFullPath.c_str(), -1, wideCharString, wideCharSize);
		effectsDll = wideCharString;
		free(wideCharString);
	}
	hinstLib = LoadLibrary(effectsDll.c_str());
	if (hinstLib != NULL) {
		// Initialize the process DLL
		Init init = (Init)GetProcAddress(hinstLib, "Init"); 
		ATLTRACE("\nLockForProcess: PotatoEffect: Called Init Function\n");
	}
	else {
		ATLTRACE("\nLockForProcess: PotatoEffect: Could not load DLL\n");
	}

	return hr;
}

HRESULT PotatoAPO::UnlockForProcess()
{
	// Unload and free the effects DLL after deinitializing it on unlock
	if (hinstLib != NULL) {
		Deinit deinit = (Deinit)GetProcAddress(hinstLib, "Deinit"); // Deinitialize the process DLL
		ATLTRACE("\nUnlockForProcess: PotatoEffect: Called Deinit\n");

		FreeLibrary(hinstLib);
		ATLTRACE("\nUnlockForProcess: PotatoEffect: Unloaded and freed DLL\n");
	}
	return CBaseAudioProcessingObject::UnlockForProcess();
}

#pragma AVRT_CODE_BEGIN
void PotatoAPO::APOProcess(UINT32 u32NumInputConnections,
	APO_CONNECTION_PROPERTY** ppInputConnections, UINT32 u32NumOutputConnections,
	APO_CONNECTION_PROPERTY** ppOutputConnections)
{
	switch (ppInputConnections[0]->u32BufferFlags)
	{
	case BUFFER_VALID:
		{
			float* inputFrames = reinterpret_cast<float*>(ppInputConnections[0]->pBuffer);
			float* outputFrames = reinterpret_cast<float*>(ppOutputConnections[0]->pBuffer);

			if (hinstLib != NULL) {
				ProcessEffect processFx = (ProcessEffect)GetProcAddress(hinstLib, "ProcessEffect");
				if (processFx != NULL) {
					processFx(inputFrames, outputFrames, channelCount, ppInputConnections[0]->u32ValidFrameCount);
					//ATLTRACE("\nAPOProcess: PotatoEffect: Called ProcessEffect\n");
				}
			}
			else {
				// Copy the input as is to output
				memcpy(outputFrames, inputFrames, ppOutputConnections[0]->u32ValidFrameCount);
			}

			ppOutputConnections[0]->u32ValidFrameCount = ppInputConnections[0]->u32ValidFrameCount;
			ppOutputConnections[0]->u32BufferFlags = ppInputConnections[0]->u32BufferFlags;

			break;
		}
	case BUFFER_SILENT:
		ppOutputConnections[0]->u32ValidFrameCount = ppInputConnections[0]->u32ValidFrameCount;
		ppOutputConnections[0]->u32BufferFlags = ppInputConnections[0]->u32BufferFlags;

		break;
	}
}
#pragma AVRT_CODE_END

HRESULT PotatoAPO::NonDelegatingQueryInterface(const IID& iid, void** ppv)
{
	if (iid == __uuidof(IUnknown))
		*ppv = static_cast<INonDelegatingUnknown*>(this);
	else if (iid == __uuidof(IAudioProcessingObject))
		*ppv = static_cast<IAudioProcessingObject*>(this);
	else if (iid == __uuidof(IAudioProcessingObjectRT))
		*ppv = static_cast<IAudioProcessingObjectRT*>(this);
	else if (iid == __uuidof(IAudioProcessingObjectConfiguration))
		*ppv = static_cast<IAudioProcessingObjectConfiguration*>(this);
	else if (iid == __uuidof(IAudioSystemEffects))
		*ppv = static_cast<IAudioSystemEffects*>(this);
	else
	{
		*ppv = NULL;
		return E_NOINTERFACE;
	}

	reinterpret_cast<IUnknown*>(*ppv)->AddRef();
	return S_OK;
}

ULONG PotatoAPO::NonDelegatingAddRef()
{
	return InterlockedIncrement(&refCount);
}

ULONG PotatoAPO::NonDelegatingRelease()
{
	if (InterlockedDecrement(&refCount) == 0)
	{
		delete this;
		return 0;
	}

	return refCount;
}