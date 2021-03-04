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

#include <C2Component.h>
#include <mutex>

#ifdef _LINUX_
#include <C2AllocatorIon.h>
#include <C2AllocatorGBM.h>
#else
#include <C2AllocatorMmap.h>
#include <C2AllocatorMmapGraphic.h>
#endif

#include <C2BlockInternal.h>

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

    std::shared_ptr<C2GraphicAllocation> alloc;
    c2_status_t err = mAllocator->newGraphicAllocation(width, height, format, usage, &alloc);
    if (err != C2_OK) {
        return err;
    }

    *block = _C2BlockFactory::CreateGraphicBlock(alloc);
    return C2_OK;
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
#ifdef _LINUX_
        *allocator = std::make_shared<C2AllocatorIon>(C2PlatformAllocatorStore::DEFAULT_LINEAR);
#else
        *allocator = std::make_shared<C2AllocatorMmap>(C2PlatformAllocatorStore::MMAP_LINEAR);
#endif
        break;
    case C2AllocatorStore::DEFAULT_GRAPHIC:
    default:
#ifdef _LINUX_
        *allocator = std::make_shared<C2AllocatorGBM>(C2PlatformAllocatorStore::GBM_GRAPHIC);
#else
        *allocator = std::make_shared<C2AllocatorMmapGraphic>(C2PlatformAllocatorStore::MMAP_GRAPHIC);
#endif
        break;
    }
    return C2_OK;
}

/* ===================================================== */
/* External */


c2_status_t GetCodec2BlockPool(
        C2BlockPool::local_id_t id, std::shared_ptr<const C2Component> component,
        std::shared_ptr<C2BlockPool> *pool) {
    pool->reset();
    std::shared_ptr<C2PlatformAllocatorStore> allocatorStore(new C2PlatformAllocatorStore);
    std::shared_ptr<C2Allocator> allocator;
    c2_status_t res = C2_NOT_FOUND;

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

} // namespace android
