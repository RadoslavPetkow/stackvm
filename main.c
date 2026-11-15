// Зарежда .bin файл (хедър "VM01" + размер + байткод) и го изпълнява.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"

static const char *status_name(VMStatus s) {
    switch (s) {
        case VM_OK:                  return "OK";
        case VM_HALTED:              return "HALTED";
        case VM_ERR_IP_OOB:          return "ip извън кода (липсва HALT?)";
        case VM_ERR_INVALID_OPCODE:  return "непозната инструкция";
        case VM_ERR_STACK_OVERFLOW:  return "препълване на стека";
        case VM_ERR_STACK_UNDERFLOW: return "празен стек";
        case VM_ERR_CALL_OVERFLOW:   return "твърде дълбока рекурсия (call stack)";
        case VM_ERR_CALL_UNDERFLOW:  return "RET без CALL";
        case VM_ERR_MEM_OOB:         return "LOAD/STORE извън паметта";
        case VM_ERR_BAD_JUMP:        return "скок към невалиден адрес";
    }
    return "?";
}

// Чете целия байткод от файла. Връща malloc-нат буфер или NULL при грешка.
static uint8_t *load_bin(const char *path, size_t *code_size) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        perror(path);
        return NULL;
    }

    uint8_t header[8];
    if (fread(header, 1, sizeof(header), f) != sizeof(header) ||
        memcmp(header, VM_MAGIC_BYTES, 4) != 0) {
        fprintf(stderr, "%s: не е VM01 файл\n", path);
        fclose(f);
        return NULL;
    }

    int32_t size = read_i32_le(header + 4);
    if (size <= 0) {
        fprintf(stderr, "%s: невалиден размер на кода\n", path);
        fclose(f);
        return NULL;
    }

    uint8_t *code = malloc((size_t)size);
    if (code == NULL || fread(code, 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "%s: файлът е по-къс от обявения размер\n", path);
        free(code);
        fclose(f);
        return NULL;
    }

    fclose(f);
    *code_size = (size_t)size;
    return code;
}

int main(int argc, char **argv) {
    bool trace = false;
    bool unsafe = false;
    const char *path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--trace") == 0) trace = true;
        else if (strcmp(argv[i], "--unsafe") == 0) unsafe = true;
        else path = argv[i];
    }
    if (path == NULL) {
        fprintf(stderr, "употреба: %s [--trace] [--unsafe] програма.bin\n", argv[0]);
        return 1;
    }

    size_t code_size = 0;
    uint8_t *code = load_bin(path, &code_size);
    if (code == NULL) return 1;

    VM vm;
    vm_init(&vm, code, code_size, trace, unsafe);
    VMStatus status = vm_run(&vm);

    if (status != VM_HALTED) {
        fprintf(stderr, "VM грешка на адрес %u: %s\n", vm.ip, status_name(status));
    }

    free(code);
    return status == VM_HALTED ? 0 : 1;
}
