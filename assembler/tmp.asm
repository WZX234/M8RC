loop2:
CMPI R0, 8
JE loop1
ADDI R0, 1, R0 :NOP
JUMP loop2
loop1:
STORE R0, R1, 0
