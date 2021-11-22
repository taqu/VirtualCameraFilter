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
#include "VCamPipe.h"
#include <utility>
namespace vcam
{
namespace
{
    const char* VCamePipeMutexName = "VCamePipeMutex"; // Mutex name
    const char* VCamePipeMappingName = "VCamePipeMapping"; // Shared memory name
}

VCamPipe::VCamPipe()
{
}

VCamPipe::~VCamPipe()
{
    close();
}

bool VCamPipe::openRead(u32 width, u32 height, u32 bpp, u32 maxFrames, u32 sizePerFrame)
{
    //Calc buffer size
    u32 minimumSize = getPageSize();
    if(minimumSize <= 0) {
        return false;
    }
    u32 desiredSize = maxFrames * sizePerFrame + sizeof(Entry) * maxFrames + sizeof(Header);
    if(desiredSize < minimumSize) {
        desiredSize = minimumSize;
    }
    u32 totalSize = 0;
    while(totalSize < desiredSize) {
        totalSize += minimumSize;
    }

    // Create named mutex
    mutex_ = CreateMutexA(NULL, FALSE, (VCamePipeMutexName));
    if(nullptr == mutex_) {
        return false;
    }

    // Create named mapped file
    file_ = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, totalSize, (VCamePipeMappingName));
    if(nullptr == file_) {
        close();
        return false;
    }
    mapped_ = reinterpret_cast<u8*>(MapViewOfFile(file_, FILE_MAP_ALL_ACCESS, 0, 0, totalSize));
    if(nullptr == mapped_) {
        close();
        return false;
    }

    // Retrieve mapped pointers
    header_ = reinterpret_cast<Header*>(mapped_);
    entries_ = reinterpret_cast<Entry*>(mapped_ + sizeof(Header));
    data_ = mapped_ + sizeof(Header) + sizeof(Entry) * maxFrames;

    // Set up stream information
    memset(header_, 0, sizeof(Header));
    header_->height_ = height;
    header_->width_ = width;
    header_->bpp_ = bpp;
    header_->maxFrames_ = maxFrames;
    header_->sizePerFrame_ = sizePerFrame;
    data_ = mapped_ + sizeof(Header) + sizeof(Entry) * header_->maxFrames_;
    for(size_t i = 0; i < header_->maxFrames_; ++i) {
        entries_[i] = {};
        entries_[i].offset_ = i*header_->sizePerFrame_;
    }
    return true;
}

bool VCamPipe::openWrite()
{
    mutex_ = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, (VCamePipeMutexName));
    if(nullptr == mutex_) {
        return false;
    }
    file_ = OpenFileMappingA(FILE_MAP_WRITE|FILE_MAP_READ, FALSE, (VCamePipeMappingName));
    if(nullptr == file_) {
        close();
        return false;
    }
    mapped_ = reinterpret_cast<u8*>(MapViewOfFile(file_, FILE_MAP_WRITE|FILE_MAP_READ, 0, 0, 0));
    if(nullptr == mapped_) {
        close();
        return false;
    }
    header_ = reinterpret_cast<Header*>(mapped_);
    entries_ = reinterpret_cast<Entry*>(mapped_ + sizeof(Header));
    data_ = mapped_ + sizeof(Header) + sizeof(Entry) * header_->maxFrames_;
    return true;
}

bool VCamPipe::connected() const
{
    return nullptr != mapped_;
}

void VCamPipe::close()
{
    data_ = nullptr;
    entries_ = nullptr;
    header_ = nullptr;

    if(nullptr != mapped_) {
        UnmapViewOfFile(mapped_);
        mapped_ = nullptr;
    }
    if(nullptr != file_) {
        CloseHandle(file_);
        file_ = nullptr;
    }
    if(nullptr != mutex_) {
        CloseHandle(mutex_);
        mutex_ = nullptr;
    }
}

bool VCamPipe::getFormat(u32& width, u32& height, u32& bpp) const
{
    if(nullptr == header_) {
        return false;
    }
    width = header_->width_;
    height = header_->height_;
    bpp = header_->bpp_;
    return true;
}

bool VCamPipe::checkFormat(u32 width, u32 height, u32 bpp)
{
    if(nullptr == header_) {
        return false;
    }
    return header_->width_ == width && header_->height_ == height && header_->bpp_ == bpp;
}

void VCamPipe::setFormat(u32 width, u32 height, u32 bpp)
{
    header_->width_ = width;
    header_->height_ = height;
    header_->bpp_ = bpp;
}

bool VCamPipe::push(u32 width, u32 height, u32 bpp, u8* data, u32 timeout)
{
    if(nullptr == header_) {
        return false;
    }
    if(WAIT_OBJECT_0 != WaitForSingleObject(mutex_, timeout)) {
        return false;
    }
    if(header_->maxFrames_ <= header_->size_) {
        header_->head_ = next(header_->head_);
    } else {
        header_->size_ += 1;
    }
    Entry& entry = entries_[header_->tail_];
    entry.width_ = width;
    entry.height_ = height;
    entry.bpp_ = bpp;
    u32 size = bpp * width * height;
    memcpy(&data_[entry.offset_], data, size);
    header_->tail_ = next(header_->tail_);
    ReleaseMutex(mutex_);
    return true;
}

VCamPipe::Status VCamPipe::pop(u8* dst, u32 dstSize, u32& width, u32& height, u32& bpp, s64 lastSyncTime, s64 currentTime, s64 syncTimeout, u32 timeout)
{
    if(nullptr == header_) {
        return Status::Fail;
    }
    if(WAIT_OBJECT_0 != WaitForSingleObject(mutex_, timeout)) {
        return Status::Fail;
    }
    Lock lock(mutex_);

    Entry& entry = entries_[header_->head_];
    if(header_->size_ <= 0) {
        //Have no last frames
        if(entry.width_==0 && entry.height_==0 && entry.bpp_==0){
            return Status::Fail;
        }
        if(syncTimeout<(lastSyncTime-currentTime)){
            for(size_t i = 0; i < header_->maxFrames_; ++i) {
                entries_[i].width_ = entries_[i].height_ = entries_[i].bpp_ = 0;
            }
            return Status::SyncTimeout;
        }
    }

    width = entry.width_;
    height = entry.height_;
    bpp = entry.bpp_;
    u32 size = (std::min)(bpp * width * height, dstSize);
    memcpy(dst, &data_[entry.offset_], size);

    if(1<=header_->size_){
        header_->head_ = next(header_->head_);
        header_->size_ -= 1;
        return Status::Success;
    }
    return Status::RepeatLastFrame;
}

u32 VCamPipe::getPageSize()
{
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    return systemInfo.dwPageSize;
}

u32 VCamPipe::next(u32 current) const
{
    ++current;
    return header_->maxFrames_ <= current ? 0 : current;
}

} // namespace vcam