#!/bin/sh
./vpxdec --codec=vp9 --progress --summary --noblit --threads=1 --frame-buffers=50 --limit=120 \
    --content-dir=/home/hyunho/MobiNAS/super_resolution/data/movie \
    --input-video=1080p_lossless_60sec_125st.webm \
    --save-final
./vpxdec --codec=vp9 --progress --summary --noblit --threads=1 --frame-buffers=50 --limit=120 \
    --content-dir=/home/hyunho/MobiNAS/super_resolution/data/movie \
    --input-video=1080p_270p_60sec_125st_EDSR_transpose_B8_F32_S4.webm \
    --save-intermediate
