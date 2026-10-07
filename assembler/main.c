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
#define MAX_VALUE  512  // 最大宏值长度

#define MAX_LINE_LENGTH 256 // 每行的最大长度

char *trim(char *s);
void preprocess(FILE *in, FILE *out);
void handle_define(const char *line);
Macro *find_macro(const char *name);
void replace_word(char *result, const char *word, const char *replacement);

typedef struct {
    char name[MAX_NAME];               // 宏名，如 "INC"
    char params[MAX_PARAMS][MAX_NAME]; // 参数名，如 ["r"]
    int param_count;                   // 参数个数，如 1
    char value[MAX_VALUE];             // 宏值，如 "ADDI r, 1"
} Macro;

Macro macros[MAX_MACROS];
int macro_count = 0;

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

    // 创建临时文件
    FILE *tmp = fopen("tmp.asm", "w");
    if (!tmp) {
        fprintf(stderr, RED"Error:"RESET" Cannot create tmp.asm\n");
        fclose(input_file);
        return 1;
    }
}

void preprocess(FILE *in, FILE *out)
{
    char line[MAX_LINE_LENGTH]; // 每行的缓冲区
    int line_num = 0;           // 当前行号

    while (fgets(line, sizeof(line), in))
    {
        line_num++;
        // 处理这一行
        line[strcspn(line, "\r\n")] = '\0'; // 去掉行尾换行
        char *comment = strchr(line, '#');  // 查找注释的起始位置
        if (comment) *comment = '\0';       // 截断至注释前
        char *trimmed = trim(line);         // 去掉行首和行尾的空格
        if (trimmed[0] == '\0') continue;   // 如果这一行是空的，跳过
    }
    fclose(in);
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

// 处理宏定义
void handle_define(const char *line)
{
    const char *p = line + 7;  // 跳过 ".define"
    while (*p == ' ' || *p == '\t') p++;
    
    // 提取宏名
    char name[MAX_NAME];
    int i = 0;
    while (*p && *p != '(' && *p != ' ' && *p != '\t' && i < MAX_NAME-1) {
        name[i++] = *p++;
    }
    name[i] = '\0';
    
    Macro *m = &macros[macro_count];
    strcpy(m->name, name);  // 复制宏名
    m->param_count = 0;
    
    // 解析参数
    if (*p == '(') {
        p++;
        while (*p && *p != ')') {
            while (*p == ' ' || *p == '\t') p++;
            int j = 0;
            while (*p && *p != ',' && *p != ')' && *p != ' ') {
                m->params[m->param_count][j++] = *p++;
            }
            m->params[m->param_count][j] = '\0';
            m->param_count++;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == ',') p++;
        }
        if (*p == ')') p++;
    }
    
    // 跳过空白
    while (*p == ' ' || *p == '\t') p++;
    
    // 提取宏值（整行，包含 ; 分隔的多条指令）
    strcpy(m->value, p);
    
    macro_count++;
}

// 查找宏定义
Macro *find_macro(const char *name) {
    for (int i = 0; i < macro_count; i++) {
        if (strcmp(macros[i].name, name) == 0)  // 如果宏名匹配
            return &macros[i];
    }
    // 如果没有找到，返回 NULL
    return NULL;
}

// 替换宏中的参数
void replace_word(char *result, const char *word, const char *replacement) {
    char temp[MAX_VALUE]; // 临时缓冲区
    char *p = result;     // 指向原始字符串
    char *out = temp;     // 指向输出缓冲区
    
    while (*p) {
        if (strncmp(p, word, strlen(word)) == 0) {
            int before_ok = (p == result || !isalnum(*(p-1)));
            int after_ok = !isalnum(*(p + strlen(word)));
            if (before_ok && after_ok) {
                strcpy(out, replacement);
                out += strlen(replacement);
                p += strlen(word);
                continue;
            }
        }
        *out++ = *p++;
    }
    *out = '\0';
    strcpy(result, temp);
}

