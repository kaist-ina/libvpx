#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void vpx_copy_c(const unsigned char *src_ptr, int src_stride,
                unsigned char *dst_ptr, int dst_stride, int height, int width)
{
    int r;

    for (r = 0; r < height; ++r)
    {
        memcpy(dst_ptr, src_ptr, width);

        src_ptr += src_stride;
        dst_ptr += dst_stride;
    }
}

uint64_t vpx_substract_c(const unsigned char *src_ptr, int src_stride,
                         unsigned char *dst_ptr, int dst_stride, int height,
                         int width)
{
    int r, c;
    uint64_t total_residual = 0;

    for (r = 0; r < height; ++r)
    {
        for (c = 0; c < width; ++c)
        {
            total_residual += abs(src_ptr[r * src_stride + c] - dst_ptr[r * dst_stride + c]);
        }
    }

    return total_residual;
}

void vpx_add_c(const int16_t *src_ptr, int src_stride,
               int16_t *dst_ptr, int dst_stride, int height,
               int width)
{
    int r, c;

    for (r = 0; r < height; ++r)
    {
        for (c = 0; c < width; ++c)
        {
            dst_ptr[r * dst_stride + c] += src_ptr[r * src_stride + c];
        }
    }
}

uint64_t vpx_sum_c(const int16_t *src_ptr, int src_stride,
                   int height, int width)
{
    int r, c;
    uint64_t total_residual = 0;

    for (r = 0; r < height; ++r)
    {
        for (c = 0; c < width; ++c)
        {
            total_residual += abs(src_ptr[r * src_stride + c]);
        }
    }

    return total_residual;
}

void vpx_print_c(const unsigned char *src_ptr, int src_stride,
                 int height, int width)
{
    int r, c;

    fprintf(stderr, "%d %d\n", height, width);
    for (r = 0; r < height; ++r)
    {
        for (c = 0; c < width; ++c)
        {
            fprintf(stderr, "%d\t", src_ptr[r * src_stride + c]);
        }
        fprintf(stderr, "%d\n", src_ptr[r * src_stride + c]);
    }
}