//
// Created by hyunho on 8/17/19.
// reference: ARM ne10 - void ne10_img_vresize_linear_neon (const int** src, unsigned char* dst, const short* beta, int width)
//
#include <arm_neon.h>
#include <assert.h>

#include "vpx_dsp/arm/vpx_bilinear_interp_neon.h"
#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "vpx/vpx_integer.h"
#include "vpx_ports/mem.h"
#include "../../vp9/common/vp9_onyxc_int.h"
#include "../vpx_dsp_common.h"

#define FRACTION_BIT (5)
#define FRACTION_SCALE (1 << FRACTION_BIT)
static const int16_t delta = (1 << (FRACTION_BIT - 1));

//TODO (hyunho): add to these files to a build script
//TODO (hyunho): write a pseudo code

//#define TX_4X4 ((TX_SIZE)0)    // 4x4 transform     // 2x2 -- 4x4, 6x6, 8x8 (UV plane의 경우)
//#define TX_8X8 ((TX_SIZE)1)    // 8x8 transform
//#define TX_16X16 ((TX_SIZE)2)  // 16x16 transform
//#define TX_32X32 ((TX_SIZE)3)  // 32x32 transform

//8개씩 처리하니 8, 16을 만들어두면 된다.
//8, 16이상인 경우 for loop을 돌면서, 마지막에 남은 것 처리하는 형식으로 만들자.
//우선 C코드로 neon을 제외한 모든 구현을 해보자. (fixed point precision, neon)
//그 다음 neon으로 vectorize를 진행하자.
//ne10을 참조하자.

static void vpx_bilinear_interp_horiz_neon_h8_int16(const int16_t *src, ptrdiff_t src_stride, int16_t *dst,  ptrdiff_t dst_stride, int width, int height, int scale, const bilinear_config_t *config)
{

}

static void vpx_bilinear_interp_horiz_neon_h16_int16(const int16_t *src, ptrdiff_t src_stride, int16_t *dst,  ptrdiff_t dst_stride, int width, int height, int scale, const bilinear_config_t *config)
{
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


static void vpx_bilinear_interp_vert_neon_w8_int16(const int16_t *src, ptrdiff_t src_stride, uint8_t *dst,  ptrdiff_t dst_stride, int width, int height, int scale, const bilinear_config_t *config)
{

}

static void vpx_bilinear_interp_vert_neon_w16_int16(const int16_t *src, ptrdiff_t src_stride, uint8_t *dst,  ptrdiff_t dst_stride, int width, int height, int scale, const bilinear_config_t *config)
{
    int x, y;

    int16x8_t qS0_01234567_0, qS0_01234567_1, qS1_01234567_0, qS1_01234567_1;
    int16x8_t qT_01234567_0, qT_01234567_1;
    uint8x8_t dDst_01234567_0, dDst_01234567_1;
    uint16x8_t dT0_01234567_0, dT0_01234567_1;
    int16x8_t y_lerp_fixed;

    for (y = 0; y < height * scale; ++y) {
        x = 0;
        const int top_y_index = config->top_y_index[y];
        const int bottom_y_index = config->bottom_y_index[y];
        y_lerp_fixed = vdupq_n_s16(config->y_lerp_fixed[y]);

        //1. process 8 values * 2 loop unrolling == 16
        for (; x <= width * scale - 16; x += 16) {
            //a. load source pixels: top0,1, bottom0,1
            qS0_01234567_0 = vld1q_s16(&src[top_y_index * src_stride + x]);
            qS0_01234567_1 = vld1q_s16(&src[top_y_index * src_stride + x + 8]);
            qS1_01234567_0 = vld1q_s16(&src[bottom_y_index * src_stride + x]);
            qS1_01234567_1 = vld1q_s16(&src[bottom_y_index * src_stride + x + 8]);

            //b. interpolate pixels
            qT_01234567_0 = vsubq_s16(qS1_01234567_0, qS0_01234567_0);
            qT_01234567_1 = vsubq_s16(qS1_01234567_1, qS0_01234567_1);
            qT_01234567_0 = vmulq_s16(qT_01234567_0, y_lerp_fixed);
            qT_01234567_1 = vmulq_s16(qT_01234567_1, y_lerp_fixed);
            qT_01234567_0 = vrsraq_n_s16(qS0_01234567_0, qT_01234567_0, FRACTION_BIT);
            qT_01234567_1 = vrsraq_n_s16(qS0_01234567_1, qT_01234567_1, FRACTION_BIT);

            //c. load & add destination pixels
            //d. clip pixels
            //e. convert from in16 to uint8
            dDst_01234567_0 = vld1_u8(&dst[y * dst_stride + x]);
            dDst_01234567_1 = vld1_u8(&dst[y * dst_stride + x + 8]);
            dT0_01234567_0 = vaddw_u8(vreinterpretq_u16_s16(qT_01234567_0), dDst_01234567_0);
            dT0_01234567_1 = vaddw_u8(vreinterpretq_u16_s16(qT_01234567_1), dDst_01234567_1);
            dDst_01234567_0 = vqmovun_s16(vreinterpretq_s16_u16(dT0_01234567_0));
            dDst_01234567_1 = vqmovun_s16(vreinterpretq_s16_u16(dT0_01234567_1));

            //f. save pixels
            vst1_u8(&dst[y * dst_stride + x], dDst_01234567_0);
            vst1_u8(&dst[y * dst_stride + x + 8], dDst_01234567_1);
        }

        //2. process remaining 8 values
        //TODO (hyunho) - need quality & latency test for scale x3
//        assert (x >= width * scale); //TODO: remove
//        if (x < width * scale) {
//
//        }
    }
}

void vpx_bilinear_interp_neon_int16(const int16_t *src, ptrdiff_t src_stride, uint8_t *dst,  ptrdiff_t dst_stride, int x_offset, int y_offset, int width, int height, int scale, const bilinear_config_t *config)
{
    int16_t temp[128 * 128];
    int w = width * scale;
    int h = height * scale;

    assert(width <= 32);
    assert(height <= 32);
    assert(scale <= 4 && scale >= 2);

    src = src + (y_offset * src_stride + x_offset);
    dst = dst + (y_offset * dst_stride + x_offset) * scale;

    assert (w >= 16 && h >= 16); //TODO: remove
    if (w >= 16) {
        vpx_bilinear_interp_horiz_neon_h16_int16(src, src_stride, temp, 128, width, height, scale, config);
    }
    else {
    }

    if (h >= 16) {
        vpx_bilinear_interp_vert_neon_w16_int16(temp, 128, dst, dst_stride, width, height, scale, config);
    }
    else {
    }
}




