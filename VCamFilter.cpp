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
#include <Wxdebug.h>
#include "VCamPipe.h"

#define DECLARE_PTR(type, ptr, expr) type* ptr = (type*)(expr);

//--- CVirtualCamera
//------------------------------------------------------
CVirtualCamera::CVirtualCamera(LPUNKNOWN unknown, HRESULT* result, const GUID guid)
    : CSource(NAME("VCam Virtual Cam"), unknown, guid)
{
    VCAM_ASSERT(nullptr != result);
    CAutoLock cAutoLock(&m_cStateLock);
    m_paStreams = (CSourceStream**)new CVirtualCameraStream*[1];
    m_paStreams[0] = new CVirtualCameraStream(result, this, L"VCam Virtual Cam");
}

CUnknown* WINAPI CVirtualCamera::CreateInstance(LPUNKNOWN lpunk, HRESULT* phr)
{
    VCAM_ASSERT(nullptr != phr);
	return new CVirtualCamera(lpunk, phr, CLSID_VCAM_VirtualCam);
}

STDMETHODIMP CVirtualCamera::QueryInterface(REFIID riid, void** ppv)
{
    if(riid == _uuidof(IAMStreamConfig) || riid == _uuidof(IKsPropertySet)) {
        return m_paStreams[0]->QueryInterface(riid, ppv);
    } else {
        return CSource::NonDelegatingQueryInterface(riid, ppv);
    }
}

//--- CVirtualCameraStream
//------------------------------------------------------
CVirtualCameraStream::CVirtualCameraStream(HRESULT* result, CVirtualCamera* parent, LPCWSTR pinName)
    : CSourceStream(NAME("VCam Virtual Cam"), result, parent, pinName)
    , parent_(parent)
{
    formats_[0] = {640, 480, 333333};
    formats_[1] = {800, 600, 333333};
    formats_[2] = {1024, 768, 333333};
    formats_[3] = {1280, 720, 333333};
    formats_[4] = {1366, 768, 333333};
    formats_[5] = {1920, 1080, 333333};
    //formats_[6] = {3840, 2160, 333333};
    GetMediaType(0, &m_mt);
    pipe_ = new vcam::VCamPipe;
    const Format& format = formats_[MAX_FORMATS - 1];
    u32 sizePerFrame = format.width_ * format.height_ * 3;
    if(!pipe_->openRead(format.width_, format.height_, 3, 4, sizePerFrame)) {
        delete pipe_;
        pipe_ = nullptr;
    }
}

CVirtualCameraStream::~CVirtualCameraStream()
{
    delete pipe_;
    pipe_ = nullptr;
}

HRESULT CVirtualCameraStream::QueryInterface(REFIID riid, void** ppv)
{
    if(riid == _uuidof(IAMStreamConfig)) {
        *ppv = (IAMStreamConfig*)this;
    } else if(riid == _uuidof(IKsPropertySet)) {
        *ppv = (IKsPropertySet*)this;
    } else {
        return CSourceStream::QueryInterface(riid, ppv);
    }
    AddRef();
    return S_OK;
}

ULONG CVirtualCameraStream::AddRef()
{
    return GetOwner()->AddRef();
}

ULONG CVirtualCameraStream::Release()
{
    return GetOwner()->Release();
}

STDMETHODIMP CVirtualCameraStream::Notify(IBaseFilter* pSender, Quality quality)
{
    return E_NOTIMPL;
}

HRESULT CVirtualCameraStream::FillBuffer(IMediaSample* pms)
{
    using namespace vcam;

    BYTE *pData;
	pms->GetPointer(&pData);
    u32 width = 0;
    u32 height = 0;
    u32 bpp = 0;
    u32 dstSize = pms->GetSize();

    REFERENCE_TIME avgTimePerFrame = ((VIDEOINFOHEADER*)m_mt.pbFormat)->AvgTimePerFrame;
    REFERENCE_TIME currentTime = prevEndTimestamp_;
    prevEndTimestamp_ += avgTimePerFrame;
    if(nullptr != pipe_) {
        VCamPipe::Status status = pipe_->pop(pData, dstSize, width, height, bpp, lastSyncTime_, currentTime, syncTimeout);
        switch(status){
        case VCamPipe::Status::Success:
            lastSyncTime_ = currentTime;
            pms->SetSyncPoint(TRUE);
            break;
        case VCamPipe::Status::RepeatLastFrame:
            pms->SetSyncPoint(FALSE);
            break;
        case VCamPipe::Status::SyncTimeout:
            pms->SetSyncPoint(FALSE);
            break;
        }
        pms->SetTime(&currentTime, &prevEndTimestamp_);
    }
	return NOERROR;
}

HRESULT CVirtualCameraStream::SetMediaType(const CMediaType* pmt)
{
    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->Format());
    HRESULT hr = CSourceStream::SetMediaType(pmt);

    syncTimeout = 10 * ((VIDEOINFOHEADER*)m_mt.pbFormat)->AvgTimePerFrame;

    u32 width = pvi->bmiHeader.biWidth;
    u32 height = pvi->bmiHeader.biHeight;
    u32 bpp = pvi->bmiHeader.biBitCount / 8;
    if(nullptr != pipe_) {
        if(!pipe_->checkFormat(width, height, bpp)) {
            pipe_->setFormat(width, height, bpp);
        }
    } else {
        pipe_ = new vcam::VCamPipe;
        const Format& format = formats_[MAX_FORMATS-1];
        u32 sizePerFrame = format.width_*format.height_*3;
        if (!pipe_->openRead(width, height, bpp, 4, sizePerFrame)) {
            delete pipe_;
            pipe_ = nullptr;
        }
    }
    return hr;
}

HRESULT CVirtualCameraStream::GetMediaType(int iPosition, CMediaType* pmt)
{
    if(iPosition < 0) {
        return E_INVALIDARG;
    }
    if (static_cast<s32>(formats_.size()) <= iPosition) {
        return VFW_S_NO_MORE_ITEMS;
    }

    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER)));
    ZeroMemory(pvi, sizeof(VIDEOINFOHEADER));

    pvi->bmiHeader.biWidth = formats_[iPosition].width_;
    pvi->bmiHeader.biHeight = formats_[iPosition].height_;
    pvi->AvgTimePerFrame = formats_[iPosition].timePerFrame_;
    pvi->bmiHeader.biCompression = BI_RGB;
    pvi->bmiHeader.biBitCount = 24;
    pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biPlanes = 1;
    pvi->bmiHeader.biSizeImage = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;

    SetRectEmpty(&(pvi->rcSource));
    SetRectEmpty(&(pvi->rcTarget));

    const GUID Subtype = GetBitmapSubtype(&pvi->bmiHeader);

    pmt->SetType(&MEDIATYPE_Video);
    pmt->SetFormatType(&FORMAT_VideoInfo);
    pmt->SetTemporalCompression(FALSE);
    pmt->SetSubtype(&Subtype);
    pmt->SetSampleSize(pvi->bmiHeader.biSizeImage);
    return NOERROR;
}

HRESULT CVirtualCameraStream::CheckMediaType(const CMediaType* pMediaType)
{
    if(nullptr == pMediaType) {
        return E_FAIL;
    }
    return *pMediaType != m_mt? E_INVALIDARG : S_OK;
}

HRESULT CVirtualCameraStream::DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pProperties)
{
    CAutoLock cAutoLock(m_pFilter->pStateLock());
    HRESULT hr = NOERROR;

    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)m_mt.Format();
    pProperties->cBuffers = 1;
    pProperties->cbBuffer = pvi->bmiHeader.biSizeImage;

    ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProperties, &Actual);

    if(FAILED(hr)) {
        return hr;
    }
    if(Actual.cbBuffer < pProperties->cbBuffer) {
        return E_FAIL;
    }
    return NOERROR;
}

HRESULT CVirtualCameraStream::OnThreadCreate()
{
    prevEndTimestamp_ = 0;
    return NOERROR;
}

HRESULT CVirtualCameraStream::OnThreadDestroy()
{
    return NOERROR;
}

HRESULT STDMETHODCALLTYPE CVirtualCameraStream::SetFormat(AM_MEDIA_TYPE* pmt)
{
    if (nullptr == pmt) {
        return E_FAIL;
    }
    if (State_Stopped != parent_->GetState()) {
        return E_FAIL;
    }
    if (S_OK != CheckMediaType(static_cast<const CMediaType*>(pmt))) {
        return E_FAIL;
    }

    m_mt = *pmt;
    IPin* pin = nullptr;
    ConnectedTo(&pin);
    if(nullptr != pin) {
        IFilterGraph* pGraph = parent_->GetGraph();
        pGraph->Reconnect(this);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVirtualCameraStream::GetFormat(AM_MEDIA_TYPE** ppmt)
{
    *ppmt = CreateMediaType(&m_mt);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVirtualCameraStream::GetNumberOfCapabilities(int* piCount, int* piSize)
{
    *piCount = static_cast<s32>(formats_.size());
    *piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVirtualCameraStream::GetStreamCaps(int iIndex, AM_MEDIA_TYPE** pmt, BYTE* pSCC)
{
    if (iIndex < 0 || static_cast<s32>(formats_.size()) <= iIndex) {
        return E_INVALIDARG;
    }

    *pmt = CreateMediaType(&m_mt);
    DECLARE_PTR(VIDEOINFOHEADER, pvi, (*pmt)->pbFormat);

    pvi->bmiHeader.biWidth = formats_[iIndex].width_;
    pvi->bmiHeader.biHeight = formats_[iIndex].height_;
    pvi->AvgTimePerFrame = formats_[iIndex].timePerFrame_;
    pvi->bmiHeader.biCompression = BI_RGB;
    pvi->bmiHeader.biBitCount = 24;
    pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biPlanes = 1;
    pvi->bmiHeader.biSizeImage = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;

    SetRectEmpty(&(pvi->rcSource));
    SetRectEmpty(&(pvi->rcTarget));

    (*pmt)->majortype = MEDIATYPE_Video;
    (*pmt)->subtype = MEDIASUBTYPE_RGB24;
    (*pmt)->formattype = FORMAT_VideoInfo;
    (*pmt)->bTemporalCompression = FALSE;
    (*pmt)->bFixedSizeSamples = FALSE;
    (*pmt)->lSampleSize = pvi->bmiHeader.biSizeImage;
    (*pmt)->cbFormat = sizeof(VIDEOINFOHEADER);

    DECLARE_PTR(VIDEO_STREAM_CONFIG_CAPS, pvscc, pSCC);

    pvscc->guid = FORMAT_VideoInfo;
    pvscc->VideoStandard = AnalogVideo_None;
    pvscc->InputSize.cx = pvi->bmiHeader.biWidth;
    pvscc->InputSize.cy = pvi->bmiHeader.biHeight;
    pvscc->MinCroppingSize.cx = 80;
    pvscc->MinCroppingSize.cy = 60;
    pvscc->MaxCroppingSize.cx = pvi->bmiHeader.biWidth;
    pvscc->MaxCroppingSize.cy = pvi->bmiHeader.biHeight;
    pvscc->CropGranularityX = 80;
    pvscc->CropGranularityY = 60;
    pvscc->CropAlignX = 0;
    pvscc->CropAlignY = 0;

    pvscc->MinOutputSize.cx = 80;
    pvscc->MinOutputSize.cy = 60;
    pvscc->MaxOutputSize.cx = pvi->bmiHeader.biWidth;
    pvscc->MaxOutputSize.cy = pvi->bmiHeader.biHeight;
    pvscc->OutputGranularityX = 0;
    pvscc->OutputGranularityY = 0;
    pvscc->StretchTapsX = 0;
    pvscc->StretchTapsY = 0;
    pvscc->ShrinkTapsX = 0;
    pvscc->ShrinkTapsY = 0;
    pvscc->MinFrameInterval = pvi->AvgTimePerFrame;
    pvscc->MaxFrameInterval = pvi->AvgTimePerFrame;
    pvscc->MinBitsPerSecond = static_cast<LONG>(pvi->bmiHeader.biWidth * pvi->bmiHeader.biHeight * 3 * 8 * (10000000 / pvi->AvgTimePerFrame));
    pvscc->MaxBitsPerSecond = static_cast<LONG>(pvi->bmiHeader.biWidth * pvi->bmiHeader.biHeight * 3 * 8 * (10000000 / pvi->AvgTimePerFrame));
    return S_OK;
}

HRESULT CVirtualCameraStream::Set(REFGUID guidPropSet, DWORD dwID, void* pInstanceData, DWORD cbInstanceData, void* pPropData, DWORD cbPropData)
{
    return E_NOTIMPL;
}

HRESULT CVirtualCameraStream::Get(REFGUID guidPropSet, DWORD dwPropID, void* pInstanceData, DWORD cbInstanceData, void* pPropData, DWORD cbPropData, DWORD* pcbReturned)
{
    if(guidPropSet != AMPROPSETID_Pin) {
        return E_PROP_SET_UNSUPPORTED;
    }
    if(dwPropID != AMPROPERTY_PIN_CATEGORY) {
        return E_PROP_ID_UNSUPPORTED;
    }
    if(pPropData == nullptr && pcbReturned == nullptr) {
        return E_POINTER;
    }

    if(nullptr != pcbReturned) {
        *pcbReturned = sizeof(GUID);
    }
    if(pPropData == nullptr) {
        return S_OK;
    }
    if(cbPropData < sizeof(GUID)) {
        return E_UNEXPECTED;
    }

    *(GUID*)pPropData = PIN_CATEGORY_CAPTURE;
    return S_OK;
}

HRESULT CVirtualCameraStream::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD* pTypeSupport)
{
    if(guidPropSet != AMPROPSETID_Pin) {
        return E_PROP_SET_UNSUPPORTED;
    }
    if(dwPropID != AMPROPERTY_PIN_CATEGORY) {
        return E_PROP_ID_UNSUPPORTED;
    }
    if(pTypeSupport) {
        *pTypeSupport = KSPROPERTY_SUPPORT_GET;
    }
    return S_OK;
}

