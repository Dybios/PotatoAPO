#include "stdafx.h"
#include "PotatoVoiceAPO.h"
#include <algorithm>

using namespace std;

long PotatoVoiceAPO::instCount = 0;
const CRegAPOProperties<1> PotatoVoiceAPO::regProperties(
	__uuidof(PotatoVoiceAPO), L"PotatoVoiceAPO", L"", 1, 0, __uuidof(IAudioProcessingObject),
	(APO_FLAG)(APO_FLAG_SAMPLESPERFRAME_MUST_MATCH | APO_FLAG_FRAMESPERSECOND_MUST_MATCH | APO_FLAG_BITSPERSAMPLE_MUST_MATCH | APO_FLAG_INPLACE));

PotatoVoiceAPO::PotatoVoiceAPO(IUnknown* pUnkOuter)
	: CBaseAudioProcessingObject(regProperties)
{
	refCount = 1;
	if (pUnkOuter != NULL)
		this->pUnkOuter = pUnkOuter;
	else
		this->pUnkOuter = reinterpret_cast<IUnknown*>(static_cast<INonDelegatingUnknown*>(this));

	InterlockedIncrement(&instCount);
}

PotatoVoiceAPO::~PotatoVoiceAPO()
{
	InterlockedDecrement(&instCount);
}

HRESULT PotatoVoiceAPO::QueryInterface(const IID& iid, void** ppv)
{
	return pUnkOuter->QueryInterface(iid, ppv);
}

ULONG PotatoVoiceAPO::AddRef()
{
	return pUnkOuter->AddRef();
}

ULONG PotatoVoiceAPO::Release()
{
	return pUnkOuter->Release();
}

HRESULT PotatoVoiceAPO::GetLatency(HNSTIME* pTime)
{
	if (!pTime)
		return E_POINTER;

	if (!m_bIsLocked)
		return APOERR_ALREADY_UNLOCKED;

	*pTime = 0;

	return S_OK;
}

HRESULT PotatoVoiceAPO::Initialize(UINT32 cbDataSize, BYTE* pbyData)
{
	if ((NULL == pbyData) && (0 != cbDataSize))
		return E_INVALIDARG;
	if ((NULL != pbyData) && (0 == cbDataSize))
		return E_POINTER;
	if (cbDataSize != sizeof(APOInitSystemEffects))
		return E_INVALIDARG;

	// Initialize the default noise model
	st = rnnoise_create(NULL);

	return S_OK;
}

HRESULT PotatoVoiceAPO::IsInputFormatSupported(IAudioMediaType* pOutputFormat,
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

HRESULT PotatoVoiceAPO::LockForProcess(UINT32 u32NumInputConnections,
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

	return hr;
}

HRESULT PotatoVoiceAPO::UnlockForProcess()
{
	return CBaseAudioProcessingObject::UnlockForProcess();
}

#pragma AVRT_CODE_BEGIN
void PotatoVoiceAPO::APOProcess(UINT32 u32NumInputConnections,
	APO_CONNECTION_PROPERTY** ppInputConnections, UINT32 u32NumOutputConnections,
	APO_CONNECTION_PROPERTY** ppOutputConnections)
{
	switch (ppInputConnections[0]->u32BufferFlags)
	{
	case BUFFER_VALID:
		{
			float* inputFrames = reinterpret_cast<float*>(ppInputConnections[0]->pBuffer);
			float* outputFrames = reinterpret_cast<float*>(ppOutputConnections[0]->pBuffer);
			float processBuffer[FRAME_SIZE];
			unsigned int bufFrameCount = 0;

			for (int i = 0; i < ppInputConnections[0]->u32ValidFrameCount; i++) {
				processBuffer[bufFrameCount] = inputFrames[i * channelCount];
				bufFrameCount++;

				if (i == FRAME_SIZE) {
					// Process a full frame
					rnnoise_process_frame(st, processBuffer, processBuffer);

					// Copy processed frame back to the output buffer
					outputFrames[i * channelCount] = processBuffer[i];

					bufFrameCount = 0; // reset the buffer index
				}
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

HRESULT PotatoVoiceAPO::NonDelegatingQueryInterface(const IID& iid, void** ppv)
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

ULONG PotatoVoiceAPO::NonDelegatingAddRef()
{
	return InterlockedIncrement(&refCount);
}

ULONG PotatoVoiceAPO::NonDelegatingRelease()
{
	if (InterlockedDecrement(&refCount) == 0)
	{
		delete this;
		return 0;
	}

	return refCount;
}