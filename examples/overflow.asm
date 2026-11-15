; Демо: препълване на буфер, което презаписва адреса на връщане.
;
;   sh run.sh examples/overflow.asm            -> 1     (нормално връщане)
;   sh run.sh examples/overflow.asm --unsafe   -> 1337  (отвлечено изпълнение)
;
; Функцията vulnerable копира "вход" в буфер от 8 клетки (memory[1015..1022]),
; без да провери дължината му. Входът е 9 числа. В режим --unsafe адресът на
; връщане лежи точно след буфера, в memory[1023], и деветото число го презаписва.

; --- вход от "нападателя": memory[0..8], дължина в memory[100] ---
; memory[0..7] остават 0 (пълнеж), деветото число е адресът на secret
PUSH secret
STORE 8
PUSH 9
STORE 100

CALL vulnerable
PUSH 1           ; нормалният път: връщаме се тук и печатаме 1
PRINT
HALT

vulnerable:
PUSH 0
STORE 101        ; i = 0

copy:
LOAD 101
LOAD 100
CMPLT            ; i < дължината на входа? (а трябваше: i < 8)
JZ copy_done

LOAD 101
LOADI            ; стойност = memory[i]
LOAD 101
PUSH 1015
ADD              ; индекс = 1015 + i
STOREI           ; buffer[i] = стойност

LOAD 101
PUSH 1
ADD
STORE 101        ; i = i + 1
JMP copy

copy_done:
RET

; Никой не вика тази функция. Стига се дотук само през презаписания адрес.
secret:
PUSH 1337
PRINT
HALT
