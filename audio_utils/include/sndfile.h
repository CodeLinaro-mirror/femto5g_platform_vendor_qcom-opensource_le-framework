/*
 * Copyright (C) 2012 The Android Open Source Project
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

/*
** Changes from Qualcomm Innovation Center are provided under the following license:
** Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted (subject to the limitations in the
** disclaimer below) provided that the following conditions are met:
**
**    * Redistributions of source code must retain the above copyright
**      notice, this list of conditions and the following disclaimer.
**
**    * Redistributions in binary form must reproduce the above
**      copyright notice, this list of conditions and the following
**      disclaimer in the documentation and/or other materials provided
**      with the distribution.
**
**    * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
**      contributors may be used to endorse or promote products derived
**      from this software without specific prior written permission.
**
** NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
** GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
** HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
** WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
** ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
** GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
** INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
** IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
** OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
** IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/

#ifndef __AUDIO_UTIL_SNDFILE_H
#define __AUDIO_UTIL_SNDFILE_H

// This is a C library for reading and writing PCM .wav files.  It is
// influenced by other libraries such as libsndfile and audiofile, except is
// much smaller and has an Apache 2.0 license.
// The API should be familiar to clients of similar libraries, but there is
// no guarantee that it will stay exactly source-code compatible with other libraries.

#include <stdio.h>
#ifndef USE_MUSL
#include <sys/cdefs.h>
#endif

#ifdef USE_MUSL
#ifdef __cplusplus
extern "C" {
#endif
#else
__BEGIN_DECLS
#endif

// visible to clients
typedef int sf_count_t;

typedef struct {
    sf_count_t frames;
    int samplerate;
    int channels;
    int format;
} SF_INFO;

// opaque to clients
typedef struct SNDFILE_ SNDFILE;

// Access modes
#define SFM_READ    1
#define SFM_WRITE   2

// Format
#define SF_FORMAT_TYPEMASK  1
#define SF_FORMAT_WAV       1
#define SF_FORMAT_SUBMASK   14
#define SF_FORMAT_PCM_16    2
#define SF_FORMAT_PCM_U8    4
#define SF_FORMAT_FLOAT     6
#define SF_FORMAT_PCM_32    8
#define SF_FORMAT_PCM_24    10

// Open stream
SNDFILE *sf_open(const char *path, int mode, SF_INFO *info);

// Close stream
void sf_close(SNDFILE *handle);

// Read interleaved frames and return actual number of frames read
sf_count_t sf_readf_short(SNDFILE *handle, short *ptr, sf_count_t desired);
sf_count_t sf_readf_float(SNDFILE *handle, float *ptr, sf_count_t desired);
sf_count_t sf_readf_int(SNDFILE *handle, int *ptr, sf_count_t desired);

// Write interleaved frames and return actual number of frames written
sf_count_t sf_writef_short(SNDFILE *handle, const short *ptr, sf_count_t desired);
sf_count_t sf_writef_float(SNDFILE *handle, const float *ptr, sf_count_t desired);
sf_count_t sf_writef_int(SNDFILE *handle, const int *ptr, sf_count_t desired);

#ifdef USE_MUSL
#ifdef __cplusplus
}
#endif
#else
__END_DECLS
#endif

#endif /* __AUDIO_UTIL_SNDFILE_H */
