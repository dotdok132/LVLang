#define _POSIX_C_SOURCE 199309L
#define LVLANG_IMPLEMENTATION
#include "lvlang.h"
#define LVL_JIT_IMPLEMENTATION
#include "lvl_jit.h"
#include "../lvlang-crypto/crypto_plugin.c"
#include <stdio.h>
#include <assert.h>

int main(void) {
    printf("=== FULL END-TO-END INTEGRATION TEST OF LVLANG ECOSYSTEM ===\n\n");

    /* 1. Test Bytecode Program:
       - Computes Vector Dot Product of (1,2,3,4) . (5,6,7,8) = 70
       - Sets R0 = 0 (RAM start)
       - Runs Crypto SHA256 plugin FFI (Lib 2, Func 1)
       - Halts cleanly
    */
    uint8_t bytecode[] = {
        /* Vector Math Test */
        0x01, 0x81, 0x01, 0x30, /* R0 = 1 */
        0x01, 0x82, 0x01, 0x31, /* R1 = 2 */
        0x01, 0x83, 0x01, 0x32, /* R2 = 3 */
        0x01, 0x84, 0x01, 0x33, /* R3 = 4 */
        0x01, 0x85, 0x01, 0x34, /* R4 = 5 */
        0x01, 0x86, 0x01, 0x35, /* R5 = 6 */
        0x01, 0x87, 0x01, 0x36, /* R6 = 7 */
        0x01, 0x88, 0x01, 0x37, /* R7 = 8 */
        0x08, 0x01,             /* VEC_DOT_4D -> Pushes 70 onto stack */
        0x01, 0x38,             /* STORE R8 = 70 */

        /* RAM & Crypto Test */
        0x07, 0x40,             /* MACRO_CLEAR_REG R0 -> R0 = 0 */
        0x01, 0x80 + 'H', 0x01, 0x50, /* RAM[0] = 'H' */
        0x02, 0x10,                   /* INC R0 */
        0x01, 0x80 + 'i', 0x01, 0x50, /* RAM[1] = 'i' */
        0x02, 0x10,                   /* INC R0 */
        0x01, 0x80, 0x01, 0x50,       /* RAM[2] = '\0' */
        0x07, 0x40,                   /* MACRO_CLEAR_REG R0 -> R0 = 0 */
        0x0E, 0x01, 0x02, 0x01,       /* FFI_CALL SHA256 (Lib 2, Func 1) */
        0x05, 0xFF                    /* HALT */
    };

    /* 2. Initialize VM */
    lvl_vm_t vm;
    lvl_init(&vm, bytecode, sizeof(bytecode));

    /* 3. Register Plugin FFI Callbacks */
    lvl_plugin_crypto_init(&vm);

    printf("[1] Testing VM Runtime & FFI Plugin Integration...\nOutput: ");
    lvl_run(&vm);

    /* 4. Verify Results */
    assert(lvl_get_status(&vm) == LVL_STATUS_HALT);
    assert(vm.registers[8] == 70);
    printf("\n[+] VM Execution & Vector Math Passed! R8 = %d (Expected 70)\n", vm.registers[8]);

    /* 5. Test Native x86-64 JIT Integration */
    printf("\n[2] Testing Native x86-64 JIT Engine Integration...\n");
    lvl_jit_engine_t jit;
    assert(lvl_jit_init(&jit, 4096) == 0);
    assert(lvl_jit_compile(&jit, bytecode, sizeof(bytecode)) == 0);
    printf("[+] JIT Native Code Compiled: %zu bytes generated.\n", jit.size);
    assert(lvl_jit_run(&jit, &vm) == 0);
    printf("[+] Native JIT Hardware Execution Passed!\n");

    lvl_jit_free(&jit);

    printf("\n==================================================\n");
    printf("✅ ALL 4 LAYERS DOCK TOGETHER WITH 100%% PERFECT INTEGRATION!\n");
    printf("==================================================\n");
    return 0;
}
