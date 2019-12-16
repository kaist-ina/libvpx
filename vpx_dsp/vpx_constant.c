//
// Created by hyunho on 8/1/19.
//

#include "vpx_dsp/vpx_constant.h"


void vpx_constant_c(uint8_t *dst, ptrdiff_t dst_stride, int x_offset, int y_offset, int width, int height, int scale, int value)
{
//    LOGD("x_offset: %d, y_offset: %d, width: %d, height: %d, max_width: %d, max_height: %d, scale: %d", x_offset, y_offset, width, height,max_width, max_height, scale);
    int y_start = y_offset * scale;
    int y_end = y_offset * scale + height * scale;
    int x_start = x_offset * scale;
    int x_end = x_offset * scale + width * scale;
    
    int x, y;
    for (y = y_start; y < y_end; ++y) {
        for (x = x_start; x < x_end; ++x) {
            dst[y * dst_stride + x] = value;
        }
    }
}

void vpx_flip_c(int16_t *dst, ptrdiff_t dst_stride, int x_offset, int y_offset, int width, int height, int scale)
{
    int y_start = y_offset * scale;
    int y_end = y_offset * scale + height * scale;
    int x_start = x_offset * scale;
    int x_end = x_offset * scale + width * scale;

    int x, y;
    for (y = y_start; y < y_end; ++y) {
        for (x = x_start; x < x_end; ++x) {
            dst[y * dst_stride + x] = -dst[y * dst_stride + x] ;
        }
    }
}
