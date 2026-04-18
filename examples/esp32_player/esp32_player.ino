//
// H.263 Player example
//
#include <bb_spi_lcd.h>
#include <bb_h263.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "homer.h"

BB_H263 h263;
BB_SPI_LCD lcd;
TaskHandle_t myTaskHandle = NULL;
volatile bool bReady = false;
int xoff, yoff, w, h;
uint8_t *pBuf;

void ShowFrame(void *p)
{
//  while (1) {
//    if (bReady) {
       lcd.setAddrWindow(xoff, yoff, w, h);
       lcd.pushPixels((uint16_t *)pBuf, w*h);
//       bReady = false;
//    } else {
//      vTaskDelay(1);
//    }
//  }
} /* ShowFrame() */

void setup()
{
  int rc;
  int iFrame;
  BB_RECT bbr;
  
  Serial.begin(115200);
  delay(3000);
  Serial.println("Starting H.263 demo");
 
 // Arduino runs on core 1, so pin this task to core 0
 // xTaskCreatePinnedToCore(ShowFrame, "Show Frame", 4096, NULL,10, &myTaskHandle, 0);

  lcd.begin(DISPLAY_LILYGO_T_DECK_PLUS); //DISPLAY_WS_AMOLED_18);
  lcd.fillScreen(TFT_BLACK);
  
  rc = h263.open(homer, (int)sizeof(homer));
  if (rc == H263_SUCCESS) {
      h263.setPixelType(H263_PIXEL_RGB565_BE);
      w = h263.getWidth();
      h = h263.getHeight();
      Serial.printf("File opened: %d x %d, %d frames\n", w, h, h263.getFrameCount());
      // clip 352x288 video to 320x176 of actual content
      bbr.x = bbr.y = 0;
      bbr.w = 320; bbr.h = 176; // needs to be a multiple of 16 (MACROBLOCK size)
      h263.setClipRect(&bbr);
      w = 320; h = 176;
      xoff = (lcd.width() - w)/2;
      yoff = (lcd.height() - h)/2;
      pBuf = (uint8_t *)malloc(w * h *2);
      h263.setFramebuffer(pBuf, w * 2);
      for (int iLoop = 0; iLoop <5; iLoop++) {
        iFrame = 0;
        rc = H263_SUCCESS;
        while (rc == H263_SUCCESS) {
            rc = h263.decodeFrame(0, 0);
            //while (bReady) {
            //  delay(1); // wait for display thread to finish
           // }
            bReady = true; // draw it
            ShowFrame(nullptr);
            //Serial.printf("Finished frame %d\n", iFrame);
            iFrame++;
        }
      }
      Serial.println("About to call close");
      h263.close();
      Serial.println("Returned from close");
  }
}

void loop()
{
}

