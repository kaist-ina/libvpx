//
// Created by hyunho on 7/24/19.
//

#include <stdio.h>
#include <math.h>
#include <android/log.h>
#include "vpx_dsp/vpx_bilinear.h"
#include "vpx_dsp/vpx_dsp_common.h"
#include "vpx_dsp_common.h"

#define TAG "vpx_bilinear.c JNI"
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

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

//TODO (hyunho): why not visible?
//TODO (hyunho): check
//need to configure exact location
void vpx_bilinear_interp_uint8_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, int x_offset, int y_offset, int width, int height, int max_width, int max_height, int scale)
{
//    LOGD("x_offset: %d, y_offset: %d, width: %d, height: %d, max_width: %d, max_height: %d, scale: %d", x_offset, y_offset, width, height,max_width, max_height, scale);
    int y_start = y_offset * scale;
    int y_end = y_offset * scale + height * scale;
    int x_start = x_offset * scale;
    int x_end = x_offset * scale + width * scale;

    for (int y = y_start; y < y_end; ++y) {
        const float in_y = (y + 0.5f) / scale - 0.5f;
        const int top_y_index = MAX(floor(in_y), 0);
        const int bottom_y_index = MIN(ceil(in_y), max_height - 1);
        const float y_lerp = in_y - floor(in_y);
        for (int x = x_start; x < x_end; ++x) {
            const float in_x = (x + 0.5f) / scale - 0.5f;
            const int left_x_index = MAX(floor(in_x), 0);
            const int right_x_index = MIN(ceil(in_x), max_width - 1);
            const float x_lerp = in_x - floor(in_x);

            const float top_left = src[top_y_index * src_stride + left_x_index];
            const float top_right = src[top_y_index * src_stride + right_x_index];
            const float bottom_left = src[bottom_y_index * src_stride + left_x_index];
            const float bottom_right = src[bottom_y_index * src_stride + right_x_index];

            const float top = top_left + (top_right - top_left) * x_lerp;
            const float bottom = bottom_left + (bottom_right - bottom_left) * x_lerp;
            const float result = top + (bottom - top) * y_lerp;

            dst[y * dst_stride + x] = clip_pixel(result);
        }
    }
}

void vpx_bilinear_interp_int16_c(const int16_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, int x_offset, int y_offset, int width, int height, int max_width, int max_height, int scale)
{
//    LOGD("x_offset: %d, y_offset: %d, width: %d, height: %d, max_width: %d, max_height: %d, scale: %d", x_offset, y_offset, width, height,max_width, max_height, scale);
    int y_start = y_offset * scale;
    int y_end = y_offset * scale + height * scale;
    int x_start = x_offset * scale;
    int x_end = x_offset * scale + width * scale;

    for (int y = y_start; y < y_end; ++y) {
        const float in_y = (y + 0.5f) / scale - 0.5f;
//        const float in_y = y / scale; //debug
//        const int top_y_index = MAX(floor(in_y), 0);
//        const int bottom_y_index = MIN(ceil(in_y), max_height - 1);
        const int top_y_index = MAX(floor(in_y), y_offset);
        const int bottom_y_index = MIN(ceil(in_y), y_offset + height - 1);
        const float y_lerp = in_y - floor(in_y);
        for (int x = x_start; x < x_end; ++x) {
            const float in_x = (x + 0.5f) / scale - 0.5f;
//            const float in_x = x / scale; //debug
//            const int left_x_index = MAX(floor(in_x), 0);
//            const int right_x_index = MIN(ceil(in_x), max_width - 1);
            const int left_x_index = MAX(floor(in_x), x_offset);
            const int right_x_index = MIN(ceil(in_x), x_offset + width - 1);
            const float x_lerp = in_x - floor(in_x);


            const float top_left = src[top_y_index * src_stride + left_x_index];
            const float top_right = src[top_y_index * src_stride + right_x_index];
            const float bottom_left = src[bottom_y_index * src_stride + left_x_index];
            const float bottom_right = src[bottom_y_index * src_stride + right_x_index];

            const float top = top_left + (top_right - top_left) * x_lerp;
            const float bottom = bottom_left + (bottom_right - bottom_left) * x_lerp;
            const float result = top + (bottom - top) * y_lerp;

            dst[y * dst_stride + x] = clip_pixel(dst[y * dst_stride + x] + result);
//            dst[y * dst_stride + x] = clip_pixel(dst[y * dst_stride + x] - result); //hyunho: debug
        }
    }
}