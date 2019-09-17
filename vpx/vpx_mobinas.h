//
// Created by hyunho on 9/2/19.
//

#ifndef LIBVPX_WRAPPER_VPX_SR_CACHE_H
#define LIBVPX_WRAPPER_VPX_SR_CACHE_H

#include <limits.h>
#include "./vpx_config.h"
#include "vpx_scale/yv12config.h"

typedef struct mobinas_latency_info {
    double decode_frame;
    double interp_intra_block;
    double interp_inter_residual;
    double decode_intra_block;
    double decode_inter_block;
    double decode_inter_residual;
} mobinas_latency_info_t;

typedef enum{
    DECODE,
    DECODE_SR,
    DECODE_CACHE,
    DECODE_BILINEAR,
} mobinas_decode_mode;

typedef enum{
    PROFILE_CACHE_RESET,
    APPLY_CACHE_RESET,
    NO_CACHE_RESET,
} mobinas_cache_mode;

typedef enum{
    PROFILE_CACHE,
    KEY_FRAME_CACHE,
    NO_CACHE_POLICY,
} mobinas_cache_policy;

typedef enum{
    ONLINE_DNN,
    OFFLINE_DNN,
    NO_DNN,
} mobinas_dnn_mode;

//TODO (chanju): define struct for SNPE

typedef struct mobinas_cfg{
    //directory
    char video_dir[PATH_MAX];
    char log_dir[PATH_MAX];
    char frame_dir[PATH_MAX];
    char serialize_dir[PATH_MAX];
    char profile_dir[PATH_MAX];

    //name
    char prefix[PATH_MAX];
    char target_file[PATH_MAX];
    char cache_file[PATH_MAX];
    char compare_file[PATH_MAX];

    //log
    int save_serialized_frame;
    int save_decoded_frame;
    int save_intermediate;
    int save_final;
    int save_quality_result;
    int save_decode_result;

    //mode
    mobinas_decode_mode decode_mode;
    mobinas_cache_mode cache_mode;
    mobinas_dnn_mode dnn_mode;
    mobinas_cache_policy cache_policy;

    int target_resolution; //TODO: to set this dyanmically, make a new API.
} mobinas_cfg_t;

typedef struct mobinas_bilinear_config{
    float *x_lerp;
    int16_t *x_lerp_fixed;
    float *y_lerp;
    int16_t *y_lerp_fixed;
    int *top_y_index;
    int *bottom_y_index;
    int *left_x_index;
    int *right_x_index;
} mobinas_bilinear_config_t;

//TODO: block size types are 25 (4,8,16,32,64 x 4,8,16,32,64)
typedef struct mobinas_bilinear_profile{
    //scale x4
    mobinas_bilinear_config_t config_TX_64X64_s4;
    //scale x3
    mobinas_bilinear_config_t config_TX_64X64_s3;
    //scale x2
    mobinas_bilinear_config_t config_TX_64X64_s2;
} mobinas_bilinear_profile_t;

typedef struct mobinas_cache_reset_profile {
    FILE *file;
    int offset;
    int length;
    uint8_t *buffer;
} mobinas_cache_reset_profile_t;

typedef struct mobinas_interp_block{
    int mi_row;
    int mi_col;
    int n4_w[3];
    int n4_h[3];
    struct mobinas_interp_block *next;
} mobinas_interp_block_t;

typedef struct mobinas_interp_block_list{
    mobinas_interp_block_t *cur;
    mobinas_interp_block_t *head;
    mobinas_interp_block_t *tail;
} mobinas_interp_block_list_t;

typedef struct mobinas_worker_data {
    YV12_BUFFER_CONFIG *lr_reference_frame;
    YV12_BUFFER_CONFIG *hr_reference_frame;
    YV12_BUFFER_CONFIG *lr_resiudal;
    YV12_BUFFER_CONFIG *hr_compare_frame;

    mobinas_interp_block_list_t *intra_block_list;
    mobinas_interp_block_list_t *inter_block_list;

    int count;
    int intra_count;
    int inter_count;
    int inter_noskip_count;
    int adaptive_cache_count;

    int index;
    int reset_cache;
    mobinas_cache_reset_profile_t *cache_reset_profile;

    mobinas_latency_info_t latency;

    FILE *latency_log;
    FILE *metadata_log;
} mobinas_worker_data_t;

#ifdef __cplusplus
extern "C" {
#endif

//cache reset
void remove_mobinas_cache_reset_profile(mobinas_cache_reset_profile_t *profile);
int read_mobinas_cache_reset_profile(mobinas_cache_reset_profile_t *profile);
int write_mobinas_cache_reset_profile(mobinas_cache_reset_profile_t *profile);
uint8_t read_mobinas_cache_reset_bit(mobinas_cache_reset_profile_t *profile);
void write_mobinas_cache_reset_bit(mobinas_cache_reset_profile_t *profile, uint8_t value);

//interpolation
void create_mobinas_interp_block(struct mobinas_interp_block_list *L, int mi_col, int mi_row, int n4_w, int n4_h);
void set_mobinas_interp_block(struct mobinas_interp_block_list *L, int plane, int n4_w, int n4_h);

//worker
void init_mobinas_worker(mobinas_worker_data_t *mwd, int num_threads, mobinas_cfg_t *mobinas_cfg);
void remove_mobinas_worker(mobinas_worker_data_t *mwd, int num_threads);

//bilinear profile, config
void init_mobinas_bilinear_profile(mobinas_bilinear_profile_t *profile);
void remove_bilinear_profile(mobinas_bilinear_profile_t *profile);
void init_mobinas_bilinear_config(mobinas_bilinear_config_t *config, int scale, int width, int height);
void remove_bilinear_config(mobinas_bilinear_config_t *config);
mobinas_bilinear_config_t *get_mobinas_bilinear_config(mobinas_bilinear_profile_t *bilinear_profile, int scale);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif //LIBVPX_WRAPPER_VPX_SR_CACHE_H
