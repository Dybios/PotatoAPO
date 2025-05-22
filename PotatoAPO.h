#pragma once
#pragma comment(lib, "legacy_stdio_definitions.lib")

#include <Unknwn.h>
#include <audioenginebaseapo.h>
#include <BaseAudioProcessingObject.h>
#include <vector>

#include "PotatoPluginManager.h"
#include "ProcessContext.h"

// Fixed path to always check if any hot-swappable modules exist
static const LPCSTR dllParentPath = "C:\\Users\\Public\\PotatoEffects\\";

class INonDelegatingUnknown
{
	virtual HRESULT __stdcall NonDelegatingQueryInterface(const IID& iid, void** ppv) = 0;
	virtual ULONG __stdcall NonDelegatingAddRef() = 0;
	virtual ULONG __stdcall NonDelegatingRelease() = 0;
};

class __declspec (uuid("46BB25C9-3D22-4ECE-9481-148C12B0B577"))
	PotatoAPO : public CBaseAudioProcessingObject, public IAudioSystemEffects, public INonDelegatingUnknown
{
public:
	PotatoAPO(IUnknown * pUnkOuter);
	virtual ~PotatoAPO();

	// IUnknown
	virtual HRESULT __stdcall QueryInterface(const IID& iid, void** ppv);
	virtual ULONG __stdcall AddRef();
	virtual ULONG __stdcall Release();

	// IAudioProcessingObject
	virtual HRESULT __stdcall GetLatency(HNSTIME* pTime);
	virtual HRESULT __stdcall Initialize(UINT32 cbDataSize, BYTE* pbyData);
	virtual HRESULT __stdcall IsInputFormatSupported(IAudioMediaType* pOutputFormat,
		IAudioMediaType* pRequestedInputFormat, IAudioMediaType** ppSupportedInputFormat);

	// IAudioProcessingObjectConfiguration
	virtual HRESULT __stdcall LockForProcess(UINT32 u32NumInputConnections,
		APO_CONNECTION_DESCRIPTOR** ppInputConnections, UINT32 u32NumOutputConnections,
		APO_CONNECTION_DESCRIPTOR** ppOutputConnections);
	virtual HRESULT __stdcall UnlockForProcess(void);

	// IAudioProcessingObjectRT
	virtual void __stdcall APOProcess(UINT32 u32NumInputConnections,
		APO_CONNECTION_PROPERTY** ppInputConnections, UINT32 u32NumOutputConnections,
		APO_CONNECTION_PROPERTY** ppOutputConnections);

	// INonDelegatingUnknown
	virtual HRESULT __stdcall NonDelegatingQueryInterface(const IID& iid, void** ppv);
	virtual ULONG __stdcall NonDelegatingAddRef();
	virtual ULONG __stdcall NonDelegatingRelease();

	static const CRegAPOProperties<1> regProperties;
	static long instCount;

private:
	long refCount;
	IUnknown* pUnkOuter;

	ProcessContext context;
	PotatoPluginManager pluginManager;
	std::vector<std::string> dllFiles;
	void executePipeline();
};
