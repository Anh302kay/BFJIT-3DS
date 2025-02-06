#include <3ds.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include "filesystem.h"
#include "directorylist.h"
#include "keyboard.h"
#include "utils.h"
#include "opcode.h"

#define posToBufferB(x, y) ((int)(y) * 40 + (int)(x))

enum {
    R0 = 0,
    R1 = 1,
    R2 = 2,
    R3 = 3,
    R4 = 4,
    R5 = 5,
    R6 = 6,
    R7 = 7,
    R8 = 8,
    R9 = 9,
    R10 = 10,
    R11 = 11,
    R12 = 12,
    SP = 13,
    LR = 14,
    PC = 15
};

void* allocateExecMemory(size_t size) {
    void* memory = memalign(4096, size);
    if(memory == NULL) {
        printf("Could not allocated memory size: %zu", size);
        return NULL;
    }
	_SetMemoryPermission(memory, size, MEMPERM_READ | MEMPERM_WRITE | MEMPERM_EXECUTE);
    return memory;
}

static inline int nearest4096(int value) {
    return (value + 4095) & ~0xFFF;
}

// where num is > 256 works normally otherwise bit stuff needed
#define ARM_ADDIMM(dst, reg1, num) ((0b111000101 << 23) | (dst << 16) | (reg1 << 12) | (num))
#define ARM_ADDSIMM(dst, reg1, num) ((0b111000101001 << 20) | (dst << 16) | (reg1 << 12) | (num))

#define ARM_MOVREG(dst, src) ((0b11100001101 << 21) | (dst << 12) | (src))
#define ARM_MOVIMM(dst, imm) ((0b11100011101 << 21) | (dst << 12) | (imm))

#define ARM_SUBIMM(dst, reg1, num) ((0b1110001001 << 22) | (dst << 16) | (reg1 << 12) | (num))
#define ARM_SUBSIMM(dst, reg1, num) ((0b111000100101 << 20) | (dst << 16) | (reg1 << 12) | (num))

#define ARM_BLX(reg1) ((0b11100001001011111111111100110000) | (reg1))
// #define ARM_LOADADDR(reg1, index) ((0b111001010001 << 20) | (PC << 16) | (reg1 << 12) | (abs(index - size-2) * 4 ) & 0xFFF)
#define ARM_LOADADDR(reg1, index) ((0b111001010001 << 20) | (PC << 16) | (reg1 << 12) | ( ((int)(index - size-2) * 4 ) < 0\ 
        ? ((abs(index - size-2) * 4 ) & 0xFFF)\ 
        : (( (index - size-2) * 4 ) & 0xFFF ) | ( 1 << 23 ) ) );

#define ARM_CMPIMM(reg, num) ( 0xE3500000 | ( reg << 16) | (num) )

int parseCode(Code* code, const char* path) {
    FILE* file = fopen(path, "r");
    if (!file) {
        printf("Could not open file: %s\n", path);
        return -1;
    }
    // int jmpStack[256];
    // int jmpStackPos = 0;
    code->size = 0;
    code->asmSize = 20;
    memset(code->code, 0, sizeof(Opcode) * code->capacity);
    int ch;
    while((ch = fgetc(file))) {
        if(ch == EOF) {
            break;
        }
        switch (ch)
        {
            case ASCII_LEFT:
                code->asmSize +=2;
                addOpcode(code, OP_LEFT);
                break;
            case ASCII_RIGHT:
                code->asmSize +=2;
                addOpcode(code, OP_RIGHT);
                break;
            case ASCII_ADD:
                code->asmSize += 1;
                addOpcode(code, OP_ADD);
                break;
            case ASCII_SUB:
                code->asmSize += 1;
                addOpcode(code, OP_SUB);
                break;
            case ASCII_OUTPUT:
                code->asmSize += 2;
                addOpcode(code, OP_OUTPUT);
                break;
            case ASCII_INPUT:
                code->asmSize += 2;
                addOpcode(code, OP_INPUT);
                break;
            case ASCII_JZ:
                code->asmSize += 2;
                addOpcode(code, OP_JZ);
                // jmpStack[jmpStackPos++] = code->size-1;
                break;
            case ASCII_JNZ:
                code->asmSize += 2;
                addOpcode(code, OP_JNZ);
                // const int matchingJZ = jmpStack[--jmpStackPos];
                // code->code[matchingJZ].size = ( (0b1010 << 24) | ( (((code->size-2) - matchingJZ )) & 0xFFFFFF) ); // beq
                // code->code[code->size-1].size = ( (0b11010 << 24) | ( ((matchingJZ - (code->size+1))) & 0xFFFFFF) ); // bne 
                break;
            default:
                break;
        } 
    }
    fclose(file);

    return 1;
}

void jitCompile(Code* s_code, void* memory) {
    u32 code[s_code->asmSize];
    int jmpStack[256];
    int jmpStackPos = 0;

    int size = 5;
    code[1] = (u32)getKeyInput;
    code[2] = (u32)putchar; // address
    code[3] = (u32)memset; // address
    code[4] = 30000; // constant
    code[size++] = 0xE92D40F0; // push {r4, r5, r6, r7, lr}
    code[size++] = 0xE24DDC75; // sub sp, sp, #29952
    code[size++] = ARM_SUBIMM(SP, SP, 48);
    code[size++] = ARM_MOVREG(R0, SP);
    code[size++] = ARM_MOVIMM(R1, 0);
    code[size] = ARM_LOADADDR(R2, 4); // ldr r2, [pc, #-32]
    size++;
    code[size] = ARM_LOADADDR(R4, 3); // ldr r4, [pc, #-40]
    size++;
    code[size++] = ARM_BLX(R4); 
    code[size++] = ARM_MOVIMM(R4, 0);
    code[size++] = ARM_MOVREG(R5, SP);
    code[size] = ARM_LOADADDR(R6, 2);
    size++;
    code[size] = ARM_LOADADDR(R7, 1);
    size++;

    for(int i = 0; i < s_code->size; i++) {
        const u8 opcode = s_code->code[i].opcode;
        switch (opcode)
        {
        case OP_LEFT:
            code[size++] = 0xE4454001;// strb r4, [r5], #-1
            code[size++] = 0xE5D54000;// ldrb r4, [r5]
            break;
        case OP_RIGHT:
            code[size++] = 0xE4C54001;// strb r4, [r5], #1
            code[size++] = 0xE5D54000; // ldrb r4, [r5]
            break;
        case OP_ADD:
            code[size++] = ARM_ADDIMM(R4, R4, 1);
            break;
        case OP_SUB:
            code[size++] = ARM_SUBIMM(R4, R4, 1);
            break;
        case OP_OUTPUT:
            code[size++] = ARM_MOVREG(R0, R4);
            code[size++] = ARM_BLX(R6);
            break;
        case OP_INPUT:
            code[size++] = ARM_BLX(R7);
            code[size++] = ARM_MOVREG(R4, R0);
            break;
        case OP_JZ:
            code[size++] = ARM_CMPIMM(R4, 0);
            jmpStack[jmpStackPos++] = size;
            code[size++] = 0xE320F000; // temp NOP
            break;
        case OP_JNZ:
            const int matchingJZ = jmpStack[--jmpStackPos];
            code[size++] = ARM_CMPIMM(R4, 0);
            code[size] = ( (0b11010 << 24) | ( ((matchingJZ - (size+2))) & 0xFFFFFF) ); // bne 
            code[matchingJZ] = ( (0b1010 << 24) | ( (((size-1) - matchingJZ )) & 0xFFFFFF) ); // beq
            size++;
            break;
        default:
            break;
        } 

    }
    code[size++] = ARM_ADDIMM(SP, SP, 48); // add sp, sp, #48
    code[size++] = 0xE28DDC75; // add sp, sp, #29952
    code[size++] = 0xE8BD80F0; // pop {r4, r5, r6, r7, pc}
    
    if(jmpStackPos != 0) {
        printf("Unbalanced amount of loops");
        code[5] = ARM_BLX(LR);
        s_code->asmSize = 6;
    }

    memcpy(memory, code, s_code->asmSize * sizeof(u32));

    // _SetMemoryPermission(memory, 4096, MEMPERM_READ | MEMPERM_EXECUTE);
	ctr_flush_invalidate_cache();

}
// // custom strncpy
// void c_strncpy(char* dst, const char* src, size_t size) {

// }

static inline void drawConfirm(const char* name, bool selected) {
    printf("\x1b[1;5H\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB");
    printf("\x1b[2;5H\xBA             RUN             \xBA");
    printf("\x1b[3;5H\xBA                             \xBA");
    printf("\x1b[3;%DH%s", 31/2 - strnlen(name, 512)/2 + 5, name);
    if(selected)
    printf("\x1b[4;5H\xBA     YES                     \xBA");
    else
    printf("\x1b[4;5H\xBA                     NO      \xBA");
    printf("\x1b[5;5H\xC8\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC");
}

typedef int (*JitFunction)();

int main() {
    gfxInitDefault();
    PrintConsole top;
    PrintConsole bottom;
    consoleInit(GFX_TOP, &top);
    consoleInit(GFX_BOTTOM, &bottom);
    char bottomBuffer[40*30+1];
    memset(bottomBuffer, ' ', 40*30+1);
    bottomBuffer[40*30] = '\0';
    hidSetRepeatParameters(1500, 1000);

    consoleSelect(&top);

	_InitializeSvcHack();
    size_t memorySize = 0x1000; // 4 KB
    void* memory = allocateExecMemory(memorySize);
    if (!memory) {
        gfxExit();
        return -1;
    }

    // char filePath[MAXFILELENGTH] = {0};

    consoleSelect(&bottom);
    DirectoryList dList;
    dList.fileList = openDirectory("/3ds");
    dList.currentFile = dList.fileList;
    dList.viewTop = dList.fileList;
    dList.viewBottom = dList.fileList;
    dList.currentY = 2;
    dList.viewY = 2;

    displayList(&dList, bottomBuffer);

    Code jitCode;
    initCode(&jitCode, 256);

    char oldLocation[512];
    char selectedFile[512];
    int mode = 0;
    bool selection = true; // false = no, true = yes
    while(aptMainLoop())
	{
        hidScanInput();
        const u32 kRepeat = hidKeysDownRepeat();
        const u32 kDown = hidKeysDown();

		if(kRepeat & KEY_START)
			break;

        switch (mode) {
        case 0:
            if(kRepeat & KEY_UP) {
                if(dList.currentFile->lastEnt != NULL)
                {
                    dList.currentFile = dList.currentFile->lastEnt;
                    if(dList.viewY == 2 && dList.currentY != dList.viewY) {
                        scrollBufferUp(&dList, bottomBuffer, 1);
                    }
                    else
                        dList.viewY--;
                    dList.currentY--;
                }
            } else if(kRepeat & KEY_DOWN) {
                if(dList.currentFile->nextEnt != NULL)
                {
                    dList.currentY++;
                    if(dList.currentY > 30 && dList.viewY == 30) {
                        scrollBufferDown(&dList, bottomBuffer, 1);
                    }
                    else 
                        dList.viewY++;
                    dList.currentFile = dList.currentFile->nextEnt;
                }
            } else if(kDown & KEY_LEFT) {
                if(dList.currentFile->lastEnt->lastEnt != NULL)
                {
                    dList.currentFile = dList.currentFile->lastEnt->lastEnt;
                    if(dList.viewY == 2 && dList.currentY != dList.viewY) {
                        scrollBufferUp(&dList, bottomBuffer, 1);
                        scrollBufferUp(&dList, bottomBuffer, 1);
                    }
                    else
                        dList.viewY-= 2;
                    dList.currentY-= 2;
                }
            } else if(kDown & KEY_RIGHT) {
                if(dList.currentFile->nextEnt->nextEnt != NULL)
                {
                    dList.currentY+= 2;
                    if(dList.currentY > 30 && dList.viewY == 30) {
                        scrollBufferDown(&dList, bottomBuffer, 1);
                        scrollBufferDown(&dList, bottomBuffer, 1);
                    }
                    else 
                        dList.viewY+= 2;
                    dList.currentFile = dList.currentFile->nextEnt->nextEnt;
                }
            }
            if(kDown & KEY_A) 
            {
                if(dList.currentFile->isDirectory) {
                    memset(bottomBuffer, ' ', 40*30);
                    openNewDirectory(dList.fileList, dList.currentFile);
                    displayList(&dList, bottomBuffer);
                }
                else {
                    mode = 1;
                }
            }
            else if(kDown & KEY_B) {
                memset(bottomBuffer, ' ', 40*30);
                strncpy(oldLocation, getSlash(dList.currentFile->path)+1, 512);
                openPreviousDirectory(dList.fileList, dList.currentFile);
                displayPrevList(&dList, bottomBuffer, oldLocation);
            }

            bottom.cursorY = 0;
            bottom.cursorX = 0;
            // printf("%s\r", currentFile->name);
            bottom.bg = 0;
            bottom.fg = 7;
            printf("%s", bottomBuffer);
            bottom.bg = 7;
            bottom.fg = 0;
            printf("\x1b[%D;0H%0.39s", dList.viewY,  dList.currentFile->name);
            printf("\x1b[0;0H%D, %D, %s", dList.currentY, dList.viewY, dList.viewBottom->name);
            break;
        case 1:

            drawConfirm(dList.currentFile->name, selection);
            if(kDown & KEY_LEFT) {
                selection = true;
            }
            else if(kDown & KEY_RIGHT) {
                selection = false;
            }
            if(kDown & KEY_A && selection) {
                strncpy(selectedFile, dList.currentFile->path, 512);
                strncat(selectedFile, dList.currentFile->name, 512);
                mode = 2;
            } else if(kDown & KEY_B || (kDown & KEY_A && !selection)) {
                mode = 0;
            }
            break;
        case 2:
            consoleSelect(&top);
            printf("Compiling: %s\n", dList.currentFile->name);
            parseCode(&jitCode, selectedFile);
            jitCompile(&jitCode, memory);
            JitFunction func = (JitFunction)memory+(5*4);
            consoleSelect(&top);
            func();
            consoleSelect(&bottom);
            mode = 0;
            break;
        
        default:
            break;
        }
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
	}

    freeCode(&jitCode);
    freeDirectory(dList.fileList);
	free(memory);
    gfxExit();
    return 0;
}
