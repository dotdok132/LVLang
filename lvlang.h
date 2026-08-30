/**
 * @file lvlang.h
 * @brief LVLang (Low-Volume Language) - Virtual Machine & High-Level Compiler
 */

#ifndef LVLANG_H
#define LVLANG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LVL_STACK_SIZE
#define LVL_STACK_SIZE 256
#endif

#ifndef LVL_CALL_STACK_SIZE
#define LVL_CALL_STACK_SIZE 64
#endif

#ifndef LVL_NUM_REGISTERS
#define LVL_NUM_REGISTERS 16
#endif

typedef enum {
    LVL_OK                       =  0,
    LVL_STATUS_HALT              =  1,
    LVL_ERR_INVALID_OPCODE       = -1,
    LVL_ERR_STACK_OVERFLOW       = -2,
    LVL_ERR_STACK_UNDERFLOW      = -3,
    LVL_ERR_CALL_STACK_OVERFLOW  = -4,
    LVL_ERR_CALL_STACK_UNDERFLOW = -5,
    LVL_ERR_DIVISION_BY_ZERO     = -6,
    LVL_ERR_OUT_OF_BOUNDS        = -7,
    LVL_ERR_COMPILATION_FAILED   = -8
} lvl_status_t;

typedef void (*lvl_print_num_fn)(int32_t val);
typedef void (*lvl_print_char_fn)(char c);

typedef struct {
    int32_t stack[LVL_STACK_SIZE];
    size_t sp;

    size_t call_stack[LVL_CALL_STACK_SIZE];
    size_t csp;

    int32_t registers[LVL_NUM_REGISTERS];

    const uint8_t *bytecode;
    size_t bytecode_size;
    size_t ip;

    lvl_status_t status;

    lvl_print_num_fn print_num;
    lvl_print_char_fn print_char;
} lvl_vm_t;

void lvl_init(lvl_vm_t *vm, const uint8_t *bytecode, size_t size);
int  lvl_step(lvl_vm_t *vm);
int  lvl_run(lvl_vm_t *vm);

lvl_status_t lvl_get_status(const lvl_vm_t *vm);
int32_t      lvl_stack_peek(const lvl_vm_t *vm);
int32_t      lvl_reg_get(const lvl_vm_t *vm, uint8_t reg_idx);
void         lvl_reg_set(lvl_vm_t *vm, uint8_t reg_idx, int32_t val);

int lvl_compile(const char *source, uint8_t *out_bytecode, size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif /* LVLANG_H */

/* ========================================================================== */
/*                              IMPLEMENTATION                                */
/* ========================================================================== */

#ifdef LVLANG_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

#ifndef LVL_NO_STDIO
static void lvl_default_print_num(int32_t val) {
    printf("%d", (int)val);
    fflush(stdout);
}

static void lvl_default_print_char(char c) {
    putchar(c);
    fflush(stdout);
}
#else
static void lvl_default_print_num(int32_t val) { (void)val; }
static void lvl_default_print_char(char c) { (void)c; }
#endif

void lvl_init(lvl_vm_t *vm, const uint8_t *bytecode, size_t size) {
    if (!vm) return;
    vm->sp = 0;
    vm->csp = 0;
    vm->ip = 0;
    vm->bytecode = bytecode;
    vm->bytecode_size = size;
    vm->status = LVL_OK;

    for (size_t i = 0; i < LVL_NUM_REGISTERS; i++) vm->registers[i] = 0;
    for (size_t i = 0; i < LVL_STACK_SIZE; i++) vm->stack[i] = 0;
    for (size_t i = 0; i < LVL_CALL_STACK_SIZE; i++) vm->call_stack[i] = 0;

    vm->print_num = lvl_default_print_num;
    vm->print_char = lvl_default_print_char;
}

static inline bool lvl_push(lvl_vm_t *vm, int32_t val) {
    if (vm->sp >= LVL_STACK_SIZE) {
        vm->status = LVL_ERR_STACK_OVERFLOW;
        return false;
    }
    vm->stack[vm->sp++] = val;
    return true;
}

static inline bool lvl_pop(lvl_vm_t *vm, int32_t *out_val) {
    if (vm->sp == 0) {
        vm->status = LVL_ERR_STACK_UNDERFLOW;
        return false;
    }
    *out_val = vm->stack[--vm->sp];
    return true;
}

int lvl_step(lvl_vm_t *vm) {
    if (!vm) return LVL_ERR_OUT_OF_BOUNDS;
    if (vm->status != LVL_OK) return vm->status;

    if (vm->ip + 1 >= vm->bytecode_size) {
        vm->status = LVL_ERR_OUT_OF_BOUNDS;
        return vm->status;
    }

    uint8_t b1 = vm->bytecode[vm->ip];
    uint8_t b2 = vm->bytecode[vm->ip + 1];
    vm->ip += 2;

    if ((b1 == 0xFF && b2 == 0xFF) || (b1 == 0x05 && b2 == 0xFF)) {
        vm->status = LVL_STATUS_HALT;
        return vm->status;
    }

    int32_t a, b;
    size_t target_byte;

    switch (b1) {
        /* MODULE 0x01: STACK & REGISTERS */
        case 0x01:
            if (b2 == 0x00) lvl_push(vm, 0);
            else if (b2 == 0x01) lvl_pop(vm, &a);
            else if (b2 == 0x02) { if (lvl_pop(vm, &a)) { lvl_push(vm, a); lvl_push(vm, a); } }
            else if (b2 == 0x03) { if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) { lvl_push(vm, b); lvl_push(vm, a); } }
            else if (b2 >= 0x10 && b2 <= 0x1F) {
                uint8_t reg = b2 - 0x10;
                lvl_push(vm, vm->registers[reg]);
            } else if (b2 >= 0x30 && b2 <= 0x3F) {
                uint8_t reg = b2 - 0x30;
                if (lvl_pop(vm, &a)) vm->registers[reg] = a;
            } else if (b2 >= 0x80) {
                lvl_push(vm, (int32_t)(b2 - 0x80));
            } else {
                vm->status = LVL_ERR_INVALID_OPCODE;
            }
            break;

        /* MODULE 0x02: ARITHMETIC & DIRECT REGISTER MANIPULATION */
        case 0x02:
            if (b2 == 0x01) { if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) lvl_push(vm, a + b); }
            else if (b2 == 0x02) { if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) lvl_push(vm, a - b); }
            else if (b2 == 0x03) { if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) lvl_push(vm, a * b); }
            else if (b2 == 0x04) {
                if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) {
                    if (b == 0) vm->status = LVL_ERR_DIVISION_BY_ZERO;
                    else lvl_push(vm, a / b);
                }
            } else if (b2 == 0x05) {
                if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) {
                    if (b == 0) vm->status = LVL_ERR_DIVISION_BY_ZERO;
                    else lvl_push(vm, a % b);
                }
            } else if (b2 >= 0x10 && b2 <= 0x1F) {
                uint8_t reg = b2 - 0x10;
                vm->registers[reg]++;
            } else if (b2 >= 0x20 && b2 <= 0x2F) {
                uint8_t reg = b2 - 0x20;
                vm->registers[reg]--;
            } else {
                vm->status = LVL_ERR_INVALID_OPCODE;
            }
            break;

        /* MODULE 0x03: LOGIC & COMPARISON */
        case 0x03:
            if (b2 == 0x01) { if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) lvl_push(vm, a == b ? 1 : 0); }
            else if (b2 == 0x02) { if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) lvl_push(vm, a != b ? 1 : 0); }
            else if (b2 == 0x03) { if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) lvl_push(vm, a > b ? 1 : 0); }
            else if (b2 == 0x04) { if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) lvl_push(vm, a < b ? 1 : 0); }
            else if (b2 == 0x05) { if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) lvl_push(vm, a >= b ? 1 : 0); }
            else if (b2 == 0x06) { if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) lvl_push(vm, a <= b ? 1 : 0); }
            else if (b2 == 0x07) { if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) lvl_push(vm, (a && b) ? 1 : 0); }
            else if (b2 == 0x08) { if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) lvl_push(vm, (a || b) ? 1 : 0); }
            else if (b2 == 0x09) { if (lvl_pop(vm, &a)) lvl_push(vm, (!a) ? 1 : 0); }
            else vm->status = LVL_ERR_INVALID_OPCODE;
            break;

        /* MODULE 0x04: FLOW CONTROL & SUBROUTINES */
        case 0x04:
            if (b2 == 0x00) {
                /* RET: Return from subroutine */
                if (vm->csp > 0) {
                    vm->ip = vm->call_stack[--vm->csp];
                } else {
                    vm->status = LVL_ERR_CALL_STACK_UNDERFLOW;
                }
            } else if (b2 >= 0x10 && b2 <= 0x4F) {
                /* JMP Target */
                target_byte = (size_t)(b2 - 0x10) * 2;
                if (target_byte < vm->bytecode_size) vm->ip = target_byte;
                else vm->status = LVL_ERR_OUT_OF_BOUNDS;
            } else if (b2 >= 0x50 && b2 <= 0x8F) {
                /* JZ Target */
                if (lvl_pop(vm, &a) && a == 0) {
                    target_byte = (size_t)(b2 - 0x50) * 2;
                    if (target_byte < vm->bytecode_size) vm->ip = target_byte;
                    else vm->status = LVL_ERR_OUT_OF_BOUNDS;
                }
            } else if (b2 >= 0x90 && b2 <= 0xCF) {
                /* JNZ Target */
                if (lvl_pop(vm, &a) && a != 0) {
                    target_byte = (size_t)(b2 - 0x90) * 2;
                    if (target_byte < vm->bytecode_size) vm->ip = target_byte;
                    else vm->status = LVL_ERR_OUT_OF_BOUNDS;
                }
            } else if (b2 >= 0xD0 && b2 <= 0xFD) {
                /* CALL Target: Macro / Subroutine Call */
                if (vm->csp < LVL_CALL_STACK_SIZE) {
                    vm->call_stack[vm->csp++] = vm->ip;
                    target_byte = (size_t)(b2 - 0xD0) * 2;
                    if (target_byte < vm->bytecode_size) vm->ip = target_byte;
                    else vm->status = LVL_ERR_OUT_OF_BOUNDS;
                } else {
                    vm->status = LVL_ERR_CALL_STACK_OVERFLOW;
                }
            } else {
                vm->status = LVL_ERR_INVALID_OPCODE;
            }
            break;

        /* MODULE 0x05: SYSTEM & IO */
        case 0x05:
            switch (b2) {
                case 0x01: if (lvl_pop(vm, &a) && vm->print_num) vm->print_num(a); break;
                case 0x02: if (lvl_pop(vm, &a) && vm->print_char) vm->print_char((char)a); break;
                case 0x03:
                    while (vm->ip < vm->bytecode_size) {
                        char c = (char)vm->bytecode[vm->ip++];
                        if (c == '\0') break;
                        if (vm->print_char) vm->print_char(c);
                    }
                    if (vm->ip % 2 != 0) vm->ip++;
                    break;
                case 0x04: if (vm->print_char) vm->print_char('\n'); break;
                case 0xFF: vm->status = LVL_STATUS_HALT; break;
                default: vm->status = LVL_ERR_INVALID_OPCODE; break;
            }
            break;

        /* MODULE 0x06: SYSTEM SYSCALLS & ENVIRONMENT */
        case 0x06:
            switch (b2) {
                case 0x01:
                    /* SYS_TIME: Push Unix timestamp in seconds */
                    lvl_push(vm, (int32_t)time(NULL));
                    break;
                case 0x02:
                    /* SYS_RAND: Push pseudo-random 15-bit integer */
                    lvl_push(vm, (int32_t)(rand() & 0x7FFF));
                    break;
                case 0x03:
                    /* SYS_CLOCK: Push process clock in milliseconds */
                    lvl_push(vm, (int32_t)(clock() * 1000 / CLOCKS_PER_SEC));
                    break;
                default:
                    vm->status = LVL_ERR_INVALID_OPCODE;
                    break;
            }
            break;

        /* MODULE 0x07: MACRO / SHORTCUT OPCODES */
        case 0x07:
            if (b2 >= 0x10 && b2 <= 0x1F) {
                /* MACRO_PRINT_REG (0x10 + R): Load R, PRINT_NUM, PRINT_NL */
                uint8_t reg = b2 - 0x10;
                int32_t val = vm->registers[reg];
                if (vm->print_num) vm->print_num(val);
                if (vm->print_char) vm->print_char('\n');
            } else if (b2 >= 0x30 && b2 <= 0x3F) {
                /* MACRO_PRINT_REG_RAW (0x30 + R): Load R, PRINT_NUM */
                uint8_t reg = b2 - 0x30;
                int32_t val = vm->registers[reg];
                if (vm->print_num) vm->print_num(val);
            } else if (b2 >= 0x40 && b2 <= 0x4F) {
                /* MACRO_CLEAR_REG (0x40 + R): Clear R to 0 */
                uint8_t reg = b2 - 0x40;
                vm->registers[reg] = 0;
            } else {
                vm->status = LVL_ERR_INVALID_OPCODE;
            }
            break;

        /* MODULE 0x08: VECTOR & EMBEDDING MATH (AI HARDWARE ACCELERATION) */
        case 0x08:
            switch (b2) {
                case 0x01: {
                    /* VEC_DOT_4D: Dot product of (R0..R3) . (R4..R7) -> Push to stack */
                    int32_t dot = (vm->registers[0] * vm->registers[4]) +
                                  (vm->registers[1] * vm->registers[5]) +
                                  (vm->registers[2] * vm->registers[6]) +
                                  (vm->registers[3] * vm->registers[7]);
                    lvl_push(vm, dot);
                    break;
                }
                case 0x02: {
                    /* VEC_ADD_4D: (R0..R3) += (R4..R7) */
                    for (int i = 0; i < 4; i++) {
                        vm->registers[i] += vm->registers[i + 4];
                    }
                    break;
                }
                case 0x03: {
                    /* VEC_SCALE_4D: (R0..R3) *= Stack Top Scalar */
                    if (lvl_pop(vm, &a)) {
                        for (int i = 0; i < 4; i++) {
                            vm->registers[i] *= a;
                        }
                    }
                    break;
                }
                default:
                    vm->status = LVL_ERR_INVALID_OPCODE;
                    break;
            }
            break;

        default: vm->status = LVL_ERR_INVALID_OPCODE; break;
    }

    return vm->status;
}

int lvl_run(lvl_vm_t *vm) {
    if (!vm) return LVL_ERR_OUT_OF_BOUNDS;
    while (vm->status == LVL_OK) {
        lvl_step(vm);
    }
    return vm->status;
}

lvl_status_t lvl_get_status(const lvl_vm_t *vm) {
    return vm ? vm->status : LVL_ERR_OUT_OF_BOUNDS;
}

int32_t lvl_stack_peek(const lvl_vm_t *vm) {
    if (!vm || vm->sp == 0) return 0;
    return vm->stack[vm->sp - 1];
}

int32_t lvl_reg_get(const lvl_vm_t *vm, uint8_t reg_idx) {
    if (!vm || reg_idx >= LVL_NUM_REGISTERS) return 0;
    return vm->registers[reg_idx];
}

void lvl_reg_set(lvl_vm_t *vm, uint8_t reg_idx, int32_t val) {
    if (!vm || reg_idx >= LVL_NUM_REGISTERS) return;
    vm->registers[reg_idx] = val;
}

/* ========================================================================== */
/* HIGH-LEVEL COMPILER (lvl_compile)                                         */
/* ========================================================================== */

typedef struct {
    char name[64];
    size_t inst_idx;
} LvlSymbol;

typedef struct {
    LvlSymbol symbols[128];
    size_t count;
} LvlSymbolTable;

static void lvl_symbol_add(LvlSymbolTable *st, const char *name, size_t inst_idx) {
    if (st->count >= 128) return;
    strncpy(st->symbols[st->count].name, name, 63);
    st->symbols[st->count].name[63] = '\0';
    st->symbols[st->count].inst_idx = inst_idx;
    st->count++;
}

static int lvl_symbol_find(const LvlSymbolTable *st, const char *name) {
    for (size_t i = 0; i < st->count; i++) {
        if (strcmp(st->symbols[i].name, name) == 0) return (int)st->symbols[i].inst_idx;
    }
    return -1;
}

static char *lvl_trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

static void lvl_strip_comments(char *line) {
    char *p1 = strchr(line, '#'); if (p1) *p1 = '\0';
    char *p2 = strstr(line, "//"); if (p2) *p2 = '\0';
}

static int lvl_parse_reg(const char *arg) {
    if (arg[0] == 'R' || arg[0] == 'r') return atoi(arg + 1);
    return atoi(arg);
}

static char *lvl_find_label_colon(char *line) {
    bool in_quote = false;
    for (char *p = line; *p; p++) {
        if (*p == '"') in_quote = !in_quote;
        if (!in_quote && *p == ':') return p;
    }
    return NULL;
}

static size_t lvl_calc_string_bytes(const char *str) {
    size_t raw_len = 0;
    for (size_t i = 0; str[i] && str[i] != '"'; i++) {
        if (str[i] == '\\' && str[i+1] == 'n') {
            raw_len++;
            i++;
        } else {
            raw_len++;
        }
    }
    size_t total_bytes = 2 + raw_len + 1;
    if (total_bytes % 2 != 0) total_bytes++;
    return total_bytes;
}

int lvl_compile(const char *source, uint8_t *out_bytecode, size_t *out_size) {
    if (!source || !out_bytecode || !out_size) return LVL_ERR_COMPILATION_FAILED;

    LvlSymbolTable st = { .count = 0 };
    char line_buf[256];

    /* PASS 1: Calculate Instruction Indices for Labels & High-Level Statements */
    size_t pass1_inst_idx = 0;
    const char *src_ptr = source;

    while (*src_ptr) {
        size_t len = 0;
        while (*src_ptr && *src_ptr != '\n' && len < 255) line_buf[len++] = *src_ptr++;
        if (*src_ptr == '\n') src_ptr++;
        line_buf[len] = '\0';

        lvl_strip_comments(line_buf);
        char *line = lvl_trim(line_buf);
        if (strlen(line) == 0) continue;

        char *colon = lvl_find_label_colon(line);
        if (colon && *(colon + 1) == '\0') {
            *colon = '\0';
            lvl_symbol_add(&st, lvl_trim(line), pass1_inst_idx);
            continue;
        } else if (colon) {
            *colon = '\0';
            lvl_symbol_add(&st, lvl_trim(line), pass1_inst_idx);
            line = lvl_trim(colon + 1);
        }

        if (strlen(line) > 0) {
            if (strchr(line, '=') && strncmp(line, "print", 5) != 0) {
                char v[32] = {0}, rhs[128] = {0};
                if (sscanf(line, "%31[^=] = %127[^\n]", v, rhs) == 2) {
                    char r1[32] = {0}, op[8] = {0}, r2[32] = {0};
                    int tok = sscanf(rhs, "%31s %7s %31s", r1, op, r2);
                    if (tok == 3) pass1_inst_idx += 4;
                    else pass1_inst_idx += 2;
                } else pass1_inst_idx++;
            } else if (strstr(line, "++") || strstr(line, "--")) {
                pass1_inst_idx++;
            } else if (strncmp(line, "print(", 6) == 0 || strncmp(line, "PRINT_STR", 9) == 0) {
                char *quote_start = strchr(line, '"');
                if (quote_start) {
                    size_t bytes = lvl_calc_string_bytes(quote_start + 1);
                    pass1_inst_idx += bytes / 2;
                } else {
                    pass1_inst_idx += 3;
                }
            } else {
                pass1_inst_idx++;
            }
        }
    }

    /* PASS 2: Code Generation */
    size_t out_offset = 0;
    src_ptr = source;

    while (*src_ptr) {
        size_t len = 0;
        while (*src_ptr && *src_ptr != '\n' && len < 255) line_buf[len++] = *src_ptr++;
        if (*src_ptr == '\n') src_ptr++;
        line_buf[len] = '\0';

        lvl_strip_comments(line_buf);
        char *line = lvl_trim(line_buf);
        if (strlen(line) == 0) continue;

        char *colon = lvl_find_label_colon(line);
        if (colon) line = lvl_trim(colon + 1);
        if (strlen(line) == 0) continue;

        /* 1. High-Level Assignment: r0 = 100  or  r2 = r0 - r1 */
        if (strchr(line, '=') && strncmp(line, "print", 5) != 0) {
            char var_name[32] = {0}, rhs_expr[128] = {0};
            if (sscanf(line, "%31[^=] = %127[^\n]", var_name, rhs_expr) == 2) {
                int reg_id = lvl_parse_reg(lvl_trim(var_name));
                char rhs1[32] = {0}, op_str[8] = {0}, rhs2[32] = {0};
                int rhs_tokens = sscanf(rhs_expr, "%31s %7s %31s", rhs1, op_str, rhs2);

                if (rhs_tokens == 1) {
                    if (isdigit((unsigned char)rhs1[0]) || rhs1[0] == '-') {
                        out_bytecode[out_offset++] = 0x01;
                        out_bytecode[out_offset++] = 0x80 + (uint8_t)(atoi(rhs1) & 0x7F);
                    } else {
                        out_bytecode[out_offset++] = 0x01;
                        out_bytecode[out_offset++] = 0x10 + (uint8_t)lvl_parse_reg(rhs1);
                    }
                    out_bytecode[out_offset++] = 0x01;
                    out_bytecode[out_offset++] = 0x30 + (uint8_t)reg_id;
                } else if (rhs_tokens == 3) {
                    if (isdigit((unsigned char)rhs1[0]) || rhs1[0] == '-') {
                        out_bytecode[out_offset++] = 0x01;
                        out_bytecode[out_offset++] = 0x80 + (uint8_t)(atoi(rhs1) & 0x7F);
                    } else {
                        out_bytecode[out_offset++] = 0x01;
                        out_bytecode[out_offset++] = 0x10 + (uint8_t)lvl_parse_reg(rhs1);
                    }
                    if (isdigit((unsigned char)rhs2[0]) || rhs2[0] == '-') {
                        out_bytecode[out_offset++] = 0x01;
                        out_bytecode[out_offset++] = 0x80 + (uint8_t)(atoi(rhs2) & 0x7F);
                    } else {
                        out_bytecode[out_offset++] = 0x01;
                        out_bytecode[out_offset++] = 0x10 + (uint8_t)lvl_parse_reg(rhs2);
                    }
                    uint8_t op_byte = 0x01;
                    if (strcmp(op_str, "+") == 0) op_byte = 0x01;
                    else if (strcmp(op_str, "-") == 0) op_byte = 0x02;
                    else if (strcmp(op_str, "*") == 0) op_byte = 0x03;
                    else if (strcmp(op_str, "/") == 0) op_byte = 0x04;

                    out_bytecode[out_offset++] = 0x02;
                    out_bytecode[out_offset++] = op_byte;

                    out_bytecode[out_offset++] = 0x01;
                    out_bytecode[out_offset++] = 0x30 + (uint8_t)reg_id;
                }
                continue;
            }
        }

        /* 2. High-Level Increment/Decrement: r0++ or r0-- */
        if (strstr(line, "++")) {
            char inc_var[32] = {0};
            if (sscanf(line, "%31[a-zA-Z0-9]++", inc_var) == 1) {
                out_bytecode[out_offset++] = 0x02;
                out_bytecode[out_offset++] = 0x10 + (uint8_t)lvl_parse_reg(inc_var);
                continue;
            }
        }
        if (strstr(line, "--")) {
            char inc_var[32] = {0};
            if (sscanf(line, "%31[a-zA-Z0-9]--", inc_var) == 1) {
                out_bytecode[out_offset++] = 0x02;
                out_bytecode[out_offset++] = 0x20 + (uint8_t)lvl_parse_reg(inc_var);
                continue;
            }
        }

        /* 3. High-Level Print: print("text") or print(r0) */
        if (strncmp(line, "print(", 6) == 0) {
            char inside[192] = {0};
            strncpy(inside, line + 6, sizeof(inside) - 1);
            char *closing = strrchr(inside, ')');
            if (closing) *closing = '\0';
            char *clean_inside = lvl_trim(inside);

            if (clean_inside[0] == '"') {
                clean_inside++;
                char *quote_end = strrchr(clean_inside, '"');
                if (quote_end) *quote_end = '\0';

                out_bytecode[out_offset++] = 0x05;
                out_bytecode[out_offset++] = 0x03; /* PRINT_STR */

                for (size_t s = 0; clean_inside[s]; s++) {
                    if (clean_inside[s] == '\\' && clean_inside[s+1] == 'n') {
                        out_bytecode[out_offset++] = '\n';
                        s++;
                    } else out_bytecode[out_offset++] = clean_inside[s];
                }
                out_bytecode[out_offset++] = '\0';
                if (out_offset % 2 != 0) out_bytecode[out_offset++] = 0x00;
            } else {
                int pr_reg = lvl_parse_reg(clean_inside);
                out_bytecode[out_offset++] = 0x01;
                out_bytecode[out_offset++] = 0x10 + (uint8_t)pr_reg;
                out_bytecode[out_offset++] = 0x05;
                out_bytecode[out_offset++] = 0x01; /* PRINT_NUM */
                out_bytecode[out_offset++] = 0x05;
                out_bytecode[out_offset++] = 0x04; /* PRINT_NL */
            }
            continue;
        }

        /* 4. Mnemonic Fallback Parsing */
        char mnem[64] = {0};
        char arg[192] = {0};
        int tokens = sscanf(line, "%63s %191[^\n]", mnem, arg);
        if (tokens < 1) continue;

        for (int i = 0; mnem[i]; i++) mnem[i] = (char)toupper((unsigned char)mnem[i]);

        uint8_t b1 = 0, b2 = 0;
        bool is_string_inst = false;

        if (strcmp(mnem, "PUSH") == 0) {
            b1 = 0x01; b2 = 0x80 + (uint8_t)(atoi(arg) & 0x7F);
        } else if (strcmp(mnem, "POP") == 0) {
            b1 = 0x01; b2 = 0x01;
        } else if (strcmp(mnem, "DUP") == 0) {
            b1 = 0x01; b2 = 0x02;
        } else if (strcmp(mnem, "SWAP") == 0) {
            b1 = 0x01; b2 = 0x03;
        } else if (strcmp(mnem, "LOAD") == 0 || strcmp(mnem, "LOAD_REG") == 0) {
            b1 = 0x01; b2 = 0x10 + (uint8_t)lvl_parse_reg(arg);
        } else if (strcmp(mnem, "STORE") == 0 || strcmp(mnem, "STORE_REG") == 0) {
            b1 = 0x01; b2 = 0x30 + (uint8_t)lvl_parse_reg(arg);
        } else if (strcmp(mnem, "ADD") == 0) { b1 = 0x02; b2 = 0x01; }
        else if (strcmp(mnem, "SUB") == 0) { b1 = 0x02; b2 = 0x02; }
        else if (strcmp(mnem, "MUL") == 0) { b1 = 0x02; b2 = 0x03; }
        else if (strcmp(mnem, "DIV") == 0) { b1 = 0x02; b2 = 0x04; }
        else if (strcmp(mnem, "MOD") == 0) { b1 = 0x02; b2 = 0x05; }
        else if (strcmp(mnem, "INC") == 0) { b1 = 0x02; b2 = 0x10 + (uint8_t)lvl_parse_reg(arg); }
        else if (strcmp(mnem, "DEC") == 0) { b1 = 0x02; b2 = 0x20 + (uint8_t)lvl_parse_reg(arg); }
        else if (strcmp(mnem, "EQ") == 0)  { b1 = 0x03; b2 = 0x01; }
        else if (strcmp(mnem, "NEQ") == 0) { b1 = 0x03; b2 = 0x02; }
        else if (strcmp(mnem, "GT") == 0)  { b1 = 0x03; b2 = 0x03; }
        else if (strcmp(mnem, "LT") == 0)  { b1 = 0x03; b2 = 0x04; }
        else if (strcmp(mnem, "GTE") == 0) { b1 = 0x03; b2 = 0x05; }
        else if (strcmp(mnem, "LTE") == 0) { b1 = 0x03; b2 = 0x06; }
        else if (strcmp(mnem, "AND") == 0) { b1 = 0x03; b2 = 0x07; }
        else if (strcmp(mnem, "OR") == 0)  { b1 = 0x03; b2 = 0x08; }
        else if (strcmp(mnem, "NOT") == 0) { b1 = 0x03; b2 = 0x09; }
        else if (strcmp(mnem, "JMP") == 0) {
            int tgt = lvl_symbol_find(&st, arg);
            if (tgt < 0) tgt = atoi(arg);
            b1 = 0x04; b2 = 0x10 + (uint8_t)tgt;
        } else if (strcmp(mnem, "JZ") == 0) {
            int tgt = lvl_symbol_find(&st, arg);
            if (tgt < 0) tgt = atoi(arg);
            b1 = 0x04; b2 = 0x50 + (uint8_t)tgt;
        } else if (strcmp(mnem, "JNZ") == 0) {
            int tgt = lvl_symbol_find(&st, arg);
            if (tgt < 0) tgt = atoi(arg);
            b1 = 0x04; b2 = 0x90 + (uint8_t)tgt;
        } else if (strcmp(mnem, "PRINT_NUM") == 0)  { b1 = 0x05; b2 = 0x01; }
        else if (strcmp(mnem, "PRINT_STR") == 0) {
            b1 = 0x05; b2 = 0x03;
            is_string_inst = true;
        } else if (strcmp(mnem, "PRINT_CHAR") == 0) { b1 = 0x05; b2 = 0x02; }
        else if (strcmp(mnem, "PRINT_NL") == 0)   { b1 = 0x05; b2 = 0x04; }
        else if (strcmp(mnem, "HALT") == 0)       { b1 = 0x05; b2 = 0xFF; }
        else return LVL_ERR_COMPILATION_FAILED;

        out_bytecode[out_offset++] = b1;
        out_bytecode[out_offset++] = b2;

        if (is_string_inst) {
            char *str_start = strchr(arg, '"');
            if (str_start) {
                str_start++;
                char *str_end = strrchr(str_start, '"');
                if (str_end) *str_end = '\0';
            } else str_start = arg;

            size_t str_len = strlen(str_start);
            for (size_t s = 0; s < str_len; s++) {
                if (str_start[s] == '\\' && str_start[s+1] == 'n') {
                    out_bytecode[out_offset++] = '\n';
                    s++;
                } else out_bytecode[out_offset++] = str_start[s];
            }
            out_bytecode[out_offset++] = '\0';
            if (out_offset % 2 != 0) out_bytecode[out_offset++] = 0x00;
        }
    }

    *out_size = out_offset;
    return LVL_OK;
}

#endif /* LVLANG_IMPLEMENTATION */
