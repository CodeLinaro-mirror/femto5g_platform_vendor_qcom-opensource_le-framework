/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <C2Component.h>
#include <mutex>

#include <C2AllocatorGBM.h>
#include <C2BlockInternal.h>

#ifdef _SUPPORT_DMABUF_
#include <C2DmaLinearAllocator.h>
#else
#include <C2AllocatorIon.h>
#endif

#include <map>
#include <mutex>

namespace android {


/* ===================================================== */

/* Linear BlockPool */
class C2PlatformLinearBlockPool : public C2BlockPool {
public:
    C2PlatformLinearBlockPool(const std::shared_ptr<C2Allocator> &allocator)
    : mAllocator(allocator) {}

    ~C2PlatformLinearBlockPool() override = default;

    c2_status_t fetchLinearBlock(
            uint32_t capacity,
            C2MemoryUsage usage,
            std::shared_ptr<C2LinearBlock> *block /* nonnull */) override;

    local_id_t getLocalId() const override {
        return localId;
    };

    C2Allocator::id_t getAllocatorId() const override {
        return allocatorId;
    };

private:
    const std::shared_ptr<C2Allocator> mAllocator;
    local_id_t localId;
    C2Allocator::id_t allocatorId;
};

c2_status_t C2PlatformLinearBlockPool::fetchLinearBlock(
            uint32_t capacity,
            C2MemoryUsage usage,
            std::shared_ptr<C2LinearBlock> *block /* nonnull */)
{
    std::shared_ptr<C2LinearAllocation> alloc;
    c2_status_t err = mAllocator->newLinearAllocation(capacity, usage, &alloc);
    if (err != C2_OK) {
        return err;
    }

    *block = _C2BlockFactory::CreateLinearBlock(alloc);

    return C2_OK;
}

/* Graphic BlockPool */
class C2PlatformGraphicBlockPool : public C2BlockPool {
public:
    C2PlatformGraphicBlockPool(const std::shared_ptr<C2Allocator> &allocator)
    : mAllocator(allocator) {}

    ~C2PlatformGraphicBlockPool() override = default;

    c2_status_t fetchGraphicBlock(
                uint32_t width __unused, uint32_t height __unused, uint32_t format __unused,
                C2MemoryUsage usage __unused,
                std::shared_ptr<C2GraphicBlock> *block /* nonnull */) override;


    local_id_t getLocalId() const override {
        return localId;
    };

    C2Allocator::id_t getAllocatorId() const override {
        return allocatorId;
    };

private:
    const std::shared_ptr<C2Allocator> mAllocator;
    local_id_t localId;
    C2Allocator::id_t allocatorId;
};

c2_status_t C2PlatformGraphicBlockPool::fetchGraphicBlock(
            uint32_t width __unused, uint32_t height __unused, uint32_t format __unused,
            C2MemoryUsage usage __unused,
            std::shared_ptr<C2GraphicBlock> *block /* nonnull */) {

    c2_status_t err = C2_NO_INIT;
    std::shared_ptr<C2GraphicAllocation> alloc;
    std::shared_ptr<android::C2AllocatorGBM> allocatorGBM =
        std::dynamic_pointer_cast<android::C2AllocatorGBM>(mAllocator);

    if (allocatorGBM && allocatorGBM->isUseExternalBuffer()) {
        allocatorGBM->acquireExtBuffer(width, height);
        C2Handle *c2Handle = nullptr;
        err = allocatorGBM->createC2HandleGBM(c2Handle, width, height, format, usage.expected);
        if (err == C2_OK) {
            err = mAllocator->priorGraphicAllocation(c2Handle, &alloc);
        }
    } else if (mAllocator) {
        err = mAllocator->newGraphicAllocation(width, height, format, usage, &alloc);
    }

    if (err != C2_OK) {
        return err;
    }

    *block = _C2BlockFactory::CreateGraphicBlock(alloc);
    return C2_OK;
}

class C2PooledLinearBlockPool : public C2PlatformLinearBlockPool {
public:
    C2PooledLinearBlockPool(const std::shared_ptr<C2Allocator> &allocator, const local_id_t localId);

    virtual ~C2PooledLinearBlockPool() override;

    virtual C2Allocator::id_t getAllocatorId() const override {
        return mAllocator->getId();
    }

    virtual local_id_t getLocalId() const override {
        return mLocalId;
    }

private:
    const std::shared_ptr<C2Allocator> mAllocator;
    const local_id_t mLocalId;
};

C2PooledLinearBlockPool::C2PooledLinearBlockPool(
        const std::shared_ptr<C2Allocator> &allocator, const local_id_t localId)
        : C2PlatformLinearBlockPool(allocator), mAllocator(allocator), mLocalId(localId) {}

C2PooledLinearBlockPool::~C2PooledLinearBlockPool() {
}

class C2PooledGraphicBlockPool : public C2PlatformGraphicBlockPool {
public:
    C2PooledGraphicBlockPool(const std::shared_ptr<C2Allocator> &allocator, const local_id_t localId);

    virtual ~C2PooledGraphicBlockPool() override;

    virtual C2Allocator::id_t getAllocatorId() const override {
        return mAllocator->getId();
    }

    virtual local_id_t getLocalId() const override {
        return mLocalId;
    }

private:
    const std::shared_ptr<C2Allocator> mAllocator;
    const local_id_t mLocalId;
};

C2PooledGraphicBlockPool::C2PooledGraphicBlockPool(
        const std::shared_ptr<C2Allocator> &allocator, const local_id_t localId)
        : C2PlatformGraphicBlockPool(allocator), mAllocator(allocator), mLocalId(localId) {}

C2PooledGraphicBlockPool::~C2PooledGraphicBlockPool() {
}

/* ===================================================== */
/* Allocator Store */
class C2PlatformAllocatorStore : public C2AllocatorStore {
public:
    typedef C2Allocator::id_t id_t;

    enum : C2Allocator::id_t {
        DEFAULT_LINEAR,     ///< basic linear allocator type
        DEFAULT_GRAPHIC,    ///< basic graphic allocator type
        PLATFORM_START = 0x10,
        VENDOR_START   = 0x100,
        MMAP_LINEAR    = 0x1000,
        MMAP_GRAPHIC   = 0x10000,
        GBM_GRAPHIC    = 0x100000,
        DMA_LINEAR     = 0x1000000,
        BAD_ID         = C2Allocator::BAD_ID, ///< DO NOT USE
    };

    C2String getName() const override;
    std::vector<std::shared_ptr<const C2Allocator::Traits>> listAllocators_nb() const override;
    c2_status_t fetchAllocator(id_t id, std::shared_ptr<C2Allocator>* const allocator) override;
};

C2String C2PlatformAllocatorStore::getName() const
{
    return "C2PlatformAllocatorStore";
}

std::vector<std::shared_ptr<const C2Allocator::Traits>> C2PlatformAllocatorStore::listAllocators_nb() const
{
    std::vector<std::shared_ptr<const C2Allocator::Traits>> tr;
    return tr;
}

c2_status_t C2PlatformAllocatorStore::fetchAllocator(id_t id, std::shared_ptr<C2Allocator>* const allocator)
{
    if (allocator == nullptr)
        return C2_NOT_FOUND;

    switch (id) {
    case C2AllocatorStore::DEFAULT_LINEAR:
#ifdef _SUPPORT_DMABUF_
        *allocator = std::make_shared<C2DmaLinearAllocator>(C2PlatformAllocatorStore::DMA_LINEAR);
#else
        *allocator = std::make_shared<C2AllocatorIon>(C2PlatformAllocatorStore::DEFAULT_LINEAR);
#endif
        break;
    case C2AllocatorStore::DEFAULT_GRAPHIC:
    default:
        *allocator = std::make_shared<C2AllocatorGBM>(C2PlatformAllocatorStore::GBM_GRAPHIC);
        break;
    }
    return C2_OK;
}

namespace {

class _C2BlockPoolCache {
public:
    _C2BlockPoolCache() : mBlockPoolSeqId(C2BlockPool::PLATFORM_START + 1) {}

    c2_status_t _createBlockPool(
            C2PlatformAllocatorStore::id_t allocatorId,
            std::shared_ptr<const C2Component> component,
            C2BlockPool::local_id_t poolId,
            std::shared_ptr<C2BlockPool> *pool) {
        std::shared_ptr<C2PlatformAllocatorStore> allocatorStore(new C2PlatformAllocatorStore);
        std::shared_ptr<C2Allocator> allocator;
        c2_status_t res = C2_NOT_FOUND;

        switch(allocatorId) {
            case C2AllocatorStore::DEFAULT_LINEAR:
                res = allocatorStore->fetchAllocator(
                        C2AllocatorStore::DEFAULT_LINEAR, &allocator);
                if (res == C2_OK) {
                    std::shared_ptr<C2BlockPool> ptr =
                        std::make_shared<C2PooledLinearBlockPool>(allocator, poolId);
                    *pool = ptr;
                    mBlockPools[poolId] = ptr;
                    mBlockAllocators[poolId] = allocator;
                    mComponents[poolId] = component;
                }
                break;
            case C2AllocatorStore::DEFAULT_GRAPHIC:
                res = allocatorStore->fetchAllocator(
                        C2AllocatorStore::DEFAULT_GRAPHIC, &allocator);
                if (res == C2_OK) {
                    std::shared_ptr<C2BlockPool> ptr =
                        std::make_shared<C2PooledGraphicBlockPool>(allocator, poolId);
                    *pool = ptr;
                    mBlockPools[poolId] = ptr;
                    mBlockAllocators[poolId] = allocator;
                    mComponents[poolId] = component;
                }
                break;
            default:
                break;
        }
        return res;
    }

    c2_status_t createBlockPool(
            C2PlatformAllocatorStore::id_t allocatorId,
            std::shared_ptr<const C2Component> component,
            std::shared_ptr<C2BlockPool> *pool) {
        return _createBlockPool(allocatorId, component, mBlockPoolSeqId++, pool);
    }

    bool getBlockPool(
            C2BlockPool::local_id_t blockPoolId,
            std::shared_ptr<const C2Component> component,
            std::shared_ptr<C2BlockPool> *pool) {
        // TODO: use one iterator for multiple blockpool type scalability.
        std::shared_ptr<C2BlockPool> ptr;
        auto it = mBlockPools.find(blockPoolId);
        if (it != mBlockPools.end()) {
            ptr = it->second.lock();
            if (!ptr) {
                mBlockPools.erase(it);
                mBlockAllocators.erase(blockPoolId);
                mComponents.erase(blockPoolId);
            } else {
                auto found = mComponents.find(blockPoolId);
                if (component == found->second.lock()) {
                    *pool = ptr;
                    return true;
                }
            }
        }
        return false;
    }

    bool getBlockPool(
            C2BlockPool::local_id_t blockPoolId,
            std::shared_ptr<const C2Component> component,
            std::shared_ptr<C2BlockPool> *pool,
            std::shared_ptr<C2Allocator> *allocator) {
        // TODO: use one iterator for multiple blockpool type scalability.
        bool ret = false;
        std::shared_ptr<C2BlockPool> ptr;
        std::shared_ptr<C2Allocator> alloc;
        auto it = mBlockPools.find(blockPoolId);
        if (it != mBlockPools.end()) {
            ptr = it->second.lock();
            if (!ptr) {
                mBlockPools.erase(it);
                mBlockAllocators.erase(blockPoolId);
                mComponents.erase(blockPoolId);
            } else {
                auto found = mComponents.find(blockPoolId);
                if (component == found->second.lock()) {
                    *pool = ptr;
                    ret = true;
                }
            }
        }

        if (ret == false)
          return ret;

        auto alloc_it  = mBlockAllocators.find(blockPoolId);
        if (alloc_it != mBlockAllocators.end()) {
            alloc = alloc_it->second.lock();
            if (!alloc) {
                mBlockAllocators.erase(alloc_it);
                mBlockPools.erase(blockPoolId);
                mComponents.erase(blockPoolId);
            } else {
                auto found = mComponents.find(blockPoolId);
                if (component == found->second.lock()) {
                    *allocator = alloc;
                    ret = true;
                }
            }
        }

        return ret;
    }

private:
    C2BlockPool::local_id_t mBlockPoolSeqId;

    std::map<C2BlockPool::local_id_t, std::weak_ptr<C2BlockPool>> mBlockPools;
    std::map<C2BlockPool::local_id_t, std::weak_ptr<C2Allocator>> mBlockAllocators;
    std::map<C2BlockPool::local_id_t, std::weak_ptr<const C2Component>> mComponents;
};

static std::unique_ptr<_C2BlockPoolCache> sBlockPoolCache =
    std::make_unique<_C2BlockPoolCache>();
static std::mutex sBlockPoolCacheMutex;

} // anynymous namespace

/* ===================================================== */
/* External */
c2_status_t GetCodec2BlockPool(
        C2BlockPool::local_id_t id, std::shared_ptr<const C2Component> component,
        std::shared_ptr<C2BlockPool> *pool) {
    pool->reset();
    std::lock_guard<std::mutex> lock(sBlockPoolCacheMutex);
    std::shared_ptr<C2PlatformAllocatorStore> allocatorStore(new C2PlatformAllocatorStore);
    std::shared_ptr<C2Allocator> allocator;
    c2_status_t res = C2_NOT_FOUND;

    if (id >= C2BlockPool::PLATFORM_START) {
        if (sBlockPoolCache->getBlockPool(id, component, pool)) {
            return C2_OK;
        }
    }

    switch (id) {
    case C2BlockPool::BASIC_LINEAR:
        res = allocatorStore->fetchAllocator(C2AllocatorStore::DEFAULT_LINEAR, &allocator);
        if (res == C2_OK) {
            *pool = std::make_shared<C2PlatformLinearBlockPool>(allocator);
        }
        break;
    case C2BlockPool::BASIC_GRAPHIC:
        res = allocatorStore->fetchAllocator(C2AllocatorStore::DEFAULT_GRAPHIC, &allocator);
        if (res == C2_OK) {
            *pool = std::make_shared<C2PlatformGraphicBlockPool>(allocator);
        }
        break;
    default:
        break;
    }
    return res;
}

c2_status_t CreateCodec2BlockPool(
        C2PlatformAllocatorStore::id_t allocatorId,
        std::shared_ptr<const C2Component> component,
        std::shared_ptr<C2BlockPool> *pool) {
    pool->reset();

    std::lock_guard<std::mutex> lock(sBlockPoolCacheMutex);
    return sBlockPoolCache->createBlockPool(allocatorId, component, pool);
}

c2_status_t GetCodec2BlockPoolWithAllocator(
        C2BlockPool::local_id_t id, std::shared_ptr<const C2Component> component,
        std::shared_ptr<C2BlockPool> *pool, std::shared_ptr<C2Allocator> *c2Allocator) {
    pool->reset();
    std::lock_guard<std::mutex> lock(sBlockPoolCacheMutex);
    std::shared_ptr<C2PlatformAllocatorStore> allocatorStore(new C2PlatformAllocatorStore);
    std::shared_ptr<C2Allocator> allocator;
    c2_status_t res = C2_NOT_FOUND;

    if (id >= C2BlockPool::PLATFORM_START) {
        if (sBlockPoolCache->getBlockPool(id, component, pool, &allocator)) {
            *c2Allocator = allocator;
            return C2_OK;
        }
    }

    switch (id) {
    case C2BlockPool::BASIC_LINEAR:
        res = allocatorStore->fetchAllocator(C2AllocatorStore::DEFAULT_LINEAR, &allocator);
        if (res == C2_OK) {
            *pool = std::make_shared<C2PlatformLinearBlockPool>(allocator);
            *c2Allocator = allocator;
        }
        break;
    case C2BlockPool::BASIC_GRAPHIC:
        res = allocatorStore->fetchAllocator(C2AllocatorStore::DEFAULT_GRAPHIC, &allocator);
        if (res == C2_OK) {
            *pool = std::make_shared<C2PlatformGraphicBlockPool>(allocator);
            *c2Allocator = allocator;
        }
        break;
    default:
        break;
    }
    return res;
}

} // namespace android
