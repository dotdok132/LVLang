#define LVLANG_IMPLEMENTATION
#include "lvlang.h"
#define LVL_JIT_IMPLEMENTATION
#include "lvl_jit.h"
#include <stdio.h>
#include <time.h>

int main(void) {
    printf("=== LVLANG NATIVE X86-64 JIT COMPILER BENCHMARK ===\n\n");

    /* Program: Set R0 = 1000, R1 = 500, R2 = R0 * R1 + 12345 */
    uint8_t bytecode[] = {
        0x01, 0x04, 0xE8, 0x03, 0x00, 0x00, /* PUSH_INT32 1000 */
        0x01, 0x30,                         /* STORE R0 */
        0x01, 0x04, 0xF4, 0x01, 0x00, 0x00, /* PUSH_INT32 500 */
        0x01, 0x31,                         /* STORE R1 */
        0x01, 0x10,                         /* LOAD R0 */
        0x01, 0x11,                         /* LOAD R1 */
        0x02, 0x03,                         /* MUL */
        0x01, 0x04, 0x39, 0x30, 0x00, 0x00, /* PUSH_INT32 12345 */
        0x02, 0x01,                         /* ADD */
        0x01, 0x32,                         /* STORE R2 */
        0x05, 0xFF                          /* HALT */
    };

    lvl_vm_t vm;
    lvl_init(&vm, bytecode, sizeof(bytecode));

    lvl_jit_engine_t jit;
    if (lvl_jit_init(&jit, 4096) != 0) {
        printf("Error: Failed to allocate executable memory for JIT.\n");
        return 1;
    }

    if (lvl_jit_compile(&jit, bytecode, sizeof(bytecode)) != 0) {
        printf("Error: JIT Compilation failed.\n");
        return 1;
    }

    printf("[+] Native x86-64 JIT Machine Code Generated: %zu bytes emitted.\n", jit.size);

    /* Execute JIT 1,000,000 times to measure performance */
    clock_t start = clock();
    const int iterations = 1000000;

    for (int i = 0; i < iterations; i++) {
        lvl_jit_run(&jit, &vm);
    }

    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;

    printf("\n=== JIT EXECUTION RESULTS ===\n");
    printf("Iterations : %d\n", iterations);
    printf("Time Spent : %.6f seconds (%.2f ns/run)\n", time_spent, (time_spent * 1e9) / iterations);
    printf("Result R2  : %d (Expected: 512345)\n", vm.registers[2]);

    lvl_jit_free(&jit);
    return 0;
}
