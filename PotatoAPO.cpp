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

	// Load all effects DLL in the C:\Users\Public\PotatoEffects directory
	ATLTRACE("Attempting to load plugins...\n");

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
	pluginManager.loadPlugin(dynamicProcessDllFullPath);

	// Create a new context for processing pipeline
	context = std::make_unique<ProcessContext>();
	context->numChannels = outFormat.dwSamplesPerFrame; // Number of channels

	return hr;
}

HRESULT PotatoAPO::UnlockForProcess()
{
	// Reset the context and unload all plugins on unlock
	context.reset();
	pluginManager.unloadAllPlugins();
	ATLTRACE("\nUnlockForProcess: PotatoEffect: Unloaded all processing DLLs\n");
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
			context->inputFrames = reinterpret_cast<float*>(ppInputConnections[0]->pBuffer);
			context->outputFrames = reinterpret_cast<float*>(ppOutputConnections[0]->pBuffer);
			context->validFrameCount = ppInputConnections[0]->u32ValidFrameCount;

			if (!pluginManager.getAllPlugins().empty()) {
				executePipeline(*context);
			} else {
				// Copy the input as is to output
				memcpy(context->outputFrames, context->inputFrames, context->validFrameCount);
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

void PotatoAPO::executePipeline(ProcessContext& context) {
	for (IPotatoPlugin* plugin : pluginManager.getAllPlugins()) {
		if (!plugin) continue;

		PluginStatus status = plugin->process(context);
		if (status == PluginStatus::FAILURE) {
			ATLTRACE("Failed processing plugin %s\n", plugin->getName());
			continue; // Processing failed; skip it and continue to next plugin
		}
		// If status is CONTINUE, proceed to the next plugin anyway.
	}
}