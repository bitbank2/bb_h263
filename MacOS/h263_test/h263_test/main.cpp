//
//  main.cpp
//  h263_test
//
//  Created by Laurence Bank on 29/03/2026.
//

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#define TRACE_MEMORY
#include "../../../src/bb_h263.h"
#include "../../../examples/esp32_player/homer.h"
BB_H263 h263;
BB_RECT bbr;

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
// Minimal code to save frames as Windows BMP files
//
void WriteBMP(const char *fname, uint8_t *pBitmap, uint8_t *pPalette, int cx, int cy, int bpp)
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

int main(int argc, const char * argv[]) {
    int rc, w, h, iFrame=0;
//    rc = h263.open("/Users/laurencebank/Documents/Arduino/libraries/bb_h263/sample_videos/homer_car_h263.avi");
      rc = h263.open("/Users/laurencebank/Downloads/out.avi");
//    rc = h263.open("/Users/laurencebank/Downloads/matrix_h263.mov");
    //    rc = h263.open("/Users/laurencebank/Downloads/homer_car_h263.mov");
 //   rc = h263.open("/Users/laurencebank/Downloads/homer_h263_320x180.mov");
    if (rc == H263_SUCCESS) {
        w = h263.getWidth();
        h = h263.getHeight();
        printf("Opened H.263 file: %d x %d, %d frames\n", w, h, h263.getFrameCount());
        iFrame = 0;
        // Clip to a 320x240 window
        bbr.x = bbr.y = 16;
        bbr.w = 320; bbr.h = 240;
        h263.setClipRect(&bbr);
        h263.allocFramebuffer();
        while (rc == H263_SUCCESS) {
            rc = h263.decodeFrame();
            printf("Frame %d of %d\n", iFrame, h263.getFrameCount());
//            if (iFrame == 20) {
//                WriteBMP("/Users/laurencebank/Downloads/h263.bmp", h263.getFramebuffer(), NULL, 320, 240, 16);
//            }
            iFrame++;
        }
    }
    h263.close();
    printf("finished decoding %d frames\n", iFrame);
    return 0;
}
