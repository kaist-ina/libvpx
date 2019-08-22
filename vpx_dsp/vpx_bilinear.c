//
// Created by hyunho on 7/24/19.
//

#include <stdio.h>
#include <math.h>
#include <android/log.h>
#include <assert.h>
#include "vpx_dsp/vpx_bilinear.h"
#include "vpx_dsp/vpx_dsp_common.h"
#include "vpx_dsp_common.h"
#include "../vp9/common/vp9_onyxc_int.h"

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
#define FRACTION_BIT (5)
#define FRACTION_SCALE (1 << FRACTION_BIT)
static const int16_t delta = (1 << (FRACTION_BIT - 1));

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
//
//    uint8_t temp[128 * 128];
//
//    for (int y = y_offset; y < y_offset + height; ++y) {
//        for (int x = x_start; x < x_end; ++x) {
//            const float in_x = (x + 0.5f) / scale - 0.5f;
//            const int left_x_index = MAX(floor(in_x), 0);
//            const int right_x_index = MIN(ceil(in_x), max_width - 1);
//            const float x_lerp = in_x - floor(in_x);
//
//            const float left = src[y * src_stride + left_x_index];
//            const float right = src[y * src_stride + right_x_index];
//
//            const float result = left + (right - left) * x_lerp;
//
////            dst[y * dst_stride + x] = clip_pixel(result);
//            temp[(y - y_offset) * 128 + (x - x_start)] = result;
//        }
//    }
//
//    for (int y = y_start; y < y_end; ++y) {
//        const float in_y = (y + 0.5f) / scale - 0.5f;
//        const int top_y_index = MAX(floor(in_y), 0);
//        const int bottom_y_index = MIN(ceil(in_y), max_height - 1);
//        const float y_lerp = in_y - floor(in_y);
//        for (int x = x_start; x < x_end; ++x) {
//            const float top = temp[(top_y_index - y_offset) * 128 + (x - x_start)];
//            const float bottom = temp[(bottom_y_index - y_offset) * 128 + (x - x_start)];
//            const float result = top + (bottom - top) * y_lerp;
//
//            dst[y * dst_stride + x] = clip_pixel(result);
//        }
//    }

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

void vpx_bilinear_interp_uint8_opt_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, int x_offset, int y_offset, int width, int height, int scale, bilinear_config_t *config)
{
//    LOGD("x_offset: %d, y_offset: %d, width: %d, height: %d, max_width: %d, max_height: %d, scale: %d", x_offset, y_offset, width, height,max_width, max_height, scale);
    int x, y;

    src = src + (y_offset * src_stride + x_offset);
    dst = dst + (y_offset * dst_stride + x_offset) * scale;

    for (int y = 0; y < height * scale; ++y) {
        const int top_y_index = config->top_y_index[y];
        const int bottom_y_index = config->bottom_y_index[y];
        const int16_t y_lerp = config->y_lerp_fixed[y];
        for (int x = 0; x < width * scale; ++x) {
            const int left_x_index = config->left_x_index[x];
            const int right_x_index = config->right_x_index[x];
            const int16_t x_lerp = config->x_lerp_fixed[x];

            const int16_t top_left = src[top_y_index * src_stride + left_x_index];
            const int16_t top_right = src[top_y_index * src_stride + right_x_index];
            const int16_t bottom_left = src[bottom_y_index * src_stride + left_x_index];
            const int16_t bottom_right = src[bottom_y_index * src_stride + right_x_index];

            const int16_t top = top_left + (((top_right - top_left) * x_lerp + delta) >> FRACTION_BIT);
            const int16_t bottom = bottom_left + (((bottom_right - bottom_left) * x_lerp + delta) >> FRACTION_BIT);
            const int16_t result = top + (((bottom - top) * y_lerp + delta) >> FRACTION_BIT);

            dst[y * dst_stride + x] = clip_pixel(result);
        }
    }
}

void vpx_bilinear_interp_int16_c(const int16_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, int x_offset, int y_offset, int width, int height, int max_width, int max_height, int scale)
{
//    LOGD("x_offset: %d, y_offset: %d, width: %d, height: %d, max_width: %d, max_height: %d, scale: %d", x_offset, y_offset, width, height,maget_bilinear_config(cm)x_width, max_height, scale);
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

int vpx_bilinear_interp_int16_test_c(const int16_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, int x_offset, int y_offset, int width, int height, int max_width, int max_height, int scale)
{
//    LOGD("x_offset: %d, y_offset: %d, width: %d, height: %d, max_width: %d, max_height: %d, scale: %d", x_offset, y_offset, width, height,max_width, max_height, scale);
    int y_start = y_offset * scale;
    int y_end = y_offset * scale + height * scale;
    int x_start = x_offset * scale;
    int x_end = x_offset * scale + width * scale;

    int top_left = 0;
    int top_right = 0;
    int bottom_left = 0;
    int bottom_right = 0;

    int total;

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


//            const int16_t top_left = src[top_y_index * src_stride + left_x_index];
//            const int16_t top_right = src[top_y_index * src_stride + right_x_index];
//            const int16_t bottom_left = src[bottom_y_index * src_stride + left_x_index];
//            const int16_t bottom_right = src[bottom_y_index * src_stride + right_x_index];

            const float top = top_left + (top_right - top_left) * x_lerp;
            const float bottom = bottom_left + (bottom_right - bottom_left) * x_lerp;
            const float result = top + (bottom - top) * y_lerp;

            total += result;
//            dst[y * dst_stride + x] = clip_pixel(dst[y * dst_stride + x] + result);
//            dst[y * dst_stride + x] = clip_pixel(dst[y * dst_stride + x] - result); //hyunho: debug
        }
    }

    return total;
}

static void vpx_bilinear_interp_horiz_c(const int16_t *src, ptrdiff_t src_stride, int16_t *dst,  ptrdiff_t dst_stride, int width, int height, int scale, const bilinear_config_t *config){
    int x, y;

    for (y = 0; y < height; ++y) {
//        for (x = 0; x < width * scale; ++x) {
        for (x = 0; x < width * scale; x = x + 2) {
//            const float in_x = (x + 0.5f) / scale - 0.5f;
//            const int left_x_index = MAX(floor(in_x), 0);
//            const int right_x_index = MIN(ceil(in_x), width * scale - 1);
//            const float x_lerp = in_x - floor(in_x);
            const int left_x_index = config->left_x_index[x];
            const int right_x_index = config->right_x_index[x];
            const int left_x_index_ = config->left_x_index[x+1];
            const int right_x_index_ = config->right_x_index[x+1];

            const int16_t x_lerp_fixed = config->x_lerp_fixed[x];
            const int16_t x_lerp_fixed_ = config->x_lerp_fixed[x+1];

            const int16_t left = src[y * src_stride + left_x_index];
            const int16_t right = src[y * src_stride + right_x_index];
            const int16_t left_ = src[y * src_stride + left_x_index_];
            const int16_t right_ = src[y * src_stride + right_x_index_];

            const int16_t result = left + (((right - left) * x_lerp_fixed + delta) >> FRACTION_BIT); //fixed-point
            const int16_t result_ = left_ + (((right_ - left_) * x_lerp_fixed_ + delta) >> FRACTION_BIT); //fixed-point

            dst[y * dst_stride + x] = result;
            dst[y * dst_stride + (x+1)] = result_;
        }
    }
}

/* deprecated */
//    for (x = 0; x < width * scale; ++x) {
//        const float in_x = (x + 0.5f) / scale - 0.5f;
//        const int left_x_index = MAX(floor(in_x), 0);
//        const int right_x_index = MIN(ceil(in_x), width * scale - 1);
//        const float x_lerp = in_x - floor(in_x);
//        for (y = 0; y < height * scale; ++y) {
//            const float left = src[y * src_stride + left_x_index];
//            const float right = src[y * src_stride + right_x_index];
//            const float result = left + (right - left) * x_lerp;
//
//            dst[y * dst_stride + x] = result;
//        }
//    }

static void vpx_bilinear_interp_vert_c(const int16_t *src, ptrdiff_t src_stride, uint8_t *dst,  ptrdiff_t dst_stride, int width, int height, int scale, const bilinear_config_t *config){
    int x, y;

    for (y = 0; y < height * scale; ++y) {
//        const float in_y = (y + 0.5f) / scale - 0.5f;
//        const int top_y_index = MAX(floor(in_y), 0);
//        const int bottom_y_index = MIN(ceil(in_y), height * scale - 1);
//        const float y_lerp = in_y - floor(in_y);
        const int top_y_index = config->top_y_index[y];
        const int bottom_y_index = config->bottom_y_index[y];
//        const float y_lerp = config->y_lerp[y];
        const int16_t y_lerp_fixed = config->y_lerp_fixed[y];

//        for (x = 0; x < width * scale; ++x) {
        for (x = 0; x < width * scale; x = x + 2) {
            const int16_t top = src[top_y_index * src_stride + x];
            const int16_t bottom = src[bottom_y_index * src_stride + x];
            const int16_t top_ = src[top_y_index * src_stride + (x + 1)];
            const int16_t bottom_ = src[bottom_y_index * src_stride + (x + 1)];

            const int16_t result = top + (((bottom - top) * y_lerp_fixed + delta) >> FRACTION_BIT);
            const int16_t result_ = top_ + (((bottom_ - top_) * y_lerp_fixed + delta) >> FRACTION_BIT); // top_ +((bottom - top) * y_lerp_fixed)

            dst[y * dst_stride + x] = clip_pixel(dst[y * dst_stride + x] + result);
            dst[y * dst_stride + (x + 1)] = clip_pixel(dst[y * dst_stride + (x + 1)] + result_);
        }
    }
}

void vpx_bilinear_interp_c(const int16_t *src, ptrdiff_t src_stride, uint8_t *dst,  ptrdiff_t dst_stride, int x_offset, int y_offset, int width, int height, int scale, const bilinear_config_t *config){
    int16_t temp[128 * 128];

    assert(width <= 32);
    assert(height <= 32);
    assert(scale <= 4 && scale >= 2);

    src = src + (y_offset * src_stride + x_offset);
    dst = dst + (y_offset * dst_stride + x_offset) * scale;

    vpx_bilinear_interp_horiz_c(src, src_stride, temp, 128, width, height, scale, config);
    vpx_bilinear_interp_vert_c(temp, 128, dst, dst_stride, width, height, scale, config);
}