//
// Created by hyunho on 9/2/19.
//

#include <memory.h>
#include <stdio.h>
#include <malloc.h>
#include <assert.h>
#include <android/log.h>
#include <vpx_scale/yv12config.h>
#include <vpx_mem/vpx_mem.h>
#include <sys/param.h>
#include <math.h>
#include "vpx/vpx_mobinas.h"
#include "vpx/vpx_mobinas.h"

#define TAG "vpx_cache.c JNI"
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

#define BUFFER_UNIT_LEN 1000
#define FRACTION_BIT (5)
#define FRACTION_SCALE (1 << FRACTION_BIT)

//TODO: reset cache reset
static mobinas_cache_reset_profile_t *init_mobinas_cache_reset_profile(const char *path, int load_profile) {
    assert (path != NULL);

    mobinas_cache_reset_profile_t *profile = (mobinas_cache_reset_profile_t*) vpx_malloc(sizeof(mobinas_cache_reset_profile_t));

    if (load_profile) {
        memset(profile, 0, sizeof(mobinas_cache_reset_profile_t));
        profile->file = fopen(path, "rb");
        if (profile->file == NULL) {
            LOGE("%s: cannot load a file %s", __func__, path);

            vpx_free(profile);
            return NULL;
        }
    }
    else {
        profile->file = fopen(path, "wb");
        if (profile->file == NULL) {
            LOGE("%s: cannot create a file %s", __func__, path);

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

//offset 정보랑 buffer 저장
int read_mobinas_cache_reset_profile(mobinas_cache_reset_profile_t *profile) {
    size_t bytes_read;
    int offset, length;

    if (profile->file == NULL) {
        LOGE("%s: file does not exist");
        return -1;
    }
    bytes_read = fread(&offset, sizeof(int), 1, profile->file);
    if(bytes_read != 1) {
        LOGE("%s: reading offset value failed", __func__);
        return -1;
    }

//    LOGD("%s: offset %d", __func__, offset);

    length = offset / 8 + 1;
    if (profile->buffer == NULL) {
        profile->buffer = (uint8_t *) malloc(sizeof(uint8_t) * (length));
    }
    else {
        profile->buffer = (uint8_t *) realloc(profile->buffer, sizeof(uint8_t ) * length);
    }

    bytes_read = fread(profile->buffer, sizeof(uint8_t), length, profile->file); //TODO: length or length +- 1
    if(bytes_read != length) {
        LOGE("%s: reading buffer failed", __func__);
        return -1;
    }

    profile->length = length;
    profile->offset = 0;

    return 0;
}

int write_mobinas_cache_reset_profile(mobinas_cache_reset_profile_t *profile) {
    int offset = profile->offset;
    int length = offset / 8 + 1;
    size_t bytes_write;

//    LOGD("%s: offset %d", __func__, offset);

    if (profile->file == NULL) {
        LOGE("%s: file does not exist");
        return -1;
    }

    bytes_write = fwrite(&offset, sizeof(int), 1, profile->file);
    if (bytes_write != 1) {
        LOGE("%s: writing offset value failed", __func__);
        return -1;
    }

    bytes_write = fwrite(profile->buffer, sizeof(uint8_t), length, profile->file);
    if(bytes_write != length) {
        LOGE("%s: writing buffer failed", __func__);
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
        LOGE("%s: invalid cache reset profile | byte_offset: %d, profile->legnth: %d"  , __func__, byte_offset, profile->length);
        return 0; // don't reset cache
    }

    profile->offset += 1;

//    LOGD("offset: %d, byte_offset: %d, bit_offset: %d, value: %d, bit: %d", offset, byte_offset, bit_offset, profile->buffer[byte_offset], (profile->buffer[byte_offset] & mask) >> bit_offset);

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
    if (mwd != NULL) {
        for (int i = 0; i < num_threads; ++i) {
            vpx_free_frame_buffer(mwd[i].hr_compare_frame);
            vpx_free_frame_buffer(mwd[i].hr_reference_frame);
            vpx_free_frame_buffer(mwd[i].lr_reference_frame);
            vpx_free_frame_buffer(mwd[i].lr_resiudal);
            vpx_free(mwd[i].hr_compare_frame);
            vpx_free(mwd[i].hr_reference_frame);
            vpx_free(mwd[i].lr_reference_frame);
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

            remove_mobinas_cache_reset_profile(mwd[i].cache_reset_profile);
        }

        vpx_free(mwd);
    }
}

static void init_mobinas_worker_data(mobinas_worker_data_t *mwd, int index){
    assert (mwd != NULL);

    mwd->hr_compare_frame = (YV12_BUFFER_CONFIG *) vpx_calloc(1, sizeof(YV12_BUFFER_CONFIG)); //debug
    mwd->hr_reference_frame = (YV12_BUFFER_CONFIG *) vpx_calloc(1, sizeof(YV12_BUFFER_CONFIG)); //debug
    mwd->lr_reference_frame = (YV12_BUFFER_CONFIG *) vpx_calloc(1, sizeof(YV12_BUFFER_CONFIG)); //debug
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
    mwd->reset_cache = 0;
    mwd->cache_reset_profile = NULL;

    mwd->latency_log = NULL;
    mwd->metadata_log = NULL;
}

void init_mobinas_worker(mobinas_worker_data_t *mwd, int num_threads, mobinas_cfg_t *mobinas_cfg) {
    char file_path[PATH_MAX];

    assert (mobinas_cfg != NULL);
    assert (num_threads > 0);

    for (int i = 0; i < num_threads; ++i) {
        init_mobinas_worker_data(&mwd[i], i);

        if (mobinas_cfg->save_decode_result == 1) {
            memset(file_path, 0, PATH_MAX);
            sprintf(file_path, "%s/latency_%s_thread%d.log", mobinas_cfg->log_dir, mobinas_cfg->prefix, i);
            mwd[i].latency_log = fopen(file_path, "w");
            memset(file_path, 0, PATH_MAX);
            sprintf(file_path, "%s/metadata_%s_thread%d.log", mobinas_cfg->log_dir, mobinas_cfg->prefix, i);
            mwd[i].metadata_log = fopen(file_path, "w");
        }

        if (mobinas_cfg->decode_mode == DECODE_CACHE) {
            memset(file_path, 0, sizeof(file_path));
            sprintf(file_path, "%s/cache_reset_%s_thread%d", mobinas_cfg->profile_dir, mobinas_cfg->prefix, mwd[i].index);

            switch (mobinas_cfg->cache_mode) {
                case PROFILE_CACHE_RESET:
                    mwd[i].cache_reset_profile = init_mobinas_cache_reset_profile(file_path, 0);
                    if (mwd[i].cache_reset_profile == NULL) {
                        LOGE("%s: turn-off cache reset", __func__);
                        mobinas_cfg->cache_mode = NO_CACHE_RESET;
                    }
                    break;
                case APPLY_CACHE_RESET:
                    mwd[i].cache_reset_profile = init_mobinas_cache_reset_profile(file_path, 1);

                    if (mwd[i].cache_reset_profile == NULL) {
                        LOGE("%s: turn-off cache reset", __func__);
                        mobinas_cfg->cache_mode = NO_CACHE_RESET;
                    }
                    break;
            }
        }
    }
}

mobinas_bilinear_config_t *get_mobinas_bilinear_config(mobinas_bilinear_profile_t *bilinear_profile, int scale) {
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
        }
}

void init_mobinas_bilinear_config(mobinas_bilinear_config_t *config, int width, int height, int scale) {
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
        const float in_x = (x + 0.5f) / scale - 0.5f;
        config->left_x_index[x] = MAX(floor(in_x), 0);
        config->right_x_index[x] = MIN(ceil(in_x), width - 1);
        config->x_lerp[x] = in_x - floor(in_x);
        config->x_lerp_fixed[x] = config->x_lerp[x] * FRACTION_SCALE;
    }

    for (y = 0; y < height * scale; ++y) {
        const float in_y = (y + 0.5f) / scale - 0.5f;
        config->top_y_index[y] = MAX(floor(in_y), 0);
        config->bottom_y_index[y] = MIN(ceil(in_y), height - 1);
        config->y_lerp[y] = in_y - floor(in_y);
        config->y_lerp_fixed[y] = config->y_lerp[y] * FRACTION_SCALE;
    }
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

void init_mobinas_bilinear_profile(mobinas_bilinear_profile_t *profile){
    assert(profile != NULL);
    //scale x4
    init_mobinas_bilinear_config(&profile->config_TX_64X64_s4, 64, 64, 4);
    //scale x3
    init_mobinas_bilinear_config(&profile->config_TX_64X64_s3, 64, 64, 3);
    //scale x2
    init_mobinas_bilinear_config(&profile->config_TX_64X64_s2, 64, 64, 2);
}

void remove_bilinear_profile(mobinas_bilinear_profile_t *profile) {
    if (profile != NULL) {
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
