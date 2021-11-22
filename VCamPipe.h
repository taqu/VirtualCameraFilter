#pragma once
#ifndef INC_VCAM_PIPE_H_
#    define INC_VCAM_PIPE_H_
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
#    include <cstdint>
namespace vcam
{
using s8 = int8_t;
using s32 = int32_t;
using s64 = int64_t;

using u8 = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;

/**
 * @brief Named pipe implementation by using shared memory
 */
class VCamPipe
{
public:
    VCamPipe();
    ~VCamPipe();

    /**
     * @brief Open as a reader. A reader retrieves frame data in Direct Show filtering process.
     * @param width [in] ... Pixel width
     * @param height [in] ... Pixel height
     * @param bpp [in] ... Bytes per pixel
     * @param maxFrames [in] ... Maximum frames in frame buffer
     * @param sizePerFrame [in] ... Maximum size per frame in bytes
     * @return true if succeeded
     */
    bool openRead(u32 width, u32 height, u32 bpp, u32 maxFrames, u32 sizePerFrame);

    /**
     * @brief Open as a writer. A writer publishes frame data from another process.
     * @return true if succeeded
     */
    bool openWrite();

    /**
     * @return true if connected
     */
    bool connected() const;

    /**
     * @brief Close resources
     */
    void close();

    /**
     * @brief Retrieve current format information
     * @param width [out] ... Pixel width
     * @param height [out] ... Pixel height
     * @param bpp [out] ... Bytes per pixel
     * @return true if succeeded
     */
    bool getFormat(u32& width, u32& height, u32& bpp) const;

    /**
     * @brief Check whether they are the same format
     * @param width [in] ... Pixel width
     * @param height [in] ... Pixel height
     * @param bpp [in] ... Bytes per pixel
     * @return true if the same format
     */
    bool checkFormat(u32 width, u32 height, u32 bpp);

    /**
     * @brief Set current format
     * @param width ... Pixel width
     * @param height ... Pixel height
     * @param bpp ... Bytes per pixel
     */
    void setFormat(u32 width, u32 height, u32 bpp);

    /**
     * @brief Push a frame into ring buffer
     * @param width ... Pixel width
     * @param height ... Pixel height
     * @param bpp ... Bytes per pixel
     * @param data ... frame data
     * @param timeout ... Timeout in milliseconds for locking
     * @return true if succeeded
     */
    bool push(u32 width, u32 height, u32 bpp, u8* data, u32 timeout = 4);

    enum class Status
    {
        Fail,
        Success,
        RepeatLastFrame,
        SyncTimeout,
    };

    /**
     * @brief Pop a frame from ring buffer
     * @param dst [out] ... Frame buffer for next data
     * @param dstSize [in] ... Buffer size of dst
     * @param width [out] ... Pixel width
     * @param height [out] ...  Pixel height
     * @param bpp [out] ... Bytes per pixel
     * @param lastSyncTime ... Last succeeded time of retrieving data
     * @param currentTime ... Current time
     * @param syncTimeout ... Timeout for giving up to retrive data
     * @param timeout ... Timeout in milliseconds for locking
     * @return Result status
     */
    Status pop(u8* dst, u32 dstSize, u32& width, u32& height, u32& bpp, s64 lastSyncTime, s64 currentTime, s64 syncTimeout, u32 timeout = 4);

private:
    VCamPipe(const VCamPipe&) = delete;
    VCamPipe& operator=(const VCamPipe&) = delete;

    /**
     * @brief Automatic realeasing utility of mutex.
     */
    struct Lock
    {
        Lock(HANDLE& handle)
            : handle_(handle)
        {
        }

        ~Lock()
        {
            ReleaseMutex(handle_);
        }

        DWORD Wait(u32 timeout)
        {
            return WaitForSingleObject(handle_, timeout);
        }
        HANDLE& handle_;
    };

    /**
     * @return Page size
     */
    static u32 getPageSize();

    /**
     * @brief Shared video and stream information
     */
    struct Header
    {
        u32 width_;        //!< Pixel width
        u32 height_;       //!< Pixel height
        u32 bpp_;          //!< Bytes per pixel
        u32 maxFrames_;    //!< Maximum frames in ring buffer
        u32 size_;         //!< Ring buffer item count
        u32 head_;         //!< Ring buffer head
        u32 tail_;         //!< Ring buffer tail
        u32 sizePerFrame_; //!< Maximum size of frame in bytes
    };

    /**
     * @brief Entry of ring buffer
     */
    struct Entry
    {
        u32 width_;  //!< Pixel width
        u32 height_; //!< Pixel height
        u32 bpp_;    //!< Bytes per pixel
        u32 padding_;
        u64 offset_; //!< Offet of raw data
    };

    /**
     * @brief Move a cursor in ring buffer
     * @param current [in] ... Current cursor position
     * @return Next cursor position
     */
    u32 next(u32 current) const;

    HANDLE mutex_ = nullptr;
    HANDLE file_ = nullptr;
    u8* mapped_ = nullptr;
    Header* header_ = nullptr;
    Entry* entries_ = nullptr;
    u8* data_ = nullptr;
};
} // namespace vcam
#endif // INC_VCAM_PIPE_H_
