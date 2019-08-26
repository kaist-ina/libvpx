/*
 *  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VPX_DSP_VPX_DSP_COMMON_H_
#define VPX_DSP_VPX_DSP_COMMON_H_

#include <assert.h>
#include "./vpx_config.h"
#include "vpx/vpx_integer.h"
#include "vpx_ports/mem.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VPXMIN(x, y) (((x) < (y)) ? (x) : (y))
#define VPXMAX(x, y) (((x) > (y)) ? (x) : (y))

#define VPX_SWAP(type, a, b) \
  do {                       \
    type c = (b);            \
    b = a;                   \
    a = c;                   \
  } while (0)

#if CONFIG_VP9_HIGHBITDEPTH
// Note:
// tran_low_t  is the datatype used for final transform coefficients.
// tran_high_t is the datatype used for intermediate transform stages.
typedef int64_t tran_high_t;
typedef int32_t tran_low_t;
#else
// Note:
// tran_low_t  is the datatype used for final transform coefficients.
// tran_high_t is the datatype used for intermediate transform stages.
typedef int32_t tran_high_t;
typedef int16_t tran_low_t;
#endif  // CONFIG_VP9_HIGHBITDEPTH

typedef int16_t tran_coef_t;

static INLINE uint8_t clip_pixel(int val) {
  return (val > 255) ? 255 : (val < 0) ? 0 : val;
}

static INLINE int16_t clip_pixel_int16(int32_t val) {
  return (val > INT16_MAX) ? INT16_MAX : (val < INT16_MIN) ? INT16_MIN : val;
}

static INLINE int clamp(int value, int low, int high) {
  return value < low ? low : (value > high ? high : value);
}

static INLINE double fclamp(double value, double low, double high) {
  return value < low ? low : (value > high ? high : value);
}

static INLINE uint16_t clip_pixel_highbd(int val, int bd) {
  switch (bd) {
    case 8:
    default: return (uint16_t)clamp(val, 0, 255);
    case 10: return (uint16_t)clamp(val, 0, 1023);
    case 12: return (uint16_t)clamp(val, 0, 4095);
  }
}

/*******************Hyunho************************/
typedef struct BilinearConfig{
    float x_lerp[128];
    int16_t x_lerp_fixed[128];
    float y_lerp[128];
    int16_t y_lerp_fixed[128];
    int top_y_index[128];
    int bottom_y_index[128];
    int left_x_index[128];
    int right_x_index[128];
} bilinear_config_t;

typedef struct BilinearProfile{
    //scale x4
    bilinear_config_t config_TX_4X4_s4;
    bilinear_config_t config_TX_8X8_s4;
    bilinear_config_t config_TX_16X16_s4;
    bilinear_config_t config_TX_32X32_s4;

    //scale x3
    bilinear_config_t config_TX_4X4_s3;
    bilinear_config_t config_TX_8X8_s3;
    bilinear_config_t config_TX_16X16_s3;
    bilinear_config_t config_TX_32X32_s3;

    //scale x2
    bilinear_config_t config_TX_4X4_s2;
    bilinear_config_t config_TX_8X8_s2;
    bilinear_config_t config_TX_16X16_s2;
    bilinear_config_t config_TX_32X32_s2;
} bilinear_profile_t;

static INLINE bilinear_config_t *get_bilinear_config(bilinear_profile_t *bilinear_profile, int scale, int size) {
  assert(scale == 4 || scale ==3 || scale ==2);
  assert(size == 32 || size ==16 || size == 8 || size == 4);

  switch (scale) {
    case 2:
      switch(size) {
        case 4:
          return &bilinear_profile->config_TX_4X4_s2;
        case 8:
          return &bilinear_profile->config_TX_8X8_s2;
        case 16:
          return &bilinear_profile->config_TX_16X16_s2;
        case 32:
          return &bilinear_profile->config_TX_32X32_s2;
      }
    case 3:
      switch(size) {
        case 4:
          return &bilinear_profile->config_TX_4X4_s3;
        case 8:
          return &bilinear_profile->config_TX_8X8_s3;
        case 16:
          return &bilinear_profile->config_TX_16X16_s3;
        case 32:
          return &bilinear_profile->config_TX_32X32_s3;
      }
    case 4:
      switch(size) {
        case 4:
          return &bilinear_profile->config_TX_4X4_s4;
        case 8:
          return &bilinear_profile->config_TX_8X8_s4;
        case 16:
          return &bilinear_profile->config_TX_16X16_s4;
        case 32:
          return &bilinear_profile->config_TX_32X32_s4;
      }
  }
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VPX_DSP_VPX_DSP_COMMON_H_
