#include "vm.h"
#include <stdio.h>
#include <string.h>

void vm_init(VM *vm, const uint8_t *code, size_t code_size, bool trace, bool unsafe) {
    memset(vm, 0, sizeof(*vm));
    vm->code = code;
    vm->code_size = code_size;
    vm->trace = trace;
    vm->unsafe = unsafe;
    vm->rsp = MEMORY_MAX;
}

static void vm_print_trace(const VM *vm, uint32_t instr_ip, Opcode op, int32_t arg) {
    printf("\033[90m[%04u]\033[0m \033[1;36m%-6s\033[0m", instr_ip, OPCODE_TABLE[op].name);
    if (OPCODE_TABLE[op].has_operand) {
        printf(" \033[33m%-6d\033[0m", arg);
    } else {
        printf("       ");
    }

    printf(" | SP:%d [", vm->sp);
    for (int i = 0; i < vm->sp; i++) {
        printf("%d%s", vm->stack[i], (i + 1 < vm->sp) ? " " : "");
    }
    printf("]");

    // В небезопасния режим показваме и адреса на връщане, който лежи в паметта
    if (vm->unsafe && vm->rsp < MEMORY_MAX) {
        printf(" \033[35m| ret@mem[%d]=%d\033[0m", vm->rsp, vm->memory[vm->rsp]);
    }
    printf("\n");
}

VMStatus vm_step(VM *vm) {
    if (vm->ip >= vm->code_size) {
        return VM_ERR_IP_OOB;
    }

    uint32_t start_ip = vm->ip;
    uint8_t raw_op = vm->code[start_ip];

    if (raw_op >= OP_COUNT) {
        return VM_ERR_INVALID_OPCODE;
    }

    Opcode op = (Opcode)raw_op;
    int32_t arg = 0;
    uint32_t next_ip = start_ip + 1;

    if (OPCODE_TABLE[op].has_operand) {
        if (vm->code_size - start_ip < 5) {
            return VM_ERR_IP_OOB;
        }
        arg = read_i32_le(&vm->code[start_ip + 1]);
        next_ip = start_ip + 5;
    }

    switch (op) {
        case OP_HALT:
            vm->ip = next_ip;
            if (vm->trace) vm_print_trace(vm, start_ip, op, arg);
            return VM_HALTED;

        case OP_PUSH:
            if (vm->sp >= STACK_MAX) return VM_ERR_STACK_OVERFLOW;
            vm->stack[vm->sp++] = arg;
            break;

        case OP_POP:
            if (vm->sp < 1) return VM_ERR_STACK_UNDERFLOW;
            vm->sp--;
            break;

        case OP_DUP:
            if (vm->sp < 1) return VM_ERR_STACK_UNDERFLOW;
            if (vm->sp >= STACK_MAX) return VM_ERR_STACK_OVERFLOW;
            vm->stack[vm->sp] = vm->stack[vm->sp - 1];
            vm->sp++;
            break;

        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_CMPEQ:
        case OP_CMPLT:
        case OP_CMPGT: {
            if (vm->sp < 2) return VM_ERR_STACK_UNDERFLOW;
            int32_t b = vm->stack[--vm->sp];
            int32_t a = vm->stack[--vm->sp];
            int32_t res = 0;

            if (op == OP_ADD)        res = safe_add_i32(a, b);
            else if (op == OP_SUB)   res = safe_sub_i32(a, b);
            else if (op == OP_MUL)   res = safe_mul_i32(a, b);
            else if (op == OP_CMPEQ) res = (a == b) ? 1 : 0;
            else if (op == OP_CMPLT) res = (a < b)  ? 1 : 0;
            else if (op == OP_CMPGT) res = (a > b)  ? 1 : 0;

            vm->stack[vm->sp++] = res;
            break;
        }

        case OP_JMP:
            if (arg < 0 || (size_t)arg >= vm->code_size) return VM_ERR_BAD_JUMP;
            next_ip = (uint32_t)arg;
            break;

        case OP_JZ: {
            if (vm->sp < 1) return VM_ERR_STACK_UNDERFLOW;
            if (arg < 0 || (size_t)arg >= vm->code_size) return VM_ERR_BAD_JUMP;
            int32_t val = vm->stack[--vm->sp];
            if (val == 0) {
                next_ip = (uint32_t)arg;
            }
            break;
        }

        case OP_CALL:
            if (arg < 0 || (size_t)arg >= vm->code_size) return VM_ERR_BAD_JUMP;
            if (vm->unsafe) {
                if (vm->rsp <= 0) return VM_ERR_CALL_OVERFLOW;
                vm->memory[--vm->rsp] = (int32_t)next_ip;
            } else {
                if (vm->csp >= CALL_STACK_MAX) return VM_ERR_CALL_OVERFLOW;
                vm->call_stack[vm->csp++] = next_ip;
            }
            next_ip = (uint32_t)arg;
            break;

        case OP_RET:
            if (vm->unsafe) {
                if (vm->rsp >= MEMORY_MAX) return VM_ERR_CALL_UNDERFLOW;
                // Адресът идва от паметта, която програмата може да е презаписала
                int32_t ret = vm->memory[vm->rsp];
                if (ret < 0 || (size_t)ret >= vm->code_size) return VM_ERR_BAD_JUMP;
                vm->rsp++;
                next_ip = (uint32_t)ret;
            } else {
                if (vm->csp <= 0) return VM_ERR_CALL_UNDERFLOW;
                next_ip = vm->call_stack[--vm->csp];
            }
            break;

        case OP_LOAD:
            if (arg < 0 || arg >= MEMORY_MAX) return VM_ERR_MEM_OOB;
            if (vm->sp >= STACK_MAX) return VM_ERR_STACK_OVERFLOW;
            vm->stack[vm->sp++] = vm->memory[arg];
            break;

        case OP_STORE:
            if (arg < 0 || arg >= MEMORY_MAX) return VM_ERR_MEM_OOB;
            if (vm->sp < 1) return VM_ERR_STACK_UNDERFLOW;
            vm->memory[arg] = vm->stack[--vm->sp];
            break;

        case OP_PRINT:
            if (vm->sp < 1) return VM_ERR_STACK_UNDERFLOW;
            printf("%d\n", vm->stack[--vm->sp]);
            break;

        case OP_SWAP: {
            if (vm->sp < 2) return VM_ERR_STACK_UNDERFLOW;
            int32_t tmp = vm->stack[vm->sp - 1];
            vm->stack[vm->sp - 1] = vm->stack[vm->sp - 2];
            vm->stack[vm->sp - 2] = tmp;
            break;
        }

        case OP_LOADI: { // [.., idx] -> [.., memory[idx]]
            if (vm->sp < 1) return VM_ERR_STACK_UNDERFLOW;
            int32_t idx = vm->stack[vm->sp - 1];
            if (idx < 0 || idx >= MEMORY_MAX) return VM_ERR_MEM_OOB;
            vm->stack[vm->sp - 1] = vm->memory[idx];
            break;
        }

        case OP_STOREI: { // [.., value, idx] -> [..], memory[idx] = value
            if (vm->sp < 2) return VM_ERR_STACK_UNDERFLOW;
            int32_t idx = vm->stack[vm->sp - 1];
            if (idx < 0 || idx >= MEMORY_MAX) return VM_ERR_MEM_OOB;
            vm->memory[idx] = vm->stack[vm->sp - 2];
            vm->sp -= 2;
            break;
        }

        default:
            return VM_ERR_INVALID_OPCODE;
    }

    vm->ip = next_ip;
    if (vm->trace) {
        vm_print_trace(vm, start_ip, op, arg);
    }

    return VM_OK;
}

VMStatus vm_run(VM *vm) {
    VMStatus status = VM_OK;
    while ((status = vm_step(vm)) == VM_OK) {
        // върти се до VM_HALTED или грешка
    }
    return status;
}
