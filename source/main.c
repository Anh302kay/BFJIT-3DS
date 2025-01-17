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

//to get raw machine code of assembly instructios use the arm architecture reference manual
void generateCode(void* memory) {
    unsigned char code[] = {
        0x2A, 0x00, 0xA0, 0xE3,  // MOV R0, #42
        0x03, 0x10, 0xA0, 0xE3,  // MOV R1, #3
        0x90, 0x01, 0x00, 0xE0,  // MUL R0, R0, R1
        0x1E, 0xFF, 0x2F, 0xE1   // BX LR
    };
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
    JitFunction func = (JitFunction)memory;

    int result = func(); 
	while(aptMainLoop())
	{
        hidScanInput();
		if(hidKeysDown() & KEY_START)
			break;

		printf("JIT code returned: %d\n", result);
	}

	free(memory);
    gfxExit();
    return 0;
}
