/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Changes from Qualcomm Innovation Center are provided under the following license:
// Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Clear

//#define LOG_NDEBUG 0
#define LOG_TAG "C2DmaLinearAllocator"
#include <C2DmaLinearAllocator.h>

#include <sys/mman.h>
#include <unistd.h>

#include <memory>

#include <utils/Log.h>
#include <C2Buffer.h>

namespace android {
/* ========================================= DMA HANDLE ======================================== */
struct C2DmaHandle : public C2Handle {
    C2DmaHandle(int bufferFd, size_t size)
        : C2Handle(cHeader),
          mFds{ bufferFd },
          mInts{ int(size & 0xFFFFFFFF), int((uint64_t(size) >> 32) & 0xFFFFFFFF), kMagic } { }

    static bool isValid(const C2Handle * const o);

    int bufferFd() const { return mFds.mBuffer; }
    size_t size() const {
        return size_t(unsigned(mInts.mSizeLo))
                | size_t(uint64_t(unsigned(mInts.mSizeHi)) << 32);
    }

protected:
    struct {
        int mBuffer; // shared buffer
    } mFds;
    struct {
        int mSizeLo; // low 32-bits of size
        int mSizeHi; // high 32-bits of size
        int mMagic;
    } mInts;

private:
    enum {
        kMagic = '\xc2io\x00',
        numFds = sizeof(mFds) / sizeof(int),
        numInts = sizeof(mInts) / sizeof(int),
        version = sizeof(C2Handle)
    };

    const static C2Handle cHeader;
};

const C2Handle C2DmaHandle::cHeader = {
    C2DmaHandle::version,
    C2DmaHandle::numFds,
    C2DmaHandle::numInts,
    {}
};

/* ======================================= DMA ALLOCATION ====================================== */
class C2DmaLinearAllocation : public C2LinearAllocation {
public:
    /* Interface methods */
    virtual c2_status_t map(
        size_t offset, size_t size, C2MemoryUsage usage, C2Fence *fence,
        void **addr /* nonnull */) override;
    virtual c2_status_t unmap(void *addr, size_t size, C2Fence *fenceFd) override;
    virtual ~C2DmaLinearAllocation() override;
    virtual const C2Handle *handle() const override;
    virtual id_t getAllocatorId() const override;
    virtual bool equals(const std::shared_ptr<C2LinearAllocation> &other) const override;

    C2DmaLinearAllocation(BufferAllocator& alloc, size_t size, C2MemoryUsage usage, C2Allocator::id_t id);

    c2_status_t status() const { return C2_OK; };

private:
    int mFd;
    C2Allocator::id_t mId;
    std::shared_ptr<C2DmaHandle> mHandle;
    void *mBase;
    size_t mMapSize;
};

C2DmaLinearAllocation::C2DmaLinearAllocation(
    BufferAllocator& alloc, size_t size, C2MemoryUsage usage, C2Allocator::id_t id)
    : C2LinearAllocation(size), mFd(-1), mBase(nullptr), mMapSize(0)
{
    const char* buf_type = nullptr;

    if (usage.expected & C2MemoryUsage::READ_PROTECTED) {
        mFd = alloc.Alloc("system-secure", size, 0);
        buf_type = "secure";
    } else {
        mFd = alloc.Alloc("qcom,system-uncached", size, 0);
        buf_type = "non-secure";
    }

    if (mFd < 0) {
        ALOGE("%s failed to allocate %s buf", LOG_TAG, buf_type);
    } else {
        ALOGD("%s allocated %s fd:%d", LOG_TAG, buf_type, mFd);
        mHandle = std::make_shared<C2DmaHandle>(mFd, size);
        mId = id;
    }
}

C2DmaLinearAllocation::~C2DmaLinearAllocation()
{
    if (mFd > 0) {
        ALOGD("%s close fd:%d", LOG_TAG, mFd);
        close(mFd);
        mFd = -1;
    }
}

c2_status_t C2DmaLinearAllocation::map(
    size_t offset, size_t size, C2MemoryUsage usage, C2Fence *fence,
    void **addr /* nonnull */)
{
    c2_status_t ret = C2_OK;
    int prot = PROT_NONE;

    if (!addr) {
        ALOGE("invalid addr");
        ret = C2_BAD_VALUE;
    } else {
        if (usage.expected & C2MemoryUsage::CPU_READ) {
            prot |= PROT_READ;
        }
        if (usage.expected & C2MemoryUsage::CPU_WRITE) {
            prot |= PROT_WRITE;
        }

        if (prot == PROT_NONE) {
            mBase = *addr = nullptr;
            ALOGE("refused to map for secure buffer");
            ret = C2_REFUSED;
        } else {
            mBase = mmap(0, size, prot, MAP_SHARED, mFd, 0);
            if (mBase == MAP_FAILED) {
                mBase = *addr = nullptr;
                ret = C2_BAD_VALUE;
            } else {
                *addr = mBase;
                mMapSize = size;
            }
        }
    }

    return ret;
}

c2_status_t C2DmaLinearAllocation::unmap(void *addr, size_t size, C2Fence *fenceFd)
{
    c2_status_t ret = C2_OK;

    if (!mMapSize || !mBase) {
        ALOGE("invalid mMapSize:%zu mBase:%p", mMapSize, mBase);
        ret = C2_REFUSED;
    } else {
        int ret = munmap(mBase, mMapSize);
        if (ret) {
            ALOGE("%s failed to ummap dma mMapSize %zu", LOG_TAG, mMapSize);
            ret = C2_BAD_VALUE;
        }
    }

    return ret;
}

const C2Handle *C2DmaLinearAllocation::handle() const
{
    return mHandle.get();
}

id_t C2DmaLinearAllocation::getAllocatorId() const
{
    return mId;
}

bool C2DmaLinearAllocation::equals(const std::shared_ptr<C2LinearAllocation> &other) const
{
    return true;
}

/* ======================================= DMA ALLOCATOR ====================================== */
C2DmaLinearAllocator::C2DmaLinearAllocator(id_t id)
    : mInit(C2_OK)
{
    C2MemoryUsage minUsage = { 0, 0 };
    C2MemoryUsage maxUsage = { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE };
    Traits traits = { "linux.allocator.dma", id, LINEAR, minUsage, maxUsage };
    mTraits = std::make_shared<Traits>(traits);
}

C2DmaLinearAllocator::~C2DmaLinearAllocator() {
}

C2Allocator::id_t C2DmaLinearAllocator::getId() const {
    return mTraits->id;
}

C2String C2DmaLinearAllocator::getName() const {
    return mTraits->name;
}

std::shared_ptr<const C2Allocator::Traits> C2DmaLinearAllocator::getTraits() const {
    return mTraits;
}

c2_status_t C2DmaLinearAllocator::newLinearAllocation(
        uint32_t capacity, C2MemoryUsage usage, std::shared_ptr<C2LinearAllocation> *allocation) {
    c2_status_t ret = C2_OK;
    if (allocation == nullptr) {
        return C2_BAD_VALUE;
    }

    std::shared_ptr<C2DmaLinearAllocation> alloc
        = std::make_shared<C2DmaLinearAllocation>(mBufferAllocator, capacity, usage, getId());
    ret = alloc->status();
    if (ret == C2_OK) {
        *allocation = alloc;
    }
    return ret;
}

c2_status_t C2DmaLinearAllocator::priorLinearAllocation(
        const C2Handle *handle, std::shared_ptr<C2LinearAllocation> *allocation) {
    c2_status_t ret = C2_OK;
    *allocation = nullptr;
    return ret;
}

bool C2DmaLinearAllocator::isValid(const C2Handle* const o) {
    return true;
}

} // namespace android
