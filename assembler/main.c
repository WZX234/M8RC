#include <stdio.h>
#include <string.h>

// ANSI color codes
#define RED      "\033[91m"
#define GREEN    "\033[92m"
#define YELLOW   "\033[93m"
#define BLUE     "\033[94m"
#define ORANGE   "\033[38;5;208m"
#define CYAN     "\033[38;5;51m"
#define RESET    "\033[0m"

// information
#define VERSION "0.0.1"

#define MAX_MACROS 256  // 最大宏定义数量
#define MAX_PARAMS 8    // 最大宏参数数量
#define MAX_NAME   64   // 最大宏名长度
#define MAX_VALUE  256  // 最大宏值长度

#define MAX_LINE_LENGTH 256 // 每行的最大长度

typedef struct {
    char name[MAX_NAME];               // 宏名，如 "INC"
    char params[MAX_PARAMS][MAX_NAME]; // 参数名，如 ["r"]
    int param_count;                   // 参数个数，如 1
    char value[MAX_VALUE];             // 宏值，如 "ADDI r, 1"
} Macro;

int main (int argc, char *argv[])
{
    //=========================================== 参数解析 =========================================

    char *input = NULL;     // 输入文件名
    char *output = "a.out"; // 输出文件名
    char *format = "bin";   // 输出格式
    char *listing = NULL;   // 汇编列表文件名
    int quiet = 0;          // 安静模式标志

    for (int i = 1; i < argc; i++)
    {
        // 检查是否请求帮助
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            printf(GREEN"Usage:"RESET" %s <input_file>\n", argv[0]);
            puts(GREEN"Description:"RESET" This program is an assembler that converts assembly code into machine code.");
            puts(GREEN"Options:"RESET);
            puts("  "YELLOW"--help, -h   "RESET" Show this help message and exit");
            puts("  "YELLOW"--version, -v"RESET" Show the version of the assembler and exit");
            puts("  "YELLOW"--input, -i  "RESET" Specify the input assembly file (required)");
            puts("  "YELLOW"--output, -o "RESET" Specify the output file (default: a.out)");
            puts("  "YELLOW"--format, -f "RESET" Specify the output format (default: bin)");
            puts("  "YELLOW"--listing, -l"RESET" Specify the listing file (optional)");
            puts("  "YELLOW"--quiet, -q  "RESET" Enable quiet mode (suppress output messages)");
            return 0;
        }
        
        // 检查是否请求版本信息
        else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0)
        {
            puts(GREEN"Assembler version: "VERSION RESET);
            puts(GREEN"Copyright (C) 2026 WZX234. All rights reserved."RESET);
            return 0;
        }
        
        // 获取输入文件名
        else if (strcmp(argv[i], "--input") == 0 || strcmp(argv[i], "-i") == 0)
        {
            if (i + 1 < argc)
            {
                input = argv[++i];
            }
            else
            {
                fprintf(stderr, RED"Error:"RESET" Missing argument for %s\n", argv[i]);
                return 1;
            }
        }

        // 检查是否指定输出文件名
        else if (strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0)
        {
            if (i + 1 < argc)
            {
                output = argv[++i];
            }
            else
            {
                fprintf(stderr, RED"Error:"RESET" Missing argument for %s\n", argv[i]);
                return 1;
            }
        }

        // 检查是否请求输出格式
        else if (strcmp(argv[i], "--format") == 0 || strcmp(argv[i], "-f") == 0)
        {
            if (i + 1 < argc)
            {
                format = argv[++i];
            }
            else
            {
                fprintf(stderr, RED"Error:"RESET" Missing argument for %s\n", argv[i]);
                return 1;
            }
        }

        // 检查是否请求生成汇编列表文件
        else if (strcmp(argv[i], "--listing") == 0 || strcmp(argv[i], "-l") == 0)
        {
            if (i + 1 < argc)
            {
                listing = argv[++i];
            }
            else
            {
                fprintf(stderr, RED"Error:"RESET" Missing argument for %s\n", argv[i]);
                return 1;
            }
        }

        // 检查是否请求安静模式
        else if (strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "-q") == 0)
        {
            quiet = 1;
        }

        else if (argv[i][0] != '-')
        {
            if (input == NULL) input = argv[i];
            else { fprintf(stderr, RED"Error:"RESET" Multiple input files\n"); return 1; }
        }
        // 检查是否有未知选项
        else
        {
            fprintf(stderr, RED"Error:"RESET" Unknown option: %s\n", argv[i]);
            return 1;
        }
    }
    // 检查是否指定了输入文件
    if (input == NULL)
    {
        fprintf(stderr, RED"Error:"RESET" No input file specified\n");
        fprintf(stderr, "Use "YELLOW"%s --help"RESET" for usage information.\n", argv[0]);
        return 1;
    }

    //=========================================== 文件打开 =========================================

    // 打开输入文件
    FILE *input_file = fopen(input, "r");
    if (!input_file)
    {
        fprintf(stderr, RED"Error:"RESET" Could not open input file: %s\n", input);
        return 1;
    }

    // 打开输出文件
    FILE *output_file = fopen(output, "w");
    if (!output_file)
    {
        fprintf(stderr, RED"Error:"RESET" Could not open output file: %s\n", output);
        fclose(input_file);
        return 1;
    }

    // 打开列表文件
    FILE *lst = NULL;
    if (listing) {
        lst = fopen(listing, "w");
        if (lst) {
            fprintf(lst, "| 地址 | 机器码 | 源码 |\n");
            fprintf(lst, "|:---|:---|:---|\n");
        }
    }
    
    //=========================================== 汇编处理 =========================================
    char line[MAX_LINE_LENGTH]; // 每行的缓冲区
    int line_num = 0;           // 当前行号

    while (fgets(line, sizeof(line), input_file))
    {
        line_num++;
        // 处理这一行
        line[strcspn(line, "\r\n")] = '\0'; // 去掉行尾换行
        char *comment = strchr(line, '#');  // 查找注释的起始位置
        if (comment) *comment = '\0';       // 截断至注释前
        char *trimmed = trim(line);         // 去掉行首和行尾的空格
        if (trimmed[0] == '\0') continue;   // 如果这一行是空的，跳过
    }
    fclose(input_file);
    return 0;
}

char *trim(char *s)
{
    // 去掉开头空格
    while (*s == ' ' || *s == '\t') s++;
    
    // 去掉尾部空格
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }
    
    return s;
}