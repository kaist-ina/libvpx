//
// Created by hyunho on 9/2/19.
//

#ifndef LIBVPX_WRAPPER_VPX_SR_CACHE_H
#define LIBVPX_WRAPPER_VPX_SR_CACHE_H

#include <limits.h>
#include "./vpx_config.h"
#include "vpx_scale/yv12config.h"

typedef int (*mobinas_get_scale_fn_t) (int);

int default_scale_policy (int resolution);

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
    NO_CACHE_RESET,
    PROFILE_CACHE_RESET,
    APPLY_CACHE_RESET,
} mobinas_cache_mode;

typedef enum{
    NO_CACHE,
    PROFILE_CACHE,
    KEY_FRAME_CACHE,
} mobinas_cache_policy;

typedef enum{
    NO_DNN,
    ONLINE_DNN,
    OFFLINE_DNN,
} mobinas_dnn_mode;

typedef enum{
    DECODED_FRAME,
    SERIALIZED_FRAME,
    ALL_FRAME,
} mobinas_frame_type;

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
} vp9_bilinear_profile_t;

typedef struct mobinas_cache_reset_profile {
    FILE *file;
    int offset;
    int length;
    uint8_t *buffer;
} mobinas_cache_reset_profile_t;

typedef struct mobinas_cache_profile {
    char name[PATH_MAX];
    FILE *file;
    uint64_t offset;
    uint8_t byte_value;
} mobinas_cache_profile_t;

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

//TODO: replace save_* into save_frame
typedef struct mobinas_cfg{
    //directory
    char save_dir[PATH_MAX];
    char prefix[PATH_MAX];

    //video
    char target_file[PATH_MAX];
    char cache_file[PATH_MAX];
    char compare_file[PATH_MAX];

    //log
    mobinas_frame_type frame_type;
    int save_intermediate_frame;
    int save_final_frame;
    int save_quality_result;
    int save_latency_result;
    int save_metadata_result;

    //mode
    mobinas_decode_mode decode_mode;
    mobinas_decode_mode saved_decode_mode;
    mobinas_cache_mode cache_mode;
    mobinas_dnn_mode dnn_mode;
    mobinas_cache_policy cache_policy;

    //configuration
    mobinas_get_scale_fn_t get_scale;
    mobinas_cache_profile_t *cache_profile;
    vp9_bilinear_profile_t *bilinear_profile;
} mobinas_cfg_t;

typedef struct rgb24_buffer_config{
    int width;
    int height;
    int stride;
    int buffer_alloc_sz;
    float *buffer_alloc;
} RGB24_BUFFER_CONFIG;

#ifdef __cplusplus
extern "C" {
#endif

//mobinas_cfg
mobinas_cfg_t *init_mobinas_cfg();
void remove_mobinas_cfg(mobinas_cfg_t *config);

//cache profile
mobinas_cache_profile_t *init_mobinas_cache_profile(const char *path);
void remove_mobinas_cache_profile(mobinas_cache_profile_t *profile);
int read_cache_profile(mobinas_cache_profile_t *profile);

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
mobinas_worker_data_t *init_mobinas_worker(int num_threads, mobinas_cfg_t *mobinas_cfg);
void remove_mobinas_worker(mobinas_worker_data_t *mwd, int num_threads);

//bilinear config
vp9_bilinear_profile_t *init_vp9_bilinear_profile();
mobinas_bilinear_config_t *get_vp9_bilinear_config(vp9_bilinear_profile_t *bilinear_profile, int scale);
void remove_vp9_bilinear_profile(vp9_bilinear_profile_t *profile);

//bilinear config
void init_bilinear_config(mobinas_bilinear_config_t *config, int scale, int width, int height);
void remove_bilinear_config(mobinas_bilinear_config_t *config);

//color space conversion
int RGB24_to_YV12(YV12_BUFFER_CONFIG *ybf, RGB24_BUFFER_CONFIG *rbf);
int YV12_to_RGB24(YV12_BUFFER_CONFIG *ybf, RGB24_BUFFER_CONFIG *rbf);
int RGB24_save_frame_buffer(RGB24_BUFFER_CONFIG *rbf, char *file_path);
int RGB24_load_frame_buffer(RGB24_BUFFER_CONFIG *rbf, char *file_path);
int RGB24_alloc_frame_buffer(RGB24_BUFFER_CONFIG *rbf, int width, int height);
int RGB24_realloc_frame_buffer(RGB24_BUFFER_CONFIG *rbf, int width, int height);
int RGB24_free_frame_buffer(RGB24_BUFFER_CONFIG *rbf);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif //LIBVPX_WRAPPER_VPX_SR_CACHE_H
