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

//
// Simple logging print
//
void H263LOG(int line, char *string, const char *result)
{
    printf("Line: %d: msg: %s%s\n", line, string, result);
} /* H263LOG() */

int main(int argc, const char * argv[]) {
    int i, rc, w, h, iTotal, iTime1, iTime2;
    uint8_t *pFuzzData;
    char *szTestName;
    int iTotalPass, iTotalFail;
    uint8_t *pFrameBuffer, c1, c2;
    uint32_t pal1;
    uint16_t pal2;
    const char *szStart = " - START";

    iTotalPass = iTotalFail = iTotal = 0;

    // Test 0 - Correct file read continuation
    iTotal++;
    szTestName = (char *)"Test Continuation";
    H263LOG(__LINE__, szTestName, szStart);
    rc = h263.open("../../../sample_videos/matrix_h263.mov");
    if (rc == H263_SUCCESS) {
        w = h263.getWidth();
        h = h263.getHeight();
	rc = h263.allocFramebuffer();
	if (rc == H263_SUCCESS) {
            if (h263.decodeFrame() == H263_SUCCESS) {
                iTotalPass++;
                H263LOG(__LINE__, szTestName, " - PASSED");
            } else {
                iTotalFail++;
                H263LOG(__LINE__, szTestName, " - FAILED");
            }
        } else {
            H263LOG(__LINE__, szTestName, "Error allocating framebuffer");
            iTotalFail++;
            H263LOG(__LINE__, szTestName, " - FAILED");
        }
	h263.close();
	h263.freeFramebuffer();
    } else {
        H263LOG(__LINE__, szTestName, "Error opening movie file.");
        iTotalFail++;
        H263LOG(__LINE__, szTestName, " - FAILED");
    }

#ifdef FUTURE
    // Test 1 - Test User pointer";
    iTotal++;
    szTestName = (char *)"Test User pointer";
    PNGLOG(__LINE__, szTestName, szStart);
    rc = png.openFLASH((uint8_t *)octocat_8bpp, sizeof(octocat_8bpp), PNGDraw);
    if (rc == PNG_SUCCESS) {
        w = png.getWidth();
        h = png.getHeight();
        priv.xoff = w/2; // arbitrary values to see if they get passed properly
        priv.yoff = h/2;
        xoff = yoff = 0;
        rc = png.decode(&priv, 0);
        if (xoff == priv.xoff && yoff == priv.yoff) {
            iTotalPass++;
            PNGLOG(__LINE__, szTestName, " - PASSED");
        } else {
            iTotalFail++;
            PNGLOG(__LINE__, szTestName, " - FAILED");
        }
    } else {
        PNGLOG(__LINE__, szTestName, "Error opening PNG file.");
    }
    // Test 2 - Verify bad parameters return an error
    szTestName = (char *)"PNG bad parameter test 1";
    iTotal++;
    PNGLOG(__LINE__, szTestName, szStart);
    png.openFLASH((uint8_t *)octocat_8bpp, sizeof(octocat_8bpp), NULL);
    // no PNGDraw callback and no framebuffer = invalid
    png.decode(NULL, 0);
    if (png.getLastError() == PNG_NO_BUFFER) {
        iTotalPass++;
        PNGLOG(__LINE__, szTestName, " - PASSED");
    } else {
        iTotalFail++;
        PNGLOG(__LINE__, szTestName, " - FAILED");
    }
    // Test 3 - Verify pixel format
    szTestName = (char *)"PNG verify pixel format";
    iTotal++;
    PNGLOG(__LINE__, szTestName, szStart);
    png.openFLASH((uint8_t *)octocat_8bpp, sizeof(octocat_8bpp), PNGDraw);
    iBpp = 0;
    png.decode(NULL, 0);
    if (iBpp == 8) { // should be 8 bits per pixel
        iTotalPass++;
        PNGLOG(__LINE__, szTestName, " - PASSED");
    } else {
        iTotalFail++;
        PNGLOG(__LINE__, szTestName, " - FAILED");
    }
    // Test 4 - Verify correct dimensions
    szTestName = (char *)"PNG verify correct dimensions";
    iTotal++;
    PNGLOG(__LINE__, szTestName, szStart);
    png.openFLASH((uint8_t *)octocat_8bpp, sizeof(octocat_8bpp), PNGDraw);
    iWidth = iLines = 0;
    png.decode(NULL, 0);
    if (iWidth == 120 && iLines == 100) { // should be 4 bits per pixel
        iTotalPass++;
        PNGLOG(__LINE__, szTestName, " - PASSED");
    } else {
        iTotalFail++;
        PNGLOG(__LINE__, szTestName, " - FAILED");
    }
    // Test 5 - Check CRC option
    szTestName = (char *)"PNG Check CRC timing";
    iTotal++;
    PNGLOG(__LINE__, szTestName, szStart);
    png.openFLASH((uint8_t *)octocat_8bpp, sizeof(octocat_8bpp), PNGDraw);
    iTime1 = Micros();
    png.decode(NULL, 0); // without CRC check should be faster
    iTime1 = Micros() - iTime1;
    png.close();
    png.openFLASH((uint8_t *)octocat_8bpp, sizeof(octocat_8bpp), PNGDraw);
    iTime2 = Micros();
    png.decode(NULL, PNG_CHECK_CRC); // with CRC check should be slower
    iTime2 = Micros() - iTime2;
    png.close();
    if (iTime1 < iTime2) { // skipping CRC check should be faster
        iTotalPass++;
        PNGLOG(__LINE__, szTestName, " - PASSED");
    } else {
        iTotalFail++;
        PNGLOG(__LINE__, szTestName, " - FAILED");
    }
    // Test 6 - Check palette options
    szTestName = (char *)"PNG Check palette options";
    iTotal++;
    PNGLOG(__LINE__, szTestName, szStart);
    png.openFLASH((uint8_t *)octocat_8bpp, sizeof(octocat_8bpp), PNGDraw);
    png.decode(NULL, 0); // default palette has 24-bit entries
    pal1 = palentry24;
    png.close();
    png.openFLASH((uint8_t *)octocat_8bpp, sizeof(octocat_8bpp), PNGDraw);
    png.decode(NULL, PNG_FAST_PALETTE); // Fast palette is RGB565 little-endian
    pal2 = palentry16;
    png.close();
    if (pal1 == 0x2c2c2a && pal2 == 0x2965) { // correct 24-bit and 16-bit palette values
        iTotalPass++;
        PNGLOG(__LINE__, szTestName, " - PASSED");
    } else {
        iTotalFail++;
        PNGLOG(__LINE__, szTestName, " - FAILED");
    }
    // Test 7 - Check full framebuffer decode
    szTestName = (char *)"PNG Check full framebuffer decode";
    iTotal++;
    PNGLOG(__LINE__, szTestName, szStart);
    png.openFLASH((uint8_t *)octocat_8bpp, sizeof(octocat_8bpp), NULL);
    pFrameBuffer = (uint8_t *)malloc(png.getWidth() * png.getHeight()); // just big enough for 8-bpp pixels
    png.setBuffer(pFrameBuffer);
    png.decode(NULL, 0);
    png.close();
    c1 = pFrameBuffer[60 + (10 * 120)]; // pixel at (60,10)
    c2 = pFrameBuffer[40 + (40 * 120)]; // pixel at (40,40)
    if ( c1 == 2 &&  c2 == 15) { // check a pixel from first line and 50th line for correct colors
        iTotalPass++;
        PNGLOG(__LINE__, szTestName, " - PASSED");
    } else {
        iTotalFail++;
        PNGLOG(__LINE__, szTestName, " - FAILED");
    }
    free(pFrameBuffer);
    // Test 8 - check 8-bit to RGB565 conversion and alpha blending
    ucPixelType = PNG_RGB565_BIG_ENDIAN;
    u32BG = 0x00ff00; // set background color to pure green
    szTestName = (char *)"PNG Verify 8-bit to RGB565 output";
    iTotal++;
    PNGLOG(__LINE__, szTestName, szStart);
    u16Out = 0xffff;
    png.openFLASH((uint8_t *)octocat_8bpp, sizeof(octocat_8bpp), PNGDraw2);
    png.setBuffer(NULL);
    png.decode(NULL, 0);
    png.close();
    if (u16Out == 0xe007) { // check transparnet pixel (0,0) to see if it matches the 32-bit BG color we asked for
        iTotalPass++;
        PNGLOG(__LINE__, szTestName, " - PASSED");
    } else {
        iTotalFail++;
        PNGLOG(__LINE__, szTestName, " - FAILED");
    }
    // Test 9 - check 32-bit to RGB565 conversion and alpha blending
    ucPixelType = PNG_RGB565_BIG_ENDIAN;
    u32BG = 0xff0000; // set background color to pure blue
    szTestName = (char *)"PNG Verify 32-bit to RGB565 output";
    iTotal++;
    PNGLOG(__LINE__, szTestName, szStart);
    u16Out = 0xffff;
    png.openFLASH((uint8_t *)octocat_8bpp, sizeof(octocat_8bpp), PNGDraw2);
    png.setBuffer(NULL);
    png.decode(NULL, 0);
    png.close();
    if (u16Out == 0x1f00) { // check transparnet pixel (0,0) to see if it matches the 32-bit BG color we asked for
        iTotalPass++;
        PNGLOG(__LINE__, szTestName, " - PASSED");
    } else {
        iTotalFail++;
        PNGLOG(__LINE__, szTestName, " - FAILED");
    }
    // Test 10 - check the decoding can be aborted when the PNGDraw callback returns 0
    ucPixelType = PNG_RGB565_BIG_ENDIAN;
    szTestName = (char *)"PNG decode aborted early";
    iTotal++;
    PNGLOG(__LINE__, szTestName, szStart);
    u16Out = 0xffff;
    png.openFLASH((uint8_t *)octocat_8bpp, sizeof(octocat_8bpp), PNGDraw3);
    png.setBuffer(NULL);
    rc = png.decode(NULL, 0);
    png.close();
    if (rc == PNG_QUIT_EARLY) { // check transparnet pixel (0,0) to see if it matches the 32-bit BG color we asked for
        iTotalPass++;
        PNGLOG(__LINE__, szTestName, " - PASSED");
    } else {
        iTotalFail++;
        PNGLOG(__LINE__, szTestName, " - FAILED");
    }

    // FUZZ testing
    // Randomize the input data (file header and compressed data) and confirm that the library returns an error code
    // and doesn't have an invalid pointer exception
    printf("Begin fuzz testing...\n");
    szTestName = (char *)"Single Byte Sequential Corruption Test";
    iTotal++;
    pFuzzData = (uint8_t *)malloc(sizeof(octocat_8bpp));
    PNGLOG(__LINE__, szTestName, szStart);
    // We don't need to corrupt the file all the way to the end because it will take a loooong time
    // The header is the main area where corruption can cause erratic behavior
    for (i=0; i<sizeof(octocat_8bpp); i++) { // corrupt each byte one at a time by inverting it
        memcpy(pFuzzData, octocat_8bpp, sizeof(octocat_8bpp)); // start with the valid data
        pFuzzData[i] = ~pFuzzData[i]; // invert the bits of this byte
        if (png.openFLASH(pFuzzData, sizeof(octocat_8bpp), PNGDraw) == PNG_SUCCESS) { // the PNG header may be rejected
            png.decode(NULL, 0);
        }
    } // for each test
    PNGLOG(__LINE__, szTestName, " - PASSED");
    iTotalPass++;
    
    // Fuzz test part 2 - multi-byte random corruption
    szTestName = (char *)"Multi-Byte Random Corruption Test";
    iTotal++;
    PNGLOG(__LINE__, szTestName, szStart);
    for (i=0; i<1000; i++) { // 1000 iterations of random spots in the file to corrupt with random values
        int iOffset;
        memcpy(pFuzzData, octocat_8bpp, sizeof(octocat_8bpp)); // start with the valid data
        iOffset = rand() % sizeof(octocat_8bpp);
        pFuzzData[iOffset] = (uint8_t)rand();
        iOffset = rand() % sizeof(octocat_8bpp); // corrupt 2 spots just for good measure
        pFuzzData[iOffset] = (uint8_t)rand();
        if (png.openFLASH(pFuzzData, sizeof(octocat_8bpp), PNGDraw) == PNG_SUCCESS) { // the PNG header may be rejected
            png.decode(NULL, 0);
        }
    } // for each test
    PNGLOG(__LINE__, szTestName, " - PASSED");
    iTotalPass++;
    
    free(pFuzzData);
#endif // FUTURE
    printf("Total tests: %d, %d passed, %d failed\n", iTotal, iTotalPass, iTotalFail);
    return 0;
} /* main() */
