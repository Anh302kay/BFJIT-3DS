#include <3ds.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include <assert.h>
#include "utils.h"

enum {
    OP_RIGHT = '>',
    OP_LEFT = '<',
    OP_ADD = '+',
    OP_SUB = '-',
    OP_OUTPUT = '.',
    OP_INPUT = ',',
    OP_JZ = '[',
    OP_JNZ = ']'
};

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
        ? ((abs(index - size-2) * 4 ) & 0xFFF) \ 
        : (( (index - size-2) * 4 ) & 0xFFF ) | ( 1 << 23 ) ) );

 int parseCode(const char* path) {
    FILE* file = fopen(path, "r");
    if (!file) {
        printf("Could not open file: %s\n", path);
        return -1;
    }
    int size = 0;
    int ch;
    while((ch = fgetc(file))) {
        if(ch == EOF) {
            break;
        }
        switch (ch)
        {
            case OP_LEFT:
                size +=2;
                break;
            case OP_RIGHT:
                size +=2;
                break;
            case OP_ADD:
                size += 1;
                break;
            case OP_SUB:
                size += 1;
                break;
            case OP_OUTPUT:
                size += 4;
                break;
            case OP_INPUT:
                break;
            case OP_JZ:
                size += 2;
                break;
            case OP_JNZ:
                size += 2;
                break;
            default:
                break;
        } 
    }
    fclose(file);

    return size;
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

    //  r3, r4, r5, lr

    int jmpStack[256];
    int jmpStackPos = 0;

    u32 code[fSize+100];
    int size = 5;
    code[2] = (u32)putchar; // address
    code[3] = (u32)memset; // address
    code[4] = 30000; // constant
    code[size++] = 0xE92D4038; // push {r3, r4, r5, lr}
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
    code[size++] = ARM_MOVIMM(R3, 0);
    code[size++] = ARM_MOVREG(R4, SP);
    code[size] = ARM_LOADADDR(R5, 2);
    size++;

    int ch;
    while((ch = fgetc(file))) {
        if(ch == EOF) {
            break;
        }
        switch (ch)
        {
        case OP_LEFT:
            // cellLocation--;
            // assert(cellLocation < 0);
            code[size++] = 0xE4443001;// strb r3, [r4], #-1
            code[size++] = 0xE5D43000;// ldrb r3, [r4]
            break;
        case OP_RIGHT:
            // cellLocation++;
            // assert(cellLocation > 30000);
            code[size++] = 0xE4C43001;// strb r3, [r4], #1
            code[size++] = 0xE5D43000; // ldrb r3, [r4]
            break;
        case OP_ADD:
            code[size++] = ARM_ADDSIMM(R3, R3, 1);
            break;
        case OP_SUB:
            code[size++] = ARM_SUBSIMM(R3, R3, 1);
            break;
        case OP_OUTPUT:
            //no idea why r3 is cleared but prolly due to blx instead of bl
            code[size++] = 0xE4C43000;// strb r3, [r4]
            code[size++] = ARM_MOVREG(R0, R3);
            code[size++] = ARM_BLX(R5);
            code[size++] = 0xE5D43000; // ldrb r3, [r4]
            break;
        case OP_INPUT:
            break;
        case OP_JZ:
            code[size++] = 0xE3530000; // cmp r3, #0
            jmpStack[jmpStackPos++] = size;
            code[size++] = 0xe320F000; // temp NOP
            break;
        case OP_JNZ:
            const int matchingJZ = jmpStack[--jmpStackPos];
            code[size++] = 0xE3530000; // cmp r3, #0
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
    code[size++] = 0xE8BD8038; // pop {r3, r4, r5, pc}
    memcpy(memory, code, size * sizeof(u32));

    if(jmpStackPos != 0) {
        printf("Unbalanced amount of loops");
        ((u32*)memory)[5] = ARM_BLX(LR);
    }
    _SetMemoryPermission(memory, 4096, MEMPERM_READ | MEMPERM_EXECUTE);
	ctr_flush_invalidate_cache();

    fclose(file);
}

typedef int (*JitFunction)();

int main() {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

	_InitializeSvcHack();
    size_t memorySize = 0x1000; // 4 KB
    void* memory = allocateExecMemory(memorySize);
    if (!memory) {
        gfxExit();
        return -1;
    }

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

	free(memory);
    gfxExit();
    return 0;
}
