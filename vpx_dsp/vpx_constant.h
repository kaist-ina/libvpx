//
// Created by hyunho on 8/1/19.
//

#ifndef LIBVPX_WRAPPER_VPX_CONSTANT_H
#define LIBVPX_WRAPPER_VPX_CONSTANT_H

#include "./vpx_config.h"
#include "vpx/vpx_integer.h"

#ifdef __cplusplus
extern "C" {
#endif
void vpx_constant_c(uint8_t *dst, ptrdiff_t dst_stride, int x_offset, int y_offset, int width, int height, int scale, int value);
void vpx_flip_c(int16_t *dst, ptrdiff_t dst_stride, int x_offset, int y_offset, int width, int height, int scale);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif //LIBVPX_WRAPPER_VPX_CONSTANT_H
