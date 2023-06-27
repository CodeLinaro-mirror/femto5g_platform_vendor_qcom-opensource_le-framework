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

#ifndef ANDROID_AUDIO_CHANNELS_H
#define ANDROID_AUDIO_CHANNELS_H

#ifdef USE_MUSL
#ifdef __cplusplus
extern "C" {
#endif
#else
__BEGIN_DECLS
#endif

/*
 * Expands or contracts sample data from one interleaved channel format to another.
 * Expanded channels are filled with zeros and put at the end of each audio frame.
 * Contracted channels are omitted from the end of each audio frame.
 *   in_buff points to the buffer of samples
 *   in_buff_channels Specifies the number of channels in the input buffer.
 *   out_buff points to the buffer to receive converted samples.
 *   out_buff_channels Specifies the number of channels in the output buffer.
 *   sample_size_in_bytes Specifies the number of bytes per sample. 1, 2, 3, 4 are
 *     currently valid.
 *   num_in_bytes size of input buffer in BYTES
 * returns
 *   the number of BYTES of output data or 0 if an error occurs.
 * NOTE
 *   The out and sums buffers must either be completely separate (non-overlapping), or
 *   they must both start at the same address. Partially overlapping buffers are not supported.
 */
size_t adjust_channels(const void* in_buff, size_t in_buff_chans,
                       void* out_buff, size_t out_buff_chans,
                       unsigned sample_size_in_bytes, size_t num_in_bytes);

#ifdef USE_MUSL
#ifdef __cplusplus
}
#endif
#else
__END_DECLS
#endif

#endif
