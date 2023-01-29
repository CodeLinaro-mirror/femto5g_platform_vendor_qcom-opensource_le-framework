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

#define LOG_NDEBUG 0
#define LOG_TAG "C2AllocatorGBM"
#include <C2AllocatorGBM.h>
#include <utils/Log.h>
#include <C2Buffer.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <inttypes.h>
#include <dlfcn.h>
using namespace std::chrono_literals;


#define DEFAULT_POOL_SIZE 6
#define DEFAULT_EXTEND_POOL_SIZE 3
#define WAIT_BUF_TIME 100ms

#ifdef _AGL_LINUX_
#include <syslog.h>
#undef ALOGV
#undef ALOGD
#undef ALOGE
#undef ALOGW

#define _C2_GBM_LOG(level, format, args...)  \
    syslog(LOG_INFO, "%s %d:%s: " format "\n", LOG_TAG, __LINE__, __func__, ##args)

#define ALOGV(fmt, args...) _C2_GBM_LOG(LOG_INFO, fmt, ##args)
#define ALOGD(fmt, args...) _C2_GBM_LOG(LOG_INFO, fmt, ##args)
#define ALOGE(fmt, args...) _C2_GBM_LOG(LOG_ERR,  fmt, ##args)
#define ALOGW(fmt, args...) _C2_GBM_LOG(LOG_WARNING,  fmt, ##args)
#endif

#define LOAD_LIBGBM_OR_ERR_RETURN()                                 \
    do {                                                            \
        const char* gbm_lib_name = "libgbm.so";                     \
        const char *dlerr = NULL;                                   \
                                                                    \
        sGbmLib = dlopen (gbm_lib_name, RTLD_NOW);                  \
        if (NULL == sGbmLib) {                                      \
            dlerr = dlerror();                                      \
            if (NULL == dlerr)                                      \
                dlerr = "NULL";                                     \
            ALOGE ("dlopen %s error: %s", gbm_lib_name, dlerr);     \
            return;                                                 \
        }                                                           \
        ALOGD ("loaded libgbm.so");                                 \
    } while (0)

#define UNLOAD_LIBGBM()                   \
    do {                                  \
        if (sGbmLib) {                    \
            dlclose(sGbmLib);             \
            ALOGD ("unloaded libgbm.so"); \
        }                                 \
    } while (0)

#define LOAD_SYMBOL_OR_ERR_RETURN(handle, sym, func_name)           \
    do {                                                            \
        dlerror (); /* clear any existing error */                  \
        sFunc##func_name = (LINK##func_name)dlsym(handle, #sym);    \
        const char *dlerr = dlerror ();                             \
        if (NULL != dlerr) {                                        \
            ALOGE ("dlsym %s error: %s", #sym, dlerr);              \
            return;                                                 \
        }                                                           \
        ALOGD ("loaded symbol %s", #sym);                           \
    } while (0)


namespace android {

DEFINE_FUNC_PTR_BY_SYM(GbmCreateDevice);
DEFINE_FUNC_PTR_BY_SYM(GbmDeviceDestroy);
DEFINE_FUNC_PTR_BY_SYM(GbmBoCreate);
DEFINE_FUNC_PTR_BY_SYM(GbmPerform);
DEFINE_FUNC_PTR_BY_SYM(GbmBoDestory);
DEFINE_FUNC_PTR_BY_SYM(GbmBoImport);
DEFINE_FUNC_PTR_BY_SYM(GbmBoGetFd);
void* GbmLib::sGbmLib = nullptr;
bool GbmLib::sLoaded = false;
std::mutex GbmLib::sLock;

GbmLib::~GbmLib() {
    // unload lib only once process exited
    UNLOAD_LIBGBM();
}

void GbmLib::loadGbm() {
    // load lib only once per process

    std::unique_lock<std::mutex> lock(sLock);
    if (!sLoaded) {
        LOAD_LIBGBM_OR_ERR_RETURN();

        LOAD_SYMBOL_OR_ERR_RETURN(sGbmLib, gbm_create_device, GbmCreateDevice);
        LOAD_SYMBOL_OR_ERR_RETURN(sGbmLib, gbm_device_destroy, GbmDeviceDestroy);
        LOAD_SYMBOL_OR_ERR_RETURN(sGbmLib, gbm_bo_create, GbmBoCreate);
        LOAD_SYMBOL_OR_ERR_RETURN(sGbmLib, gbm_perform, GbmPerform);
        LOAD_SYMBOL_OR_ERR_RETURN(sGbmLib, gbm_bo_destroy, GbmBoDestory);
        LOAD_SYMBOL_OR_ERR_RETURN(sGbmLib, gbm_bo_import, GbmBoImport);
        LOAD_SYMBOL_OR_ERR_RETURN(sGbmLib, gbm_bo_get_fd, GbmBoGetFd);
        sLoaded = true;
    }
}

// Global variable
static GbmLib gbmLib;

#define _print_buf_entry(str, buf_entry, pool)                                        \
{                                                                                     \
    if (buf_entry != nullptr) {                                                       \
        ALOGV("%s entry:%p used:%d fd:%u meta_fd:%u format:0x%x wxh:%ux%u pool:%p\n", \
            str, (buf_entry).get(), (buf_entry)->used, (buf_entry)->bo_fd,            \
            (buf_entry)->meta_fd, (uint32_t)((buf_entry)->res_fmt_id >> 32),          \
            (uint32_t)(((buf_entry)->res_fmt_id >> 16) & 0xFFFF),                     \
            (uint32_t)((buf_entry)->res_fmt_id & 0xFFFF),                             \
            (pool));                                                                  \
   }                                                                                  \
}

static inline void close_fd(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}

BufferEntryInfo::BufferEntryInfo(bool used, uint64_t res_fmt_id,
      struct gbm_bo * bo, int32_t bo_fd, int32_t meta_fd)
    : used(used), res_fmt_id(res_fmt_id), bo(bo), bo_fd(bo_fd), meta_fd(meta_fd)
{
}

BufferPool::BufferPool()
    :mMaxBufferCount(DEFAULT_POOL_SIZE),
    mExtBufferCount(0)
{
    GbmLib::loadGbm();
}

c2_status_t BufferPool::setMaxBufferCount(uint32_t size)
{
    std::lock_guard<std::mutex> listLock(mLock);
    mMaxBufferCount = size > DEFAULT_POOL_SIZE ? size : DEFAULT_POOL_SIZE;
    mExtBufferCount = 0;
    ALOGV("set max bufferCount of pool:%p: want:%d, result:%d, and reset extend buffer count",
        this, size, mMaxBufferCount);

    return C2_OK;
}

uint32_t BufferPool::getBufferCountOfCurResFmt()
{
    // caller need ensure lock before calling
    uint32_t bufferCount = 0;
    auto itr = mBufferList.begin();
    while (itr != mBufferList.end()) {
        if ((*itr)->res_fmt_id == mCurResFmtId) {
            bufferCount++;
        }
        ++itr;
    }

    return bufferCount;
}

c2_status_t BufferPool::acquireBuffer(std::shared_ptr<BufferEntryInfo> &entry, uint32_t width, uint32_t height, uint32_t format)
{
    std::unique_lock<std::mutex> listLock(mLock);
    auto itr = mBufferList.begin();
    uint64_t res_fmt_id = (uint64_t)format << 32 | width << 16 | height;
    c2_status_t ret = C2_OK;
    bool acquired = false;
    bool createNewEntry = false;

    // If there's a resolution change, destory related
    // gbm buffer and erase it from the buffer list.
    if (res_fmt_id != mCurResFmtId) {
        while (itr != mBufferList.end()) {
            // Don't free the gbm buffer be still used by display.
            // It should be freed in releaseBuffer once buffer is not used by display any more.
            if ((*itr)->used == false) {
                _print_buf_entry ("finalize", *itr, this);
                close_fd((*itr)->bo_fd);
                GbmLib::sFuncGbmBoDestory((*itr)->bo);
                itr = mBufferList.erase(itr);
            } else {
                ++itr;
            }
        }
        // set new resolution id
        mCurResFmtId = res_fmt_id;
    }

    while (!acquired) {
        itr = mBufferList.begin();
        // Need to allocate new bufer entry since the buffer
        // count is not more than the expected count.
        if (getBufferCountOfCurResFmt() < mMaxBufferCount) {
            createNewEntry = true;
            ALOGV("need to create new entry, pool:%p", this);
            break;
        } else {
            itr = mBufferList.begin();
            while (itr != mBufferList.end()) {
                if ((*itr)->used == false) {
                    (*itr)->used = true;
                    _print_buf_entry("acquired", *itr, this)
                    acquired = true;
                    break;
                } else {
                    ++itr;
                }
            }

            if (itr == mBufferList.end()) {
                ALOGV("waiting for buffer, pool:%p", this);
                if (std::cv_status::timeout == mEmptyCondition.wait_for(listLock, WAIT_BUF_TIME)) {
                    ALOGV("waiting for buffer time out, pool:%p", this);
                    // increase buffer count to make new entry possible
                    if (mExtBufferCount < DEFAULT_EXTEND_POOL_SIZE) {
                        mMaxBufferCount++;
                        mExtBufferCount++;
                        createNewEntry = true;
                        ALOGV("create a exteneded buf since time out");
                        break;
                    } else {
                        ALOGW("Warning: wait idle buf timeout and extend buf cnt(%d) reach limit, pool:%p", mExtBufferCount, this);
                        ret = C2_TIMED_OUT;
                        break;
                    }
                } else {
                    ALOGV("waited for buffer, pool:%p", this);
                }
            }
        }
    }

    if (ret == C2_OK && createNewEntry == false) {
        entry = *itr;
    }

    return ret;
}

c2_status_t BufferPool::releaseBuffer(std::shared_ptr<BufferEntryInfo> entry)
{
    c2_status_t ret = C2_OK;
    bool found = false;

    if (entry == nullptr) {
        ALOGE ("Bad value BufferEntryInfo");
        ret = C2_BAD_VALUE;
    }

    if (ret == C2_OK)
    {
        std::lock_guard<std::mutex> listLock(mLock);
        auto itr = mBufferList.begin();

        _print_buf_entry("release", entry, this)
        while (itr != mBufferList.end()) {
            if ((*itr).get() == entry.get()) {
                if (entry->res_fmt_id == mCurResFmtId) {
                    entry->used = false;
                    if (getBufferCountOfCurResFmt() > mMaxBufferCount) {
                        _print_buf_entry ("decrease", *itr, this);
                        close_fd((*itr)->bo_fd);
                        GbmLib::sFuncGbmBoDestory((*itr)->bo);
                        itr = mBufferList.erase(itr);
                        ALOGV("buffer list size:%zu after decreased", mBufferList.size());
                    }
                } else {
                    // destory old buffer once reconfig
                    _print_buf_entry ("finalize", *itr, this);
                    close_fd((*itr)->bo_fd);
                    GbmLib::sFuncGbmBoDestory((*itr)->bo);
                    itr = mBufferList.erase(itr);
                    ALOGV("release buffer list size:%zu", mBufferList.size());
                }
                found = true;
                break;
            } else {
                ++itr;
            }
        }

        if (!found) {
            mBufferList.push_back(entry);
            ALOGV("add new entry to buffer list, pool:%p", this);
        }
    }

    if (ret == C2_OK) {
        mEmptyCondition.notify_one();
        ALOGV("notify since one buffer released, pool:%p", this);
    }

    return ret;
}

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
        uint64_t *usage, uint32_t *stride, uint32_t *size, uint64_t *bo)
{
    const ExtraData *xd = getExtraData(handle);
    if (xd == nullptr) {
        return nullptr;
    }

    if (width)
        *width  = xd->width;
    if (height)
        *height = xd->height;
    if (format)
        *format = xd->format;
    if (usage)
        *usage  = xd->usage_lo | (uint64_t(xd->usage_hi) << 32);
    if (stride)
        *stride = xd->stride;
    if (size)
        *size = xd->size;
    if (bo)
        *bo = xd->bo_lo | (uint64_t(xd->bo_hi) << 32);

    return reinterpret_cast<const C2HandleGBM *>(handle);
}


C2AllocationGBM::C2AllocationGBM(struct gbm_device *gbm, std::shared_ptr<BufferPool>& pool, uint32_t width,
        uint32_t height, uint32_t format, uint64_t usage, C2Allocator::id_t allocatorId, C2HandleGBM *handle)
    : C2GraphicAllocation(width, height), mHandle(handle), mBufEntryInfo(nullptr), mBase(nullptr),
    mMapSize(0), mPool(pool), mAllocatorId(allocatorId), mRet(C2_OK)
{
    if (!gbm) {
        ALOGE("Invalid gbm device");
        mRet = C2_BAD_VALUE;
    } else {
        if (mHandle == nullptr) {
          mRet = Alloc(gbm, width, height, format, usage);
        }
    }
}

C2AllocationGBM::~C2AllocationGBM()
{
    ALOGV("destroy allocation");
    if (mBufEntryInfo) {
        mPool->releaseBuffer(mBufEntryInfo);
    }
    if (mHandle) {
        delete mHandle;
        mHandle = nullptr;
    }
}

c2_status_t C2AllocationGBM::Alloc(struct gbm_device *gbm, uint32_t w, uint32_t h, uint32_t format, int flag)
{
    uint32_t flags = flag | GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;
    struct gbm_bo *bo = NULL;
    int32_t bo_fd = -1, meta_fd = -1;
    uint64_t res_fmt_id = (uint64_t)format << 32 | w << 16 | h;
    c2_status_t ret = C2_OK;

    if (flags & C2MemoryUsage::READ_PROTECTED) {
        flags |= GBM_BO_USAGE_PROTECTED_QTI;
    }

    ret = mPool->acquireBuffer(mBufEntryInfo, w, h, format);

    if (ret != C2_OK) {
        ALOGW("acquire buffer time out");
    } else {
        if (mBufEntryInfo) {
            bo = mBufEntryInfo->bo;
            bo_fd = mBufEntryInfo->bo_fd;
            meta_fd = mBufEntryInfo->meta_fd;
        } else {
            bo = GbmLib::sFuncGbmBoCreate(gbm, w, h, format, flags);

            if (bo == NULL) {
                ALOGE("no supported gbm bo for format %x", format);
                GbmLib::sFuncGbmDeviceDestroy(gbm);
                ret = C2_BAD_VALUE;
            } else {
                bo_fd = GbmLib::sFuncGbmBoGetFd(bo);
                if (bo_fd < 0) {
                    ALOGE("Get bo fd failed");
                    GbmLib::sFuncGbmBoDestory(bo);
                    GbmLib::sFuncGbmDeviceDestroy(gbm);
                    ret = C2_BAD_VALUE;
                } else {
                    GbmLib::sFuncGbmPerform(GBM_PERFORM_GET_METADATA_ION_FD, bo, &meta_fd);
                    if (meta_fd < 0) {
                        ALOGE("Get bo meta fd failed");
                        close_fd(bo_fd);
                        GbmLib::sFuncGbmBoDestory(bo);
                        GbmLib::sFuncGbmDeviceDestroy(gbm);
                        ret = C2_BAD_VALUE;
                    } else {
                        mBufEntryInfo = std::make_shared<BufferEntryInfo>(true, res_fmt_id, bo, bo_fd, meta_fd);
                        _print_buf_entry ("new", mBufEntryInfo, mPool.get());
                        // add new entry to buffer list
                        mPool->releaseBuffer(mBufEntryInfo);
                    }
                }
            }
        }

        if (ret == C2_OK) {
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
            mHandle->mInts.bo_lo = (uint32_t)((uint64_t)bo & 0xFFFFFFFF);
            mHandle->mInts.bo_hi = (uint32_t)(((uint64_t)bo >> 32) & 0xFFFFFFFF);

            ALOGV("GBM handle data: fd:%u meta_fd:%u width:%u height:%u format:%u usage_lo:%u "
                    "usage_hi:%u stride:%u slice_height:%u size:%u, bo:%" PRIu64,
                    mHandle->data[0], mHandle->data[1], mHandle->data[2], mHandle->data[3],
                    mHandle->data[4], mHandle->data[5], mHandle->data[6], mHandle->data[7],
                    mHandle->data[8], mHandle->data[9], (uint64_t(mHandle->data[11]) << 32) | mHandle->data[10]);

            ALOGV("created gbm bo:%p fd:%u meta_fd:%u size:%d width:%d height:%d",
                    bo, bo_fd, meta_fd, bo->size, bo->width, bo->height);
        }
    }

    return ret;
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
    mPool(std::make_shared<BufferPool>()),
    mDevice_fd(-1)
{
    GbmLib::loadGbm();
    C2MemoryUsage minUsage = { 0, 0 };
    C2MemoryUsage maxUsage = { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE };
    Traits traits = { "linux.allocator.gbm", id, GRAPHIC, minUsage, maxUsage };
    mTraits = std::make_shared<Traits>(traits);

    mDevice_fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);

    if (mDevice_fd < 0) {
        ALOGE("opening dri device for gbm failed");
    } else {
        mGBM = GbmLib::sFuncGbmCreateDevice(mDevice_fd);
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
    for (auto const& i: mPool->mBufferList) {
        if (i->bo) {
            close_fd(i->bo_fd);
            GbmLib::sFuncGbmBoDestory (i->bo);
            _print_buf_entry ("finalize", i, mPool.get());
        }
    }
    mPool->mBufferList.clear();

    for (auto const& i: mExternalBufferList) {
        if (i->bo) {
            //bo_fd is owned by external allocator, no need to close here
            GbmLib::sFuncGbmBoDestory (i->bo);
            _print_buf_entry ("ext gbm finalize", i, nullptr);
        }
    }
    mExternalBufferList.clear();

    if (mGBM) {
        GbmLib::sFuncGbmDeviceDestroy(mGBM);
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

    std::shared_ptr<C2AllocationGBM> alloc = std::make_shared<C2AllocationGBM>
        (mGBM, mPool, width, height, format, usage.expected, mTraits->id);
    if (alloc != nullptr) {
      ret = alloc->status();
    } else {
      ret = C2_NO_MEMORY;
    }

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
    uint64_t bo = 0;

    if (mInit != C2_OK) {
        ALOGE("GBM device is not created, unexpected");
        return C2_NO_INIT;
    }

    C2HandleGBM *gbmHandle = const_cast<C2HandleGBM*>(C2HandleGBM::Import(
        handle, &width, &height, &format, &flags, &stride, &size, &bo));

    if (gbmHandle != nullptr) {
        allocation->reset(new C2AllocationGBM(mGBM, mPool, width, height, format, flags,
                                              mTraits->id, gbmHandle));
    } else {
        ret = C2_BAD_VALUE;
        ALOGE("gbmHandle is NULL");
    }

    return ret;
}

bool C2AllocatorGBM::isValid(const C2Handle* const o) {
    return true;
}

c2_status_t C2AllocatorGBM::setMaxAllocationCount(uint32_t size) {
    mPool->setMaxBufferCount(size);
    ALOGV("set c2 allocator buffer count:%d", size);

    return C2_OK;
}

c2_status_t C2AllocatorGBM::setUseExternalBuffer(bool useExternal)
{
    mUseExternalBuffer = useExternal;
    ALOGV("Set to use external buffer: %s", useExternal ? "True" : "False");

    return C2_OK;
}

bool C2AllocatorGBM::isUseExternalBuffer()
{
    ALOGV("If use external buffer: %s", mUseExternalBuffer ? "True" : "False");

    return mUseExternalBuffer;
}

c2_status_t C2AllocatorGBM::attachExternalFd(int fd)
{
    bool found = false;

    if (fd <= 0) {
        ALOGE("Invalid fd value: %d", fd);
        return C2_BAD_VALUE;
    }

    {
        std::lock_guard<std::mutex> listLock(mLock);
        auto itr = mExternalBufferList.begin();
        while (itr != mExternalBufferList.end()) {
            if ((*itr)->bo_fd == fd) {
                (*itr)->used = false;
                found = true;
                break;
            } else {
                ++itr;
            }
        }
        if (!found) {
            std::shared_ptr<BufferEntryInfo> buffer =
                std::make_shared<BufferEntryInfo>(false, 0, nullptr, fd, -1);
            mExternalBufferList.push_back(buffer);
            ALOGV("Add new external entry to buffer list with fd=%d", fd);
        }
    }

    mEmptyCondition.notify_one();
    ALOGV("Notify since one buffer fd attached, fd=%d", fd);

    return C2_OK;
}

c2_status_t C2AllocatorGBM::createC2HandleGBM(C2Handle *&handle,
                              uint32_t width, uint32_t height,
                              uint32_t format, int flag)
{
    bool acquired = false;
    c2_status_t ret = C2_OK;
    auto itr = mExternalBufferList.begin();

    while (!acquired) {
        std::unique_lock<std::mutex> listLock(mLock);
        itr = mExternalBufferList.begin();
        while (itr != mExternalBufferList.end()) {
            if ((*itr)->used == false) {
                (*itr)->used = true;
                acquired = true;
                break;
            } else {
                ++itr;
            }
        }
        if (itr == mExternalBufferList.end()) {
            ALOGV("waiting for external buffer");
            if (std::cv_status::timeout == mEmptyCondition.wait_for(listLock, WAIT_BUF_TIME)) {
                ALOGE("waiting for external buffer time out");
                ret = C2_TIMED_OUT;
                break;
            } else {
                ALOGV("waited for external buffer");
            }
        }
    }

    if (acquired) {
        uint32_t flags = flag | GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;
        if (!(*itr)->bo) {
            struct gbm_import_fd_data bufData;
            bufData.fd = (*itr)->bo_fd;
            bufData.width = width;
            bufData.height = height;
            bufData.format = format;

            struct gbm_bo *gbmBo = NULL;
            gbmBo = GbmLib::sFuncGbmBoImport(mGBM, GBM_BO_IMPORT_FD, &bufData, flags);
            if (gbmBo) {
                (*itr)->bo = gbmBo;
            } else {
                ALOGE("Failed to import gbm bo for fd=%d", bufData.fd);
                ret = C2_BAD_VALUE;
            }
        }

        if (ret == C2_OK) {
             C2HandleGBM *handleGBM = new C2HandleGBM();
             handleGBM->version = C2HandleGBM::VERSION;
             handleGBM->numFds = C2HandleGBM::NUM_FDS;
             handleGBM->numInts = C2HandleGBM::NUM_INTS;
             handleGBM->mFds.buffer_fd = (*itr)->bo_fd;

             handleGBM->mInts.width = (*itr)->bo->width;
             handleGBM->mInts.height = (*itr)->bo->height;
             handleGBM->mInts.stride = (*itr)->bo->stride;
             handleGBM->mInts.slice_height =  (*itr)->bo->aligned_height;
             handleGBM->mInts.format = format;
             handleGBM->mInts.usage_lo = flags;
             handleGBM->mInts.size = (*itr)->bo->size;
             //Use fd as the unique buffer id for C2Buffer
             handleGBM->mInts.id = (*itr)->bo_fd;
             handleGBM->mInts.bo_lo = (uint32_t)((uint64_t)(*itr)->bo & 0xFFFFFFFF);
             handleGBM->mInts.bo_hi = (uint32_t)(((uint64_t)(*itr)->bo >> 32) & 0xFFFFFFFF);

             ALOGV("GBM handle data: fd:%u meta_fd:%u width:%u height:%u format:%u usage_lo:%u "
                    "usage_hi:%u stride:%u slice_height:%u size:%u, bo:%" PRIu64,
                    handleGBM->data[0], handleGBM->data[1], handleGBM->data[2], handleGBM->data[3],
                    handleGBM->data[4], handleGBM->data[5], handleGBM->data[6], handleGBM->data[7],
                    handleGBM->data[8], handleGBM->data[9], (uint64_t(handleGBM->data[11]) << 32) | handleGBM->data[10]);

             handle = (C2Handle*)handleGBM;
        }
    } else {
        ALOGE("Failed to acquire available external buffer");
    }

    return ret;
}

c2_status_t C2AllocatorGBM::setAcquireExtBufCb(const AcquireExtBufFunc cb)
{
    mAcquireExtBufFunc = cb;

    return C2_OK;
}

c2_status_t C2AllocatorGBM::acquireExtBuffer(uint32_t width, uint32_t height)
{
    bool resolution_change = false;
    for (auto const& i: mExternalBufferList) {
        if (i->bo && (i->bo->width != width || i->bo->height != height)) {
            i->bo_fd = -1;  //it's specific mark, as later codec2 won't "use" this bo_fd, could set it as -1 here.
            resolution_change = true;
        }
    }

    if (mAcquireExtBufFunc) {
        mAcquireExtBufFunc(width, height);
    }

    if (resolution_change) {
        ALOGV("resolution changed to %dx%d with external buffer", width, height);
        auto itr = mExternalBufferList.begin();
        while (itr != mExternalBufferList.end()) {
            if ((*itr)->bo && ((*itr)->bo_fd == -1)) {
                GbmLib::sFuncGbmBoDestory((*itr)->bo);
                (*itr)->bo = nullptr;
                mExternalBufferList.erase(itr++);
            } else {
                ++itr;
            }
        }
    }

    return C2_OK;
}

} // namespace android

void _UnwrapNativeCodec2GBMMetadata(
        const C2Handle *const handle,
        uint32_t *width, uint32_t *height, uint32_t *format, uint64_t *usage,
        uint32_t *stride, uint32_t *size, uint64_t *bo) {
    (void)android::C2HandleGBM::Import(handle, width, height, format, usage, stride, size, bo);
}

