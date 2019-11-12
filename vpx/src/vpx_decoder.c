/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*!\file
 * \brief Provides the high level interface to wrap decoder algorithms.
 *
 */
#include <string.h>
#include <vp9/vp9_dx_iface.h>
#include "vpx/internal/vpx_codec_internal.h"

#include <vpx/vpx_mobinas.h>


#define SAVE_STATUS(ctx, var) (ctx ? (ctx->err = var) : var)

static vpx_codec_alg_priv_t *get_alg_priv(vpx_codec_ctx_t *ctx) {
  return (vpx_codec_alg_priv_t *)ctx->priv;
}

vpx_codec_err_t vpx_codec_dec_init_ver(vpx_codec_ctx_t *ctx,
                                       vpx_codec_iface_t *iface,
                                       const vpx_codec_dec_cfg_t *cfg,
                                       vpx_codec_flags_t flags, int ver) {
  vpx_codec_err_t res;

  if (ver != VPX_DECODER_ABI_VERSION)
    res = VPX_CODEC_ABI_MISMATCH;
  else if (!ctx || !iface)
    res = VPX_CODEC_INVALID_PARAM;
  else if (iface->abi_version != VPX_CODEC_INTERNAL_ABI_VERSION)
    res = VPX_CODEC_ABI_MISMATCH;
  else if ((flags & VPX_CODEC_USE_POSTPROC) &&
           !(iface->caps & VPX_CODEC_CAP_POSTPROC))
    res = VPX_CODEC_INCAPABLE;
  else if ((flags & VPX_CODEC_USE_ERROR_CONCEALMENT) &&
           !(iface->caps & VPX_CODEC_CAP_ERROR_CONCEALMENT))
    res = VPX_CODEC_INCAPABLE;
  else if ((flags & VPX_CODEC_USE_INPUT_FRAGMENTS) &&
           !(iface->caps & VPX_CODEC_CAP_INPUT_FRAGMENTS))
    res = VPX_CODEC_INCAPABLE;
  else if (!(iface->caps & VPX_CODEC_CAP_DECODER))
    res = VPX_CODEC_INCAPABLE;
  else {
    memset(ctx, 0, sizeof(*ctx));
    ctx->iface = iface;
    ctx->name = iface->name;
    ctx->priv = NULL;
    ctx->init_flags = flags;
    ctx->config.dec = cfg;

    res = ctx->iface->init(ctx, NULL);
    if (res) {
      ctx->err_detail = ctx->priv ? ctx->priv->err_detail : NULL;
      vpx_codec_destroy(ctx);
    }
  }

  return SAVE_STATUS(ctx, res);
}

vpx_codec_err_t vpx_codec_peek_stream_info(vpx_codec_iface_t *iface,
                                           const uint8_t *data,
                                           unsigned int data_sz,
                                           vpx_codec_stream_info_t *si) {
  vpx_codec_err_t res;

  if (!iface || !data || !data_sz || !si ||
      si->sz < sizeof(vpx_codec_stream_info_t))
    res = VPX_CODEC_INVALID_PARAM;
  else {
    /* Set default/unknown values */
    si->w = 0;
    si->h = 0;

    res = iface->dec.peek_si(data, data_sz, si);
  }

  return res;
}

vpx_codec_err_t vpx_codec_get_stream_info(vpx_codec_ctx_t *ctx,
                                          vpx_codec_stream_info_t *si) {
  vpx_codec_err_t res;

  if (!ctx || !si || si->sz < sizeof(vpx_codec_stream_info_t))
    res = VPX_CODEC_INVALID_PARAM;
  else if (!ctx->iface || !ctx->priv)
    res = VPX_CODEC_ERROR;
  else {
    /* Set default/unknown values */
    si->w = 0;
    si->h = 0;

    res = ctx->iface->dec.get_si(get_alg_priv(ctx), si);
  }

  return SAVE_STATUS(ctx, res);
}

vpx_codec_err_t vpx_codec_decode(vpx_codec_ctx_t *ctx, const uint8_t *data,
                                 unsigned int data_sz, void *user_priv,
                                 long deadline) {
  vpx_codec_err_t res;

  /* Sanity checks */
  /* NULL data ptr allowed if data_sz is 0 too */
  if (!ctx || (!data && data_sz) || (data && !data_sz))
    res = VPX_CODEC_INVALID_PARAM;
  else if (!ctx->iface || !ctx->priv)
    res = VPX_CODEC_ERROR;
  else {
    res = ctx->iface->dec.decode(get_alg_priv(ctx), data, data_sz, user_priv,
                                 deadline);
  }

  return SAVE_STATUS(ctx, res);
}

vpx_image_t *vpx_codec_get_frame(vpx_codec_ctx_t *ctx, vpx_codec_iter_t *iter) {
  vpx_image_t *img;

  if (!ctx || !iter || !ctx->iface || !ctx->priv)
    img = NULL;
  else
    img = ctx->iface->dec.get_frame(get_alg_priv(ctx), iter);

  return img;
}

vpx_codec_err_t vpx_codec_register_put_frame_cb(vpx_codec_ctx_t *ctx,
                                                vpx_codec_put_frame_cb_fn_t cb,
                                                void *user_priv) {
  vpx_codec_err_t res;

  if (!ctx || !cb)
    res = VPX_CODEC_INVALID_PARAM;
  else if (!ctx->iface || !ctx->priv ||
           !(ctx->iface->caps & VPX_CODEC_CAP_PUT_FRAME))
    res = VPX_CODEC_ERROR;
  else {
    ctx->priv->dec.put_frame_cb.u.put_frame = cb;
    ctx->priv->dec.put_frame_cb.user_priv = user_priv;
    res = VPX_CODEC_OK;
  }

  return SAVE_STATUS(ctx, res);
}

vpx_codec_err_t vpx_codec_register_put_slice_cb(vpx_codec_ctx_t *ctx,
                                                vpx_codec_put_slice_cb_fn_t cb,
                                                void *user_priv) {
  vpx_codec_err_t res;

  if (!ctx || !cb)
    res = VPX_CODEC_INVALID_PARAM;
  else if (!ctx->iface || !ctx->priv ||
           !(ctx->iface->caps & VPX_CODEC_CAP_PUT_SLICE))
    res = VPX_CODEC_ERROR;
  else {
    ctx->priv->dec.put_slice_cb.u.put_slice = cb;
    ctx->priv->dec.put_slice_cb.user_priv = user_priv;
    res = VPX_CODEC_OK;
  }

  return SAVE_STATUS(ctx, res);
}

vpx_codec_err_t vpx_codec_set_frame_buffer_functions(
    vpx_codec_ctx_t *ctx, vpx_get_frame_buffer_cb_fn_t cb_get,
    vpx_release_frame_buffer_cb_fn_t cb_release, void *cb_priv) {
  vpx_codec_err_t res;

  if (!ctx || !cb_get || !cb_release) {
    res = VPX_CODEC_INVALID_PARAM;
  } else if (!ctx->iface || !ctx->priv ||
             !(ctx->iface->caps & VPX_CODEC_CAP_EXTERNAL_FRAME_BUFFER)) {
    res = VPX_CODEC_ERROR;
  } else {
    res = ctx->iface->dec.set_fb_fn(get_alg_priv(ctx), cb_get, cb_release,
                                    cb_priv);
  }

  return SAVE_STATUS(ctx, res);
}


vpx_codec_err_t vpx_load_mobinas_cfg(vpx_codec_ctx_t *ctx, mobinas_cfg_t *mobinas_cfg){
    vpx_codec_err_t res;

    res = ctx->iface->mobinas.load_cfg(get_alg_priv(ctx), mobinas_cfg);

    return SAVE_STATUS(ctx, res);
}


//API function calls for snpe + libvpx, and exoplayer + snpe + libvpx

mobinas_cfg_t * snpe_mobinas_decode_cfg(const char * save_dir, const char * prefix,
                                        const char * target_video, const char * compare_video){
    mobinas_cfg_t * mobinas = init_mobinas_cfg();
    strcpy(mobinas->save_dir, save_dir);
    strcpy(mobinas->prefix, prefix);
    strcpy(mobinas->target_file, target_video);
    strcpy(mobinas->compare_file, compare_video);

    mobinas->save_intermediate_frame = 1;
    mobinas->save_final_frame = 1;
    mobinas->save_quality_result = 1;
    mobinas->save_metadata_result = 1;
    mobinas->save_latency_result = 0;
    mobinas->decode_mode = DECODE;//mode
    mobinas->dnn_mode = ONLINE_DNN;
    mobinas->cache_policy = NO_CACHE;
    mobinas->cache_profile = NULL;
    mobinas->get_scale = default_scale_policy;
    mobinas->model_quality = HQ;

    mobinas->save_sr_latency_breakdown = 0;

    return mobinas;
}

mobinas_cfg_t * snpe_mobinas_decode_sr_cfg(const char * save_dir, const char * prefix,
        const char * target_video, const char * compare_video){
    mobinas_cfg_t * mobinas = init_mobinas_cfg();
    strcpy(mobinas->save_dir, save_dir);
    strcpy(mobinas->prefix, prefix);
    strcpy(mobinas->target_file, target_video);
    strcpy(mobinas->compare_file, compare_video);

    mobinas->save_intermediate_frame = 1;
    mobinas->save_final_frame = 1;
    mobinas->save_quality_result = 0;
    mobinas->save_metadata_result = 1;
    mobinas->save_latency_result = 1;
    mobinas->decode_mode = DECODE_SR;//mode
    mobinas->dnn_mode = ONLINE_DNN;
    mobinas->cache_policy = NO_CACHE;
    mobinas->cache_profile = NULL;
    mobinas->get_scale = default_scale_policy;
    mobinas->model_quality = HQ;

    mobinas->save_sr_latency_breakdown = 1;

    return mobinas;
}

mobinas_cfg_t * exoplayer_mobinas_decode_cfg(){
    mobinas_cfg_t * mobinas = init_mobinas_cfg();
    //no need for video paths, since exoplayer streams online video and takes care of input
    mobinas->save_intermediate_frame = 1;
    mobinas->save_final_frame = 1;
    mobinas->save_quality_result = 1;
    mobinas->save_metadata_result = 1;
    mobinas->save_latency_result = 1;
    mobinas->decode_mode = DECODE;//mode
    mobinas->dnn_mode = ONLINE_DNN;
    mobinas->cache_policy = NO_CACHE;
    mobinas->cache_profile = NULL;
    mobinas->get_scale = default_scale_policy;
    mobinas->model_quality = HQ;

    return mobinas;
}

mobinas_cfg_t * exoplayer_mobinas_decode_sr_cfg(){
    mobinas_cfg_t * mobinas = init_mobinas_cfg();
    //no need for video paths, since exoplayer streams online video and takes care of input
    mobinas->save_intermediate_frame = 1;
    mobinas->save_final_frame = 1;
    mobinas->save_quality_result = 1;
    mobinas->save_metadata_result = 1;
    mobinas->save_latency_result = 1;
    mobinas->decode_mode = DECODE_SR;//mode
    mobinas->dnn_mode = ONLINE_DNN;
    mobinas->cache_policy = NO_CACHE;
    mobinas->cache_profile = NULL;
    mobinas->get_scale = default_scale_policy;
    mobinas->model_quality = HQ;

    return mobinas;
}