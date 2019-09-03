//
// Created by hyunho on 9/2/19.
//

#include <memory.h>
#include <stdio.h>
#include <malloc.h>
#include <assert.h>
#include <android/log.h>
#include "vpx/vpx_cache.h"
#include "../vpx_cache.h"

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

//TODO: reset cache reset
vpx_cache_reset_profile_t* vpx_init_cache_reset_profile(const char* path, int load_profile) {
    assert (path != NULL);

    vpx_cache_reset_profile_t *profile = (vpx_cache_reset_profile_t*) malloc(sizeof(vpx_cache_reset_profile_t));

    if (load_profile) {
        memset(profile, 0, sizeof(vpx_cache_reset_profile_t));
        profile->file = fopen(path, "rb");
        if (profile->file == NULL) {
            LOGE("%s: cannot load a file %s", __func__, path);
            return NULL;
        }
    }
    else {
        profile->file = fopen(path, "wb");
        if (profile->file == NULL) {
            LOGE("%s: cannot create a file %s", __func__, path);
            return NULL;
        }

        profile->offset = 0;
        profile->length = BUFFER_UNIT_LEN;
        profile->buffer = (uint8_t *) malloc(sizeof(uint8_t) * BUFFER_UNIT_LEN);
        memset(profile->buffer, 0, sizeof(uint8_t) * BUFFER_UNIT_LEN);
    }

    return profile;
}

void vpx_remove_cache_reset_profile(vpx_cache_reset_profile_t *profile) {
    if (profile != NULL) {
        if (profile->buffer != NULL) free(profile->buffer);
        if (profile->file != NULL) fclose(profile->file);
        free(profile);
    }
}

//offset 정보랑 buffer 저장
int vpx_read_cache_reset_profile(vpx_cache_reset_profile_t *profile) {
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

int vpx_write_cache_reset_profile(vpx_cache_reset_profile_t *profile) {
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

uint8_t vpx_read_cache_reset_bit(vpx_cache_reset_profile_t *profile){
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

void vpx_write_cache_reset_bit(vpx_cache_reset_profile_t *profile, uint8_t value){
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

//    LOGD("byte_offset: %d, bit_offset: %d, value (offset): %d, value: %d, %d", byte_offset, bit_offset, value, profile->buffer[byte_offset], profile->buffer[byte_offset] | value);

    profile->offset += 1;
}