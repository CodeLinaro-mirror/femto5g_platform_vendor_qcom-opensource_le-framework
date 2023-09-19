/* Copyright (c) 2021 The Linux Foundation. All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are
 met:
    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following
    disclaimer in the documentation and/or other materials provided
    with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 Changes from Qualcomm Innovation Center are provided under the following license:

 Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted (subject to the limitations in the
 disclaimer below) provided that the following conditions are met:

     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.

     * Redistributions in binary form must reproduce the above
       copyright notice, this list of conditions and the following
       disclaimer in the documentation and/or other materials provided
       with the distribution.

     * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
       contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

 NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _CODEC2_ALLOCATOR_GBM_H_
#define _CODEC2_ALLOCATOR_GBM_H_

#include <C2Buffer.h>
#include "gbm.h"
#include "gbm_priv.h"
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <condition_variable>

// it's a workaround as gbm lib doesn't add such usage
#ifndef GBM_BO_USAGE_NV12_512_QTI
#define GBM_BO_USAGE_NV12_512_QTI          0x40000000
#endif

 // Linux platform buffer/memory usage bits.
 // The upper 32 bits is gbm usage. The lower 32 bits is C2 usage.
struct C2MemoryUsageGBM : public C2MemoryUsage {
    C2MemoryUsageGBM(const C2MemoryUsage &usage, uint32_t gbmUsage)
        : C2MemoryUsage(usage) {
        expected = ((uint64_t)gbmUsage << 32) + usage.expected;
    }

    C2MemoryUsageGBM(const C2MemoryUsage &usage)
        : C2MemoryUsage(usage) {
    }

    C2MemoryUsageGBM(uint64_t expected_)
        : C2MemoryUsage(expected_) {
    }

    // Get GBM usage bits from overall usage
    uint32_t gbmUsage() {
        uint32_t gbmUsage = 0;

        if (expected & C2MemoryUsage::READ_PROTECTED) {
            gbmUsage |= GBM_BO_USAGE_PROTECTED_QTI;
        }

        gbmUsage |= (uint32_t)(expected >> 32);

        return gbmUsage;
    }

    // Get C2 usage bits from overall uasge
    uint64_t c2Usage() {
        // the C2 usage is stored in lower 32-bit
        return expected & 0xFFFFFFFF;
    }
};

using LINKGbmCreateDevice = struct gbm_device *(*) (int fd);
using LINKGbmDeviceDestroy = void (*) (struct gbm_device *gbm_dev);
using LINKGbmBoCreate = struct gbm_bo *(*) (struct gbm_device *gbm_dev,
        uint32_t width, uint32_t height, uint32_t format, uint32_t usage);
using LINKGbmPerform = int (*) (int operation, ...);
using LINKGbmBoDestory = void (*) (struct gbm_bo *bo);
using LINKGbmBoImport = struct gbm_bo *(*)(struct gbm_device *gbm_dev,
        uint32_t type, void *buffer, uint32_t usage);
using LINKGbmBoGetFd = int (*) (struct gbm_bo *bo);

#define DEFINE_FUNC_PTR_BY_SYM(sym)          \
        LINK##sym GbmLib::sFunc##sym;        \

#define DEFINE_STATIC_FUNC_PTR_BY_SYM(sym)   \
        static LINK##sym sFunc##sym;         \

namespace android {

class GbmLib {
public:
    ~GbmLib();

    static void loadGbm();
    DEFINE_STATIC_FUNC_PTR_BY_SYM(GbmCreateDevice);
    DEFINE_STATIC_FUNC_PTR_BY_SYM(GbmDeviceDestroy);
    DEFINE_STATIC_FUNC_PTR_BY_SYM(GbmBoCreate);
    DEFINE_STATIC_FUNC_PTR_BY_SYM(GbmPerform);
    DEFINE_STATIC_FUNC_PTR_BY_SYM(GbmBoDestory);
    DEFINE_STATIC_FUNC_PTR_BY_SYM(GbmBoImport);
    DEFINE_STATIC_FUNC_PTR_BY_SYM(GbmBoGetFd);
    static void* sGbmLib;
    static bool sLoaded;
    static std::mutex sLock;   //  mutex for loading operation
};

class C2HandleGBM;
typedef struct GbmBuf {
    int buffer_fd; // shared ion buffer
    int meta_buffer_fd;
    int external_fd; // external buffer fd used to import gbm bo
} GbmBuf;

typedef struct ExtraData {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t usage_lo;
    uint32_t usage_hi;
    uint32_t stride;
    uint32_t slice_height;
    uint32_t size;
    uint32_t magic;
    uint32_t id;
    uint32_t bo_lo;
    uint32_t bo_hi;
} ExtraData;

class C2HandleGBM : public C2Handle {

public:
    static bool isValid(const C2Handle * const o);
    static const C2HandleGBM* Import(const C2Handle *const handle,
            uint32_t *width, uint32_t *height, uint32_t *format,
            uint64_t *usage, uint32_t *stride, uint32_t *size, uint64_t *bo);
    static const ExtraData* getExtraData(const C2Handle *const handle);

    GbmBuf mFds;
    ExtraData mInts;

    enum {
        NUM_FDS = sizeof(mFds) / sizeof(int),
        NUM_INTS = sizeof(mInts) / sizeof(uint32_t),
        VERSION = sizeof(C2Handle)
    };

};

class BufferEntryInfo {
public:
  BufferEntryInfo (bool used, uint64_t res_fmt_id, struct gbm_bo *bo, int32_t bo_fd, int32_t meta_fd, int32_t ext_fd);

  bool used;
  uint64_t res_fmt_id; // id contains resolution and pixel format
  struct gbm_bo *bo;
  int32_t bo_fd;
  int32_t meta_fd;
  int32_t ext_fd;
  bool expired;
};

class BufferPool {
 public:
    BufferPool();
    c2_status_t acquireBuffer(std::shared_ptr<BufferEntryInfo> &entry, uint32_t width, uint32_t height, uint32_t format);
    c2_status_t setMaxBufferCount(uint32_t size);
    c2_status_t releaseBuffer(std::shared_ptr<BufferEntryInfo> entry);

    std::list<std::shared_ptr<BufferEntryInfo> > mBufferList; // may include old and new buffer during port reconfig
private:
    uint32_t getBufferCountOfCurResFmt(); // get buffer count of current resolution and pixel format
    uint64_t mCurResFmtId; // current id contain resolution and pixel format
    uint32_t mMaxBufferCount; // means the max buffer count in pool for current sequence
    uint32_t mExtBufferCount; // extend buffer count for time out
    std::mutex mLock;   //  mutex for the buffer lists
    std::condition_variable mEmptyCondition;
};

class C2AllocatorGBM : public C2Allocator {
public:

    virtual id_t getId() const override;

    virtual C2String getName() const override;

    virtual std::shared_ptr<const Traits> getTraits() const override;

    virtual c2_status_t newGraphicAllocation(uint32_t width, uint32_t height,
            uint32_t format, C2MemoryUsage usage,
            std::shared_ptr<C2GraphicAllocation> *allocation) override;

    virtual c2_status_t priorGraphicAllocation(const C2Handle *handle,
            std::shared_ptr<C2GraphicAllocation> *allocation) override;

    c2_status_t setMaxAllocationCount(uint32_t size);

    C2AllocatorGBM(id_t id);

    virtual c2_status_t status() const { return mInit; }

    virtual ~C2AllocatorGBM() override;

    static bool isValid(const C2Handle* const o);

    bool isUseExternalBuffer();
    c2_status_t setUseExternalBuffer(bool useExternal);
    c2_status_t attachExternalFd(int extFd);
    c2_status_t createC2HandleOfExtBuf(C2Handle *&handle, uint32_t width, uint32_t height,
            uint32_t format, C2MemoryUsage usage);
    using AcquireExtBufFunc = std::function<void(uint32_t, uint32_t)>;
    c2_status_t setAcquireExtBufCb(const AcquireExtBufFunc cb);
    c2_status_t acquireExtBuffer(uint32_t width, uint32_t height);

private:
    c2_status_t mInit;
    std::shared_ptr<const Traits> mTraits;
    struct gbm_device *mGBM;
    std::shared_ptr<BufferPool> mPool;
    int mDevice_fd;
    std::mutex mLock;
    bool mUseExternalBuffer = false;
    std::condition_variable mEmptyCondition;
    std::list<std::shared_ptr<BufferEntryInfo> > mExternalBufferList;
    AcquireExtBufFunc mAcquireExtBufFunc = nullptr;
};

class C2AllocationGBM : public C2GraphicAllocation {
public:
    /* Interface methods */
    virtual c2_status_t map(C2Rect rect, C2MemoryUsage usage, C2Fence *fence,
            C2PlanarLayout *layout /* nonnull */, uint8_t **addr /* nonnull */) override;

    virtual c2_status_t unmap(uint8_t **addr /* nonnull */, C2Rect rect,
            C2Fence *fence /* nullable */) override;

    virtual ~C2AllocationGBM() override;

    virtual const C2Handle *handle() const override;

    virtual id_t getAllocatorId() const override;

    virtual bool equals(const std::shared_ptr<const C2GraphicAllocation> &other) const override;

    C2AllocationGBM(struct gbm_device *gbm, std::shared_ptr<BufferPool> &pool, uint32_t width, uint32_t height,
            uint32_t format, C2MemoryUsage usage, C2Allocator::id_t allocatorId, C2HandleGBM *handle = nullptr);

    c2_status_t status() const { return mRet; };

private:
    c2_status_t Alloc(struct gbm_device *gbm, uint32_t w, uint32_t h, uint32_t format, C2MemoryUsage usage);

    C2HandleGBM *mHandle;
    std::shared_ptr<BufferEntryInfo> mBufEntryInfo;
    void *mBase;
    size_t mMapSize;
    std::shared_ptr<BufferPool> mPool;
    C2Allocator::id_t mAllocatorId;
    c2_status_t mRet;
};

} // namespace android

void _UnwrapNativeCodec2GBMMetadata(
        const C2Handle *const handle, uint32_t *width, uint32_t *height,
        uint32_t *format,uint64_t *usage, uint32_t *stride, uint32_t *size, uint64_t *bo=NULL);

#endif // _CODEC2_ALLOCATOR_GBM_H_
