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
#define MAX_INCLUDES 32 // 最大包含文件数量

#define MAX_LINE_LENGTH 256 // 每行的最大长度
#define MAX_LINE 1024  // 每行的最大长度(用于处理包含文件)

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

char included_files[MAX_INCLUDES][256];
int include_count = 0;
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
        // 处理宏定义和包含文件
        if (trimmed[0] == '.') {
            if (strncmp(trimmed, ".define", 7) == 0) {
                handle_define(trimmed);
                continue;  // 不写入输出文件
            }
            if (strncmp(trimmed, ".include", 8) == 0) {
                handle_include(trimmed, out, 0);
                continue;
            }
        }
        // 处理宏展开
        if (!expand_line(trimmed, out)) {
            // 不是宏，原样写入 out
            fputs(trimmed, out);
            fputc('\n', out);
        }
    }
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
    strcpy(m->name, name);
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
    
    // 提取宏值(整行，含 ; 分隔的多条指令)
    strcpy(m->value, p);
    
    macro_count++;
}

int handle_include(const char *line, FILE *out, int depth) {
    // 1. 深度检查
    if (depth > 16) {
        fprintf(stderr, RED"Error:"RESET" Include depth exceeded\n");
        return 0;
    }
    
    // 2. 解析文件名
    char filename[256] = "";
    if (sscanf(line, ".include \"%[^\"]\"", filename) != 1) {
        fprintf(stderr, RED"Error:"RESET" Invalid include syntax: %s\n", line);
        return 0;
    }
    
    // 3. 循环包含检查
    for (int i = 0; i < include_count; i++) {
        if (strcmp(included_files[i], filename) == 0) {
            fprintf(stderr, RED"Error:"RESET" Circular include: %s\n", filename);
            return 0;
        }
    }
    
    // 4. 记录已包含文件
    if (include_count < MAX_INCLUDES) {
        strcpy(included_files[include_count], filename);
        include_count++;
    } else {
        fprintf(stderr, RED"Error:"RESET" Too many includes\n");
        return 0;
    }
    
    // 5. 打开文件
    FILE *inc = fopen(filename, "r");
    if (!inc) {
        fprintf(stderr, RED"Error:"RESET" Cannot open include: %s\n", filename);
        include_count--;  // 回退记录
        return 0;
    }
    
    // 6. 逐行处理
    char buf[MAX_LINE];
    while (fgets(buf, sizeof(buf), inc)) {
        // 检测截断
        size_t len = strlen(buf);
        if (len == sizeof(buf) - 1 && buf[len-1] != '\n') {
            fprintf(stderr, RED"Error:"RESET" Line too long in %s\n", filename);
            int c;
            while ((c = fgetc(inc)) != '\n' && c != EOF);
            continue;
        }
        
        // 去换行
        buf[strcspn(buf, "\r\n")] = '\0';
        
        // 去注释
        char *comment = strchr(buf, '#');
        if (comment) *comment = '\0';
        
        // 去空格
        char *trimmed = trim(buf);
        if (trimmed[0] == '\0') continue;
        
        // 预处理指令
        if (trimmed[0] == '.') {
            if (strncmp(trimmed, ".define", 7) == 0) {
                handle_define(trimmed);
                continue;
            }
            if (strncmp(trimmed, ".include", 8) == 0) {
                handle_include(trimmed, out, depth + 1);
                continue;
            }
        }
        
        // 宏展开
        int r = expand_line(trimmed, out);
        if (r < 0) {
            // 不是宏，原样输出
            fputs(trimmed, out);
            fputc('\n', out);
        }
    }
    
    fclose(inc);
    
    // 7. 处理完毕，从列表中移除
    include_count--;
    
    return 1;
}