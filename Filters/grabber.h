#pragma once

#include <dshow.h>
#include <qedit.h>

class Grabber {
    IGraphBuilder* pGraph = NULL;
    IMediaControl* pControl = NULL;
    IMediaEventEx* pEvent = NULL;
    IBaseFilter* pGrabberF = NULL;
    ISampleGrabber* pGrabber = NULL;
    IBaseFilter* pSourceF = NULL;
    IEnumPins* pEnum = NULL;
    IPin* pPin = NULL;
    IBaseFilter* pNullF = NULL;

    bool canSample = false;

    HRESULT CreateFilterGraphManager();
    HRESULT AddSampleGrabberToFilterGraph();
    HRESULT SetMediaType();
    HRESULT BuildFilterGraph();

public:
    Grabber(IBaseFilter* source, HRESULT &hr);
    ~Grabber();

    HRESULT GetSample(BYTE* &pBuffer, long &pBufferSize, VIDEOINFOHEADER* &pVih);
};