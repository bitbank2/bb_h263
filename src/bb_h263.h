//
// bb_h263
// Written by Larry Bank (bitbank@pobox.com)
// Project started 3/22/2026
//
// SPDX-FileCopyrightText: 2026 BitBank Software, Inc.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#ifndef __BB_H263__
#define __BB_H263__

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DCTSIZE2 64
#define MB_LOWER -2048
#define MB_UPPER 2047
#define MCU0 (DCTSIZE2*0)
#define MCU1 (DCTSIZE2*1)
#define MCU2 (DCTSIZE2*2)
#define MCU3 (DCTSIZE2*3)
#define MCU4 (DCTSIZE2*4)
#define MCU5 (DCTSIZE2*5)
#define MOTOSHORT(p) (__builtin_bswap16(*(uint16_t *)p))
#define MOTOLONG(p) (__builtin_bswap32(*(uint32_t *)p))

enum {
    H263_SUCCESS = 0,
    H263_DECODE_ERROR,
    H263_FILEIO_ERROR,
    H263_INVALID_PARAMETER,
    H263_INVALID_FILE,
};

enum {
    H263_PIXEL_RGB565_LE = 0,
    H263_PIXEL_RGB565_BE
};

typedef struct H263_file_tag
{
  int32_t iPos; // current file position
  int32_t iSize; // file size
  uint8_t *pData; // memory file pointer
  void * fHandle; // class pointer to File/SdFat or whatever you want
} H263FILE;

typedef struct H263_draw_tag
{
    int x, y; // upper left corner of this block
    int iPitch; // bytes per row (not pixels)
    int iWidth, iHeight; // size of this pixel block
    uint16_t *pPixels; // 16-bit pixels
    void *pUser;
} H263DRAW;

// Callback function prototypes
typedef int32_t (H263_READ_CALLBACK)(H263FILE *pFile, uint8_t *pBuf, int32_t iLen);
typedef int32_t (H263_SEEK_CALLBACK)(H263FILE *pFile, int32_t iPosition);
typedef void (H263_DRAW_CALLBACK)(H263DRAW *pDraw);
typedef void * (H263_OPEN_CALLBACK)(const char *szFilename, uint32_t *pFileSize);
typedef void (H263_CLOSE_CALLBACK)(void *pHandle);

typedef struct tagvideo {
    int iWidth;
    int iHeight;
    int iXOffset, iYOffset; // placement on the display
    H263_READ_CALLBACK *pfnRead;
    H263_SEEK_CALLBACK *pfnSeek;
    H263_DRAW_CALLBACK *pfnDraw;
    H263_CLOSE_CALLBACK *pfnClose;
    H263FILE H263File;
    void *pUser;
    int iFrameCX; // width in whole macroblocks (multiple of 16)
    int iFrameCY; // height in whole macroblocks (multiple of 16)
    int iStreamOff; // current offset in the file
    uint8_t *pStream; // pointer to memory block data source
    int iFileOff; // offset into the already-read data
    int iFileLen; // length of data currently in the file buf
    int iFileHighWater; // high water mark for data in the file buffer
    int iOptions; // conversion options
    int iDCY, iDCCb, iDCCr; // DC predictors
    int iFramePitch;
    uint8_t *pFramebuffer;
    uint8_t *pFileBuf; // 32k for working with file data
    uint8_t *pVideo; // compressed video buffer
    uint8_t *pAudio; // compressed audio buffer
    int iFRefFrame; // frame number of forward reference frame
    int16_t *pBRef[3]; // backward reference frame
    int16_t *pFRef[3]; // forward reference frame
    uint16_t *pACTables;
    uint32_t ulBits;
    int iVideoLen; // bytes of video data available
    int iVideoOff; // current offset in video data
    int iVideoHighWater; // high water mark for reading more data
    int iAudioLen; // bytes of audio data available
    int iAudioOff; // current offset in audio data
    int iAudioHighWater;
    int iBitRate;
    int iVBVBufferSize;
    int iCurrentFrame;
    int iLastError;
    int bPacketized; // indicates if the data stream is in packets or is raw video (single stream)
    int iFrameTotal;
    uint32_t iFrameDelay;
    int16_t MCUs[6*DCTSIZE2];
    uint8_t ucIntraQuant[64];
    uint8_t ucNonIntraQuant[64];
    uint8_t cRangeTable2[1024]; // clipping table
    int8_t cMVPredX[128]; // current and previous motion vector predictors
    int8_t cMVPredY[128]; // needed for h.263
    uint8_t ucPelAspect, cQuantizerScale, u8PixelType;
    uint16_t *usYUVRGB; // lookup table for colorspace conversion
} H263STATE;

// Forward declarations
int GetH263MCU(uint32_t *pTable, uint8_t *buf, int16_t *pMCU, int *iOffset, int *iBitnum, H263STATE *pVideo, int iQuant, int bTCOEF, uint8_t ucMBType);
void H263MotCompAVG(int x, int y, signed int *pMVs, signed short *pMCUDest, H263STATE *pVideo);
void H263MotComp(int x, int y, signed int iMV_X, signed int iMV_Y, signed short *pMCUDest, H263STATE *pVideo, int bBackward);
void H263Close(H263STATE *pState);
int ReadH263(H263STATE *pVideo);

#ifdef __LINUX__

#endif // __LINUX__
static void * linuxOpen(const char *filename, uint32_t *size) {
    static FILE *myfile;
    size_t len;
    printf("Attempting to open %s\n", filename);
    myfile = fopen(filename, "r+b");
    if (myfile) {
        fseek(myfile, 0, SEEK_END);
        len = ftell(myfile);
        *size = (uint32_t)len;
        fseek(myfile, 0, SEEK_SET);
        return myfile;
    }
  return NULL;
} /* linuxOpen() */

static void linuxClose(void *handle) {
  FILE *pFile = (FILE *)handle;
  if (pFile) fclose(pFile);
} /* linuxClose() */

static int32_t linuxRead(H263FILE *handle, uint8_t *buffer, int32_t length) {
    FILE *pFile = (FILE *)handle->fHandle;
    int32_t len;
    if (!pFile) return 0;
    len = (int32_t)fread(buffer, 1, length, pFile);
    handle->iPos += len;
    return len;
}

static int32_t linuxSeek(H263FILE *handle, int32_t position) {
    FILE *pFile = (FILE *)handle->fHandle;
    if (!pFile) return 0;
    fseek(pFile, position, SEEK_SET);
    handle->iPos = (int32_t)ftell(pFile);
    return handle->iPos;
}
#ifdef __cplusplus
//
// The BB_H263 class wraps portable C code which does the actual work
//
class BB_H263
{
    
  public:
    BB_H263() {memset(&_h263, 0, sizeof(_h263));}
    int decodeFrame(int x, int y);
    int open(const uint8_t *pData, int iDataSize, H263_DRAW_CALLBACK *pDraw);
    int open(const char *szFilename, H263_OPEN_CALLBACK *pfnOpen, H263_CLOSE_CALLBACK *pfnClose, H263_READ_CALLBACK *pfnRead, H263_SEEK_CALLBACK *pfnSeek, H263_DRAW_CALLBACK *pfnDraw);
#ifdef __LINUX__
    int open(const char *szFilename, H263_DRAW_CALLBACK *pfnDraw);
#endif
    void setFramebuffer(uint8_t *pFramebuffer, int iPitch) { _h263.pFramebuffer = pFramebuffer; _h263.iFramePitch = iPitch;}
    void close() {H263Close(&_h263);}
    int getWidth() {return _h263.iWidth;}
    int getHeight() {return _h263.iHeight;}
    int getFrameCount() {return _h263.iFrameTotal;}
    uint32_t getFrameDelay() {return _h263.iFrameDelay;}
    void setUserPointer(void *p) { _h263.pUser = p;}
    void setPixelType(uint8_t u8Type) { _h263.u8PixelType = u8Type;} // defaults to little endian
    uint8_t getPixelType() {return _h263.u8PixelType;}

  private:
    H263STATE _h263;
}; // class H263

// Class implementation
int BB_H263::open(const uint8_t *pData, int iDataSize, H263_DRAW_CALLBACK *pDraw)
{
    _h263.pStream = (uint8_t *)pData;
    _h263.H263File.iSize = iDataSize;
    _h263.pfnDraw = pDraw;
    return H263_SUCCESS;
} /* open() */

int BB_H263::open(const char *szFilename, H263_DRAW_CALLBACK *pfnDraw)
{
    FILE *pFile;
    uint32_t iDataSize;
    
    if (!pfnDraw || !szFilename) return H263_INVALID_PARAMETER;
    
    memset(&_h263, 0, sizeof(H263STATE));
    _h263.pfnDraw = pfnDraw;
    _h263.pfnRead = linuxRead;
    _h263.pfnSeek = linuxSeek;
    _h263.pfnClose = linuxClose;
    pFile = (FILE *)linuxOpen(szFilename, &iDataSize);
    if (!pFile) return H263_FILEIO_ERROR;
    if (iDataSize < 4096) {
        linuxClose(pFile);
        return H263_INVALID_FILE;
    }
    _h263.H263File.fHandle = pFile;
    _h263.H263File.iPos = 0; // current file position
    _h263.H263File.iSize = iDataSize; // file size
    return H263_SUCCESS;
} /* open() */

int BB_H263::open(const char *szFilename, H263_OPEN_CALLBACK *pfnOpen, H263_CLOSE_CALLBACK *pfnClose, H263_READ_CALLBACK *pfnRead, H263_SEEK_CALLBACK *pfnSeek, H263_DRAW_CALLBACK *pfnDraw)
{
    FILE *pFile;
    uint32_t iDataSize;
    
    if (!pfnOpen || !pfnClose || !pfnRead || !pfnSeek) return H263_INVALID_PARAMETER;
    memset(&_h263, 0, sizeof(H263STATE));
    _h263.pfnDraw = pfnDraw;
    _h263.pfnRead = pfnRead;
    _h263.pfnSeek = pfnSeek;
    _h263.pfnClose = pfnClose;
    pFile = (FILE *)(*pfnOpen)(szFilename, &iDataSize);
    if (!pFile) return H263_FILEIO_ERROR;
    if (iDataSize < 4096) {
        (*pfnClose)(pFile);
        return H263_INVALID_FILE;
    }
    _h263.H263File.fHandle = pFile;
    _h263.H263File.iPos = 0; // current file position
    _h263.H263File.iSize = iDataSize; // file size
    return H263_SUCCESS;
} /* open() */

int BB_H263::decodeFrame(int x, int y)
{
    return ReadH263(&_h263);
//    return H263_decodeFrame(&_h263, x, y);
} /* decodeFrame() */

#endif // __cplusplus

void H263Close(H263STATE *pVideo)
{
    (*pVideo->pfnClose)(pVideo->H263File.fHandle);
} /* H263Close() */

const uint8_t cZigZag2[64] = {0,1,8,16,9,2,3,10,
    17,24,32,25,18,11,4,5,
    12,19,26,33,40,48,41,34,
    27,20,13,6,7,14,21,28,
    35,42,49,56,57,50,43,36,
    29,22,15,23,30,37,44,51,
    58,59,52,45,38,31,39,46,
    53,60,61,54,47,55,62,63};

// Frame size in Macroblocks of the different video formats
const int iH263Formats[16] = {0,0, 8,6, 11,9, 22,18, 44,36, 88,72, 0,0, 0,0};

const int8_t cDQUANT[4] = {-1,-2,1,2};

// Table defining VLC for H263 MCBPC - macroblock coding block patttern for INTER frames
// macroblock type (bits 4-7) + CB56 value (bits 0-1), length, bit pattern
const uint8_t ucMCBPCTab[20*3] = {0x00,1,0x01, // 0
                                  0x01,4,0x03, // 1
                                  0x02,4,0x02, // 2
                                  0x03,6,0x05, // 3
                                  0x10,3,0x03, // 4
                                  0x11,7,0x07, // 5
                                  0x12,7,0x06, // 6
                                  0x13,9,0x05, // 7
                                  0x20,3,0x02, // 8
                                  0x21,7,0x05, // 9
                                  0x22,7,0x04, // 10
                                  0x23,8,0x05, // 11
                                  0x30,5,0x03, // 12
                                  0x31,8,0x04, // 13
                                  0x32,8,0x03, // 14
                                  0x33,7,0x03, // 15
                                  0x40,6,0x04, // 16
                                  0x41,9,0x04, // 17
                                  0x42,9,0x03, // 18
                                  0x43,9,0x02}; // 19

// Table defining VLC for H263 MV (motion vector) codes
// value, length, bit pattern
// value is stored as x2 since half-pixel resolution
// can be stored as a short/long table with max length
// of 8-bits because all codes longer than 8 have at least 5 leading 0's
const int8_t ucMVDTab[64*3] = {-32,13,0x05, // 0
                            -31,13,0x07, // 1
                            -30,12,0x05, // 2
                            -29,12,0x07, // 3
                            -28,12,0x09, // 4
                            -27,12,0x0b, // 5
                            -26,12,0x0d, // 6
                            -25,12,0x0f, // 7
                            -24,11,0x09, // 8
                            -23,11,0x0b, // 9
                            -22,11,0x0d, // 10
                            -21,11,0x0f, // 11
                            -20,11,0x11, // 12
                            -19,11,0x13, // 13
                            -18,11,0x15, // 14
                            -17,11,0x17, // 15
                            -16,11,0x19, // 16
                            -15,11,0x1b, // 17
                            -14,11,0x1d, // 18
                            -13,11,0x1f, // 19
                            -12,11,0x21, // 20
                            -11,11,0x23, // 21
                            -10,10,0x13, // 22
                            -9, 10,0x15, // 23
                            -8, 10,0x17, // 24
                            -7, 8, 0x7,  // 25
                            -6, 8, 0x9,  // 26
                            -5, 8, 0xb,  // 27
                            -4, 7, 0x7,  // 28
                            -3, 5, 0x3,  // 29
                            -2, 4, 0x3,  // 30
                            -1, 3, 0x3,  // 31
                            0,  1, 0x1,  // 32
                            1,  3, 0x2,  // 33
                            2,  4, 0x2,  // 34
                            3,  5, 0x2,  // 35
                            4,  7, 0x6,  // 36
                            5,  8, 0xa,  // 37
                            6,  8, 0x8,  // 38
                            7,  8, 0x6,  // 39
                            8, 10, 0x16, // 40
                            9, 10, 0x14, // 41
                            10,10, 0x12, // 42
                            11,11, 0x22, // 43
                            12,11, 0x20, // 44
                            13,11, 0x1e, // 45
                            14,11, 0x1c, // 46
                            15,11, 0x1a, // 47
                            16,11, 0x18, // 48
                            17,11, 0x16, // 49
                            18,11, 0x14, // 50
                            19,11, 0x12, // 51
                            20,11, 0x10, // 52
                            21,11, 0x0e, // 53
                            22,11, 0x0c, // 54
                            23,11, 0x0a, // 55
                            24,11, 0x08, // 56
                            25,12, 0x0e, // 57
                            26,12, 0x0c, // 58
                            27,12, 0x0a, // 59
                            28,12, 0x08, // 60
                            29,12, 0x06, // 61
                            30,12, 0x04, // 62
                            31,13, 0x06}; // 63

// Table defining VLC for H263 CBPY codes
// value, length, bit pattern
const uint8_t ucCBPYTab[] = {0x00,4,0x03, // 0
                             0x01,5,0x05, // 1
                             0x02,5,0x04, // 2
                             0x03,4,0x09, // 3
                             0x04,5,0x03, // 4
                             0x05,4,0x07, // 5
                             0x06,6,0x02, // 6
                             0x07,4,0x0b, // 7
                             0x08,5,0x02, // 8
                             0x09,6,0x03, // 9
                             0x0a,4,0x05, // 10
                             0x0b,4,0x0a, // 11
                             0x0c,4,0x04, // 12
                             0x0d,4,0x08, // 13
                             0x0e,4,0x06, // 14
                             0x0f,2,0x03}; // 15

// Table defining VLC for H263 AC coefficients
// defined in groups of 3 with the code being 8bits (last), 8bits (run), 8bits (level), followed by length, followed by code bits
uint32_t ulTCOEF[] = {0x000001,3,0x4, // 0 pos
                           0x000002,5,0x1e, // 1 pos
                           0x000003,7,0x2a, // 2 pos
                           0x000004,8,0x2e, // 3 pos
                           0x000005,9,0x3e, // 4 pos
                           0x000006,10,0x4a, // 5 pos
                           0x000007,10,0x48, // 6 pos
                           0x000008,11,0x42, // 7 pos
                           0x000009,11,0x40, // 8 pos
                           0x00000a,12,0x0e, // 9 pos
                           0x00000b,12,0x0c, // 10 pos
                           0x00000c,12,0x40, // 11 pos
                           0x000101,4,0x0c, // 12 pos
                           0x000102,7,0x28, // 13 pos
                           0x000103,9,0x3c, // 14 pos
                           0x000104,11,0x1e, // 15 pos
                           0x000105,12,0x42, // 16 pos
                           0x000106,13,0xa0, // 17 pos
                           0x000201,5,0x1c,  // 18 pos
                           0x000202,9,0x3a, // 19 pos
                           0x000203,11,0x1c, // 20 pos
                           0x000204,13,0xa2, // 21 pos
                           0x000301,6,0x1a, // 22 pos
                           0x000302,10,0x46, // 23 pos
                           0x000303,11,0x1a, // 24 pos
                           0x000401,6,0x18, // 25 pos
                           0x000402,10,0x44, // 26 pos
                           0x000403,13,0xa4, // 27 pos
                           0x000501,6,0x16, // 28 pos
                           0x000502,11,0x18, // 29 pos
                           0x000503,13,0xa6, // 30 pos
                           0x000601,7,0x26, // 31 pos
                           0x000602,11,0x16, // 32 pos
                           0x000603,13,0xa8, // 33 pos
                           0x000701,7,0x24, // 34 pos
                           0x000702,11,0x14, // 35 pos
                           0x000801,7,0x22, // 36 pos
                           0x000802,11,0x12, // 37 pos
                           0x000901,7,0x20, // 38 pos
                           0x000902,11,0x10, // 39 pos
                           0x000a01,8,0x2c, // 40 pos
                           0x000a02,13,0xaa, // 41 pos
                           0x000b01,8,0x2a, // 42 pos
                           0x000c01,8,0x28, // 43 pos
                           0x000d01,9,0x38, // 44 pos
                           0x000e01,9,0x36, // 45 pos
                           0x000f01,10,0x42, // 46 pos
                           0x001001,10,0x40, // 47 pos
                           0x001101,10,0x3e, // 48 pos
                           0x001201,10,0x3c, // 49 pos
                           0x001301,10,0x3a, // 50 pos
                           0x001401,10,0x38, // 51 pos
                           0x001501,10,0x36, // 52
                           0x001601,10,0x34, // 53
                           0x001701,12,0x44, // 54
                           0x001801,12,0x46, // 55
                           0x001901,13,0xac, // 56
                           0x001a01,13,0xae, // 57
                           0x010001,5,0x0e, // 58
                           0x010002,10,0x32, // 59
                           0x010003,12, 0x0a, // 60
                           0x010101,7,0x1e, // 61
                           0x010102,12,0x08, // 62
                           0x010201,7,0x1c, // 63
                           0x010301,7,0x1a, // 64
                           0x010401,7,0x18, // 65
                           0x010501,8,0x26, // 66
                           0x010601,8,0x24, // 67
                           0x010701,8,0x22, // 68
                           0x010801,8,0x20, // 69
                           0x010901,9,0x34, // 70
                           0x010a01,9,0x32, // 71
                           0x010b01,9,0x30, // 72
                           0x010c01,9,0x2e, // 73
                           0x010d01,9,0x2c, // 74
                           0x010e01,9,0x2a, // 75
                           0x010f01,9,0x28, // 76
                           0x011001,9,0x26, // 77
                           0x011101,10,0x30, // 78
                           0x011201,10,0x2e, // 79
                           0x011301,10,0x2c, // 80
                           0x011401,10,0x2a, // 81
                           0x011501,10,0x28, // 82
                           0x011601,10,0x26, // 83
                           0x011701,10,0x24, // 84
                           0x011801,10,0x22, // 85
                           0x011901,11,0x0e, // 86
                           0x011a01,11,0x0c, // 87
                           0x011b01,11,0x0a, // 88
                           0x011c01,11,0x08, // 89
                           0x011d01,12,0x48, // 90
                           0x011e01,12,0x4a, // 91
                           0x011f01,12,0x4c, // 92
                           0x012001,12,0x4e, // 93
                           0x012101,13,0xb0, // 94
                           0x012201,13,0xb2, // 95
                           0x012301,13,0xb4, // 96
                           0x012401,13,0xb6, // 97
                           0x012501,13,0xb8, // 98
                           0x012601,13,0xba, // 99
                           0x012701,13,0xbc, // 100
                           0x012801,13,0xbe, // 101
                           0xffffffff,7,0x03}; // 102 = ESCAPE
/* Information to build the tables for decoding the run-level combined VLC */
/* these codes are 2 to 17 bits (including the sign bit) */
/* This table contains the run, level, bit pattern, and length (in that order) */
/* of these codes to be built into several 128K tables for fast lookup */
/* Taken from table 5.5 of the MPEG book, page 96-98 */
/* To save time on decode, the sign bit is encoded into the table */
/* The first codes for non-intra blocks are checked separately */
// The codes can be broken in to short (<=11 bits) and long (first 7 bits = 0 + 10 more bits)
// This makes for an arrangement of long codes first (1024)
// followed by short codes (2048)
// run = 0 to 31, level = -40 to 40, len = 2-17
//
const int iH263RunLenVLC[] = {
                   0,  1, 0x0002,  2, /* first & EOB */
                   0, -1, 0x0003,  2, /* first */
//                    0,  1, 0x0006,  3, /* next */
//                    0, -1, 0x0007,  3,
                    1,  1, 0x0006,  4,
                    1, -1, 0x0007,  4,
                    0,  2, 0x0008,  5,
                    0, -2, 0x0009,  5,
                    2,  1, 0x000a,  5,
                    2, -1, 0x000b,  5,
                    0, 63, 0x0001,  6, /* escape */
                    0,  3, 0x000a,  6,
                    0, -3, 0x000b,  6,
                    4,  1, 0x000c,  6,
                    4, -1, 0x000d,  6,
                    3,  1, 0x000e,  6,
                    3, -1, 0x000f,  6,
                    7,  1, 0x0008,  7,
                    7, -1, 0x0009,  7,
                    6,  1, 0x000a,  7,
                    6, -1, 0x000b,  7,
                    1,  2, 0x000c,  7,
                    1, -2, 0x000d,  7,
                    5,  1, 0x000e,  7,
                    5, -1, 0x000f,  7,
                    2,  2, 0x0008,  8,
                    2, -2, 0x0009,  8,
                    9,  1, 0x000a,  8,
                    9, -1, 0x000b,  8,
                    0,  4, 0x000c,  8,
                    0, -4, 0x000d,  8,
                    8,  1, 0x000e,  8,
                    8, -1, 0x000f,  8,
                   13,  1, 0x0040,  9,
                   13, -1, 0x0041,  9,
                    0,  6, 0x0042,  9,
                    0, -6, 0x0043,  9,
                   12,  1, 0x0044,  9,
                   12, -1, 0x0045,  9,
                   11,  1, 0x0046,  9,
                   11, -1, 0x0047,  9,
                    3,  2, 0x0048,  9,
                    3, -2, 0x0049,  9,
                    1,  3, 0x004a,  9,
                    1, -3, 0x004b,  9,
                    0,  5, 0x004c,  9,
                    0, -5, 0x004d,  9,
                   10,  1, 0x004e,  9,
                   10, -1, 0x004f,  9,
                   16,  1, 0x0010, 11,
                   16, -1, 0x0011, 11,
                    5,  2, 0x0012, 11,
                    5, -2, 0x0013, 11,
                    0,  7, 0x0014, 11,
                    0, -7, 0x0015, 11,
                    2,  3, 0x0016, 11,
                    2, -3, 0x0017, 11,
                    1,  4, 0x0018, 11,
                    1, -4, 0x0019, 11,
                   15,  1, 0x001a, 11,
                   15, -1, 0x001b, 11,
                   14,  1, 0x001c, 11,
                   14, -1, 0x001d, 11,
                    4,  2, 0x001e, 11,
                    4, -2, 0x001f, 11,
                    0, 11, 0x0020, 13,
                    0,-11, 0x0021, 13,
                    8,  2, 0x0022, 13,
                    8, -2, 0x0023, 13,
                    4,  3, 0x0024, 13,
                    4, -3, 0x0025, 13,
                    0, 10, 0x0026, 13,
                    0,-10, 0x0027, 13,
                    2,  4, 0x0028, 13,
                    2, -4, 0x0029, 13,
                    7,  2, 0x002a, 13,
                    7, -2, 0x002b, 13,
                   21,  1, 0x002c, 13,
                   21, -1, 0x002d, 13,
                   20,  1, 0x002e, 13,
                   20, -1, 0x002f, 13,
                    0,  9, 0x0030, 13,
                    0, -9, 0x0031, 13,
                   19,  1, 0x0032, 13,
                   19, -1, 0x0033, 13,
                   18,  1, 0x0034, 13,
                   18, -1, 0x0035, 13,
                    1,  5, 0x0036, 13,
                    1, -5, 0x0037, 13,
                    3,  3, 0x0038, 13,
                    3, -3, 0x0039, 13,
                    0,  8, 0x003a, 13,
                    0, -8, 0x003b, 13,
                    6,  2, 0x003c, 13,
                    6, -2, 0x003d, 13,
                   17,  1, 0x003e, 13,
                   17, -1, 0x003f, 13,
                   10,  2, 0x0020, 14,
                   10, -2, 0x0021, 14,
                    9,  2, 0x0022, 14,
                    9, -2, 0x0023, 14,
                    5,  3, 0x0024, 14,
                    5, -3, 0x0025, 14,
                    3,  4, 0x0026, 14,
                    3, -4, 0x0027, 14,
                    2,  5, 0x0028, 14,
                    2, -5, 0x0029, 14,
                    1,  7, 0x002a, 14,
                    1, -7, 0x002b, 14,
                    1,  6, 0x002c, 14,
                    1, -6, 0x002d, 14,
                    0, 15, 0x002e, 14,
                    0,-15, 0x002f, 14,
                    0, 14, 0x0030, 14,
                    0,-14, 0x0031, 14,
                    0, 13, 0x0032, 14,
                    0,-13, 0x0033, 14,
                    0, 12, 0x0034, 14,
                    0,-12, 0x0035, 14,
                   26,  1, 0x0036, 14,
                   26, -1, 0x0037, 14,
                   25,  1, 0x0038, 14,
                   25, -1, 0x0039, 14,
                   24,  1, 0x003a, 14,
                   24, -1, 0x003b, 14,
                   23,  1, 0x003c, 14,
                   23, -1, 0x003d, 14,
                   22,  1, 0x003e, 14,
                   22, -1, 0x003f, 14,
                    0, 31, 0x0020, 15,
                    0,-31, 0x0021, 15,
                    0, 30, 0x0022, 15,
                    0,-30, 0x0023, 15,
                    0, 29, 0x0024, 15,
                    0,-29, 0x0025, 15,
                    0, 28, 0x0026, 15,
                    0,-28, 0x0027, 15,
                    0, 27, 0x0028, 15,
                    0,-27, 0x0029, 15,
                    0, 26, 0x002a, 15,
                    0,-26, 0x002b, 15,
                    0, 25, 0x002c, 15,
                    0,-25, 0x002d, 15,
                    0, 24, 0x002e, 15,
                    0,-24, 0x002f, 15,
                    0, 23, 0x0030, 15,
                    0,-23, 0x0031, 15,
                    0, 22, 0x0032, 15,
                    0,-22, 0x0033, 15,
                    0, 21, 0x0034, 15,
                    0,-21, 0x0035, 15,
                    0, 20, 0x0036, 15,
                    0,-20, 0x0037, 15,
                    0, 19, 0x0038, 15,
                    0,-19, 0x0039, 15,
                    0, 18, 0x003a, 15,
                    0,-18, 0x003b, 15,
                    0, 17, 0x003c, 15,
                    0,-17, 0x003d, 15,
                    0, 16, 0x003e, 15,
                    0,-16, 0x003f, 15,
                    0, 40, 0x0020, 16,
                    0,-40, 0x0021, 16,
                    0, 39, 0x0022, 16,
                    0,-39, 0x0023, 16,
                    0, 38, 0x0024, 16,
                    0,-38, 0x0025, 16,
                    0, 37, 0x0026, 16,
                    0,-37, 0x0027, 16,
                    0, 36, 0x0028, 16,
                    0,-36, 0x0029, 16,
                    0, 35, 0x002a, 16,
                    0,-35, 0x002b, 16,
                    0, 34, 0x002c, 16,
                    0,-34, 0x002d, 16,
                    0, 33, 0x002e, 16,
                    0,-33, 0x002f, 16,
                    0, 32, 0x0030, 16,
                    0,-32, 0x0031, 16,
                    1, 14, 0x0032, 16,
                    1,-14, 0x0033, 16,
                    1, 13, 0x0034, 16,
                    1,-13, 0x0035, 16,
                    1, 12, 0x0036, 16,
                    1,-12, 0x0037, 16,
                    1, 11, 0x0038, 16,
                    1,-11, 0x0039, 16,
                    1, 10, 0x003a, 16,
                    1,-10, 0x003b, 16,
                    1,  9, 0x003c, 16,
                    1, -9, 0x003d, 16,
                    1,  8, 0x003e, 16,
                    1, -8, 0x003f, 16,
                    1, 18, 0x0020, 17,
                    1,-18, 0x0021, 17,
                    1, 17, 0x0022, 17,
                    1,-17, 0x0023, 17,
                    1, 16, 0x0024, 17,
                    1,-16, 0x0025, 17,
                    1, 15, 0x0026, 17,
                    1,-15, 0x0027, 17,
                    6,  3, 0x0028, 17,
                    6, -3, 0x0029, 17,
                   16,  2, 0x002a, 17,
                   16, -2, 0x002b, 17,
                   15,  2, 0x002c, 17,
                   15, -2, 0x002d, 17,
                   14,  2, 0x002e, 17,
                   14, -2, 0x002f, 17,
                   13,  2, 0x0030, 17,
                   13, -2, 0x0031, 17,
                   12,  2, 0x0032, 17,
                   12, -2, 0x0033, 17,
                   11,  2, 0x0034, 17,
                   11, -2, 0x0035, 17,
                   31,  1, 0x0036, 17,
                   31, -1, 0x0037, 17,
                   30,  1, 0x0038, 17,
                   30, -1, 0x0039, 17,
                   29,  1, 0x003a, 17,
                   29, -1, 0x003b, 17,
                   28,  1, 0x003c, 17,
                   28, -1, 0x003d, 17,
                   27,  1, 0x003e, 17,
                   27, -1, 0x003f, 17,
                    0,  0, 0x0000,  0};
#define W1 2841 /* 2048*sqrt(2)*cos(1*pi/16) */
#define W2 2676 /* 2048*sqrt(2)*cos(2*pi/16) */
#define W3 2408 /* 2048*sqrt(2)*cos(3*pi/16) */
#define W5 1609 /* 2048*sqrt(2)*cos(5*pi/16) */
#define W6 1108 /* 2048*sqrt(2)*cos(6*pi/16) */
#define W7 565  /* 2048*sqrt(2)*cos(7*pi/16) */
static void idctrow(short *blk)
{
  int x0, x1, x2, x3, x4, x5, x6, x7, x8;

  /* shortcut */
  if (!((x1 = blk[4]<<11) | (x2 = blk[6]) | (x3 = blk[2]) |
        (x4 = blk[1]) | (x5 = blk[7]) | (x6 = blk[5]) | (x7 = blk[3])))
  {
    blk[0]=blk[1]=blk[2]=blk[3]=blk[4]=blk[5]=blk[6]=blk[7]=blk[0]<<3;
    return;
  }

  x0 = (blk[0]<<11) + 128; /* for proper rounding in the fourth stage */

  /* first stage */
  x8 = W7*(x4+x5);
  x4 = x8 + (W1-W7)*x4;
  x5 = x8 - (W1+W7)*x5;
  x8 = W3*(x6+x7);
  x6 = x8 - (W3-W5)*x6;
  x7 = x8 - (W3+W5)*x7;

  /* second stage */
  x8 = x0 + x1;
  x0 -= x1;
  x1 = W6*(x3+x2);
  x2 = x1 - (W2+W6)*x2;
  x3 = x1 + (W2-W6)*x3;
  x1 = x4 + x6;
  x4 -= x6;
  x6 = x5 + x7;
  x5 -= x7;

  /* third stage */
  x7 = x8 + x3;
  x8 -= x3;
  x3 = x0 + x2;
  x0 -= x2;
  x2 = (181*(x4+x5)+128)>>8;
  x4 = (181*(x4-x5)+128)>>8;

  /* fourth stage */
  blk[0] = (short)((x7+x1)>>8);
  blk[1] = (short)((x3+x2)>>8);
  blk[2] = (short)((x0+x4)>>8);
  blk[3] = (short)((x8+x6)>>8);
  blk[4] = (short)((x8-x6)>>8);
  blk[5] = (short)((x0-x4)>>8);
  blk[6] = (short)((x3-x2)>>8);
  blk[7] = (short)((x7-x1)>>8);
}

/* column (vertical) IDCT
 *
 *             7                         pi         1
 * dst[8*k] = sum c[l] * src[8*l] * cos( -- * ( k + - ) * l )
 *            l=0                        8          2
 *
 * where: c[0]    = 1/1024
 *        c[1..7] = (1/1024)*sqrt(2)
 */
static void idctcol(short *blk)
{
  int x0, x1, x2, x3, x4, x5, x6, x7, x8;
  int t;

  /* shortcut */
  if (!((x1 = (blk[8*4]<<8)) | (x2 = blk[8*6]) | (x3 = blk[8*2]) |
        (x4 = blk[8*1]) | (x5 = blk[8*7]) | (x6 = blk[8*5]) | (x7 = blk[8*3])))
  {
    t = (blk[8*0]+32)>>6;
    if (t < -256) t = -256;
    if (t > 255) t = 255;
    blk[8*0]=blk[8*1]=blk[8*2]=blk[8*3]=blk[8*4]=blk[8*5]=blk[8*6]=blk[8*7]=(short)t;
    return;
  }

  x0 = (blk[8*0]<<8) + 8192;

  /* first stage */
  x8 = W7*(x4+x5) + 4;
  x4 = (x8+(W1-W7)*x4)>>3;
  x5 = (x8-(W1+W7)*x5)>>3;
  x8 = W3*(x6+x7) + 4;
  x6 = (x8-(W3-W5)*x6)>>3;
  x7 = (x8-(W3+W5)*x7)>>3;

  /* second stage */
  x8 = x0 + x1;
  x0 -= x1;
  x1 = W6*(x3+x2) + 4;
  x2 = (x1-(W2+W6)*x2)>>3;
  x3 = (x1+(W2-W6)*x3)>>3;
  x1 = x4 + x6;
  x4 -= x6;
  x6 = x5 + x7;
  x5 -= x7;

  /* third stage */
  x7 = x8 + x3;
  x8 -= x3;
  x3 = x0 + x2;
  x0 -= x2;
  x2 = (181*(x4+x5)+128)>>8;
  x4 = (181*(x4-x5)+128)>>8;

  /* fourth stage */
  t = (x7+x1)>>14;
  if (t < -256) t = -256;
  if (t > 255) t = 255;
  blk[8*0] = (short)t;
  t = (x3+x2)>>14;
  if (t < -256) t = -256;
  if (t > 255) t = 255;
  blk[8*1] = (short)t;
  t = (x0+x4)>>14;
  if (t < -256) t = -256;
  if (t > 255) t = 255;
  blk[8*2] = (short)t;
  t = (x8+x6)>>14;
  if (t < -256) t = -256;
  if (t > 255) t = 255;
  blk[8*3] = (short)t;
  t = (x8-x6)>>14;
  if (t < -256) t = -256;
  if (t > 255) t = 255;
  blk[8*4] = (short)t;
  t = (x0-x4)>>14;
  if (t < -256) t = -256;
  if (t > 255) t = 255;
  blk[8*5] = (short)t;
  t = (x3-x2)>>14;
  if (t < -256) t = -256;
  if (t > 255) t = 255;
  blk[8*6] = (short)t;
  t = (x7-x1)>>14;
  if (t < -256) t = -256;
  if (t > 255) t = 255;
  blk[8*7] = (short)t;
}

/* two dimensional inverse discrete cosine transform */
void H263IDCT(short *block, uint32_t ulMap)
{
int i;

  for (i=0; i<8; i++)
    {
    if (ulMap & (1<<i))
       idctrow(block+8*i);
    }

  for (i=0; i<8; i++)
    idctcol(block+i); //, ulMap);
} /* H263IDCT() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : H263PutMCU22(int, int, int, int*, char *)                  *
 *                                                                          *
 *  PURPOSE    : Combine and output a subsampled color macro block.         *
 *                                                                          *
 ****************************************************************************/
void H263PutMCU22(H263STATE *pVideo, int x, int y, short *pMCU, int iYBias, int iCrCbBias)
{
//signed long Cr,Cb;
int32_t Y1, Y2, Y3, Y4;
int iRow, iCol;
int16_t s, *pY, *pCr, *pCb;
uint32_t *ulDest, ulPixel; // define as long to get around compiler innefficiency
int32_t iCBG, iCRG, iCBB, iCRR;
const int iRowOffsets[8] = {0,16,32,48,128,144,160,176};
int iMaxCol, iMaxRow;
uint16_t usIndex;
uint8_t *cOutput;
int iPitch = pVideo->iFramePitch;
    
//    if (pVideo->iOptions & PIL_CONVERT_16BPP)
//       lsize >>= 2; // for longs

   pCb = (int16_t *)&pMCU[MCU4];
   pCr = (int16_t *)&pMCU[MCU5];

   /* Convert YCC pixels into RGB pixels and store in output image */
   ulDest = (uint32_t *)&pVideo->pFramebuffer[y*16*iPitch + x*8]; // destination 16x16 block of output image
   cOutput = &pVideo->pFramebuffer[y*16*iPitch + x*16*3];
   // Set the block clipping size so we don't draw beyond the image borders
   iMaxRow = iMaxCol = 7;
   if ((y+1)*16 > pVideo->iHeight)
      iMaxRow = (pVideo->iHeight/2) & 7;
   if ((x+1)*16 > pVideo->iWidth)
      iMaxCol = (pVideo->iWidth/2) & 7;
   for (iRow=0; iRow <= iMaxRow; iRow++)
      {
      pY = (signed short *)&pMCU[MCU0 + iRowOffsets[iRow]];
      for (iCol=0; iCol<8; iCol++)
         {
         if (iCol <= iMaxCol)
            {
//            Y1 = 76309 * (pY[0] + iYBias);
//            Y2 = 76309 * (pY[1] + iYBias);
//            Y3 = 76309 * (pY[8] + iYBias);
//            Y4 = 76309 * (pY[9] + iYBias);
//            Cb = pCb[0] + iCrCbBias;
//            Cr = pCr[0] + iCrCbBias;
//            iCBB = 132201 * Cb;
//            iCBG = 25675 * Cb;
//            iCRG = 53279 * Cr;
//            iCRR = 104597  * Cr;
            if (1) //pVideo->iOptions & PIL_CONVERT_16BPP)
               {  // Render 4 pixels from 4 Ys and 1 Cb,Cr
               s = pY[0];
               if (s > 255) s = 255;
               if (s < 0) s = 0;
               usIndex = (((s)>>2) & 0x3f); // Y1
               s = pCb[0];
               if (s > 255) s = 255;
               if (s < 0) s = 0;
               usIndex |= ((((s)>>3)&0x1f)<<6);
               s = pCr[0];
               if (s > 255) s = 255;
               if (s < 0) s = 0;
               usIndex |= ((((s)>>3)&0x1f)<<11);
               ulPixel = pVideo->usYUVRGB[usIndex];
               usIndex &= ~0x3f; // blast away Y1
               s = pY[1];
               if (s > 255) s = 255;
               if (s < 0) s = 0;
               usIndex |= (((s)>>2) & 0x3f); // Y2
               ulPixel |= (pVideo->usYUVRGB[usIndex] << 16);
//               ulPixel = pVideo->usRangeTableB[((iCBB + Y2) >> 16) & 0x3ff]; // blue pixel
//               ulPixel |= pVideo->usRangeTableG[((Y2 - iCBG - iCRG) >> 16) & 0x3ff]; // green pixel
//               ulPixel |= pVideo->usRangeTableR[((iCRR + Y2) >> 16) & 0x3ff]; // red pixel
//               ulPixel <<= 16;
//               ulPixel |= pVideo->usRangeTableB[((iCBB + Y1) >> 16) & 0x3ff]; // blue pixel
//               ulPixel |= pVideo->usRangeTableG[((Y1 - iCBG - iCRG) >> 16) & 0x3ff]; // green pixel
//               ulPixel |= pVideo->usRangeTableR[((iCRR + Y1) >> 16) & 0x3ff]; // red pixel
               ulDest[0] = ulPixel;
               usIndex &= ~0x3f; // blast away Y2
               s = pY[8];
               if (s > 255) s = 255;
               if (s < 0) s = 0;
               usIndex |= (((s)>>2) & 0x3f); // Y3
               ulPixel = pVideo->usYUVRGB[usIndex];
               usIndex &= ~0x3f; // blast away Y3
               s = pY[9];
               if (s > 255) s = 255;
               if (s < 0) s = 0;
               usIndex |= (((s)>>2) & 0x3f); // Y4
               ulPixel |= (pVideo->usYUVRGB[usIndex] << 16);
//               ulPixel = pVideo->usRangeTableB[((iCBB + Y4) >> 16) & 0x3ff]; // blue pixel
//               ulPixel |= pVideo->usRangeTableG[((Y4 - iCBG - iCRG) >> 16) & 0x3ff]; // green pixel
//               ulPixel |= pVideo->usRangeTableR[((iCRR + Y4) >> 16) & 0x3ff]; // red pixel
//               ulPixel <<= 16;
//               ulPixel |= pVideo->usRangeTableB[((iCBB + Y3) >> 16) & 0x3ff]; // blue pixel
//               ulPixel |= pVideo->usRangeTableG[((Y3 - iCBG - iCRG) >> 16) & 0x3ff]; // green pixel
//               ulPixel |= pVideo->usRangeTableR[((iCRR + Y3) >> 16) & 0x3ff]; // red pixel
               ulDest[iPitch] = ulPixel;
               }
            else
               {
               int32_t Cr,Cb;
               Y1 = 76309 * (pY[0] + iYBias);
               Y2 = 76309 * (pY[1] + iYBias);
               Y3 = 76309 * (pY[8] + iYBias);
               Y4 = 76309 * (pY[9] + iYBias);
               Cb = pCb[0] + iCrCbBias;
               Cr = pCr[0] + iCrCbBias;
               iCBB = 132201 * Cb;
               iCBG = 25675 * Cb;
               iCRG = 53279 * Cr;
               iCRR = 104597  * Cr;
               cOutput[0] = pVideo->cRangeTable2[((iCBB + Y1) >> 16) & 0x3ff]; // blue pixel
               cOutput[1] = pVideo->cRangeTable2[((Y1 - iCBG - iCRG ) >> 16) & 0x3ff]; // green pixel
               cOutput[2] = pVideo->cRangeTable2[((iCRR + Y1) >> 16) & 0x3ff]; // red pixel
               cOutput[iPitch] = pVideo->cRangeTable2[((iCBB + Y3) >> 16) & 0x3ff]; // blue pixel
               cOutput[iPitch+1] = pVideo->cRangeTable2[((Y3 - iCBG - iCRG) >> 16) & 0x3ff]; // green pixel
               cOutput[iPitch+2] = pVideo->cRangeTable2[((iCRR + Y3) >> 16) & 0x3ff]; // red pixel
               cOutput[3] = pVideo->cRangeTable2[((iCBB + Y2) >> 16) & 0x3ff]; // blue pixel
               cOutput[4] = pVideo->cRangeTable2[((Y2 - iCBG - iCRG) >> 16) & 0x3ff]; // green pixel
               cOutput[5] = pVideo->cRangeTable2[((iCRR + Y2) >> 16) & 0x3ff]; // red pixel
               cOutput[iPitch+3] = pVideo->cRangeTable2[((iCBB + Y4) >> 16) & 0x3ff]; // blue pixel
               cOutput[iPitch+4] = pVideo->cRangeTable2[((Y4 - iCBG - iCRG) >> 16) & 0x3ff]; // green pixel
               cOutput[iPitch+5] = pVideo->cRangeTable2[((iCRR + Y4) >> 16) & 0x3ff]; // red pixel
               }
            } // if not beyond edge
         pCb++;
         pCr++;
         pY+= 2;
         if (iCol == 3) // need to jump to the adjacent Y block
            pY += (64-8);
         ulDest++;
         cOutput += 6;
         } // for each column
      ulDest -= 8;
      ulDest += iPitch*2; // next line of dest pixels
      cOutput -= 48;
      cOutput += iPitch*2;
      } // for each row
} /* H263PutMCU22() */

#define GETMOREBITS if (iBit >= 16) {iBit -= 16; ulBits <<= 16; ulBits |= MOTOSHORT(&buf[iOff]); iOff += 2;}
#define GETMOREBITS8 if (iBit >= 8) {iBit -= 8; ulBits <<= 8; ulBits |= buf[iOff++];}

signed int H263GetMVPredictor(int x, int y, int bUnrestricted, int iMBCount, signed char *cMVArray, signed char cDelta)
{
signed char cTemp, cMV1, cMV2, cMV3; // the 3 candidate predictors

   if (x == 0)
      cMV1 = 0;
   else
      cMV1 = cMVArray[64 + (x-1)]; // current row
   if (y == 0)
      cMV2 = cMV3 = cMV1; // top row just uses previous predictor
   else
      {
      cMV2 = cMVArray[x]; // above
      cMV3 = cMVArray[x+1]; // above, right
      }
   if (x == iMBCount-1) // right edge
      cMV3 = 0;
   // find median
   // sort the 3 values
   if (cMV1 > cMV2)
      { // swap
      cTemp = cMV1;
      cMV1 = cMV2;
      cMV2 = cTemp;
      }
   if (cMV2 > cMV3)
      { // swap
      cTemp = cMV2;
      cMV2 = cMV3;
      cMV3 = cTemp;
      }
   if (cMV1 > cMV2)
      { // swap
      cTemp = cMV1;
      cMV1 = cMV2;
      cMV2 = cTemp;
      }
   // now we know the cMV2 is the median
   cDelta += cMV2;
   if (bUnrestricted) // different rules for unrestricted MV mode
      { // restricted to -31.5,+31.5 (-63,+63)
      if (cMV2 < -31 && cDelta < -63)
         cDelta += 64;
      if (cMV2 > 32 && cDelta > 63)
         cDelta -= 64;
      }
   else
      { // restricted to -16,+15.5 (-32,+31)
      if (cDelta > 31) // wrap around
         cDelta -= 64;
      else if (cDelta < -32)
         cDelta += 64;
      }
   return cDelta; // ready

} /* H263GetMVPredictor() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : H263CopyMB(int, int, short *, MPEGDATA *)                  *
 *                                                                          *
 *  PURPOSE    : Copy a MB to our prediction Luma/Chroma image.             *
 *                                                                          *
 ****************************************************************************/
void H263CopyMB(H263STATE *pVideo, int x, int y, signed short *pMCU)
{
uint32_t *pS, *pD;
signed short *pDest;
int i, cy;

   // first copy the luma component
   pDest = pVideo->pFRef[0];
   pD = (uint32_t *)&pDest[(x*16)+(y*16*pVideo->iFrameCX)];
   pS = (uint32_t *)pMCU;
   // copy top half
   for (cy=0; cy<8; cy++)
      {
      pD[0] = pS[0]; // top left block
      pD[1] = pS[1];
      pD[2] = pS[2];
      pD[3] = pS[3];
      pD[4] = pS[32]; // top right block
      pD[5] = pS[33];
      pD[6] = pS[34];
      pD[7] = pS[35];
      pD += (pVideo->iFrameCX>>1);
      pS += 4;
      }
   pD = (uint32_t *)&pDest[(x*16)+(((y*16)+8)*pVideo->iFrameCX)];
   pS = (uint32_t *)&pMCU[2*DCTSIZE2];
   // copy bottom half
   for (cy=0; cy<8; cy++)
      {
      pD[0] = pS[0]; // top left block
      pD[1] = pS[1];
      pD[2] = pS[2];
      pD[3] = pS[3];
      pD[4] = pS[32]; // top right block
      pD[5] = pS[33];
      pD[6] = pS[34];
      pD[7] = pS[35];
      pD += (pVideo->iFrameCX>>1);
      pS += 4;
      }
   // Now copy the 2 chroma components
   for (i=0; i<2; i++)
      {
      pDest = pVideo->pFRef[1+i];
      pD = (uint32_t *)&pDest[(x*8)+(y*8*(pVideo->iFrameCX>>1))];
      pS = (uint32_t *)&pMCU[(4+i)*DCTSIZE2];
      for (cy=0; cy<8; cy++)
         {
         pD[0] = pS[0];
         pD[1] = pS[1];
         pD[2] = pS[2];
         pD[3] = pS[3];
         pD += (pVideo->iFrameCX>>2);
         pS += 4;
         }
      }
} /* H263CopyMB() */

void PrepVideoStruct(H263STATE *pVideo)
{
int i, j, iValue, iCount, iRun, iLevel, iBits, iLen;

   pVideo->pFileBuf = (uint8_t *) malloc(0x10000); // 64k should be enough
   pVideo->pAudio = (uint8_t *) malloc(0x10000); // 64k should be enough
   pVideo->pVideo = (uint8_t *) malloc(0x10000);
   pVideo->usYUVRGB = (uint16_t *)malloc(0x20000);
   pVideo->iCurrentFrame = 0;
   pVideo->iFRefFrame = -1;
   // prepare the AC decode table
   pVideo->pACTables = (unsigned short *)malloc(3072*sizeof(short));
   i = 0;
   iRun = iH263RunLenVLC[i*4];
   iLevel = iH263RunLenVLC[i*4 + 1];
   iBits = iH263RunLenVLC[i*4 + 2];
   iLen = iH263RunLenVLC[i*4 + 3];
   while (iLen)
      {
      unsigned short usCode;
      usCode = (unsigned short)((iRun << 11) | ((iLevel & 0x7f)<<4) | (iLen-2));
      if (iLen > 11) // long codes
         {
         iLen -= 7; // remove 7 leading zeros
         iCount = 1 << (10-iLen); /* Number of times to repeat this code */
         iValue = iBits << (10 - iLen); /* Starting value use as an index */
         /* Fill all repeated entries */
         for (j = 0; j < iCount; j++)
            pVideo->pACTables[iValue + j] = usCode;
         }
      else // short codes
         {
             iCount = 1 << (11-iLen); /* Number of times to repeat this code */
             iValue = 1024 + (iBits << (11 - iLen)); /* Starting value use as an index */
             /* Fill all repeated entries */
             for (j = 0; j < iCount; j++)
                pVideo->pACTables[iValue + j] = usCode;
             }
          i++;
          iRun = iH263RunLenVLC[i*4];
          iLevel = iH263RunLenVLC[i*4 + 1];
          iBits = iH263RunLenVLC[i*4 + 2];
          iLen = iH263RunLenVLC[i*4 + 3];
          }

       // prepare the color conversion table
       for (i=0; i<65536; i++)
          {
               int32_t cY, cU, cV;
          int Y, iPixel, iCBB, iCBG, iCRG, iCRR;
          cY = (unsigned char)((i & 0x3f)<<2); // lower 6 bits = Y
          cU = (unsigned char)((i & 0x7c0)>>3); // next 5 bits = U
          cV = (unsigned char)((i & 0xf800)>>8); // next 5 bits = V
    //      cY += 128;
          cY -= 16;
          cU -= 128;
          cV -= 128;
          Y = cY * 76309;
          iCBB = cU * 132201;
          iCBG = cU * 25675;
          iCRG = cV * 53279;
          iCRR = cV * 104597;
          j = ((iCBB + Y) >> 16); // blue
          j &= 0x3ff;
          if (j > 255 && j < 512) j = 255;
          if (j >= 512) j = 0;
          iPixel = j >> 3; // lower 5 bits = blue
          j = ((Y - iCBG - iCRG) >> 16); // green
          j &= 0x3ff;
          if (j > 255 && j < 512) j = 255;
              if (j >= 512) j = 0;
              iPixel |= ((j >> 2) <<5); // middle 6 bits = green
              j = ((iCRR + Y) >> 16); // red
              j &= 0x3ff;
              if (j > 255 && j < 512) j = 255;
              if (j >= 512) j = 0;
              iPixel |= ((j >> 3) << 11); // upper 5 bits = red
              pVideo->usYUVRGB[i] = (unsigned short)iPixel;
              }
} /* PrepVideoStruct() */

void H263SwapFrames(H263STATE *pVideo)
{
signed short *pTemp;
int i;

   for (i=0; i<3; i++) {
       pTemp = pVideo->pBRef[i];
       pVideo->pBRef[i] = pVideo->pFRef[i];
       pVideo->pFRef[i] = pTemp;
   }
} /* H263SwapFrames() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : ReadH263(PIL_PAGE *, PIL_PAGE *, int)                   *
 *                                                                          *
 *  PURPOSE    : Decompress H263 into a flat bitmap.                        *
 *                                                                          *
 ****************************************************************************/
int ReadH263(H263STATE *pVideo)
{
int i, x, y, iGOBy, iErr, iOff, iLen, iBit;
int iTrueWidth, iTrueHeight;
uint8_t cMask, cQuant, *buf;
uint32_t ulBits, ulCode;
uint8_t cLevel, ucTR, ucPSBI, ucCBPY, *pCBPY;
int16_t *pMCU = pVideo->MCUs, us;
int iGOB, iGOBCount, iMB, iMBCount, iMBMax;
char cSourceFormat, ucMBType, ucCBPC;
uint32_t j, count, codestart, repeat, *pVLCTable;
uint32_t ulPTYPE;
signed int iMV_X, iMV_Y;
uint16_t *pMVTable, *pMCBPCTable;
int *pClip;
uint8_t *pTables;

    if (pVideo->iCurrentFrame == 0) {
        PrepVideoStruct(pVideo);
        //      pVideo->iOptions = iOptions;
        //      pVideo->iWidth = inpage->iWidth;
        //      pVideo->iHeight = inpage->iHeight;
        pTables = (uint8_t *) malloc(8192 * 2 * sizeof(uint32_t) + 128 + 8192 + 1024);
        pVideo->pACTables = (uint16_t *)pTables;
        pMVTable = (uint16_t *)pTables;
        pMCBPCTable = (uint16_t *)&pTables[1024];
        // Initialize the IDCT clipping table
        pClip = (int *)&pTables[4096]; // put clip table here
        for (i= -512; i<512; i++)
            pClip[i] = (i<-256) ? -256 : ((i>255) ? 255 : i);
        pCBPY = &pTables[8192]; // put fast CBPY lookup table here
        // Create fast lookup table for the CBPY VLCs
        for (i=0; i<16; i++) {
            ulCode = ucCBPYTab[i*3]; // code value
            iLen = ucCBPYTab[i*3+1]; // code length
            ulBits = ucCBPYTab[i*3+2]; // bit pattern
            count = 6 - iLen;
            codestart = ulBits << count;
            repeat = 1 << count;
            for (j=0; j<repeat; j++) {
                pCBPY[(codestart+j)*2] = (uint8_t)ulCode;
                pCBPY[(codestart+j)*2 + 1] = (uint8_t)iLen;
            }
        }
        pVLCTable = (uint32_t *)&pTables[0x2080];
        // Create the fast lookup table for the VLCs
        for (i=0; i<103; i++) {
            // store the positive code
            ulCode = ulTCOEF[i*3]; // code value
            iLen = ulTCOEF[i*3+1]; // code length
            ulBits = ulTCOEF[i*3+2]; // bit pattern
            count = 13 - iLen;
            codestart = ulBits << count;
            repeat = 1 << count;
            for (j=0; j<repeat; j++) {
                pVLCTable[(codestart+j)*2] = ulCode;
                pVLCTable[(codestart+j)*2 + 1] = iLen;
            }
            // store the negative code
            if (ulCode != 0xffffffff) {
                ulBits |= 1;
                // make the level value negative
                cLevel = (signed char)ulCode;
                cLevel = 0-cLevel;
                ulCode &= 0xffff00; // remove the code
                ulCode |= cLevel; // put back the negative level
                codestart = ulBits << count;
                for (j=0; j<repeat; j++) {
                    pVLCTable[(codestart+j)*2] = ulCode;
                    pVLCTable[(codestart+j)*2 + 1] = iLen;
                }
            }
        }
        // create a fast lookup table for the P-frame MCBPC values
        for (i=0; i<20; i++) {
            ulCode = ucMCBPCTab[i*3]; // code value
            iLen = ucMCBPCTab[i*3+1]; // code length
            ulBits = ucMCBPCTab[i*3+2]; // bit pattern
            if (iLen > 3 && ((ulBits >> (iLen-3)) & 0x7) == 0) { // "long" code has 3 leading zeros
                // long codes (up to 6 bits in length - after subtracting 5)
                count = 6 - (iLen-3);
                codestart = 64 + (ulBits << count);
            } else { // short codes (up to 6 bits in length)
                count = 6 - iLen;
                codestart = ulBits << count;
            }
            repeat = 1 << count;
            us = (uint16_t)((ulCode << 8) | iLen);
            for (j=0; j<repeat; j++) {
                pMCBPCTable[(codestart+j)] = us;
            }
        }
        
        // create a fast lookup table for the motion vectors
        for (i=0; i<64; i++) {
            signed char s;
            s = ucMVDTab[i*3]; // code value
            iLen = ucMVDTab[i*3+1]; // code length
            ulBits = ucMVDTab[i*3+2]; // bit pattern
            if (iLen > 5 && ((ulBits >> (iLen-5)) & 0x1f) == 0) // "long" code has 5 leading zeros
            { // long codes (up to 8 bits in length - after subtracting 5)
                count = 8 - (iLen-5);
                codestart = 256 + (ulBits << count);
            } else { // short codes (up to 8 bits in length)
                count = 8 - iLen;
                codestart = ulBits << count;
            }
            repeat = 1 << count;
            us = (uint16_t)((s << 8) | iLen);
            for (j=0; j<repeat; j++) {
                pMVTable[(codestart+j)] = us;
            }
        }
    } // prepare structure for first frame
    
   iErr = 0;
   iGOBCount = (pVideo->iHeight+15) / 16;
   iMBCount = (pVideo->iWidth+15) / 16;

// Decompress the current frame
   buf = &pVideo->pStream[pVideo->iStreamOff]; // point to raw h.263 data
   iOff = iBit = 0;  /* Pointer into data stream */
   ulBits = MOTOLONG(&buf[iOff]); // start out with 32-bits
   iOff += 4;
   ulCode = (ulBits >> 10); // get PSC (picture start code) (22 bits)
   iBit += 22;
    if (ulCode != 0x0020) { // video sequence is bogus
        pVideo->iLastError = -1;// PIL_ERROR_DECOMP;
      goto h263z;
    }
   ucTR = (uint8_t) (ulBits >> (24 - iBit)); // get TR (temporal reference) (8-bits)
   iBit += 8;
   GETMOREBITS
   ulPTYPE = (ulBits >> (19-iBit)) & 0x1fff; // get PTYPE (13-bits)
   if (ulPTYPE & 0x7) // Arithmetic, advanced prediction, PB-frame
      return 0; // we can't handle it yet
   cSourceFormat = (uint8_t)((ulPTYPE >> 5) & 7); // video size
   iTrueWidth = iH263Formats[cSourceFormat*2];
   iTrueHeight = iH263Formats[cSourceFormat*2+1];
    if (cSourceFormat >= 6) { // invalid value
      iTrueWidth = iMBCount;
      iTrueHeight = iGOBCount;
      }
    if (pVideo->iFrameCX == 0) { // need to allocate predictor pages
      x = pVideo->iFrameCX = iTrueWidth<<4;
      y = pVideo->iFrameCY = iTrueHeight<<4;
      pVideo->pBRef[0] = (int16_t *) malloc(x * y * sizeof(int16_t)); // Luma prediction
      pVideo->pBRef[1] = (int16_t *) malloc(((x * y) >> 2)*sizeof(int16_t)); // Chroma1 prediction
      pVideo->pBRef[2] = (int16_t *) malloc(((x * y) >> 2)*sizeof(int16_t)); // Chroma2 prediction
      pVideo->pFRef[0] = (int16_t *) malloc(x * y * sizeof(int16_t)); // Luma prediction
      pVideo->pFRef[1] = (int16_t *) malloc(((x * y) >> 2)*sizeof(int16_t)); // Chroma1 prediction
      pVideo->pFRef[2] = (int16_t *) malloc(((x * y) >> 2)*sizeof(int16_t)); // Chroma2 prediction
      }
   iMBMax = iTrueWidth * iTrueHeight;
   iBit += 13;
   GETMOREBITS
   cQuant = (uint8_t)(ulBits >> (27-iBit)) & 0x1f; // get PQUANT (5-bits)
   iBit += 5;
   ulCode = (ulBits >> (31-iBit)) & 1; // get CPM (1-bit)
   iBit++;
    if (ulCode) { // if CPM bit set, PSBI bits present
      ucPSBI = (uint8_t) ((ulBits >> (30-iBit)) & 3); // get 2 PSBI bits
      iBit += 2;
      }
   ulCode = (ulBits >> (31-iBit)) & 1; // get PEI extra insertion information (1-bit)
   iBit++;
   GETMOREBITS
    while (ulCode) { // groups of 9 bits = 8 data + 1 flag indicating more data (PSPARE)
      ulCode = (ulBits >> (24-iBit)) & 0xff; // get 8 PSPARE data bits
      iBit += 8;
      ulCode = (ulBits >> (31-iBit)) & 1; // get 1 flag bit indicating more data
      GETMOREBITS
      }

   iGOB = 0;
 // Loop through all macroblocks
   x = y = iGOBy = 0; // keep track of MB position
   // reset previous predictors at start of picture
   memset(pVideo->cMVPredX, 0, 128);
   memset(pVideo->cMVPredY, 0, 128);
   for (iMB=0; iMB<iMBMax && !iErr; iMB++) {
      GETMOREBITS
      GETMOREBITS8
      // Get the GOB header if present
      ulCode = (ulBits >> (16 - iBit)) & 0xffff; // see if we have a start code
       if (ulCode == 0) { // start code, read GOB header
         int iTries = 0;
         ulCode = (ulBits >> (10 - iBit)) & 0x3fffff; // get 22-bit EOS code
           if (ulCode == 0x3f) { // EOS - end of sequence
            iMB = iMBMax-1; // break out of this loop
            continue;
            }
         ulCode = (ulBits >> (15 - iBit)) & 0x1ffff; // get 17-bit code
           while (ulCode != 1 && iTries < 16) { // skip over stuffing bits
            iBit++;
            GETMOREBITS8
            ulCode = (ulBits >> (15 - iBit)) & 0x1ffff; // get 17-bit code
            iTries++;
            }
           if (iTries >= 16) { // error
               pVideo->iLastError = -1; //PIL_ERROR_DECOMP;
            goto h263z;
            }
         iGOBy = 0; // the MV predictor model is reset at the start of each GOB with a header
         iBit += 17; // skip start code
         iBit -= 16;
         ulBits <<= 16;
         ulBits |= MOTOSHORT(&buf[iOff]);
         iOff += 2;

         iGOB = (ulBits >> (27-iBit)) & 0x1f; // get the GN (group number) (5-bits)
         x = 0;
         y = iGOB;
         iMB = y*iTrueWidth;
         iBit += 5;
           if (iGOB >= iTrueHeight) { // beyond picture height
               pVideo->iLastError = -1; //PIL_ERROR_DECOMP;
            goto h263z;
            }
         ulCode = (ulBits >> (30-iBit)) & 3; // get the GFID (GOB frame ID) (2-bits)
         iBit += 2;
         cQuant = (uint8_t)((ulBits >> (27-iBit)) & 0x1f); // get the GQUANT (new quantizer value) (5-bits)
         iBit += 5;
         GETMOREBITS
//         iMB = iGOB * iMBCount; // reset position to new GOB
         } // read GOB header
get_mcbpc:
       if (ulPTYPE & 0x10) { // an INTER block has COD (coded macroblock indication)
         ulCode = (ulBits >> (31-iBit)) & 1; // 1 bit COD
         iBit++;
           if (ulCode) { // this MB is NOT coded, skip it
            iMV_X = iMV_Y = 0; // skipped blocks have a MV of 0,0
            memset(pMCU, 0, DCTSIZE2*6*sizeof(short));
            H263MotComp(x, y, iMV_X, iMV_Y, pMCU, pVideo, 0);
            H263CopyMB(pVideo, x, y, pMCU); // copy the MB to our prediction image
            goto h263next;
            }
         }
      ulCode = (ulBits >> (23-iBit)) & 0x1ff; // 9 bit code for MCBPC
       if (ulCode == 1) { // stuff bits 0000 0000 1
         iBit += 9;
         GETMOREBITS
         goto get_mcbpc; // skip the stuff bits and try again
       } else {
           if (ulPTYPE & 0x10) { // INTER picture has different table
            if (ulCode < 0x40) // long code
               ulCode = pMCBPCTable[ulCode + 64];
            else
               ulCode = pMCBPCTable[ulCode>>3];
            ucMBType = (uint8_t)(ulCode >> 12); // top nibble is MB type
            iBit += (ulCode & 0xf); // bottom nibble is length
            ucCBPC = (uint8_t)((ulCode >> 8) & 3);
           } else { // INTRA picture has a different code table
               if (ulCode < 0x20) { // 6 bit codes 0000 xx
               ucMBType = 4;
               ucCBPC = (uint8_t)(ulCode >> 3);
               iBit += 6;
               } else {
                   if (ulCode < 0x40) { // 4 bit code (only 1) 0001
                  ucMBType = 4;
                  ucCBPC = 0;
                  iBit += 4;
                  } else {
                      if (ulCode < 0x100) { // 3 bit codes 0xx
                     ucMBType = 3;
                     ucCBPC = (uint8_t)(ulCode >> 6);
                     iBit += 3;
                      } else { // 1 bit code
                     ucMBType = 3;
                     ucCBPC = 0;
                     iBit += 1;
                     }
                  }
               }
            }
         }
      // MODB and CBPB would be present if a PB frame
      ulCode = (ulBits >> (26-iBit)) & 0x3f; // get up to six bits for CBPY flags
      ucCBPY = pCBPY[ulCode*2]; // get the INTRA CBPY bits
      if (ucMBType < 3) // for INTER MBs, it's inverted
         ucCBPY ^= 0xf;
      iBit += pCBPY[ulCode*2 + 1]; // true bit length
      GETMOREBITS
       if (ucMBType == 4 || ucMBType == 1) { // DQUANT present for mode 1 and 4 blocks
         ulCode = (ulBits >> (30-iBit)) & 3; // get 2 bit DQUANT code
         iBit += 2;
         cQuant += cDQUANT[ulCode]; // get differential value
         if (cQuant < 1)
            cQuant = 1;
         if (cQuant > 31)
            cQuant = 31;
         }
       if (ucMBType < 3) { // INTER blocks of type 0,1,2 have MVD (motion vector data)
         // Get X MV delta
         ulCode = (ulBits >> (19-iBit)) & 0x1fff; // grab 13 bits (max length)
         if (ulCode < 0x100) // first 5 bits = 0 (long code)?
            ulCode += 256; // point to long table
         else
            ulCode >>= 5; // use first 8 bits
         ulCode = pMVTable[ulCode]; // get the code and length
         iBit += ulCode & 0xf; // add bit length
         iMV_X = H263GetMVPredictor(x, iGOBy, ulPTYPE & 8, iMBCount, pVideo->cMVPredX, (signed char)(ulCode >> 8)); // get X delta
         GETMOREBITS
         // Get Y MV delta
         ulCode = (ulBits >> (19-iBit)) & 0x1fff; // grab 13 bits (max length)
         if (ulCode < 0x100) // first 5 bits = 0 (long code)?
            ulCode += 256; // point to long table
         else
            ulCode >>= 5; // use first 8 bits
         ulCode = pMVTable[ulCode]; // get the code and length
         iBit += ulCode & 0xf; // add bit length
         iMV_Y = H263GetMVPredictor(x, iGOBy, ulPTYPE & 8, iMBCount, pVideo->cMVPredY, (signed char)(ulCode >> 8)); // get Y delta
         GETMOREBITS
       } else {
           iMV_X = iMV_Y = 0;
       }
      pVideo->ulBits = ulBits; // pass current bits forward
  // Decode the 6 blocks comprising the macroblock
      memset(pMCU, 0, DCTSIZE2*6*sizeof(int16_t)); // clear this MB to start
      ucCBPY <<= 2;
      ucCBPY |= ucCBPC; // combine bits of Y with CbCr bits
      cMask = 32;
       for (i=0; i<6 && !iErr; i++) { // Get the 6 blocks comprising the macroblock
         iErr = GetH263MCU(pVLCTable, buf, &pMCU[i*DCTSIZE2], &iOff, &iBit, pVideo, cQuant, ucCBPY & cMask, ucMBType);
           if (ucCBPY & cMask) {
               H263IDCT(&pMCU[i*DCTSIZE2], (uint32_t)-1);
           } else if (ucMBType >= 3) { // only have a DC value, distribute it within the block
            us = pMCU[i*DCTSIZE2 + 0] >> 3; // Get the adjusted DC value
            for (j=0; j<64; j++)
               pMCU[i*DCTSIZE2 + j] = us; // store in all cells
            }
         cMask >>= 1;
         } // for each of the 6 blocks
      ulBits = pVideo->ulBits; // get the bits back
// only draw the visible parts of the frame
       if (ucMBType < 3) { // INTER MB
          // predict block with motion compensation
         H263MotComp(x, y, iMV_X, iMV_Y, pMCU, pVideo, 0);
         }
      H263CopyMB(pVideo, x, y, pMCU); // copy the MB to our prediction image
      if (x < iMBCount && y < iGOBCount) {
            H263PutMCU22(pVideo, x, y, pMCU, -16, -128); // lay down MCU in output image
         }
h263next:
      pVideo->cMVPredX[x + 64] = (signed char)iMV_X; // store MV
      pVideo->cMVPredY[x + 64] = (signed char)iMV_Y;
      x++;
      if (x >= iTrueWidth) {
         x = 0;
         y++; // next line
         iGOBy++; // away from top of last GOB header also
         memcpy(&pVideo->cMVPredX[0], &pVideo->cMVPredX[64], 64); // current predictors become the previous
         memcpy(&pVideo->cMVPredY[0], &pVideo->cMVPredY[64], 64); // current predictors become the previous
         }
      } // for each MB

h263z:
   // swap the reference and current page for next time
   H263SwapFrames(pVideo);

   return iErr;
} /* PILReadH263() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GetH263MCU(char *, int *, int *, int *, PILBOOL)              *
 *                                                                          *
 *  PURPOSE    : Decode a MCU block for H263 data streams.                  *
 *                                                                          *
 ****************************************************************************/
int GetH263MCU(uint32_t *pTable, uint8_t *buf, int16_t *pMCU, int *iOffset, int *iBitnum, H263STATE *pVideo, int iQuant, int bTCOEF, uint8_t ucMBType)
{
int iBit = *iBitnum;
int iOff = *iOffset;
int iRun, iIndex, iErr;
signed int iLevel;
uint32_t ulBits, ulCode, ulVal;
int bLast;

   iErr = 0;
   ulBits = pVideo->ulBits;
    if (iBit >= 16) { // make sure we have enough bits to start
        iBit -= 16;
        ulBits <<= 16;
        ulBits |= MOTOSHORT(&buf[iOff]);
        iOff += 2;
    }

    if (ucMBType >= 3) { // INTRADC value only present in INTRA blocks
        ulCode = (ulBits >> (24-iBit)) & 0xff; // get 8-bit INTRADC code
// DEBUG
//      if (ulCode == 0 || ulCode == 0x80) // invalid values
//         return PIL_ERROR_DECOMP;
        if (ulCode == 0xff)
            ulCode = 0x80; // special value for level 1024
        iBit += 8; // skip 8 bits
        pMCU[0] = (int16_t)(ulCode << 3); // store the DC value in this block
    } else {
        pMCU[0] = 0; // INTER blocks use a DC value of 0 since the whole block is added to existing pixels
    }
   bLast = 0;
   iIndex = (ucMBType >= 3); // for INTRA blocks, start at 1, for INTER, start at 0
    if (bTCOEF) { // if coefficients coded for this block
      while (!bLast && iIndex < 64) {
          if (iBit >= 16) {
              iBit -= 16;
              ulBits <<= 16;
              ulBits |= MOTOSHORT(&buf[iOff]);
              iOff += 2;
          }
          ulCode = (ulBits >> (19-iBit)) & 0x1fff; // get 13-bits
          ulVal = pTable[ulCode*2]; // get the bit value
          if (ulVal == 0) { // invalid code
              pVideo->iLastError = H263_DECODE_ERROR;
              return -1;
          }
         iBit += pTable[(ulCode & 0x1ffe)*2 + 1]; // get the true length
          if (ulVal == 0xffffffff) { // special ESCAPE code
              if (iBit >= 16) { // we need 15 bits
                  iBit -= 16;
                  ulBits <<= 16;
                  ulBits |= MOTOSHORT(&buf[iOff]);
                  iOff += 2;
               }
            ulCode = (ulBits >> (17-iBit)) & 0x7fff; // get 15 bits
            bLast = ulCode & 0x4000; // 1 bit for LAST
            iRun = (ulCode >> 8) & 0x3f; // 6 bits for RUN
            iLevel = (int8_t)(ulCode & 0xff);
            iBit += 15; // 15 bits used
        } else {
            iRun = (ulVal >> 8) & 0xff; // run of zeros
            bLast = ulVal >> 16; // flag indicating last entry
            iLevel = (int32_t)(int8_t)(ulVal & 0xff); // get the level of this coefficient
        }
         iIndex += iRun; // skip value
          if (iQuant & 1) { // odd value
            if (iLevel < 0)
               iLevel = (iQuant * (2 * iLevel - 1));
            else
               iLevel = (iQuant * (2 * iLevel + 1));
          } else { // even value
            if (iLevel < 0)
               iLevel = (iQuant * (2 * iLevel - 1) + 1);
            else
               iLevel = (iQuant * (2 * iLevel + 1) - 1);
            }
         pMCU[cZigZag2[iIndex++]] = (int16_t)iLevel; // store the coeff
         }
      }
    if (iIndex > 64) { // error
        pVideo->iLastError = H263_DECODE_ERROR;
        return -1;
    }
   *iBitnum = iBit;
   *iOffset = iOff;
   pVideo->ulBits = ulBits;
   return iErr;
} /* GetH263MCU() */

#define MB_LOWER -2048
#define MB_UPPER 2047
//#define MB_LOWER -256
//#define MB_UPPER 255
/****************************************************************************
 *                                                                          *
 *  FUNCTION   : H263MotCompAVG(int, int, int *, short *, MPEGDATA)         *
 *                                                                          *
 *  PURPOSE    : Apply bidirectional motion compensation to predicted MB.   *
 *                                                                          *
 ****************************************************************************/
void H263MotCompAVG(int x, int y, signed int *pMVs, signed short *pMCUDest, H263STATE *pVideo)
{
signed short sF, sB, *pSF, *pSB, *pD;
signed int i, dxF, dyF, dxB, dyB, cx, cy, iTypeF, iTypeB;
signed int lyF, lyB;

   // determine the type of pixel capture
   iTypeF = 0;
   if (pMVs[0] & 1) // half-pel on X
      iTypeF++;
   if (pMVs[1] & 1) // half-pel on Y
      iTypeF+=2;
   iTypeB = 0;
   if (pMVs[2] & 1) // half-pel on X
      iTypeB++;
   if (pMVs[3] & 1) // half-pel on Y
      iTypeB+=2;
   dxF = (pMVs[0] >> 1); // whole pel offsets
   dyF = (pMVs[1] >> 1);
   dxB = (pMVs[2] >> 1); // whole pel offsets
   dyB = (pMVs[3] >> 1);

   // Apply the Y deltas first
   // do the luma blocks
   for (i=0; i<4; i++)
      {
      pD = &pMCUDest[i*DCTSIZE2];
      pSF = pVideo->pBRef[0];
      pSB = pVideo->pFRef[0];
      pSF += (x*16)+ dxF + ((i&1)<<3); // horiz address
      pSF += ((y*16) + dyF + ((i&2)<<2)) * pVideo->iFrameCX;
      pSB += (x*16)+ dxB + ((i&1)<<3); // horiz address
      pSB += ((y*16) + dyB + ((i&2)<<2)) * pVideo->iFrameCX;
      // Local x,y
      lyF = (y<<4)+((i&2)<<2) + dyF;
      lyB = (y<<4)+((i&2)<<2) + dyB;
      for (cy=0; cy<8; cy++)
         {
         for (cx=0; cx<8; cx++)
            {
            sF = sB = 0;
            // get forward predicted pixel value
            if (lyF + cy >= 0 && lyF + cy < pVideo->iFrameCY)
               {
               switch (iTypeF)
                  {
                  case 0: // full pel x,y
                     sF = pSF[cx];
                     break;
                  case 1: // half pel x, full pel y
                     sF = ((pSF[cx] + pSF[cx + 1])>>1);  // avg left/right pixels
                     break;
                  case 2: // full pel x, half pel y
                     sF = ((pSF[cx] + pSF[cx + (pVideo->iFrameCX)])>>1);  // avg up/down pixels
                     break;
                  case 3: // half pel x,y
                     sF = pSF[cx] + pSF[cx + 1]; // top 2
                     sF += pSF[cx + (pVideo->iFrameCX)] + pSF[cx + 1 + (pVideo->iFrameCX)]; // bottom 2
                     sF >>= 2;  // avg the 4 pixel group
                     break;
                  } // switch on iTypeF
               }
            // get backward predicted pixel value
            if (lyB + cy >= 0 && lyB + cy < pVideo->iFrameCY)
               {
               switch (iTypeB)
                  {
                  case 0: // full pel x,y
                     sB = pSB[cx];
                     break;
                  case 1: // half pel x, full pel y
                     sB = ((pSB[cx] + pSB[cx + 1])>>1);  // avg left/right pixels
                     break;
                  case 2: // full pel x, half pel y
                     sB = ((pSB[cx] + pSB[cx + (pVideo->iFrameCX)])>>1);  // avg up/down pixels
                     break;
                  case 3: // half pel x,y
                     sB = pSB[cx] + pSB[cx + 1]; // top 2
                     sB += pSB[cx + (pVideo->iFrameCX)] + pSB[cx + 1 + (pVideo->iFrameCX)]; // bottom 2
                     sB >>= 2;  // avg the 4 pixel group
                     break;
                  } // switch on iTypeB
               }
            sF = ((sF + sB)>>1); // average the forward and backward pixels
            pD[cx] += sF; // add the average
            if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
            if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
            } // for cx
         pD += 8;
         pSF += pVideo->iFrameCX; // next line
         pSB += pVideo->iFrameCX; // next line
         } // for cy
      } // for i (MB)

   // Apply the Chroma deltas
   dxF >>= 1; // divide offsets for chroma planes
   dyF >>= 1;
   dxB >>= 1;
   dyB >>= 1;
   // Local x,y
   lyF = (y<<3) + dyF;
   lyB = (y<<3) + dyB;
   for (i=0; i<2; i++)
      {
      pD = &pMCUDest[(i+4)*DCTSIZE2];
      pSF = pVideo->pBRef[i+1];
      pSB = pVideo->pFRef[i+1];
      pSF += x*8 + dxF; // horiz address
      pSF += ((y*8) + dyF) * (pVideo->iFrameCX>>1);
      pSB += x*8 + dxB; // horiz address
      pSB += ((y*8) + dyB) * (pVideo->iFrameCX>>1);
      for (cy=0; cy<8; cy++)
         {
         for (cx=0; cx<8; cx++)
            {
            sF = sB = 0;
            if (lyF + cy >= 0 && lyF + cy < (pVideo->iFrameCY>>1))
               sF = pSF[cx];
            if (lyB + cy >= 0 && lyB + cy < (pVideo->iFrameCY>>1))
               sB = pSB[cx];
            sF = (sF + sB)>>1; // average the forward and backward pixels
            pD[cx] += sF;
            if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
            if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
            } // for cx
         pD += 8;
         pSF += (pVideo->iFrameCX>>1); // next line
         pSB += (pVideo->iFrameCX>>1); // next line
         } // for cy
      }

} /* H263MotCompAVG() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : H263MotComp(int, int, int, int, short *, MPEGDATA, PILBOOL)   *
 *                                                                          *
 *  PURPOSE    : Apply motion compensation to predicted MacroBlock.         *
 *                                                                          *
 ****************************************************************************/
void H263MotComp(int x, int y, signed int iMV_X, signed int iMV_Y, signed short *pMCUDest, H263STATE *pVideo, int bBackward)
{
signed short s, *pS, *pD;
signed int i, dx, dy, cx, cy, iType;
signed int lx, ly, nx, ny, iWidth2;
int bCheckBorders;

   // determine the type of pixel capture
   iType = 0;
   if (iMV_X & 1) // half-pel on X
      iType++;
   if (iMV_Y & 1) // half-pel on Y
      iType+=2;
   dx = (iMV_X >> 1); // whole pel offsets
   dy = (iMV_Y >> 1);

   // See if it overlaps any edge of the picture
   bCheckBorders = 1;
   if ((x<<4)+dx >= 0 && (y<<4)+dy >= 0 &&
      (x<<4)+dx+15 < pVideo->iFrameCX && (y<<4)+dy+15 < pVideo->iFrameCY)
      { // we can do it faster if we don't have to test each pixel
      bCheckBorders = 0;
      }
   // do the luma blocks
   for (i=0; i<4; i++)
      {
      pD = &pMCUDest[i*DCTSIZE2];
      if (bBackward)
         pS = pVideo->pFRef[0];
      else
         pS = pVideo->pBRef[0];
      if (bCheckBorders)
         {
         // Local x,y
         lx = (x<<4)+((i&1)<<3) + dx;
         ly = (y<<4)+((i&2)<<2) + dy;
         switch (iType)
            {
            case 0: // full pel in both dirs
               for (cy=0; cy<8; cy++)
                  {
                  ny = ly + cy;
                  if (ny < 0) ny = 0;
                  if (ny >= pVideo->iFrameCY) ny = pVideo->iFrameCY-1;
                  for (cx=0; cx<8; cx++)
                     {
                     nx = lx + cx;
                     if (nx < 0) nx = 0;
                     if (nx >= pVideo->iFrameCX) nx = pVideo->iFrameCX-1;
                     pD[cx] += pS[ny*pVideo->iFrameCX + nx];
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                     }
                  pD += 8;
                  }
               break;
            case 1: // full pel Y, half-pel X
               for (cy=0; cy<8; cy++)
                  {
                  ny = ly + cy;
                  if (ny < 0) ny = 0;
                  if (ny >= pVideo->iFrameCY) ny = pVideo->iFrameCY-1;
                  for (cx=0; cx<8; cx++)
                     {
                     nx = lx + cx;
                     if (nx < 0) nx = 0;
                     if (nx >= pVideo->iFrameCX) nx = pVideo->iFrameCX-1;
                     pD[cx] += ((pS[ny*pVideo->iFrameCX + nx] + pS[ny*pVideo->iFrameCX + nx + 1] + 1)>>1);  // avg left/right pixels
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                     }
                  pD += 8;
                  }
               break;
            case 2: // half pel Y, full pel X
               for (cy=0; cy<8; cy++)
                  {
                  ny = ly + cy;
                  if (ny < 0) ny = 0;
                  if (ny >= pVideo->iFrameCY) ny = pVideo->iFrameCY-1;
                  for (cx=0; cx<8; cx++)
                     {
                     nx = lx + cx;
                     if (nx < 0) nx = 0;
                     if (nx >= pVideo->iFrameCX) nx = pVideo->iFrameCX-1;
                     pD[cx] += ((pS[ny*pVideo->iFrameCX + nx] + pS[(ny+1)*pVideo->iFrameCX + nx] + 1)>>1);  // avg left/right pixels
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                     }
                  pD += 8;
                  }
               break;
            case 3: // half pel Y, half pel X
               for (cy=0; cy<8; cy++)
                  {
                  ny = ly + cy;
                  if (ny < 0) ny = 0;
                  if (ny >= pVideo->iFrameCY) ny = pVideo->iFrameCY-1;
                  for (cx=0; cx<8; cx++)
                     {
                     nx = lx + cx;
                     if (nx < 0) nx = 0;
                     if (nx >= pVideo->iFrameCX) nx = pVideo->iFrameCX-1;
                     s = pS[ny*pVideo->iFrameCX + nx] + pS[ny*pVideo->iFrameCX + nx + 1]; // top 2
                     s += pS[(ny+1)*pVideo->iFrameCX + nx] + pS[(ny+1)*pVideo->iFrameCX + nx + 1]; // bottom 2
                     pD[cx] += ((s + 2)>>2);  // avg the 4 pixel group
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                     }
                  pD += 8;
                  }
               break;
            } // switch on MV type
         } // check borders
      else
         { // don't check borders
         pS += (x*16)+ dx + ((i&1)<<3); // horiz address
         pS += ((y*16) + dy + ((i&2)<<2)) * pVideo->iFrameCX;
         switch (iType)
            {
            case 0: // full pel in both dirs
               for (cy=0; cy<8; cy++)
                  {
                  for (cx=0; cx<8; cx++)
                     {
                     pD[cx] += pS[cx];
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                     }
                  pD += 8;
                  pS += pVideo->iFrameCX; // next line
                  }
               break;
            case 1: // full pel Y, half-pel X
               for (cy=0; cy<8; cy++)
                  {
                  for (cx=0; cx<8; cx++)
                     {
                     pD[cx] += ((pS[cx] + pS[cx + 1] + 1)>>1);  // avg left/right pixels
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                     }
                  pD += 8;
                  pS += pVideo->iFrameCX; // next line
                  }
               break;
            case 2: // half pel Y, full pel X
               for (cy=0; cy<8; cy++)
                  {
                  for (cx=0; cx<8; cx++)
                     {
                     pD[cx] += ((pS[cx] + pS[cx + pVideo->iFrameCX] + 1)>>1);  // avg left/right pixels
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                     }
                  pD += 8;
                  pS += pVideo->iFrameCX; // next line
                  }
               break;
            case 3: // half pel Y, half pel X
               for (cy=0; cy<8; cy++)
                  {
                  for (cx=0; cx<8; cx++)
                     {
                     s = pS[cx] + pS[cx + 1]; // top 2
                     s += pS[cx + pVideo->iFrameCX] + pS[cx + 1 + pVideo->iFrameCX]; // bottom 2
                     pD[cx] += ((s + 2)>>2);  // avg the 4 pixel group
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                     }
                  pD += 8;
                  pS += pVideo->iFrameCX; // next line
                  }
               break;
            } // switch on MV type
         }
      }

   // determine the type of pixel capture
   iType = 0;
   if (iMV_X & 3) // half-pel on X
      iType++;
   if (iMV_Y & 3) // half-pel on Y
      iType+=2;
   dx = (iMV_X >> 2); // whole pel offsets
   dy = (iMV_Y >> 2);

   // See if it overlaps any edge of the picture
   iWidth2 = (pVideo->iFrameCX>>1);
   bCheckBorders = 1;
   if ((x<<3)+dx >= 0 && (y<<3)+dy >= 0 &&
      (x<<3)+dx+7 < (pVideo->iFrameCX>>1) && (y<<3)+dy+7 < (pVideo->iFrameCY>>1))
      { // we can do it faster if we don't have to test each pixel
      bCheckBorders = 0;
      }

   // Local x,y
   lx = (x<<3) + dx;
   ly = (y<<3) + dy;
   for (i=0; i<2; i++)
      {
      pD = &pMCUDest[(i+4)*DCTSIZE2];
      if (bBackward)
         pS = pVideo->pFRef[i+1];
      else
         pS = pVideo->pBRef[i+1];
      if (bCheckBorders)
         {
         switch (iType)
            {
            case 0: // full pel x,y
               for (cy=0; cy<8; cy++)
                  {
                  ny = ly + cy;
                  if (ny < 0) ny = 0;
                  if (ny >= (pVideo->iFrameCY>>1)) ny = (pVideo->iFrameCY>>1)-1;
                  for (cx=0; cx<8; cx++)
                     {
                     nx = lx + cx;
                     if (nx < 0) nx = 0;
                     if (nx >= iWidth2) nx = iWidth2-1;
                     pD[cx] += pS[ny*iWidth2 + nx];
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                     }
                  pD += 8;
                  }
               break;
            case 1: // half pel x, full pel y
               for (cy=0; cy<8; cy++)
                  {
                  ny = ly + cy;
                  if (ny < 0) ny = 0;
                  if (ny >= (pVideo->iFrameCY>>1)) ny = (pVideo->iFrameCY>>1)-1;
                  for (cx=0; cx<8; cx++)
                     {
                     nx = lx + cx;
                     if (nx < 0) nx = 0;
                     if (nx >= iWidth2) nx = iWidth2-1;
                     pD[cx] += ((pS[ny*iWidth2 + nx] + pS[ny*iWidth2 + nx + 1])>>1);
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                     }
                  pD += 8;
                  }
               break;
            case 2: // full pel x, half pel y
               for (cy=0; cy<8; cy++)
                  {
                  ny = ly + cy;
                  if (ny < 0) ny = 0;
                  if (ny >= (pVideo->iFrameCY>>1)) ny = (pVideo->iFrameCY>>1)-1;
                  for (cx=0; cx<8; cx++)
                     {
                     nx = lx + cx;
                     if (nx < 0) nx = 0;
                     if (nx >= iWidth2) nx = iWidth2-1;
                     pD[cx] += ((pS[ny*iWidth2 + nx] + pS[(ny+1)*iWidth2 + nx])>>1);
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                     }
                  pD += 8;
                  }
               break;
            case 3: // half pel x,y
               for (cy=0; cy<8; cy++)
                  {
                  ny = ly + cy;
                  if (ny < 0) ny = 0;
                  if (ny >= (pVideo->iFrameCY>>1)) ny = (pVideo->iFrameCY>>1)-1;
                  for (cx=0; cx<8; cx++)
                     {
                     nx = lx + cx;
                     if (nx < 0) nx = 0;
                     if (nx >= iWidth2) nx = iWidth2-1;
                     s = pS[ny*iWidth2 + nx] + pS[ny*iWidth2 + nx + 1]; // top 2
                     s += pS[(ny+1)*iWidth2 + nx] + pS[(ny+1)*iWidth2 + nx + 1]; // bottom 2
                     pD[cx] += ((s + 2)>>2);  // avg the 4 pixel group
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                     }
                  pD += 8;
                  }
               break;
            } // switch on type
         }
      else
         { // don't check borders
         pS += x*8 + dx; // horiz address
         pS += ((y*8) + dy) * iWidth2;
         switch (iType)
            {
            case 0: // full pel x,y
               for (cy=0; cy<8; cy++)
                  {
                  for (cx=0; cx<8; cx++)
                     {
                     pD[cx] += pS[cx];
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                     }
                  pD += 8;
                  pS += iWidth2; // next line
                  }
               break;
            case 1: // half pel x, full pel y
               for (cy=0; cy<8; cy++)
                  {
                  for (cx=0; cx<8; cx++)
                     {
                     pD[cx] += ((pS[cx] + pS[cx + 1])>>1);
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                     }
                  pD += 8;
                  pS += iWidth2; // next line
                  }
               break;
            case 2: // full pel x, half pel y
               for (cy=0; cy<8; cy++)
                  {
                  for (cx=0; cx<8; cx++)
                     {
                     pD[cx] += ((pS[cx] + pS[cx + iWidth2])>>1);
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                     }
                  pD += 8;
                  pS += iWidth2; // next line
                  }
               break;
            case 3: // half pel x,y
               for (cy=0; cy<8; cy++)
                  {
                  for (cx=0; cx<8; cx++)
                     {
                     s = pS[cx] + pS[cx + 1]; // top 2
                     s += pS[cx + iWidth2] + pS[cx + 1 + iWidth2]; // bottom 2
                     pD[cx] += ((s + 2)>>2);  // avg the 4 pixel group
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                     }
                  pD += 8;
                  pS += iWidth2; // next line
                  }
               break;
            } // switch on type
         }
      }

} /* H263MotComp() */

#endif // __BB_H263__
