//
// Created by hyunho on 9/2/19.
//

#include <memory.h>
#include <stdio.h>
#include <malloc.h>
#include <assert.h>
#include <vpx_scale/yv12config.h>
#include <vpx_mem/vpx_mem.h>
#include <sys/param.h>
#include <math.h>
#include <libgen.h>
#include <vpx_dsp/vpx_dsp_common.h>
#include <android/log.h>
#include <sys/time.h>


#include "vpx/vpx_mobinas.h"

#define BUFFER_UNIT_LEN 1000
#define FRACTION_BIT (5)
#define FRACTION_SCALE (1 << FRACTION_BIT)

mobinas_cfg_t *init_mobinas_cfg() {
    mobinas_cfg_t *config = (mobinas_cfg_t *) vpx_calloc(1, sizeof(mobinas_cfg_t));

    config->decode_mode = DECODE;
    config->dnn_mode = NO_DNN;
    config->cache_policy = NO_CACHE;
    config->cache_mode = NO_CACHE_RESET;

    return config;
}

void remove_mobinas_cfg(mobinas_cfg_t *config) {
    if (config) {
        remove_mobinas_cache_profile(config->cache_profile);
        remove_vp9_bilinear_profile(config->bilinear_profile);
        vpx_free(config);
    }
}

mobinas_cache_profile_t *init_mobinas_cache_profile(const char *path) {
    char tmp[PATH_MAX];

    mobinas_cache_profile_t *profile = (mobinas_cache_profile_t *) vpx_calloc(1, sizeof(mobinas_cache_profile_t));

    if ((profile->file = fopen(path, "rb")) == NULL) {
        fprintf(stderr, "%s: fail to open a file %s", __func__, path);
        vpx_free(profile);
        return NULL;
    }

    sprintf(tmp, "%s", path);
    sprintf(profile->name, "%s", basename(tmp));

    return profile;
}

void remove_mobinas_cache_profile(mobinas_cache_profile_t *profile) {
    if (profile) {
        if (profile->file) fclose(profile->file);
        vpx_free(profile);
    }
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

mobinas_worker_data_t *init_mobinas_worker(int num_threads, mobinas_cfg_t *mobinas_cfg) {
    char latency_log_path[PATH_MAX];
    char metadata_log_path[PATH_MAX];
    char cache_reset_profile_path[PATH_MAX];

    if (!mobinas_cfg) {
        fprintf(stderr, "%s: mobinas_cfg is NULL");
        return NULL;
    }

    if (num_threads <= 0) {
        fprintf(stderr, "%s: num_threads is equal or less than 0");
        return NULL;
    }

    mobinas_worker_data_t *mwd = (mobinas_worker_data_t *) vpx_malloc(sizeof(mobinas_worker_data_t) * num_threads);

    for (int i = 0; i < num_threads; ++i) {
        init_mobinas_worker_data(&mwd[i], i);

        if (mobinas_cfg->save_latency_result == 1) {
            switch (mobinas_cfg->decode_mode)
            {
            case DECODE:
                sprintf(latency_log_path, "%s/%s/log/latency_thread%d%d.log", mobinas_cfg->save_dir, mobinas_cfg->prefix,
                        mwd[i].index, num_threads);
                break;
            case DECODE_SR:
                sprintf(latency_log_path, "%s/%s/log/latency_sr_thread%d%d.log", mobinas_cfg->save_dir,
                        mobinas_cfg->prefix, mwd[i].index, num_threads);
                break;
            case DECODE_BILINEAR:
                sprintf(latency_log_path, "%s/%s/log/latency_bilinear_thread%d%d.log", mobinas_cfg->save_dir,
                        mobinas_cfg->prefix, mwd[i].index, num_threads);
                break;
            case DECODE_CACHE:
                switch (mobinas_cfg->cache_policy)
                {
                case KEY_FRAME_CACHE:
                    sprintf(latency_log_path, "%s/%s/log/latency_cache_key_frame_thread%d%d.log", mobinas_cfg->save_dir,
                            mobinas_cfg->prefix, mwd[i].index, num_threads);
                    break;
                case PROFILE_CACHE:
                    sprintf(latency_log_path, "%s/%s/log/latency_cache_%s_thread%d%d.log", mobinas_cfg->save_dir,
                            mobinas_cfg->prefix, mobinas_cfg->cache_profile->name, mwd[i].index, num_threads);
                    break;
                }
                break;
            }

            if ((mwd[i].latency_log = fopen(latency_log_path, "w")) == NULL) {
                fprintf(stderr, "%s: cannot open a file %s", __func__, latency_log_path);
                mobinas_cfg->save_latency_result = 0;
            }
        }

        if (mobinas_cfg->save_metadata_result == 1){
            switch (mobinas_cfg->decode_mode)
            {
            case DECODE:
                sprintf(metadata_log_path, "%s/%s/log/metadata_thread%d%d.log", mobinas_cfg->save_dir,
                        mobinas_cfg->prefix, mwd[i].index, num_threads);
                break;
            case DECODE_SR:
                sprintf(metadata_log_path, "%s/%s/log/metadata_sr_thread%d%d.log", mobinas_cfg->save_dir,
                        mobinas_cfg->prefix, mwd[i].index, num_threads);
                break;
            case DECODE_BILINEAR:
                sprintf(metadata_log_path, "%s/%s/log/metadata_bilinear_thread%d%d.log", mobinas_cfg->save_dir,
                        mobinas_cfg->prefix, mwd[i].index, num_threads);
                break;
            case DECODE_CACHE:
                switch (mobinas_cfg->cache_policy)
                {
                case KEY_FRAME_CACHE:
                    sprintf(metadata_log_path, "%s/%s/log/metadata_cache_key_frame_thread%d%d.log",
                            mobinas_cfg->save_dir, mobinas_cfg->prefix, mwd[i].index, num_threads);
                    break;
                case PROFILE_CACHE:
                    sprintf(metadata_log_path, "%s/%s/log/metadata_cache_%s_thread%d%d.log", mobinas_cfg->save_dir,
                            mobinas_cfg->prefix, mobinas_cfg->cache_profile->name, mwd[i].index, num_threads);
                    break;
                }
                break;
            }

            fprintf(stderr, "%s: %s\n", __func__, metadata_log_path);

            if ((mwd[i].metadata_log = fopen(metadata_log_path, "w")) == NULL)
            {
                fprintf(stderr, "%s: cannot open a file %s", __func__, metadata_log_path);
                mobinas_cfg->save_metadata_result = 0;
            }
        }

        if (mobinas_cfg->decode_mode == DECODE_CACHE) {
            sprintf(cache_reset_profile_path, "%s/profile/cache_reset_thread%d%d", mobinas_cfg->save_dir, mwd[i].index, num_threads);

            switch (mobinas_cfg->cache_mode) {
                case PROFILE_CACHE_RESET:
                    if ((mwd[i].cache_reset_profile = init_mobinas_cache_reset_profile(cache_reset_profile_path, 0)) == NULL) {
                        fprintf(stdout, "%s: turn-off cache reset", __func__);
                        mobinas_cfg->cache_mode = NO_CACHE_RESET;
                    }
                    break;
                case APPLY_CACHE_RESET:
                    if ((mwd[i].cache_reset_profile = init_mobinas_cache_reset_profile(cache_reset_profile_path, 1)) == NULL) {
                        fprintf(stdout, "%s: turn-off cache reset", __func__);
                        mobinas_cfg->cache_mode = NO_CACHE_RESET;
                    }
                    break;
            }
        }
    }

    return mwd;
}

bilinear_config_t *get_vp9_bilinear_config(vp9_bilinear_profile_t *bilinear_profile, int scale) {
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

void init_bilinear_config(bilinear_config_t *config, int width, int height, int scale) {
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

void remove_bilinear_config(bilinear_config_t *config) {
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
        case 360:
            return 3;
        case 480:
            return 2;
        default:
            return 1;
    }
}

void convert_yuv420_to_rgb(YV12_BUFFER_CONFIG * yv12, unsigned char * rgb_buffer, int test){

    unsigned char * temp = rgb_buffer;

    //Temporary buffer to store all y u v data
    int array_size = yv12->y_crop_height * yv12->y_width + 2 * (yv12->uv_crop_height * yv12->uv_width);
    unsigned char * buffer = vpx_calloc(1, array_size);
    unsigned char * buffer_copy = buffer;
    unsigned char * y_buffer;
    unsigned char * u_buffer;
    unsigned char * v_buffer;

    /* Write to single buffer first */
    y_buffer = buffer;
    unsigned char * src = yv12->y_buffer;
    int h = yv12->y_crop_height;
    do{
        memcpy(buffer, src, yv12->y_width);
        buffer += yv12->y_width;
        src += yv12->y_stride;
    }while(--h);

    u_buffer = buffer;
    src = yv12->u_buffer;
    h = yv12->uv_crop_height;
    do{
        memcpy(buffer, src, yv12->uv_width);
        buffer += yv12->uv_width;
        src += yv12->uv_stride;
    }while(--h);

    v_buffer = buffer;
    src = yv12->v_buffer;
    h = yv12->uv_crop_height;
    do{
        memcpy(buffer, src, yv12->uv_width);
        buffer += yv12->uv_width;
        src += yv12->uv_stride;
    }while(--h);


    /* Convert to RGB */
    uint8_t y,u,v,r,g,b;
    int width = yv12->y_width;
    int height = yv12->y_crop_height;

    for(int i = 0; i < height; i++){
        for(int j = 0; j < width; j++){
            y = *(y_buffer + i * width + j);
            u = *(u_buffer + j/2 + width/2 * i/2);
            v = *(v_buffer + j/2 + width/2 * i/2);
            r = 0;
            g = 0;
            b = 0;
            YUV2RGB(&r, &g, &b, y, u, v);
            *rgb_buffer = r;
            *(rgb_buffer + 1) = g;
            *(rgb_buffer + 2) = b;
            rgb_buffer += 3;
        }
    }

    //write to file
    if(test == 20){
        char file_name[100];
        sprintf(file_name, "/sdcard/SNPEData/frame/testyo");
        FILE * yuv_file = fopen(file_name, "wb");
        fwrite(temp, 3 * height * width, 1, yuv_file);
        fclose(yuv_file);
    }


    free(buffer_copy);
}

void convert_sr_rgb_to_yuv420(float * sr_rgb_buffer, YV12_BUFFER_CONFIG * yv12){
    uint8_t y,u,v,r,g,b;

    int width = yv12->y_width;
    int height = yv12->y_crop_height;

//    __android_log_print(ANDROID_LOG_ERROR, "TAGG", "check: %d, %d, %d, %d",
//            width, yv12->uv_width, yv12->y_stride, yv12->uv_stride);

//    __android_log_print(ANDROID_LOG_ERROR, "TAGG", "check:  %d", yv12->uv_crop_height);

    unsigned char * y_pointer = yv12->y_buffer;
    unsigned char * u_pointer = yv12->u_buffer;
    unsigned char * v_pointer = yv12->v_buffer;

    for(int i = 0; i < height; i++){
        for(int j = 0; j<width;j++){
            r = (uint8_t) clamp(round(*sr_rgb_buffer), 0, 255);
            g = (uint8_t) clamp(round(*(sr_rgb_buffer+1)), 0, 255);
            b = (uint8_t) clamp(round(*(sr_rgb_buffer+2)), 0, 255);
            y = 0;
            u = 0;
            v = 0;
            RGB2YUV(&y,&u,&v,r,g,b);

            *(y_pointer++) = y;
            if( (i % 2 == 0) && ( j % 2 == 0)){
                *(u_pointer++) = u;
                *(v_pointer++) = v;
            }

            sr_rgb_buffer += 3;
        }
    }
}

void YUV2RGB(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t y, uint8_t u, uint8_t v){
    double rTmp = 1.164383 * (y-16) + 1.596027 * (v-128);
    double gTmp = 1.164383 * (y-16) - 0.391762 * (u - 128) - 0.812968 * (v - 128);
    double bTmp = 1.164383 * (y-16) + 2.017232 * (u - 128);

    *r = clamp(rTmp,0,255);
    *g = clamp(gTmp,0,255);
    *b = clamp(bTmp,0,255);
}

void RGB2YUV(uint8_t *y, uint8_t *u, uint8_t *v, uint8_t r, uint8_t g, uint8_t b){

    double yTmp = round( (0.256788 * r + 0.504129 * g + 0.097906 * b) + 16);
    double uTmp = round( (-0.148223 * r - 0.290993 * g + 0.439216 * b) + 128);
    double vTmp = round( (0.439216 * r - 0.367788 * g - 0.071427 * b) + 128);

    *y = clamp(yTmp,0,255);
    *u = clamp(uTmp,0,255);
    *v = clamp(vTmp,0,255);
}

void printTime(int checkpoint, struct timeval * begin){
    struct timeval subtract, now;

    gettimeofday(&now,NULL);
    timersub(&now,begin,&subtract);
//    __android_log_print(ANDROID_LOG_ERROR, "TAGG", "%d: %ld.%06ld", checkpoint, subtract.tv_sec,subtract.tv_usec);
}

void saveToFile(float * sr_rgb_buffer, int height, int width, int print, int frame_number){
    if(print){
        //save to file

        unsigned char * temp_buffer = vpx_calloc(1, height*width*3);
        unsigned char * temp_buffer_copy = temp_buffer;
        for(int i = 0; i < height*width*3; i++){
            *temp_buffer = (uint8_t) clamp(round(*sr_rgb_buffer), 0, 255);
            temp_buffer++;
            sr_rgb_buffer++;
        }

        char file_name[100];
        sprintf(file_name, "/sdcard/SNPEData/frame/%dSR", frame_number);
        FILE * file = fopen(file_name, "wb");
        fwrite(temp_buffer_copy, height * width * 3, 1, file);
        vpx_free(temp_buffer_copy);
        fclose(file);
    }
}



//unsigned char * yv12_to_single_buffer(YV12_BUFFER_CONFIG * s){
//    __android_log_print(ANDROID_LOG_ERROR, "TAGG", "%d %d %d %d %d %d",s->y_crop_height, s->y_width,s->y_stride,s->uv_crop_height, s->uv_width, s->uv_stride);
//
//    unsigned int array_size = s->y_crop_height * s->y_width; //y
//    array_size += s->uv_crop_height * s->uv_width;  //u
//    array_size += s->uv_crop_height * s->uv_width;  //v
//    unsigned char * buffer = (unsigned char *) vpx_calloc(array_size,1);
//    unsigned char * buffer_copy = buffer;
//
//    __android_log_print(ANDROID_LOG_ERROR, "TAGG", "%d %d %d %d %d %d",s->y_crop_height, s->y_width,s->y_stride,s->uv_crop_height, s->uv_width, s->uv_stride);
//
//
//    unsigned char * src = s->y_buffer;
//    int h = s->y_crop_height;
//    do{
//        memcpy(buffer,src,s->y_width);
//        buffer += s->y_width;
//        src += s->y_stride;
//    }while(--h);
//
//
//    src = s->u_buffer;
//    h = s->uv_crop_height;
//    do {
//        memcpy(buffer, src, s->uv_width);
//        buffer += s->uv_width;
//        src += s->uv_stride;
//    } while (--h);
//
//
//    src = s->v_buffer;
//    h = s->uv_crop_height;
//    do {
//        memcpy(buffer, src, s->uv_width);
//        buffer += s->uv_width;
//        src += s->uv_stride;
//    } while (--h);
//
//
//    return buffer_copy;
//}
//
//unsigned char * single_buffer_yuv_to_rgb(unsigned char * yuv, int width, int height,
//                                         unsigned char * y_pointer, unsigned char * u_pointer, unsigned char * v_pointer){
//
//    unsigned char * rgb_buffer = (unsigned  char*) vpx_calloc(width*height*3,1);
//    unsigned char * rgb_buffer_copy = rgb_buffer;
//
//    uint8_t y,u,v,r,g,b;
//
//    for(int i =0; i < height; i++){
//        for(int j = 0; j < width; j++){
//            y = *(y_pointer + i * width + j);
//            u = *(u_pointer + j/2 + width/2 * i/2);
//            v = *(v_pointer + j/2 + width/2 * i/2);
//            r = 0;
//            g = 0;
//            b = 0;
//            YUV2RGB(&r, &g, &b, y, u, v);
//            *rgb_buffer = r;
//            *(rgb_buffer + 1) = g;
//            *(rgb_buffer + 2) = b;
//            rgb_buffer += 3;
//        }
//    }
//
//    return rgb_buffer_copy;
//}
//
//void printToFile(char * name, int buffer_size, unsigned char * buffer){
//    FILE * file = fopen(name, "wb");
//    fwrite(buffer, buffer_size, 1, file);
//    fclose(file);
//}
//
//void sr_yv12_to_rgb_and_print(YV12_BUFFER_CONFIG * yv12, int frame_number){
//    unsigned char * yuv_buffer = yv12_to_single_buffer(yv12);
//    __android_log_print(ANDROID_LOG_ERROR, "TAGG", "verify 1");
//
//
//    int width = yv12->y_width;
//    int height = yv12->y_crop_height;
//    unsigned char * y_pointer = yuv_buffer;
//    unsigned char * u_pointer = y_pointer + width * height;
//    unsigned char * v_pointer = u_pointer + yv12->uv_crop_height * yv12->uv_width;
//    unsigned char * rgb_buffer = single_buffer_yuv_to_rgb(yuv_buffer, width, height, y_pointer,u_pointer,v_pointer);
//
//    __android_log_print(ANDROID_LOG_ERROR, "TAGG", "verify 2");
//
//    //print to file
//    char file_name[100];
//    sprintf(file_name, "/sdcard/SNPEData/frame/verify%d", frame_number);
//    printToFile(file_name, 3 * height*width,rgb_buffer);
//
//    __android_log_print(ANDROID_LOG_ERROR, "TAGG", "verify 3");
//
//    //free
//    vpx_free(yuv_buffer);
//    vpx_free(rgb_buffer);
//    __android_log_print(ANDROID_LOG_ERROR, "TAGG", "verify 4");
//
//}

