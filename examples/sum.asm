; Сума от 1 до 5. Очакван изход: 15
; memory[0] = sum, memory[1] = counter
PUSH 0
STORE 0
PUSH 5
STORE 1

loop:
LOAD 1
JZ done

LOAD 0
LOAD 1
ADD
STORE 0

LOAD 1
PUSH 1
SUB
STORE 1
JMP loop

done:
LOAD 0
PRINT
HALT
