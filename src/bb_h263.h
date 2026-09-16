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
// bb_h263 is an H.263 video decoder/player contained in a single .H file
// It is written in portable C code with a C++ class wrapper to simplify it's use
// The project includes example code for use on Arduino and Linux
// The decoder currently supports the original H.263 standard; not H.263+ (yet)
// Since the original H.263 supported a limited selection of video resolutions
// This library allows you to set a clip rectangle so that an unsupported resolution
// can be encoded in a higher resolution (e.g. 320x240 encoded into a 352x288 video)
// Macroblocks outside of the clip region are not calculated, so the are few
// wasted cycles on non-visible areas
//
#ifndef __BB_H263__
#define __BB_H263__

// Define this macro if your CPU requires aligned 2 and 4-byte reads
// This will slow down the compressed data decoding by a small amount
//#define REQUIRES_ALIGNED

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __x86_64__
#include <emmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>
#include <immintrin.h>
#define HAS_SSE
#endif

#if (__ARM_ARCH >= 7) || defined(__arm64__) || defined(__aarch64__)
#include <arm_neon.h>
#define HAS_NEON
#endif
#if defined (ARDUINO_ARCH_ESP32) && !defined(NO_SIMD)
#pragma GCC optimize("O2")
#if __has_include ("dsps_fft2r_platform.h")
#include "dsps_fft2r_platform.h"
#if (dsps_fft2r_sc16_aes3_enabled == 1)
#define HAS_S3_SIMD
#define MALLOC_ALIGNED(x) heap_caps_aligned_alloc(16, x, MALLOC_CAP_SPIRAM);
#ifdef __cplusplus
extern "C" {
#endif // cpp
void s3_simd_mb(uint8_t u8Type, int16_t *pS, int16_t *pD, uint32_t iPitch, int16_t *pConstants);
void s3_ycbcr_convert_420(int16_t *pY, int16_t *pCB, uint16_t *pOut, int iPitch, int16_t *pConsts, uint8_t ucPixelType);
#ifdef __cplusplus
}
#endif // cpp
int16_t s3_mb_constants[4] = {1, -2048, 2047, 2};
int16_t i16_Consts[16] = {0x80, 113, 90, 22, 46, 1, 16, 0x00ff, 1,32,2048, 16, 0x00ff, 1, 32, 2048};
#endif // S3 SIMD
#endif // __has_include
#endif // ESP32

#ifdef TRACE_MEMORY
#define MALLOC(x) trace_malloc(x)
#ifndef MALLOC_ALIGNED
#define MALLOC_ALIGNED(x) trace_malloc(x)
#endif // !MALLOC_ALIGNED
#else
#define MALLOC(x) malloc(x)
#ifndef MALLOC_ALIGNED
#define MALLOC_ALIGNED(x) malloc(x)
#endif // !MALLOC_ALIGNED
#endif // TRACE_MEMORY

#define DCTSIZE2 64
#define MB_LOWER -2048
#define MB_UPPER 2047
#define MCU0 (DCTSIZE2*0)
#define MCU1 (DCTSIZE2*1)
#define MCU2 (DCTSIZE2*2)
#define MCU3 (DCTSIZE2*3)
#define MCU4 (DCTSIZE2*4)
#define MCU5 (DCTSIZE2*5)
#ifdef REQUIRES_ALIGNED
#define MOTOSHORT(p) (((*(p))<<8) + (*(p+1)))
#define MOTOLONG(p) (((*p)<<24) + ((*(p+1))<<16) + ((*(p+2))<<8) + (*(p+3)))
#else
#define MOTOSHORT(p) (__builtin_bswap16(*(uint16_t *)p))
#define MOTOLONG(p) (__builtin_bswap32(*(uint32_t *)p))
#endif
#define H263_FILE_BUF_SIZE 2048
#define H263_FILE_BUF_HIGHWATER (H263_FILE_BUF_SIZE / 2)
//
// Keep track of memory allocations
//
void * trace_malloc(size_t size)
{
static uint32_t iTotal = 0;
    
    iTotal += size;
#ifdef ARDUINO
    Serial.printf("malloc size = %d, total = %d\n", (int)size, (int)iTotal);
#else
    printf("malloc size = %d, total = %d\n", (int)size, (int)iTotal);
#endif // !ARDUINO
    return malloc(size);
} /* trace_malloc() */

enum {
    H263_SUCCESS = 0,
    H263_DECODE_ERROR,
    H263_MEMORY_ERROR,
    H263_FILEIO_ERROR,
    H263_NOT_SUPPORTED,
    H263_INVALID_PARAMETER,
    H263_INVALID_FILE,
    H263_LAST_FRAME,
    H263_NO_FRAMEBUFFER,
    H263_VIDEO_ENDED
};

// Audio codecs
enum {
    H263_AUDIO_PCM_LE,
    H263_AUDIO_PCM,
    H263_AUDIO_ULAW,
    H263_AUDIO_ALAW,
    H263_AUDIO_ADPCM
};

enum {
    H263_PIXEL_RGB565_LE = 0,
    H263_PIXEL_RGB565_BE
};
enum {
    H263_FILE_INVALID = 0,
    H263_FILE_AVI,
    H263_FILE_QT
};

typedef struct H263_file_tag
{
  int32_t iPos; // current file position
  int32_t iSize; // file size
  uint8_t *pData; // memory file pointer
  void * fHandle; // class pointer to File/SdFat or whatever you want
} H263FILE;

const uint8_t u8RangeTable[1024] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
    0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
    0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
    0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,
    0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
    0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
    0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
    0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
    0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
    0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
    0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
    0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
    0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
    0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

// Callback function prototypes
typedef int32_t (H263_READ_CALLBACK)(H263FILE *pFile, uint8_t *pBuf, int32_t iLen);
typedef int32_t (H263_SEEK_CALLBACK)(H263FILE *pFile, int32_t iPosition);
typedef void * (H263_OPEN_CALLBACK)(const char *szFilename, uint32_t *pFileSize);
typedef void (H263_CLOSE_CALLBACK)(void *pHandle);

#ifndef __BB_RECT__
#define __BB_RECT__
typedef struct bbepr {
    int x;
    int y;
    int w;
    int h;
} BB_RECT;
#endif // __BB_RECT__

typedef struct tagvideo {
    int iWidth;
    int iHeight;
    int iXOffset, iYOffset; // placement on the display
    H263_READ_CALLBACK *pfnRead;
    H263_SEEK_CALLBACK *pfnSeek;
    H263_CLOSE_CALLBACK *pfnClose;
    H263FILE H263File;
    int iFrameCX; // width in whole macroblocks (multiple of 16)
    int iFrameCY; // height in whole macroblocks (multiple of 16)
    int iStreamOff; // current offset in the file
    int iFileOff; // offset into the already-read data
    int iFileLen; // length of data currently in the file buf
    int iFileHighWater; // high water mark for data in the file buffer
    int iOptions; // conversion options
    int iDCY, iDCCb, iDCCr; // DC predictors
    int iFramePitch;
    uint32_t *pFrameList; // offsets to the start of each frame and 0x80000000 flag for key frames
    uint32_t *pFrameLengths; // size of each compressed frame
    uint32_t u32MaxFrameLength;
    uint32_t *pAudioList; // offsets to the audio blobs
    uint32_t iIndex; // offset to frame index (data offsets)
    uint32_t iIndexSizes; // offset to frame index (data sizes)
    uint32_t iIndexLen;
    uint8_t *pFramebuffer;
    uint8_t *pFrameData;
    int iAudioTotal; // number of audio packets
    int iFRefFrame; // frame number of forward reference frame
    int16_t *pBRef[3]; // backward reference frame
    int16_t *pFRef[3]; // forward reference frame
    uint16_t *pACTables;
    uint32_t ulBits;
    int iVideoLen; // bytes of video data available
    int iVideoOff; // current offset in video data
    uint32_t iMovie; // offset to movie data
    uint32_t iCurrentOffset; // offset to current frame when there is no index table
    int iVideoHighWater; // high water mark for reading more data
    int iAudioFreq;
    int iAudioLen; // bytes of audio data available
    int iAudioOff; // current offset in audio data
    int iAudioHighWater;
    int iADPCMBlock;
    int iBitRate;
    int iVBVBufferSize;
    int iCurrentFrame;
    int iLastError;
    int bPacketized; // indicates if the data stream is in packets or is raw video (single stream)
    int bAllocated; // file data was allocated and needs to be freed
    int iFrameTotal;
    uint32_t iFrameDelay;
    uint8_t u8FileBuf[H263_FILE_BUF_SIZE];
    int16_t *MCUs;
    uint8_t ucIntraQuant[64];
    uint8_t ucNonIntraQuant[64];
    int8_t cMVPredX[128]; // current and previous motion vector predictors
    int8_t cMVPredY[128]; // needed for h.263
    uint8_t ucPelAspect, cQuantizerScale, u8PixelType, u8AudioChannels;
    uint8_t u8SoundBits, u8AudioType, u8FileType;
    uint16_t *usYUVRGB; // lookup table for colorspace conversion
    BB_RECT clipRect;
} H263STATE;

// Forward declarations
int GetH263MCU(uint32_t *pTable, uint8_t *buf, int16_t *pMCU, int *iOffset, int *iBitnum, H263STATE *pVideo, int iQuant, int bTCOEF, uint8_t ucMBType);
void H263MotComp(int x, int y, int32_t iMV_X, int32_t iMV_Y, int16_t *pMCUDest, H263STATE *pVideo, int bBackward);
void H263Close(H263STATE *pState);
int ReadH263(H263STATE *pVideo);
uint32_t H263_parseQT(H263STATE *pH263, const uint8_t *pData, int iDataSize);
uint32_t H263_parseAVI(H263STATE *pH263, const uint8_t *pData, int iDataSize);
int H263_decodeFrame(H263STATE *pH263, int xoff, int yoff);
void H263_close(H263STATE *pH263);
static int H263_decodeFrameInternal(H263STATE *pVideo, uint8_t *pData, int iDataLen);
#if defined( __LINUX__ ) || defined( __MACH__ )
static void * linuxOpen(const char *filename, uint32_t *size) {
    static FILE *myfile;
    size_t len;
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
#endif // __LINUX__

#ifdef __cplusplus
//
// The BB_H263 class wraps portable C code which does the actual work
//
class BB_H263
{
    
  public:
    BB_H263() {memset(&_h263, 0, sizeof(_h263));}
    int decodeFrame(int x = 0, int y = 0);
    int open(const uint8_t *pData, int iDataSize);
    int open(const char *szFilename, H263_OPEN_CALLBACK *pfnOpen, H263_CLOSE_CALLBACK *pfnClose, H263_READ_CALLBACK *pfnRead, H263_SEEK_CALLBACK *pfnSeek);
#if defined( __LINUX__ ) || defined ( __MACH__ )
    int open(const char *szFilename);
#endif
    int setClipRect(BB_RECT *pRect);
    int getClipRect(BB_RECT *pRect);
    void setFramebuffer(uint8_t *pFramebuffer, int iPitch = -1) { _h263.pFramebuffer = pFramebuffer; _h263.iFramePitch = iPitch;}
    uint8_t *getFramebuffer(void) {return _h263.pFramebuffer;}
    int allocFramebuffer(void);
    void freeFramebuffer(void);
    void close(void);
    int getWidth() {return _h263.iWidth;}
    int getFiletype() {return _h263.u8FileType;}
    int getCurrentFrame() {return _h263.iCurrentFrame;}
    int getHeight() {return _h263.iHeight;}
    int getFrameCount() {return _h263.iFrameTotal;}
    uint32_t getFrameDelay() {return _h263.iFrameDelay;}
    int getLastError() {return _h263.iLastError;}
    void setPixelType(uint8_t u8Type) { _h263.u8PixelType = u8Type;} // defaults to little endian
    uint8_t getPixelType() {return _h263.u8PixelType;}
    
  protected:
    int openInternal(void);
    
  private:
    H263STATE _h263;
}; // class H263

int BB_H263::setClipRect(BB_RECT *pRect)
{
    uint16_t x, y, w, h;
    // Adjust to be on macroblock boundaries and within the frame size
    if (pRect) {
        x = (pRect->x & 0xfff0);
        y = (pRect->y & 0xfff0);
        w = (pRect->w + 15) & 0xfff0;
        h = (pRect->h + 15) & 0xfff0;
        if (x > _h263.iWidth || x + w > _h263.iWidth) return H263_INVALID_PARAMETER;
        if (y > _h263.iHeight || y + h > _h263.iHeight) return H263_INVALID_PARAMETER;
    
        _h263.clipRect.x = x;
        _h263.clipRect.y = y;
        _h263.clipRect.w = w;
        _h263.clipRect.h = h;
        return H263_SUCCESS;
    }
    return H263_INVALID_PARAMETER;
} /* setClipRect() */

int BB_H263::getClipRect(BB_RECT *pRect)
{
    if (!pRect) return H263_INVALID_PARAMETER;
    pRect->x = _h263.clipRect.x;
    pRect->y = _h263.clipRect.y;
    pRect->w = _h263.clipRect.w;
    pRect->h = _h263.clipRect.h;
    return H263_SUCCESS;
} /* getClipRect() */

void BB_H263::freeFramebuffer(void)
{
    if (_h263.pFramebuffer) {
        free(_h263.pFramebuffer);
        _h263.pFramebuffer = nullptr;
    }
} /* freeFramebuffer() */

int BB_H263::allocFramebuffer(void)
{
    if (_h263.clipRect.w) {
        // User specified a clip rectangle; create the framebuffer of that size
        _h263.pFramebuffer = (uint8_t *)MALLOC_ALIGNED(_h263.clipRect.w * _h263.clipRect.h * sizeof(uint16_t));
        _h263.iFramePitch = _h263.clipRect.w * sizeof(uint16_t);
    } else {
        _h263.pFramebuffer = (uint8_t *)MALLOC_ALIGNED(_h263.iWidth * _h263.iHeight * sizeof(uint16_t));
        _h263.iFramePitch = _h263.iWidth * sizeof(uint16_t);
    }
    return (_h263.pFramebuffer == nullptr) ? H263_MEMORY_ERROR : H263_SUCCESS;
} /* allocFramebuffer() */

int BB_H263::openInternal(void)
{
    uint8_t *s;
    uint32_t u32, u32VideoType = 0;
    int i, iLen; //, iOffset;
    int j, iFrame, iAudio;
    uint32_t u32Len, u32MaxLen, iDataSize = 0;

    iDataSize = _h263.H263File.iSize;
    _h263.u8FileType = H263_FILE_INVALID;
    _h263.pFramebuffer = nullptr;
    _h263.iCurrentFrame = 0;
    _h263.iFrameCX = 0;
    if (_h263.H263File.pData) {
        s = _h263.H263File.pData;
    } else {
        s = _h263.u8FileBuf;
        (*_h263.pfnSeek)(&_h263.H263File, 0);
        (*_h263.pfnRead)(&_h263.H263File, s, 256);
    }
    u32 = *(uint32_t *)&s[4]; // file size
    
    if (MOTOLONG(s) == 0x52494646 /* RIFF */ &&  u32 == (iDataSize-8) && MOTOLONG(&s[8]) == 0x41564920 /* AVI */) {
        _h263.u8FileType = H263_FILE_AVI;
    }
    if (MOTOLONG(&s[4]) == 0x736b6970 /*'skip'*/ || MOTOLONG(&s[4]) == 0x66747970 /*'ftyp'*/ ||
        MOTOLONG(&s[4]) == 0x6d646174 /*'mdat'*/ || MOTOLONG(&s[4]) == 0x706e6f74 /*'pnot'*/ ||
        MOTOLONG(&s[4]) == 0x6d6f6f76 /*'moov'*/ || MOTOLONG(&s[4]) == 0x77696465 /*'wide'*/) {
        _h263.u8FileType = H263_FILE_QT;
    }
    if (_h263.u8FileType == H263_FILE_INVALID) {
        return H263_INVALID_FILE;
    }
    if (_h263.u8FileType == H263_FILE_AVI) { // parse AVI file
        u32VideoType = H263_parseAVI(&_h263, s, iDataSize);
    } else {  // Parse QuickTime file
        u32VideoType = H263_parseQT(&_h263, s, iDataSize);
    }
    // Is it H263?
    if ((u32VideoType & 0xffffff) != 0x323633 /*'x263'*/)
        return H263_NOT_SUPPORTED;
    if (_h263.u8FileType == H263_FILE_AVI) {
        u32 = MOTOLONG(&s[_h263.iMovie]);
        if (u32 == 0x6d6f7669 /* movi */) { // start of the movie data
            _h263.iMovie += 4; // offset into the 'movie'
        }
    }
    // Read the index list and shrink it a bit since we don't need to use 16 bytes per entry
    _h263.pFrameList = (uint32_t *)malloc(_h263.iFrameTotal * sizeof(uint32_t));
    _h263.pFrameLengths = (uint32_t *)malloc(_h263.iFrameTotal * sizeof(uint32_t));
    if (_h263.iAudioLen) {
        _h263.pAudioList = (uint32_t *)malloc((_h263.iIndexLen - (_h263.iFrameTotal*16))/4);
    }
    iFrame = iAudio = 0; // index output
    if (_h263.H263File.fHandle) {
        (*_h263.pfnSeek)(&_h263.H263File, _h263.iIndex); // start of index
    }
    iLen = _h263.iIndexLen;
//    iOffset = 0;
    if (iLen == 0) {
        iLen = _h263.iFrameTotal*4; // Quicktime which uses atom sizes instead of offsets
//        iOffset = _h263.iMovie;
    }
    u32MaxLen = 0; // keep track of the biggest blob of compressed frame data
    for (i = 0; i<iLen; i += 256) { // what fits in our temp buffer
        if (_h263.H263File.fHandle) {
            (*_h263.pfnRead)(&_h263.H263File, s, 256); // read a block of data
        }
        if (_h263.u8FileType == H263_FILE_AVI) {
            for (j = 0; j < 256/16; j++) {
                u32 = _h263.iMovie + *(uint32_t *)&s[(j * 16) + 8] - 4;
                if (s[j*16 + 2] == 'd') { // video frame
                    if (s[(j * 16) + 4] == 0x10) { // key frame?
                        u32 |= 0x80000000; // mark the high bit
                    }
                    u32Len = *(uint32_t *)&s[(j * 16) + 12];
                    u32Len += 8; // plus chunk header
                    _h263.pFrameLengths[iFrame] = u32Len; // AVI marker+chunk length is stored ahead of the actual data
                    _h263.pFrameList[iFrame++] = u32;
                    if (u32Len > u32MaxLen) u32MaxLen = u32Len;
                } else if (s[j*16 + 2] == 'w' && _h263.pAudioList) { // audio
                    _h263.pAudioList[iAudio++] = u32;
                }
                if (iFrame == _h263.iFrameTotal) {
                    // break out of the loop
                    j = 256; i = iLen;
                }
            } // for j
        } else { // Quicktime
            for (j = 0; j < 256/4; j++) {
                u32 = MOTOLONG(&s[(j * 4)]);
//                        u32 |= 0x80000000; // mark the high bit for key frames
                _h263.pFrameList[iFrame++] = u32;
                if (iFrame == _h263.iFrameTotal) {
                    // break out of the loop
                    j = 256; i = iLen;
                }
            } // for j
        }
    } // for i
    if (_h263.u8FileType == H263_FILE_QT) { // Get the frame lengths (separate atom)
        uint32_t iFrameOff = _h263.iMovie; // in case no frame offsets in file
        if (_h263.H263File.fHandle) {
            (*_h263.pfnSeek)(&_h263.H263File, _h263.iIndexSizes); // start of index of data sizes
        }
        iLen = _h263.iFrameTotal*4; // Quicktime which uses atom sizes instead of offsets
        iFrame = 0;
        for (i = 0; i<iLen; i += 256) { // what fits in our temp buffer
            if (_h263.H263File.fHandle) {
                (*_h263.pfnRead)(&_h263.H263File, s, 256); // read a block of data
            }
            for (j = 0; j < 256/4; j++) {
                u32 = MOTOLONG(&s[(j * 4)]);
                if (_h263.iIndex == 0) { // no index present, make one
                    _h263.pFrameList[iFrame] = iFrameOff;
                    iFrameOff += u32; // next frame will start here
                }
                _h263.pFrameLengths[iFrame++] = u32; // save the length
                if (u32 > u32MaxLen) u32MaxLen = u32;
                if (iFrame == _h263.iFrameTotal) {
                    // break out of the loop
                    j = 256; i = iLen;
                }
            } // for j
        } // for i
    }
    _h263.u32MaxFrameLength = u32MaxLen;
    //printf("max len = %d\n", u32MaxLen);
    _h263.pFrameData = (uint8_t *)malloc(u32MaxLen + 8); // allocate a buffer for the compressed data
    _h263.iAudioTotal = iAudio; // number of audio packets
    return H263_SUCCESS;

} /* openInterna() */

// Class implementation
int BB_H263::open(const uint8_t *pData, int iDataSize)
{
    _h263.H263File.pData = (uint8_t *)pData;
    _h263.H263File.iSize = iDataSize;
    return openInternal();
} /* open() */

#if defined( __LINUX__ ) || defined ( __MACH__ )
int BB_H263::open(const char *szFilename)
{
    FILE *pFile;
    uint32_t iDataSize;
    
    if (!szFilename) return H263_INVALID_PARAMETER;
    
    _h263.pfnRead = linuxRead;
    _h263.pfnSeek = linuxSeek;
    _h263.pfnClose = linuxClose;
    pFile = (FILE *)linuxOpen(szFilename, &iDataSize);
    if (!pFile) return H263_FILEIO_ERROR;
    if (iDataSize < 4096) {
        linuxClose(pFile);
        return H263_INVALID_FILE;
    }
//    pData = (uint8_t *)malloc(iDataSize);
//    _h263.bAllocated = 1;
    _h263.H263File.fHandle = pFile;
    _h263.H263File.iPos = 0; // current file position
    _h263.H263File.iSize = iDataSize; // file size
//    _h263.H263File.pData = pData;
    // Debug - read the file into memory
//    linuxRead(&_h263.H263File, pData, iDataSize);
//    linuxClose(_h263.H263File.fHandle);
//    _h263.H263File.fHandle = NULL; // DEBUG - treat it as not a file
    return openInternal();
} /* open() */
#endif // __LINUX__

void BB_H263::close(void)
{
    H263_close(&_h263);
} /* close() */

int BB_H263::open(const char *szFilename, H263_OPEN_CALLBACK *pfnOpen, H263_CLOSE_CALLBACK *pfnClose, H263_READ_CALLBACK *pfnRead, H263_SEEK_CALLBACK *pfnSeek)
{
    FILE *pFile;
    uint32_t iDataSize;
    
    if (!pfnOpen || !pfnClose || !pfnRead || !pfnSeek) return H263_INVALID_PARAMETER;
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
    return H263_decodeFrame(&_h263, x, y);
} /* decodeFrame() */

#endif // __cplusplus
//
// Decode a frame of video (composed of codebooks and strips)
//
int H263_decodeFrame(H263STATE *pH263, int xoff, int yoff)
{
    uint32_t u32ChunkLen, iOffset;
    uint8_t *s;
    uint16_t i;
    int32_t iPos, iDelta;
    int rc = H263_SUCCESS;
    
    if (!pH263) return H263_INVALID_PARAMETER;
    if (pH263->iCurrentFrame >= pH263->iFrameTotal) return H263_VIDEO_ENDED;
    if (pH263->pFramebuffer == nullptr) return H263_NO_FRAMEBUFFER;
    
    pH263->iXOffset = xoff;
    pH263->iYOffset = yoff;
    if (pH263->H263File.pData) { // reading directly from memory
        s = pH263->H263File.pData;
        iOffset = pH263->iMovie;
        if (pH263->u8FileType == H263_FILE_AVI) {
            iOffset += *(uint32_t *)&s[pH263->iIndex + (pH263->iCurrentFrame * 16) + 8];
            iOffset -= 4;
            if (MOTOLONG(&s[iOffset]) != 0x30306463 /* 00dc */) { // frame data
                rc = H263_DECODE_ERROR;
                goto decode_exit;
            }
            iOffset += 4;
            u32ChunkLen = *(uint32_t *)&s[iOffset];
            iOffset += 4;
            while (u32ChunkLen == 0 && MOTOLONG(&s[iOffset]) == 0x30306463 && iOffset < pH263->H263File.iSize) { // empty block, get next
                iOffset += 4;
                u32ChunkLen = *(uint32_t *)&s[iOffset];
                iOffset += 4;
            }
        } else { // Quicktime
            // The iIndex value is the offset to the 'stco' - sample chunk offset list
            if (pH263->iIndex) { // frame index exists
                iOffset = pH263->iIndex + (pH263->iCurrentFrame * 4);
                iOffset = MOTOLONG(&s[iOffset]); // get the data offset
                u32ChunkLen = MOTOLONG(&s[iOffset]) & 0xffffff;
            } else {
                if (pH263->iCurrentFrame == 0) { // reset offset to first frame
                    pH263->iCurrentOffset = pH263->iMovie;
                }
                iOffset = pH263->iCurrentOffset;
                // offset already set, get the length and add it
                u32ChunkLen = MOTOLONG(&s[pH263->iIndexSizes + pH263->iCurrentFrame * 4]);
                pH263->iCurrentOffset += u32ChunkLen;
            }
        }
        s += iOffset; // point to the compressed data of this frame
    } else {
        iOffset = 0;
        iPos = pH263->pFrameList[pH263->iCurrentFrame] & 0x7fffffff; // desired position
        iDelta = iPos - pH263->H263File.iPos;
        if (iDelta >= 0 && iDelta < 4096) { // avoid seeking since it's really slow on uSD cards!
            while (iDelta > 0) {
                // read and throw away the data
                i = iDelta;
                if (i > H263_FILE_BUF_SIZE) i = H263_FILE_BUF_SIZE;
                (*pH263->pfnRead)(&pH263->H263File, pH263->u8FileBuf, i);
                iDelta -= i;
            }
        } else { // The new position is behind or far ahead of the current read position, so we must use seek
            (*pH263->pfnSeek)(&pH263->H263File, iPos);
        }
//        s = pH263->u8FileBuf;
        s = pH263->pFrameData;
        iOffset = 0;
        //printf("len = %d\n", pH263->pFrameLengths[pH263->iCurrentFrame]);
        (*pH263->pfnRead)(&pH263->H263File, s, pH263->pFrameLengths[pH263->iCurrentFrame]); // read the compressed frame
        if (pH263->u8FileType == H263_FILE_AVI) {
            if (MOTOLONG(s) != 0x30306463 /* 00dc */) { // frame data
                rc = H263_DECODE_ERROR;
                goto decode_exit;
            }
            iOffset += 4;
            u32ChunkLen = *(uint32_t *)&s[iOffset];
            iOffset += 4;
            while (u32ChunkLen == 0 && MOTOLONG(&s[iOffset]) == 0x30306463 && iOffset < 252) { // empty block, get next
                iOffset += 4;
                u32ChunkLen = *(uint32_t *)&s[iOffset];
                iOffset += 4;
            }
            if (iOffset >= 252 || (u32ChunkLen == 0 && pH263->pFrameLengths[pH263->iCurrentFrame] > 8) || u32ChunkLen > 256000) {
                rc = H263_DECODE_ERROR; // something went wrong
                goto decode_exit;
            }
        } else {
            u32ChunkLen = MOTOLONG(&s[iOffset]) & 0xffffff;
//            iOffset += 4;
        }
        if (u32ChunkLen & 1) {  // can't be odd
            u32ChunkLen++;
        }
    }
    H263_decodeFrameInternal(pH263, s, u32ChunkLen);
decode_exit:
    pH263->iCurrentFrame++;
    if (pH263->iCurrentFrame == pH263->iFrameTotal) { // reset to start to automatically loop videos
        pH263->iCurrentFrame = 0;
        rc = H263_LAST_FRAME;
    }
    return rc;
} /* H263_decodeFrame() */

void H263_close(H263STATE *pH263) {
    if (!pH263) return;
    if (pH263->H263File.fHandle && pH263->pfnClose) {
        (*pH263->pfnClose)(pH263->H263File.fHandle);
        pH263->H263File.fHandle = nullptr;
    } else if (pH263->bAllocated){
        free(pH263->H263File.pData);
        pH263->H263File.pData = NULL;
    }
    if (pH263->pFrameData) {
        free(pH263->pFrameData);
        pH263->pFrameData = NULL;
    }
    if (pH263->usYUVRGB) {
        free(pH263->usYUVRGB);
        pH263->usYUVRGB = NULL;
    }
    if (pH263->pFrameList) {
        free(pH263->pFrameList);
        pH263->pFrameList = NULL;
    }
    if (pH263->pFrameLengths) {
        free(pH263->pFrameLengths);
        pH263->pFrameLengths = NULL;
    }
    if (pH263->pAudioList) {
        free(pH263->pAudioList);
        pH263->pAudioList = NULL;
    }
    if (pH263->pACTables) {
        free(pH263->pACTables);
        pH263->pACTables = NULL;
    }
    if (pH263->pBRef[0]) {
        free(pH263->MCUs); pH263->MCUs = NULL;
        free(pH263->pBRef[0]); pH263->pBRef[0] = NULL;
        free(pH263->pBRef[1]); pH263->pBRef[1] = NULL;
        free(pH263->pBRef[2]); pH263->pBRef[2] = NULL;
        free(pH263->pFRef[0]); pH263->pBRef[0] = NULL;
        free(pH263->pFRef[1]); pH263->pBRef[1] = NULL;
        free(pH263->pFRef[2]); pH263->pBRef[2] = NULL;
    }
} /* H263_close() */

//
// Parse the useful info from a Quicktime file to allow playback
//
uint32_t H263_parseQT(H263STATE *pH263, const uint8_t *pData, int iDataSize)
{
    uint8_t *s;
    uint32_t u32, i, j, iOffset;
    uint32_t u32Chunk, u32Type, u32VideoType = 0; //, u32AudioType = 0;
    uint32_t iTimeScale = 0, iAudioTimeScale = 0, iVideoTimeScale = 0;
    
    s = (uint8_t *)pData;
    if (pH263->H263File.fHandle) { // from a file
//        pEnd = &s[256];
    } else {
//        pEnd = &s[iDataSize];
    }
    iOffset = i = 0;
    u32Type = 0;
    // i is the file offset and iOffset is the buffer offset
    while (i + iOffset < iDataSize) {
        u32 = MOTOLONG(&s[iOffset]);
        u32Chunk = MOTOLONG(&s[iOffset+4]); // atom type
        switch (u32Chunk) {
            case 0x636d6f76: // cmov - compressed movie
                pH263->iMovie = i + iOffset + 32;
                break;
            case 0x736f756e: // 'soun' sound info
                break;
            case 0x706e6f74: // 'pnot' preview image
                break;
            case 0x6d646174: // 'mdat' movie data
                pH263->iMovie = i + iOffset;
                break;
            case 0x6d6f6f76: // 'moov' atom contains child atoms
                iOffset += 8;
                continue;
            case 0x7472616b: // 'trak' track info, dive in
                iOffset += 8;
                continue;
            case 0x6d646961: //'mdia' media Atom, dive in
                iOffset += 8;
                continue;
            case 0x6d696e66: //'minf' we need media info, dive in
                iOffset += 8;
                continue;
            case 0x50494354: //'PICT' picture info
                break;
            case 0x746b6864: //'tkhd' information about the video size
                if (MOTOLONG(&s[iOffset+82]) && pH263->iWidth == 0) {
                    pH263->iWidth = MOTOLONG(&s[iOffset+82]);
                    pH263->iHeight = MOTOLONG(&s[iOffset+86]);
                } // bpp?
                break;
            case 0x6d646864: //'mdhd' media header
                iTimeScale = MOTOLONG(&s[iOffset+20]);
                break;
            case 0x766d6864: //'vmhd' movie header info
                u32Type = MOTOLONG(&s[iOffset+4]);    // keep the media type for later
                iVideoTimeScale = iTimeScale;    // previously defined by media header
                break;
            case 0x736d6864: //'smhd' sound header info
                u32Type = MOTOLONG(&s[iOffset+4]);    // keep the media type for later
                iAudioTimeScale = iTimeScale;   // previously defined by media header
                break;
            case 0x7374626c: //'stbl' sample table info
                iOffset += 8;
                continue; // nest down 1 level
            case 0x73747364: //'stsd' sample description
                if (u32Type == 0x766d6864 /*'vmhd'*/) { // video codec
                    //                  if (MOTOLONG(s[iOffset+20]) == 0x61766331 /*'avc1'*/)
                    //                     pH263->u8Codec = PIL_COMP_H264;
                    u32VideoType = MOTOLONG(&s[iOffset+20]);
                    //pH263->u8Bpp = s[iOffset+99];
                    //== 0x6a706567 /*'jpeg'*/ || MOTOLONG(&s[iOffset+20]) == 0x6d6a7067 /*'mjpg'*/)
                    //                      if (MOTOLONG(&s[iOffset+20]) == 0x6d6a7061 /*'mjpa'*/ || MOTOLONG(&s[iOffset+20]) == 0x6d6a7062 /*'mjpb'*/)
                    //                        pH263->u8Codec = PIL_COMP_MJPEG_AB;
                    //                  if (MOTOLONG(&s[iOffset+20]) == 0x63766964 /*'cvid'*/)
                    //                      pH263->u8Codec = PIL_COMP_CINEPAK; // we can handle it
                    //                 if (MOTOLONG(&s[iOffset+20]) == 0x68323633 /*'h263'*/ || MOTOLONG(&s[iOffset+20]) == 0x73323633 /*'s263'*/)
                    //                   pH263->u8Codec = PIL_COMP_H263; // we can handle it
                    }
                    if (u32Type == 0x736d6864 /*'smhd'*/ /* && pFile->cSoundBits == 0*/ ) { // audio codec
                        pH263->u8AudioChannels = s[iOffset+41];
                        pH263->u8SoundBits = s[iOffset+43]; // audio bits per sample
                        if (MOTOLONG(&s[iOffset+20]) == 0x736f7774 /*'sowt'*/) // twos backwards == little endian data
                            pH263->u8AudioType = H263_AUDIO_PCM_LE; // we can handle it
                        if (MOTOLONG(&s[iOffset+20]) == 0x72617720 /*'raw '*/ || MOTOLONG(&s[iOffset+20]) == 0x74776f73 /*'twos'*/)
                            pH263->u8AudioType = H263_AUDIO_PCM; // we can handle it
                        if (MOTOLONG(&s[iOffset+20]) == 0x756c6177 /*'ulaw'*/)
                            pH263->u8AudioType = H263_AUDIO_ULAW; // we can handle it
                        if (MOTOLONG(&s[iOffset+20]) == 0x616c6177 /*'alaw'*/)
                            pH263->u8AudioType = H263_AUDIO_ALAW; // we can handle it
                        if (MOTOLONG(&s[iOffset+20]) == 0x696d6134 /*'ima4'*/) {
                            pH263->u8AudioType = H263_AUDIO_ADPCM; // we can handle it
                            pH263->iADPCMBlock = 34; // QT uses 34-byte blocks
                        }
                        // a/uLaw are implicitly 8 bits. MOV Files get it wrong.
                        if ((H263_AUDIO_ULAW == pH263->u8AudioType) || (H263_AUDIO_ALAW == pH263->u8AudioType))
                            pH263->u8SoundBits = 8;
                    }
                    break;
                case 0x73747363: //'stsc' sample-to-chunk table
                    if (u32Type == 0x766d6864 /*'vmhd'*/) {
                        //                      iSamplesToChunksOffset = iOffset + i;
                        //                      iSTCLen = iLen;
                    }
                    if (u32Type == 0x736d6864 /*'smhd'*/) {
                        //                      iAudioSamplesToChunksOffset = iOffset + i;
                        //                      iSTCAudioLen = iLen;
                    }
                    break;
                case 0x73747473: //'stts' time-to-sample atom
                    j = MOTOLONG(&s[iOffset+20]);
                    if (j != 0) { // don't allow a divide by zero
                        if (u32Type == 0x766d6864 /*'vmhd'*/ && iVideoTimeScale != 0 && (iVideoTimeScale/j) != 0) // video info
                            pH263->iFrameDelay = 1000000 / (iVideoTimeScale / j);
                        if (u32Type == 0x736d6864 /*'smhd'*/) // audio info
                            pH263->iAudioFreq = iAudioTimeScale / j;
                    }
                    break;
                case 0x73747373: //'stss' Sync sample atom
                    //                   iSyncSize = MOTOLONG(&cBuf[i+12]); // number of entries in the table
                    //                   iSyncTable = iOffset + 16;
                    break;
                case 0x7374636f: //'stco' sample chunk offsets
                    if (u32Type == 0x766d6864 /*'vmhd'*/ ) {
                        if (MOTOLONG(&s[iOffset+12]) == 1) {
                            pH263->iMovie = MOTOLONG(&s[iOffset + 16]);
                        } else {
                            pH263->iIndex = i + iOffset + 16; // start of absolute file offsets
                            pH263->iFrameTotal = MOTOLONG(&s[iOffset+12]);
                            pH263->iIndexLen = pH263->iFrameTotal * 4;
                        }
                    }
                    //                      iChunkOffset = iOffset + i; // offset to chunk offset list
                    //                   if (ulMediaType == 0x736d6864 /*'smhd'*/)
                    //                      iAudioChunkOffset = iOffset + i; // offset to chunk offset list
                    break;
                case 0x7374737a: //'stsz' sample sizes
                    if (u32Type == 0x766d6864 /*'vmhd'*/) { // movie data sizes
                        pH263->iFrameTotal = MOTOLONG(&s[iOffset+16]); // number of samples = number of frames
                        pH263->iIndexSizes = iOffset + i + 20; // save this for later
//                        if (pH263->iFrameTotal == 1) // single frame, get its length
//                            iSampleSize = MOTOLONG(&s[iOffset+12]);
//                        if ((u32 & 1) && MOTOLONG(&s[iOffset+12]) == 0) { // variable sized samples, don't trust atom size
//                            iLen = (pH263->iFrameTotal * 4) + 20;
//                        }
                    }
                    if (u32Type == 0x736d6864 /*'smhd'*/) { // sound data sizes
                        //                      pFile->iSoundLen = MOTOLONG(&cBuf[i+16]); // number of sound samples (bytes)
                        //                      iAudioIndex = iOffset + i;
                    }
                    break;
                } // switch on atom type
                iOffset += u32;
                if (pH263->H263File.fHandle) { // from file - seek and read a new block of data
                    i += iOffset;
                    iOffset = 0;
                    (*pH263->pfnSeek)(&pH263->H263File, i);
                    (*pH263->pfnRead)(&pH263->H263File, s, 256);
                }
            }
    return u32VideoType;
} /* H263_parseQT() */
//
// Parse the useful info from an AVI file to allow playback
//
uint32_t H263_parseAVI(H263STATE *pH263, const uint8_t *pData, int iDataSize)
{
    uint8_t *s;
    uint32_t u32, iExtra, i, iOffset;
    uint32_t u32Chunk, u32VideoType = 0; //, u32AudioType = 0;

    s = (uint8_t *)pData;
    if (pH263->H263File.fHandle) { // from a file
 //       pEnd = &s[256];
        iDataSize = pH263->H263File.iSize;
    } else {
//        pEnd = &s[iDataSize];
    }

    i = 0;
    iOffset = 12; // point to first chunk
    while (i+iOffset < iDataSize) {
        u32 = *(uint32_t *)&s[iOffset+4]; // offset to next chunk/list
        if (u32 + iOffset > iDataSize) {
            u32 &= 0xffffff; // some files have bogus upper byte
        }
        u32Chunk = MOTOLONG(&s[iOffset]);
        switch (u32Chunk) { // switch on RIFF chunk
            case 0x69647831: // idx1 - index chunk
                pH263->iIndex = i + iOffset + 8; // start of index chunk
                pH263->iIndexLen = *(uint32_t *)&s[iOffset+4];
                break;
            case 0x61766968: //'avih' AVI header - we need this info
                pH263->iFrameTotal = *(uint32_t *)&s[iOffset+24]; // number of frames
                pH263->iWidth = *(uint32_t *)&s[iOffset+40];
                pH263->iHeight = *(uint32_t *)&s[iOffset+44];
                pH263->iFrameDelay = *(uint32_t *)&s[iOffset+8]; // microseconds per frame
                break;
            case 0x73747266: //'strf' video and info
                iExtra = 0;
                if (u32 >= 20) // only present if structure is 20 bytes or larger
                    iExtra = *(uint16_t *)&s[iOffset+24]; // cbSize has extra info for ADPCM
                if ((u32 - iExtra) == 16 || (u32-iExtra) == 18 || (u32-iExtra) == 20) { // waveformatex
                    //                  pH263->cSoundBits = cBuf[i+22]; // audio bits per sample
                    //                  pH263->cAudioChannels = cBuf[i+10]; // 1=mono, 2=stereo
                    //                  pH263->iSampleFreq = *(uint32_t *)&s[iOffset+12];  // get the sample rate
                    //                  pH263->iADPCMBlock = *(uint16_t *)&s[iOffset+20]; // nBlockAlign is the ADPCM block size
                    //                  if (s[iOffset+8] == 6) // CCITT ALAW
                    //                     pH263->cAudioCodec = H263_AUDIO_ALAW;
                    //                  if (s[iOffset+8] == 7)
                    //                     pH263->cAudioCodec = H263_AUDIO_ULAW;
                    //                  if (s[iOffset+8] == 1)
                    //                     pH263->cAudioCodec = H263_AUDIO_PCM_LE;
                    //                  if (s[iOffset+8] == 0x11 || s[iOffset+8] == 0x02) // ADPCM
                    //                     pH263->cAudioCodec = H263_AUDIO_ADPCM;
                }
                if (u32 == 0x28 || u32 == 0x428) { // bitmapinfo or bitmapinfo + palette
                    if (u32VideoType == 0) {
                        u32VideoType = MOTOLONG(&s[iOffset+24]); // get the codec type
                        pH263->iWidth = *(uint32_t *)&s[iOffset+12];
                        pH263->iHeight = *(uint32_t *)&s[iOffset+16];
                    }
                    //if (pH263->u8Bpp == 0) {
                    //    pH263->u8Bpp = s[iOffset+22];
                    //    if (pH263->u8Bpp == 40) // monochrome reads as 40bpp
                    //        pH263->u8Bpp = 8;
                    //}
                }
                break;
            case 0x73747268: //'strh' audio or video info
                if (MOTOLONG(&s[iOffset+8]) == 0x76696473 /*'vids'*/) { // video info
                    u32VideoType = MOTOLONG(&s[iOffset+12]); // get the codec type
                }
                if (MOTOLONG(&s[iOffset+8]) == 0x61756473 /*'auds'*/) { // audio info
                    //                  pH263->iSampleFreq = INTELLONG(&cBuf[i+0x20]);  // get the sample rate
                    // sometimes gives number of chunks, not samples
                    //                  pH263->iSoundLen = INTELLONG(&cBuf[i+40]); // get the total number of samples
                }
                break;
            case 0x4c495354: //'LIST' a LIST, dive in if it's what we want
                if (MOTOLONG(&s[iOffset+8]) == 0x6864726c /*'hdrl'*/ || MOTOLONG(&s[iOffset+8]) == 0x7374726c /*'strl'*/) {
                    iOffset += 12; // dive into this LIST and process it's sub-parts
                    continue;
                }
                if (MOTOLONG(&s[iOffset+8]) == 0x6d6f7669 /*'movi'*/) {
                    pH263->iMovie = i + iOffset + 8; // keep this for later
                }
                break;
        } // switch on RIFF chunk
        iOffset += u32 + 8;
        if (pH263->H263File.fHandle) { // from file - seek and read a new block of data
            i += iOffset;
            iOffset = 0;
            (*pH263->pfnSeek)(&pH263->H263File, i);
            (*pH263->pfnRead)(&pH263->H263File, s, 256);
        }
    } // while scanning the AVI file
    return u32VideoType;
} /* H263_parseAVI() */

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

#define W1 2841 /* 2048*sqrt(2)*cos(1*pi/16) */
#define W2 2676 /* 2048*sqrt(2)*cos(2*pi/16) */
#define W3 2408 /* 2048*sqrt(2)*cos(3*pi/16) */
#define W5 1609 /* 2048*sqrt(2)*cos(5*pi/16) */
#define W6 1108 /* 2048*sqrt(2)*cos(6*pi/16) */
#define W7 565  /* 2048*sqrt(2)*cos(7*pi/16) */
static void idctrow(int16_t *blk)
{
int x0, x1, x2, x3, x4, x5, x6, x7, x8;
    
  /* shortcut for row of 0s */
  if (!((x1 = blk[4]*2048) | (x2 = blk[6]) | (x3 = blk[2]) |
        (x4 = blk[1]) | (x5 = blk[7]) | (x6 = blk[5]) | (x7 = blk[3])))
  {
    blk[0]=blk[1]=blk[2]=blk[3]=blk[4]=blk[5]=blk[6]=blk[7]=blk[0]*8;
  } else {
      x0 = (blk[0]*2048) + 128; /* for proper rounding in the fourth stage */
      
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
      blk[0] = (int16_t)((x7+x1)>>8);
      blk[1] = (int16_t)((x3+x2)>>8);
      blk[2] = (int16_t)((x0+x4)>>8);
      blk[3] = (int16_t)((x8+x6)>>8);
      blk[4] = (int16_t)((x8-x6)>>8);
      blk[5] = (int16_t)((x0-x4)>>8);
      blk[6] = (int16_t)((x3-x2)>>8);
      blk[7] = (int16_t)((x7-x1)>>8);
  }
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
static void idctcol(int16_t *blk)
{
  int x0, x1, x2, x3, x4, x5, x6, x7, x8;
  int t;

  /* shortcut for column of 0s */
  if (!((x1 = (blk[8*4]*256)) | (x2 = blk[8*6]) | (x3 = blk[8*2]) |
        (x4 = blk[8*1]) | (x5 = blk[8*7]) | (x6 = blk[8*5]) | (x7 = blk[8*3])))
  {
      t = (blk[8*0]+32)>>6;
      if (t < -256) t = -256;
      if (t > 255) t = 255;
      blk[8*0]=blk[8*1]=blk[8*2]=blk[8*3]=blk[8*4]=blk[8*5]=blk[8*6]=blk[8*7]=(int16_t)t;
  } else {
      x0 = (blk[8*0]*256) + 8192;
      
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
      blk[8*0] = (int16_t)t;
      t = (x3+x2)>>14;
      blk[8*1] = (int16_t)t;
      t = (x0+x4)>>14;
      blk[8*2] = (int16_t)t;
      t = (x8+x6)>>14;
      blk[8*3] = (int16_t)t;
      t = (x8-x6)>>14;
      blk[8*4] = (int16_t)t;
      t = (x0-x4)>>14;
      blk[8*5] = (int16_t)t;
      t = (x3-x2)>>14;
      blk[8*6] = (int16_t)t;
      t = (x7-x1)>>14;
      blk[8*7] = (int16_t)t;
  }
}
//
// 2-dimensional inverse discrete cosine transform
//
void H263IDCT(int16_t *block)
{
int i;
    int16_t *s = block;
    
    for (i=0; i<8; i++) {
        idctrow(s);
        s += 8;
    }
    for (i=0; i<8; i++) {
        idctcol(block+i); //, ulMap);
    }
} /* H263IDCT() */

//
// Convert a 16x16 macroblock from YUV to RGB565 and store it in the destination framebuffer
//
void H263PutMCU22(H263STATE *pVideo, int x, int y, int16_t *pMCU, int iYBias, int iCrCbBias)
{
//signed long Cr,Cb;
//int32_t Y1, Y2, Y3, Y4;
int iRow;
int16_t *pY, *pCr, *pCb;
uint32_t *ulDest; // define as long to get around compiler innefficiency
//int32_t iCBG, iCRG, iCBB, iCRR;
int iMaxCol, iMaxRow;
const int iPitch = pVideo->iFramePitch;
#ifdef HAS_NEON
// 16-bit constants for NEON ycc->rgb conversion
static const int16_t __attribute__((aligned(16))) sYCCRGBConstants[4] = {5742/2, -2925/2, -1409/2, 7258/2};
#else
const int iRowOffsets[8] = {0,16,32,48,128,144,160,176};
int iCol;
    uint16_t usIndex;
    int16_t s;
    uint32_t ulPixel;
    const int iPitch32 = iPitch/4; // pitch in uint32_t's
#endif
    // Adjust position for clipped output
    if (pVideo->clipRect.w) {
        x -= (pVideo->clipRect.x >> 4);
        if (x < 0 || (x<<4) >= pVideo->clipRect.w) return; // not visible
        y -= (pVideo->clipRect.y >> 4);
        if (y < 0 || (y<<4) >= pVideo->clipRect.h) return;
    }
   pCb = (int16_t *)&pMCU[MCU4];
   pCr = (int16_t *)&pMCU[MCU5];

   /* Convert YCC pixels into RGB pixels and store in output image */
   ulDest = (uint32_t *)&pVideo->pFramebuffer[y*16*iPitch + x*32]; // destination 16x16 block of output image
   // Set the block clipping size so we don't draw beyond the image borders
   iMaxRow = iMaxCol = 7;
   if ((y+1)*16 > pVideo->iHeight)
      iMaxRow = (pVideo->iHeight/2) & 7;
   if ((x+1)*16 > pVideo->iWidth)
      iMaxCol = (pVideo->iWidth/2) & 7;
#ifdef HAS_NEON
    const int16x4_t i164Constants = vld1_s16(&sYCCRGBConstants[0]); // 4 different constants used for "lane" multiplications by scalar
    const int16x8_t p16 = vdupq_n_s16(16);
    const int16x8_t p128 = vdupq_n_s16(-0x8000);
    int16x8x2_t cr16x8x2, cb16x8x2;
    int16x8_t cb16x8, cr16x8, cg16x8, y00_16x8, y01_16x8, y10_16x8, y11_16x8;
    int16x8_t iCBB, iCBG, iCRG, iCRR;
    uint8x8_t u88B, u88R, u88G;
    uint16x8_t u168Temp, u168Temp2;
    pY = (int16_t *)&pMCU[MCU0];
    // Each pass through the loop produces 16x2 pixels, so 8 iterations are needed for 16x16
       for (iRow=0; iRow<=iMaxRow; iRow++) { // do up to 8 pairs of rows
           cr16x8 = vld1q_s16(pCr); // load 1 row of Cr
           cb16x8 = vld1q_s16(pCb); // load 1 row of Cb
           y00_16x8 = vld1q_s16(pY); // load top row of Y (left block)
           y10_16x8 = vld1q_s16(pY+64); // load top row of Y (right block)
           y01_16x8 = vld1q_s16(pY+8); // load bottom row of Y (left block)
           y11_16x8 = vld1q_s16(pY+64+8); // load bottom row of Y (right block)
           cr16x8 = vshlq_n_s16(cr16x8, 8); // ready for mult-take-high-part
           cb16x8 = vshlq_n_s16(cb16x8, 8);
           cr16x8 = vaddq_s16(cr16x8, p128); // -128
           cb16x8 = vaddq_s16(cb16x8, p128);
           y00_16x8 = vsubq_s16(y00_16x8, p16); // - 16
           y10_16x8 = vsubq_s16(y10_16x8, p16);
           y01_16x8 = vsubq_s16(y01_16x8, p16); // - 16
           y11_16x8 = vsubq_s16(y11_16x8, p16);
           y00_16x8 = vshlq_n_s16(y00_16x8, 4); // adjust scale of Y to match cr/cb
           y10_16x8 = vshlq_n_s16(y10_16x8, 4);
           y01_16x8 = vshlq_n_s16(y01_16x8, 4);
           y11_16x8 = vshlq_n_s16(y11_16x8, 4);
           cr16x8x2 = vzipq_s16(cr16x8, cr16x8); // double each cr
           cb16x8x2 = vzipq_s16(cb16x8, cb16x8); // double each cb
       // top row of left block
           // multiply by a large constant to get a 32-bit result, then keep the top 16-bits
           iCBB = vqdmulhq_lane_s16(cb16x8x2.val[0], i164Constants, 0);
           iCBG = vqdmulhq_lane_s16(cb16x8x2.val[0], i164Constants, 1);
           iCRG = vqdmulhq_lane_s16(cr16x8x2.val[0], i164Constants, 2);
           iCRR = vqdmulhq_lane_s16(cr16x8x2.val[0], i164Constants, 3);
           // create 8 pixels each of blue, green and red
           cb16x8 = vaddq_s16(iCBB, y00_16x8); // left 8 blue pixels
           cg16x8 = vaddq_s16(y00_16x8, iCBG);
           cg16x8 = vaddq_s16(cg16x8, iCRG); // left 8 green pixels
           cr16x8 = vaddq_s16(y00_16x8, iCRR); // left 8 red pixels
           // Reduce to 8-bits per color stimulus and clip to 0-255
           u88R = vqrshrun_n_s16(cr16x8, 4); // shift right, narrow and saturate to 8-bit unsigned
           u168Temp = vshll_n_u8(u88R, 8); // place red in upper part of 16-bit words
           u88B = vqrshrun_n_s16(cb16x8, 4); // shift right, narrow and saturate to 8-bit unsigned
           u88G = vqrshrun_n_s16(cg16x8, 4); // shift right, narrow and saturate to 8-bit unsigned
           u168Temp2 = vshll_n_u8(u88G, 8); // shift green elements to top of 16-bit words
           u168Temp = vsriq_n_u16(u168Temp, u168Temp2, 5); // shift green elements right and insert red elements
           u168Temp2 = vshll_n_u8(u88B, 8); // shift blue elements to top of 16-bit words
           u168Temp = vsriq_n_u16(u168Temp, u168Temp2, 11); // shift blue elements right and inserted into bottom 5 bits
           vst1q_u16((uint16_t *)ulDest, u168Temp);
           // second row of left block
           cb16x8 = vaddq_s16(iCBB, y01_16x8); // left 8 blue pixels
           cg16x8 = vaddq_s16(y01_16x8, iCBG);
           cg16x8 = vaddq_s16(cg16x8, iCRG); // left 8 green pixels
           cr16x8 = vaddq_s16(y01_16x8, iCRR); // left 8 red pixels
           u88R = vqrshrun_n_s16(cr16x8, 4); // shift right, narrow and saturate to 8-bit unsigned
           u168Temp = vshll_n_u8(u88R, 8); // place red in upper part of 16-bit words
           u88B = vqrshrun_n_s16(cb16x8, 4); // shift right, narrow and saturate to 8-bit unsigned
           u88G = vqrshrun_n_s16(cg16x8, 4); // shift right, narrow and saturate to 8-bit unsigned
           u168Temp2 = vshll_n_u8(u88G, 8); // shift green elements to top of 16-bit words
           u168Temp = vsriq_n_u16(u168Temp, u168Temp2, 5); // shift green elements right and insert red elements
           u168Temp2 = vshll_n_u8(u88B, 8); // shift blue elements to top of 16-bit words
           u168Temp = vsriq_n_u16(u168Temp, u168Temp2, 11); // shift blue elements right and inserted into bottom 5 bits
           vst1q_u16((uint16_t *)ulDest + iPitch/2, u168Temp);
           // top row of right block
           iCBB = vqdmulhq_lane_s16(cb16x8x2.val[1], i164Constants, 0);
           iCBG = vqdmulhq_lane_s16(cb16x8x2.val[1], i164Constants, 1);
           iCRG = vqdmulhq_lane_s16(cr16x8x2.val[1], i164Constants, 2);
           iCRR = vqdmulhq_lane_s16(cr16x8x2.val[1], i164Constants, 3);
           cb16x8 = vaddq_s16(iCBB, y10_16x8); // left 8 blue pixels
           cg16x8 = vaddq_s16(y10_16x8, iCBG);
           cg16x8 = vaddq_s16(cg16x8, iCRG); // left 8 green pixels
           cr16x8 = vaddq_s16(y10_16x8, iCRR); // left 8 red pixels
           u88R = vqrshrun_n_s16(cr16x8, 4); // shift right, narrow and saturate to 8-bit unsigned
           u168Temp = vshll_n_u8(u88R, 8); // place red in upper part of 16-bit words
           u88B = vqrshrun_n_s16(cb16x8, 4); // shift right, narrow and saturate to 8-bit unsigned
           u88G = vqrshrun_n_s16(cg16x8, 4); // shift right, narrow and saturate to 8-bit unsigned
           u168Temp2 = vshll_n_u8(u88G, 8); // shift green elements to top of 16-bit words
           u168Temp = vsriq_n_u16(u168Temp, u168Temp2, 5); // shift green elements right and insert red elements
           u168Temp2 = vshll_n_u8(u88B, 8); // shift blue elements to top of 16-bit words
           u168Temp = vsriq_n_u16(u168Temp, u168Temp2, 11); // shift blue elements right and inserted into bottom 5 bits
           vst1q_u16((uint16_t *)ulDest + 8, u168Temp);
           // second row of right block
           cb16x8 = vaddq_s16(iCBB, y11_16x8); // left 8 blue pixels
           cg16x8 = vaddq_s16(y11_16x8, iCBG);
           cg16x8 = vaddq_s16(cg16x8, iCRG); // left 8 green pixels
           cr16x8 = vaddq_s16(y11_16x8, iCRR); // left 8 red pixels
           u88R = vqrshrun_n_s16(cr16x8, 4); // shift right, narrow and saturate to 8-bit unsigned
           u168Temp = vshll_n_u8(u88R, 8); // place red in upper part of 16-bit words
           u88B = vqrshrun_n_s16(cb16x8, 4); // shift right, narrow and saturate to 8-bit unsigned
           u88G = vqrshrun_n_s16(cg16x8, 4); // shift right, narrow and saturate to 8-bit unsigned
           u168Temp2 = vshll_n_u8(u88G, 8); // shift green elements to top of 16-bit words
           u168Temp = vsriq_n_u16(u168Temp, u168Temp2, 5); // shift green elements right and insert red elements
           u168Temp2 = vshll_n_u8(u88B, 8); // shift blue elements to top of 16-bit words
           u168Temp = vsriq_n_u16(u168Temp, u168Temp2, 11); // shift blue elements right and inserted into bottom 5 bits
           vst1q_u16((uint16_t *)ulDest + 8 + iPitch/2, u168Temp);
           pCr += 8; pCb += 8; pY += 16; // next pair of rows
           ulDest += iPitch/2;
           if (iRow == 3) { // halfway down, switch to next 2 MCUs
               pY += 64;
           }
        } // for each row
#elif defined HAS_S3_SIMD
    pY = (int16_t *)&pMCU[MCU0];
    for (iRow=0; iRow<= iMaxRow; iRow++) {
        s3_ycbcr_convert_420(pY, pCb, (uint16_t*)ulDest, iPitch, i16_Consts, pVideo->u8PixelType);
        pCr += 8; pCb += 8; pY += 16; // next pair of rows
        ulDest += iPitch/2;
        if (iRow == 3) { // halfway down, switch to next 2 MCUs
            pY += 64;
        }
    } // for each pair of rows
#else
   for (iRow=0; iRow <= iMaxRow; iRow++) {
       pY = (int16_t *)&pMCU[MCU0 + iRowOffsets[iRow]];
       for (iCol=0; iCol<=iMaxCol; iCol++) {
           s = pY[0];
            s = u8RangeTable[s & 0x3ff];
               usIndex = (((s)>>2) & 0x3f); // Y1
               s = pCb[0];
           s = u8RangeTable[s & 0x3ff];
               usIndex |= ((((s)>>3)&0x1f)<<6);
               s = pCr[0];
           s = u8RangeTable[s & 0x3ff];
               usIndex |= ((((s)>>3)&0x1f)<<11);
               ulPixel = pVideo->usYUVRGB[usIndex];
               usIndex &= ~0x3f; // blast away Y1
               s = pY[1];
           s = u8RangeTable[s & 0x3ff];
               usIndex |= (((s)>>2) & 0x3f); // Y2
               ulPixel |= (pVideo->usYUVRGB[usIndex] << 16);
               ulDest[0] = ulPixel;
               usIndex &= ~0x3f; // blast away Y2
               s = pY[8];
           s = u8RangeTable[s & 0x3ff];
               usIndex |= (((s)>>2) & 0x3f); // Y3
               ulPixel = pVideo->usYUVRGB[usIndex];
               usIndex &= ~0x3f; // blast away Y3
               s = pY[9];
           s = u8RangeTable[s & 0x3ff];
               usIndex |= (((s)>>2) & 0x3f); // Y4
               ulPixel |= (pVideo->usYUVRGB[usIndex] << 16);
               ulDest[iPitch32] = ulPixel;
         pCb++;
         pCr++;
         pY+= 2;
          if (iCol == 3) { // need to jump to the adjacent Y block
              pY += (64-8);
          }
         ulDest++; // advance 2 pixels
         } // for each column (pixel pair)
      ulDest -= (iMaxCol+1); // pull back 16 pixels
      ulDest += iPitch32*2; // next pair of lines of dest pixels
      } // for each row
#endif
} /* H263PutMCU22() */

#define GETMOREBITS if (iBit >= 16) {iBit -= 16; ulBits <<= 16; ulBits |= MOTOSHORT(&buf[iOff]); iOff += 2;}
#define GETMOREBITS8 if (iBit >= 8) {iBit -= 8; ulBits <<= 8; ulBits |= buf[iOff++];}

int32_t H263GetMVPredictor(int x, int y, int bUnrestricted, int iMBCount, signed char *cMVArray, signed char cDelta)
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
void H263CopyMB(H263STATE *pVideo, int x, int y, int16_t *pMCU)
{
uint64_t *pS, *pD;
int16_t *pDest;
int i, cy;
const int iFrameDelta = pVideo->iFrameCX>>2;
    
   // first copy the luma component
   pDest = pVideo->pFRef[0];
   pD = (uint64_t *)&pDest[(x*16)+(y*16*pVideo->iFrameCX)];
   pS = (uint64_t *)pMCU;
   // copy top half
    for (cy=0; cy<8; cy++) {
#ifdef HAS_NEON
        uint64x2_t u64x2_0, u64x2_1;
        u64x2_0 = vld1q_u64(pS); // top left
        u64x2_1 = vld1q_u64(pS+16); // top right
        vst1q_u64(pD, u64x2_0);
        vst1q_u64(pD+2, u64x2_1);
#else
        pD[0] = pS[0]; // top left block
        pD[1] = pS[1];
        pD[2] = pS[16]; // top right block
        pD[3] = pS[17];
#endif
        pD += iFrameDelta;
        pS += 2;
    }
   pD = (uint64_t *)&pDest[(x*16)+(((y*16)+8)*pVideo->iFrameCX)];
   pS = (uint64_t *)&pMCU[2*DCTSIZE2];
   // copy bottom half
   for (cy=0; cy<8; cy++) {
#ifdef HAS_NEON
        uint64x2_t u64x2_0, u64x2_1;
        u64x2_0 = vld1q_u64(pS); // top left
        u64x2_1 = vld1q_u64(pS+16); // top right
        vst1q_u64(pD, u64x2_0);
        vst1q_u64(pD+2, u64x2_1);
#else
      pD[0] = pS[0]; // top left block
      pD[1] = pS[1];
      pD[2] = pS[16]; // top right block
      pD[3] = pS[17];
#endif
      pD += iFrameDelta;
      pS += 2;
      }
   // Now copy the 2 chroma components
   for (i=0; i<2; i++) {
      pDest = pVideo->pFRef[1+i];
      pD = (uint64_t *)&pDest[(x*8)+(y*8*(pVideo->iFrameCX>>1))];
      pS = (uint64_t *)&pMCU[(4+i)*DCTSIZE2];
#ifdef HAS_NEON
       for (cy=0; cy<8; cy+=2) {
           uint64x2_t u64x2_0, u64x2_1;
           u64x2_0 = vld1q_u64(pS);
           u64x2_1 = vld1q_u64(pS+2);
           vst1q_u64(pD, u64x2_0);
           pD += iFrameDelta>>1;
           vst1q_u64(pD, u64x2_1);
           pD += iFrameDelta>>1;
           pS += 4;
       } // for cy
#else
      for (cy=0; cy<8; cy++) {
         pD[0] = pS[0];
         pD[1] = pS[1];
         pD += iFrameDelta>>1;
         pS += 2;
         } // for cy
#endif
      } // for i
} /* H263CopyMB() */

void PrepVideoStruct(H263STATE *pVideo)
{
int i, j;

   pVideo->usYUVRGB = (uint16_t *)MALLOC(0x20000);
   pVideo->iCurrentFrame = 0;
   pVideo->iFRefFrame = -1;
    
       // prepare the color conversion table
       for (i=0; i<65536; i++) {
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
              if (pVideo->u8PixelType == H263_PIXEL_RGB565_LE) {
                  pVideo->usYUVRGB[i] = (uint16_t)iPixel;
              } else { // big endian RGB565
                  pVideo->usYUVRGB[i] = __builtin_bswap16((uint16_t)iPixel);
              }
        } // for i
} /* PrepVideoStruct() */

void H263SwapFrames(H263STATE *pVideo)
{
int16_t *pTemp;
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
static int H263_decodeFrameInternal(H263STATE *pVideo, uint8_t *pData, int iDataLen)
{
int i, x, y, iGOBy, iErr, iOff, iLen, iBit;
int iTrueWidth, iTrueHeight;
uint8_t cMask, cQuant, *buf;
uint32_t ulBits, ulCode;
uint8_t cLevel, ucCBPY, *pCBPY;
// uint8_t ucTR, ucPSBI;
int16_t *pMCU = pVideo->MCUs, us;
int iGOB, iGOBCount, iMB, iMBCount, iMBMax;
char cSourceFormat, ucMBType, ucCBPC;
uint32_t j, count, codestart, repeat, *pVLCTable;
uint32_t ulPTYPE;
int32_t iMV_X=0, iMV_Y=0;
uint16_t *pMVTable, *pMCBPCTable;
int *pClip;
uint8_t *pTables;

    if (pVideo->pACTables == NULL) {
        if (pVideo->iFramePitch <= 0) {
            pVideo->iFramePitch = pVideo->iWidth * 2; // set default pitch
        }
        PrepVideoStruct(pVideo);
        pTables = (uint8_t *) MALLOC(8192 * 2 * sizeof(uint32_t) + 128 + 8192 + 1024);
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
            uint8_t s;
            s = (uint8_t)ucMVDTab[i*3]; // code value
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
    } else { // prepare structure for first frame
        pTables = (uint8_t *)pVideo->pACTables;
        pMVTable = (uint16_t *)pTables;
        pMCBPCTable = (uint16_t *)&pTables[1024];
        pCBPY = &pTables[8192]; // put fast CBPY lookup table here
        pVLCTable = (uint32_t *)&pTables[0x2080];
        pClip = (int *)&pTables[4096]; // put clip table here
    }
    
   iErr = 0;
   iGOBCount = (pVideo->iHeight+15) / 16;
   iMBCount = (pVideo->iWidth+15) / 16;

// Decompress the current frame
    buf = pData;
   iOff = iBit = 0;  /* Pointer into data stream */
   ulBits = MOTOLONG(&buf[iOff]); // start out with 32-bits
   iOff += 4;
   ulCode = (ulBits >> 10); // get PSC (picture start code) (22 bits)
   iBit += 22;
    if (ulCode != 0x0020) { // video sequence is bogus
        pVideo->iLastError = H263_DECODE_ERROR;
      return H263_DECODE_ERROR;
    }
//   ucTR = (uint8_t) (ulBits >> (24 - iBit)); // get TR (temporal reference) (8-bits)
   iBit += 8;
   GETMOREBITS
   ulPTYPE = (ulBits >> (19-iBit)) & 0x1fff; // get PTYPE (13-bits)
    if (ulPTYPE & 0x7) {// Arithmetic, advanced prediction, PB-frame
        pVideo->iLastError = H263_NOT_SUPPORTED;
        return H263_NOT_SUPPORTED; // we can't handle it yet
    }
    cSourceFormat = (uint8_t)((ulPTYPE >> 5) & 7); // video size
    iBit += 13;
    GETMOREBITS
    if (cSourceFormat == 7) { // extended PTYPE, use the video file's size
        uint8_t plusPTYPE = (ulBits >> (29-iBit)) & 0x7; // 3 bits
        iBit += 3;
        iTrueWidth = iMBCount;
        iTrueHeight = iGOBCount;
        if (plusPTYPE != 0) {
            // 18 more bits of flags/options
            iBit += 18;
        }
        // the mandatory part of plusPTYPE (9 bits)
        iBit += 9;
        GETMOREBITS
        GETMOREBITS
    } else {
        iTrueWidth = iH263Formats[cSourceFormat*2];
        iTrueHeight = iH263Formats[cSourceFormat*2+1];
    }
    if (pVideo->iFrameCX == 0) { // need to allocate predictor pages
        x = pVideo->iFrameCX = iTrueWidth<<4;
        y = pVideo->iFrameCY = iTrueHeight<<4;
        pVideo->MCUs = (int16_t *)MALLOC_ALIGNED(6*DCTSIZE2*sizeof(uint16_t));
        pVideo->pBRef[0] = (int16_t *) MALLOC_ALIGNED(x * y * sizeof(int16_t)); // Luma prediction
        pVideo->pBRef[1] = (int16_t *) MALLOC_ALIGNED(((x * y) >> 2)*sizeof(int16_t)); // Chroma1 prediction
        pVideo->pBRef[2] = (int16_t *) MALLOC_ALIGNED(((x * y) >> 2)*sizeof(int16_t)); // Chroma2 prediction
        pVideo->pFRef[0] = (int16_t *) MALLOC_ALIGNED(x * y * sizeof(int16_t)); // Luma prediction
        pVideo->pFRef[1] = (int16_t *) MALLOC_ALIGNED(((x * y) >> 2)*sizeof(int16_t)); // Chroma1 prediction
        pVideo->pFRef[2] = (int16_t *) MALLOC_ALIGNED(((x * y) >> 2)*sizeof(int16_t)); // Chroma2 prediction
    }
   iMBMax = iTrueWidth * iTrueHeight;
   cQuant = (uint8_t)(ulBits >> (27-iBit)) & 0x1f; // get PQUANT (5-bits)
   iBit += 5;
   ulCode = (ulBits >> (31-iBit)) & 1; // get CPM (1-bit)
   iBit++;
    if (ulCode) { // if CPM bit set, PSBI bits present
//      ucPSBI = (uint8_t) ((ulBits >> (30-iBit)) & 3); // get 2 PSBI bits
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
       int bSkip = 0; // TRUE indicates that this MB is outside of the clip area
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
       // For skipping macroblack calculations, we need to include an extra border MB because motion deltas
       // can come from and go into these bordering blocks. +/-16 pixels outside of our clip rect needs to be
       // calculated to not introduce errors on the border MBs within our clip region.
       if (pVideo->clipRect.w) { // a clipping rectangle is defined
           int tx = x << 4, ty = y << 4; // convert to pixels
           bSkip = (tx < pVideo->clipRect.x-16 || tx >= (16 + pVideo->clipRect.x + pVideo->clipRect.w) || ty < pVideo->clipRect.y-16 || ty >= (16 + pVideo->clipRect.y + pVideo->clipRect.h));
       }
       if (ulPTYPE & 0x10) { // an INTER block has COD (coded macroblock indication)
         ulCode = (ulBits >> (31-iBit)) & 1; // 1 bit COD
         iBit++;
           if (ulCode) { // this MB is NOT coded, skip it
               if (!bSkip) {
                   iMV_X = iMV_Y = 0; // skipped blocks have a MV of 0,0
                   memset(pMCU, 0, DCTSIZE2*6*sizeof(int16_t));
                   H263MotComp(x, y, iMV_X, iMV_Y, pMCU, pVideo, 0);
                   H263CopyMB(pVideo, x, y, pMCU); // copy the MB to our prediction image
               }
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
       pMCU = pVideo->MCUs;
      memset(pMCU, 0, DCTSIZE2*6*sizeof(int16_t)); // clear this MB to start
      ucCBPY <<= 2;
      ucCBPY |= ucCBPC; // combine bits of Y with CbCr bits
      cMask = 32;
       for (i=0; i<6 && !iErr; i++) { // Get the 6 blocks comprising the macroblock
         iErr = GetH263MCU(pVLCTable, buf, &pMCU[i*DCTSIZE2], &iOff, &iBit, pVideo, cQuant, ucCBPY & cMask, ucMBType);
           if (!bSkip) {
               if (ucCBPY & cMask) {
                   H263IDCT(&pMCU[i*DCTSIZE2]);
               } else if (ucMBType >= 3) { // only have a DC value, distribute it within the block
                   us = pMCU[i*DCTSIZE2 + 0] >> 3; // Get the adjusted DC value
                   for (j=0; j<64; j++)
                       pMCU[i*DCTSIZE2 + j] = us; // store in all cells
               }
           }
         cMask >>= 1;
         } // for each of the 6 blocks
      ulBits = pVideo->ulBits; // get the bits back
// only draw the visible parts of the frame
       if (!bSkip) {
           if (ucMBType < 3) { // INTER MB
               // predict block with motion compensation
               H263MotComp(x, y, iMV_X, iMV_Y, pMCU, pVideo, 0);
           }
           H263CopyMB(pVideo, x, y, pMCU); // copy the MB to our prediction image
           if (x < iMBCount && y < iGOBCount) {
               H263PutMCU22(pVideo, x, y, pMCU, -16, -128); // lay down MCU in output image
           }
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
uint8_t *start_buf = buf;
int iRun, iIndex, iErr;
int32_t iLevel;
uint32_t ulBits, ulCode, ulVal;
int bLast;
uint8_t ucZig;
uint8_t *pZig;
const uint8_t *pZigEnd = &cZigZag2[64];
    
    buf += *iOffset; // fold 2 variables into 1 to speed up this function
    iErr = H263_SUCCESS;
    ulBits = pVideo->ulBits;
    if (iBit >= 16) { // make sure we have enough bits to start
        iBit -= 16;
        ulBits <<= 16;
        ulBits |= MOTOSHORT(buf);
        buf += 2;
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
    pZig = (uint8_t *)&cZigZag2[iIndex];
    
    if (bTCOEF) { // if coefficients coded for this block
      while (!bLast && pZig < pZigEnd) {
          if (iBit >= 16) {
              iBit -= 16;
              ulBits <<= 16;
              ulBits |= MOTOSHORT(buf);
              buf += 2;
          }
          ulCode = (ulBits >> (18-iBit)) & 0x3ffe; // get 13-bits, shifted left by 1 to index shorts
          ulVal = pTable[ulCode]; // get the bit value
          if (ulVal == 0) { // invalid code
              iErr = H263_DECODE_ERROR;
          }
          iBit += pTable[(ulCode & 0x3ffc)+1]; //(ulCode & 0x1ffe)*2 + 1]; // get the true length
          if (ulVal == 0xffffffff) { // special ESCAPE code
              if (iBit >= 16) { // we need 15 bits
                  iBit -= 16;
                  ulBits <<= 16;
                  ulBits |= MOTOSHORT(buf);
                  buf += 2;
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
         pZig += iRun; // skip value
          ucZig = *pZig++; // get the zigzag index here to avoid a pipeline stall
          if (iLevel < 0) {
              iLevel = (iQuant * (2 * iLevel - 1)) + (iQuant & 1);
          } else {
              iLevel = (iQuant * (2 * iLevel + 1)) - (iQuant & 1);
          }
         pMCU[ucZig] = (int16_t)iLevel; // store the coeff
         }
      }
    if (pZig > pZigEnd) { // error
        iErr = H263_DECODE_ERROR;
    }
   *iBitnum = iBit;
   *iOffset = (int)(buf - start_buf);
   pVideo->ulBits = ulBits;
   return iErr;
} /* GetH263MCU() */

#define MB_LOWER -2048
#define MB_UPPER 2047

#ifdef HAS_NEON
void neon_simd_mb(int iType, int16_t *pS, int16_t *pD, int iPitch)
{
    int cy;
    
    switch (iType) {
       case 0: // full pel in both dirs
           {
               int16x8_t s16x8_0, s16x8_1, d16x8_0, d16x8_1;
               const int16x8_t upper16x8 = vdupq_n_s16(MB_UPPER);
               const int16x8_t lower16x8 = vdupq_n_s16(MB_LOWER);
               for (cy = 0; cy < 8; cy+=2) {
                   s16x8_0 = vld1q_s16(pS);
                   d16x8_0 = vld1q_s16(pD);
                   s16x8_1 = vld1q_s16(pS+iPitch);
                   d16x8_1 = vld1q_s16(pD+8);
                   d16x8_0 = vaddq_s16(d16x8_0, s16x8_0);
                   d16x8_1 = vaddq_s16(d16x8_1, s16x8_1);
                   d16x8_0 = vminq_s16(d16x8_0, upper16x8);
                   d16x8_1 = vminq_s16(d16x8_1, upper16x8);
                   d16x8_0 = vmaxq_s16(d16x8_0, lower16x8);
                   d16x8_1 = vmaxq_s16(d16x8_1, lower16x8);
                   vst1q_s16(pD, d16x8_0);
                   vst1q_s16(pD+8, d16x8_1);
                   pD += 16;
                   pS += iPitch*2;
               } // for cy
           }
          break;
       case 1: // full pel Y, half-pel X
           {
               int16x8_t s16x8_0, s16x8_1, d16x8;
               const int16x8_t upper16x8 = vdupq_n_s16(MB_UPPER);
               const int16x8_t lower16x8 = vdupq_n_s16(MB_LOWER);
               for (cy = 0; cy < 8; cy++) {
                   s16x8_0 = vld1q_s16(pS);
                   s16x8_1 = vld1q_s16(pS+1);
                   d16x8 = vld1q_s16(pD);
                   s16x8_0 = vaddq_s16(s16x8_0, s16x8_1); // horizontal sum
                   s16x8_0 = vrshrq_n_s16(s16x8_0, 1); // average with rounding up
                   d16x8 = vaddq_s16(d16x8, s16x8_0);
                   d16x8 = vminq_s16(d16x8, upper16x8);
                   d16x8 = vmaxq_s16(d16x8, lower16x8);
                   vst1q_s16(pD, d16x8);
                   pD += 8;
                   pS += iPitch;
               } // for cy
           }
          break;
       case 2: // half pel Y, full pel X
           {
               int16x8_t s16x8_0, s16x8_1, d16x8;
               const int16x8_t upper16x8 = vdupq_n_s16(MB_UPPER);
               const int16x8_t lower16x8 = vdupq_n_s16(MB_LOWER);
               for (cy = 0; cy < 8; cy++) {
                   s16x8_0 = vld1q_s16(pS);
                   s16x8_1 = vld1q_s16(pS+iPitch);
                   d16x8 = vld1q_s16(pD);
                   s16x8_0 = vaddq_s16(s16x8_0, s16x8_1); // vertical sum
                   d16x8 = vrsraq_n_s16(d16x8, s16x8_0, 1); // average with rounding up
                   d16x8 = vminq_s16(d16x8, upper16x8);
                   d16x8 = vmaxq_s16(d16x8, lower16x8);
                   vst1q_s16(pD, d16x8);
                   pD += 8;
                   pS += iPitch;
               } // for cy
           }
          break;
       case 3: // half pel Y, half pel X
           {
               int16x8_t s16x8_00, s16x8_10, s16x8_01, s16x8_11, d16x8;
               const int16x8_t upper16x8 = vdupq_n_s16(MB_UPPER);
               const int16x8_t lower16x8 = vdupq_n_s16(MB_LOWER);
               for (cy = 0; cy < 8; cy++) {
                   d16x8 = vld1q_s16(pD);
                   s16x8_00 = vld1q_s16(pS);
                   s16x8_10 = vld1q_s16(pS+1);
                   s16x8_01 = vld1q_s16(pS+iPitch);
                   s16x8_11 = vld1q_s16(pS+1+iPitch);
                   s16x8_00 = vaddq_s16(s16x8_00, s16x8_10); // add horizontally
                   s16x8_01 = vaddq_s16(s16x8_01, s16x8_11);
                   s16x8_00 = vaddq_s16(s16x8_00, s16x8_01); // add vertically
                   d16x8 = vrsraq_n_s16(d16x8, s16x8_00, 2); // average all together
                   d16x8 = vminq_s16(d16x8, upper16x8);
                   d16x8 = vmaxq_s16(d16x8, lower16x8);
                   vst1q_s16(pD, d16x8);
                   pD += 8;
                   pS += iPitch;
               } // for cy
           }
          break;
       } // switch on MV type
} /* neon_simd_mb() */
#endif // HAS_NEON

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : H263MotComp(int, int, int, int, short *, MPEGDATA, PILBOOL)   *
 *                                                                          *
 *  PURPOSE    : Apply motion compensation to predicted MacroBlock.         *
 *                                                                          *
 ****************************************************************************/
void H263MotComp(int x, int y, int32_t iMV_X, int32_t iMV_Y, int16_t *pMCUDest, H263STATE *pVideo, int bBackward)
{
int16_t *pS, *pD;
int32_t i, dx, dy;
int iType, iPitch;
#if !defined(HAS_NEON) && !defined(HAS_S3_SIMD)
    int cx, cy;
    int16_t s;
#endif
    
   // Do the luma blocks (0-3), followed by the chroma blocks (4-5)
   for (i=0; i<6; i++) {
       pD = &pMCUDest[i*DCTSIZE2];
       if (i < 4) {
           iPitch = pVideo->iFrameCX;
           // Determine the type of pixel motion
           iType = 0;
           if (iMV_X & 1) // half-pel on X
               iType++;
           if (iMV_Y & 1) // half-pel on Y
               iType+=2;
           dx = (iMV_X >> 1); // whole pel offsets
           dy = (iMV_Y >> 1);
           if (bBackward) {
               pS = pVideo->pFRef[0];
           } else {
               pS = pVideo->pBRef[0];
           }
           pS += (x*16)+ dx + ((i&1)<<3); // horiz address
           pS += ((y*16) + dy + ((i&2)<<2)) * iPitch;
       } else { // chroma
           iPitch = pVideo->iFrameCX/2;
           // Determine the type of pixel capture
           iType = 0;
           if (iMV_X & 3) // half-pel on X
              iType++;
           if (iMV_Y & 3) // half-pel on Y
              iType+=2;
           dx = (iMV_X >> 2); // whole pel offsets
           dy = (iMV_Y >> 2);
           if (bBackward) {
               pS = pVideo->pFRef[i-3];
           } else {
               pS = pVideo->pBRef[i-3];
           }
           pS += x*8 + dx; // horiz address
           pS += ((y*8) + dy) * iPitch;
       }
#ifdef HAS_S3_SIMD
       s3_simd_mb(iType, pS, pD, iPitch*2, s3_mb_constants);
#elif defined( HAS_NEON )
       neon_simd_mb(iType, pS, pD, iPitch);
#else // generic C solution
         switch (iType) {
            case 0: // full pel in both dirs
               for (cy=0; cy<8; cy++) {
                  for (cx=0; cx<8; cx++) {
                     pD[cx] += pS[cx];
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     else if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                  } // for cx
                  pD += 8;
                  pS += iPitch; // next line
               } // for cy
               break;
            case 1: // full pel Y, half-pel X
               for (cy=0; cy<8; cy++) {
                  for (cx=0; cx<8; cx++) {
                     pD[cx] += ((pS[cx] + pS[cx + 1] + 1)>>1);  // avg left/right pixels
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     else if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                     } // for cx
                  pD += 8;
                  pS += iPitch; // next line
                  } // for cy
               break;
            case 2: // half pel Y, full pel X
               for (cy=0; cy<8; cy++) {
                  for (cx=0; cx<8; cx++) {
                     pD[cx] += ((pS[cx] + pS[cx + iPitch] + 1)>>1);  // avg left/right pixels
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     else if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                     } // for cx
                  pD += 8;
                  pS += iPitch; // next line
                  } // for cy
               break;
            case 3: // half pel Y, half pel X
               for (cy=0; cy<8; cy++) {
                  for (cx=0; cx<8; cx++) {
                     s = pS[cx] + pS[cx + 1]; // top 2
                     s += pS[cx + iPitch] + pS[cx + 1 + iPitch]; // bottom 2
                     pD[cx] += ((s + 2)>>2);  // avg the 4 pixel group
                     if (pD[cx] > MB_UPPER) pD[cx] = MB_UPPER;
                     else if (pD[cx] < MB_LOWER) pD[cx] = MB_LOWER;
                     } // for cx
                  pD += 8;
                  pS += iPitch; // next line
                  } // for cy
               break;
            } // switch on MV type
#endif // SIMD vs C
      } // for each of the 6 Y/Cb/Cr blocks
} /* H263MotComp() */

#endif // __BB_H263__
