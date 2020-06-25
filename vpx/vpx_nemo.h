//
// Created by hyunho on 9/2/19.
//

#ifndef LIBVPX_WRAPPER_VPX_SR_CACHE_H
#define LIBVPX_WRAPPER_VPX_SR_CACHE_H

#include <limits.h>
#include "./vpx_config.h"
#include "vpx_scale/yv12config.h"

typedef struct mobinas_latency_info {
    //decode
    double decode;
    double decode_intra_block;
    double decode_inter_block;
    double decode_inter_residual;

    //interpolation (interp)
    double interp;
    double interp_intra_block;
    double interp_inter_block;
    double interp_inter_residual;

    //super-resolution (sr)
    double sr;
    double sr_convert_rgb_to_yuv;
    double sr_execute_dnn;
    double sr_convert_yuv_to_rgb;
    double sr_convert_float_to_int;
} mobinas_latency_info_t;

typedef struct mobinas_frame_info {
    int current_video_frame;
    int current_super_frame;
} mobinas_frame_info_t;

typedef struct mobinas_metadata_info {
    mobinas_frame_info_t reference_frames[3];
    int num_blocks;
    int num_intrablocks;
    int num_interblocks;
    int num_noskip_interblocks;
} mobinas_metadata_info_t;

typedef enum{
    DECODE,
    DECODE_SR,
    DECODE_CACHE,
} mobinas_decode_mode;

typedef enum{
    NO_CACHE,
    PROFILE_CACHE,
    KEY_FRAME_CACHE,
} mobinas_cache_mode;

typedef enum{
    NO_DNN,
    ONLINE_DNN,
    OFFLINE_DNN,
} mobinas_dnn_mode;

typedef enum{
    CPU_FLOAT32,
    GPU_FLOAT32_16_HYBRID,
    DSP_FIXED8,
    GPU_FLOAT16,
    AIP_FIXED8
} mobinas_dnn_runtime;

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

//TODO: Remove this
typedef struct mobinas_bilinear_profile{
    //scale x4
    mobinas_bilinear_config_t config_TX_64X64_s4;
    //scale x3
    mobinas_bilinear_config_t config_TX_64X64_s3;
    //scale x2
    mobinas_bilinear_config_t config_TX_64X64_s2;
} vp9_bilinear_profile_t;

//TODO: remove this
typedef struct mobinas_cache_reset_profile {
    FILE *file;
    int offset;
    int length;
    uint8_t *buffer;
} mobinas_cache_reset_profile_t;

typedef struct mobinas_cache_profile {
    FILE *file;
    uint64_t offset;
    uint8_t byte_value;
    off_t file_size;
    int num_dummy_bits;
} mobinas_cache_profile_t;

//TODO: remove target_width, target_height
//TODO: remove dnn_profile
typedef struct mobinas_dnn_profile{
    void *dnn_instance;
    int target_width;
    int target_height;
    int scale;
} mobinas_dnn_profile_t;

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
    //interpolation
	YV12_BUFFER_CONFIG *lr_resiudal;
    mobinas_interp_block_list_t *intra_block_list;
    mobinas_interp_block_list_t *inter_block_list;

    //log
    int index;
    FILE *latency_log;
    FILE *metadata_log;
    mobinas_latency_info_t latency;
    mobinas_metadata_info_t metadata;
} mobinas_worker_data_t;


//TODO: change dnn_profiles, cache_profiles, bilinear_profile to use single element
//TODO: add target_width, target_height, dnn
typedef struct mobinas_cfg{
    //direcetory
    char log_dir[PATH_MAX];
    char input_frame_dir[PATH_MAX];
    char sr_frame_dir[PATH_MAX];
    char input_reference_frame_dir[PATH_MAX];
    char sr_reference_frame_dir[PATH_MAX];
    char sr_offline_frame_dir[PATH_MAX]; //OFFLINE_DNN (load images)

    //logging
    int save_rgbframe; // rgb
    int save_yuvframe;
    int save_quality;
    int save_latency;
    int save_metadata;
    int filter_interval;

    //mode
    mobinas_decode_mode decode_mode;
    mobinas_cache_mode cache_mode;
    mobinas_dnn_mode dnn_mode;
    mobinas_dnn_runtime dnn_runtime;

    //TODO: use a single profile
    //profile
    mobinas_dnn_profile_t *dnn_profiles[5];
    mobinas_cache_profile_t *cache_profiles[5];
    vp9_bilinear_profile_t *bilinear_profile;
} mobinas_cfg_t;

typedef struct rgb24_buffer_config{
    int width;
    int height;
    int stride;
    int buffer_alloc_sz;
    uint8_t *buffer_alloc; 
    float *buffer_alloc_float; //TODO: remove this
} RGB24_BUFFER_CONFIG;

#ifdef __cplusplus
extern "C" {
#endif

//mobinas_cfg
mobinas_cfg_t *init_mobinas_cfg();
void remove_mobinas_cfg(mobinas_cfg_t *config);

//dnn profile
mobinas_dnn_profile_t *init_mobinas_dnn_profile(int width, int height, int scale);
void remove_mobinas_dnn_profile(mobinas_dnn_profile_t *profile);

//cache profile
mobinas_cache_profile_t *init_mobinas_cache_profile();
void remove_mobinas_cache_profile(mobinas_cache_profile_t *profile);
int read_cache_profile(mobinas_cache_profile_t *profile);
int read_cache_profile_dummy_bits(mobinas_cache_profile_t *profile); 

//TODO: remove this
//cache reset (deprecated)
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

//TODO; refactor below functions
//bilinear config
vp9_bilinear_profile_t *init_vp9_bilinear_profile();
mobinas_bilinear_config_t *get_vp9_bilinear_config(vp9_bilinear_profile_t *bilinear_profile, int scale);
void remove_vp9_bilinear_profile(vp9_bilinear_profile_t *profile);

//bilinear config
void init_bilinear_config(mobinas_bilinear_config_t *config, int scale, int width, int height);
void remove_bilinear_config(mobinas_bilinear_config_t *config);

//color space conversion
int RGB24_float_to_uint8(RGB24_BUFFER_CONFIG *rbf);
int RGB24_to_YV12(YV12_BUFFER_CONFIG *ybf, RGB24_BUFFER_CONFIG *rbf);
int YV12_to_RGB24(YV12_BUFFER_CONFIG *ybf, RGB24_BUFFER_CONFIG *rbf);
int RGB24_to_YV12_c(YV12_BUFFER_CONFIG *ybf, RGB24_BUFFER_CONFIG *rbf);
int YV12_to_RGB24_c(YV12_BUFFER_CONFIG *ybf, RGB24_BUFFER_CONFIG *rbf);

//rgb24 buffer
int RGB24_save_frame_buffer(RGB24_BUFFER_CONFIG *rbf, char *file_path);
int RGB24_load_frame_buffer(RGB24_BUFFER_CONFIG *rbf, char *file_path);
int RGB24_alloc_frame_buffer(RGB24_BUFFER_CONFIG *rbf, int width, int height);
int RGB24_realloc_frame_buffer(RGB24_BUFFER_CONFIG *rbf, int width, int height);
int RGB24_free_frame_buffer(RGB24_BUFFER_CONFIG *rbf);
double RGB24_calc_psnr(const RGB24_BUFFER_CONFIG *a, const RGB24_BUFFER_CONFIG *b);

//TODO: remove this (get_dnn_profile, get_cache_profile)
int default_scale_policy (int resolution);
mobinas_dnn_profile_t *get_dnn_profile(mobinas_cfg_t *mobinas_cfg, int resolution);
mobinas_cache_profile_t *get_cache_profile(mobinas_cfg_t *mobinas_cfg, int resolution);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif //LIBVPX_WRAPPER_VPX_SR_CACHE_H
