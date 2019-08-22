//
// Created by hyunho on 8/17/19.
//

#ifndef LIBVPX_WRAPPER_VPX_BILINEAR_INTERP_NEON_H
#define LIBVPX_WRAPPER_VPX_BILINEAR_INTERP_NEON_H

#include <arm_neon.h>

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "../../vp9/common/vp9_onyxc_int.h"

#ifdef __cplusplus
extern "C" {
#endif

void vpx_bilinear_interp_neon_uint8(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride,
                                    int x_offset, int y_offset, int width, int height, int scale,
                                    const bilinear_config_t *config, int plane);

void vpx_bilinear_interp_neon_int16(const int16_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride,
                                    int x_offset, int y_offset, int width, int height, int scale,
                                    const bilinear_config_t *config);

#ifdef __cplusplus
    }  // extern "C"
#endif

#endif //LIBVPX_WRAPPER_VPX_BILINEAR_INTERP_NEON_H
