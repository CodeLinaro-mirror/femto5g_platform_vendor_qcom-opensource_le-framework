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
 Changes from Qualcomm Innovation Center, Inc. are provided under the following license:

 Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.

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
#include <C2Debug.h>
#include <C2Buffer.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <cinttypes>
#include <dlfcn.h>

using namespace std::chrono_literals;


#define DEFAULT_POOL_SIZE 6
#define DEFAULT_EXTEND_POOL_SIZE 3
#define MAX_WAIT_BUF_TIME 100ms
#define BASE_WAIT_BUF_TIME 34ms
#define INVALID_FD (-1)


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
        ALOGI ("loaded libgbm.so");                                 \
    } while (0)

#define UNLOAD_LIBGBM()                   \
    do {                                  \
        if (sGbmLib) {                    \
            dlclose(sGbmLib);             \
            ALOGI ("unloaded libgbm.so"); \
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
        ALOGI ("loaded symbol %s", #sym);                           \
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

#define _print_buf_entry(level, str, entry, pool)                            \
do {                                                                         \
    if (entry != nullptr) {                                                  \
        __C2_LOG(level, "%s entry:%p bo:%p used:%d fd:%u meta_fd:%u"         \
            " format:0x%x wxh:%ux%u expired:%u pool:%p\n",                   \
            str, (entry).get(), (entry)->bo, (entry)->used, (entry)->bo_fd,  \
            (entry)->meta_fd, (uint32_t)((entry)->res_fmt_id >> 32),         \
            (uint32_t)(((entry)->res_fmt_id >> 16) & 0xFFFF),                \
            (uint32_t)((entry)->res_fmt_id & 0xFFFF), (entry)->expired,      \
            (pool));                                                         \
   }                                                                         \
} while (0)

#define _print_buf_entry_i(str, entry, pool) \
    _print_buf_entry(DEBUG_INFO, str, entry, pool)

#define _print_buf_entry_d(str, entry, pool) \
    _print_buf_entry(DEBUG_DEBUG, str, entry, pool)


static inline void close_fd(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}

// GPU doesn't support those formats below as rendering target.
// 1. GBM_FORMAT_P010
// 2. GBM_FORMAT_YCbCr_420_TP10_UBWC
// 3. GBM_FORMAT_RGB888
static inline uint32_t getBoRenderUsage(uint32_t format)
{
    uint32_t gbmUsage = GBM_BO_USE_SCANOUT;

    switch (format) {
        case GBM_FORMAT_P010:
        case GBM_FORMAT_YCbCr_420_TP10_UBWC:
        case GBM_FORMAT_RGB888:
            break;
        case GBM_FORMAT_NV12:
            gbmUsage |= GBM_BO_USE_RENDERING;
            break;
        default:
            gbmUsage |= GBM_BO_USE_RENDERING;
            break;
    }

    return gbmUsage;
}

/* Helper function to return real GBM usages after video private usages get cleared. */
static inline uint32_t getRealGBMUsage(C2MemoryUsageGBM &usages) {
    uint32_t gbmUsages = usages.gbmUsage();
    gbmUsages &= ~(GBM_BO_PRIVATE_USAGE_NV12_512_QTI | GBM_BO_PRIVATE_USAGE_C2D_OUTPUT_BUF);
    return gbmUsages;
}

class BufferEntryInfo {
public:
    BufferEntryInfo (bool used, uint64_t res_fmt_id, struct gbm_bo *bo, int32_t bo_fd, int32_t meta_fd, int32_t ext_fd, int32_t idx);

    bool used;
    uint64_t res_fmt_id; // id contains resolution and pixel format
    struct gbm_bo *bo;
    int32_t bo_fd;
    int32_t meta_fd;
    int32_t ext_fd;
    bool expired;
    int32_t idx; // identify the external buf transfering through binder
};

class BufferPool {
public:
    BufferPool();
    c2_status_t acquireBuffer(std::shared_ptr<BufferEntryInfo> &entry, uint32_t width, uint32_t height, uint32_t format);
    c2_status_t setMaxBufferCount(uint32_t size);
    uint32_t    getMaxBufferCount(void) { return mMaxBufferCount; }
    c2_status_t releaseBuffer(std::shared_ptr<BufferEntryInfo> entry);

    std::list<std::shared_ptr<BufferEntryInfo> > mBufferList; // may include old and new buffer during port reconfig
private:
    uint32_t getBufferCountOfCurResFmt(); // get buffer count of current resolution and pixel format
    uint64_t mCurResFmtId; // current id contain resolution and pixel format
    uint32_t mMaxBufferCount; // max buffer count set by API setMaxBufferCount
    uint32_t mExtBufferCount; // extended buffer count for time out
    uint32_t mTotalBufferCount; // sum of max buffer count and extended buffer count
    std::mutex mBufMutex;   //  mutex for the buffer lists
    std::condition_variable mBufCond;
    bool mBufSignaled; // avoid spurious wakeup
    std::chrono::milliseconds mWaitTime = BASE_WAIT_BUF_TIME;
    uint32_t mWaitTimeFactor = 1;
};

// Caller needs to free handle properly when handle is not used any more
static c2_status_t createC2HandleGBM(C2Handle *&handle, std::shared_ptr<BufferEntryInfo> &entry, uint64_t usage,
        uintptr_t func = 0, uintptr_t comp = 0)
{
    c2_status_t ret = C2_OK;
    C2HandleGBM *handleGBM = nullptr;

    if (entry && entry->bo) {
        handleGBM = new C2HandleGBM();
        struct gbm_bo *bo = entry->bo;
        if (handleGBM) {
            handleGBM->version = C2HandleGBM::VERSION;
            handleGBM->numFds = C2HandleGBM::NUM_FDS;
            handleGBM->numInts = C2HandleGBM::NUM_INTS;
            handleGBM->mFds.buffer_fd = entry->bo_fd;
            handleGBM->mFds.meta_buffer_fd = entry->meta_fd;
            handleGBM->mFds.external_fd = entry->ext_fd;

            handleGBM->mInts.width = bo->width;
            handleGBM->mInts.height = bo->height;
            handleGBM->mInts.stride = bo->stride;
            handleGBM->mInts.slice_height = bo->aligned_height;
            handleGBM->mInts.format = bo->format;
            handleGBM->mInts.usage_lo = (uint32_t)usage;
            handleGBM->mInts.usage_hi = (uint32_t)(usage >> 32);
            handleGBM->mInts.size = bo->size;
            //Use fd as the unique buffer id for C2Buffer
            handleGBM->mInts.id = entry->bo_fd;
            handleGBM->mInts.bo_lo = (uint32_t)((uint64_t)bo & 0xFFFFFFFF);
            handleGBM->mInts.bo_hi = (uint32_t)(((uint64_t)bo >> 32) & 0xFFFFFFFF);
            handleGBM->mInts.need_free_ext_buf = 1;
            handleGBM->mInts.idx = entry->idx;
            // save the callback function address
            handleGBM->mInts.func_lo = (uint32_t)((uint64_t)func & 0xFFFFFFFF);
            handleGBM->mInts.func_hi = (uint32_t)(((uint64_t)func >> 32) & 0xFFFFFFFF);
            handleGBM->mInts.comp_lo = (uint32_t)((uint64_t)comp & 0xFFFFFFFF);
            handleGBM->mInts.comp_hi = (uint32_t)(((uint64_t)comp >> 32) & 0xFFFFFFFF);

            ALOGD("GBM handle data: fd:%d meta_fd:%d ext_fd:%d width:%u height:%u format:0x%x "
                    "C2&GBM usage_lo:0x%x usage_hi:0x%x stride:%u slice_height:%u size:%u, bo:0x%" PRIx64,
                    handleGBM->data[0], handleGBM->data[1], handleGBM->data[2], handleGBM->data[3],
                    handleGBM->data[4], handleGBM->data[5], handleGBM->data[6], handleGBM->data[7],
                    handleGBM->data[8], handleGBM->data[9], handleGBM->data[10],
                    (uint64_t(handleGBM->data[14]) << 32) | handleGBM->data[13]);

            handle = handleGBM;
        } else {
            ALOGE("Failed to create GBM C2 handle");
            ret = C2_NO_MEMORY;
        }
    } else {
        ALOGE("invalid entry:%p or bo:%p", entry.get(), entry.get() ? entry->bo : nullptr);
        ret = C2_BAD_VALUE;
    }

    return ret;
}

BufferEntryInfo::BufferEntryInfo(bool used, uint64_t res_fmt_id,
      struct gbm_bo * bo, int32_t bo_fd, int32_t meta_fd, int32_t ext_fd, int32_t idx)
    : used(used), res_fmt_id(res_fmt_id), bo(bo), bo_fd(bo_fd), meta_fd(meta_fd), ext_fd(ext_fd), idx(idx),
      expired(false)
{
}

BufferPool::BufferPool()
    :mMaxBufferCount(DEFAULT_POOL_SIZE),
    mExtBufferCount(0),
    mTotalBufferCount(mMaxBufferCount + mExtBufferCount),
    mBufSignaled(false)
{
    GbmLib::loadGbm();
}

c2_status_t BufferPool::setMaxBufferCount(uint32_t size)
{
    std::lock_guard<std::mutex> listLock(mBufMutex);
    mMaxBufferCount = size > DEFAULT_POOL_SIZE ? size : DEFAULT_POOL_SIZE;
    mExtBufferCount = 0;
    mTotalBufferCount = mMaxBufferCount + mExtBufferCount;
    ALOGI("set max bufferCount of pool:%p: want:%d, result:%d, and reset extend buffer count",
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
    std::unique_lock<std::mutex> listLock(mBufMutex);
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
                _print_buf_entry_i("finalize", *itr, this);
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

    auto bufSignaledFunc = [&]() -> bool {
        bool ret = mBufSignaled == true;
        mBufSignaled = false;

        return ret;
    };

    while (!acquired) {
        itr = mBufferList.begin();
        // Need to allocate new bufer entry since the buffer
        // count is not more than the expected count.
        if (getBufferCountOfCurResFmt() < mTotalBufferCount) {
            createNewEntry = true;
            ALOGI("need to create new entry, pool:%p", this);
            break;
        } else {
            itr = mBufferList.begin();
            while (itr != mBufferList.end()) {
                if ((*itr)->used == false) {
                    (*itr)->used = true;
                    _print_buf_entry_d("acquired", *itr, this);
                    acquired = true;
                    break;
                } else {
                    ++itr;
                }
            }

            if (itr == mBufferList.end()) {
                ALOGD("waiting for buffer, pool:%p", this);
                if (!mBufCond.wait_for(listLock, mWaitTime, bufSignaledFunc)) {
                    ALOGD("wait for buffer time out, wait time %d ms, pool:%p", mWaitTime.count(), this);
                    if (mWaitTime < MAX_WAIT_BUF_TIME) {
                        mWaitTime = std::min(++mWaitTimeFactor * BASE_WAIT_BUF_TIME, MAX_WAIT_BUF_TIME);
                        ALOGD("increase wait time to %d ms", mWaitTime.count());
                    }
                    // increase buffer count to make new entry possible
                    if (mExtBufferCount < DEFAULT_EXTEND_POOL_SIZE) {
                        mTotalBufferCount++;
                        mExtBufferCount++;
                        createNewEntry = true;
                        ALOGI("create a exteneded buf since time out");
                        break;
                    } else {
                        ALOGW("Warning: wait idle buf timeout and extend buf cnt(%d)"
                                " reach limit, pool:%p", mExtBufferCount, this);
                        ret = C2_TIMED_OUT;
                        break;
                    }
                } else {
                    ALOGD("waited for buffer, pool:%p", this);
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

    if (ret == C2_OK) {
        std::lock_guard<std::mutex> listLock(mBufMutex);
        auto itr = mBufferList.begin();

        _print_buf_entry_d("release", entry, this);
        while (itr != mBufferList.end()) {
            if ((*itr).get() == entry.get()) {
                if (entry->res_fmt_id == mCurResFmtId) {
                    entry->used = false;
                    if (getBufferCountOfCurResFmt() > mTotalBufferCount) {
                        _print_buf_entry_i("decrease", *itr, this);
                        close_fd((*itr)->bo_fd);
                        GbmLib::sFuncGbmBoDestory((*itr)->bo);
                        itr = mBufferList.erase(itr);
                        ALOGD("buffer list size:%zu after decreased", mBufferList.size());
                    }
                } else {
                    // destory old buffer once reconfig
                    _print_buf_entry_i("finalize", *itr, this);
                    close_fd((*itr)->bo_fd);
                    GbmLib::sFuncGbmBoDestory((*itr)->bo);
                    itr = mBufferList.erase(itr);
                    ALOGD("release buffer list size:%zu", mBufferList.size());
                }
                found = true;
                break;
            } else {
                ++itr;
            }
        }

        if (!found) {
            mBufferList.push_back(entry);
            ALOGD("add new entry to buffer list, pool:%p", this);
        }
    }

    if (ret == C2_OK) {
        {
            std::lock_guard<std::mutex> listLock(mBufMutex);
            mBufSignaled = true;
        }
        mBufCond.notify_one();
        ALOGD("notify since one buffer released, pool:%p", this);
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
        uint64_t *usage, uint32_t *stride, uint32_t *size,
        uint64_t *bo, uint64_t *func, uint64_t *comp)
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
    if (func)
        *func = xd->func_lo | (uint64_t(xd->func_hi) << 32);
    if (comp)
        *comp = xd->comp_lo | (uint64_t(xd->comp_hi) << 32);

    return reinterpret_cast<const C2HandleGBM *>(handle);
}


C2AllocationGBM::C2AllocationGBM(struct gbm_device *gbm, std::shared_ptr<BufferPool>& pool, uint32_t width,
        uint32_t height, uint32_t format, C2MemoryUsage usage, C2Allocator::id_t allocatorId, C2HandleGBM *handle,
        ReleaseExtBufFunc releaseExtBufFunc, std::shared_ptr<C2AllocatorGBM::ICallback> cb)
    : C2GraphicAllocation(width, height), mHandle(handle), mBufEntryInfo(nullptr), mBase(nullptr),
    mMapSize(0), mPool(pool), mAllocatorId(allocatorId), mRet(C2_OK), mReleaseExtBufFunc(releaseExtBufFunc),
    mIsFromRemote(false), mIsToRemote(false), mCallback(cb)
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
    ALOGD("destroy allocation");
    if (mBufEntryInfo) {
        mPool->releaseBuffer(mBufEntryInfo);
    }

    if (mHandle) {
        // For ext buffer, client side's allocation will be released by downstream component or this dtor.
        // Usually service side's allocation will not be released in this dtor as client might be using it.
        // But, if allocations are dropped by service internally (!mIsToRemote), they should be released from dtor.
        if ((mIsFromRemote || !mIsToRemote) && mHandle->mFds.external_fd > 0 && mHandle->mInts.need_free_ext_buf) {
            if (mCallback) {
                mCallback->onReleaseExtBuf(mHandle->mInts.idx);
            } else if (mReleaseExtBufFunc) {
                // Only external buffer cases can be here. For C2service scenario, only decoder case
                // can be here and mIsFromRemote is true in client side so that we use idx.
                // For non-C2service scenario, mIsFromRemote is always false so that we use external_fd
                mReleaseExtBufFunc(mIsFromRemote ? mHandle->mInts.idx : mHandle->mFds.external_fd);
            }
        }
        // only client side handles the ext buffer release
        // service side will recycle the idx, and allocator will release the fds
        if (mIsFromRemote) {
            uint64_t bo = mHandle->mInts.bo_lo | (uint64_t(mHandle->mInts.bo_hi) << 32);
            GbmLib::sFuncGbmBoDestory ((struct gbm_bo *)bo);
            native_handle_close(mHandle);
            native_handle_delete(mHandle); // created in readC2Handle()
            ALOGD("close remote fds");
        } else {
            delete mHandle;
        }

        mHandle = nullptr;
    }
}

c2_status_t C2AllocationGBM::Alloc(struct gbm_device *gbm, uint32_t w, uint32_t h, uint32_t format, C2MemoryUsage usage)
{
    uint32_t gbmUsages = 0;
    struct gbm_bo *bo = NULL;
    int32_t bo_fd = INVALID_FD, meta_fd = INVALID_FD;
    uint64_t res_fmt_id = (uint64_t)format << 32 | w << 16 | h;
    c2_status_t ret = C2_OK;
    C2MemoryUsageGBM c2GbmUsage(usage);

    gbmUsages = c2GbmUsage.gbmUsage() | getBoRenderUsage(format);
    c2GbmUsage = C2MemoryUsageGBM(C2MemoryUsage(c2GbmUsage.c2Usage()), gbmUsages);

    ret = mPool->acquireBuffer(mBufEntryInfo, w, h, format);

    if (ret != C2_OK) {
        ALOGW("acquire buffer time out");
    } else {
        if (!mBufEntryInfo) {
            gbmUsages = getRealGBMUsage(c2GbmUsage);
            ALOGI("GBM will create bo wxh:%ux%u format:0x%x usage:0x%x", w, h, format, gbmUsages);
            bo = GbmLib::sFuncGbmBoCreate(gbm, w, h, format, gbmUsages);
            ALOGI("GBM bo wxh:%ux%u format:0x%x usage:0x%x, ret bo %p", w, h, format, gbmUsages, bo);
            if (bo == NULL) {
                ALOGE("Failed to create GBM bo for format 0x%x, width-height:%dx%d, GBM usages:0x%x",
                        format, w, h, gbmUsages);
                ret = C2_BAD_VALUE;
            } else {
                bo_fd = GbmLib::sFuncGbmBoGetFd(bo);
                ALOGI("Newly created gbm bo=%p fd=%d", bo, bo_fd);
                if (bo_fd < 0) {
                    ALOGE("Get bo fd failed");
                    GbmLib::sFuncGbmBoDestory(bo);
                    ret = C2_BAD_VALUE;
                } else {
                    GbmLib::sFuncGbmPerform(GBM_PERFORM_GET_METADATA_ION_FD, bo, &meta_fd);
                    if (meta_fd < 0) {
                        ALOGE("Get bo meta fd failed");
                        close_fd(bo_fd);
                        GbmLib::sFuncGbmBoDestory(bo);
                        ret = C2_BAD_VALUE;
                    } else {
                        mBufEntryInfo = std::make_shared<BufferEntryInfo>(true, res_fmt_id, bo, bo_fd, meta_fd, INVALID_FD, -1);
                        _print_buf_entry_i("new", mBufEntryInfo, mPool.get());
                        // add new entry to buffer list
                        mPool->releaseBuffer(mBufEntryInfo);
                    }
                }
            }
        }

        if (ret == C2_OK) {
            C2Handle* handle = nullptr;
            ALOGD("input usage:0x%" PRIx64 ", C2MemoryUsageGBM usage:0x%" PRIx64,
                usage.expected, c2GbmUsage.expected);
            ret = createC2HandleGBM(handle, mBufEntryInfo, c2GbmUsage.expected);
            if (ret == C2_OK) {
                mHandle = static_cast<C2HandleGBM*>(handle);
            } else {
                ALOGE("Failed to create C2 handle from gbm bo");
            }
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
        ALOGE("failed to mmap() gbm fd, errno = %d, fd %d, sz %d", errno, fd, size);
        return C2_BAD_VALUE;
    }

    *addr = (uint8_t*) mBase;
    ALOGD("mapping gbm fd: %d, size: %d addr:%p", fd, size, *addr);
    mMapSize = size;

    return C2_OK;
}

c2_status_t C2AllocationGBM::unmap(uint8_t **addr, C2Rect rect, C2Fence *fence)
{
    int ret = munmap(mBase, mMapSize);
    ALOGD("unmap gbm buffer addr:%p size:%zu", mBase, mMapSize);

    if (ret) {
        ALOGE("failed to munmap() gbm fd, errno = %d, addr %p, sz %zu", errno, mBase, mMapSize);
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
    mDevice_fd(INVALID_FD)
{
    GbmLib::loadGbm();
    C2MemoryUsage minUsage = { 0, 0 };
    C2MemoryUsage maxUsage = { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE };
    Traits traits = { "linux.allocator.gbm", id, GRAPHIC, minUsage, maxUsage };
    mTraits = std::make_shared<Traits>(traits);

    mDevice_fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
    ALOGI("C2AllocatorGBM constructor(%u) open gbm dev node, ret fd %d", (unsigned int)id, mDevice_fd);
    if (mDevice_fd < 0) {
        int e = errno;
        ALOGE("opening dri device for gbm failed, errno %d(%s)", e, strerror(e));
    } else {
        mGBM = GbmLib::sFuncGbmCreateDevice(mDevice_fd);
        if (mGBM == NULL) {
            ALOGE("create gbm device failed with fd: %d", mDevice_fd);
        } else {
            mInit = C2_OK;
            ALOGI("Successfully created gbm device: %p", mGBM);
        }
    }
}

C2AllocatorGBM::~C2AllocatorGBM()
{
    ALOGI("destroy C2AllocatorGBM !");
    for (auto const& i: mPool->mBufferList) {
        if (i->bo) {
            close_fd(i->bo_fd);
            GbmLib::sFuncGbmBoDestory (i->bo);
            _print_buf_entry_i("finalize", i, mPool.get());
        }
    }
    mPool->mBufferList.clear();

    for (auto const& i: mExternalBufferList) {
        if (i->bo) {
            close_fd(i->bo_fd);
#ifdef USE_AGL_C2SERVICE
            // In non-c2service scenario, ext_fd is closed by the owner of ext buf
            close_fd(i->ext_fd);
#endif
            GbmLib::sFuncGbmBoDestory (i->bo);
            _print_buf_entry_i("ext gbm finalize", i, nullptr);
        }
    }
    mExternalBufferList.clear();

    if (mGBM) {
        ALOGI("Destroy gbm device: %p", mGBM);
        GbmLib::sFuncGbmDeviceDestroy(mGBM);
        mGBM = NULL;
    }

    if (mDevice_fd > 0) {
        ALOGI("C2AllocatorGBM destructor close gbm dev node fd %d", mDevice_fd);
        close(mDevice_fd);
        mDevice_fd = INVALID_FD;
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

c2_status_t C2AllocatorGBM::newGraphicAllocation(uint32_t width, uint32_t height,
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
        (mGBM, mPool, width, height, format, usage, mTraits->id);
    if (alloc) {
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
    uint64_t usage = 0;
    uint32_t stride = 0;
    uint32_t size = 0;
    uint64_t bo = 0;

    if (mInit != C2_OK) {
        ALOGE("GBM device is not created, unexpected");
        return C2_NO_INIT;
    }

    C2HandleGBM *gbmHandle = const_cast<C2HandleGBM*>(C2HandleGBM::Import(
        handle, &width, &height, &format, &usage, &stride, &size, &bo));

    C2MemoryUsage usages(usage);

    if (gbmHandle != nullptr) {
        allocation->reset(new C2AllocationGBM(mGBM, mPool, width, height, format, usages,
                                              mTraits->id, gbmHandle, mReleaseExtBufFunc, mCallback));
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
    ALOGI("set c2 allocator max buffer count:%u", size);

    return C2_OK;
}

uint32_t C2AllocatorGBM::getMaxAllocationCount(void) {
    uint32_t count = mPool->getMaxBufferCount();
    ALOGI("get c2 allocator max buffer count:%u", count);

    return count;
}

c2_status_t C2AllocatorGBM::setUseExternalBuffer(bool useExternal)
{
    mUseExternalBuffer = useExternal;
    ALOGI("Set to use external buffer: %s", useExternal ? "True" : "False");

    return C2_OK;
}

bool C2AllocatorGBM::isUseExternalBuffer()
{
    ALOGD("If use external buffer: %s", mUseExternalBuffer ? "True" : "False");

    return mUseExternalBuffer;
}

c2_status_t C2AllocatorGBM::attachExternalFd(int extFd) {
    return attachExternalFd(extFd, extFd);
}

c2_status_t C2AllocatorGBM::attachExternalFd(int extFd, int idx)
{
#ifdef USE_AGL_C2SERVICE
    std::lock_guard<std::mutex> lk(mExtBufLock);
#endif

    bool found = false;

    if (extFd <= 0) {
        ALOGE("Invalid external buffer fd: %d !", extFd);
        return C2_BAD_VALUE;
    }

    auto itr = mExternalBufferList.begin();
    while (itr != mExternalBufferList.end()) {
#ifndef USE_AGL_C2SERVICE
        if ((*itr)->ext_fd == extFd && !(*itr)->expired) {
#else
        if ((*itr)->idx == idx && !(*itr)->expired) {
#endif
            (*itr)->used = false;
            found = true;
#ifdef USE_AGL_C2SERVICE
            // close the extFd, use the cached fd instead
            close(extFd);
#endif
            break;
        } else {
            ++itr;
        }
    }
    if (!found) {
        std::shared_ptr<BufferEntryInfo> buffer =
            std::make_shared<BufferEntryInfo>(false, 0, nullptr, INVALID_FD, INVALID_FD, extFd, idx);
        mExternalBufferList.push_back(buffer);
        ALOGD("Add new external entry to buffer list with external fd=%d", extFd);
    }

    return C2_OK;
}

// Rebuild C2AllocationGBM of GBM buffer from remote in C2 service & client for
// decoding & encoding cases. In decoding case, service sends buffer to client
// to rebuild allocation and unflat C2Buffer, and vice versa in encoding case.
c2_status_t C2AllocatorGBM::rebuildAllocationGBM(
        C2Handle *handle, std::shared_ptr<C2GraphicAllocation> *allocation)
{
    c2_status_t ret = C2_BAD_VALUE;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;
    uint64_t usage = 0;
    uint32_t stride = 0;
    uint32_t size = 0;
    uint64_t bo = 0;
    uint64_t func = 0;
    uint64_t comp = 0;

    if (mInit != C2_OK) {
        ALOGE("GBM device is not created, unexpected");
        return C2_NO_INIT;
    }

    C2HandleGBM *gbmHandle = const_cast<C2HandleGBM*>(C2HandleGBM::Import(
        handle, &width, &height, &format, &usage, &stride, &size, &bo, &func, &comp));
    if (gbmHandle == nullptr) {
        ALOGE("Failed to import C2HandleGBM for handle=%p", handle);
        return ret;
    }
    C2MemoryUsageGBM usages(usage);

    ALOGD("GBM handle: fd:%d meta_fd:%d ext_fd:%d width:%u height:%u "
        "format:%x stride:%u slice_height:%u size:%u usage64:0x%" PRIx64 " bo:0x%" PRIx64
        " func:0x%" PRIx64 " comp:0x%" PRIx64,
        gbmHandle->mFds.buffer_fd, gbmHandle->mFds.meta_buffer_fd, gbmHandle->mFds.external_fd,
        width, height, format, stride, gbmHandle->mInts.slice_height, size, usage, bo, func, comp);

    struct gbm_buf_info buf_info;
    buf_info.fd = handle->data[0];
    buf_info.metadata_fd = handle->data[1];
    buf_info.width = width;
    buf_info.height = height;
    buf_info.format = format;
    buf_info.is_external_fd = 0;

    struct gbm_bo *gbmBo;
    uint32_t gbmUsages = getRealGBMUsage(usages);
    gbmBo = GbmLib::sFuncGbmBoImport(mGBM, GBM_BO_IMPORT_GBM_BUF_TYPE, &buf_info, gbmUsages);
    if (gbmBo == nullptr) {
        ALOGE("Failed to import gbm bo for fd=%d meta fd=%d", buf_info.fd, buf_info.metadata_fd);
        return ret;
    }

    gbmHandle->mInts.bo_lo = (uint32_t)((uint64_t)gbmBo & 0xFFFFFFFF);
    gbmHandle->mInts.bo_hi = (uint32_t)(((uint64_t)gbmBo >> 32) & 0xFFFFFFFF);
    ALOGD("GBM imported bo:0x%" PRIx64, (uint64_t)gbmBo);

    ReleaseExtBufFunc releaseFunc = nullptr;
    if ((0 != func) && (0 != comp)) {
        using ReleaseCb = void (*) (void*, int32_t);
        releaseFunc = std::bind((ReleaseCb)func, (void*)comp, std::placeholders::_1);
    }

    auto alloc = new C2AllocationGBM(mGBM, mPool, width, height, format, usages,
                                     mTraits->id, gbmHandle, releaseFunc);
    if (alloc == nullptr) {
        ALOGE("Failed to new C2AllocationGBM");
        return ret;
    } else {
        alloc->fromRemote();
        allocation->reset(alloc);
    }

    return C2_OK;
}


c2_status_t C2AllocatorGBM::setCallback(std::shared_ptr<ICallback> cb)
{
    ALOGI("%s mUseExternalBuffer:%s", __func__, mUseExternalBuffer ? "YES" : "NO");
    mCallback = cb;

    return C2_OK;
}

c2_status_t C2AllocatorGBM::passReleaseExtBufCb(uintptr_t func, uintptr_t comp) {
    ALOGI("passReleaseExtBufCb func:0x%" PRIx64 " comp:0x%" PRIx64, func, comp);
    mReleaseExtFunc = func;
    mComponent = comp;

    return C2_OK;
}

c2_status_t C2AllocatorGBM::createC2HandleOfExtBuf(C2Handle *&handle,
                              uint32_t width, uint32_t height,
                              uint32_t format, C2MemoryUsage usage)
{
    uint32_t gbmUsages = 0;
    bool acquired = false;
    c2_status_t ret = C2_OK;
    struct gbm_bo *gbmBo = NULL;
    auto itr = mExternalBufferList.begin();
    C2MemoryUsageGBM c2GbmUsage(usage);

    gbmUsages = c2GbmUsage.gbmUsage() | getBoRenderUsage(format);

    while (itr != mExternalBufferList.end()) {
        if ((*itr)->used == false) {
            (*itr)->used = true;
            acquired = true;
            break;
        } else {
            ++itr;
        }
    }

    if (acquired) {
        if (!(*itr)->bo) {
            int32_t bo_fd = INVALID_FD, meta_fd = INVALID_FD;
            struct gbm_import_fd_data bufData;
            bufData.fd = (*itr)->ext_fd;
            bufData.width = width;
            bufData.height = height;
            bufData.format = format;

            gbmUsages = getRealGBMUsage(c2GbmUsage);
            gbmBo = GbmLib::sFuncGbmBoImport(mGBM, GBM_BO_IMPORT_FD, &bufData, gbmUsages);
            if (gbmBo) {
                bo_fd = GbmLib::sFuncGbmBoGetFd(gbmBo);
                ALOGI("Newly imported gbm bo=%p bo_fd=%d from ext_fd=%d, idx=%d", gbmBo, bo_fd, (*itr)->ext_fd, (*itr)->idx);
                if (bo_fd < 0) {
                    ALOGE("Failed to get imported bo(%p, ext_fd %d)'s fd(%d)", gbmBo, (*itr)->ext_fd, bo_fd);
                    GbmLib::sFuncGbmBoDestory(gbmBo);
                    ret = C2_BAD_VALUE;
                } else {
                    if (mCallback) { // ext-buf + c2service senario
                        (*itr)->bo = gbmBo;
                        (*itr)->bo_fd = bo_fd;
                        (*itr)->meta_fd = meta_fd;
                    } else {
                        GbmLib::sFuncGbmPerform(GBM_PERFORM_GET_METADATA_ION_FD, gbmBo, &meta_fd);
                        if (meta_fd < 0) {
                            ALOGE("Failed to get imported bo(%p, bo_fd %d, ext_fd %d)'s meta fd(%d)",
                                gbmBo, bo_fd, (*itr)->ext_fd, meta_fd);
                        }
                        (*itr)->bo = gbmBo;
                        (*itr)->bo_fd = bo_fd;
                        (*itr)->meta_fd = meta_fd;
                    }
                }
            } else {
                ALOGE("Failed to import gbm bo for bufData.fd=%d ext_fd=%d, idx=%d", bufData.fd, (*itr)->ext_fd, (*itr)->idx);
                ret = C2_BAD_VALUE;
            }
        } else {
            gbmBo = (*itr)->bo;
        }

        if (ret == C2_OK) {
            ret = createC2HandleGBM(handle, *itr, usage.expected, mReleaseExtFunc, mComponent);
            if (ret != C2_OK) {
                ALOGE("Failed to create C2 handle from gbm bo");
            }
        } else {
            (*itr)->used = false;
        }
    } else {
        ALOGE("Failed to acquire available external buffer");
        ret = C2_TIMED_OUT;
    }

    return ret;
}

c2_status_t C2AllocatorGBM::setAcquireExtBufCb(const AcquireExtBufFunc cb)
{
    mAcquireExtBufFunc = cb;

    return C2_OK;
}

c2_status_t C2AllocatorGBM::setReleaseExtBufCb(const ReleaseExtBufFunc cb)
{
    mReleaseExtBufFunc = cb;

    return C2_OK;
}

c2_status_t C2AllocatorGBM::acquireExtBuffer(uint32_t width, uint32_t height, bool isC2D)
{
    bool resolution_change = false;
    for (auto const& i: mExternalBufferList) {
        if (i->bo && (i->bo->width != width || i->bo->height != height)) {
            i->expired = true;
            resolution_change = true;
        }
    }

    if (mCallback) {
        mCallback->onAcquireExtBuf(width, height, isC2D);
    } else if (mAcquireExtBufFunc) {
        mAcquireExtBufFunc(width, height, isC2D);
    }

    if (resolution_change) {
        ALOGI("resolution changed to %dx%d with external buffer", width, height);
        auto itr = mExternalBufferList.begin();
        while (itr != mExternalBufferList.end()) {
            if ((*itr)->bo && (*itr)->expired) {
                close_fd((*itr)->bo_fd);
                GbmLib::sFuncGbmBoDestory((*itr)->bo);
                (*itr)->bo = nullptr;
#ifdef USE_AGL_C2SERVICE
                close_fd((*itr)->ext_fd);
#endif
                itr = mExternalBufferList.erase(itr);
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

