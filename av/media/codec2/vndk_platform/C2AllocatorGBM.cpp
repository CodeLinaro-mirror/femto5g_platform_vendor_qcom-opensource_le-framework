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

#define LOG_NDEBUG 0
#include <C2AllocatorGBM.h>
#include <utils/Log.h>
#include <C2Buffer.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


namespace android {

const ExtraData* C2HandleGBM::getExtraData(const C2Handle *const handle) {
    if (handle == nullptr || handle->numInts < NUM_INTS) {
        return nullptr;
    }
    const C2HandleGBM *gbmHandle = reinterpret_cast<const C2HandleGBM *>(handle);

    return &gbmHandle->mInts;
}

const C2HandleGBM* C2HandleGBM::Import(
        const C2Handle *const handle,
        uint32_t *width, uint32_t *height, uint32_t *format,
        uint64_t *usage, uint32_t *stride, uint32_t *size)
{
    const ExtraData *xd = getExtraData(handle);
    if (xd == nullptr) {
        return nullptr;
    }

    *width  = xd->width;
    *height = xd->height;
    *format = xd->format;
    *usage  = xd->usage_lo | (uint64_t(xd->usage_hi) << 32);
    *stride = xd->stride;
    *size   = xd->size;

    return reinterpret_cast<const C2HandleGBM *>(handle);
}


C2AllocationGBM::C2AllocationGBM(struct gbm_device *gbm, uint32_t width, uint32_t height,
        uint32_t format, uint64_t usage, C2Allocator::id_t allocatorId)
    : C2GraphicAllocation(width, height), mBase(nullptr),
    mMapSize(0), mAllocatorId(allocatorId)
{
    if (!gbm) {
        ALOGE("Invalid gbm device");
    } else {
        Alloc(gbm, width, height, format, usage);
    }
}

C2AllocationGBM::~C2AllocationGBM()
{
    if(mBo) {
        gbm_bo_destroy(mBo);
        mBo = nullptr;
        delete mHandle;
        mHandle = nullptr;
    }
}

bool C2AllocationGBM::Alloc(struct gbm_device *gbm, uint32_t w, uint32_t h, uint32_t format, int flag)
{
    uint32_t flags = flag | GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;
    struct gbm_bo *bo = NULL;
    int bo_fd = -1, meta_fd = -1;

    ALOGV("creating gbm_bo with width:%d, height:%d foramt:%x, flag:%d", w, h, format, flag);

    bo = gbm_bo_create(gbm, w, h, format, flags);

    if (bo == NULL) {
        ALOGE("no supported gbm bo for format %x", format);
        gbm_device_destroy(gbm);
        return false;
    }

    //bo_fd = gbm_bo_get_fd(bo);
    //TODO: use gbm_bo_get_fd
    bo_fd = bo->ion_fd;
    if (bo_fd < 0) {
        ALOGE("Get bo fd failed");
        gbm_bo_destroy(bo);
        gbm_device_destroy(gbm);
        return false;
    }

    gbm_perform(GBM_PERFORM_GET_METADATA_ION_FD, bo, &meta_fd);
    if (meta_fd < 0) {
        ALOGE("Get bo meta fd failed");
        gbm_bo_destroy(bo);
        gbm_device_destroy(gbm);
        return false;
    }

    mBo = bo;

    mHandle = new C2HandleGBM();
    mHandle->version = C2HandleGBM::VERSION;
    mHandle->numFds = C2HandleGBM::NUM_FDS;
    mHandle->numInts = C2HandleGBM::NUM_INTS;
    mHandle->mFds.buffer_fd = bo_fd;
    mHandle->mFds.meta_buffer_fd = meta_fd;

    mHandle->mInts.width = bo->width;
    mHandle->mInts.height = bo->height;
    mHandle->mInts.stride = bo->stride;
    mHandle->mInts.slice_height =  bo->aligned_height;
    mHandle->mInts.format = format;
    mHandle->mInts.usage_lo = flags;
    mHandle->mInts.size = bo->size;
    //Use fd as the unique buffer id for C2Buffer
    mHandle->mInts.id = bo_fd;

    ALOGV("GBM handle data: fd:%d meta_fd:%d width:%u height:%u format:%u usage_lo:%u "
          "usage_hi:%u stride:%u slice_height:%u size:%u",
            mHandle->data[0], mHandle->data[1], mHandle->data[2], mHandle->data[3],
            mHandle->data[4], mHandle->data[5], mHandle->data[6], mHandle->data[7],
            mHandle->data[8], mHandle->data[9]);

    ALOGV("created gbm bo fd meta_fd size width height %p %d %d %d %d %d",
            bo, bo_fd, meta_fd, bo->size, bo->width, bo->height);

    return true;
}

c2_status_t C2AllocationGBM::map(C2Rect rect, C2MemoryUsage usage,
        C2Fence *fence, C2PlanarLayout *layout, uint8_t **addr)
{
    int fd = mHandle->mFds.buffer_fd;
    int size = mHandle->mInts.size;

    mBase = mmap(NULL, size, PROT_READ|PROT_WRITE,
            MAP_SHARED, fd, 0);
    if (mBase == MAP_FAILED) {
        ALOGE("failed to mmap shmem object, errno = %d", errno);
        return C2_BAD_VALUE;
    }

    *addr = (uint8_t*) mBase;
    ALOGV("mapping gbm fd: %d, size: %d addr:%p", fd, size, *addr);
    mMapSize = size;

    return C2_OK;
}

c2_status_t C2AllocationGBM::unmap(uint8_t **addr, C2Rect rect, C2Fence *fence)
{
    int ret = munmap(mBase, mMapSize);
    ALOGV("unmap gbm buffer addr:%p size:%zu", mBase, mMapSize);

    if (ret) {
        ALOGE("failed to ummap shmem object, errno = %d", errno);
        return C2_BAD_VALUE;
    } else {
        mBase = NULL;
        mMapSize = 0;
    }

    return C2_OK;
}

const C2Handle *C2AllocationGBM::handle() const
{
    return reinterpret_cast<const C2Handle*>(mHandle);
}

id_t C2AllocationGBM::getAllocatorId() const
{
    return mAllocatorId;
}

bool C2AllocationGBM::equals(const std::shared_ptr<const C2GraphicAllocation> &other) const
{
    return other && other->handle() == handle();
}

C2AllocatorGBM::C2AllocatorGBM(id_t id)
    : mInit(C2_NO_INIT),
    mGBM(NULL),
    mDevice_fd(-1)
{
    C2MemoryUsage minUsage = { 0, 0 };
    C2MemoryUsage maxUsage = { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE };
    Traits traits = { "linux.allocator.gbm", id, GRAPHIC, minUsage, maxUsage };
    mTraits = std::make_shared<Traits>(traits);

    mDevice_fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);

    if (mDevice_fd < 0) {
        ALOGE("opening dri device for gbm failed");
    } else {
        mGBM = gbm_create_device(mDevice_fd);
        if (mGBM == NULL) {
            ALOGE("create gbm device failed with fd: %d", mDevice_fd);
        } else {
            mInit = C2_OK;
            ALOGV( "Successfully created gbm device: %p", mGBM);
        }
    }
}

C2AllocatorGBM::~C2AllocatorGBM()
{
    ALOGV( "destroy C2AllocatorGBM");
    if (mGBM) {
        gbm_device_destroy(mGBM);
        mGBM = NULL;
    }

    if (mDevice_fd > 0) {
        close(mDevice_fd);
        mDevice_fd = -1;
    }

    mInit = C2_NO_INIT;
}

C2Allocator::id_t C2AllocatorGBM::getId() const
{
    return mTraits->id;
}

C2String C2AllocatorGBM::getName() const
{
    return mTraits->name;
}

std::shared_ptr<const C2Allocator::Traits> C2AllocatorGBM::getTraits() const
{
    return mTraits;
}

c2_status_t C2AllocatorGBM::newGraphicAllocation( uint32_t width, uint32_t height,
        uint32_t format, C2MemoryUsage usage,
        std::shared_ptr<C2GraphicAllocation> *allocation)
{
    c2_status_t ret = C2_OK;

    if (allocation == nullptr) {
        return C2_BAD_VALUE;
    }

    if (mInit != C2_OK) {
        ALOGE("GBM device is not created, unexpected");
        return C2_NO_INIT;
    }

    std::shared_ptr<C2AllocationGBM> alloc
        = std::make_shared<C2AllocationGBM>(mGBM, width, height, format, usage.expected, mTraits->id);
    ret = alloc->status();

    if (ret == C2_OK) {
        *allocation = alloc;
    }

    return ret;
}

c2_status_t C2AllocatorGBM::priorGraphicAllocation(
        const C2Handle *handle, std::shared_ptr<C2GraphicAllocation> *allocation)
{
    c2_status_t ret = C2_OK;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;
    uint64_t flags = 0;
    uint32_t stride = 0;
    uint32_t size = 0;

    if (mInit != C2_OK) {
        ALOGE("GBM device is not created, unexpected");
        return C2_NO_INIT;
    }

    const C2HandleGBM *gbmHandle = C2HandleGBM::Import(handle, &width, &height, &format, &flags, &stride, &size);

    if (gbmHandle == nullptr) {
        allocation->reset(new C2AllocationGBM(mGBM, width, height, format, flags, mTraits->id));
    } else {
        ret = C2_BAD_VALUE;
        ALOGE("priorGraphicAllocation failed due to invalid handle");
    }

    return ret;
}

bool C2AllocatorGBM::isValid(const C2Handle* const o) {
    return true;
}

} // namespace android

void _UnwrapNativeCodec2GBMMetadata(
        const C2Handle *const handle,
        uint32_t *width, uint32_t *height, uint32_t *format,uint64_t *usage, uint32_t *stride, uint32_t *size) {
    (void)android::C2HandleGBM::Import(handle, width, height, format, usage, stride, size);
}

