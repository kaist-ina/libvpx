//
// Created by hyunho on 8/1/19.
//

#include "vpx_dsp/vpx_copy.h"
#include "vpx_dsp_common.h"

void vpx_copy_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, int x_offset, int y_offset, int width, int height, int scale)
{
//    LOGD("x_offset: %d, y_offset: %d, width: %d, height: %d, max_width: %d, max_height: %d, scale: %d", x_offset, y_offset, width, height,max_width, max_height, scale);
    int y_start = y_offset * scale;
    int y_end = y_offset * scale + height * scale;
    int x_start = x_offset * scale;
    int x_end = x_offset * scale + width * scale;

    int x, y;
    for (y = y_start; y < y_end; ++y) {
        for (x = x_start; x < x_end; ++x) {
            dst[y * dst_stride + x] = src[y * src_stride + x];
        }
    }
}

void vpx_copy_and_add_c(const int16_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, int width, int height)
{
//    LOGD("x_offset: %d, y_offset: %d, width: %d, height: %d, max_width: %d, max_height: %d, scale: %d", x_offset, y_offset, width, height,max_width, max_height, scale);
    int x, y;
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            dst[y * dst_stride + x] = clip_pixel(dst[y * dst_stride + x] + src[y * src_stride + x]);
        }
    }
}
