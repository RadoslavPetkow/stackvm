; Факториел от 5. Очакван изход: 120
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
