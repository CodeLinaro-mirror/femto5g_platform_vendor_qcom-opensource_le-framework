/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ANDROID_AUDIO_FORMAT_H
#define ANDROID_AUDIO_FORMAT_H

#include <stdint.h>
#ifndef USE_MUSL
#include <sys/cdefs.h>
#endif
#include <system/audio.h>

#ifdef USE_MUSL
#ifdef __cplusplus
extern "C" {
#endif
#else
__BEGIN_DECLS
#endif

/* Copy buffers with conversion between buffer sample formats.
 *
 *  dst        Destination buffer
 *  dst_format Destination buffer format
 *  src        Source buffer
 *  src_format Source buffer format
 *  count      Number of samples to copy
 *
 * Allowed format conversions are given by either case 1 or 2 below:
 *
 * 1) One of src_format or dst_format is AUDIO_FORMAT_PCM_16_BIT or
 * AUDIO_FORMAT_PCM_FLOAT, and the other format type is one of:
 *
 * AUDIO_FORMAT_PCM_16_BIT
 * AUDIO_FORMAT_PCM_FLOAT
 * AUDIO_FORMAT_PCM_8_BIT
 * AUDIO_FORMAT_PCM_24_BIT_PACKED
 * AUDIO_FORMAT_PCM_32_BIT
 * AUDIO_FORMAT_PCM_8_24_BIT
 *
 * 2) Both dst_format and src_format are identical and of the list given
 * in (1). This is a straight copy.
 *
 * The destination and source buffers must be completely separate if the destination
 * format size is larger than the source format size. These routines call functions
 * in primitives.h, so descriptions of detailed behavior can be reviewed there.
 *
 * Logs a fatal error if dst or src format is not allowed by the conversion rules above.
 */
void memcpy_by_audio_format(void *dst, audio_format_t dst_format,
        const void *src, audio_format_t src_format, size_t count);


/* This function creates an index array for converting audio data with different
 * channel position and index masks, used by memcpy_by_index_array().
 * Returns the number of array elements required.
 * This may be greater than idxcount, so the return value should be checked
 * if idxary size is less than 32. Returns zero if the input masks are unrecognized.
 *
 * Note that idxary is a caller allocated array
 * of at least as many channels as present in the dst_mask.
 *
 * Parameters:
 *  idxary      Updated array of indices of channels in the src frame for the dst frame
 *  idxcount    Number of caller allocated elements in idxary
 *  dst_mask    Bit mask corresponding to destination channels present
 *  src_mask    Bit mask corresponding to source channels present
 */
size_t memcpy_by_index_array_initialization_from_channel_mask(int8_t *idxary, size_t arysize,
        audio_channel_mask_t dst_channel_mask, audio_channel_mask_t src_channel_mask);

#ifdef USE_MUSL
#ifdef __cplusplus
}
#endif
#else
__END_DECLS
#endif

#endif  // ANDROID_AUDIO_FORMAT_H
