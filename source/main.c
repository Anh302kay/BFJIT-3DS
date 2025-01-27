#include <3ds.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include "filesystem.h"
#include "utils.h"
#include "opcode.h"

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

int getNumber() {
    return 42;
}

static inline int nearest4096(int value) {
    return (value + 4095) & ~0xFFF;
}

//to get raw machine code of assembly instructios use the arm architecture reference manual
void generateCode(void* memory) {
    void* addr = getNumber;
    printf("getNumber() address: %d\n", addr);
    
    u32 code[] = {
        (u32)addr, // Placeholder for the address of getNumber
	    0xE51f000c, // ldr r0, [pc, #-12]
	    0xE52de004, // push {lr}
        0xE12FFF30,  // blx r0
        0xE3A01003,  // MOV R1, #3
        0xE0000190,  // MUL R0, R0, R1   
        0xE49df004   // pop PC
    };
    memcpy(memory, code, sizeof(code));
    //__builtin___clear_cache(memory, memory + sizeof(code)); // Sync CPU cache
	ctr_flush_invalidate_cache();
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
    int jmpStack[256];
    int jmpStackPos = 0;
    code->asmSize = 18;
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
                break;
            case ASCII_JZ:
                jmpStack[jmpStackPos++] = code->asmSize+1;
                code->asmSize += 2;
                addOpcode(code, OP_JZ);
                break;
            case ASCII_JNZ:
                const int matchingJZ = jmpStack[--jmpStackPos];
                code->code[code->asmSize+1].size = ( (0b11010 << 24) | ( ((matchingJZ - (code->asmSize+3))) & 0xFFFFFF) ); // bne 
                code->code[matchingJZ].size = ( (0b1010 << 24) | ( (((code->asmSize) - matchingJZ )) & 0xFFFFFF) ); // beq
                code->asmSize += 2;
                addOpcode(code, OP_JNZ);
                break;
            default:
                break;
        } 
    }
    fclose(file);

    return 1;
}

void jitCompile(const char* path, void* memory) {
    FILE* file = fopen(path, "r");
    if (!file) {
        printf("Could not open file: %s\n", path);
        ((u32*)memory)[5] = ARM_MOVREG(R0, PC);
        ((u32*)memory)[6] = ARM_BLX(LR);
        return;
    }
    fseek(file, 0, SEEK_END);
    int fSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    //  r4, r5, lr

    int jmpStack[256];
    int jmpStackPos = 0;

    u32 code[fSize+100];
    int size = 5;
    code[2] = (u32)putchar; // address
    code[3] = (u32)memset; // address
    code[4] = 30000; // constant
    code[size++] = 0xE92D4070; // push {r4, r5, r6, lr}
    code[size++] = 0xE24DDC75; // sub sp, sp, #29952
    code[size++] = ARM_SUBIMM(SP, SP, 48);
    code[size++] = ARM_MOVREG(R0, SP);
    code[size++] = ARM_MOVIMM(R1, 0);
    code[size] = ARM_LOADADDR(R2, 4); // ldr r2, [pc, #-32]
    size++;
    code[size] = ARM_LOADADDR(R4, 3); // ldr r4, [pc, #-40]
    size++;
    // code[size++] = 0xE51F4028; // ldr r4, [pc, #-40]
    code[size++] = ARM_BLX(R4); 
    code[size++] = ARM_MOVIMM(R4, 0);
    code[size++] = ARM_MOVREG(R5, SP);
    code[size] = ARM_LOADADDR(R6, 2);
    size++;

    int ch;
    while((ch = fgetc(file))) {
        if(ch == EOF) {
            break;
        }
        switch (ch)
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
            code[size++] = ARM_ADDSIMM(R4, R4, 1);
            break;
        case OP_SUB:
            code[size++] = ARM_SUBSIMM(R4, R4, 1);
            break;
        case OP_OUTPUT:
            code[size++] = ARM_MOVREG(R0, R4);
            code[size++] = ARM_BLX(R6);
            break;
        case OP_INPUT:
            break;
        case OP_JZ:
            code[size++] = ARM_CMPIMM(R4, 0);
            jmpStack[jmpStackPos++] = size;
            code[size++] = 0xe320F000; // temp NOP
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
    code[size++] = 0xE8BD8070; // pop {r4, r5, r6, pc}
    
    memcpy(memory, code, size * sizeof(u32));

    if(jmpStackPos != 0) {
        printf("Unbalanced amount of loops");
        ((u32*)memory)[5] = ARM_BLX(LR);
    }
    _SetMemoryPermission(memory, 4096, MEMPERM_READ | MEMPERM_EXECUTE);
	ctr_flush_invalidate_cache();

    fclose(file);
}

static inline void drawConfirm(const char* name) {
    printf("\x1b[1;5H\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB");
    printf("\x1b[2;5H\xBA             RUN             \xBA");
    printf("\x1b[3;5H\xBA                             \xBA");
    printf("\x1b[3;%DH%s", 31/2 - strnlen(name, 512)/2 + 5, name);
    printf("\x1b[4;5H\xBA                             \xBA");
    printf("\x1b[5;5H\xC8\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC");
}

typedef int (*JitFunction)();

int main() {
    gfxInitDefault();
    PrintConsole top;
    PrintConsole bottom;
    consoleInit(GFX_TOP, &top);
    consoleInit(GFX_BOTTOM, &bottom);
    hidSetRepeatParameters(1500, 1750);

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
    Files* fileList = openDirectory("");
    Files* currentFile = fileList;

    Code jitCode;
    initCode(&jitCode, 256);

    char oldLocation[512];
    int mode = 0;
    bool selection = false; // false = no, true = yes
    while(aptMainLoop())
	{
        hidScanInput();
        u32 kRepeat = hidKeysDownRepeat();
		if(kRepeat & KEY_START)
			break;

        switch (mode) {
        case 0:
            if(kRepeat & KEY_UP) {
                if(currentFile->lastEnt != NULL)
                    currentFile = currentFile->lastEnt;
            } else if(kRepeat & KEY_DOWN) {
                if(currentFile->nextEnt != NULL)
                    currentFile = currentFile->nextEnt;
            }
            if(kRepeat & KEY_A) 
            {
                if(currentFile->isDirectory) {
                    consoleClear();
                    openNewDirectory(fileList, currentFile);
                    currentFile = fileList;
                }
                else {
                    mode = 1;
                }
            }
            else if(kRepeat & KEY_B) {
                consoleClear();
                strncpy(oldLocation, getSlash(currentFile->path)+1, 512);
                openPreviousDirectory(fileList, currentFile);
                for(Files* file = fileList; file != NULL; file = file->nextEnt) {
                    if(strcmp(oldLocation, file->name) == 0) {
                        currentFile = file;
                        break;
                    }
                }
                
            }
        
            printf("%s              \r", currentFile->name);
            break;
        case 1:

            drawConfirm(currentFile->name);
            if(kRepeat & KEY_A && selection) {
                strncpy(oldLocation, currentFile->path, 512);
                strncat(oldLocation, currentFile->name, 512);
                mode = 2;
            } else if(kRepeat & KEY_B) {
                mode = 0;
            }
            break;
        case 2:
                    consoleSelect(&top);
            printf("Compiling: %s\n", currentFile->name);
            parseCode(&jitCode, oldLocation);
            jitCompile("/bf.txt", memory);
            JitFunction func = (JitFunction)memory+(5*4);
            break;
        
        default:
            break;
        }

        gspWaitForVBlank();
	}

    consoleSelect(&top);

    // generateCode(memory);
    jitCompile("/bf.txt", memory);
    JitFunction func = (JitFunction)memory+(5*4);

    u64 start = svcGetSystemTick();
    int result = func(); 
    u64 end = svcGetSystemTick();
	printf("JIT code returned: %p\n", result);
	printf("Jit Address:       %p\n", memory+(5*4));
	printf("Time: %lld\n", end-start);
	while(aptMainLoop())
	{
        hidScanInput();
		if(hidKeysDown() & KEY_START)
			break;

	}

    freeCode(&jitCode);

    freeDirectory(fileList);
	free(memory);
    gfxExit();
    return 0;
}
