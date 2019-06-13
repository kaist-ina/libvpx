/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "./vpx_dsp_rtcd.h"
#include "vp9/common/vp9_filter.h"
#include "vp9/common/vp9_scale.h"
#include "vpx_dsp/vpx_filter.h"

#include <android/log.h>

#define TAG "vp9_scale.c JNI"
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

static INLINE int scaled_x(int val, const struct scale_factors *sf) {
    return (int) ((int64_t) val * sf->x_scale_fp >> REF_SCALE_SHIFT);
}

static INLINE int scaled_y(int val, const struct scale_factors *sf) {
    return (int) ((int64_t) val * sf->y_scale_fp >> REF_SCALE_SHIFT);
}

static int unscaled_value(int val, const struct scale_factors *sf) {
    (void) sf;
    return val;
}

static int get_fixed_point_scale_factor(int other_size, int this_size) {
    // Calculate scaling factor once for each reference frame
    // and use fixed point scaling factors in decoding and encoding routines.
    // Hardware implementations can calculate scale factor in device driver
    // and use multiplication and shifting on hardware instead of division.
    return (other_size << REF_SCALE_SHIFT) / this_size;
}

MV32 vp9_scale_mv(const MV *mv, int x, int y, const struct scale_factors *sf) {
    const int x_off_q4 = scaled_x(x << SUBPEL_BITS, sf) & SUBPEL_MASK;
    const int y_off_q4 = scaled_y(y << SUBPEL_BITS, sf) & SUBPEL_MASK;
    const MV32 res = {scaled_y(mv->row, sf) + y_off_q4,
                      scaled_x(mv->col, sf) + x_off_q4};

    //LOGD("mv->row: %d, y_off_q4: %d, res.row: %d", mv->row, y_off_q4, res.row);
    return res;
}

#if CONFIG_VP9_HIGHBITDEPTH
void vp9_setup_scale_factors_for_frame(struct scale_factors *sf, int other_w,
                                       int other_h, int this_w, int this_h,
                                       int use_highbd) {
#else

void vp9_setup_scale_factors_for_sr_frame(struct scale_factors *sf, int other_w,
                                          int other_h, int this_w, int this_h, bool upsample, bool add) {
#endif
    //TODO (Hyunho): how to check valid frame size?
    /*
    if (!valid_ref_frame_size(other_w, other_h, this_w, this_h)) {
        sf->x_scale_fp = REF_INVALID_SCALE;
        sf->y_scale_fp = REF_INVALID_SCALE;
        return;
    }
    */

    if (upsample) { //hyunho: both scale x_scale_fp, x_step_q4
        sf->x_scale_fp = get_fixed_point_scale_factor(this_w, other_w);
        sf->y_scale_fp = get_fixed_point_scale_factor(this_w, other_w);

        sf->x_step_q4 = scaled_x(16, sf);
        sf->y_step_q4 = scaled_y(16, sf);

        sf->x_scale_fp = get_fixed_point_scale_factor(other_w, this_w);
        sf->y_scale_fp = get_fixed_point_scale_factor(other_h, this_h);
    } else {
        sf->x_scale_fp = get_fixed_point_scale_factor(other_w, this_w);
        sf->y_scale_fp = get_fixed_point_scale_factor(other_h, this_h);

        sf->x_step_q4 = 16;
        sf->y_step_q4 = 16;
    }

    if (vp9_is_scaled(sf)) {
        sf->scale_value_x = scaled_x;
        sf->scale_value_y = scaled_y;
    } else {
        sf->scale_value_x = unscaled_value;
        sf->scale_value_y = unscaled_value;
    }

    // TODO(agrange): Investigate the best choice of functions to use here
    // for EIGHTTAP_SMOOTH. Since it is not interpolating, need to choose what
    // to do at full-pel offsets. The current selection, where the filter is
    // applied in one direction only, and not at all for 0,0, seems to give the
    // best quality, but it may be worth trying an additional mode that does
    // do the filtering on full-pel.

    if (sf->x_step_q4 == 16) {
        if (sf->y_step_q4 == 16) {
            // No scaling in either direction.
            sf->predict[0][0][0] = vpx_convolve_copy;
            sf->predict[0][0][1] = vpx_convolve_avg;
            sf->predict[0][1][0] = vpx_convolve8_vert;
            sf->predict[0][1][1] = vpx_convolve8_avg_vert;
            sf->predict[1][0][0] = vpx_convolve8_horiz;
            sf->predict[1][0][1] = vpx_convolve8_avg_horiz;
        } else {
            // No scaling in x direction. Must always scale in the y direction.
            sf->predict[0][0][0] = vpx_scaled_vert;
            sf->predict[0][0][1] = vpx_scaled_avg_vert;
            sf->predict[0][1][0] = vpx_scaled_vert;
            sf->predict[0][1][1] = vpx_scaled_avg_vert;
            sf->predict[1][0][0] = vpx_scaled_2d;
            sf->predict[1][0][1] = vpx_scaled_avg_2d;
        }
    } else {
        if (sf->y_step_q4 == 16) {
            // No scaling in the y direction. Must always scale in the x direction.
//            sf->predict[0][0][0] = vpx_scaled_horiz;
//            sf->predict[0][0][1] = vpx_scaled_avg_horiz;
//            sf->predict[0][1][0] = vpx_scaled_2d;
//            sf->predict[0][1][1] = vpx_scaled_avg_2d;
//            sf->predict[1][0][0] = vpx_scaled_horiz;
//            sf->predict[1][0][1] = vpx_scaled_avg_horiz;
        } else {
            // Must always scale in both directions. //TODO (hyunho): handle all case
            if (add) {
                sf->predict_residual[0][0][0] = vpx_bilinear_interp_add;
                sf->predict_residual[0][0][1] = vpx_bilinear_interp_add;
                sf->predict_residual[0][1][0] = vpx_bilinear_interp_add;
                sf->predict_residual[0][1][1] = vpx_bilinear_interp_add;
                sf->predict_residual[1][0][0] = vpx_bilinear_interp_add;
                sf->predict_residual[1][0][1] = vpx_bilinear_interp_add;
//                sf->predict[0][0][0] = vpx_scaled_2d;
//                sf->predict[0][0][1] = vpx_scaled_avg_2d;
//                sf->predict[0][1][0] = vpx_scaled_2d;
//                sf->predict[0][1][1] = vpx_scaled_avg_2d;
//                sf->predict[1][0][0] = vpx_scaled_2d;
//                sf->predict[1][0][1] = vpx_scaled_avg_2d;
            }
            else {
                sf->predict[0][0][0] = vpx_bilinear_interp;
                sf->predict[0][0][1] = vpx_bilinear_interp;
                sf->predict[0][1][0] = vpx_bilinear_interp;
                sf->predict[0][1][1] = vpx_bilinear_interp;
                sf->predict[1][0][0] = vpx_bilinear_interp;
                sf->predict[1][0][1] = vpx_bilinear_interp;
            }
        }
    }

    // 2D subpel motion always gets filtered in both directions

    if ((sf->x_step_q4 != 16) || (sf->y_step_q4 != 16)) {
        sf->predict[1][1][0] = vpx_scaled_2d;
        sf->predict[1][1][1] = vpx_scaled_avg_2d;
    } else {
        sf->predict[1][1][0] = vpx_convolve8;
        sf->predict[1][1][1] = vpx_convolve8_avg;
    }

#if CONFIG_VP9_HIGHBITDEPTH
    if (use_highbd) {
      if (sf->x_step_q4 == 16) {
        if (sf->y_step_q4 == 16) {
          // No scaling in either direction.
          sf->highbd_predict[0][0][0] = vpx_highbd_convolve_copy;
          sf->highbd_predict[0][0][1] = vpx_highbd_convolve_avg;
          sf->highbd_predict[0][1][0] = vpx_highbd_convolve8_vert;
          sf->highbd_predict[0][1][1] = vpx_highbd_convolve8_avg_vert;
          sf->highbd_predict[1][0][0] = vpx_highbd_convolve8_horiz;
          sf->highbd_predict[1][0][1] = vpx_highbd_convolve8_avg_horiz;
        } else {
          // No scaling in x direction. Must always scale in the y direction.
          sf->highbd_predict[0][0][0] = vpx_highbd_convolve8_vert;
          sf->highbd_predict[0][0][1] = vpx_highbd_convolve8_avg_vert;
          sf->highbd_predict[0][1][0] = vpx_highbd_convolve8_vert;
          sf->highbd_predict[0][1][1] = vpx_highbd_convolve8_avg_vert;
          sf->highbd_predict[1][0][0] = vpx_highbd_convolve8;
          sf->highbd_predict[1][0][1] = vpx_highbd_convolve8_avg;
        }
      } else {
        if (sf->y_step_q4 == 16) {
          // No scaling in the y direction. Must always scale in the x direction.
          sf->highbd_predict[0][0][0] = vpx_highbd_convolve8_horiz;
          sf->highbd_predict[0][0][1] = vpx_highbd_convolve8_avg_horiz;
          sf->highbd_predict[0][1][0] = vpx_highbd_convolve8;
          sf->highbd_predict[0][1][1] = vpx_highbd_convolve8_avg;
          sf->highbd_predict[1][0][0] = vpx_highbd_convolve8_horiz;
          sf->highbd_predict[1][0][1] = vpx_highbd_convolve8_avg_horiz;
        } else {
          // Must always scale in both directions.
          sf->highbd_predict[0][0][0] = vpx_highbd_convolve8;
          sf->highbd_predict[0][0][1] = vpx_highbd_convolve8_avg;
          sf->highbd_predict[0][1][0] = vpx_highbd_convolve8;
          sf->highbd_predict[0][1][1] = vpx_highbd_convolve8_avg;
          sf->highbd_predict[1][0][0] = vpx_highbd_convolve8;
          sf->highbd_predict[1][0][1] = vpx_highbd_convolve8_avg;
        }
      }
      // 2D subpel motion always gets filtered in both directions.
      sf->highbd_predict[1][1][0] = vpx_highbd_convolve8;
      sf->highbd_predict[1][1][1] = vpx_highbd_convolve8_avg;
    }
#endif
}


#if CONFIG_VP9_HIGHBITDEPTH
void vp9_setup_scale_factors_for_frame(struct scale_factors *sf, int other_w,
                                       int other_h, int this_w, int this_h,
                                       int use_highbd) {
#else

void vp9_setup_scale_factors_for_frame(struct scale_factors *sf, int other_w,
                                       int other_h, int this_w, int this_h) {
#endif
    if (!valid_ref_frame_size(other_w, other_h, this_w, this_h)) {
        sf->x_scale_fp = REF_INVALID_SCALE;
        sf->y_scale_fp = REF_INVALID_SCALE;
        return;
    }

    sf->x_scale_fp = get_fixed_point_scale_factor(other_w, this_w);
    sf->y_scale_fp = get_fixed_point_scale_factor(other_h, this_h);

    sf->x_step_q4 = scaled_x(16, sf);
    sf->y_step_q4 = scaled_y(16, sf);

    if (vp9_is_scaled(sf)) {
        sf->scale_value_x = scaled_x;
        sf->scale_value_y = scaled_y;
    } else {
        sf->scale_value_x = unscaled_value;
        sf->scale_value_y = unscaled_value;
    }

    // TODO(agrange): Investigate the best choice of functions to use here
    // for EIGHTTAP_SMOOTH. Since it is not interpolating, need to choose what
    // to do at full-pel offsets. The current selection, where the filter is
    // applied in one direction only, and not at all for 0,0, seems to give the
    // best quality, but it may be worth trying an additional mode that does
    // do the filtering on full-pel.

    if (sf->x_step_q4 == 16) {
        if (sf->y_step_q4 == 16) {
            // No scaling in either direction.
            sf->predict[0][0][0] = vpx_convolve_copy;
            sf->predict[0][0][1] = vpx_convolve_avg;
            sf->predict[0][1][0] = vpx_convolve8_vert;
            sf->predict[0][1][1] = vpx_convolve8_avg_vert;
            sf->predict[1][0][0] = vpx_convolve8_horiz;
            sf->predict[1][0][1] = vpx_convolve8_avg_horiz;
        } else {
            // No scaling in x direction. Must always scale in the y direction.
            sf->predict[0][0][0] = vpx_scaled_vert;
            sf->predict[0][0][1] = vpx_scaled_avg_vert;
            sf->predict[0][1][0] = vpx_scaled_vert;
            sf->predict[0][1][1] = vpx_scaled_avg_vert;
            sf->predict[1][0][0] = vpx_scaled_2d;
            sf->predict[1][0][1] = vpx_scaled_avg_2d;
        }
    } else {
        if (sf->y_step_q4 == 16) {
            // No scaling in the y direction. Must always scale in the x direction.
            sf->predict[0][0][0] = vpx_scaled_horiz;
            sf->predict[0][0][1] = vpx_scaled_avg_horiz;
            sf->predict[0][1][0] = vpx_scaled_2d;
            sf->predict[0][1][1] = vpx_scaled_avg_2d;
            sf->predict[1][0][0] = vpx_scaled_horiz;
            sf->predict[1][0][1] = vpx_scaled_avg_horiz;
        } else {
            // Must always scale in both directions.
            sf->predict[0][0][0] = vpx_scaled_2d;
            sf->predict[0][0][1] = vpx_scaled_avg_2d;
            sf->predict[0][1][0] = vpx_scaled_2d;
            sf->predict[0][1][1] = vpx_scaled_avg_2d;
            sf->predict[1][0][0] = vpx_scaled_2d;
            sf->predict[1][0][1] = vpx_scaled_avg_2d;
        }
    }

    // 2D subpel motion always gets filtered in both directions

    if ((sf->x_step_q4 != 16) || (sf->y_step_q4 != 16)) {
        sf->predict[1][1][0] = vpx_scaled_2d;
        sf->predict[1][1][1] = vpx_scaled_avg_2d;
    } else {
        sf->predict[1][1][0] = vpx_convolve8;
        sf->predict[1][1][1] = vpx_convolve8_avg;
    }

#if CONFIG_VP9_HIGHBITDEPTH
    if (use_highbd) {
      if (sf->x_step_q4 == 16) {
        if (sf->y_step_q4 == 16) {
          // No scaling in either direction.
          sf->highbd_predict[0][0][0] = vpx_highbd_convolve_copy;
          sf->highbd_predict[0][0][1] = vpx_highbd_convolve_avg;
          sf->highbd_predict[0][1][0] = vpx_highbd_convolve8_vert;
          sf->highbd_predict[0][1][1] = vpx_highbd_convolve8_avg_vert;
          sf->highbd_predict[1][0][0] = vpx_highbd_convolve8_horiz;
          sf->highbd_predict[1][0][1] = vpx_highbd_convolve8_avg_horiz;
        } else {
          // No scaling in x direction. Must always scale in the y direction.
          sf->highbd_predict[0][0][0] = vpx_highbd_convolve8_vert;
          sf->highbd_predict[0][0][1] = vpx_highbd_convolve8_avg_vert;
          sf->highbd_predict[0][1][0] = vpx_highbd_convolve8_vert;
          sf->highbd_predict[0][1][1] = vpx_highbd_convolve8_avg_vert;
          sf->highbd_predict[1][0][0] = vpx_highbd_convolve8;
          sf->highbd_predict[1][0][1] = vpx_highbd_convolve8_avg;
        }
      } else {
        if (sf->y_step_q4 == 16) {
          // No scaling in the y direction. Must always scale in the x direction.
          sf->highbd_predict[0][0][0] = vpx_highbd_convolve8_horiz;
          sf->highbd_predict[0][0][1] = vpx_highbd_convolve8_avg_horiz;
          sf->highbd_predict[0][1][0] = vpx_highbd_convolve8;
          sf->highbd_predict[0][1][1] = vpx_highbd_convolve8_avg;
          sf->highbd_predict[1][0][0] = vpx_highbd_convolve8_horiz;
          sf->highbd_predict[1][0][1] = vpx_highbd_convolve8_avg_horiz;
        } else {
          // Must always scale in both directions.
          sf->highbd_predict[0][0][0] = vpx_highbd_convolve8;
          sf->highbd_predict[0][0][1] = vpx_highbd_convolve8_avg;
          sf->highbd_predict[0][1][0] = vpx_highbd_convolve8;
          sf->highbd_predict[0][1][1] = vpx_highbd_convolve8_avg;
          sf->highbd_predict[1][0][0] = vpx_highbd_convolve8;
          sf->highbd_predict[1][0][1] = vpx_highbd_convolve8_avg;
        }
      }
      // 2D subpel motion always gets filtered in both directions.
      sf->highbd_predict[1][1][0] = vpx_highbd_convolve8;
      sf->highbd_predict[1][1][1] = vpx_highbd_convolve8_avg;
    }
#endif
}
#if CONFIG_VP9_HIGHBITDEPTH
void vp9_setup_scale_factors_for_frame(struct scale_factors *sf, int other_w,
                                       int other_h, int this_w, int this_h,
                                       int use_highbd) {
#else
void vp9_setup_scale_factors_for_tmp_frame(struct scale_factors *sf) {
#endif
    sf->x_scale_fp = 1 << REF_SCALE_SHIFT;
    sf->y_scale_fp = 1 << REF_SCALE_SHIFT;

    sf->x_step_q4 = 16;
    sf->y_step_q4 = 16;

    sf->scale_value_x = unscaled_value;
    sf->scale_value_y = unscaled_value;

    // TODO(agrange): Investigate the best choice of functions to use here
    // for EIGHTTAP_SMOOTH. Since it is not interpolating, need to choose what
    // to do at full-pel offsets. The current selection, where the filter is
    // applied in one direction only, and not at all for 0,0, seems to give the
    // best quality, but it may be worth trying an additional mode that does
    // do the filtering on full-pel.

    // No scaling in either direction.
    sf->predict[0][0][0] = vpx_convolve_copy_add;

#if CONFIG_VP9_HIGHBITDEPTH
    if (use_highbd) {
      if (sf->x_step_q4 == 16) {
        if (sf->y_step_q4 == 16) {
          // No scaling in either direction.
          sf->highbd_predict[0][0][0] = vpx_highbd_convolve_copy;
          sf->highbd_predict[0][0][1] = vpx_highbd_convolve_avg;
          sf->highbd_predict[0][1][0] = vpx_highbd_convolve8_vert;
          sf->highbd_predict[0][1][1] = vpx_highbd_convolve8_avg_vert;
          sf->highbd_predict[1][0][0] = vpx_highbd_convolve8_horiz;
          sf->highbd_predict[1][0][1] = vpx_highbd_convolve8_avg_horiz;
        } else {
          // No scaling in x direction. Must always scale in the y direction.
          sf->highbd_predict[0][0][0] = vpx_highbd_convolve8_vert;
          sf->highbd_predict[0][0][1] = vpx_highbd_convolve8_avg_vert;
          sf->highbd_predict[0][1][0] = vpx_highbd_convolve8_vert;
          sf->highbd_predict[0][1][1] = vpx_highbd_convolve8_avg_vert;
          sf->highbd_predict[1][0][0] = vpx_highbd_convolve8;
          sf->highbd_predict[1][0][1] = vpx_highbd_convolve8_avg;
        }
      } else {
        if (sf->y_step_q4 == 16) {
          // No scaling in the y direction. Must always scale in the x direction.
          sf->highbd_predict[0][0][0] = vpx_highbd_convolve8_horiz;
          sf->highbd_predict[0][0][1] = vpx_highbd_convolve8_avg_horiz;
          sf->highbd_predict[0][1][0] = vpx_highbd_convolve8;
          sf->highbd_predict[0][1][1] = vpx_highbd_convolve8_avg;
          sf->highbd_predict[1][0][0] = vpx_highbd_convolve8_horiz;
          sf->highbd_predict[1][0][1] = vpx_highbd_convolve8_avg_horiz;
        } else {
          // Must always scale in both directions.
          sf->highbd_predict[0][0][0] = vpx_highbd_convolve8;
          sf->highbd_predict[0][0][1] = vpx_highbd_convolve8_avg;
          sf->highbd_predict[0][1][0] = vpx_highbd_convolve8;
          sf->highbd_predict[0][1][1] = vpx_highbd_convolve8_avg;
          sf->highbd_predict[1][0][0] = vpx_highbd_convolve8;
          sf->highbd_predict[1][0][1] = vpx_highbd_convolve8_avg;
        }
      }
      // 2D subpel motion always gets filtered in both directions.
      sf->highbd_predict[1][1][0] = vpx_highbd_convolve8;
      sf->highbd_predict[1][1][1] = vpx_highbd_convolve8_avg;
    }
#endif
}
