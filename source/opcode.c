#include <stdio.h>
#include <3ds.h>
#include <stdlib.h>

#include "opcode.h"

void initCode(Code* code, size_t capacity)
{
    code->code = malloc(sizeof(Opcode) * capacity);
    code->size = 0;
    code->capacity = capacity;
}

inline void addOpcode(Code* code, const u8 opcode)
{
    if(code->size >= code->capacity) {
        code->capacity *= 2;
        code->code = realloc(code->code, sizeof(Opcode) * code->capacity);
    }
    code->code[code->size].opcode = opcode;
    code->code[code->size].size = 1;
    code->size++;
}

inline void freeCode(Code* code)
{
    free(code->code);
}
