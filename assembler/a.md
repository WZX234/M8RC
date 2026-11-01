addr  code  line  file                    source
0000  1010  00005 test.asm                ADDI R0, 16, R0
0002  1472  00008 test.asm                ADDI R1, 50, R1
0004  8100  00009 test.asm                STORE R1, R0, 0
0006  144a  00010 test.asm                ADDI R1, 10, R1
0008  8101  00011 test.asm                STORE R1, R0, 1
000a  1468  00012 test.asm                ADDI R1, 40, R1
000c  8102  00013 test.asm                STORE R1, R0, 2
000e  1454  00014 test.asm                ADDI R1, 20, R1
0010  8103  00015 test.asm                STORE R1, R0, 3
0012  145e  00016 test.asm                ADDI R1, 30, R1
0014  8104  00017 test.asm                STORE R1, R0, 4
0016  1cc4  00020 test.asm                ADDI R3, 4, R3
0018  ----  00022 test.asm                OUTER:
0018  7040  00024 test.asm                LOAD R0, 0, R1
001a  7081  00025 test.asm                LOAD R0, 1, R2
001c  b608  00026 test.asm                CMP R1, R2
001e  c042  00027 test.asm                JCF SWAP01
0020  0000  00028 test.asm                NOP
0022  c040  00029 test.asm                JUMPF NEXT01
0024  0000  00030 test.asm                NOP
0026  ----  00031 test.asm                SWAP01:
0026  8200  00032 test.asm                STORE R2, R0, 0
0028  8101  00033 test.asm                STORE R1, R0, 1
002a  ----  00034 test.asm                NEXT01:
002a  7041  00037 test.asm                LOAD R0, 1, R1
002c  7082  00038 test.asm                LOAD R0, 2, R2
002e  b608  00039 test.asm                CMP R1, R2
0030  c042  00040 test.asm                JCF SWAP12
0032  0000  00041 test.asm                NOP
0034  c040  00042 test.asm                JUMPF NEXT12
0036  0000  00043 test.asm                NOP
0038  ----  00044 test.asm                SWAP12:
0038  8201  00045 test.asm                STORE R2, R0, 1
003a  8102  00046 test.asm                STORE R1, R0, 2
003c  ----  00047 test.asm                NEXT12:
003c  7042  00050 test.asm                LOAD R0, 2, R1
003e  7083  00051 test.asm                LOAD R0, 3, R2
0040  b608  00052 test.asm                CMP R1, R2
0042  c042  00053 test.asm                JCF SWAP23
0044  0000  00054 test.asm                NOP
0046  c040  00055 test.asm                JUMPF NEXT23
0048  0000  00056 test.asm                NOP
004a  ----  00057 test.asm                SWAP23:
004a  8202  00058 test.asm                STORE R2, R0, 2
004c  8103  00059 test.asm                STORE R1, R0, 3
004e  ----  00060 test.asm                NEXT23:
004e  7043  00063 test.asm                LOAD R0, 3, R1
0050  7084  00064 test.asm                LOAD R0, 4, R2
0052  b608  00065 test.asm                CMP R1, R2
0054  c042  00066 test.asm                JCF SWAP34
0056  0000  00067 test.asm                NOP
0058  c040  00068 test.asm                JUMPF NEXT34
005a  0000  00069 test.asm                NOP
005c  ----  00070 test.asm                SWAP34:
005c  8203  00071 test.asm                STORE R2, R0, 3
005e  8104  00072 test.asm                STORE R1, R0, 4
0060  ----  00073 test.asm                NEXT34:
0060  2cc1  00076 test.asm                SUBI R3, 1, R3
0062  6c00  00077 test.asm                CMPI R3, 0
0064  c044  00078 test.asm                JEF DONE
0066  0000  00079 test.asm                NOP
0068  c281  00080 test.asm                JUMPB OUTER
006a  0000  00081 test.asm                NOP
006c  ----  00083 test.asm                DONE:
006c  ffff  00084 test.asm                HLT
