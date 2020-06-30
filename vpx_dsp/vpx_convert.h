//
// Created by hyunho on 6/30/20.
//

#ifndef LIBVPX_WRAPPER_VPX_CONVERT_H
#define LIBVPX_WRAPPER_VPX_CONVERT_H


#define BUFFER_UNIT_LEN 1000
#define CONVERT_FRACTION_BIT 6
static const int CONVERT_FRACTION_SCALE = (1 << CONVERT_FRACTION_BIT);
static const int CONVERT_DELTA = (1 << (CONVERT_FRACTION_BIT - 1));

/* Matric coefficients: RGB to YUV */
static const float RY_COEFF_FLOAT = 0.183;
static const float GY_COEFF_FLOAT = 0.614;
static const float BY_COEFF_FLOAT = 0.062;
static const float RU_COEFF_FLOAT = 0.101;
static const float GU_COEFF_FLOAT = 0.339;
static const float BU_COEFF_FLOAT = 0.439;
static const float RV_COEFF_FLOAT = 0.439;
static const float GV_COEFF_FLOAT = 0.399;
static const float BV_COEFF_FLOAT = 0.040;

static const int RY_COEFF_INT = (RY_COEFF_FLOAT * CONVERT_FRACTION_SCALE + 0.5);
static const int GY_COEFF_INT = (GY_COEFF_FLOAT * CONVERT_FRACTION_SCALE + 0.5);
static const int BY_COEFF_INT = (BY_COEFF_FLOAT * CONVERT_FRACTION_SCALE + 0.5);
static const int RU_COEFF_INT = (RU_COEFF_FLOAT * CONVERT_FRACTION_SCALE + 0.5);
static const int GU_COEFF_INT = (GU_COEFF_FLOAT * CONVERT_FRACTION_SCALE + 0.5);
static const int BU_COEFF_INT = (BU_COEFF_FLOAT * CONVERT_FRACTION_SCALE + 0.5);
static const int RV_COEFF_INT = (RV_COEFF_FLOAT * CONVERT_FRACTION_SCALE + 0.5);
static const int GV_COEFF_INT = (GV_COEFF_FLOAT * CONVERT_FRACTION_SCALE + 0.5);
static const int BV_COEFF_INT = (BV_COEFF_FLOAT * CONVERT_FRACTION_SCALE + 0.5);

static const int Y_OFFSET = 16;
static const int U_OFFSET = 128;
static const int V_OFFSET = 128;

/* Matric coefficients: YUV to RGB */
static const float YR_COEFF_FLOAT = 1.164;
static const float UR_COEFF_FLOAT = 0;
static const float VR_COEFF_FLOAT = 1.793;
static const float YG_COEFF_FLOAT = 1.164;
static const float UG_COEFF_FLOAT = 0.213;
static const float VG_COEFF_FLOAT = 0.533;
static const float YB_COEFF_FLOAT = 1.164;
static const float UB_COEFF_FLOAT = 2.112;
static const float VB_COEFF_FLOAT = 0;

static const int YR_COEFF_INT = (YR_COEFF_FLOAT * CONVERT_FRACTION_SCALE + 0.5);
static const int UR_COEFF_INT = (UR_COEFF_FLOAT * CONVERT_FRACTION_SCALE + 0.5);
static const int VR_COEFF_INT = (VR_COEFF_FLOAT * CONVERT_FRACTION_SCALE + 0.5);
static const int YG_COEFF_INT = (YG_COEFF_FLOAT * CONVERT_FRACTION_SCALE + 0.5);
static const int UG_COEFF_INT = (UG_COEFF_FLOAT * CONVERT_FRACTION_SCALE + 0.5);
static const int VG_COEFF_INT = (VG_COEFF_FLOAT * CONVERT_FRACTION_SCALE + 0.5);
static const int YB_COEFF_INT = (YB_COEFF_FLOAT * CONVERT_FRACTION_SCALE + 0.5);
static const int UB_COEFF_INT = (UB_COEFF_FLOAT * CONVERT_FRACTION_SCALE + 0.5);
static const int VB_COEFF_INT = (VB_COEFF_FLOAT * CONVERT_FRACTION_SCALE + 0.5);

static const int R_OFFSET = ((Y_OFFSET * YR_COEFF_FLOAT + V_OFFSET * VR_COEFF_FLOAT) * CONVERT_FRACTION_SCALE + 0.5);
static const int G_OFFSET = ((-Y_OFFSET * YG_COEFF_FLOAT + U_OFFSET * UG_COEFF_FLOAT + V_OFFSET * VG_COEFF_FLOAT) * CONVERT_FRACTION_SCALE + 0.5);
static const int B_OFFSET = ((Y_OFFSET * YB_COEFF_FLOAT + U_OFFSET * UB_COEFF_FLOAT) * CONVERT_FRACTION_SCALE + 0.5);

#endif //LIBVPX_WRAPPER_VPX_CONVERT_H
