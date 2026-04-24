//
// ESP32-S3 SIMD optimized code
// Written by Larry Bank
// Copyright (c) 2024 BitBank Software, Inc.
// Project started Jan 21, 2024
//
#if defined (ARDUINO_ARCH_ESP32) && !defined(NO_SIMD)
#if __has_include ("dsps_fft2r_platform.h")
#include "dsps_fft2r_platform.h"
#if (dsps_fft2r_sc16_aes3_enabled == 1)
	.text
	.align 4

    .global s3_ycbcr_convert_420
    .type   s3_ycbcr_convert_420,@function

// Convert 16 pixels of YCbCr 4:2:0 to RGB565
//                                       A2            A3            A4              A5               A6             A7
// Call as void s3_ycbcr_convert_420(uint8_t *pY, uint8_t *pCB, uint16_t *pOut, int iPitch, int16_t *pConsts, uint8_t ucPixelType);
  s3_ycbcr_convert_420:
// supported pixel types: 0 = RGB656_LE, 1 = RGB565_BE, 2 = RGBA8888
    .align 4
    entry    a1,64
  movi.n  a8,2             # iteration count
  ee.vld.l.64.ip    q1,a3,128   # load 4 Cb values into Q1
  ee.vld.l.64.ip    q2,a3,0   # load 4 Cr values into Q2
  addi.n a3,a3,-120           # point to next 4 Cb/Cr values
.convert_420_loop:
  ee.vldbc.16.ip  q3,a6,2   # get constant 0x80 as 16-bits in all 128 bits of q3
  mv.qr q5,q1
  ee.vzip.16 q1,q5          # duplicate Cb values since it's subsampled
  mv.qr q5,q2
  ee.vzip.16 q2,q5          # duplicate Cr values
  ee.vsubs.s16 q1,q1,q3     # subtract 0x0080 from Cb's
  ee.vsubs.s16 q2,q2,q3     # subtract 0x0080 from Cr's
  ee.vldbc.16.ip  q3,a6,2   # get constant 1.77200 as 16-bits in all 128 bits of q3
  movi.n    a9,6           # load the shift register with 6
  wsr.sar    a9             # put it in the SAR (shift amount register)
  ee.vmul.s16 q1,q1,q3   # (Cb *= 1.77200) >> 6
  ee.vldbc.16.ip  q3,a6,2   # get constant 1.402 as 16-bits in all 128 bits of q3
  ee.vmul.s16 q2,q2,q3   # (Cr *= 1.402) >> 6
  ee.vldbc.16.ip  q3,a6,2   # get constant 0.34414 as 16-bits in all 128 bits of q3
  movi.n    a9,0           # load the shift register with 0
  wsr.sar    a9             # put it in the SAR (shift amount register)
  ee.vmul.s16 q4,q1,q3   # (Cb * 0.34414) >> 0
  ee.vldbc.16.ip  q3,a6,2   # get constant 0.71414 as 16-bits in all 128 bits of q3
  ee.vmul.s16 q3,q2,q3   # (Cr * 0.71414) >> 0
  ee.vadds.s16 q3,q3,q4  # (Cb * 0.34414) + (Cr * 0.71414)
  ee.vldbc.16.ip q4,a6,2   # get constant value of 1
  movi.n    a9,6           # load the shift register with 6
  wsr.sar    a9             # put it in the SAR (shift amount register)
  ee.vmul.s16 q3,q3,q4     # shift right after adding for improved precision
// We now have:
// Cb * 1.772 in Q1 = blue
// Cr * 1.402 in Q2 = red
// (Cr * 0.714 + Cb * 0.344) in Q3 = green
  ee.vld.128.ip    q0,a2,16   # load top left 8 Y values into Q0 and point to second line of Y for next load
  ee.vldbc.16.ip q5,a6,2  # get constant 16
  ee.vsubs.s16 q0,q0,q5   # Y -= 16;
  ee.vadds.s16 q5,q1,q0  # Cb += y (8 blue pixels in q5)
  ee.vadds.s16 q6,q2,q0  # Cr += y (8 red pixels in q6)
  ee.vsubs.s16 q7,q0,q3   # Y - ((Cb * 0.34414) + (Cr * 0.71414)) = green in Q7
// Saturate R, G and B to 8 bits
  ee.xorq q0,q0,q0
  ee.vmax.s16 q5,q5,q0
  ee.vmax.s16 q6,q6,q0
  ee.vmax.s16 q7,q7,q0
  ee.vldbc.16.ip q0,a6,2  # get constant 0x00ff
  ee.vmin.s16 q5,q5,q0         # clamp to 255
  ee.vmin.s16 q6,q6,q0
  ee.vmin.s16 q7,q7,q0
// Convert to RGB565
  movi.n    a9,3           # load the shift register with 3 (for blue and red)
  wsr.sar    a9             # put it in the SAR (shift amount register)
  ee.vldbc.16.ip  q0,a6,2 # get constant 1 in q0
  ee.vmul.s16 q5,q0,q5   # shift blue right by 3
  ee.vmul.s16 q6,q0,q6   # shift red right by 3
  movi.n    a9,2           # load the shift register with 2 (for green)
  wsr.sar    a9             # put it in the SAR (shift amount register)
  ee.vmul.s16 q7,q0,q7   # shift green right by 2
// now combine to form RGB565 pixels
  movi.n    a9,0
  wsr.sar    a9              # no shift after multiply
  ee.vldbc.16.ip  q0,a6,2   # get constant value 32 (to shift green left by 5 bits)
  ee.vmul.s16 q7,q7,q0   # shift green left by 5
  ee.vldbc.16.ip  q0,a6,2   # get constant value 2048 (to shift red left by 11 bits)
  ee.vmul.s16 q6,q6,q0   # shift red left by 11
  ee.orq q5,q7,q5        # combine blue + green
  ee.orq q5,q6,q5        # add red
  mv.qr q6,q5            # in case we're generating little endian output
  beqi a7,0,.rgb565_le # RGB565 little endian?
  ee.vunzip.8 q6,q5      # swap the byte order to be big-endian
  ee.vzip.8 q5,q6
.rgb565_le:
  ee.vst.128.ip q5,a4,0  # store 8 RGB565 pixels
  add a4,a4,a5           # point to next output row
// Prepare second row
  ee.vld.128.ip    q0,a2,112   # prepare to load top right block next time
  ee.vldbc.16.ip q5,a6,2  # get constant 16
  ee.vsubs.s16 q0,q0,q5   # Y -= 16;
  ee.vadds.s16 q5,q1,q0  # Cb += y (8 blue pixels in q5)
  ee.vadds.s16 q6,q2,q0  # Cr += y (8 red pixels in q6)
  ee.vsubs.s16 q7,q0,q4   # Y - ((Cb * 0.34414) + (Cr * 0.71414)) = green in Q7
  ee.vsubs.s16 q7,q7,q3
// Saturate R, G and B to 8 bits
  ee.xorq q0,q0,q0
  ee.vmax.s16 q5,q5,q0
  ee.vmax.s16 q6,q6,q0
  ee.vmax.s16 q7,q7,q0
  ee.vldbc.16.ip q0,a6,2  # get constant 0x00ff
  ee.vmin.s16 q5,q5,q0         # clamp to 255
  ee.vmin.s16 q6,q6,q0
  ee.vmin.s16 q7,q7,q0
// Convert to RGB565
  movi.n    a9,3           # load the shift register with 3 (for blue and red)
  wsr.sar    a9             # put it in the SAR (shift amount register)
  ee.vldbc.16.ip  q0,a6,2 # get constant 1 in q0
  ee.vmul.s16 q5,q0,q5   # shift blue right by 3
  ee.vmul.s16 q6,q0,q6   # shift red right by 3
  movi.n    a9,2           # load the shift register with 2 (for green)
  wsr.sar    a9             # put it in the SAR (shift amount register)
  ee.vmul.s16 q7,q0,q7   # shift green right by 2
// now combine to form RGB565 pixels
  movi.n    a9,0
  wsr.sar    a9              # no shift after multiply
  ee.vldbc.16.ip  q0,a6,2   # get constant value 32 (to shift green left by 5 bits)
  ee.vmul.s16 q7,q7,q0   # shift green left by 5
  ee.vldbc.16.ip  q0,a6,2   # get constant value 2048 (to shift red left by 11 bits)
  ee.vmul.s16 q6,q6,q0   # shift red left by 11
  ee.orq q5,q7,q5        # combine blue + green
  ee.orq q5,q6,q5        # add red
  mv.qr q6,q5            # in case we're generating little endian output
  beqi a7,0,.rgb565_le_2 # RGB565 little endian?
  ee.vunzip.8 q6,q5      # swap the byte order to be big-endian
  ee.vzip.8 q5,q6
.rgb565_le_2:
  ee.vst.128.ip q5,a4,0  # store 8 RGB565 pixels
// Now work on the top right block
  sub a4,a4,a5           # point to previous output row
  addi.n a4,a4,16        # next 8 pixels to the right
  addi.n a6,a6,-(16*2)   # restore constants to start of the list
  ee.vld.l.64.ip    q1,a3,128   # load 4 Cb values into Q1
  ee.vld.l.64.ip    q2,a3,0   # load 4 Cr values into Q2
  addi.n a8,a8,-1
  bnez.n a8,.convert_420_loop

  # return value of 0
    retw.n
#endif // dsps_fft2r_sc16_aes3_enabled
#endif // __has_include
#endif // ESP32
