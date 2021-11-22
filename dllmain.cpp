// clang-format off
/*
# License
This software is distributed under two licenses, choose whichever you like.

## MIT License
Copyright (c) 2021 Takuro Sakai

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Public Domain
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org>
*/
// clang-format on
/**
@author t-sakai
*/
#include "VCamFilter.h"
#include <dshow.h>
#include <initguid.h>
#include <streams.h>

#ifdef VCAM_DLL_EXPORT
#    define EXPORT_API __declspec(dllexport)
#else
#    define EXPORT_API
#endif

//STDAPI AMovieSetupRegisterServer(CLSID clsServer, LPCWSTR szDescription, LPCWSTR szFileName, LPCWSTR szThreadingModel = L"Both", LPCWSTR szServerType = L"InprocServer32");
//STDAPI AMovieSetupUnregisterServer(CLSID clsServer);

// {0C55EFF1-B421-48A8-B537-6194617AC29D}
DEFINE_GUID(CLSID_VCAM_VirtualCam, 0xc55eff1, 0xb421, 0x48a8, 0xb5, 0x37, 0x61, 0x94, 0x61, 0x7a, 0xc2, 0x9d);

const AMOVIESETUP_MEDIATYPE AMSMediaTypesCam = {
    &MEDIATYPE_Video,  // Major
    &MEDIASUBTYPE_RGB24 // Minor
};

const AMOVIESETUP_PIN AMSPinCam = {
    L"Output",
    FALSE,
    TRUE,
    FALSE,
    FALSE,
    &CLSID_NULL,
    nullptr,
    1,
    &AMSMediaTypesCam
};

const AMOVIESETUP_FILTER AMSFilterCam = {
    &CLSID_VCAM_VirtualCam,
    L"VCam Virtual Cam",
    MERIT_DO_NOT_USE,
    1,
    &AMSPinCam
};

REGFILTER2 RegFilter2 =
{
	1,
	MERIT_NORMAL,
	1,
	&AMSPinCam
};

CFactoryTemplate g_Templates[] = {
    {L"VCam Virtual Cam",
     &CLSID_VCAM_VirtualCam,
     CVirtualCamera::CreateInstance,
     nullptr,
     &AMSFilterCam},
};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

STDAPI RegisterFilters(BOOL bRegister)
{
    HRESULT hr = AMovieDllRegisterServer2(bRegister);
    if (FAILED(hr)) {
        return hr;
    }
    IFilterMapper2* filterMapper2 = nullptr;
    hr = CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER, IID_IFilterMapper2, (void **)&filterMapper2);
    if (FAILED(hr)) {
        return hr;
    }
    if (bRegister) {
        hr = filterMapper2->RegisterFilter(
            CLSID_VCAM_VirtualCam,
            NAME("VCam Virtual Cam"),
            NULL,
            &CLSID_VideoInputDeviceCategory,
            NAME("VCam Virtual Cam"),
            &RegFilter2);
    } else {
        hr = filterMapper2->UnregisterFilter(&CLSID_VideoInputDeviceCategory, NAME("VCam Virtual Cam"), CLSID_VCAM_VirtualCam);
    }
    filterMapper2->Release();
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

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
    return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}