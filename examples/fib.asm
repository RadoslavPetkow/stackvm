; 10-то число на Фибоначи. Очакван изход: 55
PUSH 10
CALL fib
PRINT
HALT

fib:
DUP
PUSH 2
CMPLT
JZ fib_recurse
RET

fib_recurse:
DUP
PUSH 1
SUB
CALL fib
SWAP
PUSH 2
SUB
CALL fib
ADD
RET
