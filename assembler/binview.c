#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char *argv[])
{
    const char *filename = "a.out";
    if (argc >= 2) filename = argv[1];

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Cannot open %s\n", filename);
        return 1;
    }

    int c;
    int count = 0;

    while ((c = fgetc(fp)) != EOF) {
        // 每行打印 2 个字节（16 位一条指令）
        if (count % 2 == 0) {
            printf("%04X  ", count);
        }

        // 逐位打印
        for (int i = 7; i >= 0; i--) {
            putchar((c & (1 << i)) ? '1' : '0');
        }
        putchar(' ');

        count++;

        if (count % 2 == 0) {
            putchar('\n');
        }
    }

    fclose(fp);

    if (count % 2 != 0) putchar('\n');
    printf("Total: %d bytes\n", count);

    return 0;
}