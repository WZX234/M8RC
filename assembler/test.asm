# bubble_sort.asm —— 8 位冒泡排序
# 数据存放在内存 0x10 到 0x14
# 不依赖 CALL 和 SYSCALL

        ADDI R0, 16, R0     # R0 = 基址 0x10

        # 初始化内存（乱序）
        ADDI R1, 50, R1     # 50
        STORE R1, R0, 0
        ADDI R1, 10, R1     # 10
        STORE R1, R0, 1
        ADDI R1, 40, R1     # 40
        STORE R1, R0, 2
        ADDI R1, 20, R1     # 20
        STORE R1, R0, 3
        ADDI R1, 30, R1     # 30
        STORE R1, R0, 4

        # 循环计数器：排 4 遍（n-1 遍）
        ADDI R3, 4, R3

OUTER:
        # --- 比较 0 和 1 ---
        LOAD R0, 0, R1      # R1 = [0x10]
        LOAD R0, 1, R2      # R2 = [0x11]
        CMP R1, R2          # 比较 R1 和 R2
        JCF SWAP01          # 如果 R1 < R2，产生借位(CF=1)，说明已经升序，跳过交换
        NOP
        JUMPF NEXT01
        NOP
SWAP01:
        STORE R2, R0, 0     # [0x10] = R2
        STORE R1, R0, 1     # [0x11] = R1
NEXT01:

        # --- 比较 1 和 2 ---
        LOAD R0, 1, R1
        LOAD R0, 2, R2
        CMP R1, R2
        JCF SWAP12
        NOP
        JUMPF NEXT12
        NOP
SWAP12:
        STORE R2, R0, 1
        STORE R1, R0, 2
NEXT12:

        # --- 比较 2 和 3 ---
        LOAD R0, 2, R1
        LOAD R0, 3, R2
        CMP R1, R2
        JCF SWAP23
        NOP
        JUMPF NEXT23
        NOP
SWAP23:
        STORE R2, R0, 2
        STORE R1, R0, 3
NEXT23:

        # --- 比较 3 和 4 ---
        LOAD R0, 3, R1
        LOAD R0, 4, R2
        CMP R1, R2
        JCF SWAP34
        NOP
        JUMPF NEXT34
        NOP
SWAP34:
        STORE R2, R0, 3
        STORE R1, R0, 4
NEXT34:

        # 外层循环控制
        SUBI R3, 1, R3
        CMPI R3, 0
        JEF DONE
        NOP
        JUMPB OUTER
        NOP

DONE:
        HLT