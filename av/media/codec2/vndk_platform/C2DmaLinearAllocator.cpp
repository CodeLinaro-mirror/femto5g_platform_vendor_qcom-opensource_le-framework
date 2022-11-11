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
#include <C2Buffer.h>
#include <sys/mman.h>
#include <unistd.h>

namespace android {
/* ========================================= DMA HANDLE ======================================== */
struct C2DmaHandle : public C2Handle {

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

    C2DmaLinearAllocation(BufferAllocator& alloc, size_t size, unsigned flags, C2Allocator::id_t id);

    c2_status_t status() const { return C2_OK; };

private:
    int mFd;
    C2Allocator::id_t mId;
    C2DmaHandle mHandle;
    void *mBase;
    size_t mMapSize;
};

C2DmaLinearAllocation::C2DmaLinearAllocation(BufferAllocator& alloc, size_t size,unsigned flags, C2Allocator::id_t id)
    : C2LinearAllocation(size), mFd(-1), mBase(nullptr), mMapSize(0)
{
    int ret = 0;
    mFd = alloc.Alloc("qcom,system", size, flags);
    if (mFd < 0) {
        ret = mFd;
        return;
    }

    mHandle.version = 0;
    mHandle.numFds = 1;
    mHandle.numInts = 1;
    mHandle.data[0] = mFd;
    mId = id;
}

C2DmaLinearAllocation::~C2DmaLinearAllocation()
{
    if(mFd > 0) {
        close(mFd);
        mFd = -1;
        mHandle.data[0] = -1;
    }
}

c2_status_t C2DmaLinearAllocation::map(
    size_t offset, size_t size, C2MemoryUsage usage, C2Fence *fence,
    void **addr /* nonnull */)
{
    int prot = PROT_NONE;
    if (usage.expected & C2MemoryUsage::CPU_READ) {
        prot |= PROT_READ;
    }
    if (usage.expected & C2MemoryUsage::CPU_WRITE) {
        prot |= PROT_WRITE;
    }
    mBase = mmap(0, size, prot, MAP_SHARED, mFd, 0);
    if (mBase == MAP_FAILED) {
        mBase = *addr = nullptr;
        return C2_BAD_VALUE;
    }

    *addr = mBase;
    mMapSize = size;

    return C2_OK;
}

c2_status_t C2DmaLinearAllocation::unmap(void *addr, size_t size, C2Fence *fenceFd)
{
    int ret = munmap(mBase, mMapSize);
    if (ret) {
        printf("failed to ummap dma mMapSize %u", mMapSize);
        return C2_BAD_VALUE;
    }
    return C2_OK;
}

const C2Handle *C2DmaLinearAllocation::handle() const
{
    return &mHandle;
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

    //size_t align = 0;
    int flags = 0;
    if (usage.expected & C2MemoryUsage::CPU_READ)
        flags |= PROT_READ;
    if (usage.expected & C2MemoryUsage::CPU_WRITE)
        flags |= PROT_WRITE;
    std::shared_ptr<C2DmaLinearAllocation> alloc
        = std::make_shared<C2DmaLinearAllocation>(mBufferAllocator, capacity, flags, getId());
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
