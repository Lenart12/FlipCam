#include <streams.h>
#include <stdio.h>
#include <olectl.h>
#include <dvdmedia.h>
#include <fstream>
#include <system_error>
#include "filters.h"
#include "grabber.h"
#include "gfx.h"

#include <synchapi.h>

//////////////////////////////////////////////////////////////////////////
//  CVCam is the source filter which masquerades as a capture device
//////////////////////////////////////////////////////////////////////////
CUnknown* WINAPI CVCam::CreateInstance(LPUNKNOWN lpunk, HRESULT* phr)
{
    ASSERT(phr);
    CUnknown* punk = new CVCam(lpunk, phr);
    return punk;
}

CVCam::CVCam(LPUNKNOWN lpunk, HRESULT* phr) :
    CSource(NAME("Virtual Cam"), lpunk, CLSID_VirtualCam)
{
    ASSERT(phr);
    CAutoLock cAutoLock(&m_cStateLock);

    // Create the one and only output pin
    m_paStreams = (CSourceStream**) new CVCamStream * [1];
    m_paStreams[0] = new CVCamStream(phr, this, L"FlipCam");
}

HRESULT CVCam::QueryInterface(REFIID riid, void** ppv)
{
    //Forward request for IAMStreamConfig & IKsPropertySet to the pin
    if (riid == _uuidof(IAMStreamConfig) || riid == _uuidof(IKsPropertySet))
        return m_paStreams[0]->QueryInterface(riid, ppv);
    else
        return CSource::QueryInterface(riid, ppv);
}

//////////////////////////////////////////////////////////////////////////
// CVCamStream is the one and only output pin of CVCam which handles 
// all the stuff.
//////////////////////////////////////////////////////////////////////////
CVCamStream::CVCamStream(HRESULT* phr, CVCam* pParent, LPCWSTR pPinName) :
    CSourceStream(NAME("Virtual Cam"), phr, pParent, pPinName), m_pParent(pParent)
{
    char* appdata = nullptr;
    _dupenv_s(&appdata, nullptr, "APPDATA");
    ASSERT(appdata);
    std::string path = std::string(appdata) + "\\FlipCam";

    std::string configPath = path + "\\flipcam.cfg";
    std::wifstream conf_fh(configPath, std::ios_base::in);
    if (conf_fh.is_open()) {
        std::wstring cfg;
        getline(conf_fh, cfg);
        fc_config = new FlipCamConfig(cfg);
    }
    else {
        fc_config = new FlipCamConfig();
    }
    grab = NULL;
    GetMediaType(4, &m_mt);
}

CVCamStream::~CVCamStream()
{
    delete fc_config;
}

HRESULT CVCamStream::QueryInterface(REFIID riid, void** ppv)
{
    // Standard OLE stuff
    if (riid == _uuidof(IAMStreamConfig))
        *ppv = (IAMStreamConfig*)this;
    else if (riid == _uuidof(IKsPropertySet))
        *ppv = (IKsPropertySet*)this;
    else
        return CSourceStream::QueryInterface(riid, ppv);

    AddRef();
    return S_OK;
}


//////////////////////////////////////////////////////////////////////////
//  This is the routine where we create the data being output by the Virtual
//  Camera device.
//////////////////////////////////////////////////////////////////////////

HRESULT CVCamStream::FillBuffer(IMediaSample* pms)
{
    SYSTEMTIME time;
    static REFERENCE_TIME start_time = 0;
    static REFERENCE_TIME end_time = 0;

    LONG draw_time = end_time - start_time;

    GetSystemTime(&time);
    start_time = ((REFERENCE_TIME)time.wSecond * 1000) + time.wMilliseconds;

    LONG idle_time = start_time - end_time;


    DECLARE_PTR(VIDEOINFOHEADER, pvi, m_mt.pbFormat);
    ASSERT(pvi);

    REFERENCE_TIME rtNow;
    REFERENCE_TIME avgFrameTime = pvi->AvgTimePerFrame;

    rtNow = m_rtLastTime;
    m_rtLastTime += avgFrameTime;
    pms->SetTime(&rtNow, &m_rtLastTime);
    pms->SetMediaTime(&rtNow, &m_rtLastTime);
    pms->SetSyncPoint(TRUE);
    pms->SetDiscontinuity(rtNow <= 1);

    BYTE* pData;
    pms->GetPointer(&pData);

    int w = pvi->bmiHeader.biWidth;
    int h = pvi->bmiHeader.biHeight;

    Gfx gfx(pData, w, h);

    std::string err = "";

    BYTE* webcam = NULL;
    long webSize = 0;
    VIDEOINFOHEADER* pVih = NULL;

    HRESULT hr = grab->GetSample(webcam, webSize, pVih);
    if (FAILED(hr) || webSize == 0 || pVih == NULL) {
        err = "error:" + std::system_category().message(hr);
        gfx.fillScren(40, 44, 52);
        gfx.putText(w / 2 - 384, h / 2 - 16, "Cannot connect to target webcam", 255, 30, 30, false);

        static int retryGrabCount = 0;
        if (retryGrabCount++ >= 10000000 / fc_config->timePerFrame) {
            delete grab;
            grab = new Grabber(fc_config->webcamSource, hr);
            retryGrabCount = 0;
        }
    }
    else {
        gfx.ingest(webcam,
            pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight,
            fc_config->vFlip, fc_config->hFlip
        );
    }

    CoTaskMemFree(webcam);
    CoTaskMemFree(pVih);

    if (fc_config->dvd) {
#pragma warning(disable: 4244)
        static float dbg_x = 0;
        static float dbg_y = 0;
        static int dbg_r = rand();
        static int dbg_g = rand();
        static int dbg_b = rand();
        static int v_x = 1;
        static int v_y = 1;
        const  int dvd_w = 24 * 5;
        const  int dvd_h = 32;
        static float time_step = fc_config->timePerFrame / 100000.0;

        gfx.putText(dbg_x, dbg_y, " DVD ", dbg_r, dbg_g, dbg_b, true);
        dbg_x += v_x * time_step;
        dbg_y += v_y * time_step;

        if (dbg_x < 0 || dbg_x + dvd_w >= w) {
            dbg_x = (dbg_x <= 0) ? 0 : w - dvd_w - 1;
            v_x *= -1;
            dbg_r = rand();
            dbg_g = rand();
            dbg_b = rand();
        }
        if (dbg_y < 0 || dbg_y + dvd_h >= h) {
            dbg_y = (dbg_y <= 0) ? 0 : h - dvd_h - 1;
            v_y *= -1;
            dbg_r = rand();
            dbg_g = rand();
            dbg_b = rand();
        }
#pragma warning(default:4244)
    }
    if (fc_config->debug || err.length() > 0) {
        
        char debugln[300];
        #ifdef _WIN64
            char platform[] = "x64";
        #else
            char platform[] = "x86";
        #endif // _WIN64
        #ifdef DEBUG
            char configuration[] = "debug";
        #else
            char configuration[] = "release";
        #endif // DEBUG

        int srcW = (pVih != NULL) ? pVih->bmiHeader.biWidth : 0;
        int srcH = (pVih != NULL) ? pVih->bmiHeader.biHeight : 0;


        sprintf_s(debugln, sizeof(debugln)/sizeof(char), "FlipCam v1.3 %s/%s\nres:%dx%d@%lldfps\nsrc:%dx%d\nvflip:%d\nhflip:%d\nrtNow:%lld\ndraw:%ldms\nidle:%ldms\nFPS:%f\n%s",
            platform, configuration,
            w, h, 10000000 / fc_config->timePerFrame,
            srcW, srcH,
            fc_config->vFlip, fc_config->hFlip,
            rtNow,
            draw_time,
            idle_time,
            1000.f / (draw_time + idle_time),
            err.c_str());
        gfx.putText(10, 10, debugln, 215, 211, 203, true);
    }

    GetSystemTime(&time);
    end_time = ((REFERENCE_TIME)time.wSecond * 1000) + time.wMilliseconds;

    LONG sleep_time = fc_config->timePerFrame / 10000 - (2 + end_time - start_time);

    if (sleep_time >= 5 && sleep_time < fc_config->timePerFrame / 10000) {
        SleepEx(sleep_time, false);
    }

    return NOERROR;
} // FillBuffer


//
// Notify
// Ignore quality management messages sent from the downstream filter
STDMETHODIMP CVCamStream::Notify(IBaseFilter * pSender, Quality q)
{
    return E_NOTIMPL;
} // Notify

//////////////////////////////////////////////////////////////////////////
// This is called when the output format has been negotiated
//////////////////////////////////////////////////////////////////////////
HRESULT CVCamStream::SetMediaType(const CMediaType *pmt)
{
    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->Format());
    HRESULT hr = CSourceStream::SetMediaType(pmt);
    return hr;
}

// See Directshow help topic for IAMStreamConfig for details on this method
HRESULT CVCamStream::GetMediaType(int iPosition, CMediaType *pmt)
{
    if(iPosition < 0) return E_INVALIDARG;
    if(iPosition > 8) return VFW_S_NO_MORE_ITEMS;

    if(iPosition == 0) 
    {
        *pmt = m_mt;
        return S_OK;
    }

    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER)));
    ZeroMemory(pvi, sizeof(VIDEOINFOHEADER));

    pvi->bmiHeader.biCompression = BI_RGB;
    pvi->bmiHeader.biBitCount    = 24;
    pvi->bmiHeader.biSize       = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth      = fc_config->width  * iPosition;
    pvi->bmiHeader.biHeight     = fc_config->height * iPosition;
    pvi->bmiHeader.biPlanes     = 1;
    pvi->bmiHeader.biSizeImage  = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;

    pvi->AvgTimePerFrame = fc_config->timePerFrame;

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    pmt->SetType(&MEDIATYPE_Video);
    pmt->SetFormatType(&FORMAT_VideoInfo);
    pmt->SetTemporalCompression(FALSE);

    // Work out the GUID for the subtype from the header info.
    const GUID SubTypeGUID = GetBitmapSubtype(&pvi->bmiHeader);
    pmt->SetSubtype(&SubTypeGUID);
    pmt->SetSampleSize(pvi->bmiHeader.biSizeImage);
    
    return NOERROR;

} // GetMediaType

// This method is called to see if a given output format is supported
HRESULT CVCamStream::CheckMediaType(const CMediaType *pMediaType)
{
    VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *)(pMediaType->Format());
    if(*pMediaType != m_mt) 
        return E_INVALIDARG;
    return S_OK;
} // CheckMediaType

// This method is called after the pins are connected to allocate buffers to stream data
HRESULT CVCamStream::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties)
{
    CAutoLock cAutoLock(m_pFilter->pStateLock());
    HRESULT hr = NOERROR;

    VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *) m_mt.Format();
    pProperties->cBuffers = 1;
    pProperties->cbBuffer = pvi->bmiHeader.biSizeImage;

    ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProperties,&Actual);

    if(FAILED(hr)) return hr;
    if(Actual.cbBuffer < pProperties->cbBuffer) return E_FAIL;

    return NOERROR;
} // DecideBufferSize

// Called when graph is run
HRESULT CVCamStream::OnThreadCreate()
{
    m_rtLastTime = 0;
    char* appdata = nullptr;
    _dupenv_s(&appdata, nullptr, "APPDATA");
    ASSERT(appdata);
    std::string path = std::string(appdata) + "\\FlipCam";

    std::string configPath = path + "\\flipcam.cfg";
    std::wifstream conf_fh(configPath, std::ios_base::in);
    if (conf_fh.is_open()) {
        std::wstring cfg;
        getline(conf_fh, cfg);
        fc_config = new FlipCamConfig(cfg);
    }
    else {
        fc_config = new FlipCamConfig();
    }
    HRESULT hr;
    grab = new Grabber(fc_config->webcamSource, hr);
    return NOERROR;
} // OnThreadCreate

// Called when graph is run
HRESULT CVCamStream::OnThreadDestroy()
{
    delete grab;
    grab = NULL;
    return NOERROR;
} // OnThreadCreate


//////////////////////////////////////////////////////////////////////////
//  IAMStreamConfig
//////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CVCamStream::SetFormat(AM_MEDIA_TYPE *pmt)
{
    DECLARE_PTR(VIDEOINFOHEADER, pvi, m_mt.pbFormat);
    m_mt = *pmt;
    IPin* pin; 
    ConnectedTo(&pin);
    if(pin)
    {
        IFilterGraph *pGraph = m_pParent->GetGraph();
        pGraph->Reconnect(this);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetFormat(AM_MEDIA_TYPE **ppmt)
{
    *ppmt = CreateMediaType(&m_mt);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetNumberOfCapabilities(int *piCount, int *piSize)
{
    *piCount = 8;
    *piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC)
{
    *pmt = CreateMediaType(&m_mt);
    DECLARE_PTR(VIDEOINFOHEADER, pvi, (*pmt)->pbFormat);

    if (iIndex == 0) iIndex = 4;

    pvi->bmiHeader.biCompression = BI_RGB;
    pvi->bmiHeader.biBitCount    = 24;
    pvi->bmiHeader.biSize       = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth      = fc_config->width  * iIndex;
    pvi->bmiHeader.biHeight     = fc_config->height * iIndex;
    pvi->bmiHeader.biPlanes     = 1;
    pvi->bmiHeader.biSizeImage  = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    (*pmt)->majortype = MEDIATYPE_Video;
    (*pmt)->subtype = MEDIASUBTYPE_RGB24;
    (*pmt)->formattype = FORMAT_VideoInfo;
    (*pmt)->bTemporalCompression = FALSE;
    (*pmt)->bFixedSizeSamples= FALSE;
    (*pmt)->lSampleSize = pvi->bmiHeader.biSizeImage;
    (*pmt)->cbFormat = sizeof(VIDEOINFOHEADER);
    

    // DEPRICATED
    DECLARE_PTR(VIDEO_STREAM_CONFIG_CAPS, pvscc, pSCC);
    
    pvscc->guid = FORMAT_VideoInfo;
    pvscc->VideoStandard = AnalogVideo_None;
    pvscc->InputSize.cx = 640;
    pvscc->InputSize.cy = 480;
    pvscc->MinCroppingSize.cx = 80;
    pvscc->MinCroppingSize.cy = 60;
    pvscc->MaxCroppingSize.cx = 640;
    pvscc->MaxCroppingSize.cy = 480;
    pvscc->CropGranularityX = 80;
    pvscc->CropGranularityY = 60;
    pvscc->CropAlignX = 0;
    pvscc->CropAlignY = 0;

    pvscc->MinOutputSize.cx = 80;
    pvscc->MinOutputSize.cy = 60;
    pvscc->MaxOutputSize.cx = 640;
    pvscc->MaxOutputSize.cy = 480;
    pvscc->OutputGranularityX = 0;
    pvscc->OutputGranularityY = 0;
    pvscc->StretchTapsX = 0;
    pvscc->StretchTapsY = 0;
    pvscc->ShrinkTapsX = 0;
    pvscc->ShrinkTapsY = 0;
    pvscc->MinFrameInterval = 200000;   //50 fps
    pvscc->MaxFrameInterval = 50000000; // 0.2 fps
    pvscc->MinBitsPerSecond = (80 * 60 * 3 * 8) / 5;
    pvscc->MaxBitsPerSecond = 640 * 480 * 3 * 8 * 50;
    // DEPRICATED

    return S_OK;
}

//////////////////////////////////////////////////////////////////////////
// IKsPropertySet
//////////////////////////////////////////////////////////////////////////


HRESULT CVCamStream::Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData, 
                        DWORD cbInstanceData, void *pPropData, DWORD cbPropData)
{// Set: Cannot set any properties.
    return E_NOTIMPL;
}

// Get: Return the pin category (our only property). 
HRESULT CVCamStream::Get(
    REFGUID guidPropSet,   // Which property set.
    DWORD dwPropID,        // Which property in that set.
    void *pInstanceData,   // Instance data (ignore).
    DWORD cbInstanceData,  // Size of the instance data (ignore).
    void *pPropData,       // Buffer to receive the property data.
    DWORD cbPropData,      // Size of the buffer.
    DWORD *pcbReturned     // Return the size of the property.
)
{
    if (guidPropSet != AMPROPSETID_Pin)             return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY)        return E_PROP_ID_UNSUPPORTED;
    if (pPropData == NULL && pcbReturned == NULL)   return E_POINTER;
    
    if (pcbReturned) *pcbReturned = sizeof(GUID);
    if (pPropData == NULL)          return S_OK; // Caller just wants to know the size. 
    if (cbPropData < sizeof(GUID))  return E_UNEXPECTED;// The buffer is too small.
        
    *(GUID *)pPropData = PIN_CATEGORY_CAPTURE;
    return S_OK;
}

// QuerySupported: Query whether the pin supports the specified property.
HRESULT CVCamStream::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport)
{
    if (guidPropSet != AMPROPSETID_Pin) return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY) return E_PROP_ID_UNSUPPORTED;
    // We support getting this property, but not setting it.
    if (pTypeSupport) *pTypeSupport = KSPROPERTY_SUPPORT_GET; 
    return S_OK;
}
