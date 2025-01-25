#pragma once

#include <stdio.h>
#include <3ds.h>
#include <stdlib.h>

// the hardest thing in programming is naming things
enum {
    ASCII_RIGHT = '>',
    ASCII_LEFT = '<',
    ASCII_ADD = '+',
    ASCII_SUB = '-',
    ASCII_OUTPUT = '.',
    ASCII_INPUT = ',',
    ASCII_JZ = '[',
    ASCII_JNZ = ']'
};

enum {
    OP_RIGHT = (1 << 0),
    OP_LEFT = (1 << 1),
    OP_ADD = (1 << 2),
    OP_SUB = (1 << 3),
    OP_OUTPUT = (1 << 4),
    OP_INPUT = (1 << 5),
    OP_JZ = (1 << 6),
    OP_JNZ = (1 << 7)
};

typedef struct Opcode {
    u8 opcode;
    size_t size; // unused right now but left for an optimisation im thinking of
} Opcode;

typedef struct Code {
    Opcode* code;
    size_t size;
    size_t capacity;
    size_t asmSize;
} Code;

void initCode(Code* code, size_t capacity);
inline void addOpcode(Code* code, const u8 opcode);
void freeCode(Code* code);