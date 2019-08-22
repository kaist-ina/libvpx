//
// Created by hyunho on 7/24/19.
//

#ifndef LIBVPX_WRAPPER_VPX_BILINEAR_H
#define LIBVPX_WRAPPER_VPX_BILINEAR_H

#include <vp9/common/vp9_onyxc_int.h>
#include "./vpx_config.h"
#include "vpx/vpx_integer.h"

#ifdef __cplusplus
extern "C" {
#endif
    void vpx_bilinear_interp_uint8_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, int x_offset, int y_offest, int width, int height, int max_height, int max_width, int scale);
    void vpx_bilinear_interp_uint8_opt_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, int x_offset, int y_offset, int width, int height, int scale, bilinear_config_t *config);
    void vpx_bilinear_interp_int16_c(const int16_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, int x_offset, int y_offset, int width, int height, int max_width, int max_height, int scale);
    int vpx_bilinear_interp_int16_test_c(const int16_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, int x_offset, int y_offset, int width, int height, int max_width, int max_height, int scale);
    void vpx_bilinear_interp_c(const int16_t *src, ptrdiff_t src_stride, uint8_t *dst,  ptrdiff_t dst_stride, int x_offset, int y_offset, int width, int height, int scale, const bilinear_config_t *config);


#ifdef __cplusplus
}  // extern "C"
#endif

#endif //LIBVPX_WRAPPER_VPX_SCALE_H
