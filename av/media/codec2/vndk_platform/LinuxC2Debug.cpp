// Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Clear

// Copyright (c) 1999, Google Inc.
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

#include "linux-C2Debug-log.h"

#include <iostream>
#include <sstream>
#include <ostream>
#include <vector>
#include <syslog.h>
#include <stdlib.h>

uint32_t gC2VndkLogLevel = DEBUG_INFO;
static C2DebugLevel sC2DebugLevel;

// enum DEBUG_LEVEL severity to syslog level
static const int SEVERITY_TO_LEVEL[] = { LOG_ERR, LOG_WARNING, LOG_NOTICE, LOG_NOTICE, LOG_DEBUG };
// corresponding definition is enum DEBUG_LEVEL
static const char SEVERITY_TO_CHAR[] = { 'E', 'W', 'I', 'D', 'V' };

C2DebugLevel::C2DebugLevel() {
  char debugLevel[32] = {0};
  char *str = getenv("C2_VNDK_LOG");
  if (str) {
    snprintf(debugLevel, sizeof(debugLevel), "%s", str);
    gC2VndkLogLevel = strtoul(debugLevel, NULL, 0);
    //std::cout << "C2 vndk debug level " << gC2VndkLogLevel << std::endl;
  }
};

void LogMessage::Flush() {
  if (data_->has_been_flushed_) {
    return;
  }

  data_->num_chars_to_log_ = data_->stream_.pcount();
  data_->num_chars_to_syslog_ =
    data_->num_chars_to_log_ - data_->num_prefix_chars_;

  // Do we need to add a \n to the end of this message?
  bool append_newline =
      (data_->message_text_[data_->num_chars_to_log_-1] != '\n');
  char original_final_char = '\0';

  // If we do need to add a \n, we'll do it by violating the memory of the
  // ostrstream buffer.  This is quick, and we'll make sure to undo our
  // modification before anything else is done with the ostrstream.  It
  // would be preferable not to do things this way, but it seems to be
  // the best way to deal with this.
  if (append_newline) {
    original_final_char = data_->message_text_[data_->num_chars_to_log_];
    data_->message_text_[data_->num_chars_to_log_++] = '\n';
  }
  data_->message_text_[data_->num_chars_to_log_] = '\0';

  {
    std::lock_guard<std::mutex> log_lock(mutex_);
    SendToSyslog();
  }

  if (append_newline) {
    // Fix the ostrstream back how it was before we screwed with it.
    // It's 99.44% certain that we don't need to worry about doing this.
    data_->message_text_[data_->num_chars_to_log_-1] = original_final_char;
  }

  // Note that this message is now safely logged.  If we're asked to flush
  // again, as a result of destruction, say, we'll do nothing on future calls.
  data_->has_been_flushed_ = true;
}

void LogMessage::SendToSyslog() {
  if (gC2VndkLogLevel >= severity_) {
    syslog(LOG_USER | SEVERITY_TO_LEVEL[severity_], "%c %s:%d] %.*s",
        SEVERITY_TO_CHAR[severity_], file_, line_,
        int(data_->num_chars_to_syslog_),
        data_->message_text_ + data_->num_prefix_chars_);
  }
}

LogMessage::LogMessage(const char* file, int line, int severity)
  : file_(file),
    line_(line),
    severity_(severity)
{
    data_ = new LogMessageData();
}

LogMessage::~LogMessage() {
    Flush();
    delete data_;
}

LogMessage::LogMessageData::LogMessageData()
  : stream_(message_text_, kMaxLogMessageLen),
    num_prefix_chars_(0),
    num_chars_to_log_(0),
    num_chars_to_syslog_(0),
    has_been_flushed_(false) {
}

std::ostream& LogMessage::stream() {
    return data_->stream_;
}


static inline void
__c2_vndk_log_write(int severity, const char *fmt, va_list ap)
{
    char buf[1024];
    int level = SEVERITY_TO_LEVEL[severity];
    char s = SEVERITY_TO_CHAR[severity];

    vsnprintf(buf, sizeof(buf), fmt, ap);
    syslog(level, "%c %s", s, buf);
}

void __c2_vndk_log(int severity, const char *fmt, ...)
{
    if (severity > gC2VndkLogLevel)
        return;

    va_list ap;
    va_start(ap, fmt);
    __c2_vndk_log_write(severity, fmt, ap);
    va_end(ap);
}
