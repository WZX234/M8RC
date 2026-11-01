.file "c:\Users\dell\Desktop\M8RC\assembler\test.asm"
.line 1
.file "test_include.asm"
.line 1
.file "c:\Users\dell\Desktop\M8RC\assembler\test.asm"
.line 2
loop2:
.line 3
NN
.line 4
CMPI R0, 8
.line 5
JE loop1
.line 6
.file "test_include.asm"
.line 1
ADDI R0, 1, R0 :NOP
.line 7
JUMP loop2
.line 8
loop1:
.line 9
STORE R0, R1, 0
