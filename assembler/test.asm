.include "test_include.asm"
loop2:
CMPI R0, 8
JE loop1
INC R0
JUMP loop2
loop1:
STORE R0, R1, 0