#!/bin/sh
# Компилира всичко и пуска една програма:
#   sh run.sh examples/fib.asm [--trace] [--unsafe]
set -e
clang -Wall -Wextra -Werror -std=c11 asm.c -o asm
clang -Wall -Wextra -Werror -std=c11 vm.c main.c -o vm
prog="$1"
shift
./asm "$prog" /tmp/stackvm_prog.bin
./vm "$@" /tmp/stackvm_prog.bin
