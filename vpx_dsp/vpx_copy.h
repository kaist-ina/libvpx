//
// Created by hyunho on 8/1/19.
//

#ifndef LIBVPX_WRAPPER_VPX_COPY_H
#define LIBVPX_WRAPPER_VPX_COPY_H

#include "./vpx_config.h"
#include "vpx/vpx_integer.h"

#ifdef __cplusplus
extern "C" {
#endif

void vpx_copy_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, int x_offset, int y_offset, int width, int height, int scale);
void vpx_copy_and_add_c(const int16_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, int width, int height);


#ifdef __cplusplus
}  // extern "C"
#endif

#endif //LIBVPX_WRAPPER_VPX_COPY_H
