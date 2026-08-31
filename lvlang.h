/**
 * @file lvlang.h
 * @brief LVLang (Low-Volume Language) - Virtual Machine & High-Level Compiler
 */

#ifndef LVLANG_H
#define LVLANG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>

typedef union {
    int32_t i;
    float f;
} lvl_float_conv_t;

static inline float lvl_bits_to_float(int32_t bits) {
    lvl_float_conv_t u;
    u.i = bits;
    return u.f;
}

static inline int32_t lvl_float_to_bits(float val) {
    lvl_float_conv_t u;
    u.f = val;
    return u.i;
}

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

#ifndef LVL_RAM_SIZE
#define LVL_RAM_SIZE 1024
#endif

typedef enum {
    LVL_OK                       =  0,
    LVL_STATUS_HALT              =  1,
    LVL_STATUS_YIELD             =  2,
    LVL_ERR_INVALID_OPCODE       = -1,
    LVL_ERR_STACK_OVERFLOW       = -2,
    LVL_ERR_STACK_UNDERFLOW      = -3,
    LVL_ERR_CALL_STACK_OVERFLOW  = -4,
    LVL_ERR_CALL_STACK_UNDERFLOW = -5,
    LVL_ERR_DIVISION_BY_ZERO     = -6,
    LVL_ERR_OUT_OF_BOUNDS        = -7,
    LVL_ERR_COMPILATION_FAILED   = -8,
    LVL_ERR_OUT_OF_MEMORY        = -9,
    LVL_ERR_INVALID_HEAP_ID      = -10
} lvl_status_t;

#ifndef LVL_MAX_FFI_FUNCS
#define LVL_MAX_FFI_FUNCS 64
#endif

#ifndef LVL_MAX_HEAP_CHUNKS
#define LVL_MAX_HEAP_CHUNKS 256
#endif

struct lvl_vm;
typedef void (*lvl_native_fn)(struct lvl_vm *vm);

typedef struct {
    uint8_t lib_id;
    uint8_t func_id;
    lvl_native_fn fn;
} LvlFFIFunc;

typedef void (*lvl_print_num_fn)(int32_t val);
typedef void (*lvl_print_char_fn)(char c);

#ifndef LVL_MAX_MACROS
#define LVL_MAX_MACROS 32
#endif

typedef struct lvl_vm {
    int32_t stack[LVL_STACK_SIZE];
    size_t sp;

    size_t call_stack[LVL_CALL_STACK_SIZE];
    size_t csp;

    int32_t registers[LVL_NUM_REGISTERS];
    int32_t ram[LVL_RAM_SIZE];
    size_t trap_ip;

    size_t macro_table[LVL_MAX_MACROS];

    LvlFFIFunc ffi_table[LVL_MAX_FFI_FUNCS];
    size_t ffi_count;

    int32_t *heap_chunks[LVL_MAX_HEAP_CHUNKS];
    size_t heap_chunk_sizes[LVL_MAX_HEAP_CHUNKS];

    const uint8_t *bytecode;
    size_t bytecode_size;
    size_t ip;

    lvl_status_t status;

    lvl_print_num_fn print_num;
    lvl_print_char_fn print_char;
} lvl_vm_t;

void lvl_init(lvl_vm_t *vm, const uint8_t *bytecode, size_t size);
void lvl_destroy(lvl_vm_t *vm);
int  lvl_register_ffi(lvl_vm_t *vm, uint8_t lib_id, uint8_t func_id, lvl_native_fn fn);
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

#if defined(LVLANG_IMPLEMENTATION) && !defined(LVLANG_IMPLEMENTATION_GUARD)
#define LVLANG_IMPLEMENTATION_GUARD

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
    vm->trap_ip = 0;
    vm->ffi_count = 0;
    vm->bytecode = bytecode;
    vm->bytecode_size = size;
    vm->status = LVL_OK;

    for (size_t i = 0; i < LVL_NUM_REGISTERS; i++) vm->registers[i] = 0;
    for (size_t i = 0; i < LVL_RAM_SIZE; i++) vm->ram[i] = 0;
    for (size_t i = 0; i < LVL_STACK_SIZE; i++) vm->stack[i] = 0;
    for (size_t i = 0; i < LVL_CALL_STACK_SIZE; i++) vm->call_stack[i] = 0;
    for (size_t i = 0; i < LVL_MAX_MACROS; i++) vm->macro_table[i] = 0;
    for (size_t i = 0; i < LVL_MAX_HEAP_CHUNKS; i++) {
        vm->heap_chunks[i] = NULL;
        vm->heap_chunk_sizes[i] = 0;
    }

    vm->print_num = lvl_default_print_num;
    vm->print_char = lvl_default_print_char;
}

void lvl_destroy(lvl_vm_t *vm) {
    if (!vm) return;
    for (size_t i = 0; i < LVL_MAX_HEAP_CHUNKS; i++) {
        if (vm->heap_chunks[i]) {
            free(vm->heap_chunks[i]);
            vm->heap_chunks[i] = NULL;
            vm->heap_chunk_sizes[i] = 0;
        }
    }
}

int lvl_register_ffi(lvl_vm_t *vm, uint8_t lib_id, uint8_t func_id, lvl_native_fn fn) {
    if (!vm || !fn || vm->ffi_count >= LVL_MAX_FFI_FUNCS) return -1;
    vm->ffi_table[vm->ffi_count].lib_id = lib_id;
    vm->ffi_table[vm->ffi_count].func_id = func_id;
    vm->ffi_table[vm->ffi_count].fn = fn;
    vm->ffi_count++;
    return 0;
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

static inline int32_t lvl_float_to_raw(float f) {
    union { float f; int32_t i; } u;
    u.f = f;
    return u.i;
}

static inline float lvl_raw_to_float(int32_t i) {
    union { float f; int32_t i; } u;
    u.i = i;
    return u.f;
}

int lvl_step(lvl_vm_t *vm) {
    if (!vm || vm->status != LVL_OK) return vm->status;
    if (vm->ip + 1 >= vm->bytecode_size) {
        vm->status = LVL_STATUS_HALT;
        return vm->status;
    }

    uint8_t b1 = vm->bytecode[vm->ip++];
    uint8_t b2 = vm->bytecode[vm->ip++];

    int32_t a = 0, b = 0;
    size_t target_byte = 0;

    switch (b1) {
        /* MODULE 0x01: STACK, REGISTERS & RAM MEMORY */
        case 0x01:
            if (b2 >= 0x80) {
                /* PUSH_IMM 7-bit */
                lvl_push(vm, (int32_t)(b2 & 0x7F));
            } else if (b2 == 0x04) {
                /* PUSH_SHIFT_8: Pop a, push (a << 8) */
                if (lvl_pop(vm, &a)) lvl_push(vm, a << 8);
            } else if (b2 == 0x05) {
                /* PUSH_SHIFT_16: Pop a, push (a << 16) */
                if (lvl_pop(vm, &a)) lvl_push(vm, a << 16);
            } else if (b2 == 0x01) {
                lvl_pop(vm, &a);
            } else if (b2 == 0x02) {
                if (lvl_pop(vm, &a)) { lvl_push(vm, a); lvl_push(vm, a); }
            } else if (b2 == 0x03) {
                if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) { lvl_push(vm, b); lvl_push(vm, a); }
            } else if (b2 == 0x06) {
                int32_t count, start_idx;
                if (lvl_pop(vm, &count) && lvl_pop(vm, &start_idx)) {
                    for (int32_t i = 0; i < count; i++) {
                        if (start_idx + i >= 0 && start_idx + i < LVL_RAM_SIZE) {
                            vm->ram[start_idx + i] = 0;
                        } else {
                            vm->status = LVL_ERR_OUT_OF_BOUNDS;
                            break;
                        }
                    }
                }
            } else if (b2 == 0x07) {
                int32_t count, src_idx, dst_idx;
                if (lvl_pop(vm, &count) && lvl_pop(vm, &src_idx) && lvl_pop(vm, &dst_idx)) {
                    for (int32_t i = 0; i < count; i++) {
                        if (src_idx + i >= 0 && src_idx + i < LVL_RAM_SIZE &&
                            dst_idx + i >= 0 && dst_idx + i < LVL_RAM_SIZE) {
                            vm->ram[dst_idx + i] = vm->ram[src_idx + i];
                        } else {
                            vm->status = LVL_ERR_OUT_OF_BOUNDS;
                            break;
                        }
                    }
                }
            } else if (b2 >= 0x10 && b2 <= 0x1F) {
                uint8_t reg = b2 - 0x10;
                lvl_push(vm, vm->registers[reg]);
            } else if (b2 >= 0x30 && b2 <= 0x3F) {
                uint8_t reg = b2 - 0x30;
                if (lvl_pop(vm, &a)) vm->registers[reg] = a;
            } else if (b2 >= 0x40 && b2 <= 0x4F) {
                /* LOAD_RAM R: Push RAM[R_idx] onto stack */
                uint8_t reg = b2 - 0x40;
                size_t ram_idx = (size_t)(vm->registers[reg] >= 0 ? vm->registers[reg] : 0) % LVL_RAM_SIZE;
                lvl_push(vm, vm->ram[ram_idx]);
            } else if (b2 >= 0x50 && b2 <= 0x5F) {
                /* STORE_RAM R: Pop value into RAM[R_idx] */
                uint8_t reg = b2 - 0x50;
                size_t ram_idx = (size_t)(vm->registers[reg] >= 0 ? vm->registers[reg] : 0) % LVL_RAM_SIZE;
                if (lvl_pop(vm, &a)) vm->ram[ram_idx] = a;
            } else if (b2 == 0x60) {
                /* MALLOC: pop size, push chunk_id */
                if (lvl_pop(vm, &a)) {
                    if (a <= 0) {
                        lvl_push(vm, -1);
                    } else {
                        int chunk_id = -1;
                        for (int i = 0; i < LVL_MAX_HEAP_CHUNKS; i++) {
                            if (vm->heap_chunks[i] == NULL) {
                                chunk_id = i;
                                break;
                            }
                        }
                        if (chunk_id != -1) {
                            vm->heap_chunks[chunk_id] = (int32_t *)calloc(a, sizeof(int32_t));
                            if (vm->heap_chunks[chunk_id]) {
                                vm->heap_chunk_sizes[chunk_id] = (size_t)a;
                                lvl_push(vm, chunk_id);
                            } else {
                                vm->status = LVL_ERR_OUT_OF_MEMORY;
                            }
                        } else {
                            vm->status = LVL_ERR_OUT_OF_MEMORY;
                        }
                    }
                }
            } else if (b2 == 0x61) {
                /* FREE: pop chunk_id */
                if (lvl_pop(vm, &a)) {
                    if (a >= 0 && a < LVL_MAX_HEAP_CHUNKS && vm->heap_chunks[a] != NULL) {
                        free(vm->heap_chunks[a]);
                        vm->heap_chunks[a] = NULL;
                        vm->heap_chunk_sizes[a] = 0;
                    } else {
                        vm->status = LVL_ERR_INVALID_HEAP_ID;
                    }
                }
            } else if (b2 == 0x62) {
                /* LOAD_HEAP: pop offset, pop chunk_id, push heap[chunk_id][offset] */
                if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) {
                    if (a >= 0 && a < LVL_MAX_HEAP_CHUNKS && vm->heap_chunks[a] != NULL) {
                        if (b >= 0 && (size_t)b < vm->heap_chunk_sizes[a]) {
                            lvl_push(vm, vm->heap_chunks[a][b]);
                        } else {
                            vm->status = LVL_ERR_OUT_OF_BOUNDS;
                        }
                    } else {
                        vm->status = LVL_ERR_INVALID_HEAP_ID;
                    }
                }
            } else if (b2 == 0x63) {
                /* STORE_HEAP: pop value, pop offset, pop chunk_id, heap[chunk_id][offset] = value */
                int32_t val;
                if (lvl_pop(vm, &val) && lvl_pop(vm, &b) && lvl_pop(vm, &a)) {
                    if (a >= 0 && a < LVL_MAX_HEAP_CHUNKS && vm->heap_chunks[a] != NULL) {
                        if (b >= 0 && (size_t)b < vm->heap_chunk_sizes[a]) {
                            vm->heap_chunks[a][b] = val;
                        } else {
                            vm->status = LVL_ERR_OUT_OF_BOUNDS;
                        }
                    } else {
                        vm->status = LVL_ERR_INVALID_HEAP_ID;
                    }
                }
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
            } else if (b2 == 0x06) {
                if (vm->sp > 0) vm->stack[vm->sp - 1]++;
                else vm->status = LVL_ERR_STACK_UNDERFLOW;
            } else if (b2 == 0x07) {
                if (vm->sp > 0) vm->stack[vm->sp - 1]--;
                else vm->status = LVL_ERR_STACK_UNDERFLOW;
            } else if (b2 >= 0x10 && b2 <= 0x1F) {
                uint8_t reg = b2 - 0x10;
                vm->registers[reg]++;
            } else if (b2 >= 0x20 && b2 <= 0x2F) {
                uint8_t reg = b2 - 0x20;
                vm->registers[reg]--;
            } else if (b2 == 0x40) {
                /* FADD: IEEE 754 Float Addition */
                if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) {
                    float res = lvl_raw_to_float(a) + lvl_raw_to_float(b);
                    lvl_push(vm, lvl_float_to_raw(res));
                }
            } else if (b2 == 0x41) {
                /* FSUB: IEEE 754 Float Subtraction */
                if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) {
                    float res = lvl_raw_to_float(a) - lvl_raw_to_float(b);
                    lvl_push(vm, lvl_float_to_raw(res));
                }
            } else if (b2 == 0x42) {
                /* FMUL: IEEE 754 Float Multiplication */
                if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) {
                    float res = lvl_raw_to_float(a) * lvl_raw_to_float(b);
                    lvl_push(vm, lvl_float_to_raw(res));
                }
            } else if (b2 == 0x43) {
                /* FDIV: IEEE 754 Float Division */
                if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) {
                    float res = lvl_raw_to_float(a) / lvl_raw_to_float(b);
                    lvl_push(vm, lvl_float_to_raw(res));
                }
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

        /* MODULE 0x04: FLOW CONTROL, SUBROUTINES, TRAPS & BITWISE LOGIC */
        case 0x04:
            if (b2 == 0x01) { if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) lvl_push(vm, a & b); }
            else if (b2 == 0x02) { if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) lvl_push(vm, a | b); }
            else if (b2 == 0x03) { if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) lvl_push(vm, a ^ b); }
            else if (b2 == 0x04) { if (lvl_pop(vm, &a)) lvl_push(vm, ~a); }
            else if (b2 == 0x05) { if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) lvl_push(vm, a << b); }
            else if (b2 == 0x06) { if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) lvl_push(vm, a >> b); }
            else if (b2 == 0x07) { if (lvl_pop(vm, &a)) lvl_push(vm, a == 0 ? 1 : 0); }
            else if (b2 == 0x08) { if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) lvl_push(vm, (a != 0 && b != 0) ? 1 : 0); }
            else if (b2 == 0x09) { if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) lvl_push(vm, (a != 0 || b != 0) ? 1 : 0); }
            else if (b2 == 0x00) {
                /* RET: Return from subroutine */
                if (vm->csp > 0) {
                    vm->ip = vm->call_stack[--vm->csp];
                } else {
                    vm->status = LVL_ERR_CALL_STACK_UNDERFLOW;
                }
            } else if (b2 == 0x0A) {
                /* YIELD: Async coroutine pause */
                vm->status = LVL_STATUS_YIELD;
            } else if (b2 == 0x0B) {
                /* SET_TRAP Target: Set Exception Catch Handler */
                if (vm->ip + 1 < vm->bytecode_size) {
                    uint8_t low = vm->bytecode[vm->ip++];
                    uint8_t high = vm->bytecode[vm->ip++];
                    size_t far_inst = (size_t)(low | (high << 8));
                    vm->trap_ip = far_inst * 2;
                }
            } else if (b2 == 0x0C) {
                /* CLEAR_TRAP: Disable Exception Handler */
                vm->trap_ip = 0;
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
            } else if (b2 >= 0xD0 && b2 <= 0xFB) {
                /* CALL Target: Macro / Subroutine Call */
                if (vm->csp < LVL_CALL_STACK_SIZE) {
                    vm->call_stack[vm->csp++] = vm->ip;
                    target_byte = (size_t)(b2 - 0xD0) * 2;
                    if (target_byte < vm->bytecode_size) vm->ip = target_byte;
                    else vm->status = LVL_ERR_OUT_OF_BOUNDS;
                } else {
                    vm->status = LVL_ERR_CALL_STACK_OVERFLOW;
                }
            } else if (b2 == 0xFC) {
                /* JZ_FAR: 16-bit Target */
                if (lvl_pop(vm, &a)) {
                    if (vm->ip + 1 < vm->bytecode_size) {
                        uint8_t low = vm->bytecode[vm->ip++];
                        uint8_t high = vm->bytecode[vm->ip++];
                        if (a == 0) {
                            size_t far_inst = (size_t)(low | (high << 8));
                            vm->ip = far_inst * 2;
                        }
                    } else vm->status = LVL_ERR_OUT_OF_BOUNDS;
                }
            } else if (b2 == 0xFD) {
                /* JNZ_FAR: 16-bit Target */
                if (lvl_pop(vm, &a)) {
                    if (vm->ip + 1 < vm->bytecode_size) {
                        uint8_t low = vm->bytecode[vm->ip++];
                        uint8_t high = vm->bytecode[vm->ip++];
                        if (a != 0) {
                            size_t far_inst = (size_t)(low | (high << 8));
                            vm->ip = far_inst * 2;
                        }
                    } else vm->status = LVL_ERR_OUT_OF_BOUNDS;
                }
            } else if (b2 == 0xFE) {
                /* JMP_FAR: 16-bit Target */
                if (vm->ip + 1 < vm->bytecode_size) {
                    uint8_t low = vm->bytecode[vm->ip++];
                    uint8_t high = vm->bytecode[vm->ip++];
                    size_t far_inst = (size_t)(low | (high << 8));
                    vm->ip = far_inst * 2;
                } else vm->status = LVL_ERR_OUT_OF_BOUNDS;
            } else if (b2 == 0xFF) {
                /* CALL_FAR: 16-bit Target */
                if (vm->ip + 1 < vm->bytecode_size) {
                    uint8_t low = vm->bytecode[vm->ip++];
                    uint8_t high = vm->bytecode[vm->ip++];
                    if (vm->csp < LVL_CALL_STACK_SIZE) {
                        vm->call_stack[vm->csp++] = vm->ip;
                        size_t far_inst = (size_t)(low | (high << 8));
                        vm->ip = far_inst * 2;
                    } else vm->status = LVL_ERR_CALL_STACK_OVERFLOW;
                } else vm->status = LVL_ERR_OUT_OF_BOUNDS;
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
                case 0x05: {
                    int32_t val = 0;
                    if (scanf("%d", &val) == 1) {
                        lvl_push(vm, val);
                    } else {
                        lvl_push(vm, 0);
                    }
                    break;
                }
                case 0x06: {
                    int32_t val = 0;
                    if (scanf("%d", &val) == 1) {
                        lvl_push(vm, val);
                    } else {
                        lvl_push(vm, 0);
                    }
                    break;
                }
                case 0x07: {
                    float val = 0.0f;
                    if (scanf("%f", &val) == 1) {
                        lvl_push(vm, lvl_float_to_bits(val));
                    } else {
                        lvl_push(vm, 0);
                    }
                    break;
                }
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

        /* MODULE 0x07: MACRO / SHORTCUT OPCODES & DYNAMIC USER MACROS */
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
            } else if (b2 >= 0x70 && b2 <= 0x7F) {
                /* DEF_MACRO ID (0x70 + ID): Register macro start, skip body until RET (0x04 0x00) */
                uint8_t macro_id = b2 - 0x70;
                vm->macro_table[macro_id] = vm->ip;
                while (vm->ip + 1 < vm->bytecode_size) {
                    uint8_t op1 = vm->bytecode[vm->ip++];
                    uint8_t op2 = vm->bytecode[vm->ip++];
                    if (op1 == 0x04 && op2 == 0x00) {
                        break;
                    }
                    if (op1 == 0x01 && op2 == 0x04) vm->ip += 4;
                    else if (op1 == 0x05 && op2 == 0x03) {
                        while (vm->ip < vm->bytecode_size && vm->bytecode[vm->ip] != 0) vm->ip++;
                        if (vm->ip < vm->bytecode_size) vm->ip++;
                        if (vm->ip % 2 != 0) vm->ip++;
                    }
                }
            } else if (b2 >= 0x80 && b2 <= 0x8F) {
                /* EXEC_MACRO ID (0x80 + ID): Call registered macro in 1 instruction */
                uint8_t macro_id = b2 - 0x80;
                if (vm->csp < LVL_CALL_STACK_SIZE) {
                    vm->call_stack[vm->csp++] = vm->ip;
                    vm->ip = vm->macro_table[macro_id];
                } else {
                    vm->status = LVL_ERR_CALL_STACK_OVERFLOW;
                }
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

        /* MODULE 0x09: RELATIVE JUMPS BACKWARD (09xN -> Jump backward N instructions) */
        case 0x09: {
            size_t n = (size_t)b2;
            if (vm->ip >= n * 2) vm->ip -= n * 2;
            else vm->status = LVL_ERR_OUT_OF_BOUNDS;
            break;
        }

        /* MODULE 0x0B: RELATIVE JUMPS FORWARD (0BxN -> Jump forward N instructions) */
        case 0x0B: {
            size_t n = (size_t)b2;
            if (vm->ip + n * 2 <= vm->bytecode_size) vm->ip += n * 2;
            else vm->status = LVL_ERR_OUT_OF_BOUNDS;
            break;
        }

        /* MODULE 0x0C: CONDITIONAL RELATIVE JUMPS (STACK OPERAND) */
        case 0x0C: {
            if (b2 == 0x01) {
                /* JZ_REL_BACK N: pop count from stack */
                if (lvl_pop(vm, &b) && lvl_pop(vm, &a) && a == 0) {
                    size_t n = (size_t)b;
                    if (vm->ip >= n * 2) vm->ip -= n * 2;
                    else vm->status = LVL_ERR_OUT_OF_BOUNDS;
                }
            } else if (b2 == 0x02) {
                /* JNZ_REL_BACK N: pop count from stack */
                if (lvl_pop(vm, &b) && lvl_pop(vm, &a) && a != 0) {
                    size_t n = (size_t)b;
                    if (vm->ip >= n * 2) vm->ip -= n * 2;
                    else vm->status = LVL_ERR_OUT_OF_BOUNDS;
                }
            } else if (b2 == 0x03) {
                /* JZ_REL_FWD N: pop count from stack */
                if (lvl_pop(vm, &b) && lvl_pop(vm, &a) && a == 0) {
                    size_t n = (size_t)b;
                    if (vm->ip + n * 2 <= vm->bytecode_size) vm->ip += n * 2;
                    else vm->status = LVL_ERR_OUT_OF_BOUNDS;
                }
            } else if (b2 == 0x04) {
                /* JNZ_REL_FWD N: pop count from stack */
                if (lvl_pop(vm, &b) && lvl_pop(vm, &a) && a != 0) {
                    size_t n = (size_t)b;
                    if (vm->ip + n * 2 <= vm->bytecode_size) vm->ip += n * 2;
                    else vm->status = LVL_ERR_OUT_OF_BOUNDS;
                }
            }
            break;
        }

        /* MODULE 0x0D: DIRECT JZ RELATIVE FORWARD (0DxN -> Pop a, if a==0 jump forward N inst) */
        case 0x0D: {
            size_t n = (size_t)b2;
            if (lvl_pop(vm, &a) && a == 0) {
                if (vm->ip + n * 2 <= vm->bytecode_size) vm->ip += n * 2;
                else vm->status = LVL_ERR_OUT_OF_BOUNDS;
            }
            break;
        }

        /* MODULE 0x0F: DIRECT JNZ RELATIVE FORWARD (0FxN -> Pop a, if a!=0 jump forward N inst) */
        case 0x0F: {
            size_t n = (size_t)b2;
            if (lvl_pop(vm, &a) && a != 0) {
                if (vm->ip + n * 2 <= vm->bytecode_size) vm->ip += n * 2;
                else vm->status = LVL_ERR_OUT_OF_BOUNDS;
            }
            break;
        }

        /* MODULE 0x0A: IEEE-754 32-BIT FLOAT MATH & ARITHMETIC */
        case 0x0A:
            switch (b2) {
                case 0x01: { /* FADD: Pop b, a (floats); push a + b */
                    if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) {
                        float fa = lvl_bits_to_float(a);
                        float fb = lvl_bits_to_float(b);
                        lvl_push(vm, lvl_float_to_bits(fa + fb));
                    }
                    break;
                }
                case 0x02: { /* FSUB: Pop b, a (floats); push a - b */
                    if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) {
                        float fa = lvl_bits_to_float(a);
                        float fb = lvl_bits_to_float(b);
                        lvl_push(vm, lvl_float_to_bits(fa - fb));
                    }
                    break;
                }
                case 0x03: { /* FMUL: Pop b, a (floats); push a * b */
                    if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) {
                        float fa = lvl_bits_to_float(a);
                        float fb = lvl_bits_to_float(b);
                        lvl_push(vm, lvl_float_to_bits(fa * fb));
                    }
                    break;
                }
                case 0x04: { /* FDIV: Pop b, a (floats); push a / b */
                    if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) {
                        float fa = lvl_bits_to_float(a);
                        float fb = lvl_bits_to_float(b);
                        if (fb != 0.0f) lvl_push(vm, lvl_float_to_bits(fa / fb));
                        else vm->status = LVL_ERR_DIVISION_BY_ZERO;
                    }
                    break;
                }
                case 0x05: { /* I2F: Pop integer a; convert to float and push */
                    if (lvl_pop(vm, &a)) {
                        lvl_push(vm, lvl_float_to_bits((float)a));
                    }
                    break;
                }
                case 0x06: { /* F2I: Pop float a; convert to integer and push */
                    if (lvl_pop(vm, &a)) {
                        float fa = lvl_bits_to_float(a);
                        lvl_push(vm, (int32_t)fa);
                    }
                    break;
                }
                case 0x07: { /* PRINT_FLOAT: Pop float a; print float to stdout */
                    if (lvl_pop(vm, &a)) {
                        float fa = lvl_bits_to_float(a);
                        printf("%f", fa);
                    }
                    break;
                }
                case 0x08: { /* FSQRT: Pop float a; push sqrt(a) */
                    if (lvl_pop(vm, &a)) {
                        float fa = lvl_bits_to_float(a);
                        lvl_push(vm, lvl_float_to_bits(sqrtf(fa)));
                    }
                    break;
                }
                case 0x09: { /* FABS: Pop float a; push fabsf(a) */
                    if (lvl_pop(vm, &a)) {
                        float fa = lvl_bits_to_float(a);
                        lvl_push(vm, lvl_float_to_bits(fabsf(fa)));
                    }
                    break;
                }
                case 0x0A: { /* FEQ: Pop b, a (floats); push 1 if a == b else 0 */
                    if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) {
                        float fa = lvl_bits_to_float(a);
                        float fb = lvl_bits_to_float(b);
                        lvl_push(vm, (fa == fb) ? 1 : 0);
                    }
                    break;
                }
                case 0x0B: { /* FGT: Pop b, a (floats); push 1 if a > b else 0 */
                    if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) {
                        float fa = lvl_bits_to_float(a);
                        float fb = lvl_bits_to_float(b);
                        lvl_push(vm, (fa > fb) ? 1 : 0);
                    }
                    break;
                }
                case 0x0C: { /* FLT: Pop b, a (floats); push 1 if a < b else 0 */
                    if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) {
                        float fa = lvl_bits_to_float(a);
                        float fb = lvl_bits_to_float(b);
                        lvl_push(vm, (fa < fb) ? 1 : 0);
                    }
                    break;
                }
                case 0x0D: { /* FFLOOR */
                    if (lvl_pop(vm, &a)) {
                        lvl_push(vm, lvl_float_to_bits(floorf(lvl_bits_to_float(a))));
                    }
                    break;
                }
                case 0x0E: { /* FCEIL */
                    if (lvl_pop(vm, &a)) {
                        lvl_push(vm, lvl_float_to_bits(ceilf(lvl_bits_to_float(a))));
                    }
                    break;
                }
                case 0x0F: { /* FROUND */
                    if (lvl_pop(vm, &a)) {
                        lvl_push(vm, lvl_float_to_bits(roundf(lvl_bits_to_float(a))));
                    }
                    break;
                }
                case 0x10: { /* FMOD */
                    if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) {
                        lvl_push(vm, lvl_float_to_bits(fmodf(lvl_bits_to_float(a), lvl_bits_to_float(b))));
                    }
                    break;
                }
                case 0x11: { /* FPOW */
                    if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) {
                        lvl_push(vm, lvl_float_to_bits(powf(lvl_bits_to_float(a), lvl_bits_to_float(b))));
                    }
                    break;
                }
                case 0x12: { /* FSIN */
                    if (lvl_pop(vm, &a)) {
                        lvl_push(vm, lvl_float_to_bits(sinf(lvl_bits_to_float(a))));
                    }
                    break;
                }
                case 0x13: { /* FCOS */
                    if (lvl_pop(vm, &a)) {
                        lvl_push(vm, lvl_float_to_bits(cosf(lvl_bits_to_float(a))));
                    }
                    break;
                }
                case 0x14: { /* FTAN */
                    if (lvl_pop(vm, &a)) {
                        lvl_push(vm, lvl_float_to_bits(tanf(lvl_bits_to_float(a))));
                    }
                    break;
                }
                case 0x15: { /* FMIN */
                    if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) {
                        float fa = lvl_bits_to_float(a), fb = lvl_bits_to_float(b);
                        lvl_push(vm, lvl_float_to_bits(fa < fb ? fa : fb));
                    }
                    break;
                }
                case 0x16: { /* FMAX */
                    if (lvl_pop(vm, &b) && lvl_pop(vm, &a)) {
                        float fa = lvl_bits_to_float(a), fb = lvl_bits_to_float(b);
                        lvl_push(vm, lvl_float_to_bits(fa > fb ? fa : fb));
                    }
                    break;
                }
                case 0x17: { /* FLOG */
                    if (lvl_pop(vm, &a)) {
                        lvl_push(vm, lvl_float_to_bits(logf(lvl_bits_to_float(a))));
                    }
                    break;
                }
                default:
                    vm->status = LVL_ERR_INVALID_OPCODE;
                    break;
            }
            break;

        /* MODULE 0x0E: EXTERNAL LIBRARIES & NATIVE FFI PLUGINS */
        case 0x0E:
            if (b2 == 0x01) {
                /* FFI_CALL [LibID] [FuncID] */
                if (vm->ip + 1 < vm->bytecode_size) {
                    uint8_t lib_id = vm->bytecode[vm->ip++];
                    uint8_t func_id = vm->bytecode[vm->ip++];
                    bool found = false;
                    for (size_t f = 0; f < vm->ffi_count; f++) {
                        if (vm->ffi_table[f].lib_id == lib_id && vm->ffi_table[f].func_id == func_id) {
                            vm->ffi_table[f].fn(vm);
                            found = true;
                            break;
                        }
                    }
                    if (!found) vm->status = LVL_ERR_INVALID_OPCODE;
                } else vm->status = LVL_ERR_OUT_OF_BOUNDS;
            } else {
                vm->status = LVL_ERR_INVALID_OPCODE;
            }
            break;

        default: vm->status = LVL_ERR_INVALID_OPCODE; break;
    }

    if (vm->status < 0 && vm->trap_ip > 0) {
        lvl_status_t err = vm->status;
        vm->status = LVL_OK;
        lvl_push(vm, (int32_t)err);
        vm->ip = vm->trap_ip;
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



#endif /* LVLANG_IMPLEMENTATION */
