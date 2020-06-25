//
// Created by hyunho on 9/2/19.
//
#include <time.h>
#include <memory.h>
#include <stdio.h>
#include <malloc.h>
#include <assert.h>
#include "./vpx_dsp_rtcd.h"
#include <vpx_dsp/psnr.h>
#include <vpx_dsp/vpx_dsp_common.h>
#include <vpx_scale/yv12config.h>
#include <vpx_mem/vpx_mem.h>
#include <sys/param.h>
#include <math.h>
#include <libgen.h>

#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
//#include "third_party/libyuv/include/libyuv/scale.h"

#include "vpx/vpx_mobinas.h"

#ifdef __ANDROID_API__
#include <android/log.h>
#include <arm_neon.h>

#define TAG "LoadInputTensor JNI"
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
#endif

#define BUFFER_UNIT_LEN 1000
#define FRACTION_BIT (5)
#define FRACTION_SCALE (1 << FRACTION_BIT)

mobinas_cfg_t *init_mobinas_cfg() {
    mobinas_cfg_t *config = (mobinas_cfg_t *) vpx_calloc(1, sizeof(mobinas_cfg_t));

    //mode
    config->decode_mode = DECODE;
    config->dnn_mode = NO_DNN;
    config->cache_mode = NO_CACHE;

    //bilinear
    config->bilinear_profile = init_vp9_bilinear_profile();

    //dnn profile
    config->dnn_profiles[0] = init_mobinas_dnn_profile(1920, 1080, 4);
    config->dnn_profiles[1] = init_mobinas_dnn_profile(1920, 1080, 3);
    config->dnn_profiles[2] = init_mobinas_dnn_profile(1920, 1080, 2);
    config->dnn_profiles[3] = init_mobinas_dnn_profile(1920, 1080, 1);
    config->dnn_profiles[4] = init_mobinas_dnn_profile(1920, 1080, 1);

    //cache profile
    config->cache_profiles[0] = init_mobinas_cache_profile();
    config->cache_profiles[1] = init_mobinas_cache_profile();
    config->cache_profiles[2] = init_mobinas_cache_profile();
    config->cache_profiles[3] = init_mobinas_cache_profile();
    config->cache_profiles[4] = init_mobinas_cache_profile();

    return config;
}

//TODO: replace multiple profiles by a single profile
void remove_mobinas_cfg(mobinas_cfg_t *config) {
    if (config) {
        for (int i=0; i<5; i++) {
            remove_mobinas_dnn_profile(config->dnn_profiles[i]);
            remove_mobinas_cache_profile(config->cache_profiles[i]);
        }
        remove_vp9_bilinear_profile(config->bilinear_profile);
        vpx_free(config);
    }
}

mobinas_dnn_profile_t *init_mobinas_dnn_profile(int width, int height, int scale) {
    mobinas_dnn_profile_t *profile = (mobinas_dnn_profile_t *) vpx_calloc(1, sizeof(mobinas_dnn_profile_t));
    profile->dnn_instance = NULL;
    profile->target_height = height;
    profile->target_width = width;
    profile->scale = scale;

    return profile;
}

void remove_mobinas_dnn_profile(mobinas_dnn_profile_t *profile) {
    if (profile) {
        LOGE("remove a dnn profile");
        if (profile->dnn_instance) {
#if CONFIG_SNPE
            snpe_free(profile->dnn_instance);
#endif
        }
        vpx_free(profile);
    }
}

mobinas_cache_profile_t *init_mobinas_cache_profile() {
    mobinas_cache_profile_t *profile = (mobinas_cache_profile_t *) vpx_calloc(1, sizeof(mobinas_cache_profile_t));
    profile->file = NULL;
    profile->num_dummy_bits = 0;

    return profile;
    /*
    char tmp[PATH_MAX];
    if ((profile->file = fopen(path, "rb")) == NULL) {
        fprintf(stderr, "%s: fail to open a file %s", __func__, path);
        vpx_free(profile);
        return NULL;
    }

    sprintf(tmp, "%s", path);
    sprintf(profile->name, "%s", basename(tmp));
    */
}

void remove_mobinas_cache_profile(mobinas_cache_profile_t *profile) {
    if (profile) {
        LOGE("remove a cache profile");
        if (profile->file) fclose(profile->file);
        vpx_free(profile);
    }
}

int read_cache_profile_dummy_bits(mobinas_cache_profile_t *profile) {
    size_t bytes_read;
    uint8_t byte_value, apply_dnn;
    int i, dummy;

    if (profile == NULL) {
        fprintf(stderr, "%s: profile is NULL", __func__);
        return -1;
    }

    if (profile->file == NULL) {
        fprintf(stderr, "%s: profile->file is NULL", __func__);
        return -1;
    }

    if (profile->num_dummy_bits > 0) {
        for (i = 0; i < profile->num_dummy_bits; i++) {
            /*
            if (fread(&dummy, sizeof(uint8_t), 1, profile->file) != 1) {
                fprintf(stderr, "%s: fail to read a cache profile", __func__);
                return -1;
            }
            */
            profile->offset += 1;
        }
    }

    if (fread(&profile->num_dummy_bits, sizeof(int), 1, profile->file) != 1)
    {
        fprintf(stderr, "%s: fail to read a cache profile", __func__);
        return -1;
    }

    printf("num_dummy_bits: %d\n", profile->num_dummy_bits);

    return 0;
}

int read_cache_profile(mobinas_cache_profile_t *profile) {
    size_t bytes_read;
    uint8_t byte_value, apply_dnn;


    if (profile == NULL) {
        fprintf(stderr, "%s: profile is NULL", __func__);
        return -1;
    }

    if (profile->file == NULL) {
        fprintf(stderr, "%s: profile->file is NULL", __func__);
        return -1;
    }

    if (profile->offset % 8 == 0) {
        if (fread(&profile->byte_value, sizeof(uint8_t), 1, profile->file) != 1)
        {
            fprintf(stderr, "%s: fail to read a cache profile", __func__);
            return -1;
        }
    }

    apply_dnn = (profile->byte_value & (1 << (profile->offset % 8))) >> (profile->offset % 8); //TODO: 1, 0
    profile->offset += 1;

    return apply_dnn;
}

//TODO: reset cache reset
static mobinas_cache_reset_profile_t *init_mobinas_cache_reset_profile(const char *path, int load_profile) {
    mobinas_cache_reset_profile_t *profile = (mobinas_cache_reset_profile_t*) vpx_malloc(sizeof(mobinas_cache_reset_profile_t));

    if (load_profile) {
        memset(profile, 0, sizeof(mobinas_cache_reset_profile_t));
        profile->file = fopen(path, "rb");
        if (profile->file == NULL) {
            fprintf(stderr, "%s: fail to open a file %s", __func__, path);
            vpx_free(profile);
            return NULL;
        }
    }
    else {
        profile->file = fopen(path, "wb");
        if (profile->file == NULL) {
            fprintf(stderr, "%s: fail to open a file %s", __func__, path);
            vpx_free(profile);
            return NULL;
        }

        profile->offset = 0;
        profile->length = BUFFER_UNIT_LEN;
        profile->buffer = (uint8_t *) malloc(sizeof(uint8_t) * BUFFER_UNIT_LEN);
        memset(profile->buffer, 0, sizeof(uint8_t) * BUFFER_UNIT_LEN);
    }

    return profile;
}

void remove_mobinas_cache_reset_profile(mobinas_cache_reset_profile_t *profile) {
	if (profile != NULL) {
        if (profile->buffer != NULL) free(profile->buffer);
        if (profile->file != NULL) fclose(profile->file);
        vpx_free(profile);
    }
}

int read_mobinas_cache_reset_profile(mobinas_cache_reset_profile_t *profile) {
    size_t bytes_read, offset, length;

    if (profile->file == NULL) {
        fprintf(stderr, "%s: profile is NULL", __func__);
        return -1;
    }
    bytes_read = fread(&offset, sizeof(int), 1, profile->file);
    if(bytes_read != 1) {
        fprintf(stderr, "%s: fail to read offset values", __func__);
        return -1;
    }

    length = offset / 8 + 1;
    if (profile->buffer == NULL) {
        profile->buffer = (uint8_t *) malloc(sizeof(uint8_t) * (length));
    }
    else {
        profile->buffer = (uint8_t *) realloc(profile->buffer, sizeof(uint8_t ) * length);
    }

    bytes_read = fread(profile->buffer, sizeof(uint8_t), length, profile->file); //TODO: length or length +- 1
    if(bytes_read != length) {
        fprintf(stderr, "%s: fail to read buffer values", __func__);
        return -1;
    }

    profile->length = length;
    profile->offset = 0;

    return 0;
}

int write_mobinas_cache_reset_profile(mobinas_cache_reset_profile_t *profile) {
    size_t offset = profile->offset;
    size_t length = offset / 8 + 1;
    size_t bytes_write;

    if (profile->file == NULL) {
        fprintf(stderr, "%s: profile is NULL", __func__);
        return -1;
    }

    bytes_write = fwrite(&offset, sizeof(int), 1, profile->file);
    if (bytes_write != 1) {
        fprintf(stderr, "%s: fail to write offset values", __func__);
        return -1;
    }

    bytes_write = fwrite(profile->buffer, sizeof(uint8_t), length, profile->file);
    if(bytes_write != length) {
        fprintf(stderr, "%s: fail to write buffer values", __func__);
        return -1;
    }

    profile->offset = 0;
    memset(profile->buffer, 0, sizeof(uint8_t) * profile->length);

    return 0;
}

uint8_t read_mobinas_cache_reset_bit(mobinas_cache_reset_profile_t *profile){
    int offset = profile->offset;
    int byte_offset = offset / 8;
    int bit_offset = offset % 8;
    uint8_t mask = 1 << bit_offset;

    //TODO: refactor, this is worting
    if (byte_offset + 1 > profile->length) {
        fprintf(stderr, "%s: invalid cache reset profile | byte_offset: %d, profile->legnth: %d"  , __func__, byte_offset, profile->length);
        return 0; // don't reset cache
    }

    profile->offset += 1;

    return (profile->buffer[byte_offset] & mask) >> bit_offset;
}

void write_mobinas_cache_reset_bit(mobinas_cache_reset_profile_t *profile, uint8_t value){
    assert(value == 0 || value == 1);

    int offset = profile->offset;
    int byte_offset = offset / 8;
    int bit_offset = offset % 8;

    if (byte_offset + 1 > profile->length) {
        profile->buffer = (uint8_t *) realloc(profile->buffer, profile->length + BUFFER_UNIT_LEN);
        memset(profile->buffer + profile->length, 0,  sizeof(uint8_t) * BUFFER_UNIT_LEN);
        profile->length += BUFFER_UNIT_LEN;
    }

    if (value != 0) {
        value = value << bit_offset;
        profile->buffer[byte_offset] = profile->buffer[byte_offset] | value;
    }

    profile->offset += 1;
}

void remove_mobinas_worker(mobinas_worker_data_t *mwd, int num_threads){
    int i;
    if (mwd != NULL) {
        for (i = 0; i < num_threads; ++i) {
            vpx_free_frame_buffer(mwd[i].lr_resiudal);
            vpx_free(mwd[i].lr_resiudal);

            //free decode block lists
            mobinas_interp_block_t *intra_block = mwd[i].intra_block_list->head;
            mobinas_interp_block_t *prev_block = NULL;
            while (intra_block != NULL) {
                prev_block = intra_block;
                intra_block = intra_block->next;
                vpx_free(prev_block);
            }
            vpx_free(mwd[i].intra_block_list);

            mobinas_interp_block_t *inter_block = mwd[i].inter_block_list->head;
            while (inter_block != NULL) {
                prev_block = inter_block;
                inter_block = inter_block->next;
                vpx_free(prev_block);
            }
            vpx_free(mwd[i].inter_block_list);

            if (mwd[i].latency_log != NULL) fclose(mwd[i].latency_log);
            if (mwd[i].metadata_log != NULL) fclose(mwd[i].metadata_log);
        }
        vpx_free(mwd);
    }
}

static void init_mobinas_worker_data(mobinas_worker_data_t *mwd, int index){
    assert (mwd != NULL);

    mwd->lr_resiudal = (YV12_BUFFER_CONFIG *) vpx_calloc(1, sizeof(YV12_BUFFER_CONFIG));

    mwd->intra_block_list = (mobinas_interp_block_list_t *) vpx_calloc(1, sizeof(mobinas_interp_block_list_t));
    mwd->intra_block_list->cur = NULL;
    mwd->intra_block_list->head = NULL;
    mwd->intra_block_list->tail = NULL;

    mwd->inter_block_list = (mobinas_interp_block_list_t *) vpx_calloc(1, sizeof(mobinas_interp_block_list_t));
    mwd->inter_block_list->cur = NULL;
    mwd->inter_block_list->head = NULL;
    mwd->inter_block_list->tail = NULL;

    mwd->index = index;

    mwd->latency_log = NULL;
    mwd->metadata_log = NULL;
}

mobinas_worker_data_t *init_mobinas_worker(int num_threads, mobinas_cfg_t *mobinas_cfg) {
    char latency_log_path[PATH_MAX];
    char metadata_log_path[PATH_MAX];
    char cache_reset_profile_path[PATH_MAX];

    if (!mobinas_cfg) {
        fprintf(stderr, "%s: mobinas_cfg is NULL", __func__);
        return NULL;
    }

    if (num_threads <= 0) {
        fprintf(stderr, "%s: num_threads is equal or less than 0", __func__);
        return NULL;
    }

    mobinas_worker_data_t *mwd = (mobinas_worker_data_t *) vpx_malloc(sizeof(mobinas_worker_data_t) * num_threads);

    int i;
    for (i = 0; i < num_threads; ++i) {
        init_mobinas_worker_data(&mwd[i], i);

        if (mobinas_cfg->save_latency == 1) {
            sprintf(latency_log_path, "%s/latency_thread%d%d.txt", mobinas_cfg->log_dir, mwd[i].index, num_threads);
            if ((mwd[i].latency_log = fopen(latency_log_path, "w")) == NULL) {
                fprintf(stderr, "%s: cannot open a file %s", __func__, latency_log_path);
                mobinas_cfg->save_latency = 0;
            }
        }

        if (mobinas_cfg->save_metadata == 1){
            sprintf(metadata_log_path, "%s/metadata_thread%d%d.txt", mobinas_cfg->log_dir, mwd[i].index, num_threads);
            if ((mwd[i].metadata_log = fopen(metadata_log_path, "w")) == NULL)
            {
                fprintf(stderr, "%s: cannot open a file %s", __func__, metadata_log_path);
                mobinas_cfg->save_metadata = 0;
            }
        }
    }

    return mwd;
}

mobinas_bilinear_config_t *get_vp9_bilinear_config(vp9_bilinear_profile_t *bilinear_profile, int scale) {
    assert(scale == 4 || scale ==3 || scale ==2);

    switch (scale) {
        case 2:
            return &bilinear_profile->config_TX_64X64_s2;
        case 3:
            return &bilinear_profile->config_TX_64X64_s3;
        case 4:
            return &bilinear_profile->config_TX_64X64_s4;
        default:
            assert("%s: invalid scale");
            return NULL;
        }
}

void init_bilinear_config(mobinas_bilinear_config_t *config, int width, int height, int scale) {
    int x, y;

    assert (config != NULL);
    assert (width != 0 && height != 0 && scale > 0);

    config->x_lerp = (float *) vpx_malloc(sizeof(float) * width * scale);
    config->x_lerp_fixed = (int16_t *) vpx_malloc(sizeof(int16_t) * width * scale);
    config->left_x_index = (int *) vpx_malloc(sizeof(int) * width * scale);
    config->right_x_index = (int *) vpx_malloc(sizeof(int) * width * scale);

    config->y_lerp = (float *) vpx_malloc(sizeof(float) * height * scale);
    config->y_lerp_fixed = (int16_t *) vpx_malloc(sizeof(int16_t) * height * scale);
    config->top_y_index = (int *) vpx_malloc(sizeof(int) * height * scale);
    config->bottom_y_index = (int *) vpx_malloc(sizeof(int) * height * scale);

    for (x = 0; x < width * scale; ++x) {
        const double in_x = (x + 0.5f) / scale - 0.5f;
        config->left_x_index[x] = MAX(floor(in_x), 0);
        config->right_x_index[x] = MIN(ceil(in_x), width - 1);
        config->x_lerp[x] = in_x - floor(in_x);
        config->x_lerp_fixed[x] = config->x_lerp[x] * FRACTION_SCALE;
    }

    for (y = 0; y < height * scale; ++y) {
        const double in_y = (y + 0.5f) / scale - 0.5f;
        config->top_y_index[y] = MAX(floor(in_y), 0);
        config->bottom_y_index[y] = MIN(ceil(in_y), height - 1);
        config->y_lerp[y] = in_y - floor(in_y);
        config->y_lerp_fixed[y] = config->y_lerp[y] * FRACTION_SCALE;
    }
}

//TODO: refactor to cover all BLOCK type used in vp9
vp9_bilinear_profile_t *init_vp9_bilinear_profile(){
    vp9_bilinear_profile_t *profile = (vp9_bilinear_profile_t *) vpx_calloc(1, sizeof(vp9_bilinear_profile_t));

    //scale x4
    init_bilinear_config(&profile->config_TX_64X64_s4, 64, 64, 4);
    //scale x3
    init_bilinear_config(&profile->config_TX_64X64_s3, 64, 64, 3);
    //scale x2
    init_bilinear_config(&profile->config_TX_64X64_s2, 64, 64, 2);

    return profile;
}

void remove_bilinear_config(mobinas_bilinear_config_t *config) {
    if (config != NULL) {
        vpx_free(config->x_lerp);
        vpx_free(config->x_lerp_fixed);
        vpx_free(config->left_x_index);
        vpx_free(config->right_x_index);

        vpx_free(config->y_lerp);
        vpx_free(config->y_lerp_fixed);
        vpx_free(config->top_y_index);
        vpx_free(config->bottom_y_index);
    }
}

void remove_vp9_bilinear_profile(vp9_bilinear_profile_t *profile) {
    if (profile != NULL) {
        LOGE("remove a dnn profile");
        //scale x4
        remove_bilinear_config(&profile->config_TX_64X64_s4);
        //scale x3
        remove_bilinear_config(&profile->config_TX_64X64_s3);
        //scale x2
        remove_bilinear_config(&profile->config_TX_64X64_s2);

        vpx_free(profile);
    }
}


void create_mobinas_interp_block(mobinas_interp_block_list_t *L, int mi_col, int mi_row, int n4_w, int n4_h) {
    mobinas_interp_block_t *newBlock = (mobinas_interp_block_t *) vpx_calloc (1, sizeof(mobinas_interp_block_t));
    newBlock->mi_col = mi_col;
    newBlock->mi_row = mi_row;
    newBlock->n4_w[0] = n4_w;
    newBlock->n4_h[0] = n4_h;
    newBlock->next = NULL;

    if(L->head == NULL && L->tail == NULL) {
        L->head = L->tail = newBlock;
    }
    else {
        L->tail->next = newBlock;
        L->tail = newBlock;
    }

    L->cur = newBlock;
}

void set_mobinas_interp_block(mobinas_interp_block_list_t *L, int plane, int n4_w, int n4_h) {
    mobinas_interp_block_t *currentBlock = L->cur;
    currentBlock->n4_w[plane] = n4_w;
    currentBlock->n4_h[plane] = n4_h;
}

int default_scale_policy (int resolution){
    switch(resolution){
        case 270:
            return 4;
        case 240:
            return 4;
        case 360:
            return 3;
        case 480:
            return 2;
        case 720:
            return 1;
        case 1080:
            return 1;
        default:
            fprintf(stderr, "Unsupported resolution: %d\n", resolution);
            return -1;
    }
}

//TODO: Implement float-int conversion inside vpx_dsp module
int RGB24_float_to_uint8(RGB24_BUFFER_CONFIG *rbf) {
#ifdef __ANDROID_API__
    return RGB24_float_to_uint8_neon(rbf);
#else
    return RGB24_float_to_uint8_c(rbf);
#endif
}

int RGB24_float_to_uint8_neon(RGB24_BUFFER_CONFIG *rbf) {
#ifdef __ANDROID_API__
    if (rbf == NULL) {
        return -1;
    }

    float *src = rbf->buffer_alloc_float;
    uint8_t *dst = rbf->buffer_alloc;

    int w, h;
    LOGD("height: %d, width: %d", rbf->height, rbf->width);

    const float init[4] = {0.5, 0.5, 0.5, 0.5};
    float32x4_t c0 = vld1q_f32(init);

    for (h = 0; h < rbf->height; h++) {
        for (w = 0; w < rbf->width; w += 8) {
            if(rbf->width - w < 8) {
                for (; w < rbf->width; w++) {
                    *(dst + w + h * rbf->stride) = clamp(round(*(src + w + h * rbf->stride)), 0, 255);
                }
            }
            else {
                float32x4_t src_float_0 = vld1q_f32(src + w + h * rbf->stride);
                src_float_0 = vaddq_f32(src_float_0, c0);
                uint32x4_t dst_uint32_0 = vcvtq_u32_f32(src_float_0);
                uint16x4_t dst_uint16_0 = vqmovn_s32(dst_uint32_0);

                float32x4_t src_float_1 = vld1q_f32(src + w + 4 + h * rbf->stride);
                src_float_1 = vaddq_f32(src_float_1, c0);
                uint32x4_t dst_uint32_1 =  vcvtq_u32_f32(src_float_1);
                uint16x4_t dst_uint16_1 =  vqmovn_s32(dst_uint32_1);

                uint16x8_t dst_uint16 = vcombine_u16(dst_uint16_0, dst_uint16_1);
                uint8x8_t dst_uint8 = vqmovn_u16(dst_uint16);

                vst1_u8(dst + w + h * rbf->stride, dst_uint8);
            }
        }
    }
#endif
    return 0;
}

int RGB24_float_to_uint8_c(RGB24_BUFFER_CONFIG *rbf) {
    if(rbf == NULL) {
        return -1;
    }

    float *src = rbf->buffer_alloc_float;
    uint8_t *dst = rbf->buffer_alloc;

    int w, h;
    for (h = 0; h < rbf->height; h++) {
        for (w = 0; w < rbf->width; w++) {
               *(dst + w + h * rbf->stride) = clamp(round(*(src + w + h * rbf->stride)), 0, 255);
        }
    }

    return 0;
}

int RGB24_to_YV12(YV12_BUFFER_CONFIG *ybf, RGB24_BUFFER_CONFIG *rbf) {
    if(ybf == NULL || rbf == NULL) {
        return -1;
    }
    int result = RAWToI420(rbf->buffer_alloc, rbf->stride, ybf->y_buffer, ybf->y_stride,
                                 ybf->u_buffer, ybf->uv_stride, ybf->v_buffer, ybf->uv_stride,
                                 ybf->y_crop_width, ybf->y_crop_height);
    return result;
}

int YV12_to_RGB24(YV12_BUFFER_CONFIG *ybf, RGB24_BUFFER_CONFIG *rbf) {
    if(ybf == NULL || rbf == NULL) {
        return -1;
    }
    int result = I420ToRAW(ybf->y_buffer, ybf->y_stride, ybf->u_buffer, ybf->uv_stride,
                             ybf->v_buffer, ybf->uv_stride, rbf->buffer_alloc, rbf->stride,
                             ybf->y_crop_width, ybf->y_crop_height);
    return result;
}

int RGB24_to_YV12_c(YV12_BUFFER_CONFIG *ybf, RGB24_BUFFER_CONFIG *rbf) {
    if(ybf == NULL || rbf == NULL) {
        return -1;
    }

    uint8_t r, g, b;
    const int height = ybf->y_crop_height;
    const int width = ybf->y_crop_width;

    int i, j;
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            r = *(rbf->buffer_alloc + i * rbf->stride + j * 3);
            g = *(rbf->buffer_alloc + i * rbf->stride + j * 3 + 1);
            b = *(rbf->buffer_alloc + i * rbf->stride + j * 3 + 2);

            *(ybf->y_buffer + i * ybf->y_stride + j) = (uint8_t) clamp(round((0.256788 * r + 0.504129 * g + 0.097906 * b) + 16), 0, 255);
            *(ybf->u_buffer + (i >> ybf->subsampling_y) * ybf->uv_stride + (j >> ybf->subsampling_x)) = (uint8_t) clamp(round((-0.148223 * r - 0.290993 * g + 0.439216 * b) + 128), 0, 255);
            *(ybf->v_buffer + (i >> ybf->subsampling_y) * ybf->uv_stride + (j >> ybf->subsampling_x)) = (uint8_t) clamp(round((0.439216 * r - 0.367788 * g - 0.071427 * b) + 128), 0, 255);
        }
    }

    return 0;
}


int YV12_to_RGB24_c(YV12_BUFFER_CONFIG *ybf, RGB24_BUFFER_CONFIG *rbf) {
    if(ybf == NULL || rbf == NULL) {
        return -1;
    }
    uint8_t y, u, v, r, g, b;
    int i, j;
    const int height = ybf->y_crop_height;
    const int width = ybf->y_crop_width;

    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            y = *(ybf->y_buffer + i * ybf->y_stride + j);
            u = *(ybf->u_buffer + (i >> ybf->subsampling_y) * ybf->uv_stride + (j >> ybf->subsampling_x));
            v = *(ybf->v_buffer + (i >> ybf->subsampling_y) * ybf->uv_stride + (j >> ybf->subsampling_x));

            *(rbf->buffer_alloc + i * rbf->stride + j * 3) =  (uint8_t) clamp(round(1.164383 * (y-16) + 1.596027 * (v-128)), 0, 255); // R value
            *(rbf->buffer_alloc + i * rbf->stride + j * 3 + 1) =  (uint8_t) clamp(round(1.164383 * (y-16) - 0.391762 * (u - 128) - 0.812968 * (v - 128)), 0, 255); // G value
            *(rbf->buffer_alloc + i * rbf->stride + j * 3 + 2) =  (uint8_t) clamp(round(1.164383 * (y-16) + 2.017232 * (u - 128)), 0, 255); // B value
        }
    }

    return 0;
}

int RGB24_save_frame_buffer(RGB24_BUFFER_CONFIG *rbf, char *file_path) {
    FILE *serialize_file = fopen(file_path, "wb");
    if(serialize_file == NULL)
    {
        fprintf(stderr, "%s: fail to save a file to %s\n", __func__, file_path);
        return -1;
    }

    uint8_t *src = rbf->buffer_alloc;
    int h = rbf->height;
    do {
        fwrite(src, sizeof(uint8_t), rbf->width, serialize_file);
        src += rbf->stride;
    } while (--h);

    fclose(serialize_file);

    return 0;
}

int RGB24_load_frame_buffer(RGB24_BUFFER_CONFIG *rbf, char *file_path) {
    //load file into rgb_buffer
    //convert RGB24 to I420
    FILE *serialize_file = fopen(file_path, "rb");
    if(serialize_file == NULL)
    {
        fprintf(stderr, "%s: fail to open a file from %s\n", __func__, file_path);
        return -1;
    }

    uint8_t *src = rbf->buffer_alloc;
    int h = rbf->height;
    do
    {
        fread(src, sizeof(uint8_t), rbf->width, serialize_file);
        src += rbf->stride;
    }
    while (--h);

    fclose(serialize_file);

    return 0;
}

int RGB24_alloc_frame_buffer(RGB24_BUFFER_CONFIG *rbf, int width, int height) {
    if(rbf) {
        RGB24_free_frame_buffer(rbf);
        return RGB24_realloc_frame_buffer(rbf, width, height);
    }
    return -1;
}

int RGB24_realloc_frame_buffer(RGB24_BUFFER_CONFIG *rbf, int width, int height) {
    if(rbf) {
        const int stride = width * 3;

        const int frame_size = height * stride;

        if (frame_size > rbf->buffer_alloc_sz) {
            //printf("rbf->buffer_alloc_sz: %d\n", rbf->buffer_alloc_sz);
            //printf("rbf->buffer_alloc: %p\n", rbf->buffer_alloc); 
            if (rbf->buffer_alloc_sz != 0) {
                vpx_free(rbf->buffer_alloc);
                vpx_free(rbf->buffer_alloc_float);
            }

            rbf->buffer_alloc = (uint8_t *) vpx_calloc(1, (size_t) frame_size * sizeof(uint8_t));
            if (!rbf->buffer_alloc) {
                return -1;
            }

            rbf->buffer_alloc_float = (float *) vpx_calloc(1, (size_t) frame_size * sizeof(float));
            if (!rbf->buffer_alloc_float) {
                return -1;
            }

            rbf->buffer_alloc_sz = (int) frame_size;
        }
        rbf->height = height;
        rbf->width = width * 3;
        rbf->stride = stride;

        return 0;
    }
    return -1;
}

int RGB24_free_frame_buffer(RGB24_BUFFER_CONFIG *rbf) {
    if (rbf) {
        if (rbf->buffer_alloc_sz > 0) {
            vpx_free(rbf->buffer_alloc);
            vpx_free(rbf->buffer_alloc_float);
        }
        memset(rbf, 0, sizeof(RGB24_BUFFER_CONFIG));
    }
    else {
        return -1;
    }
    return 0;
}

//from <vpx_dsp/src/psnr.c>
static void encoder_variance(const uint8_t *a, int a_stride, const uint8_t *b,
                             int b_stride, int w, int h, unsigned int *sse,
                             int *sum) {
    int i, j;

    *sum = 0;
    *sse = 0;

    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            const int diff = a[j] - b[j];
            *sum += diff;
            *sse += diff * diff;
        }

        a += a_stride;
        b += b_stride;
    }
}

//from <vpx_dsp/src/psnr.c>
static int64_t get_sse(const uint8_t *a, int a_stride, const uint8_t *b,
                       int b_stride, int width, int height) {
    const int dw = width % 16;
    const int dh = height % 16;
    int64_t total_sse = 0;
    unsigned int sse = 0;
    int sum = 0;
    int x, y;

    if (dw > 0) {
        encoder_variance(&a[width - dw], a_stride, &b[width - dw], b_stride, dw,
                         height, &sse, &sum);
        total_sse += sse;
    }

    if (dh > 0) {
        encoder_variance(&a[(height - dh) * a_stride], a_stride,
                         &b[(height - dh) * b_stride], b_stride, width - dw, dh,
                         &sse, &sum);
        total_sse += sse;
    }

    for (y = 0; y < height / 16; ++y) {
        const uint8_t *pa = a;
        const uint8_t *pb = b;
        for (x = 0; x < width / 16; ++x) {
            vpx_mse16x16(pa, a_stride, pb, b_stride, &sse);
            total_sse += sse;

            pa += 16;
            pb += 16;
        }

        a += 16 * a_stride;
        b += 16 * b_stride;
    }

    return total_sse;
}

//TODO: due to float value
double RGB24_calc_psnr(const RGB24_BUFFER_CONFIG *a, const RGB24_BUFFER_CONFIG *b) {
    static const double peak = 255.0;
    double psnr;
    int i;
    uint64_t total_sse = 0;
    uint32_t total_samples = 0;

    const int w = a->width;
    const int h = a->height;
    const uint32_t samples = w * h;

    const uint64_t sse = get_sse(a->buffer_alloc, a->stride, b->buffer_alloc, b->stride, w, h);

    psnr = vpx_sse_to_psnr(samples, peak, (double) sse);

    return psnr;
}

mobinas_dnn_profile_t *get_dnn_profile(mobinas_cfg_t *mobinas_cfg, int resolution){
    switch(resolution){
        case 240:
            return mobinas_cfg->dnn_profiles[0];
        case 360:
            return mobinas_cfg->dnn_profiles[1];
        case 480:
            return mobinas_cfg->dnn_profiles[2];
        case 720:
            return mobinas_cfg->dnn_profiles[3];
        case 1080:
            return mobinas_cfg->dnn_profiles[4];
        default:
            fprintf(stderr, "Invalid resolution\n");
            return NULL;
    }
}

mobinas_cache_profile_t *get_cache_profile(mobinas_cfg_t *mobinas_cfg, int resolution){
    switch(resolution){
        case 240:
            return mobinas_cfg->cache_profiles[0];
        case 360:
            return mobinas_cfg->cache_profiles[1];
        case 480:
            return mobinas_cfg->cache_profiles[2];
        case 720:
            return mobinas_cfg->cache_profiles[3];
        case 1080:
            return mobinas_cfg->cache_profiles[4];
        default:
            fprintf(stderr, "Invalid resolution\n");
            return NULL;
    }
}
