/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "./vpx_config.h"
#include "./vpx_version.h"

#include "vpx/internal/vpx_codec_internal.h"
#include "vpx/vp8dx.h"
#include "vpx/vpx_decoder.h"
#include "vpx_dsp/bitreader_buffer.h"
#include "vpx_dsp/vpx_dsp_common.h"
#include "vpx_util/vpx_thread.h"

#include "vp9/common/vp9_alloccommon.h"
#include "vp9/common/vp9_frame_buffers.h"

#include "vp9/decoder/vp9_decodeframe.h"

#include "vp9/vp9_dx_iface.h"
#include "vp9/vp9_iface_common.h"

#include <android/log.h>
#include <sys/param.h>
#include <vpx_util/vpx_write_yuv_frame.h>
#include <vpx_dsp/psnr.h>
#include <vpx_dsp/ssim.h>
#include <vpx_dsp_rtcd.h>

#define LOG_MAX 1000
#define TAG "vp9_dx_iface.c JNI"
#define _UNKNOWN   0
#define _DEFAULT   1
#define _VERBOSE   2
#define _DEBUG    3
#define _INFO        4
#define _WARN        5
#define _ERROR    6
#define _FATAL    7
#define _SILENT       8
#define LOGUNK(...) __android_log_print(_UNKNOWN,TAG,__VA_ARGS__)
#define LOGDEF(...) __android_log_print(_DEFAULT,TAG,__VA_ARGS__)
#define LOGV(...) __android_log_print(_VERBOSE,TAG,__VA_ARGS__)
#define LOGD(...) __android_log_print(_DEBUG,TAG,__VA_ARGS__)
#define LOGI(...) __android_log_print(_INFO,TAG,__VA_ARGS__)
#define LOGW(...) __android_log_print(_WARN,TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(_ERROR,TAG,__VA_ARGS__)
#define LOGF(...) __android_log_print(_FATAL,TAG,__VA_ARGS__)
#define LOGS(...) __android_log_print(_SILENT,TAG,__VA_ARGS__)

#define VP9_CAP_POSTPROC (CONFIG_VP9_POSTPROC ? VPX_CODEC_CAP_POSTPROC : 0)

//hyunho
#define DEBUG_LATENCY 1
#define DEBUG_LR_QUALITY 0
#define FRACTION_BIT (5)
#define FRACTION_SCALE (1 << FRACTION_BIT)

static void bilinear_config_init(bilinear_config_t *config, int scale, int width, int height) {
    int x, y;

    float *x_lerp = config->x_lerp;
    int16_t *x_lerp_fixed = config->x_lerp_fixed;
    float *y_lerp = config->y_lerp;
    int16_t *y_lerp_fixed = config->y_lerp_fixed;
    int *top_y_index = config->top_y_index;
    int *bottom_y_index = config->bottom_y_index;
    int *left_x_index = config->left_x_index;
    int *right_x_index = config->right_x_index;

    for (x = 0; x < width * scale; ++x) {
        const float in_x = (x + 0.5f) / scale - 0.5f;
        left_x_index[x] = MAX(floor(in_x), 0);
        right_x_index[x] = MIN(ceil(in_x), width - 1);
        x_lerp[x] = in_x - floor(in_x);
        x_lerp_fixed[x] = x_lerp[x] * FRACTION_SCALE;
    }

    for (y = 0; y < height * scale; ++y) {
        const float in_y = (y + 0.5f) / scale - 0.5f;
        top_y_index[y] = MAX(floor(in_y), 0);
        bottom_y_index[y] = MIN(ceil(in_y), height - 1);
        y_lerp[y] = in_y - floor(in_y);
        y_lerp_fixed[y] = y_lerp[y] * FRACTION_SCALE;
    }
}

static void bilinear_profile_init(bilinear_profile_t *profile){
    //scale x4
    bilinear_config_init(get_bilinear_config(profile, 4, 4), 4, 4, 4);
    bilinear_config_init(get_bilinear_config(profile, 4, 8), 4, 8, 8);
    bilinear_config_init(get_bilinear_config(profile, 4, 16), 4, 16, 16);
    bilinear_config_init(get_bilinear_config(profile, 4, 32), 4, 32, 32);

    //scale x3
    bilinear_config_init(get_bilinear_config(profile, 4, 4), 4, 4, 4);
    bilinear_config_init(get_bilinear_config(profile, 4, 8), 4, 8, 8);
    bilinear_config_init(get_bilinear_config(profile, 4, 16), 4, 16, 16);
    bilinear_config_init(get_bilinear_config(profile, 4, 32), 4, 32, 32);

    //scale x2
    bilinear_config_init(get_bilinear_config(profile, 4, 4), 4, 4, 4);
    bilinear_config_init(get_bilinear_config(profile, 4, 8), 4, 8, 8);
    bilinear_config_init(get_bilinear_config(profile, 4, 16), 4, 16, 16);
    bilinear_config_init(get_bilinear_config(profile, 4, 32), 4, 32, 32);
}

static vpx_codec_err_t decoder_init(vpx_codec_ctx_t *ctx,
                                    vpx_codec_priv_enc_mr_cfg_t *data) {
    // This function only allocates space for the vpx_codec_alg_priv_t
    // structure. More memory may be required at the time the stream
    // information becomes known.
    (void) data;

    if (!ctx->priv) {
        vpx_codec_alg_priv_t *const priv =
                (vpx_codec_alg_priv_t *) vpx_calloc(1, sizeof(*priv));
        if (priv == NULL) return VPX_CODEC_MEM_ERROR;

        ctx->priv = (vpx_codec_priv_t *) priv;
        ctx->priv->init_flags = ctx->init_flags;
        priv->si.sz = sizeof(priv->si);
        priv->flushed = 0;
        if (ctx->config.dec) {
            priv->cfg = *ctx->config.dec;
            ctx->config.dec = &priv->cfg;
        }
        /*******************Hyunho************************/
//        memset(&priv->mobinas_cfg, 0, sizeof(priv->mobinas_cfg));
//        priv->mobinas_cfg.mode = DECODE; //hyunho: default mode is DECODE.
        /*******************Hyunho************************/
    }

    return VPX_CODEC_OK;
}

static vpx_codec_err_t decoder_destroy(vpx_codec_alg_priv_t *ctx) {
    if (ctx->pbi != NULL) {
        vp9_decoder_remove(ctx->pbi);
    }

    if (ctx->buffer_pool) {
        vp9_free_ref_frame_buffers(ctx->buffer_pool);
        vp9_free_internal_frame_buffers(&ctx->buffer_pool->int_frame_buffers);
    }

    vpx_free(ctx->buffer_pool);
    vpx_free(ctx);

    /*******************Hyunho************************/
    VP9_COMMON *cm = &ctx->pbi->common;
    if (cm->latency_log != NULL) fclose(cm->latency_log);
    if (cm->metadata_log != NULL) fclose(cm->metadata_log);
    if (cm->quality_log != NULL) fclose(cm->quality_log);
    /*******************Hyunho************************/

    return VPX_CODEC_OK;
}

static int parse_bitdepth_colorspace_sampling(BITSTREAM_PROFILE profile,
                                              struct vpx_read_bit_buffer *rb) {
    vpx_color_space_t color_space;
    if (profile >= PROFILE_2) rb->bit_offset += 1;  // Bit-depth 10 or 12.
    color_space = (vpx_color_space_t) vpx_rb_read_literal(rb, 3);
    if (color_space != VPX_CS_SRGB) {
        rb->bit_offset += 1;  // [16,235] (including xvycc) vs [0,255] range.
        if (profile == PROFILE_1 || profile == PROFILE_3) {
            rb->bit_offset += 2;  // subsampling x/y.
            rb->bit_offset += 1;  // unused.
        }
    } else {
        if (profile == PROFILE_1 || profile == PROFILE_3) {
            rb->bit_offset += 1;  // unused
        } else {
            // RGB is only available in version 1.
            return 0;
        }
    }
    return 1;
}

static vpx_codec_err_t decoder_peek_si_internal(
        const uint8_t *data, unsigned int data_sz, vpx_codec_stream_info_t *si,
        int *is_intra_only, vpx_decrypt_cb decrypt_cb, void *decrypt_state) {
    int intra_only_flag = 0;
    uint8_t clear_buffer[10];

    if (data + data_sz <= data) return VPX_CODEC_INVALID_PARAM;

    si->is_kf = 0;
    si->w = si->h = 0;

    if (decrypt_cb) {
        data_sz = VPXMIN(sizeof(clear_buffer), data_sz);
        decrypt_cb(decrypt_state, data, clear_buffer, data_sz);
        data = clear_buffer;
    }

    // A maximum of 6 bits are needed to read the frame marker, profile and
    // show_existing_frame.
    if (data_sz < 1) return VPX_CODEC_UNSUP_BITSTREAM;

    {
        int show_frame;
        int error_resilient;
        struct vpx_read_bit_buffer rb = {data, data + data_sz, 0, NULL, NULL};
        const int frame_marker = vpx_rb_read_literal(&rb, 2);
        const BITSTREAM_PROFILE profile = vp9_read_profile(&rb);

        if (frame_marker != VP9_FRAME_MARKER) return VPX_CODEC_UNSUP_BITSTREAM;

        if (profile >= MAX_PROFILES) return VPX_CODEC_UNSUP_BITSTREAM;

        if (vpx_rb_read_bit(&rb)) {  // show an existing frame
            // If profile is > 2 and show_existing_frame is true, then at least 1 more
            // byte (6+3=9 bits) is needed.
            if (profile > 2 && data_sz < 2) return VPX_CODEC_UNSUP_BITSTREAM;
            vpx_rb_read_literal(&rb, 3);  // Frame buffer to show.
            return VPX_CODEC_OK;
        }

        // For the rest of the function, a maximum of 9 more bytes are needed
        // (computed by taking the maximum possible bits needed in each case). Note
        // that this has to be updated if we read any more bits in this function.
        if (data_sz < 10) return VPX_CODEC_UNSUP_BITSTREAM;

        si->is_kf = !vpx_rb_read_bit(&rb);
        show_frame = vpx_rb_read_bit(&rb);
        error_resilient = vpx_rb_read_bit(&rb);

        if (si->is_kf) {
            if (!vp9_read_sync_code(&rb)) return VPX_CODEC_UNSUP_BITSTREAM;

            if (!parse_bitdepth_colorspace_sampling(profile, &rb))
                return VPX_CODEC_UNSUP_BITSTREAM;
            vp9_read_frame_size(&rb, (int *) &si->w, (int *) &si->h);
        } else {
            intra_only_flag = show_frame ? 0 : vpx_rb_read_bit(&rb);

            rb.bit_offset += error_resilient ? 0 : 2;  // reset_frame_context

            if (intra_only_flag) {
                if (!vp9_read_sync_code(&rb)) return VPX_CODEC_UNSUP_BITSTREAM;
                if (profile > PROFILE_0) {
                    if (!parse_bitdepth_colorspace_sampling(profile, &rb))
                        return VPX_CODEC_UNSUP_BITSTREAM;
                }
                rb.bit_offset += REF_FRAMES;  // refresh_frame_flags
                vp9_read_frame_size(&rb, (int *) &si->w, (int *) &si->h);
            }
        }
    }
    if (is_intra_only != NULL) *is_intra_only = intra_only_flag;
    return VPX_CODEC_OK;
}

static vpx_codec_err_t decoder_peek_si(const uint8_t *data,
                                       unsigned int data_sz,
                                       vpx_codec_stream_info_t *si) {
    return decoder_peek_si_internal(data, data_sz, si, NULL, NULL, NULL);
}

static vpx_codec_err_t decoder_get_si(vpx_codec_alg_priv_t *ctx,
                                      vpx_codec_stream_info_t *si) {
    const size_t sz = (si->sz >= sizeof(vp9_stream_info_t))
                      ? sizeof(vp9_stream_info_t)
                      : sizeof(vpx_codec_stream_info_t);
    memcpy(si, &ctx->si, sz);
    si->sz = (unsigned int) sz;

    return VPX_CODEC_OK;
}

static void set_error_detail(vpx_codec_alg_priv_t *ctx,
                             const char *const error) {
    ctx->base.err_detail = error;
}

static vpx_codec_err_t update_error_state(
        vpx_codec_alg_priv_t *ctx, const struct vpx_internal_error_info *error) {
    if (error->error_code)
        set_error_detail(ctx, error->has_detail ? error->detail : NULL);

    return error->error_code;
}

static void init_buffer_callbacks(vpx_codec_alg_priv_t *ctx) {
    VP9_COMMON *const cm = &ctx->pbi->common;
    BufferPool *const pool = cm->buffer_pool;

    cm->new_fb_idx = INVALID_IDX;
    cm->byte_alignment = ctx->byte_alignment;
    cm->skip_loop_filter = ctx->skip_loop_filter;

    if (ctx->get_ext_fb_cb != NULL && ctx->release_ext_fb_cb != NULL) {
        pool->get_fb_cb = ctx->get_ext_fb_cb;
        pool->release_fb_cb = ctx->release_ext_fb_cb;
        pool->cb_priv = ctx->ext_priv;
    } else {
        pool->get_fb_cb = vp9_get_frame_buffer;
        pool->release_fb_cb = vp9_release_frame_buffer;

        if (vp9_alloc_internal_frame_buffers(&pool->int_frame_buffers))
            vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                               "Failed to initialize internal frame buffers");

        pool->cb_priv = &pool->int_frame_buffers;
    }

    pool->mode = cm->mobinas_cfg->mode;
}

static void set_default_ppflags(vp8_postproc_cfg_t *cfg) {
    cfg->post_proc_flag = VP8_DEBLOCK | VP8_DEMACROBLOCK;
    cfg->deblocking_level = 4;
    cfg->noise_level = 0;
}

static void set_ppflags(const vpx_codec_alg_priv_t *ctx, vp9_ppflags_t *flags) {
    flags->post_proc_flag = ctx->postproc_cfg.post_proc_flag;

    flags->deblocking_level = ctx->postproc_cfg.deblocking_level;
    flags->noise_level = ctx->postproc_cfg.noise_level;
}

static vpx_codec_err_t init_decoder(vpx_codec_alg_priv_t *ctx) {
    ctx->last_show_frame = -1;
    ctx->need_resync = 1;
    ctx->flushed = 0;

    ctx->buffer_pool = (BufferPool *) vpx_calloc(1, sizeof(BufferPool));
    if (ctx->buffer_pool == NULL) return VPX_CODEC_MEM_ERROR;

    ctx->pbi = vp9_decoder_create(ctx->buffer_pool);
    if (ctx->pbi == NULL) {
        set_error_detail(ctx, "Failed to allocate decoder");
        return VPX_CODEC_MEM_ERROR;
    }
    ctx->pbi->max_threads = ctx->cfg.threads;
    ctx->pbi->inv_tile_order = ctx->invert_tile_order;

    // If postprocessing was enabled by the application and a
    // configuration has not been provided, default it.
    if (!ctx->postproc_cfg_set && (ctx->base.init_flags & VPX_CODEC_USE_POSTPROC))
        set_default_ppflags(&ctx->postproc_cfg);

    /*******************Hyunho************************/
    //hyunho: additional overhead is < 0.1 msec
    VP9_COMMON *cm = &ctx->pbi->common;
    cm->mobinas_cfg = &ctx->mobinas_cfg;
//    clock_t start, end;
//    double cpu_time_used;
//    start = clock();
    bilinear_profile_init(&cm->bl_profile);
//    end = clock();
//    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC * 1000;
//    LOGD("additional init overhead: %.2fmsec", cpu_time_used);
    /*******************Hyunho************************/

    init_buffer_callbacks(ctx);

    return VPX_CODEC_OK;
}

static INLINE void check_resync(vpx_codec_alg_priv_t *const ctx,
                                const VP9Decoder *const pbi) {
    // Clear resync flag if the decoder got a key frame or intra only frame.
    if (ctx->need_resync == 1 && pbi->need_resync == 0 &&
        (pbi->common.intra_only || pbi->common.frame_type == KEY_FRAME))
        ctx->need_resync = 0;
}

static vpx_codec_err_t decode_one(vpx_codec_alg_priv_t *ctx,
                                  const uint8_t **data, unsigned int data_sz,
                                  void *user_priv, int64_t deadline) {
    (void) deadline;

    // Determine the stream parameters. Note that we rely on peek_si to
    // validate that we have a buffer that does not wrap around the top
    // of the heap.
    if (!ctx->si.h) {
        int is_intra_only = 0;
        const vpx_codec_err_t res =
                decoder_peek_si_internal(*data, data_sz, &ctx->si, &is_intra_only,
                                         ctx->decrypt_cb, ctx->decrypt_state);
        if (res != VPX_CODEC_OK) return res;

        if (!ctx->si.is_kf && !is_intra_only) return VPX_CODEC_ERROR;
    }

    ctx->user_priv = user_priv;

    // Set these even if already initialized.  The caller may have changed the
    // decrypt config between frames.
    ctx->pbi->decrypt_cb = ctx->decrypt_cb;
    ctx->pbi->decrypt_state = ctx->decrypt_state;

    if (vp9_receive_compressed_data(ctx->pbi, data_sz, data)) {
        ctx->pbi->cur_buf->buf.corrupted = 1;
        ctx->pbi->need_resync = 1;
        ctx->need_resync = 1;
        return update_error_state(ctx, &ctx->pbi->common.error);
    }

    check_resync(ctx, ctx->pbi);

    return VPX_CODEC_OK;
}

static void save_serialized_intermediate_frame(VP9_COMMON *cm, int current_video_frame, int current_super_frame)
{
    char file_path[PATH_MAX];
    if (cm->mobinas_cfg->mode == DECODE_SR_CACHE) {
        memset(file_path, 0, sizeof(char) * PATH_MAX);
        sprintf(file_path, "%s/%d_%d_hr_%s.serialize", cm->mobinas_cfg->serialize_dir, current_video_frame, current_super_frame, cm->mobinas_cfg->prefix);
        if (vpx_serialize_save(file_path, get_sr_frame_new_buffer(cm))) { //check: sr frame
            LOGE("save a serialized frame fail");
        }

        sprintf(file_path, "%s/%d_%d_lr_%s.serialize", cm->mobinas_cfg->serialize_dir, current_video_frame, current_super_frame, cm->mobinas_cfg->prefix);
        if (vpx_serialize_save(file_path, get_frame_new_buffer(cm))) {
            LOGE("save a serialized frame fail");
        }
    }
    else {
        sprintf(file_path, "%s/%d_%d_%s.serialize", cm->mobinas_cfg->serialize_dir, current_video_frame, current_super_frame, cm->mobinas_cfg->prefix);
        if (vpx_serialize_save(file_path, get_frame_new_buffer(cm))) //check: original frame
        {
            LOGE("save a serialized frame fail");
        }
    }
}

static void save_serialized_final_frame(VP9_COMMON *cm, int current_video_frame)
{
    char file_path[PATH_MAX];
    if (cm->mobinas_cfg->mode == DECODE_SR_CACHE) {
        memset(file_path, 0, sizeof(char) * PATH_MAX);
        sprintf(file_path, "%s/%d_hr_%s.serialize", cm->mobinas_cfg->serialize_dir, current_video_frame, cm->mobinas_cfg->prefix);
        if (vpx_serialize_save(file_path, get_sr_frame_new_buffer(cm))) { //check: sr frame
            LOGE("save a serialized frame fail");
        }

        sprintf(file_path, "%s/%d_lr_%s.serialize", cm->mobinas_cfg->serialize_dir, current_video_frame, cm->mobinas_cfg->prefix);
        if (vpx_serialize_save(file_path, get_frame_new_buffer(cm))) {
            LOGE("save a serialized frame fail");
        }
    }
    else {
        sprintf(file_path, "%s/%d_%s.serialize", cm->mobinas_cfg->serialize_dir, current_video_frame, cm->mobinas_cfg->prefix);
        if (vpx_serialize_save(file_path, get_frame_new_buffer(cm))) //check: original frame
        {
            LOGE("save a serialized frame fail");
        }
    }
}

static void save_decoded_intermediate_frame(VP9_COMMON *cm, int current_video_frame,
                                            int current_super_frame)
{
    char file_path[PATH_MAX];
    if (cm->mobinas_cfg->save_decoded_frame && cm->mobinas_cfg->save_intermediate)
    {
        if (cm->mobinas_cfg->mode == DECODE_SR_CACHE) {
            memset(file_path, 0, sizeof(char) * PATH_MAX);
            sprintf(file_path, "%s/%d_%d_hr_%s.y", cm->mobinas_cfg->frame_dir, current_video_frame, current_super_frame, cm->mobinas_cfg->prefix);
            if (vpx_write_y_frame(file_path, get_sr_frame_new_buffer(cm))) //check: sr frame
            {
                LOGE("save a decoded frame fail");
            }

            sprintf(file_path, "%s/%d_%d_lr_%s.y", cm->mobinas_cfg->frame_dir, current_video_frame, current_super_frame, cm->mobinas_cfg->prefix);
            if (vpx_write_y_frame(file_path, get_frame_new_buffer(cm)))
            {
                LOGE("save a decoded frame fail");
            }
        }
        else {
            memset(file_path, 0, sizeof(char) * PATH_MAX);
            sprintf(file_path, "%s/%d_%d_%s.y", cm->mobinas_cfg->frame_dir, current_video_frame, current_super_frame, cm->mobinas_cfg->prefix);
            if (vpx_write_y_frame(file_path, get_frame_new_buffer(cm))) //check: original frame
            {
                LOGE("save a decoded frame fail");
            }
        }
    }
}

static void save_decoded_final_frame(VP9_COMMON *cm, int current_video_frame)
{
    char file_path[PATH_MAX];
    if (cm->mobinas_cfg->save_decoded_frame && cm->mobinas_cfg->save_intermediate)
    {
        if (cm->mobinas_cfg->mode == DECODE_SR_CACHE) {
            memset(file_path, 0, sizeof(char) * PATH_MAX);
            sprintf(file_path, "%s/%d_hr_%s.y", cm->mobinas_cfg->frame_dir, current_video_frame, cm->mobinas_cfg->prefix);
            if (vpx_write_y_frame(file_path, get_sr_frame_new_buffer(cm))) //check: sr frame
            {
                LOGE("save a decoded frame fail");
            }

            sprintf(file_path, "%s/%d_lr_%s.y", cm->mobinas_cfg->frame_dir, current_video_frame, cm->mobinas_cfg->prefix);
            if (vpx_write_y_frame(file_path, get_frame_new_buffer(cm)))
            {
                LOGE("save a decoded frame fail");
            }
        }
        else {
            memset(file_path, 0, sizeof(char) * PATH_MAX);
            sprintf(file_path, "%s/%d_%s.y", cm->mobinas_cfg->frame_dir, current_video_frame, cm->mobinas_cfg->prefix);
            if (vpx_write_y_frame(file_path, get_frame_new_buffer(cm))) //check: original frame
            {
                LOGE("save a decoded frame fail");
            }
        }
    }
}

static void save_sr_cache_decode_result(VP9_COMMON *cm, int current_video_frame, int current_super_frame)
{
    char log[LOG_MAX];

    //latency log
    memset(log, 0, LOG_MAX);
    sprintf(log, "%d\t%d\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\n", current_video_frame,
            current_super_frame, cm->latency.decode_frame,
            cm->latency.interp_intra_block, cm->latency.interp_inter_residual,
            cm->latency.decode_intra_block, cm->latency.decode_inter_block,
            cm->latency.decode_inter_residual);
    fputs(log, cm->latency_log);

    //metadata log
    memset(log, 0, LOG_MAX);
    sprintf(log, "%d\t%d\t%d\t%d\t%d\t%d\t%d\n", current_video_frame, current_super_frame, cm->count, cm->intra_count, cm->inter_count, cm->inter_noskip_count, cm->adaptive_cache_count); //TODO: 추가...
    fputs(log, cm->metadata_log);
}

//TODO: check it's only for super-resolution
static void save_sr_cache_quality_result(VP9_COMMON *cm){
    char file_path[PATH_MAX];
    char log[LOG_MAX];
    PSNR_STATS psnr;
    memset(file_path, 0, sizeof(char) * PATH_MAX);
    int width_ = get_sr_frame_new_buffer(cm)->y_width; //check: sr frame
    int height_ = get_sr_frame_new_buffer(cm)->y_height; //check: sr frame
//    LOGD("width_:%d, height_: %d", width_, height_);
    sprintf(file_path, "%s/%d_%s.serialize", cm->mobinas_cfg->serialize_dir, cm->current_video_frame - 1, cm->mobinas_cfg->compare_file);
    if(vpx_deserialize_load(cm->hr_reference_frame, file_path, width_, height_,
                            cm->subsampling_x, cm->subsampling_y, cm->byte_alignment))
    {
        vpx_internal_error(&cm->error, VPX_MOBINAS_ERROR,
                           "deserialize failed");
    }

    double weight;
    double ssim = vpx_calc_ssim(get_sr_frame_new_buffer(cm), cm->hr_reference_frame, &weight); //check: sr frame
    vpx_calc_psnr(get_sr_frame_new_buffer(cm), cm->hr_reference_frame, &psnr); //check: sr frame
    LOGD("High-resolution SR-cache quality, %d, PSNR %.2fdB, SSIM %.2f", cm->current_video_frame - 1, psnr.psnr[0], ssim);

    //qualtiy log
    memset(log, 0, LOG_MAX);
    sprintf(log, "%d\t%.2f\t%.2f\t%.2f\t%.2f\n", cm->current_video_frame - 1, psnr.psnr[0], psnr.psnr[1], psnr.psnr[2], psnr.psnr[3]);

    fputs(log, cm->quality_log);
}

static void show_lr_cache_quality(VP9_COMMON *cm, int current_video_frame,
                                  int current_super_frame)
{
    char file_path[PATH_MAX];
    PSNR_STATS psnr;
    memset(file_path, 0, sizeof(char) * PATH_MAX);
    int width_ = get_frame_new_buffer(cm)->y_width;
    int height_ = get_frame_new_buffer(cm)->y_height;
    sprintf(file_path, "%s/%d_%d_%s.serialize", cm->mobinas_cfg->serialize_dir, current_video_frame, current_super_frame, cm->mobinas_cfg->target_file);
    LOGD("file path: %s", file_path);
    if(vpx_deserialize_load(cm->lr_reference_frame, file_path, width_, height_,
                            cm->subsampling_x, cm->subsampling_y, cm->byte_alignment))
    {
        vpx_internal_error(&cm->error, VPX_MOBINAS_ERROR,
                           "deserialize failed");
    }
    vpx_calc_psnr(get_frame_new_buffer(cm), cm->lr_reference_frame, &psnr);

    LOGD("Low-resolution quality, %d, %d, %.2fdB", current_video_frame, current_super_frame, psnr.psnr[0]);
}

static void apply_bilinear(VP9_COMMON *cm) {
    YV12_BUFFER_CONFIG *lr_frame = get_frame_new_buffer(cm); //check: original frame
    uint8_t *const lr_frame_buffers[MAX_MB_PLANE] = {lr_frame->y_buffer, lr_frame->u_buffer,
                                                     lr_frame->v_buffer};
    const int lr_frame_strides[MAX_MB_PLANE] = {lr_frame->y_stride, lr_frame->uv_stride,
                                                lr_frame->uv_stride};
    const int max_heights[MAX_MB_PLANE] = {lr_frame->y_crop_height, lr_frame->uv_crop_height,
                                           lr_frame->uv_crop_height};
    const int max_widths[MAX_MB_PLANE] = {lr_frame->y_crop_width, lr_frame->uv_crop_width,
                                          lr_frame->uv_crop_width};

    YV12_BUFFER_CONFIG *hr_debug_frame = cm->hr_debug_frame;
    uint8_t *const hr_debug_frame_buffers[MAX_MB_PLANE] = {hr_debug_frame->y_buffer, hr_debug_frame->u_buffer,
                                                           hr_debug_frame->v_buffer};
    const int hr_debug_frame_strides[MAX_MB_PLANE] = {hr_debug_frame->y_stride, hr_debug_frame->uv_stride,
                                                      hr_debug_frame->uv_stride};

    //1. make own bilinear config
    bilinear_config_t config;

    //2. aaply bilinear interpolation
    LOGF("%s: Need to test", __func__);
    for (int plane = 0; plane < MAX_MB_PLANE; ++plane) {
        bilinear_config_init(&config, cm->scale, max_widths[plane], max_heights[plane]);
        vpx_bilinear_interp_uint8(lr_frame_buffers[plane], lr_frame_strides[plane],
                                       hr_debug_frame_buffers[plane], hr_debug_frame_strides[plane],
                                       0, 0, max_widths[plane], max_heights[plane], cm->scale,
                                       get_bilinear_config(&cm->bl_profile, cm->scale, max_widths[plane]));
    }
}

static void save_bilinear_quality_result(VP9_COMMON *cm) {
    char file_path[PATH_MAX];
    char log[LOG_MAX];
    FILE *quality_log;
    int target_width = get_frame_new_buffer(cm)->y_crop_width * cm->scale; //check: original frame
    int target_height = get_frame_new_buffer(cm)->y_crop_height * cm->scale; //check: original frame

    if (cm->current_video_frame == 1) {
        memset(file_path, 0, PATH_MAX);
        sprintf(file_path, "%s/quality_bilinear_%dp_%s.log", cm->mobinas_cfg->log_dir,
                target_height, cm->mobinas_cfg->prefix);
        quality_log = fopen(file_path, "w");
    } else {
        memset(file_path, 0, PATH_MAX);
        sprintf(file_path, "%s/quality_bilinear_%dp_%s.log", cm->mobinas_cfg->log_dir,
                target_height, cm->mobinas_cfg->prefix);
        quality_log = fopen(file_path, "a");
    }

    PSNR_STATS psnr;
    memset(file_path, 0, sizeof(char) * PATH_MAX);

    sprintf(file_path, "%s/%d_%s.serialize", cm->mobinas_cfg->serialize_dir,
            cm->current_video_frame - 1, cm->mobinas_cfg->compare_file);
    if (vpx_deserialize_load(cm->hr_reference_frame, file_path, target_width, target_height,
                             cm->subsampling_x, cm->subsampling_y, cm->byte_alignment)) {
        vpx_internal_error(&cm->error, VPX_MOBINAS_ERROR,
                           "deserialize failed");
    }
    vpx_calc_psnr(cm->hr_debug_frame, cm->hr_reference_frame, &psnr);

    //qualtiy log
    memset(log, 0, LOG_MAX);
    sprintf(log, "%d\t%.2f\t%.2f\t%.2f\t%.2f\n", cm->current_video_frame - 1, psnr.psnr[0],
            psnr.psnr[1], psnr.psnr[2], psnr.psnr[3]);
    LOGD("Bilinear quality, %d, PSNR %.2fdB", cm->current_video_frame - 1, psnr.psnr[0]);

    //TODO: move to decoder_destroy()
    fputs(log, quality_log);
    if (quality_log != NULL) fclose(quality_log);
}

static void mobinas_init(vpx_codec_alg_priv_t *ctx, vpx_mobinas_cfg_t *mobinas_cfg) {
    //copy directory
    strcpy(ctx->mobinas_cfg.video_dir, mobinas_cfg->video_dir);
    strcpy(ctx->mobinas_cfg.log_dir, mobinas_cfg->log_dir);
    strcpy(ctx->mobinas_cfg.frame_dir, mobinas_cfg->frame_dir);
    strcpy(ctx->mobinas_cfg.serialize_dir, mobinas_cfg->serialize_dir);

    //copy name
    strcpy(ctx->mobinas_cfg.prefix, mobinas_cfg->prefix);
    strcpy(ctx->mobinas_cfg.target_file, mobinas_cfg->target_file);
    strcpy(ctx->mobinas_cfg.cache_file, mobinas_cfg->cache_file);
    strcpy(ctx->mobinas_cfg.compare_file, mobinas_cfg->compare_file);

    //copy log setting
    ctx->mobinas_cfg.save_serialized_frame = mobinas_cfg->save_serialized_frame;
    ctx->mobinas_cfg.save_decoded_frame = mobinas_cfg->save_decoded_frame;
    ctx->mobinas_cfg.save_intermediate = mobinas_cfg->save_intermediate;
    ctx->mobinas_cfg.save_final = mobinas_cfg->save_final;
    ctx->mobinas_cfg.save_quality_result = mobinas_cfg->save_quality_result;
    ctx->mobinas_cfg.save_decode_result = mobinas_cfg->save_decode_result;

    //copy decoder setting
    ctx->mobinas_cfg.mode = mobinas_cfg->mode;
    ctx->mobinas_cfg.target_resolution = mobinas_cfg->target_resolution;
}
//여기서 set을 하고, 이후에 decode loop에서 가져오는 형식으로 하자. decode loop에서 NULL이 아니면 적용하는 방식. 나중에 destroy할때 free시켜줘야한다.
// 1. API design + buffer handle (release) + decode에서는 NULL인지 check하고 넘어가는 방식으로 // cm에서 pointer만 가져오는 형식으로
// 2. Check latency, quality on cache mode

//TODO (chanju): SNPE runtime configuration
//TODO (chanju): if scale is change, model output should be changed.
//TODO: video resolution can be changed during streaming, so log name should be changed including more info. (e.g., resolution)
//TODO: Exoplayer should pass scale, mode, log_dir can be hard-coded.
static vpx_codec_err_t decoder_decode(vpx_codec_alg_priv_t *ctx,
                                      const uint8_t *data, unsigned int data_sz,
                                      void *user_priv, long deadline) {
    const uint8_t *data_start = data;
    const uint8_t *const data_end = data + data_sz;
    vpx_codec_err_t res;
    uint32_t frame_sizes[8];
    int frame_count;
    VP9_COMMON *cm;

    if (data == NULL && data_sz == 0) {
        ctx->flushed = 1;
        return VPX_CODEC_OK;
    }

    // Reset flushed when receiving a valid frame.
    ctx->flushed = 0;

    // Initialize the decoder on the first frame.
    if (ctx->pbi == NULL) {
        const vpx_codec_err_t res = init_decoder(ctx);
        if (res != VPX_CODEC_OK) return res;

        /*******************Hyunho************************/
        cm = &ctx->pbi->common;

        //init logs
        char file_path[PATH_MAX];
        if (cm->mobinas_cfg->save_decode_result == 1) {
            memset(file_path, 0, PATH_MAX);
            sprintf(file_path, "%s/latency_%s.log", cm->mobinas_cfg->log_dir, cm->mobinas_cfg->prefix);
            cm->latency_log = fopen(file_path, "w");
            memset(file_path, 0, PATH_MAX);
            sprintf(file_path, "%s/metadata_%s.log", cm->mobinas_cfg->log_dir, cm->mobinas_cfg->prefix);
            cm->metadata_log = fopen(file_path, "w");
        }
        if (cm->mobinas_cfg->save_quality_result == 1) {
            memset(file_path, 0, PATH_MAX);
            sprintf(file_path, "%s/quality_%s.log", cm->mobinas_cfg->log_dir, cm->mobinas_cfg->prefix);
            cm->quality_log = fopen(file_path, "w");
        }
        /*******************Hyunho************************/
    }
    else {
        cm = &ctx->pbi->common;
        cm->mobinas_cfg = &ctx->mobinas_cfg;
        cm->buffer_pool->mode = cm->mobinas_cfg->mode;
    }

    res = vp9_parse_superframe_index(data, data_sz, frame_sizes, &frame_count,
                                     ctx->decrypt_cb, ctx->decrypt_state);
    if (res != VPX_CODEC_OK) return res;

    if (ctx->svc_decoding && ctx->svc_spatial_layer < frame_count - 1)
        frame_count = ctx->svc_spatial_layer + 1;

#if DEBUG_LATENCY
    clock_t start, end;
    double cpu_time_used;
#endif

    if (frame_count > 0) {
        int i;
        int current_video_frame;
        cm->current_super_frame = 0;

        for (i = 0; i < frame_count; ++i) {
            const uint8_t *data_start_copy = data_start;
            const uint32_t frame_size = frame_sizes[i];
            vpx_codec_err_t res;
            if (data_start < data || frame_size > (uint32_t) (data_end - data_start)) {
                set_error_detail(ctx, "Invalid frame size in index");
                return VPX_CODEC_CORRUPT_FRAME;
            }
#if DEBUG_LATENCY
            memset(&cm->latency, 0, sizeof(cm->latency));
            start = clock();
#endif
            res = decode_one(ctx, &data_start_copy, frame_size, user_priv, deadline);
            if (res != VPX_CODEC_OK) return res;
#if DEBUG_LATENCY
            end = clock();
            cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC * 1000;
            cm->latency.decode_frame += cpu_time_used;
#endif

            data_start += frame_size;

            //TODO: log latency (예전 log 뒤에 추가하기)

            /*******************Hyunho************************/
            if (cm->show_frame == 0) current_video_frame = cm->current_video_frame;
            else current_video_frame = cm->current_video_frame - 1;

            if (cm->mobinas_cfg->save_intermediate) {
                if (cm->mobinas_cfg->save_serialized_frame)
                    save_serialized_intermediate_frame(cm, current_video_frame,
                                                       cm->current_super_frame);
                if (cm->mobinas_cfg->save_decoded_frame)
                    save_decoded_intermediate_frame(cm, current_video_frame,
                                                    cm->current_super_frame);
            }

            if (cm->mobinas_cfg->mode == DECODE_SR_CACHE) {
                if (cm->mobinas_cfg->save_decode_result) save_sr_cache_decode_result(cm, current_video_frame, cm->current_super_frame);
#if DEBUG_LR_QUALITY
                if (mobinas_cfg->save_quality_result) show_lr_cache_quality(cm, current_video_frame, cm->current_super_frame);
#endif
            }

            cm->current_super_frame++;
            /*******************Hyunho************************/
        }
    } else {
        while (data_start < data_end) {
            const uint32_t frame_size = (uint32_t) (data_end - data_start);
#if DEBUG_LATENCY
            memset(&cm->latency, 0, sizeof(cm->latency));
            start = clock();
#endif
            const vpx_codec_err_t res = decode_one(ctx, &data_start, frame_size, user_priv, deadline);
            if (res != VPX_CODEC_OK) return res;
#if DEBUG_LATENCY
            end = clock();
            cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC * 1000;
            cm->latency.decode_frame += cpu_time_used;
#endif

            // Account for suboptimal termination by the encoder.
            while (data_start < data_end) {
                const uint8_t marker =
                        read_marker(ctx->decrypt_cb, ctx->decrypt_state, data_start);
                if (marker) break;

                ++data_start;
            }

            /*******************Hyunho************************/
            if (cm->mobinas_cfg->save_intermediate) {
                if (cm->mobinas_cfg->save_serialized_frame) save_serialized_intermediate_frame(cm,
                                                                                           cm->current_video_frame -
                                                                                               1,
                                                                                               cm->current_super_frame);
                if (cm->mobinas_cfg->save_decoded_frame) save_decoded_intermediate_frame(cm,
                                                                                     cm->current_video_frame -
                                                                                         1,
                                                                                         cm->current_super_frame);
            }

            if (cm->mobinas_cfg->mode == DECODE_SR_CACHE) {
                if (cm->mobinas_cfg->save_decode_result) save_sr_cache_decode_result(cm, cm->current_video_frame - 1, cm->current_super_frame);

#if DEBUG_LR_QUALITY
                if (mobinas_cfg->save_quality_result) show_lr_cache_quality(cm, cm->current_video_frame - 1, cm->current_super_frame);
#endif
            }
            /*******************Hyunho************************/
        }
    }

    /*******************Hyunho************************/
    if (cm->current_super_frame > 0) cm->current_super_frame--;

    if (cm->mobinas_cfg->save_final) {
        if (cm->mobinas_cfg->save_serialized_frame) save_serialized_final_frame(cm, cm->current_video_frame - 1);
        if (cm->mobinas_cfg->save_decoded_frame) save_decoded_final_frame(cm, cm->current_video_frame -1);
    }

    if (cm->mobinas_cfg->mode == DECODE_SR_CACHE) {
        if (cm->mobinas_cfg->save_quality_result) save_sr_cache_quality_result(cm);
    }
    else if (cm->mobinas_cfg->mode == DECODE_BILINEAR){ //calculate bilinear interpolation quality
        //apply bilinear interpolation
        apply_bilinear(cm);

        //measure quality and save a log
        save_bilinear_quality_result(cm);
    }
    /*******************Hyunho************************/
    return res;
}

static vpx_image_t *decoder_get_frame(vpx_codec_alg_priv_t *ctx,
                                      vpx_codec_iter_t *iter) {
    vpx_image_t *img = NULL;

//     Legacy parameter carried over from VP8. Has no effect for VP9 since we
//     always return only 1 frame per decode call.
    (void) iter;

    if (ctx->pbi != NULL) {
        YV12_BUFFER_CONFIG sd;
        vp9_ppflags_t flags = {0, 0, 0};
        if (ctx->base.init_flags & VPX_CODEC_USE_POSTPROC) set_ppflags(ctx, &flags);
        if (vp9_get_raw_frame(ctx->pbi, &sd, &flags) == 0) {
            VP9_COMMON *const cm = &ctx->pbi->common;
            RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;
            ctx->last_show_frame = ctx->pbi->common.new_fb_idx;
            if (ctx->need_resync) return NULL;
            yuvconfig2image(&ctx->img, &sd, ctx->user_priv);
            ctx->img.fb_priv = frame_bufs[cm->new_fb_idx].raw_frame_buffer.priv;
            img = &ctx->img;
            return img;
        }
    }
    return NULL;
}

static vpx_codec_err_t decoder_set_fb_fn(
        vpx_codec_alg_priv_t *ctx, vpx_get_frame_buffer_cb_fn_t cb_get,
        vpx_release_frame_buffer_cb_fn_t cb_release, void *cb_priv) {
    if (cb_get == NULL || cb_release == NULL) {
        return VPX_CODEC_INVALID_PARAM;
    } else if (ctx->pbi == NULL) {
        // If the decoder has already been initialized, do not accept changes to
        // the frame buffer functions.
        ctx->get_ext_fb_cb = cb_get;
        ctx->release_ext_fb_cb = cb_release;
        ctx->ext_priv = cb_priv;
        return VPX_CODEC_OK;
    }

    return VPX_CODEC_ERROR;
}

static vpx_codec_err_t ctrl_set_reference(vpx_codec_alg_priv_t *ctx,
                                          va_list args) {
    vpx_ref_frame_t *const data = va_arg(args, vpx_ref_frame_t *);

    if (data) {
        vpx_ref_frame_t *const frame = (vpx_ref_frame_t *) data;
        YV12_BUFFER_CONFIG sd;
        image2yuvconfig(&frame->img, &sd);
        return vp9_set_reference_dec(
                &ctx->pbi->common, ref_frame_to_vp9_reframe(frame->frame_type), &sd);
    } else {
        return VPX_CODEC_INVALID_PARAM;
    }
}

static vpx_codec_err_t ctrl_copy_reference(vpx_codec_alg_priv_t *ctx,
                                           va_list args) {
    vpx_ref_frame_t *data = va_arg(args, vpx_ref_frame_t *);

    if (data) {
        vpx_ref_frame_t *frame = (vpx_ref_frame_t *) data;
        YV12_BUFFER_CONFIG sd;
        image2yuvconfig(&frame->img, &sd);
        return vp9_copy_reference_dec(ctx->pbi, (VP9_REFFRAME) frame->frame_type,
                                      &sd);
    } else {
        return VPX_CODEC_INVALID_PARAM;
    }
}

static vpx_codec_err_t ctrl_get_reference(vpx_codec_alg_priv_t *ctx,
                                          va_list args) {
    vp9_ref_frame_t *data = va_arg(args, vp9_ref_frame_t *);

    if (data) {
        YV12_BUFFER_CONFIG *fb;
        fb = get_ref_frame(&ctx->pbi->common, data->idx);
        if (fb == NULL) return VPX_CODEC_ERROR;
        yuvconfig2image(&data->img, fb, NULL);
        return VPX_CODEC_OK;
    } else {
        return VPX_CODEC_INVALID_PARAM;
    }
}

static vpx_codec_err_t ctrl_set_postproc(vpx_codec_alg_priv_t *ctx,
                                         va_list args) {
#if CONFIG_VP9_POSTPROC
    vp8_postproc_cfg_t *data = va_arg(args, vp8_postproc_cfg_t *);

    if (data) {
      ctx->postproc_cfg_set = 1;
      ctx->postproc_cfg = *((vp8_postproc_cfg_t *)data);
      return VPX_CODEC_OK;
    } else {
      return VPX_CODEC_INVALID_PARAM;
    }
#else
    (void) ctx;
    (void) args;
    return VPX_CODEC_INCAPABLE;
#endif
}

static vpx_codec_err_t ctrl_get_quantizer(vpx_codec_alg_priv_t *ctx,
                                          va_list args) {
    int *const arg = va_arg(args, int *);
    if (arg == NULL || ctx->pbi == NULL) return VPX_CODEC_INVALID_PARAM;
    *arg = ctx->pbi->common.base_qindex;
    return VPX_CODEC_OK;
}

static vpx_codec_err_t ctrl_get_last_ref_updates(vpx_codec_alg_priv_t *ctx,
                                                 va_list args) {
    int *const update_info = va_arg(args, int *);

    if (update_info) {
        if (ctx->pbi != NULL) {
            *update_info = ctx->pbi->refresh_frame_flags;
            return VPX_CODEC_OK;
        } else {
            return VPX_CODEC_ERROR;
        }
    }

    return VPX_CODEC_INVALID_PARAM;
}

static vpx_codec_err_t ctrl_get_frame_corrupted(vpx_codec_alg_priv_t *ctx,
                                                va_list args) {
    int *corrupted = va_arg(args, int *);

    if (corrupted) {
        if (ctx->pbi != NULL) {
            RefCntBuffer *const frame_bufs = ctx->pbi->common.buffer_pool->frame_bufs;
            if (ctx->pbi->common.frame_to_show == NULL) return VPX_CODEC_ERROR;
            if (ctx->last_show_frame >= 0)
                *corrupted = frame_bufs[ctx->last_show_frame].buf.corrupted;
            return VPX_CODEC_OK;
        } else {
            return VPX_CODEC_ERROR;
        }
    }

    return VPX_CODEC_INVALID_PARAM;
}

static vpx_codec_err_t ctrl_get_frame_size(vpx_codec_alg_priv_t *ctx,
                                           va_list args) {
    int *const frame_size = va_arg(args, int *);

    if (frame_size) {
        if (ctx->pbi != NULL) {
            const VP9_COMMON *const cm = &ctx->pbi->common;
            frame_size[0] = cm->width;
            frame_size[1] = cm->height;
            return VPX_CODEC_OK;
        } else {
            return VPX_CODEC_ERROR;
        }
    }

    return VPX_CODEC_INVALID_PARAM;
}

static vpx_codec_err_t ctrl_get_render_size(vpx_codec_alg_priv_t *ctx,
                                            va_list args) {
    int *const render_size = va_arg(args, int *);

    if (render_size) {
        if (ctx->pbi != NULL) {
            const VP9_COMMON *const cm = &ctx->pbi->common;
            render_size[0] = cm->render_width;
            render_size[1] = cm->render_height;
            return VPX_CODEC_OK;
        } else {
            return VPX_CODEC_ERROR;
        }
    }

    return VPX_CODEC_INVALID_PARAM;
}

static vpx_codec_err_t ctrl_get_bit_depth(vpx_codec_alg_priv_t *ctx,
                                          va_list args) {
    unsigned int *const bit_depth = va_arg(args, unsigned int *);

    if (bit_depth) {
        if (ctx->pbi != NULL) {
            const VP9_COMMON *const cm = &ctx->pbi->common;
            *bit_depth = cm->bit_depth;
            return VPX_CODEC_OK;
        } else {
            return VPX_CODEC_ERROR;
        }
    }

    return VPX_CODEC_INVALID_PARAM;
}

static vpx_codec_err_t ctrl_set_invert_tile_order(vpx_codec_alg_priv_t *ctx,
                                                  va_list args) {
    ctx->invert_tile_order = va_arg(args, int);
    return VPX_CODEC_OK;
}

static vpx_codec_err_t ctrl_set_decryptor(vpx_codec_alg_priv_t *ctx,
                                          va_list args) {
    vpx_decrypt_init *init = va_arg(args, vpx_decrypt_init *);
    ctx->decrypt_cb = init ? init->decrypt_cb : NULL;
    ctx->decrypt_state = init ? init->decrypt_state : NULL;
    return VPX_CODEC_OK;
}

static vpx_codec_err_t ctrl_set_byte_alignment(vpx_codec_alg_priv_t *ctx,
                                               va_list args) {
    const int legacy_byte_alignment = 0;
    const int min_byte_alignment = 32;
    const int max_byte_alignment = 1024;
    const int byte_alignment = va_arg(args, int);

    if (byte_alignment != legacy_byte_alignment &&
        (byte_alignment < min_byte_alignment ||
         byte_alignment > max_byte_alignment ||
         (byte_alignment & (byte_alignment - 1)) != 0))
        return VPX_CODEC_INVALID_PARAM;

    ctx->byte_alignment = byte_alignment;
    if (ctx->pbi != NULL) {
        ctx->pbi->common.byte_alignment = byte_alignment;
    }
    return VPX_CODEC_OK;
}

static vpx_codec_err_t ctrl_set_skip_loop_filter(vpx_codec_alg_priv_t *ctx,
                                                 va_list args) {
    ctx->skip_loop_filter = va_arg(args, int);

    if (ctx->pbi != NULL) {
        ctx->pbi->common.skip_loop_filter = ctx->skip_loop_filter;
    }

    return VPX_CODEC_OK;
}

static vpx_codec_err_t ctrl_set_spatial_layer_svc(vpx_codec_alg_priv_t *ctx,
                                                  va_list args) {
    ctx->svc_decoding = 1;
    ctx->svc_spatial_layer = va_arg(args, int);
    if (ctx->svc_spatial_layer < 0)
        return VPX_CODEC_INVALID_PARAM;
    else
        return VPX_CODEC_OK;
}

static vpx_codec_ctrl_fn_map_t decoder_ctrl_maps[] = {
        {VP8_COPY_REFERENCE,           ctrl_copy_reference},

        // Setters
        {VP8_SET_REFERENCE,            ctrl_set_reference},
        {VP8_SET_POSTPROC,             ctrl_set_postproc},
        {VP9_INVERT_TILE_DECODE_ORDER, ctrl_set_invert_tile_order},
        {VPXD_SET_DECRYPTOR,           ctrl_set_decryptor},
        {VP9_SET_BYTE_ALIGNMENT,       ctrl_set_byte_alignment},
        {VP9_SET_SKIP_LOOP_FILTER,     ctrl_set_skip_loop_filter},
        {VP9_DECODE_SVC_SPATIAL_LAYER, ctrl_set_spatial_layer_svc},

        // Getters
        {VPXD_GET_LAST_QUANTIZER,      ctrl_get_quantizer},
        {VP8D_GET_LAST_REF_UPDATES,    ctrl_get_last_ref_updates},
        {VP8D_GET_FRAME_CORRUPTED,     ctrl_get_frame_corrupted},
        {VP9_GET_REFERENCE,            ctrl_get_reference},
        {VP9D_GET_DISPLAY_SIZE,        ctrl_get_render_size},
        {VP9D_GET_BIT_DEPTH,           ctrl_get_bit_depth},
        {VP9D_GET_FRAME_SIZE,          ctrl_get_frame_size},

        {-1, NULL},
};

#ifndef VERSION_STRING
#define VERSION_STRING
#endif

CODEC_INTERFACE(vpx_codec_vp9_dx) = {
        "WebM Project VP9 Decoder" VERSION_STRING,
        VPX_CODEC_INTERNAL_ABI_VERSION,
#if CONFIG_VP9_HIGHBITDEPTH
        VPX_CODEC_CAP_HIGHBITDEPTH |
#endif
        VPX_CODEC_CAP_DECODER | VP9_CAP_POSTPROC |
        VPX_CODEC_CAP_EXTERNAL_FRAME_BUFFER,  // vpx_codec_caps_t
        decoder_init,                             // vpx_codec_init_fn_t
        decoder_destroy,                          // vpx_codec_destroy_fn_t
        decoder_ctrl_maps,                        // vpx_codec_ctrl_fn_map_t
        {
                // NOLINT
                decoder_peek_si,    // vpx_codec_peek_si_fn_t
                decoder_get_si,     // vpx_codec_get_si_fn_t
                decoder_decode,     // vpx_codec_decode_fn_t
                decoder_get_frame,  // vpx_codec_frame_get_fn_t
                decoder_set_fb_fn,  // vpx_codec_set_fb_fn_t
        },
        {
                // NOLINT
                0,
                NULL,  // vpx_codec_enc_cfg_map_t
                NULL,  // vpx_codec_encode_fn_t
                NULL,  // vpx_codec_get_cx_data_fn_t
                NULL,  // vpx_codec_enc_config_set_fn_t
                NULL,  // vpx_codec_get_global_headers_fn_t
                NULL,  // vpx_codec_get_preview_frame_fn_t
                NULL   // vpx_codec_enc_mr_get_mem_loc_fn_t
        },
        {
            mobinas_init
        }
};
