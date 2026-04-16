//
//  main.cpp
//  h263_test
//
//  Created by Laurence Bank on 29/03/2026.
//

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "../../../src/bb_h263.h"
#include "../../../examples/esp32_player/homer.h"
BB_H263 h263;

int main(int argc, const char * argv[]) {
    int rc, w, h, iFrame;
//    rc = h263.open("/Users/laurencebank/Downloads/matrix_h263.mov");
    rc = h263.open("/Users/laurencebank/Downloads/homer_car_h263.mov");
    if (rc == H263_SUCCESS) {
        w = h263.getWidth();
        h = h263.getHeight();
        printf("Opened H.263 file: %d x %d, %d frames\n", w, h, h263.getFrameCount());
        iFrame = 0;
        h263.allocFramebuffer();
        while (rc == H263_SUCCESS) {
            rc = h263.decodeFrame();
            printf("Frame %d of %d\n", iFrame, h263.getFrameCount());
            iFrame++;
        }
    }
    h263.close();
    printf("finished\n");
    return 0;
}
