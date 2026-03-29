//
// ESP32-S3 SIMD optimized code
// Written by Larry Bank
// Copyright (c) 2026 BitBank Software, Inc.
// Project started March 29, 2026
// This set of functions accelerates H.263 macroblock processing
//
#if defined (ARDUINO_ARCH_ESP32) && !defined(NO_SIMD)
#if __has_include ("dsps_fft2r_platform.h")
#include "dsps_fft2r_platform.h"
#if (dsps_fft2r_sc16_aes3_enabled == 1)
	.text
	.align 4

    .global s3_simd_mb
    .type   s3_simd_mb,@function

// Process 8x8 blocks of 16-bit values based on the operation type
// 0: full pel x,y
// 1: half pel x, full pel y
// 2: full pel x, half pel y
// 3: half pel x,y
//                               A2            A3            A4           A5               A6        A7
// Call as void s3_simd_mb(uint8_t u8Type, int16_t *pS, int16_t *pD, uint32_t iPitch, int16_t *pConstants);
  s3_simd_mb:
    .align 4
    entry    a1,64
    movi.n  a8,8             # cy loop count
    ee.vldbc.16.ip q5,a6,2  # get 1 to be able to right shift 16-bit values with multiply instructions
    ee.vldbc.16.ip q6,a6,2  # get min value of results (-2048)
    ee.vldbc.16.ip q7,a6,2  # get max value of results (+2047)
    beqi a2,1,.half_x_full_y
    beqi a2,2,.full_x_half_y
    beqi a2,3,.half_x_half_y
// full pixel in both x and y (0)
.full_x_full_y:
    ee.vld.128.ip q0,a3,0   # read 8 source values
    ee.vld.128.ip q1,a4,0   # read 8 destination values
    ee.vadds.s16 q0,q1,q0   # sum src+dest
    ee.vmin.s16 q0,q7,q0    # clamp to max of 2047
    ee.vmax.s16 q0,q6,q0    # clamp to min of -2048
    ee.vst.128.ip q0,a4,16  # store results
    add a3,a3,a5            # pS += iPitch
    addi.n a8,a8,-1         # decrement loop count
    bnez.n a8,.full_x_full_y
    retw.n                  # return

// half pixel in x, full pixel in y
.half_x_full_y:
    movi.n    a9,1          # load the shift register with 1 (for averaging)
    wsr.sar    a9           # put it in the SAR (shift amount register)
    movi.n    a9,2          # shift bytes 2 (one short) for unaligned col+1
    wur.sar_byte a9
    addi.n a5,a5,-16        # adjust source pitch
.h_x_f_y_0:
    ee.vld.128.ip q0,a3,16  # read 8 source values
    ee.vld.128.ip q1,a3,0   # read next 8 values to get col+1
    ee.src.q q1,q0,q1       # now q1 has the col+1 values
    ee.vld.128.ip q2,a4,0   # read 8 destination values
    ee.vadds.s16 q0,q1,q0   # sum s[cx]+s[cx+1] values
    ee.vadds.s16 q0,q0,q5   # +1 for rounding up
    ee.vmul.s16 q0,q0,q5    # multiply by "1" and then shift right by 1
    ee.vadds.s16 q0,q0,q2   # add destination values
    ee.vmin.s16 q0,q7,q0    # clamp to max of 2047
    ee.vmax.s16 q0,q6,q0    # clamp to min of -2048
    ee.vst.128.ip q0,a4,16  # store results
    add a3,a3,a5            # pS += iPitch
    addi.n a8,a8,-1         # decrement loop count
    bnez.n a8,.h_x_f_y_0
    retw.n                  # return

.full_x_half_y:
    movi.n    a9,1          # load the shift register with 1 (for averaging)
    wsr.sar    a9           # put it in the SAR (shift amount register)
    ee.vld.128.ip q0,a3,0   # preload first line
    add a3,a3,a5            # pS += iPitch
.f_x_h_y_0:
    ee.vld.128.ip q1,a3,0   # read 8 source values
    ee.vld.128.ip q2,a4,0   # read 8 destination values
    ee.vadds.s16 q0,q1,q0   # sum s[cx]+s[cx+iPitch] values
    ee.vadds.s16 q0,q0,q5   # +1 for rounding up
    ee.vmul.s16 q0,q0,q5    # multiply by "1" and then shift right by 1
    ee.vadds.s16 q0,q0,q2   # add destination values
    ee.vmin.s16 q0,q7,q0    # clamp to max of 2047
    ee.vmax.s16 q0,q6,q0    # clamp to min of -2048
    ee.vst.128.ip q0,a4,16  # store results
    add a3,a3,a5            # pS += iPitch
    mv.qr q0,q1          # old becomes new
    addi.n a8,a8,-1         # decrement loop count
    bnez.n a8,.f_x_h_y_0
    retw.n                  # return

// half pixel in x, half pixel in y
.half_x_half_y:
    ee.vldbc.16.ip q4,a6,2  # get 2 to right shift 16-bit values with mul
    movi.n    a9,2          # load the shift register with 2 (for averaging)
    wsr.sar    a9           # put it in the SAR (shift amount register)
    movi.n    a9,2          # shift bytes 2 (one short) for unaligned col+1
    wur.sar_byte a9
    addi.n a5,a5,-16        # adjust source pitch
    ee.vld.128.ip q0,a3,16  # preload 8 source values
    ee.vld.128.ip q1,a3,0   # read next 8 values to get col+1
    ee.src.q q1,q0,q1       # now q1 has the col+1 values
    ee.vadds.s16 q0,q0,q1   # sum horizontally
    add a3,a3,a5            # pS += iPitch
.h_x_h_y_0:
    ee.vld.128.ip q2,a3,16  # read 8 source values
    ee.vld.128.ip q3,a3,0   # read next 8 values to get col+1
    ee.src.q q3,q2,q3       # now q3 has the col+1 values
    ee.vadds.s16 q2,q2,q3   # sum s[cx+iPitch] + s[cx+iPitch+1]
    ee.vld.128.ip q3,a4,0   # read 8 destination values
    ee.vadds.s16 q0,q2,q0   # sum top+bottom rows
    ee.vadds.s16 q0,q0,q4   # +2 for rounding up
    ee.vmul.s16 q0,q0,q5    # multiply by "1" and then shift right by 2
    ee.vadds.s16 q0,q0,q3   # add destination values
    ee.vmin.s16 q0,q7,q0    # clamp to max of 2047
    ee.vmax.s16 q0,q6,q0    # clamp to min of -2048
    add a3,a3,a5            # pS += iPitch
    ee.vst.128.ip q0,a4,16  # store destination results
    mv.qr q0,q2             # old becomes new
    addi.n a8,a8,-1         # decrement loop count
    bnez.n a8,.h_x_h_y_0
    retw.n                  # return

#endif // dsps_fft2r_sc16_aes3_enabled
#endif // __has_include
#endif // ESP32
