/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <string.h>

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "vpx/vpx_integer.h"
#include "vpx_dsp/vpx_convolve.h"
#include "vpx_dsp/vpx_dsp_common.h"
#include "vpx_dsp/vpx_filter.h"
#include "vpx_ports/mem.h"

#include <android/log.h>
#include <arm_neon.h>

#define TAG "vpx_convolve.c JNI"
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

static void convolve_horiz(const uint8_t *src, ptrdiff_t src_stride,
                           uint8_t *dst, ptrdiff_t dst_stride,
                           const InterpKernel *x_filters, int x0_q4,
                           int x_step_q4, int w, int h) {
  int x, y;
  src -= SUBPEL_TAPS / 2 - 1;

  for (y = 0; y < h; ++y) {
    int x_q4 = x0_q4;
    for (x = 0; x < w; ++x) {
      const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
      dst[x] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void convolve_horiz_residual(const int16_t *src, ptrdiff_t src_stride,
                           int16_t *dst, ptrdiff_t dst_stride,
                           const InterpKernel *x_filters, int x0_q4,
                           int x_step_q4, int w, int h) {
    int x, y;
    src -= SUBPEL_TAPS / 2 - 1;

    for (y = 0; y < h; ++y) {
        int x_q4 = x0_q4;
        for (x = 0; x < w; ++x) {
            const int16_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
            const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
            int k, sum = 0;
            for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
//            dst[x] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
//            dst[x] = clip_pixel_int8(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
            dst[x] = ROUND_POWER_OF_TWO(sum, FILTER_BITS);
            x_q4 += x_step_q4;
        }
        src += src_stride;
        dst += dst_stride;
    }
}


static void convolve_avg_horiz(const uint8_t *src, ptrdiff_t src_stride,
                               uint8_t *dst, ptrdiff_t dst_stride,
                               const InterpKernel *x_filters, int x0_q4,
                               int x_step_q4, int w, int h) {
  int x, y;
  src -= SUBPEL_TAPS / 2 - 1;

  for (y = 0; y < h; ++y) {
    int x_q4 = x0_q4;
    for (x = 0; x < w; ++x) {
      const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
      dst[x] = ROUND_POWER_OF_TWO(
          dst[x] + clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS)), 1);
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void convolve_vert(const uint8_t *src, ptrdiff_t src_stride,
                          uint8_t *dst, ptrdiff_t dst_stride,
                          const InterpKernel *y_filters, int y0_q4,
                          int y_step_q4, int w, int h) {
  int x, y;
  src -= src_stride * (SUBPEL_TAPS / 2 - 1);

  for (x = 0; x < w; ++x) {
    int y_q4 = y0_q4;
    for (y = 0; y < h; ++y) {
      const uint8_t *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k)
        sum += src_y[k * src_stride] * y_filter[k];
      dst[y * dst_stride] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

static void convolve_vert_residual(const int16_t *src, ptrdiff_t src_stride,
                          int16_t *dst, ptrdiff_t dst_stride,
                          const InterpKernel *y_filters, int y0_q4,
                          int y_step_q4, int w, int h) {
    int x, y;
    src -= src_stride * (SUBPEL_TAPS / 2 - 1);

    for (x = 0; x < w; ++x) {
        int y_q4 = y0_q4;
        for (y = 0; y < h; ++y) {
            const int16_t *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
            const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
            int k, sum = 0;
            for (k = 0; k < SUBPEL_TAPS; ++k)
                sum += src_y[k * src_stride] * y_filter[k];
            //dst[y * dst_stride] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
            dst[y * dst_stride] = ROUND_POWER_OF_TWO(sum, FILTER_BITS);
            y_q4 += y_step_q4;
        }
        ++src;
        ++dst;
    }
}

static void convolve_avg_vert(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const InterpKernel *y_filters, int y0_q4,
                              int y_step_q4, int w, int h) {
  int x, y;
  src -= src_stride * (SUBPEL_TAPS / 2 - 1);

  for (x = 0; x < w; ++x) {
    int y_q4 = y0_q4;
    for (y = 0; y < h; ++y) {
      const uint8_t *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k)
        sum += src_y[k * src_stride] * y_filter[k];
      dst[y * dst_stride] = ROUND_POWER_OF_TWO(
          dst[y * dst_stride] +
              clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS)),
          1);
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

void vpx_convolve8_horiz_c(const uint8_t *src, ptrdiff_t src_stride,
                           uint8_t *dst, ptrdiff_t dst_stride,
                           const InterpKernel *filter, int x0_q4, int x_step_q4,
                           int y0_q4, int y_step_q4, int w, int h) {
  (void)y0_q4;
  (void)y_step_q4;
  convolve_horiz(src, src_stride, dst, dst_stride, filter, x0_q4, x_step_q4, w,
                 h);
}

void vpx_convolve8_avg_horiz_c(const uint8_t *src, ptrdiff_t src_stride,
                               uint8_t *dst, ptrdiff_t dst_stride,
                               const InterpKernel *filter, int x0_q4,
                               int x_step_q4, int y0_q4, int y_step_q4, int w,
                               int h) {
  (void)y0_q4;
  (void)y_step_q4;
  convolve_avg_horiz(src, src_stride, dst, dst_stride, filter, x0_q4, x_step_q4,
                     w, h);
}

void vpx_convolve8_vert_c(const uint8_t *src, ptrdiff_t src_stride,
                          uint8_t *dst, ptrdiff_t dst_stride,
                          const InterpKernel *filter, int x0_q4, int x_step_q4,
                          int y0_q4, int y_step_q4, int w, int h) {
  (void)x0_q4;
  (void)x_step_q4;
  convolve_vert(src, src_stride, dst, dst_stride, filter, y0_q4, y_step_q4, w,
                h);
}

void vpx_convolve8_avg_vert_c(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const InterpKernel *filter, int x0_q4,
                              int x_step_q4, int y0_q4, int y_step_q4, int w,
                              int h) {
  (void)x0_q4;
  (void)x_step_q4;
  convolve_avg_vert(src, src_stride, dst, dst_stride, filter, y0_q4, y_step_q4,
                    w, h);
}

void vpx_convolve8_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                     ptrdiff_t dst_stride, const InterpKernel *filter,
                     int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w,
                     int h) {
  // Note: Fixed size intermediate buffer, temp, places limits on parameters.
  // 2d filtering proceeds in 2 steps:
  //   (1) Interpolate horizontally into an intermediate buffer, temp.
  //   (2) Interpolate temp vertically to derive the sub-pixel result.
  // Deriving the maximum number of rows in the temp buffer (135):
  // --Smallest scaling factor is x1/2 ==> y_step_q4 = 32 (Normative).
  // --Largest block size is 64x64 pixels.
  // --64 rows in the downscaled frame span a distance of (64 - 1) * 32 in the
  //   original frame (in 1/16th pixel units).
  // --Must round-up because block may be located at sub-pixel position.
  // --Require an additional SUBPEL_TAPS rows for the 8-tap filter tails.
  // --((64 - 1) * 32 + 15) >> 4 + 8 = 135.
  // When calling in frame scaling function, the smallest scaling factor is x1/4
  // ==> y_step_q4 = 64. Since w and h are at most 16, the temp buffer is still
  // big enough.
  uint8_t temp[64 * 135];
  const int intermediate_height =
      (((h - 1) * y_step_q4 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

  assert(w <= 64);
  assert(h <= 64);
  assert(y_step_q4 <= 32 || (y_step_q4 <= 64 && h <= 32));
  assert(x_step_q4 <= 64);

  convolve_horiz(src - src_stride * (SUBPEL_TAPS / 2 - 1), src_stride, temp, 64,
                 filter, x0_q4, x_step_q4, w, intermediate_height);
  convolve_vert(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst, dst_stride, filter,
                y0_q4, y_step_q4, w, h);
}

void vpx_convolve8_residual_c(const int16_t *src, ptrdiff_t src_stride, int16_t *dst,
                     ptrdiff_t dst_stride, const InterpKernel *filter,
                     int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w,
                     int h) {
    // Note: Fixed size intermediate buffer, temp, places limits on parameters.
    // 2d filtering proceeds in 2 steps:
    //   (1) Interpolate horizontally into an intermediate buffer, temp.
    //   (2) Interpolate temp vertically to derive the sub-pixel result.
    // Deriving the maximum number of rows in the temp buffer (135):
    // --Smallest scaling factor is x1/2 ==> y_step_q4 = 32 (Normative).
    // --Largest block size is 64x64 pixels.
    // --64 rows in the downscaled frame span a distance of (64 - 1) * 32 in the
    //   original frame (in 1/16th pixel units).
    // --Must round-up because block may be located at sub-pixel position.
    // --Require an additional SUBPEL_TAPS rows for the 8-tap filter tails.
    // --((64 - 1) * 32 + 15) >> 4 + 8 = 135.
    // When calling in frame scaling function, the smallest scaling factor is x1/4
    // ==> y_step_q4 = 64. Since w and h are at most 16, the temp buffer is still
    // big enough.
    LOGD("vpx_connvolve8_residual_c is called");
    int16_t temp[64 * 135];
    const int intermediate_height =
            (((h - 1) * y_step_q4 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

    assert(w <= 64);
    assert(h <= 64);
    assert(y_step_q4 <= 32 || (y_step_q4 <= 64 && h <= 32));
    assert(x_step_q4 <= 64);

    convolve_horiz_residual(src - src_stride * (SUBPEL_TAPS / 2 - 1), src_stride, temp, 64,
                            filter, x0_q4, x_step_q4, w, intermediate_height);
    convolve_vert_residual(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst, dst_stride, filter,
                           y0_q4, y_step_q4, w, h);
//    convolve_horiz(src - src_stride * (SUBPEL_TAPS / 2 - 1), src_stride, temp, 64,
//                   filter, x0_q4, x_step_q4, w, intermediate_height);
//    convolve_vert(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst, dst_stride, filter,
//                  y0_q4, y_step_q4, w, h);
}

void vpx_convolve8_avg_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                         ptrdiff_t dst_stride, const InterpKernel *filter,
                         int x0_q4, int x_step_q4, int y0_q4, int y_step_q4,
                         int w, int h) {
  // Fixed size intermediate buffer places limits on parameters.
  DECLARE_ALIGNED(16, uint8_t, temp[64 * 64]);
  assert(w <= 64);
  assert(h <= 64);

  vpx_convolve8_c(src, src_stride, temp, 64, filter, x0_q4, x_step_q4, y0_q4,
                  y_step_q4, w, h);
  vpx_convolve_avg_c(temp, 64, dst, dst_stride, NULL, 0, 0, 0, 0, w, h);
}

void vpx_convolve8_add_c(const int16_t *src, ptrdiff_t src_stride, uint8_t *dst,
                         ptrdiff_t dst_stride, const InterpKernel *filter,
                         int x0_q4, int x_step_q4, int y0_q4, int y_step_q4,
                         int w, int h) {
  // Fixed size intermediate buffer places limits on parameters.
  DECLARE_ALIGNED(16, int16_t, temp[64 * 64]);
  assert(w <= 64);
  assert(h <= 64);

//  LOGD("vpx_convolve8_add_c is called");
  vpx_convolve8_residual_c(src, src_stride, temp, 64, filter, x0_q4, x_step_q4, y0_q4,
                  y_step_q4, w, h);
  vpx_convolve_copy_add_c(temp, 64, dst, dst_stride, NULL, 0, 0, 0, 0, w, h);
}

void vpx_convolve_copy_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                         ptrdiff_t dst_stride, const InterpKernel *filter,
                         int x0_q4, int x_step_q4, int y0_q4, int y_step_q4,
                         int w, int h) {
  int r;

  (void)filter;
  (void)x0_q4;
  (void)x_step_q4;
  (void)y0_q4;
  (void)y_step_q4;

  for (r = h; r > 0; --r) {
    memcpy(dst, src, w);
    src += src_stride;
    dst += dst_stride;
  }
}

void vpx_convolve_avg_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                        ptrdiff_t dst_stride, const InterpKernel *filter,
                        int x0_q4, int x_step_q4, int y0_q4, int y_step_q4,
                        int w, int h) {
  int x, y;

  (void)filter;
  (void)x0_q4;
  (void)x_step_q4;
  (void)y0_q4;
  (void)y_step_q4;

  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) dst[x] = ROUND_POWER_OF_TWO(dst[x] + src[x], 1);
    src += src_stride;
    dst += dst_stride;
  }
}

void vpx_bilinear_interp_add(const int16_t *src, ptrdiff_t src_stride, uint8_t *dst,
                             ptrdiff_t dst_stride, const InterpKernel *filter,
                             int x0_q4, int x_step_q4, int y0_q4, int y_step_q4,
                             int w, int h) {
    int x, y;

    (void)filter;
    (void)x0_q4;
    (void)y0_q4;

    double x_scale = x_step_q4;
    double y_scale = y_step_q4;

    //LOGD("x_scale: %lf, y_scale: %lf", x_scale, y_scale);

    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            int px = (int)(x / x_scale);
            int py = (int)(y / y_scale);

            double fx1 = (double)x/x_scale - (double)px;
            double fx2 = 1 - fx1;
            double fy1 = (double)y/y_scale - (double)py;
            double fy2 = 1 - fy1;

            double w1 = fx2 * fy2;
            double w2 = fx1 * fy2;
            double w3 = fx2 * fy1;
            double w4 = fx1 * fy1;

            int sum = w1 * src[py * src_stride + px] +
                    w2 * src[py * src_stride + (px + 1)] +
                    w3 * src[(py + 1) * src_stride + px] +
                    w4 * src[(py + 1) * src_stride + (px + 1)]; //TODO (hyunho): performance bottleneck 1 - replace by fix-point

            dst[y * dst_stride + x] = clip_pixel(dst[y * dst_stride + x] + sum); //TODO (hyunho): performance bottleneck 2 - efficient clipping (?) - why?

        }
    }

//    for (y = 0; y < h; ++y) {
//        for (x = 0; x < w; ++x) {
//            int px = (int)(x / x_scale);
//            int py = (int)(y / y_scale);
//
//            float fx1 = (float)x/x_scale - px;
//            float fx2 = 1 - fx1;
//            float fy1 = (float)y/y_scale - py;
//            float fy2 = 1 - fy1;
//
//            int w1 = fx2 * fy2 * 256.0f;
//            int w2 = fx1 * fy2 * 256.0f;
//            int w3 = fx2 * fy1 * 256.0f;
//            int w4 = fx1 * fy1 * 256.0f;
//
//            int sum = w1 * src[py * src_stride + px] +
//                    w2 * src[py * src_stride + (px + 1)] +
//                    w3 * src[(py + 1) * src_stride + px] +
//                    w4 * src[(py + 1) * src_stride + (px + 1)]; //TODO (hyunho): performance bottleneck 1 - replace by fix-point
//
//            dst[y * dst_stride + x] = clip_pixel(dst[y * dst_stride + x] + (sum >> 8)); //TODO (hyunho): performance bottleneck 2 - efficient clipping (?) - why?
//
//        }
//    }

//    for (y = 0; y < h; ++y) {
//        for (x = 0; x < w; ++x) {
//            int px = (int)(x / x_scale);
//            int py = (int)(y / y_scale);
//
//            float fx1 = (float)x/x_scale - px;
//            float fx2 = 1 - fx1;
//            float fy1 = (float)y/y_scale - py;
//            float fy2 = 1 - fy1;
//
//            int w1 = fx2 * fy2 * 256.0f;
//            int w2 = fx1 * fy2 * 256.0f;
//            int w3 = fx2 * fy1 * 256.0f;
//            int w4 = fx1 * fy1 * 256.0f;
//
//            int sum = w1 * src[py * src_stride + px] +
//                    w2 * src[py * src_stride + (px + 1)] +
//                    w3 * src[(py + 1) * src_stride + px] +
//                    w4 * src[(py + 1) * src_stride + (px + 1)]; //TODO (hyunho): save this on memory and apply neon instructions
//
//            dst[y * dst_stride + x] = clip_pixel(dst[y * dst_stride + x] + (sum >> 8)); //TODO (hyunho): performance bottleneck 2 - efficient clipping (?) - why?

//            if (x % 16 == 0) {
//                LOGD("x: %d", x);
//                uint8x16_t s0 = vld1q_u8(src);
//                vst1q_u8(dst, s0);
//                vst1q_u8(dst, s0);
//            }

            //sum = sum >> 8;
//            dst[y * dst_stride + x] = sum; // Performance bottleneck is happening value saving

//            dst[y * dst_stride + x] = sum >> 8;
//
//            dst[y * dst_stride + x] = clip_pixel(dst[y * dst_stride + x] + sum); //TODO (hyunho): performance bottleneck 2 - efficient clipping (?) - why?

//        }
//    }
}

void vpx_bilinear_interp(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                             ptrdiff_t dst_stride, const InterpKernel *filter,
                             int x0_q4, int x_step_q4, int y0_q4, int y_step_q4,
                             int w, int h) {
    int x, y;

    (void)filter;
    (void)x0_q4;
    (void)y0_q4;

    double x_scale = x_step_q4;
    double y_scale = y_step_q4;

    //LOGD("x_scale: %lf, y_scale: %lf", x_scale, y_scale);

    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            int px = (int)(x / x_scale);
            int py = (int)(y / y_scale);

            double fx1 = (double)x/x_scale - (double)px;
            double fx2 = 1 - fx1;
            double fy1 = (double)y/y_scale - (double)py;
            double fy2 = 1 - fy1;

            double w1 = fx2 * fy2;
            double w2 = fx1 * fy2;
            double w3 = fx2 * fy1;
            double w4 = fx1 * fy1;

            int sum = w1 * src[py * src_stride + px] +
                      w2 * src[py * src_stride + (px + 1)] +
                      w3 * src[(py + 1) * src_stride + px] +
                      w4 * src[(py + 1) * src_stride + (px + 1)];

            dst[y * dst_stride + x] = clip_pixel(sum);
        }
    }
}


void vpx_convolve_copy_add_c(const int16_t *src, ptrdiff_t src_stride, uint8_t *dst,
                        ptrdiff_t dst_stride, const InterpKernel *filter,
                        int x0_q4, int x_step_q4, int y0_q4, int y_step_q4,
                        int w, int h) {
  int x, y;

  (void)filter;
  (void)x0_q4;
  (void)x_step_q4;
  (void)y0_q4;
  (void)y_step_q4;

  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) {
        dst[x] = clip_pixel(dst[x] + src[x]);
        //dst[x] = dst[x];
        //dst[x] = 0;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

void vpx_convolve_add_c(const int16_t *src, ptrdiff_t src_stride, uint8_t *dst,
                             ptrdiff_t dst_stride, const InterpKernel *filter,
                             int x0_q4, int x_step_q4, int y0_q4, int y_step_q4,
                             int w, int h) {
    int x, y;

    (void)filter;
    (void)x0_q4;
    (void)x_step_q4;
    (void)y0_q4;
    (void)y_step_q4;

    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) dst[x] = clip_pixel(dst[x] + src[x]);
        src += src_stride;
        dst += dst_stride;
    }
}

void vpx_scaled_horiz_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                        ptrdiff_t dst_stride, const InterpKernel *filter,
                        int x0_q4, int x_step_q4, int y0_q4, int y_step_q4,
                        int w, int h) {
  vpx_convolve8_horiz_c(src, src_stride, dst, dst_stride, filter, x0_q4,
                        x_step_q4, y0_q4, y_step_q4, w, h);
}

void vpx_scaled_vert_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                       ptrdiff_t dst_stride, const InterpKernel *filter,
                       int x0_q4, int x_step_q4, int y0_q4, int y_step_q4,
                       int w, int h) {
  vpx_convolve8_vert_c(src, src_stride, dst, dst_stride, filter, x0_q4,
                       x_step_q4, y0_q4, y_step_q4, w, h);
}

void vpx_scaled_2d_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                     ptrdiff_t dst_stride, const InterpKernel *filter,
                     int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w,
                     int h) {
  vpx_convolve8_c(src, src_stride, dst, dst_stride, filter, x0_q4, x_step_q4,
                  y0_q4, y_step_q4, w, h);
}

void vpx_scaled_avg_horiz_c(const uint8_t *src, ptrdiff_t src_stride,
                            uint8_t *dst, ptrdiff_t dst_stride,
                            const InterpKernel *filter, int x0_q4,
                            int x_step_q4, int y0_q4, int y_step_q4, int w,
                            int h) {
  vpx_convolve8_avg_horiz_c(src, src_stride, dst, dst_stride, filter, x0_q4,
                            x_step_q4, y0_q4, y_step_q4, w, h);
}

void vpx_scaled_avg_vert_c(const uint8_t *src, ptrdiff_t src_stride,
                           uint8_t *dst, ptrdiff_t dst_stride,
                           const InterpKernel *filter, int x0_q4, int x_step_q4,
                           int y0_q4, int y_step_q4, int w, int h) {
  vpx_convolve8_avg_vert_c(src, src_stride, dst, dst_stride, filter, x0_q4,
                           x_step_q4, y0_q4, y_step_q4, w, h);
}

void vpx_scaled_avg_2d_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                         ptrdiff_t dst_stride, const InterpKernel *filter,
                         int x0_q4, int x_step_q4, int y0_q4, int y_step_q4,
                         int w, int h) {
  vpx_convolve8_avg_c(src, src_stride, dst, dst_stride, filter, x0_q4,
                      x_step_q4, y0_q4, y_step_q4, w, h);
}

void vpx_scaled_add_2d_c(const int16_t *src, ptrdiff_t src_stride, uint8_t *dst,
                         ptrdiff_t dst_stride, const InterpKernel *filter,
                         int x0_q4, int x_step_q4, int y0_q4, int y_step_q4,
                         int w, int h) {
  vpx_convolve8_add_c(src, src_stride, dst, dst_stride, filter, x0_q4,
                      x_step_q4, y0_q4, y_step_q4, w, h);
}

#if CONFIG_VP9_HIGHBITDEPTH
static void highbd_convolve_horiz(const uint16_t *src, ptrdiff_t src_stride,
                                  uint16_t *dst, ptrdiff_t dst_stride,
                                  const InterpKernel *x_filters, int x0_q4,
                                  int x_step_q4, int w, int h, int bd) {
  int x, y;
  src -= SUBPEL_TAPS / 2 - 1;

  for (y = 0; y < h; ++y) {
    int x_q4 = x0_q4;
    for (x = 0; x < w; ++x) {
      const uint16_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
      dst[x] = clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void highbd_convolve_avg_horiz(const uint16_t *src, ptrdiff_t src_stride,
                                      uint16_t *dst, ptrdiff_t dst_stride,
                                      const InterpKernel *x_filters, int x0_q4,
                                      int x_step_q4, int w, int h, int bd) {
  int x, y;
  src -= SUBPEL_TAPS / 2 - 1;

  for (y = 0; y < h; ++y) {
    int x_q4 = x0_q4;
    for (x = 0; x < w; ++x) {
      const uint16_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
      dst[x] = ROUND_POWER_OF_TWO(
          dst[x] + clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd),
          1);
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void highbd_convolve_vert(const uint16_t *src, ptrdiff_t src_stride,
                                 uint16_t *dst, ptrdiff_t dst_stride,
                                 const InterpKernel *y_filters, int y0_q4,
                                 int y_step_q4, int w, int h, int bd) {
  int x, y;
  src -= src_stride * (SUBPEL_TAPS / 2 - 1);

  for (x = 0; x < w; ++x) {
    int y_q4 = y0_q4;
    for (y = 0; y < h; ++y) {
      const uint16_t *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k)
        sum += src_y[k * src_stride] * y_filter[k];
      dst[y * dst_stride] =
          clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

static void highbd_convolve_avg_vert(const uint16_t *src, ptrdiff_t src_stride,
                                     uint16_t *dst, ptrdiff_t dst_stride,
                                     const InterpKernel *y_filters, int y0_q4,
                                     int y_step_q4, int w, int h, int bd) {
  int x, y;
  src -= src_stride * (SUBPEL_TAPS / 2 - 1);

  for (x = 0; x < w; ++x) {
    int y_q4 = y0_q4;
    for (y = 0; y < h; ++y) {
      const uint16_t *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
      int k, sum = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k)
        sum += src_y[k * src_stride] * y_filter[k];
      dst[y * dst_stride] = ROUND_POWER_OF_TWO(
          dst[y * dst_stride] +
              clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd),
          1);
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

static void highbd_convolve(const uint16_t *src, ptrdiff_t src_stride,
                            uint16_t *dst, ptrdiff_t dst_stride,
                            const InterpKernel *filter, int x0_q4,
                            int x_step_q4, int y0_q4, int y_step_q4, int w,
                            int h, int bd) {
  // Note: Fixed size intermediate buffer, temp, places limits on parameters.
  // 2d filtering proceeds in 2 steps:
  //   (1) Interpolate horizontally into an intermediate buffer, temp.
  //   (2) Interpolate temp vertically to derive the sub-pixel result.
  // Deriving the maximum number of rows in the temp buffer (135):
  // --Smallest scaling factor is x1/2 ==> y_step_q4 = 32 (Normative).
  // --Largest block size is 64x64 pixels.
  // --64 rows in the downscaled frame span a distance of (64 - 1) * 32 in the
  //   original frame (in 1/16th pixel units).
  // --Must round-up because block may be located at sub-pixel position.
  // --Require an additional SUBPEL_TAPS rows for the 8-tap filter tails.
  // --((64 - 1) * 32 + 15) >> 4 + 8 = 135.
  uint16_t temp[64 * 135];
  const int intermediate_height =
      (((h - 1) * y_step_q4 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

  assert(w <= 64);
  assert(h <= 64);
  assert(y_step_q4 <= 32);
  assert(x_step_q4 <= 32);

  highbd_convolve_horiz(src - src_stride * (SUBPEL_TAPS / 2 - 1), src_stride,
                        temp, 64, filter, x0_q4, x_step_q4, w,
                        intermediate_height, bd);
  highbd_convolve_vert(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst, dst_stride,
                       filter, y0_q4, y_step_q4, w, h, bd);
}

void vpx_highbd_convolve8_horiz_c(const uint16_t *src, ptrdiff_t src_stride,
                                  uint16_t *dst, ptrdiff_t dst_stride,
                                  const InterpKernel *filter, int x0_q4,
                                  int x_step_q4, int y0_q4, int y_step_q4,
                                  int w, int h, int bd) {
  (void)y0_q4;
  (void)y_step_q4;

  highbd_convolve_horiz(src, src_stride, dst, dst_stride, filter, x0_q4,
                        x_step_q4, w, h, bd);
}

void vpx_highbd_convolve8_avg_horiz_c(const uint16_t *src, ptrdiff_t src_stride,
                                      uint16_t *dst, ptrdiff_t dst_stride,
                                      const InterpKernel *filter, int x0_q4,
                                      int x_step_q4, int y0_q4, int y_step_q4,
                                      int w, int h, int bd) {
  (void)y0_q4;
  (void)y_step_q4;

  highbd_convolve_avg_horiz(src, src_stride, dst, dst_stride, filter, x0_q4,
                            x_step_q4, w, h, bd);
}

void vpx_highbd_convolve8_vert_c(const uint16_t *src, ptrdiff_t src_stride,
                                 uint16_t *dst, ptrdiff_t dst_stride,
                                 const InterpKernel *filter, int x0_q4,
                                 int x_step_q4, int y0_q4, int y_step_q4, int w,
                                 int h, int bd) {
  (void)x0_q4;
  (void)x_step_q4;

  highbd_convolve_vert(src, src_stride, dst, dst_stride, filter, y0_q4,
                       y_step_q4, w, h, bd);
}

void vpx_highbd_convolve8_avg_vert_c(const uint16_t *src, ptrdiff_t src_stride,
                                     uint16_t *dst, ptrdiff_t dst_stride,
                                     const InterpKernel *filter, int x0_q4,
                                     int x_step_q4, int y0_q4, int y_step_q4,
                                     int w, int h, int bd) {
  (void)x0_q4;
  (void)x_step_q4;

  highbd_convolve_avg_vert(src, src_stride, dst, dst_stride, filter, y0_q4,
                           y_step_q4, w, h, bd);
}

void vpx_highbd_convolve8_c(const uint16_t *src, ptrdiff_t src_stride,
                            uint16_t *dst, ptrdiff_t dst_stride,
                            const InterpKernel *filter, int x0_q4,
                            int x_step_q4, int y0_q4, int y_step_q4, int w,
                            int h, int bd) {
  highbd_convolve(src, src_stride, dst, dst_stride, filter, x0_q4, x_step_q4,
                  y0_q4, y_step_q4, w, h, bd);
}

void vpx_highbd_convolve8_avg_c(const uint16_t *src, ptrdiff_t src_stride,
                                uint16_t *dst, ptrdiff_t dst_stride,
                                const InterpKernel *filter, int x0_q4,
                                int x_step_q4, int y0_q4, int y_step_q4, int w,
                                int h, int bd) {
  // Fixed size intermediate buffer places limits on parameters.
  DECLARE_ALIGNED(16, uint16_t, temp[64 * 64]);
  assert(w <= 64);
  assert(h <= 64);

  vpx_highbd_convolve8_c(src, src_stride, temp, 64, filter, x0_q4, x_step_q4,
                         y0_q4, y_step_q4, w, h, bd);
  vpx_highbd_convolve_avg_c(temp, 64, dst, dst_stride, NULL, 0, 0, 0, 0, w, h,
                            bd);
}

void vpx_highbd_convolve_copy_c(const uint16_t *src, ptrdiff_t src_stride,
                                uint16_t *dst, ptrdiff_t dst_stride,
                                const InterpKernel *filter, int x0_q4,
                                int x_step_q4, int y0_q4, int y_step_q4, int w,
                                int h, int bd) {
  int r;

  (void)filter;
  (void)x0_q4;
  (void)x_step_q4;
  (void)y0_q4;
  (void)y_step_q4;
  (void)bd;

  for (r = h; r > 0; --r) {
    memcpy(dst, src, w * sizeof(uint16_t));
    src += src_stride;
    dst += dst_stride;
  }
}

void vpx_highbd_convolve_avg_c(const uint16_t *src, ptrdiff_t src_stride,
                               uint16_t *dst, ptrdiff_t dst_stride,
                               const InterpKernel *filter, int x0_q4,
                               int x_step_q4, int y0_q4, int y_step_q4, int w,
                               int h, int bd) {
  int x, y;

  (void)filter;
  (void)x0_q4;
  (void)x_step_q4;
  (void)y0_q4;
  (void)y_step_q4;
  (void)bd;

  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) dst[x] = ROUND_POWER_OF_TWO(dst[x] + src[x], 1);
    src += src_stride;
    dst += dst_stride;
  }
}
#endif
