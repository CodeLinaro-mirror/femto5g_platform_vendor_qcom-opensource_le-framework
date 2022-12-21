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

#ifndef _CODEC2_DMA_LINEAR_ALLOCATOR_H_
#define _CODEC2_DMA_LINEAR_ALLOCATOR_H_

#include <BufferAllocator/BufferAllocator.h>
#include <C2Buffer.h>

namespace android {

class C2DmaLinearAllocator : public C2Allocator {
public:

    virtual id_t getId() const override;

    virtual C2String getName() const override;

    virtual std::shared_ptr<const Traits> getTraits() const override;

    virtual c2_status_t newLinearAllocation(
            uint32_t capacity, C2MemoryUsage usage,
            std::shared_ptr<C2LinearAllocation> *allocation) override;

    virtual c2_status_t priorLinearAllocation(
            const C2Handle *handle,
            std::shared_ptr<C2LinearAllocation> *allocation) override;

    C2DmaLinearAllocator(id_t id);

    virtual c2_status_t status() const { return mInit; }

    virtual ~C2DmaLinearAllocator() override;

    static bool isValid(const C2Handle* const o);

private:
    c2_status_t mInit;

    BufferAllocator mBufferAllocator;

    std::shared_ptr<const Traits> mTraits;
};

} // namespace android

#endif // _CODEC2_DMA_LINEAR_ALLOCATOR_H_
