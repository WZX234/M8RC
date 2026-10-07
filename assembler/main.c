#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

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

#define INSTRUCTION_SIZE 1  // 每条指令占用的字节数（16位 = 2字节）

#define MAX_MACROS   256  // 最大宏定义数量
#define MAX_PARAMS   8    // 最大宏参数数量
#define MAX_NAME     64   // 最大宏名长度
#define MAX_VALUE    512  // 最大宏值长度
#define MAX_INCLUDES 32 // 最大包含文件数量

#define MAX_LINE_LENGTH 256 // 每行的最大长度
#define MAX_LINE   1024 // 每行的最大长度(用于处理包含文件)

#define MAX_LABELS 256  // 最大标签数量

// 报错级别
#define LEVEL_ERROR   0
#define LEVEL_WARNING 1
#define LEVEL_NOTICE  2

// 格式类型
#define FMT_NONE    0
#define FMT_RD      1
#define FMT_RR      2
#define FMT_RRI     3
#define FMT_IMM8    4
#define FMT_STORE   5
#define FMT_CALL    6
#define FMT_SYSCALL 7

typedef struct {
    const char *mnemonic;
    uint16_t opcode;
    uint16_t ext;
    int format;
} Instruction;

Instruction instructions[] = {
    {"NOP",    0x0, 0x0, FMT_NONE},
    {"ADDI",   0x1, 0x0, FMT_RRI},
    {"SUBI",   0x2, 0x0, FMT_RRI},
    {"ANDI",   0x3, 0x0, FMT_RRI},
    {"ORI",    0x4, 0x0, FMT_RRI},
    {"XORI",   0x5, 0x0, FMT_RRI},
    {"CMPI",   0x6, 0x0, FMT_RRI},
    {"LOAD",   0x7, 0x0, FMT_RRI},
    {"STORE",  0x8, 0x0, FMT_STORE},
    {"CALLF",  0x9, 0x0, FMT_CALL},
    {"CALLB",  0xA, 0x0, FMT_CALL},
    {"ADD",    0xB, 0x0, FMT_RR},
    {"ADC",    0xB, 0x1, FMT_RR},
    {"SUB",    0xB, 0x2, FMT_RR},
    {"SBC",    0xB, 0x3, FMT_RR},
    {"AND",    0xB, 0x4, FMT_RR},
    {"OR",     0xB, 0x5, FMT_RR},
    {"XOR",    0xB, 0x6, FMT_RR},
    {"RIGHT",  0xB, 0x7, FMT_RD},
    {"CMP",    0xB, 0x8, FMT_RR},
    {"JUMPF",  0xC, 0x0, FMT_IMM8},
    {"JUMPB",  0xC, 0x1, FMT_IMM8},
    {"JCF",    0xC, 0x2, FMT_IMM8},
    {"JCB",    0xC, 0x3, FMT_IMM8},
    {"JEF",    0xC, 0x4, FMT_IMM8},
    {"JEB",    0xC, 0x5, FMT_IMM8},
    {"JROF",   0xC, 0x6, FMT_IMM8},
    {"JROB",   0xC, 0x7, FMT_IMM8},
    {"JUMRF",  0xC, 0x8, FMT_RD},
    {"JUMRB",  0xC, 0x9, FMT_RD},
    {"EXPC",   0xC, 0xA, FMT_RD},
    {"SYSCALL",0xD, 0x0, FMT_SYSCALL},
    {"RETI",   0xE, 0x0, FMT_NONE},
    {"HLT",    0xF, 0x0, FMT_NONE},
};

int instruction_count = sizeof(instructions) / sizeof(instructions[0]);

// 宏定义结构体
typedef struct {
    char name[MAX_NAME];               // 宏名，如 "INC"
    char params[MAX_PARAMS][MAX_NAME]; // 参数名，如 ["r"]
    int param_count;                   // 参数个数，如 1
    char value[MAX_VALUE];             // 宏值，如 "ADDI r, 1"
} Macro;

// 标签结构体
typedef struct {
    char name[MAX_NAME];  // 标签名，如 "LOOP"
    int addr;             // 标签地址，如 0x100
} Label;

char  *trim(char *s);
void  preprocess(FILE *in, FILE *out);
void  handle_define(const char *line);
Macro *find_macro(const char *name);
void  replace_word(char *result, const char *word, const char *replacement);
int   expand_line(const char *line, FILE *out);
int   handle_include(const char *line, FILE *out, int depth);
int   expand_param_macro(const char *line, Macro *m, FILE *out);
int   replace_all_macros(char *line);
void  add_label(const char *name, int addr);
int   find_label(const char *name);
int   pass1(FILE *in);
int   is_label(const char *line);
int   instruction_size(const char *line);
char  *extract_label(const char *line);
void  error_msg(const char *filename, int line_num, const char *source,
               int col, int len, int level, const char *fmt, ...);
Instruction *find_instruction(const char *mnemonic);
void  parse_operands(const char *s, const char *source,
                    char args[MAX_PARAMS][MAX_NAME], int cols[MAX_PARAMS],
                    int *count);
int   parse_register(const char *s, const char *filename, int line_num,
                   const char *source, int col);
int   parse_immediate(const char *s, const char *filename, int line_num,
                    const char *source, int col);
const char *resolve_loop_jump(const char *mnemonic, const char *operand,
                              int current_addr);
uint16_t encode(Instruction *inst, const char *operands,
                const char *filename, int line_num, const char *source);
int   pass2(FILE *in, FILE *out, const char *filename);

//=========================== 全局变量 ===========================

// 宏定义表
Macro macros[MAX_MACROS];
int macro_count = 0;

// 标签表
Label labels[MAX_LABELS];
int label_count = 0;

// 包含文件栈
char included_files[MAX_INCLUDES][256];
int include_count = 0;

//=========================== 日志级别 ===========================
const char *level_names[] = {"Error", "Warning", "Notice"};
const char *level_colors[] = {RED, YELLOW, CYAN};

// 当前源文件名（用于报错）
char *source_filename = NULL;

//=========================== 命令行参数 ===========================
char *input = NULL;     // 输入文件名
char *output = "a.out"; // 输出文件名
char *format = "bin";   // 输出格式
char *listing = NULL;   // 汇编列表文件名
int quiet = 0;          // 安静模式标志

int main (int argc, char *argv[])
{
    //=========================================== 参数解析 =========================================

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
    FILE *tmp = fopen("tmp.asm", "w+");
    if (!tmp) {
        fprintf(stderr, RED"Error:"RESET" Cannot create tmp.asm\n");
        fclose(input_file);
        return 1;
    }

    // 运行预处理器
    preprocess(input_file, tmp);

    // 运行第一遍扫描
    rewind(tmp);
    pass1(tmp);

    // 运行第二遍扫描
    rewind(tmp);
    FILE *out = fopen(output, "w");
    if (!out) {
        fprintf(stderr, RED"Error:"RESET" Cannot create %s\n", output);
        fclose(tmp);
        fclose(input_file);
        return 1;
    }
    pass2(tmp, out, input);
    fclose(out);
    
    // 关闭输入文件和临时文件
    fclose(input_file);
    fclose(tmp);
    
    return 0;
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
        // 宏展开
        int r = expand_line(trimmed, out);
        if (r < 0) {
            // 不是宏，原样输出
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

int handle_include(const char *line, FILE *out, int depth)
{
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
    
    // 3. 循环包含检查（只检查当前递归路径）
    for (int i = 0; i < include_count; i++) {
        if (strcmp(included_files[i], filename) == 0) {
            fprintf(stderr, RED"Error:"RESET" Circular include: %s\n", filename);
            return 0;
        }
    }
    
    // 4. 记录已包含文件（压栈）
    if (include_count >= MAX_INCLUDES) {
        fprintf(stderr, RED"Error:"RESET" Too many includes\n");
        return 0;
    }
    strcpy(included_files[include_count], filename);
    include_count++;
    
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
            fputs(trimmed, out);
            fputc('\n', out);
        }
    }
    
    fclose(inc);
    
    // 7. 弹栈
    include_count--;
    
    return 1;
}
// 展开一行
// 返回值：
//   >= 0：是宏，返回写入的行数
//   -1：不是宏，未写入
int expand_line(const char *line, FILE *out)
{
    // 提取第一个词
    char first[MAX_NAME];
    int i = 0;
    while (line[i] && line[i] != ' ' && line[i] != '\t' && i < MAX_NAME-1) {
        first[i] = line[i];
        i++;
    }
    first[i] = '\0';
    
    // 查找宏
    Macro *m = find_macro(first);
    
    // 第一步：行首是带参宏？
    if (m && m->param_count > 0) {
        return expand_param_macro(line, m, out);
    }
    
    // 第二步：行首是无参宏？
    if (m && m->param_count == 0) {
        char result[MAX_VALUE];
        strcpy(result, line);
        
        int total = 0;
        for (int pass = 0; pass < 32; pass++) {
            int changed = replace_all_macros(result);
            if (changed == 0) break;
            total += changed;
        }
        
        fputs(result, out);
        fputc('\n', out);
        return 1;
    }

    // 第三步：行首不是宏，但行内可能有宏
    char result[MAX_VALUE];
    strcpy(result, line);

    int total = 0;
    for (int pass = 0; pass < 32; pass++) {
        int changed = replace_all_macros(result);
        if (changed == 0) break;
        total += changed;
    }

    if (total > 0) {
        fputs(result, out);
        fputc('\n', out);
        return 1;
    }

    return -1;
}

// 替换宏参数
void replace_word(char *result, const char *word, const char *replacement)
{
    char temp[MAX_VALUE];
    char *p = result;
    char *out = temp;
    
    while (*p) {
        if (strncmp(p, word, strlen(word)) == 0) {
            int before_ok = (p == result || !isalnum((unsigned char)*(p-1)));
            int after_ok = !isalnum((unsigned char)*(p + strlen(word)));
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

// 查找宏定义
Macro *find_macro(const char *name) {
    for (int i = 0; i < macro_count; i++) {
        if (strcmp(macros[i].name, name) == 0) {
            return &macros[i];
        }
    }
    return NULL;
}

char *trim(char *s) {
    // 去掉首部空格和制表符
    while (*s == ' ' || *s == '\t') s++;
    
    // 如果全是空格，直接返回
    if (*s == '\0') return s;
    
    // 去掉尾部空格和制表符
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }
    
    return s;
}

// 展开带参宏
// line: "LEF R0"
// m: 宏定义
// out: 输出文件
// 返回：写入的行数
int expand_param_macro(const char *line, Macro *m, FILE *out)
{
    // 跳过宏名
    const char *p = line;
    while (*p && *p != ' ' && *p != '\t') p++;
    while (*p == ' ' || *p == '\t') p++;
    
    // 解析参数
    char args[MAX_PARAMS][MAX_NAME];
    int arg_count = 0;
    while (*p && arg_count < MAX_PARAMS) {
        int j = 0;
        while (*p && *p != ',' && j < MAX_NAME-1) {
            args[arg_count][j++] = *p++;
        }
        args[arg_count][j] = '\0';
        // 去掉尾部空格
        while (j > 0 && args[arg_count][j-1] == ' ') {
            args[arg_count][--j] = '\0';
        }
        arg_count++;
        if (*p == ',') p++;
    }
    
    // 按 ; 切分宏值，逐条展开
    char value_copy[MAX_VALUE];
    strcpy(value_copy, m->value);
    
    int written = 0;
    char *segment = strtok(value_copy, ";");
    while (segment) {
        char *trimmed_seg = trim(segment);
        if (trimmed_seg[0] != '\0') {
            char expanded[MAX_VALUE];
            strcpy(expanded, trimmed_seg);
            
            // 替换参数
            for (int k = 0; k < m->param_count && k < arg_count; k++) {
                replace_word(expanded, m->params[k], args[k]);
            }
            
            // 替换无参宏（参数值里可能含宏）
            for (int pass = 0; pass < 32; pass++) {
                if (replace_all_macros(expanded) == 0) break;
            }
            
            fputs(expanded, out);
            fputc('\n', out);
            written++;
        }
        segment = strtok(NULL, ";");
    }
    
    return written;
}

// 替换一行中所有无参宏，返回替换次数
int replace_all_macros(char *line)
{
    char temp[MAX_VALUE];
    strcpy(temp, line);
    
    char *p = temp;
    char *outp = line;
    int changed = 0;
    
    while (*p) {
        // 检查是否是标识符开头
        if (isalpha((unsigned char)*p) || *p == '_') {
            char ident[MAX_NAME];
            int i = 0;
            char *start = p;
            
            // 提取标识符
            while ((isalnum((unsigned char)*p) || *p == '_') && i < MAX_NAME-1) {
                ident[i++] = *p++;
            }
            ident[i] = '\0';
            
            // 查宏表
            Macro *m = find_macro(ident);
            if (m && m->param_count == 0) {
                // 无参宏，替换
                strcpy(outp, m->value);
                outp += strlen(m->value);
                changed++;
            } else {
                // 不是宏，原样拷贝
                while (start < p) *outp++ = *start++;
            }
        } else {
            *outp++ = *p++;
        }
    }
    *outp = '\0';
    
    return changed;
}

// 添加标签
void add_label(const char *name, int addr)
{
    strcpy(labels[label_count].name, name);
    labels[label_count].addr = addr;
    label_count++;
}

// 查找标签地址
int find_label(const char *name)
{
    for (int i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, name) == 0)
            return labels[i].addr;
    }
    return -1;
}

// 第一遍扫描：收集标签地址
// 返回总字节数
int pass1(FILE *in)
{
    int addr = 0;
    char line[MAX_LINE];
    
    while (fgets(line, sizeof(line), in)) {
        // 去掉行尾换行
        line[strcspn(line, "\r\n")] = '\0';
        
        // 去掉注释
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';
        
        // 去掉首尾空格
        char *trimmed = trim(line);
        if (trimmed[0] == '\0') continue;
        
        if (is_label(trimmed)) {
            char *label = extract_label(trimmed);
            add_label(label, addr);
            
            // 检查标签后是否还有指令
            char *rest = strchr(trimmed, ':') + 1;
            rest = trim(rest);
            if (rest[0] != '\0') {
                addr += instruction_size(rest);
            }
            continue;
        }
        
        // 普通指令，地址递增
        addr += instruction_size(trimmed);
    }
    
    return addr;
}

// 检查是否是标签行
// 格式：LABEL: 或 LABEL:
int is_label(const char *line) {
    // 找冒号
    const char *colon = strchr(line, ':');
    if (!colon) return 0;
    
    // 冒号前必须是合法标识符
    for (const char *p = line; p < colon; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_') return 0;
    }
    
    // 冒号前不能为空
    if (colon == line) return 0;
    
    return 1;
}

// 提取标签名
char *extract_label(const char *line) {
    static char label[MAX_NAME];
    const char *colon = strchr(line, ':');
    int len = colon - line;
    if (len >= MAX_NAME) len = MAX_NAME - 1;
    strncpy(label, line, len);
    label[len] = '\0';
    return label;
}

// 返回一条指令占几个字节
int instruction_size(const char *line) {
    // 所有指令都是16位
    return INSTRUCTION_SIZE;
}

// 报错函数
// filename: 源文件名
// line_num: 行号
// source: 源代码行（未经处理）
// col: 错误起始位置（列号，从0开始）
// len: 错误长度（字符数）
// level: 级别
// fmt: 错误信息格式
void error_msg(const char *filename, int line_num, const char *source,
               int col, int len, int level, const char *fmt, ...)
{
    // 安静模式只输出 Error
    if (quiet && level != LEVEL_ERROR) return;
    
    // 第一行：文件名和行号
    fprintf(stderr, "File \"%s\", line %d\n", filename, line_num);
    
    // 第二行：源代码
    fprintf(stderr, "\t%s\n", source);
    
    // 第三行：^ 对齐
    fprintf(stderr, "\t");
    for (int i = 0; i < col; i++) {
        // 制表符按 4 空格算
        if (source[i] == '\t') fprintf(stderr, "    ");
        else fputc(' ', stderr);
    }
    fprintf(stderr, "%s", level_colors[level]);
    for (int i = 0; i < len; i++) {
        fputc('^', stderr);
    }
    fprintf(stderr, "\n");
    
    // 第四行：错误信息
    fprintf(stderr, "%s%s : " RESET, level_colors[level], level_names[level]);
    
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    
    fprintf(stderr, "\n");
    
    // Error 退出
    if (level == LEVEL_ERROR) {
        exit(1);
    }
}

// 查找指令
Instruction *find_instruction(const char *mnemonic) {
    for (int i = 0; i < instruction_count; i++) {
        if (strcmp(instructions[i].mnemonic, mnemonic) == 0)
            return &instructions[i];
    }
    return NULL;
}

// 解析操作数
void parse_operands(const char *s, const char *source,
                    char args[MAX_PARAMS][MAX_NAME], int cols[MAX_PARAMS],
                    int *count)
{
    *count = 0;
    const char *p = s;
    
    while (*p && *count < MAX_PARAMS) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        
        cols[*count] = p - source;
        
        int j = 0;
        while (*p && *p != ',' && j < MAX_NAME-1) {
            args[*count][j++] = *p++;
        }
        args[*count][j] = '\0';
        
        while (j > 0 && args[*count][j-1] == ' ') {
            args[*count][--j] = '\0';
        }
        
        (*count)++;
        if (*p == ',') p++;
    }
}

// 解析寄存器名
int parse_register(const char *s, const char *filename, int line_num,
                   const char *source, int col)
{
    if ((s[0] == 'R' || s[0] == 'r') && s[1] >= '0' && s[1] <= '3' && s[2] == '\0') {
        return s[1] - '0';
    }
    
    error_msg(filename, line_num, source, col, strlen(s), LEVEL_ERROR,
              "The name \"%s\" is not a valid register name.", s);
    return -1;
}

// 解析立即数
int parse_immediate(const char *s, const char *filename, int line_num,
                    const char *source, int col)
{
    char *end;
    long value;
    
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        value = strtol(s, &end, 16);
    } else if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) {
        value = strtol(s + 2, &end, 2);
    } else {
        value = strtol(s, &end, 10);
    }
    
    if (*end != '\0') {
        error_msg(filename, line_num, source, col, strlen(s), LEVEL_ERROR,
                  "The name \"%s\" is not a valid immediate value.", s);
        return -1;
    }
    
    return (int)value;
}

// 检查是否是 loop 形式的跳转
// 返回新的助记符，如果不需要转换返回原助记符
// 如果标签未定义返回 NULL
const char *resolve_loop_jump(const char *mnemonic, const char *operand,
                              int current_addr)
{
    static const struct {
        const char *loop_form;
        const char *forward;
        const char *backward;
    } loop_jumps[] = {
        {"JUMP", "JUMPF", "JUMPB"},
        {"JC",   "JCF",   "JCB"},
        {"JE",   "JEF",   "JEB"},
        {"JRO",  "JROF",  "JROB"},
    };
    
    for (int i = 0; i < 4; i++) {
        if (strcmp(mnemonic, loop_jumps[i].loop_form) == 0) {
            int target = find_label(operand);
            if (target < 0) return NULL;
            
            if (target >= current_addr) {
                return loop_jumps[i].forward;
            } else {
                return loop_jumps[i].backward;
            }
        }
    }
    
    return mnemonic;
}

uint16_t encode(Instruction *inst, const char *operands,
                const char *filename, int line_num, const char *source)
{
    uint16_t code = inst->opcode << 12;
    char args[MAX_PARAMS][MAX_NAME];
    int cols[MAX_PARAMS];
    int arg_count = 0;
    
    parse_operands(operands, source, args, cols, &arg_count);
    
    switch (inst->format) {
        case FMT_NONE:
            code |= inst->ext;
            if (strcmp(inst->mnemonic, "HLT") == 0) {
                code |= 0x0FFF;
            }
            break;
            
        case FMT_RD:
            {
                int rs = parse_register(args[0], filename, line_num, source, cols[0]);
                int rd = parse_register(args[1], filename, line_num, source, cols[1]);
                code |= (rs << 10);
                code |= (rd << 6);
                code |= inst->ext;
            }
            break;
            
        case FMT_RR:
            {
                int rs0 = parse_register(args[0], filename, line_num, source, cols[0]);
                int rs1 = parse_register(args[1], filename, line_num, source, cols[1]);
                int rd  = parse_register(args[2], filename, line_num, source, cols[2]);
                code |= (rs0 << 10);
                code |= (rs1 << 8);
                code |= (rd << 6);
                code |= inst->ext;
            }
            break;
            
        case FMT_RRI:
            {
                int rs0 = parse_register(args[0], filename, line_num, source, cols[0]);
                int imm = parse_immediate(args[1], filename, line_num, source, cols[1]);
                int rd  = parse_register(args[2], filename, line_num, source, cols[2]);
                
                if (imm < 0 || imm > 255) {
                    error_msg(filename, line_num, source, cols[1], strlen(args[1]),
                              LEVEL_ERROR, "Immediate value %d out of range (0-255).", imm);
                }
                
                code |= (rs0 << 10);
                code |= ((imm >> 6) & 0x3) << 8;
                code |= (rd << 6);
                code |= ((imm >> 4) & 0x3) << 4;
                code |= (imm & 0xF);
            }
            break;
            
        case FMT_IMM8:
            {
                int imm;
                if (arg_count == 1 && isalpha((unsigned char)args[0][0])) {
                    int target = find_label(args[0]);
                    if (target < 0) {
                        error_msg(filename, line_num, source, cols[0], strlen(args[0]),
                                  LEVEL_ERROR, "Undefined label: %s", args[0]);
                    }
                    imm = target - line_num;
                } else {
                    imm = parse_immediate(args[0], filename, line_num, source, cols[0]);
                }
                
                if (imm < 0 || imm > 255) {
                    error_msg(filename, line_num, source, cols[0], strlen(args[0]),
                              LEVEL_ERROR, "Immediate value %d out of range (0-255).", imm);
                }
                
                code |= ((imm >> 6) & 0x3) << 10;
                code |= ((imm >> 4) & 0x3) << 8;
                code |= ((imm >> 2) & 0x3) << 6;
                code |= (imm & 0x3) << 4;
                code |= inst->ext;
            }
            break;
            
        case FMT_STORE:
            {
                // 汇编顺序：STORE Rs_1, Rs_0, imm8
                // 硬件顺序：[11:10]=Rs_0, [9:8]=Rs_1
                int rs1 = parse_register(args[0], filename, line_num, source, cols[0]);
                int rs0 = parse_register(args[1], filename, line_num, source, cols[1]);
                int imm = parse_immediate(args[2], filename, line_num, source, cols[2]);
                code |= (rs0 << 10);
                code |= (rs1 << 8);
                code |= ((imm >> 2) & 0x3) << 6;
                code |= (imm & 0x3) << 4;
            }
            break;
            
        case FMT_CALL:
            {
                int rd = parse_register(args[1], filename, line_num, source, cols[1]);
                int imm;
                if (isalpha((unsigned char)args[0][0])) {
                    int target = find_label(args[0]);
                    if (target < 0) {
                        error_msg(filename, line_num, source, cols[0], strlen(args[0]),
                                  LEVEL_ERROR, "Undefined label: %s", args[0]);
                    }
                    imm = target - line_num;
                } else {
                    imm = parse_immediate(args[0], filename, line_num, source, cols[0]);
                }
                
                code |= ((imm >> 6) & 0x3) << 8;
                code |= (rd << 6);
                code |= ((imm >> 4) & 0x3) << 4;
                code |= (imm & 0xF);
            }
            break;
            
        case FMT_SYSCALL:
            {
                int imm = parse_immediate(args[0], filename, line_num, source, cols[0]);
                code |= ((imm >> 6) & 0x3) << 10;
                code |= ((imm >> 4) & 0x3) << 8;
                code |= ((imm >> 2) & 0x3) << 6;
                code |= (imm & 0x3) << 4;
            }
            break;
    }
    
    return code;
}

int pass2(FILE *in, FILE *out, const char *filename)
{
    int addr = 0;
    char line[MAX_LINE];
    int line_num = 0;
    
    while (fgets(line, sizeof(line), in)) {
        line_num++;
        
        // 保存原始行（用于报错）
        char original[MAX_LINE];
        strcpy(original, line);
        original[strcspn(original, "\r\n")] = '\0';
        
        // 去掉行尾换行
        line[strcspn(line, "\r\n")] = '\0';
        
        // 去掉注释
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';
        
        // 去掉首尾空格
        char *trimmed = trim(line);
        if (trimmed[0] == '\0') continue;
        
        // 跳过标签
        if (is_label(trimmed)) {
            char *rest = strchr(trimmed, ':') + 1;
            rest = trim(rest);
            if (rest[0] == '\0') continue;
            trimmed = rest;
        }
        
        // 提取助记符
        char mnemonic[MAX_NAME];
        int i = 0;
        while (trimmed[i] && trimmed[i] != ' ' && trimmed[i] != '\t' && i < MAX_NAME-1) {
            mnemonic[i] = trimmed[i];
            i++;
        }
        mnemonic[i] = '\0';
        
        // 提取操作数
        const char *operands = trimmed + i;
        while (*operands == ' ' || *operands == '\t') operands++;
        
        // 检查 loop 形式
        const char *resolved = resolve_loop_jump(mnemonic, operands, addr);
        if (resolved == NULL) {
            error_msg(filename, line_num, original, 0, strlen(mnemonic),
                      LEVEL_ERROR, "Undefined label: %s", operands);
        }
        
        // 查找指令
        Instruction *inst = find_instruction(resolved);
        if (!inst) {
            error_msg(filename, line_num, original, 0, strlen(mnemonic),
                      LEVEL_ERROR, "Unknown instruction: %s", mnemonic);
        }
        
        // 编码
        uint16_t code = encode(inst, operands, filename, line_num, original);
        
        // 写入输出
        fwrite(&code, sizeof(uint16_t), 1, out);
        
        addr++;
    }
    
    return addr;
}