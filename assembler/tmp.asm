.file "test.asm"
.line 1
.file "test_include.asm"
.line 1
.file "test.asm"
.line 2
loop2:
.line 3
CMPI R0, 8
.line 4
JE loop1
.line 5
.file "test_include.asm"
.line 1
ADDI R0, 1, R0 :NOP
.line 6
JUMP loop2
.line 7
loop1:
.line 8
STORE R0, R1, 0
