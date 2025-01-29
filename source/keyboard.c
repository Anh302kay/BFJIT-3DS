#include <stdio.h>
#include <string.h>
#include <3ds.h>

#include "keyboard.h"

#include "keyboard1_bin.h"
#include "keyboard2_bin.h"

const char keyboardLUT[] = "`1234567890-=qwertyuiop[]\\asdfghjkl;'zxcvbnm,./"
                           "~!@#$%^&*()_+QWERTYUIOP{}|ASDFGHJKL:\"ZXCVBNM<>?";

u8 getKeyInput()
{
    touchPosition touch;
    bool capsLock = false;
    memcpy(gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL), keyboard1_bin, keyboard1_bin_size);
    while(aptMainLoop()) {
        hidScanInput();
        hidTouchRead(&touch);
        const u32 kDown = hidKeysDown();
        touch.py -= 25;
        if(kDown & KEY_TOUCH) {
            if(touch.py > 195-25 && touch.py < 238-25 && touch.px > 3 && touch.px < 62) {
                capsLock = !capsLock;
                memcpy(gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL), capsLock ? keyboard2_bin : keyboard1_bin, capsLock ? keyboard2_bin_size : keyboard1_bin_size);
                continue;
            }
            switch(touch.py / 40) {
                case 0:
                    touch.px -= 3;
                    return keyboardLUT[capsLock * 47 + (touch.px / 24)];
                    break;
                case 1:
                    touch.px -= 7;
                    if(touch.px / 24 < 13)
                        return keyboardLUT[capsLock * 47 + (touch.px / 24) + 13];
                    break;
                case 2:
                    touch.px -= 23;
                    if(touch.px / 24 < 10)
                        return keyboardLUT[capsLock * 47 + (touch.px / 24) + 26];
                    break;
                case 3:
                    touch.px -= 31;
                    if(touch.px / 24 < 10)
                        return keyboardLUT[capsLock * 47 + (touch.px / 24) + 37];
                    break;
                case 4:
                    if(touch.px > 72 && touch.px < 188)
                        return ' ';
                    break;
                default:
                    break;
            }
        }
            

    }
    return 0;
}