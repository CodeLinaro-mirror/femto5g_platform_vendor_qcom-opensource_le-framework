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

#ifndef _CODEC2_ALLOCATOR_GBM_H_
#define _CODEC2_ALLOCATOR_GBM_H_

#include <C2Buffer.h>
#include "gbm.h"
#include "gbm_priv.h"

namespace android {

struct C2HandleGBM;

struct C2HandleGBM : public C2Handle {
   uint32_t mWidth;
   uint32_t mHeight;
   uint32_t mFormat;
   uint32_t mStride;
   uint32_t mSliceHeight;
   uint32_t mSize;
   uint64_t mUsage;
   struct gbm_bo *bo;
   int mFd;
   int mMeta_Fd;
   uint32_t mId;
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

    C2AllocatorGBM(id_t id);

    virtual c2_status_t status() const { return mInit; }

    virtual ~C2AllocatorGBM() override;

    static bool isValid(const C2Handle* const o);

    c2_status_t getExtra(const C2Handle *handle, uint32_t *width, uint32_t *height,
            uint32_t *format, uint64_t *flags);

private:
    c2_status_t mInit;
    std::shared_ptr<const Traits> mTraits;
    struct gbm_device *mGBM;
    int mDevice_fd;
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

    uint8_t *gbm_map(int fd, int len);

    c2_status_t gbm_unmap(int fd, void *bufaddr, int len);

    bool Alloc(struct gbm_device *gbm, uint32_t w, uint32_t h, uint32_t format, int flag);

    C2AllocationGBM(struct gbm_device *gbm, uint32_t width, uint32_t height,
            uint32_t format, uint64_t usage, C2Allocator::id_t allocatorId);

    c2_status_t status() const { return C2_OK; };

private:
    int mFd;
    bool mInitialized;
    C2HandleGBM mHandle;
    void *mBase;
    size_t mMapSize;
    C2Allocator::id_t mAllocatorId;
};

} // namespace android

#endif // _CODEC2_ALLOCATOR_GBM_H_
