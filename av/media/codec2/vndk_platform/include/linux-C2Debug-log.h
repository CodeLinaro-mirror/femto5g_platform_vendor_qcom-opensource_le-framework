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

#include <iostream>

#ifndef C2_LINUX_DEBUG_LOG_H_
#define C2_LINUX_DEBUG_LOG_H_

// Placeholder stringprintf stub
// implent the correct function
#define StringPrintf(fmt, ...) " "

// Placholder logging header

class hostLogger {
};

/*
 * Logging and debug macros.
 */
enum logLevel {
	VERBOSE,
	DEBUG,
	INFO,
	WARNING,
	ERROR,
	FATAL_WITHOUT_ABORT,
	FATAL,
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

// TODO implment LV logger
#define LOG_LEVEL(level) std::cout.setstate(std::ios::failbit) ;\
				 std::cout
#define LOG(LEVEL) LOG_LEVEL(logLevel::LEVEL)
#define C2_LOG(LEVEL) LOG_LEVEL(LEVEL)

#endif  // C2_HOST_DEBUG_LOG_H_

