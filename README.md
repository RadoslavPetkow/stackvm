# stackvm

A small stack-based virtual machine and a two-pass assembler, written in C11 with no dependencies.

- **`asm`** translates a text assembly file into a compact bytecode file.
- **`vm`** loads the bytecode and executes it, with an optional step-by-step trace.
- **`--unsafe`** mode moves return addresses into program memory, so a classic
  buffer overflow can hijack control flow. The default mode makes the same attack impossible.

## Build and run

```sh
sh run.sh examples/fib.asm            # prints 55
sh run.sh examples/fact.asm --trace   # shows the stack after every instruction
```

Or by hand:

```sh
clang -Wall -Wextra -Werror -std=c11 asm.c -o asm
clang -Wall -Wextra -Werror -std=c11 vm.c main.c -o vm
./asm examples/gcd.asm gcd.bin
./vm [--trace] [--unsafe] gcd.bin
```

## Example

```asm
; factorial of 5
PUSH 5
CALL fact
PRINT
HALT

fact:
DUP
PUSH 1
CMPEQ
JZ recurse
RET

recurse:
DUP
PUSH 1
SUB
CALL fact
MUL
RET
```

With `--trace`, each line shows the instruction address, the instruction, and the data stack after it ran:

```text
[0000] PUSH   5      | SP:1 [5]
[0005] CALL   12     | SP:1 [5]
[0012] DUP           | SP:2 [5 5]
...
[0037] MUL           | SP:2 [5 24]
[0038] RET           | SP:2 [5 24]
[0037] MUL           | SP:1 [120]
[0038] RET           | SP:1 [120]
120
```

## Architecture

| Part | Size | Notes |
|---|---|---|
| Data stack | 256 × `int32_t` | operands and results |
| Call stack | 64 return addresses | separate from data in the default mode |
| Memory | 1024 × `int32_t` | `LOAD` / `STORE` |
| Code | loaded from file | read-only, addressed by byte offset |

Every instruction is a 1-byte opcode. Instructions with an operand are followed by a 4-byte little-endian `int32_t`.

| Instruction | Operand | Effect |
|---|---|---|
| `HALT` | | stop |
| `PUSH n` | value or label | push a constant |
| `POP`, `DUP`, `SWAP` | | stack manipulation |
| `ADD`, `SUB`, `MUL` | | pop `b`, pop `a`, push `a op b` (wraps on overflow) |
| `CMPEQ`, `CMPLT`, `CMPGT` | | pop `b`, pop `a`, push 1 or 0 |
| `JMP addr` | label | jump |
| `JZ addr` | label | pop a value, jump if it is 0 |
| `CALL addr`, `RET` | label | subroutine call and return |
| `LOAD i`, `STORE i` | index | read / write `memory[i]` |
| `LOADI`, `STOREI` | | same, but the index comes from the stack |
| `PRINT` | | pop and print |

The bytecode file starts with an 8-byte header: the bytes `VM01`, then the code size as a little-endian `uint32_t`.

### Assembler

Pass 1 walks the source, tracks the byte offset of each instruction, and stores labels in a singly linked list.
Pass 2 emits bytecode into a growable buffer and replaces label operands with their addresses.
Errors (unknown instruction, duplicate or undefined label, missing operand) are reported with the line number.

### Safety checks

The VM validates every step and stops with a specific error instead of invoking undefined behaviour:
stack overflow and underflow, call-stack overflow and underflow, out-of-range memory index,
jump outside the code, truncated operand, unknown opcode. Operands are read byte by byte
(no unaligned access), and arithmetic is done in `uint32_t` to avoid signed overflow.

## The overflow demo

`examples/overflow.asm` contains a function that copies 9 input values into an 8-cell buffer.

```sh
sh run.sh examples/overflow.asm            # prints 1
sh run.sh examples/overflow.asm --unsafe   # prints 1337
```

In the default mode, return addresses live on a separate call stack that program code cannot address,
so the extra write lands in ordinary memory and the function returns normally.

With `--unsafe`, return addresses are stored at the top of `memory[]` and grow downward, like a native stack.
The buffer sits directly below the return address, so the ninth write replaces it with the address of
`secret`, a function nothing calls. The trace shows the moment it happens:

```text
[0074] ADD           | SP:2 [98 1023] | ret@mem[1023]=25
[0075] STOREI        | SP:0 []        | ret@mem[1023]=98
...
[0097] RET           | SP:0 []
[0098] PUSH   1337   | SP:1 [1337]
1337
```

Every individual memory access in that run is in bounds. The bug is in the program's logic, and the
machine's layout decides whether it is exploitable. That is the same reason shadow stacks and
split-stack designs exist on real hardware.

## Files

| File | Purpose |
|---|---|
| `opcodes.h` | instruction table shared by the assembler and the VM |
| `vm.h`, `vm.c` | the machine: state, `vm_step`, trace output |
| `main.c` | loads a `.bin` file and runs it |
| `asm.c` | the two-pass assembler |
| `examples/` | `sum`, `fact`, `fib`, `gcd`, `overflow` |
