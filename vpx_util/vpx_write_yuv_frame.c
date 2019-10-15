/*
 *  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vpx_dsp/skin_detection.h"
#include "vpx_util/vpx_write_yuv_frame.h"

#include <android/log.h>
#include <vpx_mem/vpx_mem.h>
#include <main.hpp>


#define TAG "vpx_write_yuv_frame.c JNI"
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

int vpx_write_y_frame(char *file_path, YV12_BUFFER_CONFIG *s){
    FILE *y_file = fopen(file_path, "wb");
    if(y_file == NULL)
    {
        LOGE("file open fail: %s", file_path);
        return -1;
    }

//    LOGD("y_crop_height: %d, y_crop_width: %d", s->y_crop_height, s->y_crop_width);

    unsigned char *src = s->y_buffer;
    int h = s->y_crop_height;

    do {
        fwrite(src, s->y_crop_width, 1, y_file);
        src += s->y_stride;
    } while (--h);

    fclose(y_file);
    return 0;
}

//chanju
unsigned char * vpx_write_yuv_frame_to_buffer(YV12_BUFFER_CONFIG *s){

    unsigned int array_size = s->y_crop_height * s->y_width; //y
    array_size += s->uv_crop_height * s->uv_width;  //u
    array_size += s->uv_crop_height * s->uv_width;  //v
    unsigned char * buffer = vpx_calloc(array_size,1);
    unsigned char * buffer_copy = buffer;

    unsigned char * src = s->y_buffer;
    int h = s->y_crop_height;
    do{
        memcpy(buffer,src,s->y_width);
        buffer += s->y_width;
        src += s->y_stride;
    }while(--h);


    src = s->u_buffer;
    h = s->uv_crop_height;
    do {
        memcpy(buffer, src, s->uv_width);
        buffer += s->uv_width;
        src += s->uv_stride;
    } while (--h);


    src = s->v_buffer;
    h = s->uv_crop_height;
    do {
        memcpy(buffer, src, s->uv_width);
        buffer += s->uv_width;
        src += s->uv_stride;
    } while (--h);


    return buffer_copy;
}


void vpx_write_yuv_frame(FILE *yuv_file, YV12_BUFFER_CONFIG *s) {
//#if defined(OUTPUT_YUV_SRC) || defined(OUTPUT_YUV_DENOISED) || \
//    defined(OUTPUT_YUV_SKINMAP)

    unsigned char * buffer = vpx_write_yuv_frame_to_buffer(s);
    unsigned int array_size = s->y_crop_height * s->y_width; //y
    array_size += s->uv_crop_height * s->uv_width;  //u
    array_size += s->uv_crop_height * s->uv_width;  //v
    fwrite(buffer, array_size,1,yuv_file);
    fclose(yuv_file);
    free(buffer);

    return;


  unsigned char *src = s->y_buffer;
  int h = s->y_crop_height;

  do {
    fwrite(src, s->y_width, 1, yuv_file);
    src += s->y_stride;
  } while (--h);

  src = s->u_buffer;
  h = s->uv_crop_height;

  do {
    fwrite(src, s->uv_width, 1, yuv_file);
    src += s->uv_stride;
  } while (--h);

  src = s->v_buffer;
  h = s->uv_crop_height;

  do {
    fwrite(src, s->uv_width, 1, yuv_file);
    src += s->uv_stride;
  } while (--h);

  //
  fclose(yuv_file);

//#else
//  (void)yuv_file;
//  (void)s;
//#endif
}
