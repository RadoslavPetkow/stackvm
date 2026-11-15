#ifndef OPCODES_H
#define OPCODES_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    OP_HALT  = 0x00,
    OP_PUSH  = 0x01,
    OP_POP   = 0x02,
    OP_DUP   = 0x03,
    OP_ADD   = 0x04,
    OP_SUB   = 0x05,
    OP_MUL   = 0x06,
    OP_CMPEQ = 0x07,
    OP_CMPLT = 0x08,
    OP_CMPGT = 0x09,
    OP_JMP   = 0x0A,
    OP_JZ    = 0x0B,
    OP_CALL  = 0x0C,
    OP_RET   = 0x0D,
    OP_LOAD  = 0x0E,
    OP_STORE = 0x0F,
    OP_PRINT = 0x10,
    OP_SWAP  = 0x11,
    OP_LOADI  = 0x12, // индексът идва от стека, не от аргумент
    OP_STOREI = 0x13,
    OP_COUNT
} Opcode;

typedef struct {
    const char *name;
    Opcode opcode;
    bool has_operand; // false = 1 байт, true = 5 байта (1B опкод + 4B int32_t)
} InstrInfo;

static const InstrInfo OPCODE_TABLE[OP_COUNT] = {
    [OP_HALT]  = {"HALT",  OP_HALT,  false},
    [OP_PUSH]  = {"PUSH",  OP_PUSH,  true},
    [OP_POP]   = {"POP",   OP_POP,   false},
    [OP_DUP]   = {"DUP",   OP_DUP,   false},
    [OP_ADD]   = {"ADD",   OP_ADD,   false},
    [OP_SUB]   = {"SUB",   OP_SUB,   false},
    [OP_MUL]   = {"MUL",   OP_MUL,   false},
    [OP_CMPEQ] = {"CMPEQ", OP_CMPEQ, false},
    [OP_CMPLT] = {"CMPLT", OP_CMPLT, false},
    [OP_CMPGT] = {"CMPGT", OP_CMPGT, false},
    [OP_JMP]   = {"JMP",   OP_JMP,   true},
    [OP_JZ]    = {"JZ",    OP_JZ,    true},
    [OP_CALL]  = {"CALL",  OP_CALL,  true},
    [OP_RET]   = {"RET",   OP_RET,   false},
    [OP_LOAD]  = {"LOAD",  OP_LOAD,  true},
    [OP_STORE] = {"STORE", OP_STORE, true},
    [OP_PRINT] = {"PRINT", OP_PRINT, false},
    [OP_SWAP]  = {"SWAP",  OP_SWAP,  false},
    [OP_LOADI]  = {"LOADI",  OP_LOADI,  false},
    [OP_STOREI] = {"STOREI", OP_STOREI, false}
};

#endif // OPCODES_H
