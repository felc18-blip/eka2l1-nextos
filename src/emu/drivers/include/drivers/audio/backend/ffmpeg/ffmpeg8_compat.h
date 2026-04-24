// SPDX-License-Identifier: GPL-2.0-or-later
// NextOS: FFmpeg 4.x → 8.x compatibility shim.
//
// Upstream EKA2L1 still uses the legacy (pre-5.1) FFmpeg audio API; system
// FFmpeg 8.x removed several symbols. This header provides drop-in inline
// replacements so the existing source compiles unmodified against new libav*.
//
// Include this from every .cpp under drivers/src/audio/backend/ffmpeg/ and
// drivers/src/video/backend/ffmpeg/. Safe to include multiple times.
#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

// avcodec_close was deprecated in FFmpeg 4.0 and removed in 8.0.
// avcodec_free_context performs the equivalent teardown.
static inline int avcodec_close(AVCodecContext *ctx) {
    if (ctx) avcodec_free_context(&ctx);
    return 0;
}

// Legacy swr_alloc_set_opts (uint64 channel layouts) removed in 8.0. Reimplement
// via swr_alloc_set_opts2 with AVChannelLayout values derived from the masks.
static inline struct SwrContext *swr_alloc_set_opts(
        struct SwrContext *s,
        int64_t out_layout, enum AVSampleFormat out_fmt, int out_rate,
        int64_t in_layout,  enum AVSampleFormat in_fmt,  int in_rate,
        int log_offset, void *log_ctx) {
    AVChannelLayout out_ch, in_ch;
    av_channel_layout_from_mask(&out_ch, (uint64_t)out_layout);
    av_channel_layout_from_mask(&in_ch,  (uint64_t)in_layout);
    int err = swr_alloc_set_opts2(&s, &out_ch, out_fmt, out_rate,
                                  &in_ch, in_fmt, in_rate,
                                  log_offset, log_ctx);
    av_channel_layout_uninit(&out_ch);
    av_channel_layout_uninit(&in_ch);
    return err < 0 ? nullptr : s;
}

// av_get_channel_layout_nb_channels: replaced by AVChannelLayout.nb_channels.
static inline int av_get_channel_layout_nb_channels(uint64_t layout) {
    AVChannelLayout ch;
    av_channel_layout_from_mask(&ch, layout);
    int n = ch.nb_channels;
    av_channel_layout_uninit(&ch);
    return n;
}
