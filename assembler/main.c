#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>

// ANSI color codes
#define RED      "\033[91m"
#define GREEN    "\033[92m"
#define YELLOW   "\033[93m"
#define BLUE     "\033[94m"
#define ORANGE   "\033[38;5;208m"
#define CYAN     "\033[38;5;51m"
#define RESET    "\033[0m"

// information
#define VERSION "1.0.0"

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

// 达到此数量后停止继续报错
#define MAX_ERRORS 100

#define INSTRUCTION_BYTES 2          // 每条指令 2 字节
#define LOGISIM_WORDS_PER_LINE 16    // 每行 16 个字
#define LOGISIM_MAX_WORDS 32768      // 64KB / 2 = 32768 个字（logisim 补零到满）

// 指令结构体
typedef struct {
    const char *mnemonic;  // 指令助记符，如 "ADDI"
    uint16_t opcode;       // 操作码，如 0x1
    uint16_t ext;          // 扩展码，如 0x0
    int format;            // 指令格式，如 FMT_RRI
    int arg_count;         // 操作数个数
} Instruction;

//=========================== 指令表 ===========================
Instruction instructions[] = {
    {"NOP",    0x0, 0x0, FMT_NONE,    0},
    {"ADDI",   0x1, 0x0, FMT_RRI,     3},
    {"SUBI",   0x2, 0x0, FMT_RRI,     3},
    {"ANDI",   0x3, 0x0, FMT_RRI,     3},
    {"ORI",    0x4, 0x0, FMT_RRI,     3},
    {"XORI",   0x5, 0x0, FMT_RRI,     3},
    {"CMPI",   0x6, 0x0, FMT_RRI,     2},
    {"LOAD",   0x7, 0x0, FMT_RRI,     3},
    {"STORE",  0x8, 0x0, FMT_STORE,   3},
    {"CALLF",  0x9, 0x0, FMT_CALL,    2},
    {"CALLB",  0xA, 0x0, FMT_CALL,    2},
    {"ADD",    0xB, 0x0, FMT_RR,      3},
    {"ADC",    0xB, 0x1, FMT_RR,      3},
    {"SUB",    0xB, 0x2, FMT_RR,      3},
    {"SBC",    0xB, 0x3, FMT_RR,      3},
    {"AND",    0xB, 0x4, FMT_RR,      3},
    {"OR",     0xB, 0x5, FMT_RR,      3},
    {"XOR",    0xB, 0x6, FMT_RR,      3},
    {"RIGHT",  0xB, 0x7, FMT_RD,      2},
    {"CMP",    0xB, 0x8, FMT_RR,      2},
    {"JUMPF",  0xC, 0x0, FMT_IMM8,    1},
    {"JUMPB",  0xC, 0x1, FMT_IMM8,    1},
    {"JCF",    0xC, 0x2, FMT_IMM8,    1},
    {"JCB",    0xC, 0x3, FMT_IMM8,    1},
    {"JEF",    0xC, 0x4, FMT_IMM8,    1},
    {"JEB",    0xC, 0x5, FMT_IMM8,    1},
    {"JROF",   0xC, 0x6, FMT_IMM8,    1},
    {"JROB",   0xC, 0x7, FMT_IMM8,    1},
    {"JUMRF",  0xC, 0x8, FMT_RD,      1},
    {"JUMRB",  0xC, 0x9, FMT_RD,      1},
    {"EXPC",   0xC, 0xA, FMT_RD,      2},
    {"SYSCALL",0xD, 0x0, FMT_SYSCALL, 1},
    {"RETI",   0xE, 0x0, FMT_NONE,    0},
    {"HLT",    0xF, 0x0, FMT_NONE,    0},
};

int instruction_count = sizeof(instructions) / sizeof(instructions[0]);

// 宏定义结构体
typedef struct {
    char name[MAX_NAME];               // 宏名，如 "INC"
    char params[MAX_PARAMS][MAX_NAME]; // 参数名，如 ["r"]
    int param_count;                   // 参数个数，如 1
    char value[MAX_VALUE];             // 宏值，如 "ADDI r, 1"
    char source_file[256];             // 宏定义所在的文件
    int source_line;                   // 宏定义所在的行
} Macro;

// 标签结构体
typedef struct {
    char name[MAX_NAME];  // 标签名，如 "LOOP"
    int addr;             // 标签地址，如 0x100
} Label;

void  cleanup_tmp(void);
char  *trim(char *s);
void  handle_define(const char *line, const char *current_file, int current_line_num);
Macro *find_macro(const char *name);
int   replace_word(char *result, size_t cap, const char *word, const char *replacement);
int   expand_line(const char *line, FILE *out, const char *filename, int line_num);
int expand_param_macro(const char *line, Macro *m, FILE *out,
                       const char *filename, int line_num);
int   replace_all_macros(char *line, size_t cap, int *truncated);
int   add_label(const char *name, int addr);
int   find_label(const char *name);
int   pass1(FILE *in);
int   is_label(const char *line);
int   instruction_size(const char *line);
char  *extract_label(const char *line);
void  error_msg(const char *filename, int line_num, const char *source,
               int col, int len, int level, const char *fmt, ...);
Instruction *find_instruction(const char *mnemonic);
void  parse_operands(const char *s, int base, const char *source,
                    char args[MAX_PARAMS][MAX_NAME], int cols[MAX_PARAMS],
                    int *count);
int   parse_register(const char *s, const char *filename, int line_num,
                   const char *source, int col);
int   parse_immediate(const char *s, const char *filename, int line_num,
                    const char *source, int col);
const char *resolve_loop_jump(const char *mnemonic, const char *operand,
                              int current_addr);
int   encode(Instruction *inst, const char *operands, int addr, int base,
           const char *filename, int line_num, const char *source,
           uint16_t *code_out);
int  pass2(FILE *in, const char *filename);
int  is_backward_jump(const char *mnemonic);
int  safe_strcpy(char *dest, const char *src, size_t dest_size);
int  is_valid_identifier(const char *s);
int  is_absolute_path(const char *path);
int  is_path_sep(char c);
void unify_separators(char *path);
int  normalize_path(const char *in, char *out, size_t out_size);
int  get_directory(const char *file_path, char *dir, size_t dir_size);
int resolve_include_path(const char *base_dir, const char *include_name,
                         char *out_path, size_t out_size);
int  preprocess(FILE *in, FILE *out, const char *filename, const char *base_dir);
int handle_include(const char *line, FILE *out, int depth,
                   const char *caller_file, int caller_line,
                   const char *base_dir);
static int  ensure_word_capacity(int addr);
static int  record_word(int addr, uint16_t code);
static void free_words(void);
static int  write_bin(FILE *out);
static int  write_hex(FILE *out);
static int  write_logisim(FILE *out);
static int  write_output(FILE *out, const char *format);
static int  is_valid_format(const char *format);
static int  safe_append(char *dst, size_t cap, size_t *used, const char *src);
static int  safe_putc(char *dst, size_t cap, size_t *used, char c);

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

// 预处理输出和报错追踪上下文
char current_filename[256] = "";
int current_line = 0;

// 临时文件名
char tmp_filename[256] = "";

// 错误标志
int has_error = 0;

// 分级计数器
int error_count       = 0;    // Error 计数器
int warning_count     = 0;    // Warning 计数器
int notice_count      = 0;    // Notice 计数器
int werror            = 0;    // -Werror：警告当错误
int fatal_warnings    = 0;    // --fatal-warnings：有警告则退出非 0
int show_warnings     = 1;    // -w/--no-warnings 置 0：抑制警告
int show_notices      = 0;    // -v/--verbose 置 1：显示 Notice
int max_errors        = 100;  // --max-errors N：全局错误上限
int max_errors_per_line = 5;  // 同一行最多报多少条 ERROR

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
int  quiet = 0;          // 安静模式标志

// 输出缓冲：pass2 收集，最后按 format 写出
static uint16_t *g_words = NULL;   // 每条指令的机器码
static int       g_word_count = 0; // 实际条数
static int       g_word_cap   = 0; // 容量

int main(int argc, char *argv[])
{
    // 获取当前时间戳
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    // 生成临时文件名
    snprintf(tmp_filename, sizeof(tmp_filename),
             "tmp_%04d%02d%02d_%02d%02d%02d.asm",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);

    // 注册退出清理
    atexit(cleanup_tmp);

    //=========================================== 参数解析 =========================================

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf(GREEN"Usage:"RESET" %s <input_file>\n", argv[0]);
            puts(GREEN"Description:"RESET" This program is an assembler that converts assembly code into machine code.");
            puts(GREEN"Options:"RESET);
            puts("  "YELLOW"--help, -h          "RESET" Show this help message and exit");
            puts("  "YELLOW"--version, -v       "RESET" Show the version of the assembler and exit");
            puts("  "YELLOW"--input, -i         "RESET" Specify the input assembly file (required)");
            puts("  "YELLOW"--output, -o        "RESET" Specify the output file (default: a.out)");
            puts("  "YELLOW"--format, -f        "RESET" Specify the output format (default: bin)");
            puts("  "YELLOW"--listing, -l       "RESET" Specify the listing file (optional)");
            puts("  "YELLOW"--quiet, -q         "RESET" Enable quiet mode (suppress output messages)");
            puts("  "YELLOW"--verbose           "RESET" Show notice-level messages");
            puts("  "YELLOW"--no-warnings, -w   "RESET" Suppress warning and notice messages");
            puts("  "YELLOW"--Werror            "RESET" Treat warnings as errors");
            puts("  "YELLOW"--fatal-warnings    "RESET" Exit non-zero if any warning is emitted");
            puts("  "YELLOW"--max-errors N      "RESET" Stop reporting after N errors (default: 100)");
            puts("  "YELLOW"--max-errors-per-line N"RESET" Max errors per source line (default: 5)");
            return 0;
        }
        else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            // 注意：-v 是 version，--verbose 才是 verbose；避免歧义
            puts(GREEN"Assembler version: "VERSION RESET);
            puts(GREEN"Copyright (C) 2026 WZX234. All rights reserved."RESET);
            return 0;
        }
        else if (strcmp(argv[i], "--input") == 0 || strcmp(argv[i], "-i") == 0) {
            if (i + 1 < argc) input = argv[++i];
            else { fprintf(stderr, RED"Error:"RESET" Missing argument for %s\n", argv[i]); return 1; }
        }
        else if (strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) output = argv[++i];
            else { fprintf(stderr, RED"Error:"RESET" Missing argument for %s\n", argv[i]); return 1; }
        }
        else if (strcmp(argv[i], "--format") == 0 || strcmp(argv[i], "-f") == 0) {
            if (i + 1 < argc) format = argv[++i];
            else { fprintf(stderr, RED"Error:"RESET" Missing argument for %s\n", argv[i]); return 1; }
        }
        else if (strcmp(argv[i], "--listing") == 0 || strcmp(argv[i], "-l") == 0) {
            if (i + 1 < argc) listing = argv[++i];
            else { fprintf(stderr, RED"Error:"RESET" Missing argument for %s\n", argv[i]); return 1; }
        }
        else if (strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "-q") == 0) {
            quiet = 1;
        }
        else if (strcmp(argv[i], "--verbose") == 0) {
            show_notices = 1;
        }
        else if (strcmp(argv[i], "--no-warnings") == 0 || strcmp(argv[i], "-w") == 0) {
            show_warnings = 0;
            show_notices  = 0;
        }
        else if (strcmp(argv[i], "--Werror") == 0) {
            werror = 1;
        }
        else if (strcmp(argv[i], "--fatal-warnings") == 0) {
            fatal_warnings = 1;
        }
        else if (strcmp(argv[i], "--max-errors") == 0) {
            if (i + 1 < argc) {
                int v = atoi(argv[++i]);
                if (v > 0) max_errors = v;
                else { fprintf(stderr, RED"Error:"RESET" Invalid --max-errors value\n"); return 1; }
            } else {
                fprintf(stderr, RED"Error:"RESET" Missing argument for --max-errors\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "--max-errors-per-line") == 0) {
            if (i + 1 < argc) {
                int v = atoi(argv[++i]);
                if (v > 0) max_errors_per_line = v;
                else { fprintf(stderr, RED"Error:"RESET" Invalid --max-errors-per-line value\n"); return 1; }
            } else {
                fprintf(stderr, RED"Error:"RESET" Missing argument for --max-errors-per-line\n");
                return 1;
            }
        }
        else if (argv[i][0] != '-') {
            if (input == NULL) input = argv[i];
            else { fprintf(stderr, RED"Error:"RESET" Multiple input files\n"); return 1; }
        }
        else {
            fprintf(stderr, RED"Error:"RESET" Unknown option: %s\n", argv[i]);
            return 1;
        }
    }

    if (input == NULL) {
        fprintf(stderr, RED"Error:"RESET" No input file specified\n");
        fprintf(stderr, "Use "YELLOW"%s --help"RESET" for usage information.\n", argv[0]);
        return 1;
    }

    // 检查输出格式
    if (!is_valid_format(format)) {
        fprintf(stderr, RED "Error:" RESET " Unknown format '%s' (supported: bin, hex, logisim)\n", format);
        return 1;
    }

    //=========================================== 文件打开 =========================================

    char abs_input_path[1024];
    if (normalize_path(input, abs_input_path, sizeof(abs_input_path)) != 0) {
        fprintf(stderr, RED "Error:" RESET " Input path too long or invalid: %s\n", input);
        return 1;
    }

    char base_dir[1024];
    if (get_directory(abs_input_path, base_dir, sizeof(base_dir)) != 0) {
        fprintf(stderr, RED "Error:" RESET " Input directory path too long.\n");
        return 1;
    }

    FILE *input_file = fopen(abs_input_path, "r");
    if (!input_file) {
        fprintf(stderr, RED"Error:"RESET" Could not open input file: %s\n", input);
        return 1;
    }

    FILE *tmp = fopen(tmp_filename, "w+");
    if (!tmp) {
        fprintf(stderr, RED"Error:"RESET" Cannot create %s\n", tmp_filename);
        fclose(input_file);
        return 1;
    }

    // 1. 预处理器
    if (!preprocess(input_file, tmp, abs_input_path, base_dir) || has_error) {
        fprintf(stderr, RED"Error:"RESET" Preprocessing failed.\n");
        if (!quiet) {
            printf("%d error(s), %d warning(s), %d notice(s)\n",
                   error_count, warning_count, notice_count);
        }
        fclose(input_file);
        fclose(tmp);
        return 1;
    }

    // 2. 第一遍扫描
    rewind(tmp);
    int total = pass1(tmp);
    if (total < 0) {
        fprintf(stderr, RED"Error:"RESET" First pass failed (fatal).\n");
        if (!quiet) {
            printf("%d error(s), %d warning(s), %d notice(s)\n",
                   error_count, warning_count, notice_count);
        }
        fclose(input_file);
        fclose(tmp);
        return 1;
    }

    // 3. 第二遍扫描（收集，不直接写文件）
    rewind(tmp);
    int n_words = pass2(tmp, abs_input_path);
    if (n_words < 0) {
        fprintf(stderr, RED "Error:" RESET " Assembly failed (pass2 fatal).\n");
        free_words();
        fclose(input_file);
        fclose(tmp);
        return 1;
    }

    fclose(input_file);
    fclose(tmp);

    // 4. 按格式写出
    FILE *out = NULL;
    if (strcmp(format, "bin") == 0) {
        out = fopen(output, "wb");
    } else {
        out = fopen(output, "w");   // hex / logisim 是文本
    }
    if (!out) {
        fprintf(stderr, RED "Error:" RESET " Cannot create %s\n", output);
        free_words();
        return 1;
    }

    if (write_output(out, format) < 0) {
        fprintf(stderr, RED "Error:" RESET " Failed to write output file.\n");
        fclose(out);
        remove(output);
        free_words();
        return 1;
    }

    if (fclose(out) != 0) {
        fprintf(stderr, RED "Error:" RESET " Failed to flush output file.\n");
        remove(output);
        free_words();
        return 1;
    }

    // 5. 汇总
    if (!quiet) {
        printf("%d error(s), %d warning(s), %d notice(s)\n",
            error_count, warning_count, notice_count);
    }

    int failed = (error_count > 0) || (fatal_warnings && warning_count > 0);
    if (failed) {
        if (error_count > 0) {
            fprintf(stderr, RED "Error:" RESET " Assembly failed with %d error(s). "
                    "Output file removed.\n", error_count);
        } else {
            fprintf(stderr, RED "Error:" RESET " Assembly failed: warnings treated as fatal "
                    "(%d warning(s)). Output file removed.\n", warning_count);
        }
        remove(output);
        free_words();
        return 1;
    }

    if (!quiet) {
        printf(GREEN "Success:" RESET " Output written to %s (%s format)\n", output, format);
    }

    free_words();
    return 0;
}

// 预处理器
int preprocess(FILE *in, FILE *out, const char *filename, const char *base_dir)
{
    char line[MAX_LINE];
    int line_num = 0;

    fprintf(out, ".file \"%s\"\n", filename);

    while (fgets(line, sizeof(line), in))
    {
        line_num++;

        size_t len = strlen(line);
        if (len == sizeof(line) - 1 && line[len - 1] != '\n' && !feof(in)) {
            error_msg(filename, line_num, line, 0, strlen(line), LEVEL_ERROR,
                      "Line too long (max %d bytes).", MAX_LINE - 1);
            int c;
            while ((c = fgetc(in)) != '\n' && c != EOF);
            return 0;
        }

        fprintf(out, ".line %d\n", line_num);
        line[strcspn(line, "\r\n")] = '\0';

        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';

        char *trimmed = trim(line);
        if (trimmed[0] == '\0') continue;

        if (trimmed[0] == '.') {
            if (strncmp(trimmed, ".define", 7) == 0) {
                handle_define(trimmed, filename, line_num);
                continue;
            }
            if (strncmp(trimmed, ".include", 8) == 0) {
                // 传实际行号
                if (!handle_include(trimmed, out, 0, filename, line_num, base_dir)) {
                    return 0;
                }
                continue;
            }
        }

        // 传实际文件名 + 行号
        int r = expand_line(trimmed, out, filename, line_num);
        if (r == -1) {
            fputs(trimmed, out);
            fputc('\n', out);
        }
        // r == -2：expand_line 已写截断结果，不再重复
    }
    return (has_error ? 0 : 1);
}

// 处理宏定义
void handle_define(const char *line, const char *current_file, int current_line_num)
{
    if (macro_count >= MAX_MACROS) {
        error_msg(current_file, current_line_num, line, 0, strlen(line), LEVEL_ERROR,
                  "Too many macros (max %d).", MAX_MACROS);
        return;
    }

    const char *p = line + 7;
    while (*p == ' ' || *p == '\t') p++;
    
    char name[MAX_NAME];
    int i = 0;
    while (*p && *p != '(' && *p != ' ' && *p != '\t' && i < MAX_NAME - 1) {
        name[i++] = *p++;
    }
    name[i] = '\0';
    
    if (name[0] == '\0') {
        error_msg(current_file, current_line_num, line, 0, strlen(line), LEVEL_ERROR,
                  "Macro name is empty.");
        return;
    }
    
    Macro *m = &macros[macro_count];
    safe_strcpy(m->name, name, MAX_NAME);
    m->param_count = 0;
    
    if (*p == '(') {
        p++;
        while (*p && *p != ')') {
            if (m->param_count >= MAX_PARAMS) {
                error_msg(current_file, current_line_num, line, 0, strlen(line), LEVEL_ERROR,
                          "Too many macro parameters (max %d).", MAX_PARAMS);
                return;
            }
            while (*p == ' ' || *p == '\t') p++;
            int j = 0;
            while (*p && *p != ',' && *p != ')' && *p != ' ' && j < MAX_NAME - 1) {
                m->params[m->param_count][j++] = *p++;
            }
            m->params[m->param_count][j] = '\0';
            m->param_count++;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == ',') p++;
        }
        if (*p == ')') p++;
    }
    
    while (*p == ' ' || *p == '\t') p++;
    
    if (safe_strcpy(m->value, p, MAX_VALUE) != 0) {
        error_msg(current_file, current_line_num, line, 0, strlen(line), LEVEL_ERROR,
                  "Macro value too long (max %d).", MAX_VALUE);
        return;
    }
    
    safe_strcpy(m->source_file, current_file, sizeof(m->source_file));
    m->source_line = current_line_num;
    
    macro_count++;
}

// caller_file / caller_line 是包含此文件的上一级文件及行号
int handle_include(const char *line, FILE *out, int depth,
                   const char *caller_file, int caller_line,
                   const char *base_dir)
{
    if (depth > 16) {
        error_msg(caller_file, caller_line, line, 0, strlen(line), LEVEL_ERROR,
                  "Include depth exceeded");
        return 0;
    }

    char include_name[256] = "";
    if (sscanf(line, ".include \"%[^\"]\"", include_name) != 1) {
        error_msg(caller_file, caller_line, line, 0, strlen(line), LEVEL_ERROR,
                  "Invalid include syntax");
        return 0;
    }

    // 解析包含文件的完整路径（以调用者所在目录为基准）
    char resolved_path[1024];
    if (resolve_include_path(base_dir, include_name,
                             resolved_path, sizeof(resolved_path)) != 0) {
        error_msg(caller_file, caller_line, line, 0, strlen(line), LEVEL_ERROR,
                  "Include path too long or invalid: %s", include_name);
        return 0;
    }

    // 循环包含检测（用完整路径比较，彻底解决 ./a.asm 和 a.asm 冲突）
    for (int i = 0; i < include_count; i++) {
        if (strcmp(included_files[i], resolved_path) == 0) {
            error_msg(caller_file, caller_line, line, 0, strlen(line), LEVEL_ERROR,
                      "Circular include: %s", resolved_path);
            return 0;
        }
    }

    if (include_count >= MAX_INCLUDES) {
        error_msg(caller_file, caller_line, line, 0, strlen(line), LEVEL_ERROR,
                  "Too many includes");
        return 0;
    }
    safe_strcpy(included_files[include_count], resolved_path, 256);
    include_count++;

    FILE *inc = fopen(resolved_path, "r");
    if (!inc) {
        error_msg(caller_file, caller_line, line, 0, strlen(line), LEVEL_ERROR,
                  "Cannot open include: %s", resolved_path);
        include_count--;
        return 0;
    }

    // 提取被包含文件所在的目录，供嵌套包含使用
    char new_base_dir[1024];
    if (get_directory(resolved_path, new_base_dir, sizeof(new_base_dir)) != 0) {
        error_msg(caller_file, caller_line, line, 0, strlen(line), LEVEL_ERROR,
                  "Included file directory path too long.");
        fclose(inc);
        include_count--;
        return 0;
    }

    // 记录被包含文件的绝对（或规范）路径
    fprintf(out, ".file \"%s\"\n", resolved_path);

    char buf[MAX_LINE];
    int line_num = 0;
    while (fgets(buf, sizeof(buf), inc)) {
        line_num++;

        size_t len = strlen(buf);
        if (len == sizeof(buf) - 1 && buf[len - 1] != '\n' && !feof(inc)) {
            error_msg(resolved_path, line_num, buf, 0, strlen(buf), LEVEL_ERROR,
                      "Line too long (max %d bytes).", MAX_LINE - 1);
            int c;
            while ((c = fgetc(inc)) != '\n' && c != EOF);
            fclose(inc);
            include_count--;
            return 0;
        }

        fprintf(out, ".line %d\n", line_num);
        buf[strcspn(buf, "\r\n")] = '\0';

        char *comment = strchr(buf, '#');
        if (comment) *comment = '\0';
        char *trimmed = trim(buf);
        if (trimmed[0] == '\0') continue;

        if (trimmed[0] == '.') {
            if (strncmp(trimmed, ".define", 7) == 0) {
                handle_define(trimmed, resolved_path, line_num);
                continue;
            }
            if (strncmp(trimmed, ".include", 8) == 0) {
                // 递归传 resolved_path + 当前行号
                if (!handle_include(trimmed, out, depth + 1,
                                    resolved_path, line_num, new_base_dir)) {
                    fclose(inc);
                    include_count--;
                    return 0;
                }
                continue;
            }
        }

        // 传 resolved_path + 当前行号
        int r = expand_line(trimmed, out, resolved_path, line_num);
        if (r == -1) {
            fputs(trimmed, out);
            fputc('\n', out);
        }
        // r == -2：expand_line 已写截断结果，不再重复
    }

    fclose(inc);
    fprintf(out, ".file \"%s\"\n", caller_file);

    include_count--;
    return (has_error ? 0 : 1);
}

// 展开一行
// 返回值：
//   >0 ：是宏，返回写入的行数
//   -1 ：不是宏，未写入
//   -2 ：展开截断，已写出截断结果（调用方不要再写原行）
int expand_line(const char *line, FILE *out, const char *filename, int line_num)
{
    char first[MAX_NAME];
    int i = 0;
    while (line[i] && line[i] != ' ' && line[i] != '\t' && i < MAX_NAME - 1) {
        first[i] = line[i];
        i++;
    }
    first[i] = '\0';

    Macro *m = find_macro(first);

    if (m && m->param_count > 0) {
        // 透传 filename + line_num
        return expand_param_macro(line, m, out, filename, line_num);
    }

    // 用 MAX_LINE 而不是 MAX_VALUE：源行本身可能接近 MAX_LINE
    char result[MAX_LINE];
    if (safe_strcpy(result, line, sizeof(result)) != 0) {
        error_msg(filename, line_num, line, 0, strlen(line), LEVEL_ERROR,
                  "Line too long to expand (max %d bytes).", MAX_LINE - 1);
        return -1;
    }

    int total = 0;
    int truncated = 0;
    for (int pass = 0; pass < 32; pass++) {
        int changed = replace_all_macros(result, sizeof(result), &truncated);
        if (truncated) break;
        if (changed == 0) break;
        total += changed;
    }

    if (truncated) {
        error_msg(filename, line_num, line, 0, strlen(line), LEVEL_ERROR,
                  "Macro expansion exceeds %d bytes (truncated).", MAX_LINE - 1);
        fprintf(out, ".line %d\n", line_num);
        fputs(result, out);
        fputc('\n', out);
        return -2;
    }

    if (total > 0 || m) {
        fprintf(out, ".line %d\n", line_num);
        fputs(result, out);
        fputc('\n', out);
        return 1;
    }

    return -1;
}

// 替换宏参数
// 在 result（容量 cap）中把独立单词 word 替换为 replacement。
// 返回 0 成功；-1 表示结果被截断（内存安全，但语义不完整）。
int replace_word(char *result, size_t cap, const char *word, const char *replacement)
{
    char temp[MAX_LINE];       // 与源行同尺寸
    size_t used = 0;
    int truncated = 0;

    const char *p = result;
    size_t wlen = strlen(word);

    while (*p) {
        if (wlen > 0 && strncmp(p, word, wlen) == 0) {
            int before_ok = (p == result || !isalnum((unsigned char)*(p - 1)));
            int after_ok  = !isalnum((unsigned char)*(p + wlen));
            if (before_ok && after_ok) {
                if (safe_append(temp, sizeof(temp), &used, replacement) < 0) {
                    truncated = 1;
                    break;
                }
                p += wlen;
                continue;
            }
        }
        if (safe_putc(temp, sizeof(temp), &used, *p) < 0) {
            truncated = 1;
            break;
        }
        p++;
    }

    if (safe_strcpy(result, temp, cap) != 0) {
        truncated = 1;
    }
    return truncated ? -1 : 0;
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
// filename / line_num: 宏调用所在文件与行号（用于报错）
// 返回：写入的行数
int expand_param_macro(const char *line, Macro *m, FILE *out,
                       const char *filename, int line_num)
{
    const char *p = line;
    while (*p && *p != ' ' && *p != '\t') p++;
    while (*p == ' ' || *p == '\t') p++;

    char args[MAX_PARAMS][MAX_NAME];
    int arg_count = 0;
    while (*p && arg_count < MAX_PARAMS) {
        int j = 0;
        while (*p && *p != ',' && j < MAX_NAME - 1) {
            args[arg_count][j++] = *p++;
        }
        args[arg_count][j] = '\0';
        while (j > 0 && args[arg_count][j - 1] == ' ') {
            args[arg_count][--j] = '\0';
        }
        arg_count++;
        if (*p == ',') p++;
    }

    // 参数个数错误：用调用方传进来的 filename / line_num
    if (arg_count != m->param_count) {
        error_msg(filename, line_num, line, 0, strlen(line), LEVEL_ERROR,
                  "Macro '%s' expects %d arguments, but got %d.",
                  m->name, m->param_count, arg_count);
        return 0;
    }

    char value_copy[MAX_VALUE];
    if (safe_strcpy(value_copy, m->value, sizeof(value_copy)) != 0) {
        error_msg(filename, line_num, line, 0, strlen(line), LEVEL_ERROR,
                  "Macro value too long (max %d).", MAX_VALUE - 1);
        return 0;
    }

    int written = 0;
    char *segment = strtok(value_copy, ";");
    while (segment) {
        char *trimmed_seg = trim(segment);
        if (trimmed_seg[0] != '\0') {
            char expanded[MAX_LINE];   // 与源行同尺寸，容纳替换后增长
            if (safe_strcpy(expanded, trimmed_seg, sizeof(expanded)) != 0) {
                error_msg(filename, line_num, line, 0, strlen(line), LEVEL_ERROR,
                          "Macro body segment too long (max %d bytes).", MAX_LINE - 1);
                return written;
            }

            for (int k = 0; k < m->param_count && k < arg_count; k++) {
                if (replace_word(expanded, sizeof(expanded),
                                 m->params[k], args[k]) < 0) {
                    error_msg(filename, line_num, line, 0, strlen(line), LEVEL_ERROR,
                              "Macro '%s' expansion truncated (max %d bytes).",
                              m->name, MAX_LINE - 1);
                    // 继续写出截断结果，让 pass 报更多错
                }
            }

            int trunc = 0;
            for (int pass = 0; pass < 32; pass++) {
                int changed = replace_all_macros(expanded, sizeof(expanded), &trunc);
                if (trunc) break;
                if (changed == 0) break;
            }
            if (trunc) {
                error_msg(filename, line_num, line, 0, strlen(line), LEVEL_ERROR,
                          "Macro '%s' expansion truncated (max %d bytes).",
                          m->name, MAX_LINE - 1);
            }

            fprintf(out, ".file \"%s\"\n", m->source_file);
            fprintf(out, ".line %d\n", m->source_line);
            fputs(expanded, out);
            fputc('\n', out);
            written++;
        }
        segment = strtok(NULL, ";");
    }

    return written;
}

// 替换一行中所有无参宏。
// 返回替换次数；*truncated 置 1 表示结果被截断。
int replace_all_macros(char *line, size_t cap, int *truncated)
{
    if (truncated) *truncated = 0;

    char temp[MAX_LINE];
    size_t used = 0;
    int changed = 0;
    int trunc = 0;

    const char *p = line;
    while (*p) {
        if (isalpha((unsigned char)*p) || *p == '_') {
            char ident[MAX_NAME];
            int i = 0;
            const char *start = p;

            while ((isalnum((unsigned char)*p) || *p == '_') && i < MAX_NAME - 1) {
                ident[i++] = *p++;
            }
            ident[i] = '\0';

            Macro *m = find_macro(ident);
            if (m && m->param_count == 0) {
                if (safe_append(temp, sizeof(temp), &used, m->value) < 0) {
                    trunc = 1;
                    break;
                }
                changed++;
            } else {
                // 不是宏，原样拷贝这个 token
                while (start < p) {
                    if (safe_putc(temp, sizeof(temp), &used, *start) < 0) {
                        trunc = 1;
                        break;
                    }
                    start++;
                }
                if (trunc) break;
            }
        } else {
            if (safe_putc(temp, sizeof(temp), &used, *p) < 0) {
                trunc = 1;
                break;
            }
            p++;
        }
    }

    if (safe_strcpy(line, temp, cap) != 0) trunc = 1;
    if (truncated) *truncated = trunc;
    return changed;
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
// 返回总字节数；仅行超长等致命情况返回 -1
int pass1(FILE *in)
{
    int addr = 0;
    char line[MAX_LINE];

    while (fgets(line, sizeof(line), in)) {
        size_t len = strlen(line);
        if (len == sizeof(line) - 1 && line[len - 1] != '\n' && !feof(in)) {
            error_msg(current_filename, current_line, line, 0, strlen(line), LEVEL_ERROR,
                      "Line too long (max %d bytes).", MAX_LINE - 1);
            // 致命：吞掉本行剩余部分，继续扫描以收集更多错误
            int c;
            while ((c = fgetc(in)) != '\n' && c != EOF);
            continue;
        }

        line[strcspn(line, "\r\n")] = '\0';

        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';

        char *trimmed = trim(line);
        if (trimmed[0] == '\0') continue;

        // 处理 .file 和 .line 指令，更新当前文件名和行号
        if (strncmp(trimmed, ".file \"", 7) == 0) {
            sscanf(trimmed, ".file \"%[^\"]\"", current_filename);
            continue;
        }

        if (strncmp(trimmed, ".line ", 6) == 0) {
            sscanf(trimmed, ".line %d", &current_line);
            continue;
        }

        if (is_label(trimmed)) {
            char *label = extract_label(trimmed);
            add_label(label, addr);          // 失败也继续，不 return

            char *rest = strchr(trimmed, ':') + 1;
            rest = trim(rest);
            if (rest[0] != '\0') {
                addr += instruction_size(rest);
            }
            continue;
        }

        // 未知助记符/操作数错误留给 pass2 报，pass1 只推进地址
        addr += instruction_size(trimmed);
    }

    return addr;   // 不再因 has_error 返回 -1
}

// 检查是否是标签行
// 格式：LABEL: 或 LABEL:
int is_label(const char *line) {
    const char *colon = strchr(line, ':');
    if (!colon) return 0;
    if (colon == line) return 0;
    
    // 提取冒号前的部分
    char name[MAX_NAME];
    int len = colon - line;
    if (len >= MAX_NAME) return 0;
    strncpy(name, line, len);
    name[len] = '\0';
    
    // 统一标识符规则
    return is_valid_identifier(name);
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

// 报错函数（支持分级计数、每行去重、全局上限、Werror、级别抑制）
//
// 参数：
//   filename : 源文件名
//   line_num : 行号
//   source   : 源代码行（未经处理，用于对齐 ^）
//   col      : 错误起始列（从 0 开始）
//   len      : 错误长度（字符数）
//   level    : LEVEL_ERROR / LEVEL_WARNING / LEVEL_NOTICE
//   fmt, ... : 消息格式
void error_msg(const char *filename, int line_num, const char *source,
               int col, int len, int level, const char *fmt, ...)
{
    // ---------- 1. 级别抑制 ----------
    if (quiet && level != LEVEL_ERROR) return;
    if (level == LEVEL_WARNING && !show_warnings) return;
    if (level == LEVEL_NOTICE  && !show_notices)  return;

    // ---------- 2. 全局错误上限 ----------
    // 只有 ERROR 受全局上限约束；警告/提示不设上限
    if (level == LEVEL_ERROR && error_count >= max_errors) {
        static int too_many_reported = 0;
        if (!too_many_reported) {
            fprintf(stderr, "%s%s : " RESET
                    "too many errors (limit %d), further errors suppressed\n",
                    level_colors[LEVEL_ERROR], level_names[LEVEL_ERROR], max_errors);
            too_many_reported = 1;
        }
        has_error = 1;
        return;   // 不增加 error_count，避免无限增长
    }

    // ---------- 3. 同一行 ERROR 去重 / 每行上限 ----------
    static const char *last_err_file = NULL;
    static int         last_err_line = -1;
    static int         line_err_cnt  = 0;

    if (level == LEVEL_ERROR) {
        if (filename == last_err_file && line_num == last_err_line) {
            // 同一行
            if (line_err_cnt >= max_errors_per_line) {
                // 仅在该行第一次触达上限时提示
                if (line_err_cnt == max_errors_per_line) {
                    fprintf(stderr, "%s%s : " RESET
                            "too many errors on this line (limit %d)\n",
                            level_colors[LEVEL_ERROR], level_names[LEVEL_ERROR],
                            max_errors_per_line);
                    line_err_cnt++;
                }
                has_error = 1;
                return;
            }
            line_err_cnt++;
        } else {
            // 新行
            last_err_file = filename;
            last_err_line = line_num;
            line_err_cnt  = 1;
        }
    }

    // ---------- 4. 打印 ----------
    int source_len = strlen(source);
    if (col < 0) col = 0;
    if (col > source_len) col = source_len;
    if (len < 1) len = 1;
    if (col + len > source_len) len = source_len - col;

    fprintf(stderr, "File \"%s\", line %d\n", filename, line_num);
    fprintf(stderr, "\t%s\n", source);

    fprintf(stderr, "\t");
    for (int i = 0; i < col; i++) {
        if (source[i] == '\t') fprintf(stderr, "    ");
        else fputc(' ', stderr);
    }
    fprintf(stderr, "%s", level_colors[level]);
    for (int i = 0; i < len; i++) {
        fputc('^', stderr);
    }
    fprintf(stderr, "\n");

    fprintf(stderr, "%s%s : " RESET, level_colors[level], level_names[level]);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");

    // ---------- 5. Werror 升级 ----------
    if (level == LEVEL_WARNING && werror) {
        // 警告升级为错误：计入 error_count
        has_error = 1;
        error_count++;
    } else if (level == LEVEL_ERROR) {
        has_error = 1;
        error_count++;
    } else if (level == LEVEL_WARNING) {
        warning_count++;
    } else {
        notice_count++;
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
// s: 操作数字符串
// base: 操作数字符串在原始行里的起始偏移
// source: 原始行
// args: 输出的参数数组
// cols: 输出的列号数组（相对于原始行）
// count: 输出的参数个数
void parse_operands(const char *s, int base, const char *source,
                    char args[MAX_PARAMS][MAX_NAME], int cols[MAX_PARAMS],
                    int *count)
{
    *count = 0;
    const char *p = s;
    
    while (*p && *count < MAX_PARAMS) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        
        cols[*count] = base + (p - s);
        
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
    
    errno = 0; // 清除 errno
    
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        if (strlen(s) <= 2) {
            error_msg(filename, line_num, source, col, strlen(s), LEVEL_ERROR,
                      "Invalid hexadecimal immediate value: \"%s\".", s);
            return -1;
        }
        value = strtol(s, &end, 16);
    } else if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) {
        if (strlen(s) <= 2) {
            error_msg(filename, line_num, source, col, strlen(s), LEVEL_ERROR,
                      "Invalid binary immediate value: \"%s\".", s);
            return -1;
        }
        value = strtol(s + 2, &end, 2);
    } else {
        value = strtol(s, &end, 10);
    }
    
    // 检查是否解析到了数字
    if (end == s || (s[0] == '0' && (s[1] == 'x' || s[1] == 'X') && end == s + 2) ||
        (s[0] == '0' && (s[1] == 'b' || s[1] == 'B') && end == s + 2)) {
        error_msg(filename, line_num, source, col, strlen(s), LEVEL_ERROR,
                  "The name \"%s\" is not a valid immediate value.", s);
        return -1;
    }
    
    // 检查是否有多余字符
    if (*end != '\0') {
        error_msg(filename, line_num, source, col, strlen(s), LEVEL_ERROR,
                  "The name \"%s\" is not a valid immediate value.", s);
        return -1;
    }
    
    // 检查溢出
    if (errno == ERANGE || value > INT_MAX || value < INT_MIN) {
        error_msg(filename, line_num, source, col, strlen(s), LEVEL_ERROR,
                  "Immediate value \"%s\" is out of range.", s);
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
            // 如果操作数不是合法标识符，说明是立即数，不需要转换
            if (!is_valid_identifier(operand)) {
                return mnemonic;
            }
            
            int target = find_label(operand);
            if (target < 0) {
                return NULL; // 未定义标签
            }
            
            if (target >= current_addr) {
                return loop_jumps[i].forward;
            } else {
                return loop_jumps[i].backward;
            }
        }
    }
    
    return mnemonic;
}

// 编码一条指令
// 返回 0 成功（*code_out 有效），-1 表示本行有错误（已报错，*code_out 未定义）
int encode(Instruction *inst, const char *operands, int addr, int base,
           const char *filename, int line_num, const char *source,
           uint16_t *code_out)
{
    int errs = 0;
    uint16_t code = inst->opcode << 12;
    char args[MAX_PARAMS][MAX_NAME];
    int cols[MAX_PARAMS];
    int arg_count = 0;

    parse_operands(operands, base, source, args, cols, &arg_count);

    // 操作数个数不对：无法继续有意义地解析，直接返回
    if (arg_count != inst->arg_count) {
        error_msg(filename, line_num, source, 0, strlen(source), LEVEL_ERROR,
                  "Instruction '%s' expects %d operands, but got %d.",
                  inst->mnemonic, inst->arg_count, arg_count);
        return -1;
    }

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
                if (rs < 0) { errs++; rs = 0; }

                if (strcmp(inst->mnemonic, "JUMRF") == 0 ||
                    strcmp(inst->mnemonic, "JUMRB") == 0) {
                    code |= (rs << 8);
                } else {
                    code |= (rs << 10);
                    int rd = parse_register(args[1], filename, line_num, source, cols[1]);
                    if (rd < 0) { errs++; rd = 0; }
                    code |= (rd << 6);
                }
                code |= inst->ext;
            }
            break;

        case FMT_RR:
            {
                int rs0 = parse_register(args[0], filename, line_num, source, cols[0]);
                if (rs0 < 0) { errs++; rs0 = 0; }
                int rs1 = parse_register(args[1], filename, line_num, source, cols[1]);
                if (rs1 < 0) { errs++; rs1 = 0; }
                code |= (rs0 << 10);
                code |= (rs1 << 8);
                if (inst->arg_count == 3) {
                    int rd = parse_register(args[2], filename, line_num, source, cols[2]);
                    if (rd < 0) { errs++; rd = 0; }
                    code |= (rd << 6);
                }
                code |= inst->ext;
            }
            break;

        case FMT_RRI:
            {
                int rs0 = parse_register(args[0], filename, line_num, source, cols[0]);
                if (rs0 < 0) { errs++; rs0 = 0; }

                int imm = parse_immediate(args[1], filename, line_num, source, cols[1]);
                if (imm < 0) { errs++; imm = 0; }
                else if (imm > 255) {
                    error_msg(filename, line_num, source, cols[1], strlen(args[1]),
                              LEVEL_ERROR, "Immediate value %d out of range (0-255).", imm);
                    errs++; imm = 0;
                }

                code |= (rs0 << 10);
                code |= ((imm >> 6) & 0x3) << 8;

                if (inst->arg_count == 3) {
                    int rd = parse_register(args[2], filename, line_num, source, cols[2]);
                    if (rd < 0) { errs++; rd = 0; }
                    code |= (rd << 6);
                }

                code |= ((imm >> 4) & 0x3) << 4;
                code |= (imm & 0xF);
            }
            break;

        case FMT_IMM8:
            {
                int imm;
                if (is_valid_identifier(args[0])) {
                    int target = find_label(args[0]);
                    if (target < 0) {
                        error_msg(filename, line_num, source, cols[0], strlen(args[0]),
                                  LEVEL_ERROR, "Undefined label: %s", args[0]);
                        errs++;
                        imm = 0;
                    } else if (is_backward_jump(inst->mnemonic)) {
                        imm = addr - target;
                    } else {
                        imm = target - addr;
                    }
                } else {
                    imm = parse_immediate(args[0], filename, line_num, source, cols[0]);
                    if (imm < 0) { errs++; imm = 0; }
                }

                if (imm < 0 || imm > 255) {
                    error_msg(filename, line_num, source, cols[0], strlen(args[0]),
                              LEVEL_ERROR, "Immediate value %d out of range (0-255).", imm);
                    errs++; imm = 0;
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
                int rs1 = parse_register(args[0], filename, line_num, source, cols[0]);
                if (rs1 < 0) { errs++; rs1 = 0; }
                int rs0 = parse_register(args[1], filename, line_num, source, cols[1]);
                if (rs0 < 0) { errs++; rs0 = 0; }

                int imm = parse_immediate(args[2], filename, line_num, source, cols[2]);
                if (imm < 0) { errs++; imm = 0; }
                else if (imm > 255) {
                    error_msg(filename, line_num, source, cols[2], strlen(args[2]),
                              LEVEL_ERROR, "Immediate value %d out of range (0-255).", imm);
                    errs++; imm = 0;
                }

                code |= (rs0 << 10);
                code |= (rs1 << 8);
                code |= ((imm >> 6) & 0x3) << 6;
                code |= ((imm >> 4) & 0x3) << 4;
                code |= (imm & 0xF);
            }
            break;

        case FMT_CALL:
            {
                int rd = parse_register(args[1], filename, line_num, source, cols[1]);
                if (rd < 0) { errs++; rd = 0; }

                int imm;
                if (is_valid_identifier(args[0])) {
                    int target = find_label(args[0]);
                    if (target < 0) {
                        error_msg(filename, line_num, source, cols[0], strlen(args[0]),
                                  LEVEL_ERROR, "Undefined label: %s", args[0]);
                        errs++;
                        imm = 0;
                    } else if (strcmp(inst->mnemonic, "CALLB") == 0) {
                        imm = addr - target;
                    } else {
                        imm = target - addr;
                    }
                } else {
                    imm = parse_immediate(args[0], filename, line_num, source, cols[0]);
                    if (imm < 0) { errs++; imm = 0; }
                }

                if (imm < 0 || imm > 255) {
                    error_msg(filename, line_num, source, cols[0], strlen(args[0]),
                              LEVEL_ERROR, "Immediate value %d out of range (0-255).", imm);
                    errs++; imm = 0;
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
                if (imm < 0) { errs++; imm = 0; }
                else if (imm > 255) {
                    error_msg(filename, line_num, source, cols[0], strlen(args[0]),
                              LEVEL_ERROR, "Immediate value %d out of range (0-255).", imm);
                    errs++; imm = 0;
                }

                code |= ((imm >> 6) & 0x3) << 10;
                code |= ((imm >> 4) & 0x3) << 8;
                code |= ((imm >> 2) & 0x3) << 6;
                code |= (imm & 0x3) << 4;
            }
            break;
    }

    if (errs) return -1;
    *code_out = code;
    return 0;
}

// 第二遍扫描：收集机器码到 g_words
// 返回处理的指令数；仅致命错误返回 -1
int pass2(FILE *in, const char *filename)
{
    int addr = 0;
    char line[MAX_LINE];

    safe_strcpy(current_filename, filename, sizeof(current_filename));
    current_line = 0;

    while (fgets(line, sizeof(line), in)) {
        // 长行截断检查
        size_t len = strlen(line);
        if (len == sizeof(line) - 1 && line[len - 1] != '\n' && !feof(in)) {
            error_msg(current_filename, current_line, line, 0, strlen(line), LEVEL_ERROR,
                      "Line too long (max %d bytes).", MAX_LINE - 1);
            int c;
            while ((c = fgetc(in)) != '\n' && c != EOF);
            continue;
        }

        line[strcspn(line, "\r\n")] = '\0';
        char *trimmed = trim(line);
        if (trimmed[0] == '\0') continue;

        if (strncmp(trimmed, ".file \"", 7) == 0) {
            sscanf(trimmed, ".file \"%[^\"]\"", current_filename);
            continue;
        }
        if (strncmp(trimmed, ".line ", 6) == 0) {
            sscanf(trimmed, ".line %d", &current_line);
            continue;
        }

        char source_code[MAX_LINE];
        safe_strcpy(source_code, line, MAX_LINE);

        char parse_line[MAX_LINE];
        safe_strcpy(parse_line, line, MAX_LINE);
        char *comment = strchr(parse_line, '#');
        if (comment) *comment = '\0';

        char *code = trim(parse_line);
        if (code[0] == '\0') continue;

        if (is_label(code)) {
            char *rest = strchr(code, ':') + 1;
            rest = trim(rest);
            if (rest[0] == '\0') continue;
            code = rest;
        }

        char mnemonic[MAX_NAME];
        int i = 0;
        while (code[i] && code[i] != ' ' && code[i] != '\t' && i < MAX_NAME - 1) {
            mnemonic[i] = code[i];
            i++;
        }
        mnemonic[i] = '\0';

        const char *operands = code + i;
        while (*operands == ' ' || *operands == '\t') operands++;

        uint16_t code_val = 0x0000;

        const char *resolved = resolve_loop_jump(mnemonic, operands, addr);
        if (resolved == NULL) {
            error_msg(current_filename, current_line, source_code, 0, strlen(operands),
                      LEVEL_ERROR, "Undefined label: \"%s\".", operands);
            if (record_word(addr, 0x0000) < 0) return -1;
            addr++;
            continue;
        }

        Instruction *inst = find_instruction(resolved);
        if (!inst) {
            error_msg(current_filename, current_line, source_code, 0, strlen(mnemonic),
                      LEVEL_ERROR, "Unknown instruction: \"%s\".", mnemonic);
            if (record_word(addr, 0x0000) < 0) return -1;
            addr++;
            continue;
        }

        int base = operands - parse_line;
        if (encode(inst, operands, addr, base, current_filename, current_line,
                   source_code, &code_val) < 0) {
            code_val = 0x0000;
        }

        if (record_word(addr, code_val) < 0) return -1;
        addr++;
    }

    return addr;
}

// 判断是否是向后跳转指令
int is_backward_jump(const char *mnemonic) {
    return strcmp(mnemonic, "JUMPB") == 0 ||
           strcmp(mnemonic, "JCB")   == 0 ||
           strcmp(mnemonic, "JEB")   == 0 ||
           strcmp(mnemonic, "JROB")  == 0;
}

// 退出时的清理函数
void cleanup_tmp(void) {
    if (tmp_filename[0] != '\0') {
        remove(tmp_filename);
    }
}

// 安全字符串拷贝，返回 0 表示截断
int safe_strcpy(char *dest, const char *src, size_t dest_size) {
    if (dest_size == 0) return -1;
    size_t i = 0;
    while (i < dest_size - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
    return (src[i] != '\0') ? -1 : 0;  // 返回 -1 表示被截断
}
// 添加标签
// 添加标签，返回 0 成功，-1 失败（已报错）
int add_label(const char *name, int addr)
{
    if (!is_valid_identifier(name)) {
        error_msg(current_filename, current_line, name, 0, strlen(name), LEVEL_ERROR,
                  "Invalid label name: \"%s\".", name);
        return -1;
    }

    if (label_count >= MAX_LABELS) {
        error_msg(current_filename, current_line, name, 0, strlen(name), LEVEL_ERROR,
                  "Too many labels (max %d).", MAX_LABELS);
        return -1;
    }

    // 重复检测
    for (int i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, name) == 0) {
            error_msg(current_filename, current_line, name, 0, strlen(name), LEVEL_ERROR,
                      "Duplicate label definition: \"%s\".", name);
            return -1;
        }
    }

    safe_strcpy(labels[label_count].name, name, MAX_NAME);
    labels[label_count].addr = addr;
    label_count++;
    return 0;
}

// 检查是否是合法标识符
int is_valid_identifier(const char *s) {
    if (!s || !*s) return 0;
    if (!isalpha((unsigned char)*s) && *s != '_') return 0;
    for (const char *p = s + 1; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_') return 0;
    }
    return 1;
}

// ============================================================
// 路径处理工具函数（纯 C 标准库，跨平台）
// ============================================================

// 判断是否是绝对路径（兼容 Windows 和 Unix）
int is_absolute_path(const char *path) {
    if (!path || !*path) return 0;
    // Unix 的 / 或 Windows 的 \ 开头
    if (path[0] == '/' || path[0] == '\\') return 1;
    // Windows 的 C: 盘符
    if (strlen(path) > 1 && path[1] == ':') return 1;
    return 0;
}

// 判断是否是路径分隔符
int is_path_sep(char c) {
    return c == '/' || c == '\\';
}

// 将路径中的反斜杠统一替换为正斜杠（统一输出格式）
void unify_separators(char *path) {
    for (char *p = path; *p; p++) {
        if (*p == '\\') *p = '/';
    }
}

// 规范化路径：处理 ./、../ 和重复的斜杠。
// 成功返回 0；路径过深、过长或参数非法返回 -1。
int normalize_path(const char *in, char *out, size_t out_size)
{
    if (!in || !out || out_size == 0) return -1;

    if (in[0] == '\0') {
        out[0] = '\0';
        return 0;
    }

    char temp[1024];
    if (safe_strcpy(temp, in, sizeof(temp)) != 0) {
        out[0] = '\0';
        return -1;   // 输入本身超过 temp 容量
    }
    unify_separators(temp);

    // 段数组带上限
    #define MAX_PATH_SEGS 256
    char *segments[MAX_PATH_SEGS];
    int seg_count = 0;

    char *p = temp;
    int is_abs = (temp[0] == '/');
    int drive_len = 0;

    if (strlen(temp) > 1 && temp[1] == ':') {
        drive_len = 2;
        p = temp + 2;
        while (is_path_sep(*p)) p++;
    } else if (is_abs) {
        p++;
    }

    // 切割段
    char *token = p;
    while (*p) {
        if (is_path_sep(*p)) {
            *p = '\0';
            if (token[0] != '\0') {
                if (seg_count >= MAX_PATH_SEGS) {
                    out[0] = '\0';
                    return -1;
                }
                segments[seg_count++] = token;
            }
            token = p + 1;
        }
        p++;
    }
    if (token[0] != '\0') {
        if (seg_count >= MAX_PATH_SEGS) {
            out[0] = '\0';
            return -1;
        }
        segments[seg_count++] = token;
    }

    // 处理 . 和 ..
    char *result[MAX_PATH_SEGS];
    int res_count = 0;

    for (int i = 0; i < seg_count; i++) {
        if (strcmp(segments[i], ".") == 0) {
            continue;
        } else if (strcmp(segments[i], "..") == 0) {
            if (res_count > 0 && strcmp(result[res_count - 1], "..") != 0) {
                res_count--;
            } else if (!is_abs && drive_len == 0) {
                if (res_count >= MAX_PATH_SEGS) {
                    out[0] = '\0';
                    return -1;
                }
                result[res_count++] = "..";
            }
        } else {
            if (res_count >= MAX_PATH_SEGS) {
                out[0] = '\0';
                return -1;
            }
            result[res_count++] = segments[i];
        }
    }

    // 重新拼接：用"实际写入长度"，检测截断
    out[0] = '\0';
    size_t len = 0;
    int truncated = 0;

    // 计算"想写"的长度，但只复制能放下的部分
    #define APPEND_STR(s) do {                                       \
        int _n = snprintf(out + len, out_size - len, "%s", (s));     \
        if (_n < 0) { out[0] = '\0'; return -1; }                    \
        if ((size_t)_n >= out_size - len) { truncated = 1; len = out_size - 1; } \
        else { len += (size_t)_n; }                                  \
    } while (0)

    #define APPEND_CHAR(c) do {                                      \
        int _n = snprintf(out + len, out_size - len, "%c", (c));     \
        if (_n < 0) { out[0] = '\0'; return -1; }                    \
        if ((size_t)_n >= out_size - len) { truncated = 1; len = out_size - 1; } \
        else { len += (size_t)_n; }                                  \
    } while (0)

    if (drive_len > 0) {
        // drive_len 是 2，temp 前 2 字节是盘符
        char drive[3];
        drive[0] = temp[0];
        drive[1] = temp[1];
        drive[2] = '\0';
        APPEND_STR(drive);
        if (res_count > 0) APPEND_CHAR('/');
    } else if (is_abs) {
        APPEND_CHAR('/');
    }

    for (int i = 0; i < res_count && !truncated; i++) {
        APPEND_STR(result[i]);
        if (i < res_count - 1) APPEND_CHAR('/');
    }

    #undef APPEND_STR
    #undef APPEND_CHAR

    if (truncated) {
        out[0] = '\0';
        return -1;
    }

    if (out[0] == '\0' && is_abs) {
        // 根路径特例
        if (out_size < 2) { out[0] = '\0'; return -1; }
        out[0] = '/';
        out[1] = '\0';
    }

    #undef MAX_PATH_SEGS
    return 0;
}

// 提取文件所在目录（包含末尾的斜杠）。
// 成功返回 0；路径过长无法完整写入 dir 时返回 -1（dir 置空）。
// 无分隔符时 dir 置空并返回 0（表示"当前目录"）。
int get_directory(const char *file_path, char *dir, size_t dir_size)
{
    if (!file_path || !dir || dir_size == 0) {
        if (dir && dir_size > 0) dir[0] = '\0';
        return -1;
    }

    const char *last_slash     = strrchr(file_path, '/');
    const char *last_backslash = strrchr(file_path, '\\');

    const char *last_sep = NULL;
    if (last_slash && last_backslash) {
        // 两者都非 NULL：比较它们在 file_path 内的偏移，避免比较无关指针
        last_sep = (last_slash > last_backslash) ? last_slash : last_backslash;
    } else if (last_slash) {
        last_sep = last_slash;
    } else if (last_backslash) {
        last_sep = last_backslash;
    }

    if (!last_sep) {
        dir[0] = '\0';
        return 0;
    }

    size_t len = (size_t)(last_sep - file_path) + 1;   // 含末尾分隔符
    if (len >= dir_size) {
        dir[0] = '\0';
        return -1;   // 放不下，视为失败
    }

    memcpy(dir, file_path, len);
    dir[len] = '\0';
    return 0;
}

// 将相对路径拼接到基准目录下。
// 成功返回 0；失败（路径过长、normalize 失败）返回 -1，out_path 置空。
int resolve_include_path(const char *base_dir, const char *include_name,
                         char *out_path, size_t out_size)
{
    if (!out_path || out_size == 0) return -1;
    out_path[0] = '\0';

    if (!include_name) return -1;

    if (is_absolute_path(include_name)) {
        return normalize_path(include_name, out_path, out_size);
    }

    char temp[1024];
    int n = snprintf(temp, sizeof(temp), "%s%s",
                     base_dir ? base_dir : "", include_name);
    if (n < 0 || (size_t)n >= sizeof(temp)) {
        return -1;   // 拼接后超长
    }

    return normalize_path(temp, out_path, out_size);
}

// 确保容量足够存放第 addr 条指令
static int ensure_word_capacity(int addr)
{
    if (addr < g_word_cap) return 0;

    int new_cap = g_word_cap ? g_word_cap * 2 : 256;
    while (new_cap <= addr) new_cap *= 2;

    uint16_t *p = realloc(g_words, (size_t)new_cap * sizeof(uint16_t));
    if (!p) {
        fprintf(stderr, RED "Error:" RESET " Out of memory.\n");
        return -1;
    }
    // 新扩容部分清零（logisim 补零需要，bin/hex 不关心）
    memset(p + g_word_cap, 0, (size_t)(new_cap - g_word_cap) * sizeof(uint16_t));
    g_words = p;
    g_word_cap = new_cap;
    return 0;
}

// 记录一条指令（addr 为指令序号）
static int record_word(int addr, uint16_t code)
{
    if (ensure_word_capacity(addr) < 0) return -1;
    g_words[addr] = code;
    if (addr + 1 > g_word_count) g_word_count = addr + 1;
    return 0;
}

// 释放
static void free_words(void)
{
    free(g_words);
    g_words = NULL;
    g_word_count = 0;
    g_word_cap = 0;
}

// ---------------- bin：大端 2 字节/指令 ----------------
static int write_bin(FILE *out)
{
    for (int i = 0; i < g_word_count; i++) {
        uint8_t hi = (g_words[i] >> 8) & 0xFF;
        uint8_t lo = g_words[i] & 0xFF;
        if (fwrite(&hi, 1, 1, out) != 1 || fwrite(&lo, 1, 1, out) != 1) {
            return -1;
        }
    }
    return 0;
}

// ---------------- hex：每行一个 %04X ----------------
static int write_hex(FILE *out)
{
    for (int i = 0; i < g_word_count; i++) {
        if (fprintf(out, "%04X\n", g_words[i]) < 0) return -1;
    }
    return 0;
}

// ---------------- logisim：v3.0 hex words addressed ----------------
// 地址按字节计；每行 16 个字（=32 字节）；数据补零到 64KB
static int write_logisim(FILE *out)
{
    if (fprintf(out, "v3.0 hex words addressed\n") < 0) return -1;

    // 总字数：补零到 32768 个字（64KB）
    // 但如果实际指令超过 32768，则按实际长度输出（避免截断）
    int total_words = g_word_count > LOGISIM_MAX_WORDS
                        ? g_word_count : LOGISIM_MAX_WORDS;

    int words_per_line = LOGISIM_WORDS_PER_LINE;
    for (int base = 0; base < total_words; base += words_per_line) {
        // 地址以字节计：word 序号 * 2
        unsigned int byte_addr = (unsigned int)base * INSTRUCTION_BYTES;
        if (fprintf(out, "%04x: ", byte_addr) < 0) return -1;

        for (int k = 0; k < words_per_line; k++) {
            int idx = base + k;
            uint16_t w = (idx < g_word_count) ? g_words[idx] : 0x0000;
            // 字之间用单空格分隔；行尾不要多余空格
            if (fprintf(out, "%04x", w) < 0) return -1;
            if (k + 1 < words_per_line) {
                if (fputc(' ', out) == EOF) return -1;
            }
        }
        if (fputc('\n', out) == EOF) return -1;
    }
    return 0;
}

// 按 format 分派
static int write_output(FILE *out, const char *format)
{
    if (strcmp(format, "bin") == 0) {
        return write_bin(out);
    } else if (strcmp(format, "hex") == 0) {
        return write_hex(out);
    } else if (strcmp(format, "logisim") == 0) {
        return write_logisim(out);
    }
    return -1;  // 未知格式
}

// 格式校验
static int is_valid_format(const char *format)
{
    return strcmp(format, "bin") == 0
        || strcmp(format, "hex") == 0
        || strcmp(format, "logisim") == 0;
}

// 把 src 追加到 dst 末尾。dst 总容量 cap，*used 是当前已用长度（不含 '\0'）。
// 成功返回 0；空间不足返回 -1（已尽量写入，dst 保证 '\0' 结尾）。
static int safe_append(char *dst, size_t cap, size_t *used, const char *src)
{
    size_t u = *used;
    while (*src) {
        if (u + 1 >= cap) {
            dst[u] = '\0';
            *used = u;
            return -1;
        }
        dst[u++] = *src++;
    }
    dst[u] = '\0';
    *used = u;
    return 0;
}

// 追加单个字符。成功返回 0；空间不足返回 -1。
static int safe_putc(char *dst, size_t cap, size_t *used, char c)
{
    size_t u = *used;
    if (u + 1 >= cap) {
        dst[u] = '\0';
        *used = u;
        return -1;
    }
    dst[u++] = c;
    dst[u] = '\0';
    *used = u;
    return 0;
}