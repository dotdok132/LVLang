/**
 * @file lvl_jit.h
 * @brief LVLang Native x86-64 Machine-Code JIT Compiler
 * @details Translates LVLang 2-byte bytecode directly into executable x86-64 machine code (mmap PROT_EXEC).
 */

#ifndef LVL_JIT_H
#define LVL_JIT_H

#include "lvlang.h"
#include <sys/mman.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*lvl_jit_fn)(lvl_vm_t *vm);

typedef struct {
    uint8_t *code_buf;
    size_t capacity;
    size_t size;
    lvl_jit_fn exec_fn;
} lvl_jit_engine_t;

int  lvl_jit_init(lvl_jit_engine_t *jit, size_t capacity);
int  lvl_jit_compile(lvl_jit_engine_t *jit, const uint8_t *bytecode, size_t bytecode_size);
int  lvl_jit_run(lvl_jit_engine_t *jit, lvl_vm_t *vm);
void lvl_jit_free(lvl_jit_engine_t *jit);

#ifdef __cplusplus
}
#endif

#endif /* LVL_JIT_H */

/* ========================================================================== */
/*                              IMPLEMENTATION                                */
/* ========================================================================== */

#ifdef LVL_JIT_IMPLEMENTATION

int lvl_jit_init(lvl_jit_engine_t *jit, size_t capacity) {
    if (!jit) return -1;
    if (capacity == 0) capacity = 65536;

    jit->capacity = capacity;
    jit->size = 0;
    jit->code_buf = (uint8_t *)mmap(NULL, capacity,
                                   PROT_READ | PROT_WRITE | PROT_EXEC,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (jit->code_buf == MAP_FAILED) {
        jit->code_buf = NULL;
        return -1;
    }
    jit->exec_fn = (lvl_jit_fn)jit->code_buf;
    return 0;
}

static inline void emit_byte(lvl_jit_engine_t *jit, uint8_t b) {
    if (jit->size < jit->capacity) {
        jit->code_buf[jit->size++] = b;
    }
}

static inline void emit_bytes(lvl_jit_engine_t *jit, const uint8_t *bytes, size_t len) {
    for (size_t i = 0; i < len; i++) emit_byte(jit, bytes[i]);
}

static inline void emit_int32(lvl_jit_engine_t *jit, int32_t val) {
    emit_byte(jit, (uint8_t)(val & 0xFF));
    emit_byte(jit, (uint8_t)((val >> 8) & 0xFF));
    emit_byte(jit, (uint8_t)((val >> 16) & 0xFF));
    emit_byte(jit, (uint8_t)((val >> 24) & 0xFF));
}

int lvl_jit_compile(lvl_jit_engine_t *jit, const uint8_t *bytecode, size_t bytecode_size) {
    if (!jit || !jit->code_buf || !bytecode) return -1;

    jit->size = 0;

    /* x86-64 Function Prologue: push rbp; mov rbp, rsp */
    emit_byte(jit, 0x55); /* push rbp */
    emit_byte(jit, 0x48); emit_byte(jit, 0x89); emit_byte(jit, 0xE5); /* mov rbp, rsp */

    size_t ip = 0;
    while (ip + 1 < bytecode_size) {
        uint8_t b1 = bytecode[ip++];
        uint8_t b2 = bytecode[ip++];

        /* 1. Stack & Register Management (Group 0x01) */
        if (b1 == 0x01) {
            if (b2 >= 0x80) {
                /* PUSH_IMM: push imm32 */
                int32_t imm = (int32_t)(b2 & 0x7F);
                emit_byte(jit, 0x68);
                emit_int32(jit, imm);
            } else if (b2 == 0x04) {
                /* PUSH_INT32: push imm32 */
                if (ip + 3 < bytecode_size) {
                    uint8_t u0 = bytecode[ip++];
                    uint8_t u1 = bytecode[ip++];
                    uint8_t u2 = bytecode[ip++];
                    uint8_t u3 = bytecode[ip++];
                    int32_t val32 = (int32_t)((uint32_t)u0 | ((uint32_t)u1 << 8) | ((uint32_t)u2 << 16) | ((uint32_t)u3 << 24));
                    emit_byte(jit, 0x68);
                    emit_int32(jit, val32);
                }
            } else if (b2 == 0x01) {
                /* POP: add rsp, 8 */
                emit_byte(jit, 0x48); emit_byte(jit, 0x83); emit_byte(jit, 0xC4); emit_byte(jit, 0x08);
            } else if (b2 >= 0x10 && b2 <= 0x1F) {
                /* LOAD_REG R: mov rax, [rdi + reg_offset]; push rax */
                uint8_t reg = b2 - 0x10;
                size_t offset = offsetof(lvl_vm_t, registers) + (reg * sizeof(int32_t));
                emit_byte(jit, 0x8B); emit_byte(jit, 0x87); emit_int32(jit, (int32_t)offset); /* mov eax, [rdi + offset] */
                emit_byte(jit, 0x50); /* push rax */
            } else if (b2 >= 0x30 && b2 <= 0x3F) {
                /* STORE_REG R: pop rax; mov [rdi + reg_offset], eax */
                uint8_t reg = b2 - 0x30;
                size_t offset = offsetof(lvl_vm_t, registers) + (reg * sizeof(int32_t));
                emit_byte(jit, 0x58); /* pop rax */
                emit_byte(jit, 0x89); emit_byte(jit, 0x87); emit_int32(jit, (int32_t)offset); /* mov [rdi + offset], eax */
            }
        }
        /* 2. Arithmetic (Group 0x02) */
        else if (b1 == 0x02) {
            if (b2 == 0x01) {
                /* ADD: pop rcx; pop rax; add eax, ecx; push rax */
                emit_byte(jit, 0x59); /* pop rcx */
                emit_byte(jit, 0x58); /* pop rax */
                emit_byte(jit, 0x01); emit_byte(jit, 0xC8); /* add eax, ecx */
                emit_byte(jit, 0x50); /* push rax */
            } else if (b2 == 0x02) {
                /* SUB: pop rcx; pop rax; sub eax, ecx; push rax */
                emit_byte(jit, 0x59); /* pop rcx */
                emit_byte(jit, 0x58); /* pop rax */
                emit_byte(jit, 0x29); emit_byte(jit, 0xC8); /* sub eax, ecx */
                emit_byte(jit, 0x50); /* push rax */
            } else if (b2 == 0x03) {
                /* MUL: pop rcx; pop rax; imul eax, ecx; push rax */
                emit_byte(jit, 0x59); /* pop rcx */
                emit_byte(jit, 0x58); /* pop rax */
                emit_byte(jit, 0x0F); emit_byte(jit, 0xAF); emit_byte(jit, 0xC1); /* imul eax, ecx */
                emit_byte(jit, 0x50); /* push rax */
            } else if (b2 >= 0x10 && b2 <= 0x1F) {
                /* INC_REG R: add dword ptr [rdi + offset], 1 */
                uint8_t reg = b2 - 0x10;
                size_t offset = offsetof(lvl_vm_t, registers) + (reg * sizeof(int32_t));
                emit_byte(jit, 0x83); emit_byte(jit, 0x87); emit_int32(jit, (int32_t)offset); emit_byte(jit, 0x01);
            } else if (b2 >= 0x20 && b2 <= 0x2F) {
                /* DEC_REG R: sub dword ptr [rdi + offset], 1 */
                uint8_t reg = b2 - 0x20;
                size_t offset = offsetof(lvl_vm_t, registers) + (reg * sizeof(int32_t));
                emit_byte(jit, 0x83); emit_byte(jit, 0xAF); emit_int32(jit, (int32_t)offset); emit_byte(jit, 0x01);
            }
        }
        /* 3. System & Halt (Group 0x05) */
        else if (b1 == 0x05 && b2 == 0xFF) {
            /* HALT: Function Epilogue */
            emit_byte(jit, 0x48); emit_byte(jit, 0x89); emit_byte(jit, 0xEC); /* mov rsp, rbp */
            emit_byte(jit, 0x5D); /* pop rbp */
            emit_byte(jit, 0xC3); /* ret */
            break;
        }
    }

    /* Default Epilogue if HALT was not reached */
    emit_byte(jit, 0x48); emit_byte(jit, 0x89); emit_byte(jit, 0xEC);
    emit_byte(jit, 0x5D);
    emit_byte(jit, 0xC3);

    return 0;
}

int lvl_jit_run(lvl_jit_engine_t *jit, lvl_vm_t *vm) {
    if (!jit || !jit->exec_fn || !vm) return -1;
    jit->exec_fn(vm);
    vm->status = LVL_STATUS_HALT;
    return 0;
}

void lvl_jit_free(lvl_jit_engine_t *jit) {
    if (jit && jit->code_buf) {
        munmap(jit->code_buf, jit->capacity);
        jit->code_buf = NULL;
        jit->capacity = 0;
        jit->size = 0;
    }
}

#endif /* LVL_JIT_IMPLEMENTATION */
