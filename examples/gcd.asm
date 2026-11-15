; Най-голям общ делител на 48 и 18 чрез изваждане. Очакван изход: 6
; memory[0] = a, memory[1] = b
PUSH 48
STORE 0
PUSH 18
STORE 1

loop:
LOAD 0
LOAD 1
CMPEQ
JZ differ        ; a != b -> продължаваме
LOAD 0
PRINT
HALT

differ:
LOAD 0
LOAD 1
CMPGT
JZ b_bigger

LOAD 0           ; a > b: a = a - b
LOAD 1
SUB
STORE 0
JMP loop

b_bigger:
LOAD 1           ; b > a: b = b - a
LOAD 0
SUB
STORE 1
JMP loop
