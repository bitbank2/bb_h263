//
//  main.cpp
//  bb_h263 functional tests
//
//  Created by Laurence Bank on 9/14/26
//

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#define __LINUX__
#include "../../../src/bb_h263.h"
BB_H263 h263;

/* Windows BMP header for RGB565 images */
uint8_t winbmphdr_rgb565[138] =
        {0x42,0x4d,0,0,0,0,0,0,0,0,0x8a,0,0,0,0x7c,0,
         0,0,0,0,0,0,0,0,0,0,1,0,8,0,3,0,
         0,0,0,0,0,0,0x13,0x0b,0,0,0x13,0x0b,0,0,0,0,
         0,0,0,0,0,0,0,0xf8,0,0,0xe0,0x07,0,0,0x1f,0,
         0,0,0,0,0,0,0x42,0x47,0x52,0x73,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,
         0,0,0,0,0,0,0,0,0,0};

/* Windows BMP header for 8/24/32-bit images (54 bytes) */
uint8_t winbmphdr[54] =
        {0x42,0x4d,
         0,0,0,0,         /* File size */
         0,0,0,0,0x36,4,0,0,0x28,0,0,0,
         0,0,0,0, /* Xsize */
         0,0,0,0, /* Ysize */
         1,0,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,       /* number of planes, bits per pel */
         0,0,0,0};
//
// Read a Windows BMP file into memory
// For this demo, the only supported files are 24 or 32-bits per pixel
//
uint8_t * ReadBMP(const char *fname, int *width, int *height, int *bpp, unsigned char *pPal)
{
    int y, w, h, bits, offset;
    uint8_t *s, *d, *pTemp, *pBitmap;
    int pitch, bytewidth;
    int iSize, iDelta;
    FILE *infile;
    
    infile = fopen(fname, "r+b");
    if (infile == NULL) {
        printf("Error opening input file %s\n", fname);
        return NULL;
    }
    // Read the bitmap into RAM
    fseek(infile, 0, SEEK_END);
    iSize = (int)ftell(infile);
    fseek(infile, 0, SEEK_SET);
    pBitmap = (uint8_t *)malloc(iSize);
    pTemp = (uint8_t *)malloc(iSize);
    fread(pTemp, 1, iSize, infile);
    fclose(infile);
    
    if (pTemp[0] != 'B' || pTemp[1] != 'M' || pTemp[14] < 0x28) {
        free(pBitmap);
        free(pTemp);
        printf("Not a Windows BMP file!\n");
        return NULL;
    }
    w = *(int32_t *)&pTemp[18];
    h = *(int32_t *)&pTemp[22];
    bits = *(int16_t *)&pTemp[26] * *(int16_t *)&pTemp[28];
    if (bits <= 8) { // it has a palette, copy it
        uint8_t *p = pPal;
        for (int i=0; i<(1<<bits); i++)
        {
           *p++ = pTemp[54+i*4];
           *p++ = pTemp[55+i*4];
           *p++ = pTemp[56+i*4];
        }
    }
    offset = *(int32_t *)&pTemp[10]; // offset to bits
    bytewidth = (w * bits) >> 3;
    pitch = (bytewidth + 3) & 0xfffc; // DWORD aligned
// move up the pixels
    d = pBitmap;
    s = &pTemp[offset];
    iDelta = pitch;
    if (h > 0) {
        iDelta = -pitch;
        s = &pTemp[offset + (h-1) * pitch];
    } else {
        h = -h;
    }
    for (y=0; y<h; y++) {
        if (bits == 32) {// need to swap red and blue
            for (int i=0; i<bytewidth; i+=4) {
                d[i] = s[i+2];
                d[i+1] = s[i+1];
                d[i+2] = s[i];
                d[i+3] = s[i+3];
            }
        } else {
            memcpy(d, s, bytewidth);
        }
        d += bytewidth;
        s += iDelta;
    }
    *width = w;
    *height = h;
    *bpp = bits;
    free(pTemp);
    return pBitmap;
    
} /* ReadBMP() */

//
// Minimal code to save frames as Windows BMP files
//
void WriteBMP(char *fname, uint8_t *pBitmap, uint8_t *pPalette, int cx, int cy, int bpp)
{
FILE * oHandle;
int i, bsize, lsize;
uint32_t *l;
uint8_t *s;
uint8_t *ucTemp;
uint8_t *pHdr;
int iHeaderSize;

    ucTemp = (uint8_t *)malloc(cx * 4);

    if (bpp == 16) {
        pHdr = winbmphdr_rgb565;
        iHeaderSize = sizeof(winbmphdr_rgb565);
    } else {
        pHdr = winbmphdr;
        iHeaderSize = sizeof(winbmphdr);
    }
    
    oHandle = fopen(fname, "w+b");
    bsize = (cx * bpp) >> 3;
    lsize = (bsize + 3) & 0xfffc; /* Width of each line */
    pHdr[26] = 1; // number of planes
    pHdr[28] = (uint8_t)bpp;

   /* Write the BMP header */
   l = (uint32_t *)&pHdr[2];
    i =(cy * lsize) + iHeaderSize;
    if (bpp <= 8)
        i += 1024;
   *l = (uint32_t)i; /* Store the file size */
   l = (uint32_t *)&pHdr[34]; // data size
   i = (cy * lsize);
   *l = (uint32_t)i; // store data size
   l = (uint32_t *)&pHdr[18];
   *l = (uint32_t)cx;      /* width */
   *(l+1) = (uint32_t)cy;  /* height */
    l = (uint32_t *)&pHdr[10]; // OFFBITS
    if (bpp <= 8) {
        *l = iHeaderSize + 1024;
    } else { // no palette
        *l = iHeaderSize;
    }
   fwrite(pHdr, 1, iHeaderSize, oHandle);
    if (bpp <= 8) {
    if (pPalette == NULL) {// create a grayscale palette
        int iDelta, iCount = 1<<bpp;
        int iGray = 0;
        iDelta = 255/(iCount-1);
        for (i=0; i<iCount; i++) {
            ucTemp[i*4+0] = (uint8_t)iGray;
            ucTemp[i*4+1] = (uint8_t)iGray;
            ucTemp[i*4+2] = (uint8_t)iGray;
            ucTemp[i*4+3] = 0;
            iGray += iDelta;
        }
    } else {
        for (i=0; i<256; i++) // change palette to WinBMP format
        {
            ucTemp[i*4 + 0] = pPalette[(i*3)+2];
            ucTemp[i*4 + 1] = pPalette[(i*3)+1];
            ucTemp[i*4 + 2] = pPalette[(i*3)+0];
            ucTemp[i*4 + 3] = 0;
        }
    }
    fwrite(ucTemp, 1, 1024, oHandle);
    } // palette write
   /* Write the image data */
   for (i=cy-1; i>=0; i--)
    {
        s = &pBitmap[i*bsize];
        if (bpp == 24) { // swap R/B for Windows BMP byte order
            int j, iBpp = bpp/8;
            uint8_t *d = ucTemp;
            for (j=0; j<cx; j++) {
                d[0] = s[2]; d[1] = s[1]; d[2] = s[0];
                d += iBpp; s += iBpp;
            }
            fwrite(ucTemp, 1, (size_t)lsize, oHandle);
        } else {
            fwrite(s, 1, (size_t)lsize, oHandle);
        }
    }
    free(ucTemp);
    fclose(oHandle);
} /* WriteBMP() */

//
// Return the current time in microseconds
//
int Micros(void)
{
int iTime;
struct timespec res;

    clock_gettime(CLOCK_MONOTONIC, &res);
    iTime = (int)(1000000*res.tv_sec + res.tv_nsec/1000);

    return iTime;
} /* Micros() */

uint8_t *LoadFile(const char *fname, int *pSize)
{
FILE *f;
int iSize;
uint8_t *p;
    
    f = fopen(fname, "r+b");
    if (f == NULL) {
        return NULL;
    }
    // Read the bitmap into RAM
    fseek(f, 0, SEEK_END);
    iSize = (int)ftell(f);
    fseek(f, 0, SEEK_SET);
    p = (uint8_t *)malloc(iSize);
    fread(p, 1, iSize, f);
    fclose(f);
    *pSize = iSize;
    return p;
} /* LoadFile() */
//
// Simple logging print
//
void H263LOG(int line, char *string, const char *result)
{
    printf("Line: %d: msg: %s%s\n", line, string, result);
} /* H263LOG() */

int main(int argc, const char * argv[]) {
    int i, rc, w, h, bpp, iTotal;
    uint8_t *pCompare;
    uint8_t *pFuzzData;
    char *szTestName;
    int iFrames, iTotalPass, iTotalFail;
    int iFileSize;
    const char *szStart = " - START";

    iTotalPass = iTotalFail = iTotal = 0;

    // Test 0 - Correct file open
    iTotal++;
    szTestName = (char *)"File open, get info";
    H263LOG(__LINE__, szTestName, szStart);
    rc = h263.open("../../../sample_videos/matrix_h263.mov");
    if (rc == H263_SUCCESS) {
        w = h263.getWidth();
        h = h263.getHeight();
        iFrames = h263.getFrameCount();
     //   printf("w=%d, h=%d, f=%d\n", w, h, iFrames);
        if (w == 352 && h == 288 && iFrames == 4351) {
            iTotalPass++;
            H263LOG(__LINE__, szTestName, " - PASSED");
        } else {
            iTotalFail++;
            H263LOG(__LINE__, szTestName, " - FAILED");
        }
        h263.close();
    } else {
        H263LOG(__LINE__, szTestName, "Error opening movie file.");
        iTotalFail++;
        H263LOG(__LINE__, szTestName, " - FAILED");
    }

    // Test 1 - Invalid file
    iTotal++;
    szTestName = (char *)"Invalid file";
    H263LOG(__LINE__, szTestName, szStart);
    rc = h263.open("../../../src/bb_h263.h");
    if (rc == H263_INVALID_FILE) {
        iTotalPass++;
        H263LOG(__LINE__, szTestName, " - PASSED");
    } else {
        iTotalFail++;
        printf("rc=%d\n", rc);
        H263LOG(__LINE__, szTestName, " - FAILED");
    }
    // Test 2 - Verify error when asked to decode a frame with no buffer
    szTestName = (char *)"Missing framebuffer";
    iTotal++;
    H263LOG(__LINE__, szTestName, szStart);
    rc = h263.open("../../../sample_videos/matrix_h263.mov");
    if (rc == H263_SUCCESS) {
        rc = h263.decodeFrame();
        if (rc == H263_NO_FRAMEBUFFER) {
            iTotalPass++;
            H263LOG(__LINE__, szTestName, " - PASSED");
        } else {
            iTotalFail++;
            H263LOG(__LINE__, szTestName, " - FAILED");
        }
        h263.close();
    } else {
        H263LOG(__LINE__, szTestName, "Error opening movie file.");
        iTotalFail++;
        H263LOG(__LINE__, szTestName, " - FAILED");
    }
    // Test 3 - frame counter advance
    szTestName = (char *)"Frame counter advance";
    iTotal++;
    H263LOG(__LINE__, szTestName, szStart);
    rc = h263.open("../../../sample_videos/matrix_h263.mov");
    if (rc == H263_SUCCESS) {
        rc = h263.allocFramebuffer();
        for (i=0; i<10 && rc == H263_SUCCESS; i++) {
            rc = h263.decodeFrame();
        } // for i
        if (rc == H263_SUCCESS && i == h263.getCurrentFrame()) {
            iTotalPass++;
            H263LOG(__LINE__, szTestName, " - PASSED");
        } else {
//            printf("frame count = %d\n", h263.getCurrentFrame());
            iTotalFail++;
            H263LOG(__LINE__, szTestName, " - FAILED");
        }
        h263.freeFramebuffer();
        h263.close();
    } else {
        H263LOG(__LINE__, szTestName, "Error opening movie file.");
        iTotalFail++;
        H263LOG(__LINE__, szTestName, " - FAILED");
    }
    // Test 4 - Verify correct decoding
    szTestName = (char *)"Verify correct decoding";
    iTotal++;
    H263LOG(__LINE__, szTestName, szStart);
    rc = h263.open("../../../sample_videos/matrix_h263.mov");
    if (rc == H263_SUCCESS) {
        h263.setPixelType(H263_PIXEL_RGB565_LE);
        rc = h263.allocFramebuffer();
        for (i=0; i<15 && rc == H263_SUCCESS; i++) {
            rc = h263.decodeFrame();
        } // for i
        if (rc == H263_SUCCESS) {
//            WriteBMP("./frame_compare.bmp", h263.getFramebuffer(), NULL, h263.getWidth(), h263.getHeight(), 16);
            pCompare = ReadBMP("./frame_compare.bmp", &w, &h, &bpp, NULL);
            // Compare correct output to the current frame
            if (pCompare && memcmp(pCompare, h263.getFramebuffer(), h * w * 2) == 0) {
                iTotalPass++;
                H263LOG(__LINE__, szTestName, " - PASSED");
            } else {
                iTotalFail++;
//                uint8_t *s, *d;
//                s = pCompare;
//                d = h263.getFramebuffer();
//                for (i=0; i<h*w*2; i++) {
//                    if (s[i] != d[i]) {
//                        printf("difference found at offset %d, original: 0x%02x incorrect: 0x%02x\n", i, s[i], d[i]);
//                    }
//                }
                H263LOG(__LINE__, szTestName, " - FAILED");
            }
        } else {
            iTotalFail++;
            H263LOG(__LINE__, szTestName, " - FAILED");
        }
        h263.freeFramebuffer();
        h263.close();
    } else {
        H263LOG(__LINE__, szTestName, "Error opening movie file.");
        iTotalFail++;
        H263LOG(__LINE__, szTestName, " - FAILED");
    }
    // Test 5 - Check clipping rectangle out of bounds
    szTestName = (char *)"Check clipping rectangle out of bounds";
    iTotal++;
    H263LOG(__LINE__, szTestName, szStart);
    rc = h263.open("../../../sample_videos/matrix_h263.mov");
    if (rc == H263_SUCCESS) {
        BB_RECT bbr;
        bbr.x = bbr.y = 0;
        bbr.w = h263.getWidth() * 2; // beyond right edge
        bbr.h = h263.getHeight();
        rc = h263.setClipRect(&bbr);
        if (rc == H263_INVALID_PARAMETER) {
            iTotalPass++;
            H263LOG(__LINE__, szTestName, " - PASSED");
        } else {
            iTotalFail++;
            H263LOG(__LINE__, szTestName, " - FAILED");
        }
        h263.close();
    } else {
        H263LOG(__LINE__, szTestName, "Error opening movie file.");
        iTotalFail++;
        H263LOG(__LINE__, szTestName, " - FAILED");
    }
    // Test 6 - Check clipping rectangle snaps to macroblocks
    szTestName = (char *)"Check clipping rectangle snaps to macroblocks";
    iTotal++;
    H263LOG(__LINE__, szTestName, szStart);
    rc = h263.open("../../../sample_videos/matrix_h263.mov");
    if (rc == H263_SUCCESS) {
        BB_RECT bbr;
        bbr.x = bbr.y = 3;
        bbr.w = h263.getWidth() - 21;
        bbr.h = h263.getHeight() - 4;
        rc = h263.setClipRect(&bbr);
        if (rc == H263_SUCCESS) {
            h263.getClipRect(&bbr);
            //printf("x:%d y:%d w:%d h:%d\n", bbr.x, bbr.y, bbr.w, bbr.h);
            if (bbr.x == 0 && bbr.y == 0 && bbr.h == h263.getHeight() && bbr.w == h263.getWidth() - 16) {
                iTotalPass++;
                H263LOG(__LINE__, szTestName, " - PASSED");
            } else {
                iTotalFail++;
                H263LOG(__LINE__, szTestName, " - FAILED");
            }
        } else {
            iTotalFail++;
            H263LOG(__LINE__, szTestName, " - FAILED");
        }
        h263.close();
    } else {
        H263LOG(__LINE__, szTestName, "Error opening movie file.");
        iTotalFail++;
        H263LOG(__LINE__, szTestName, " - FAILED");
    }
    // Test 7 - Detect AVI video file type
    iTotal++;
    szTestName = (char *)"Detect AVI video file type";
    H263LOG(__LINE__, szTestName, szStart);
    rc = h263.open("../../../sample_videos/homer_car_h263.avi");
    if (rc == H263_SUCCESS && h263.getFiletype() == H263_FILE_AVI) {
        iTotalPass++;
        H263LOG(__LINE__, szTestName, " - PASSED");
    } else {
        iTotalFail++;
        H263LOG(__LINE__, szTestName, " - FAILED");
    }
    h263.close();
    // Test 8 - Decoding past end of file
    iTotal++;
    szTestName = (char *)"Decoding past end of file";
    H263LOG(__LINE__, szTestName, szStart);
    rc = h263.open("../../../sample_videos/matrix_h263.mov");
    if (rc == H263_SUCCESS) {
        rc = h263.allocFramebuffer();
//        printf("count = %d\n", h263.getFrameCount());
        for (i=0; i<h263.getFrameCount() && rc == H263_SUCCESS; i++) {
            rc = h263.decodeFrame();
        }
//        printf("i=%d\n", i);
        if (i == h263.getFrameCount() && rc == H263_LAST_FRAME) {
            iTotalPass++;
            H263LOG(__LINE__, szTestName, " - PASSED");
        } else {
            iTotalFail++;
            H263LOG(__LINE__, szTestName, " - FAILED");
        }
    } else {
        H263LOG(__LINE__, szTestName, "Error opening movie file.");
        iTotalFail++;
        H263LOG(__LINE__, szTestName, " - FAILED");
    }
    h263.close();
    // FUZZ testing
    // Randomize the input data (file header and compressed data) and confirm that the library
    // returns an error code instead of causing an invalid pointer exception
    printf("Begin fuzz testing...\n");
    szTestName = (char *)"Single Byte Sequential Corruption Test";
    iTotal++;
    pFuzzData = LoadFile("../../../sample_videos/matrix_h263.mov", &iFileSize);
    if (pFuzzData) {
        H263LOG(__LINE__, szTestName, szStart);
        // Since the file is quite large and going through all 17MB would take a looong time, we can safely
        // try to corrupt the header and first frame, followed by the indices at the end of the file.
        for (i=0; i<32768; i++) { // corrupt each byte one at a time by inverting it
            uint8_t c = pFuzzData[i]; // keep copy of the byte we changed
            pFuzzData[i] = ~pFuzzData[i]; // invert the bits of this byte
            if (h263.open(pFuzzData, iFileSize) == H263_SUCCESS) { // the header may be rejected
                rc = h263.allocFramebuffer();
                if (rc == H263_SUCCESS) {
                    rc = h263.decodeFrame(); // try to decode the first frame
                    h263.freeFramebuffer();
                }
                h263.close();
            }
            pFuzzData[i] = c; // restore the byte we changed
        } // for each test
        for (i=iFileSize - 32768; i<iFileSize; i++) { // corrupt each byte one at a time by inverting it
            uint8_t c = pFuzzData[i];
            pFuzzData[i] = ~pFuzzData[i]; // invert the bits of this byte
            if (h263.open(pFuzzData, iFileSize) == H263_SUCCESS) { // the header may be rejected
                rc = h263.allocFramebuffer();
                if (rc == H263_SUCCESS) {
                    rc = h263.decodeFrame(); // try to decode the first frame
                    h263.freeFramebuffer();
                }
                h263.close();
            }
            pFuzzData[i] = c; // restore byte we changed
        } // for each test
        H263LOG(__LINE__, szTestName, " - PASSED");
        iTotalPass++;
        // Fuzz test part 2 - multi-byte random corruption
        szTestName = (char *)"Multi-Byte Random Corruption Test";
        iTotal++;
        H263LOG(__LINE__, szTestName, szStart);
        for (i=0; i<1000; i++) { // 1000 iterations of random spots in the file to corrupt with random values
            int iOffset1, iOffset2;
            uint8_t a, b;
            iOffset1 = rand() % iFileSize;
            a = pFuzzData[iOffset1]; // save old byte
            pFuzzData[iOffset1] = (uint8_t)rand();
            iOffset2 = rand() % iFileSize; // corrupt 2 spots just for good measure
            b = pFuzzData[iOffset2];
            pFuzzData[iOffset2] = (uint8_t)rand();
            if (h263.open(pFuzzData, iFileSize) == H263_SUCCESS) { // the header may be rejected
                rc = h263.allocFramebuffer();
                if (rc == H263_SUCCESS) {
                    rc = h263.decodeFrame(); // try to decode the first frame
                    h263.freeFramebuffer();
                }
                h263.close();
            }
            pFuzzData[iOffset1] = a; // restore the 2 bytes we changed
            pFuzzData[iOffset2] = b;
        } // for each test
        H263LOG(__LINE__, szTestName, " - PASSED");
        iTotalPass++;
        free(pFuzzData);
    } else {
        printf("Error opening file for fuzz testing\n");
    }
    printf("Total tests: %d, %d passed, %d failed\n", iTotal, iTotalPass, iTotalFail);
    return 0;
} /* main() */
