//
// Created by hyunho on 9/2/19.
//

#ifndef LIBVPX_WRAPPER_VPX_SR_CACHE_H
#define LIBVPX_WRAPPER_VPX_SR_CACHE_H

#include "./vpx_config.h"

typedef struct vpx_cache_reset_profile {
    FILE *file;
    int offset;
    int length;
    uint8_t *buffer;
} vpx_cache_reset_profile_t;

#ifdef __cplusplus
extern "C" {
#endif

vpx_cache_reset_profile_t* vpx_init_cache_reset_profile(const char* path, int load_profile);
void vpx_remove_cache_reset_profile(vpx_cache_reset_profile_t *profile);
int vpx_read_cache_reset_profile(vpx_cache_reset_profile_t *profile);
int vpx_write_cache_reset_profile(vpx_cache_reset_profile_t *profile);
uint8_t vpx_read_cache_reset_bit(vpx_cache_reset_profile_t *profile);
void vpx_write_cache_reset_bit(vpx_cache_reset_profile_t *profile, uint8_t value);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif //LIBVPX_WRAPPER_VPX_SR_CACHE_H
