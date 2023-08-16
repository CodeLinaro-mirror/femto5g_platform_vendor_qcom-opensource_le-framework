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
#include <map>
#include <list>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <cinttypes>

#include <utils/Log.h>
#include <C2Buffer.h>

using namespace std::chrono_literals;

namespace android {

const C2Handle C2DmaHandle::cHeader = {
    C2DmaHandle::version,
    C2DmaHandle::numFds,
    C2DmaHandle::numInts,
    {}
};

/* ======================================= DMA Buffer Pool ====================================== */
class DmaBufferPool {
public:
    explicit DmaBufferPool(BufferAllocator *allocator)
        : mSweepedBufferCount(0), mMaxBufferSize(0),
        mTotalBufferSize(0), mBufferAllocator(allocator) {
        ALOGD("%s %s max pool buffer count %zu", LOG_TAG, __func__, kMaxPoolBufferCount);
    }
    ~DmaBufferPool();

    c2_status_t acquireBuffer(std::shared_ptr<C2DmaHandle> &handle,
            size_t size, C2MemoryUsage usage);
    c2_status_t releaseBuffer(std::shared_ptr<C2DmaHandle> &handle);

private:
    bool allocateBuffer(std::shared_ptr<C2DmaHandle> &handle, size_t size, C2MemoryUsage usage);

    bool findFreeBuffer(size_t size, std::shared_ptr<C2DmaHandle> &handle) {
        bool ret = false;
        int index = 0;
        /* try to find a min capacity buffer that fit in requested buffer size */
        for (auto it = mFreeBuffers.begin(); it != mFreeBuffers.end(); ++it) {
            if (size <= it->first) {
                ALOGV("capacity:fd:index %zu:%d:%d", it->first, it->second->bufferFd(), index);
                handle = it->second;
                mFreeBuffers.erase(it);
                mUsedBuffers.push_back(handle);
                ret = true;
                break;
            }
            ++index;
        }
        ALOGD("%s size:ret:index:free %zu:%u:%d:%zu", LOG_TAG,
            size, ret, index, mFreeBuffers.size());
        return ret;
    }

    bool sweepFreeBuffer(void);

    bool isOverAllocated(size_t total, size_t totalSize) {
        return (total > kMaxPoolBufferCount) ||
                ((total > kMinTotalBufferCountOverAllocated) &&
                (totalSize > kTotalBufferSizeNeedSweep));
    }

private:
    size_t mSweepedBufferCount;
    size_t mMaxBufferSize;
    size_t mTotalBufferSize;
    std::mutex mLock;
    std::condition_variable mBufferAvailable;
    /* free buffers are in ascending order of buffer size as key. */
    std::multimap<size_t, std::shared_ptr<C2DmaHandle>> mFreeBuffers;
    std::list<std::shared_ptr<C2DmaHandle>> mUsedBuffers; /* for debugging */
    BufferAllocator* mBufferAllocator;

    /* interlaced video output meta data buffer requirement is 37. */
    static constexpr size_t kMaxPoolBufferCount = 48; /* soft limit need to sweep buffer */
    /* to run smoothly, at least decoder needs 8 buffers and encoder needs 4 buffers. */
    static constexpr size_t kMinTotalBufferCountOverAllocated = 8;
    static constexpr size_t kFreeBufferCountAllowSweep = 8;
    static constexpr size_t kTotalBufferSizeNeedSweep = 32 * 1024 * 1024; /* 32MB */
    /* in case of encoding 8192*4320, an output buffer is 50.62MB,
     * need at least 4 buffers to run smoothly for only one encoding instance. */
    static constexpr size_t kMaxTotalBufferSize = 256 * 1024 * 1024; /* 256MB hard limit */
    static constexpr int kWaitCountToAllocateBuffer = 3;
};

DmaBufferPool::~DmaBufferPool()
{
    ALOGD("%s %s free:used:sweeped %zu:%zu:%zu, pool max:sweep %zu:%zu, "
        "total size:sweep:max %zu:%zu:%zu", LOG_TAG, __func__,
        mFreeBuffers.size(), mUsedBuffers.size(), mSweepedBufferCount,
        kMaxPoolBufferCount, kFreeBufferCountAllowSweep,
        mTotalBufferSize, kTotalBufferSizeNeedSweep, kMaxTotalBufferSize);

    auto format = [] (const std::list<std::shared_ptr<C2DmaHandle>> &list) {
        std::ostringstream oss;
        int count = 0;
        oss << "sorted buffers size:fd in 8 per line:";
        for (const auto &h : list) {
            if ((count++ % 8) == 0)
                oss << std::endl;
            oss << h->size() << ":" << h->bufferFd() << " ";
        }
        oss << std::endl;
        return oss.str();
    };

    if (mUsedBuffers.size() > 0) {
        ALOGE("%s error: still have used buffer", LOG_TAG);
        ALOGE("%s used %s", __func__, format(mUsedBuffers).c_str());
    }

    std::list<std::shared_ptr<C2DmaHandle>> freeBuffers;
    for (const auto &p : mFreeBuffers) {
        freeBuffers.push_back(p.second);
    }
    ALOGD("%s free %s", __func__, format(freeBuffers).c_str());
    freeBuffers.clear();

    /* buffer handle ref count gets to zero and buffers in pool are freed. */
    mFreeBuffers.clear();
}

c2_status_t DmaBufferPool::acquireBuffer(std::shared_ptr<C2DmaHandle> &handle,
        size_t size, C2MemoryUsage usage)
{
    c2_status_t ret = C2_OK;
    int waitCount = 0;
    std::unique_lock<std::mutex> lock(mLock);
    size_t total = mFreeBuffers.size() + mUsedBuffers.size();
    size_t totalSize = size + mTotalBufferSize;
    bool overAllocated = isOverAllocated(total + 1, totalSize);

    ALOGD("%s total count:size %zu:%zu, pool max:%zu", LOG_TAG,
        total, mTotalBufferSize, kMaxPoolBufferCount);

    if (size > mMaxBufferSize && !overAllocated) {
        ALOGD("%s size:max size %zu:%zu", LOG_TAG, size, mMaxBufferSize);
        if (!allocateBuffer(handle, size, usage)) {
            ret = C2_NO_MEMORY;
        }
        goto out;
    }

    while (!findFreeBuffer(size, handle)) {
        ALOGD("%s overAllocated:%u, wait count:%d", LOG_TAG, overAllocated, waitCount);

        if (!overAllocated || waitCount == kWaitCountToAllocateBuffer) {
            if (!allocateBuffer(handle, size, usage)) {
                ret = C2_NO_MEMORY;
            }
            break;
        } else {
            auto status = mBufferAvailable.wait_for(lock, 10ms);
            if (std::cv_status::timeout == status) {
                ALOGD("%s timeout awaiting free buffer", LOG_TAG);
            } else {
                ALOGD("%s awaited a free buffer back", LOG_TAG);
            }
            ++waitCount;
        }
    }

out:
    if (C2_OK == ret) {
        ALOGD("%s acquired buffer fd:%d size:%zu", LOG_TAG, handle->bufferFd(), handle->size());
    } else {
        ALOGE("%s %s failed ret:%d requested size:%zu", LOG_TAG, __func__, ret, size);
    }

    return ret;
}

c2_status_t DmaBufferPool::releaseBuffer(std::shared_ptr<C2DmaHandle> &handle)
{
    c2_status_t ret = C2_OK;

    if (handle) {
        std::lock_guard<std::mutex> lock(mLock);
        mFreeBuffers.insert({handle->size(), handle});
        mUsedBuffers.remove(handle);
        mBufferAvailable.notify_one();
        ALOGD("%s released buffer fd:%d size:%zu", LOG_TAG, handle->bufferFd(), handle->size());

        (void)sweepFreeBuffer();
    } else {
        ALOGE("%s %s null handle", LOG_TAG, __func__);
        ret = C2_BAD_VALUE;
    }

    return ret;
}

/* buffer fd shall be closed by ~C2DmaHandle() while destructing DmaBufferPool. */
bool DmaBufferPool::allocateBuffer(std::shared_ptr<C2DmaHandle> &handle,
        size_t size, C2MemoryUsage usage)
{
    const char* buf_type = nullptr;
    bool ret = true;
    int fd = -1;
    size_t totalSize = mTotalBufferSize + size;

    if (totalSize > kMaxTotalBufferSize) {
        ALOGE("%s %s total size:%zu exceeds memory limit!", LOG_TAG, __func__, totalSize);
        ret = false;
        goto out;
    }

    if (usage.expected & C2MemoryUsage::READ_PROTECTED) {
        fd = mBufferAllocator->Alloc("system-secure", size, 0);
        buf_type = "secure";
    } else {
        fd = mBufferAllocator->Alloc("qcom,system-uncached", size, 0);
        buf_type = "non-secure";
    }

    if (fd < 0) {
        ALOGE("%s failed to allocate %s buf size:%zu", LOG_TAG, buf_type, size);
        ret = false;
    } else {
        ALOGD("%s allocated %s fd:%d size:%zu", LOG_TAG, buf_type, fd, size);
        handle = std::make_shared<C2DmaHandle>(fd, size);
        if (handle) {
            mUsedBuffers.push_back(handle);
            mTotalBufferSize = totalSize;
            if (size > mMaxBufferSize)
                mMaxBufferSize = size;
        } else {
            ALOGE("%s %s null handle, fd:%d", LOG_TAG, __func__, fd);
            close (fd);
            ret = false;
        }
    }

out:
    return ret;
}

/* sweep out i.e. free an over-allocated buffer a time. */
bool DmaBufferPool::sweepFreeBuffer(void)
{
    size_t free = mFreeBuffers.size();
    size_t used = mUsedBuffers.size();
    size_t total = free + used;
    size_t sweep = kFreeBufferCountAllowSweep;
    bool allowSweep = (free >= sweep) && isOverAllocated(total, mTotalBufferSize);

    if (allowSweep) {
        ALOGD("%s total:free %zu:%zu, total size %zu, pool max:sweep %zu:%zu", __func__,
            total, free, mTotalBufferSize, kMaxPoolBufferCount, sweep);

        /* remove and free the min size buffer */
        auto it = mFreeBuffers.begin();
        size_t size = it->second->size();
        int fd = it->second->bufferFd();
        mTotalBufferSize -= size;

        mFreeBuffers.erase(it);
        mSweepedBufferCount += 1;

        free = mFreeBuffers.size();
        total = free + used;
        ALOGD("%s total:free %zu:%zu, total size %zu, sweeped %zu:%d:%zu", __func__,
            total, free, mTotalBufferSize, size, fd, mSweepedBufferCount);
    }

    return allowSweep;
}

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

    C2DmaLinearAllocation(std::shared_ptr<DmaBufferPool> &pool, size_t size, C2MemoryUsage usage, C2Allocator::id_t id);
    C2DmaLinearAllocation(int bufferFd, size_t capacity, C2Allocator::id_t id);

    c2_status_t status() const { return mRet; };

private:
    C2Allocator::id_t mId;
    std::shared_ptr<C2DmaHandle> mHandle;
    void *mBase;
    size_t mMapSize;
    std::shared_ptr<DmaBufferPool> mPool;
    c2_status_t mRet;
    bool mIsExternalBuffer;
};

C2DmaLinearAllocation::C2DmaLinearAllocation(
    std::shared_ptr<DmaBufferPool> &pool, size_t size, C2MemoryUsage usage, C2Allocator::id_t id)
    : C2LinearAllocation(size), mId(C2Allocator::BAD_ID), mBase(nullptr),
    mMapSize(0), mPool(pool), mRet(C2_OK), mIsExternalBuffer(false)
{
    mRet = mPool->acquireBuffer(mHandle, size, usage);
    if (C2_OK != mRet) {
        ALOGE("%s failed to acquire buf ret:%d", LOG_TAG, mRet);
    }
}

C2DmaLinearAllocation::C2DmaLinearAllocation(
    int bufferFd, size_t capacity, C2Allocator::id_t id)
    : C2LinearAllocation(capacity), mId(id), mBase(nullptr),
    mMapSize(0), mRet(C2_OK), mIsExternalBuffer(true)
{
    if (bufferFd < 0) {
        ALOGE("%s failed to import buf since invalid fd:%d", LOG_TAG, bufferFd);
        mRet = C2_BAD_VALUE;
    } else {
        mHandle = std::make_shared<C2DmaHandle>(bufferFd, capacity);
        ALOGV("%s import buf fd:%d size:%zu", LOG_TAG, bufferFd, capacity);
    }
}

C2DmaLinearAllocation::~C2DmaLinearAllocation()
{
    if (!mHandle) {
        ALOGE("%s %s null handle", LOG_TAG, __func__);
        return;
    }

    if (mIsExternalBuffer) {
        // closed by ~C2DmaHandle()
        ALOGD("%s close fd:%d", LOG_TAG, mHandle->bufferFd());
    } else {
        //ALOGD("%s release buffer fd:%d", LOG_TAG, mHandle->bufferFd());
        if (mPool) {
            mPool->releaseBuffer(mHandle);
        } else {
            ALOGE("%s null pool", LOG_TAG);
        }
    }
}

c2_status_t C2DmaLinearAllocation::map(
    size_t offset, size_t size, C2MemoryUsage usage, C2Fence *fence,
    void **addr /* nonnull */)
{
    c2_status_t ret = C2_BAD_VALUE;
    int prot = PROT_NONE;

    if (!mHandle) {
        ALOGE("%s %s null handle", LOG_TAG, __func__);
    } else if (!addr) {
        ALOGE("invalid addr");
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
            int fd = mHandle->bufferFd();
            mBase = mmap(0, size, prot, MAP_SHARED, fd, 0);
            if (mBase == MAP_FAILED) {
                mBase = *addr = nullptr;
                ret = C2_CANNOT_DO;
                ALOGE("mmap error fd:%d size:%zu prot:%x", fd, size, prot);
            } else {
                *addr = mBase;
                mMapSize = size;
                ret = C2_OK;
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
    ALOGD("%s this:id %p:%u", __func__, this, id);
}

C2DmaLinearAllocator::~C2DmaLinearAllocator() {
    ALOGD("%s this:id %p:%u", __func__, this, getId());
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

c2_status_t C2DmaLinearAllocator::acquirePool(
        uint32_t capacity, C2MemoryUsage usage,
        std::shared_ptr<DmaBufferPool> &pool) {
    c2_status_t ret = C2_OK;
    (void)(capacity);

    uint64_t key = usage.expected;
    auto i = mPools.find(key);
    if (i != mPools.end()) {
        ALOGD("%s found pool of usage:0x%" PRIx64, LOG_TAG, key);
        pool = i->second;
    } else {
        pool = std::make_shared<DmaBufferPool>(&mBufferAllocator);
        if (pool) {
            ALOGD("%s created pool of usage:0x%" PRIx64, LOG_TAG, key);
            mPools.insert({key, pool});
        } else {
            ALOGE("%s error: pool is NULL", LOG_TAG);
            ret = C2_NO_MEMORY;
        }
    }

    return ret;
}

c2_status_t C2DmaLinearAllocator::newLinearAllocation(
        uint32_t capacity, C2MemoryUsage usage, std::shared_ptr<C2LinearAllocation> *allocation) {
    c2_status_t ret = C2_OK;
    ALOGD("%s %s this:id:capacity %p:%u:%u", LOG_TAG, __func__, this, getId(), capacity);

    if (allocation == nullptr) {
        return C2_BAD_VALUE;
    }

    std::shared_ptr<DmaBufferPool> pool;
    ret = acquirePool(capacity, usage, pool);
    if (C2_OK != ret) {
        return ret;
    }

    std::shared_ptr<C2DmaLinearAllocation> alloc
        = std::make_shared<C2DmaLinearAllocation>(pool, capacity, usage, getId());
    if (alloc) {
        ret = alloc->status();
        if (ret == C2_OK) {
            *allocation = alloc;
        }
    } else {
        ret = C2_NO_MEMORY;
        ALOGE("%s %s null alloc", LOG_TAG, __func__);
    }

    return ret;
}

c2_status_t C2DmaLinearAllocator::priorLinearAllocation(
        const C2Handle *handle, std::shared_ptr<C2LinearAllocation> *allocation) {
    c2_status_t ret = C2_OK;
    *allocation = nullptr;

    const C2DmaHandle *h = static_cast<const C2DmaHandle*>(handle);
    std::shared_ptr<C2DmaLinearAllocation> alloc
        = std::make_shared<C2DmaLinearAllocation>(h->bufferFd(), h->size(), getId());

    if (alloc) {
        ret = alloc->status();
    } else {
        ret = C2_NO_MEMORY;
    }
    if (ret == C2_OK) {
        *allocation = alloc;
        native_handle_delete(const_cast<native_handle_t*>(
                reinterpret_cast<const native_handle_t*>(handle)));
    }

    return ret;
}

bool C2DmaLinearAllocator::isValid(const C2Handle* const o) {
    return true;
}

} // namespace android
