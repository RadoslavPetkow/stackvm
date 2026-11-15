#ifndef VM_H
#define VM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "opcodes.h"

#define STACK_MAX      256
#define CALL_STACK_MAX 64
#define MEMORY_MAX     1024
#define VM_MAGIC_BYTES "VM01" // записва се/проверява се с memcmp(buf, "VM01", 4)

typedef enum {
    VM_OK = 0,
    VM_HALTED,
    VM_ERR_IP_OOB,          // ip извън code_size или непълен 4-байтов аргумент
    VM_ERR_INVALID_OPCODE,  // непознат байт за инструкция
    VM_ERR_STACK_OVERFLOW,  // sp >= STACK_MAX
    VM_ERR_STACK_UNDERFLOW, // sp < необходимия брой операнди (1 или 2)
    VM_ERR_CALL_OVERFLOW,   // csp >= CALL_STACK_MAX
    VM_ERR_CALL_UNDERFLOW,  // csp <= 0 при RET
    VM_ERR_MEM_OOB,         // LOAD/STORE индекс < 0 или >= MEMORY_MAX
    VM_ERR_BAD_JUMP         // целевият адрес на JMP/JZ/CALL е < 0 или >= code_size
} VMStatus;

typedef struct {
    const uint8_t *code;
    size_t code_size;
    uint32_t ip;

    int32_t stack[STACK_MAX];
    int sp; // сочи към първия свободен слот (0 = празен стек)

    uint32_t call_stack[CALL_STACK_MAX];
    int csp; // сочи към първия свободен слот (0 = празен call stack)

    int32_t memory[MEMORY_MAX];
    bool trace;

    // Небезопасен режим (--unsafe): адресите на връщане не стоят в call_stack,
    // а в края на memory[] и растат надолу — като стека на истински процесор.
    // Така програма, която пише извън буфера си, може да ги презапише.
    bool unsafe;
    int rsp; // индекс в memory[] на последния записан адрес (MEMORY_MAX = празно)
} VM;

// Чете little-endian int32_t байт по байт (без неподравнен достъп)
static inline int32_t read_i32_le(const uint8_t *buf) {
    uint32_t u = ((uint32_t)buf[0])       |
                 ((uint32_t)buf[1] << 8)  |
                 ((uint32_t)buf[2] << 16) |
                 ((uint32_t)buf[3] << 24);
    return (int32_t)u;
}

// Знакова аритметика през uint32_t (избягва signed overflow UB)
static inline int32_t safe_add_i32(int32_t a, int32_t b) {
    return (int32_t)((uint32_t)a + (uint32_t)b);
}

static inline int32_t safe_sub_i32(int32_t a, int32_t b) {
    return (int32_t)((uint32_t)a - (uint32_t)b);
}

static inline int32_t safe_mul_i32(int32_t a, int32_t b) {
    return (int32_t)((uint32_t)a * (uint32_t)b);
}

void vm_init(VM *vm, const uint8_t *code, size_t code_size, bool trace, bool unsafe);
VMStatus vm_step(VM *vm);
VMStatus vm_run(VM *vm);

#endif // VM_H
