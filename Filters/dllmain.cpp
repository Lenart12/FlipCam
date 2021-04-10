//////////////////////////////////////////////////////////////////////////
//  This file contains routines to register / Unregister the 
//  Directshow filter 'Virtual Cam'
//  We do not use the inbuilt BaseClasses routines as we need to register as
//  a capture source
//////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <streams.h>
#include <initguid.h>
#include <dllsetup.h>
#include <fstream>
#include <cstdio>
#include "filters.h"

#pragma comment(lib, "winmm.lib")

#ifdef DEBUG
    #pragma comment(lib, "strmbasd.lib")
#else
    #pragma comment(lib, "strmbase.lib")
#endif // DEBUG


#define CreateComObject(clsid, iid, var) CoCreateInstance( clsid, NULL, CLSCTX_INPROC_SERVER, iid, (void **)&var);

STDAPI AMovieSetupRegisterServer( CLSID   clsServer, LPCWSTR szDescription, LPCWSTR szFileName, LPCWSTR szThreadingModel = L"Both", LPCWSTR szServerType     = L"InprocServer32" );
STDAPI AMovieSetupUnregisterServer( CLSID clsServer );

// {F89EB97B-2A47-49B5-A1B1-5E530517CDF6}
DEFINE_GUID(CLSID_VirtualCam,
    0xf89eb97b, 0x2a47, 0x49b5, 0xa1, 0xb1, 0x5e, 0x53, 0x5, 0x17, 0xcd, 0xf6);

const AMOVIESETUP_MEDIATYPE AMSMediaTypesVCam = 
{ 
    &MEDIATYPE_Video, 
    &MEDIASUBTYPE_NULL 
};

const AMOVIESETUP_PIN AMSPinVCam=
{
    L"Output",             // Pin string name
    FALSE,                 // Is it rendered
    TRUE,                  // Is it an output
    FALSE,                 // Can we have none
    FALSE,                 // Can we have many
    &CLSID_NULL,           // Connects to filter
    NULL,                  // Connects to pin
    1,                     // Number of types
    &AMSMediaTypesVCam      // Pin Media types
};

const AMOVIESETUP_FILTER AMSFilterVCam =
{
    &CLSID_VirtualCam,  // Filter CLSID
    L"FlipCam",     // String name
    MERIT_DO_NOT_USE,      // Filter merit
    1,                     // Number pins
    &AMSPinVCam             // Pin details
};

CFactoryTemplate g_Templates[] = 
{
    {
        L"FlipCam",
        &CLSID_VirtualCam,
        CVCam::CreateInstance,
        NULL,
        &AMSFilterVCam
    },

};

REGFILTER2 rf2FilterReg = {
    1,                  // Version 1 (no pin mediums or pin category).
    MERIT_DO_NOT_USE,   // Merit.
    1,                  // Number of pins.
    &AMSPinVCam         // Pointer to pin information.
};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

STDAPI RegisterFilters( BOOL bRegister, PCWSTR pszCmdLine = L"")
{
    HRESULT hr = NOERROR;
    WCHAR achFileName[MAX_PATH];
    char achTemp[MAX_PATH];

    if (pszCmdLine != L"") {
        char* appdata = nullptr;
        _dupenv_s(&appdata, nullptr, "APPDATA");
        ASSERT(appdata);
        std::string path = std::string(appdata) + "\\FlipCam";
        std::string mkdir = "mkdir \"" + path + '"';
        system(mkdir.c_str());

        path += "\\flipcam.cfg";
        std::wofstream out_fh(path.c_str(), std::ios_base::out);

        ASSERT(out_fh.is_open());

        out_fh << pszCmdLine;
        out_fh.close();
    }

    ASSERT(g_hInst != 0);

    if( 0 == GetModuleFileNameA(g_hInst, achTemp, sizeof(achTemp))) 
        return AmHresultFromWin32(GetLastError());

    MultiByteToWideChar(CP_ACP, 0L, achTemp, lstrlenA(achTemp) + 1, 
                       achFileName, NUMELMS(achFileName));
  
    hr = CoInitialize(0);
    if(bRegister)
    {
        hr = AMovieSetupRegisterServer(CLSID_VirtualCam, L"FlipCam", achFileName, L"Both", L"InprocServer32");
    }

    if( SUCCEEDED(hr) )
    {
        IFilterMapper2 *fm = 0;
        hr = CreateComObject( CLSID_FilterMapper2, IID_IFilterMapper2, fm );
        if( SUCCEEDED(hr) )
        {
            if(bRegister)
            {
                IMoniker *pMoniker = 0;
                
                hr = fm->RegisterFilter(CLSID_VirtualCam, L"FlipCam", &pMoniker, &CLSID_VideoInputDeviceCategory, NULL, &rf2FilterReg);
            }
            else
            {
                hr = fm->UnregisterFilter(&CLSID_VideoInputDeviceCategory, 0, CLSID_VirtualCam);
            }
        }

      // release interface
      //
      if(fm)
          fm->Release();
    }

    if( SUCCEEDED(hr) && !bRegister )
        hr = AMovieSetupUnregisterServer( CLSID_VirtualCam );

    CoFreeUnusedLibraries();
    CoUninitialize();
    return hr;
}

STDAPI DllRegisterServer()
{
    return RegisterFilters(TRUE);
}

STDAPI DllUnregisterServer()
{
    return RegisterFilters(FALSE);
}

STDAPI DllInstall(BOOL   bInstall, PCWSTR pszCmdLine) {
    return RegisterFilters(bInstall, pszCmdLine);
}

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, DWORD  dwReason, LPVOID lpReserved)
{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}
