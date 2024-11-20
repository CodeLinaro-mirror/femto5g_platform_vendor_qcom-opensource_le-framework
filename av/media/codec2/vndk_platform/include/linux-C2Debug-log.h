/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Copyright (c) 2022, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Changes from Qualcomm Innovation Center are provided under the following license:
// Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <iostream>
#include <mutex>

#ifndef C2_LINUX_DEBUG_LOG_H_
#define C2_LINUX_DEBUG_LOG_H_

// Placeholder stringprintf stub
// implent the correct function
#define StringPrintf(fmt, ...) " "

const static size_t kMaxLogMessageLen = 255;

class LogMessage {
    public:
        LogMessage(const char* file, int line, int severity);
        ~LogMessage();
        void Flush();
        void SendToSyslog();

        class LogStreamBuf : public std::streambuf {
            public:
                // REQUIREMENTS: "len" must be >= 2 to account for the '\n' and '\0'.
                LogStreamBuf(char *buf, int len) {
                    setp(buf, buf + len - 2);
                }

                // This effectively ignores overflow.
                int_type overflow(int_type ch) {
                    return ch;
                }
                // Legacy public ostrstream method.
                size_t pcount() const { return static_cast<size_t>(pptr() - pbase()); }
                char* pbase() const { return std::streambuf::pbase(); }
        };

        std::ostream& stream();

        class LogStream : public std::ostream {
            public:
                LogStream(char *buf, int len)
                    : std::ostream(NULL),
                      streambuf_(buf, len) {
                        rdbuf(&streambuf_);
                    }

                // Legacy std::streambuf methods.
                size_t pcount() const { return streambuf_.pcount(); }
                char* pbase() const { return streambuf_.pbase(); }
                char* str() const { return pbase(); }

            private:
                LogStream(const LogStream&);
                LogStream& operator=(const LogStream&);
                LogStreamBuf streambuf_;
        };

        class LogMessageData  {
            public:
                LogMessageData();

                // Buffer space; contains complete message text.
                char message_text_[kMaxLogMessageLen+1];
                LogStream stream_;
                size_t num_prefix_chars_;     // # of chars of prefix in this message
                size_t num_chars_to_log_;     // # of chars of msg to send to log
                size_t num_chars_to_syslog_;  // # of chars of msg to send to syslog
                bool has_been_flushed_;       // false => data has not been flushed

            private:
                LogMessageData(const LogMessageData&);
                void operator=(const LogMessageData&);
        };

    private:
        LogMessageData* data_;
        LogMessage(const LogMessage&);
        void operator=(const LogMessage&);
        const char* file_;         // file name where logging call is
        int line_;                 // line number where logging call is.
        int severity_;
        std::mutex mutex_;
};

#define C2_CHECK
#define C2_CHECK_LT
#define C2_CHECK_LE
#define C2_CHECK_EQ
#define C2_CHECK_GE
#define C2_CHECK_GT
#define C2_CHECK_NE

#define C2_DCHECK
#define C2_DCHECK_LT
#define C2_DCHECK_LE
#define C2_DCHECK_EQ
#define C2_DCHECK_GE
#define C2_DCHECK_GT
#define C2_DCHECK_NE

enum DEBUG_LEVEL {
  DEBUG_ERROR = 0,
  DEBUG_WARNING,
  DEBUG_INFO,
  DEBUG_DEBUG,
  DEBUG_VERBOSE
};

#ifndef __FILENAME__
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define SYSLOG_ERROR()      LogMessage(__FILENAME__, __LINE__, DEBUG_ERROR)
#define SYSLOG_WARNING()    LogMessage(__FILENAME__, __LINE__, DEBUG_WARNING)
#define SYSLOG_INFO()       LogMessage(__FILENAME__, __LINE__, DEBUG_INFO)
#define SYSLOG_DEBUG()      LogMessage(__FILENAME__, __LINE__, DEBUG_DEBUG)
#define SYSLOG_VERBOSE()    LogMessage(__FILENAME__, __LINE__, DEBUG_VERBOSE)
#define SYSLOG_FATAL()      SYSLOG_ERROR()
#define SYSLOG(severity)    SYSLOG_ ## severity().stream()

#define C2_LOG(LEVEL) SYSLOG(LEVEL)

class C2DebugLevel {
    public:
        C2DebugLevel();
};


/* undef Android log macros, use defines below instead. */
#undef ALOGV
#undef ALOGD
#undef ALOGI
#undef ALOGW
#undef ALOGE

#ifndef LOG_NDEBUG
#define LOG_NDEBUG 1
#endif

extern void __c2_vndk_log(int level, const char *fmt, ...);

#define __C2_LOG(level, format, args...) \
    __c2_vndk_log(level, LOG_TAG ":%s:%d: " format "\n", __func__, __LINE__, ##args)

/* C style debug logging macros for codec2 vndk. */
#if LOG_NDEBUG
#define ALOGV(fmt, args...)
#else
#define ALOGV(fmt, args...) __C2_LOG(DEBUG_VERBOSE, fmt, ##args)
#endif
#define ALOGD(fmt, args...) __C2_LOG(DEBUG_DEBUG, fmt, ##args)
#define ALOGI(fmt, args...) __C2_LOG(DEBUG_INFO, fmt, ##args)
#define ALOGW(fmt, args...) __C2_LOG(DEBUG_WARNING, fmt, ##args)
#define ALOGE(fmt, args...) __C2_LOG(DEBUG_ERROR, fmt, ##args)

void updateLogLevel();

#endif  // C2_HOST_DEBUG_LOG_H_

