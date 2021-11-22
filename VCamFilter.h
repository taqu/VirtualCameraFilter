#pragma once
#ifndef INC_VCAM_FILTER_H_
#    define INC_VCAM_FILTER_H_
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
#    include <Windows.h>
#    include <array>
#    include <cassert>
#    include <cstdint>
#    include <streams.h>

#    define VCAM_ASSERT(exp) assert(exp)

using s32 = int32_t;
using s64 = int64_t;
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f32 = float;

extern const GUID CLSID_VCAM_VirtualCam;

namespace vcam
{
    class VCamPipe;
}

class CVirtualCameraStream;

//--- CVirtualCamera
//------------------------------------------------------
class CVirtualCamera: public CSource
{
public:
    static CUnknown* WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT* phr);

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv);

    inline IFilterGraph* GetGraph()
    {
        return m_pGraph;
    }

    inline FILTER_STATE GetState()
    {
        return m_State;
    }

private:
    CVirtualCamera(LPUNKNOWN unknown, HRESULT* result, const GUID guid);
};

//--- CVirtualCameraStream
//------------------------------------------------------
class CVirtualCameraStream
    : public CSourceStream
    , public IAMStreamConfig
    , public IKsPropertySet
{
public:
    static constexpr u32 MIN_WIDTH = 320;
    static constexpr u32 MIN_HEIGHT = 240;
    static constexpr u32 MAX_WIDTH = 4096;
    static constexpr u32 MAX_HEIGHT = 3072;
    static constexpr u32 MAX_FRAMETIME = 1000000;
    static constexpr u32 MIN_FRAMETIME = 166666;
    static constexpr u32 SLEEP_DURATION = 5;
    static constexpr u32 MAX_FORMATS = 6;

    struct Format
    {
        u32 width_;
        u32 height_;
        s64 timePerFrame_;
    };

    CVirtualCameraStream(HRESULT* result, CVirtualCamera* parent, LPCWSTR pinName);
    ~CVirtualCameraStream();

    // IUnknown interfaces
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv);
    STDMETHODIMP_(ULONG)
    AddRef();
    STDMETHODIMP_(ULONG)
    Release();

    // IQualityControl interfaces
    STDMETHODIMP Notify(IBaseFilter* pSender, Quality quality);

    // IAMStreamConfig interfaces
    HRESULT STDMETHODCALLTYPE SetFormat(AM_MEDIA_TYPE* pmt);
    HRESULT STDMETHODCALLTYPE GetFormat(AM_MEDIA_TYPE** ppmt);
    HRESULT STDMETHODCALLTYPE GetNumberOfCapabilities(int* piCount, int* piSize);
    HRESULT STDMETHODCALLTYPE GetStreamCaps(int iIndex, AM_MEDIA_TYPE** pmt, BYTE* pSCC);

    // IKsPropertySet interfaces
    HRESULT STDMETHODCALLTYPE Set(REFGUID guidPropSet, DWORD dwID, void* pInstanceData, DWORD cbInstanceData, void* pPropData, DWORD cbPropData);
    HRESULT STDMETHODCALLTYPE Get(REFGUID guidPropSet, DWORD dwPropID, void* pInstanceData, DWORD cbInstanceData, void* pPropData, DWORD cbPropData, DWORD* pcbReturned);
    HRESULT STDMETHODCALLTYPE QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD* pTypeSupport);

    // CSourceStream interfaces
    /**
    @brief Retrieve one media sample
    */
    HRESULT FillBuffer(IMediaSample* pms);

    HRESULT DecideBufferSize(IMemAllocator* pIMemAlloc, ALLOCATOR_PROPERTIES* pProperties);
    HRESULT CheckMediaType(const CMediaType* pMediaType);
    HRESULT GetMediaType(int iPosition, CMediaType* pmt);
    HRESULT SetMediaType(const CMediaType* pmt);
    HRESULT OnThreadCreate(void);
    HRESULT OnThreadDestroy(void);

private:
    CVirtualCamera* parent_ = nullptr;

    vcam::VCamPipe* pipe_ = nullptr;
    REFERENCE_TIME lastSyncTime_ = 0;
    REFERENCE_TIME syncTimeout = 0;
    REFERENCE_TIME prevEndTimestamp_ = 0;
    std::array<Format, MAX_FORMATS> formats_;
};
#endif // INC_VCAM_FILTER_H_
