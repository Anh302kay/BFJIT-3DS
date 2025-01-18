#include <3ds.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include "utils.h"

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
        0, // Placeholder for the address of getNumber
	    0xE51f000c, // ldr r0, [pc, #-12]
	    0xE52de004, // push {lr}
        0xE12FFF30,  // bx r0
        0xE3A01003,  // MOV R1, #3
        0xE0000190,  // MUL R0, R0, R1   
        0xE49df004   // pop PC
    };
    code[0] = (u32)addr;
    memcpy(memory, code, sizeof(code));
    //__builtin___clear_cache(memory, memory + sizeof(code)); // Sync CPU cache
	ctr_flush_invalidate_cache();
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

    generateCode(memory);
    JitFunction func = (JitFunction)memory+4;

    int result = func(); 
		printf("JIT code returned: %d\n", result);
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
