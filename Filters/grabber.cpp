#include "grabber.h"
#include <streams.h>


#pragma region HelperFunctions

template <class T> void SafeRelease(T** ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

// Query whether a pin is connected to another pin.
//
// Note: This function does not return a pointer to the connected pin.

HRESULT IsPinConnected(IPin* pPin, BOOL* pResult)
{
    IPin* pTmp = NULL;
    HRESULT hr = pPin->ConnectedTo(&pTmp);
    if (SUCCEEDED(hr))
    {
        *pResult = TRUE;
    }
    else if (hr == VFW_E_NOT_CONNECTED)
    {
        // The pin is not connected. This is not an error for our purposes.
        *pResult = FALSE;
        hr = S_OK;
    }

    SafeRelease(&pTmp);
    return hr;
}

// Query whether a pin has a specified direction (input / output)
HRESULT IsPinDirection(IPin* pPin, PIN_DIRECTION dir, BOOL* pResult)
{
    PIN_DIRECTION pinDir;
    HRESULT hr = pPin->QueryDirection(&pinDir);
    if (SUCCEEDED(hr))
    {
        *pResult = (pinDir == dir);
    }
    return hr;
}

// Match a pin by pin direction and connection state.
HRESULT MatchPin(IPin* pPin, PIN_DIRECTION direction, BOOL bShouldBeConnected, BOOL* pResult)
{
    ASSERT(pResult != NULL);

    BOOL bMatch = FALSE;
    BOOL bIsConnected = FALSE;

    HRESULT hr = IsPinConnected(pPin, &bIsConnected);
    if (SUCCEEDED(hr))
    {
        if (bIsConnected == bShouldBeConnected)
        {
            hr = IsPinDirection(pPin, direction, &bMatch);
        }
    }

    if (SUCCEEDED(hr))
    {
        *pResult = bMatch;
    }
    return hr;
}

// Return the first unconnected input pin or output pin.
HRESULT FindUnconnectedPin(IBaseFilter* pFilter, PIN_DIRECTION PinDir, IPin** ppPin)
{
    IEnumPins* pEnum = NULL;
    IPin* pPin = NULL;
    BOOL bFound = FALSE;

    HRESULT hr = pFilter->EnumPins(&pEnum);
    if (FAILED(hr))
    {
        goto done;
    }

    while (S_OK == pEnum->Next(1, &pPin, NULL))
    {
        hr = MatchPin(pPin, PinDir, FALSE, &bFound);
        if (FAILED(hr))
        {
            goto done;
        }
        if (bFound)
        {
            *ppPin = pPin;
            (*ppPin)->AddRef();
            break;
        }
        SafeRelease(&pPin);
    }

    if (!bFound)
    {
        hr = VFW_E_NOT_FOUND;
    }

done:
    SafeRelease(&pPin);
    SafeRelease(&pEnum);
    return hr;
}

HRESULT ConnectFilters(
    IGraphBuilder* pGraph, // Filter Graph Manager.
    IPin* pOut,            // Output pin on the upstream filter.
    IBaseFilter* pDest)    // Downstream filter.
{
    IPin* pIn = NULL;

    // Find an input pin on the downstream filter.
    HRESULT hr = FindUnconnectedPin(pDest, PINDIR_INPUT, &pIn);
    if (SUCCEEDED(hr))
    {
        // Try to connect them.
        hr = pGraph->Connect(pOut, pIn);
        pIn->Release();
    }
    return hr;
}

#pragma endregion

HRESULT Grabber::CreateFilterGraphManager()
{
    HRESULT hr = CoCreateInstance(CLSID_FilterGraph, NULL,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pGraph));
    if (FAILED(hr)) goto done;

    hr = pGraph->QueryInterface(IID_PPV_ARGS(&pControl));
    if (FAILED(hr)) goto done;

    hr = pGraph->QueryInterface(IID_PPV_ARGS(&pEvent));

done:
    return hr;
}

HRESULT Grabber::AddSampleGrabberToFilterGraph()
{
    HRESULT hr = CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&pGrabberF));
    if (FAILED(hr)) goto done;

    hr = pGraph->AddFilter(pGrabberF, L"Sample Grabber");
    if (FAILED(hr)) goto done;

    hr = pGrabberF->QueryInterface(IID_ISampleGrabber, (void**)(&pGrabber));

    hr = pGrabber->SetOneShot(TRUE);
    if (FAILED(hr)) goto done;

    hr = pGrabber->SetBufferSamples(TRUE);

done:
    return hr;
}

HRESULT Grabber::SetMediaType()
{
    AM_MEDIA_TYPE mt;
    ZeroMemory(&mt, sizeof(mt));
    mt.majortype = MEDIATYPE_Video;
    mt.subtype = MEDIASUBTYPE_RGB24;

    HRESULT hr = pGrabber->SetMediaType(&mt);
    FreeMediaType(mt);
	return hr;
}

HRESULT Grabber::BuildFilterGraph()
{
    HRESULT hr = pGraph->AddFilter(pSourceF, L"Video Source");
    if (FAILED(hr)) goto done;

    hr = pSourceF->EnumPins(&pEnum);
    if (FAILED(hr)) goto done;

    while (S_OK == pEnum->Next(1, &pPin, NULL))
    {
        hr = ConnectFilters(pGraph, pPin, pGrabberF);
        SafeRelease(&pPin);
        if (SUCCEEDED(hr)) break;
    }

    hr = CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&pNullF));
    if (FAILED(hr)) goto done;

    hr = pGraph->AddFilter(pNullF, L"Null Filter");
    if (FAILED(hr)) goto done;

    hr = pGrabberF->EnumPins(&pEnum);
    if (FAILED(hr)) goto done;

    while (S_OK == pEnum->Next(1, &pPin, NULL))
    {
        hr = ConnectFilters(pGraph, pPin, pNullF);
        SafeRelease(&pPin);
        if (SUCCEEDED(hr)) break;
    }

done:
    return hr;
}

Grabber::Grabber(IBaseFilter* source, HRESULT &hr)
    : pSourceF(source) {
    hr = CreateFilterGraphManager();
    if (FAILED(hr)) return;
    hr = AddSampleGrabberToFilterGraph();
    if (FAILED(hr)) return;
    hr = SetMediaType();
    if (FAILED(hr)) return;
    hr = BuildFilterGraph();
    if (FAILED(hr)) return;
    canSample = true;
}

Grabber::~Grabber() {
    SafeRelease(&pPin);
    SafeRelease(&pEnum);
    SafeRelease(&pNullF);
    SafeRelease(&pSourceF);
    SafeRelease(&pGrabber);
    SafeRelease(&pGrabberF);
    SafeRelease(&pControl);
    SafeRelease(&pEvent);
    SafeRelease(&pGraph);
}

HRESULT Grabber::GetSample(BYTE* &pBuffer, long &pBufferSize, VIDEOINFOHEADER* &pVih)
{
    if (!canSample) return E_FAIL;

    HRESULT hr = pControl->Run();
    long evCode;
    hr = pEvent->WaitForCompletion(INFINITE, &evCode);
    if (FAILED(hr)) goto done;

    // Find the required buffer size.
    hr = pGrabber->GetCurrentBuffer(&pBufferSize, NULL);
    if (FAILED(hr)) goto done;

    pBuffer = (BYTE*)CoTaskMemAlloc(pBufferSize);
    if (!pBuffer) {
        hr = E_OUTOFMEMORY;
        goto done;
    }

    hr = pGrabber->GetCurrentBuffer(&pBufferSize, (long*)pBuffer);
    if (FAILED(hr)) goto done;

    AM_MEDIA_TYPE mt;
    hr = pGrabber->GetConnectedMediaType(&mt);
    if (FAILED(hr)) goto done;

    // Examine the format block.
    if ((mt.formattype == FORMAT_VideoInfo) &&
        (mt.cbFormat >= sizeof(VIDEOINFOHEADER)) &&
        (mt.pbFormat != NULL)) {
        pVih = (VIDEOINFOHEADER*)CoTaskMemAlloc(sizeof(VIDEOINFOHEADER));
        if (!pVih) {
            hr = E_OUTOFMEMORY;
        }
        else {
            memcpy_s(pVih, sizeof(VIDEOINFOHEADER), mt.pbFormat, sizeof(VIDEOINFOHEADER));
        }
    }
    else{
        // Invalid format.
        hr = VFW_E_INVALIDMEDIATYPE;
    }

    FreeMediaType(mt);

done:


    return hr;
}
