#define _POSIX_C_SOURCE 199309L
#define LVLANG_IMPLEMENTATION
#include "lvlang.h"
#include <stdio.h>
#include <time.h>

int main(void) {
    printf("=== LVLang C-Runtime Benchmark (1,000,000 Iterations) ===\n\n");

    /* Bytecode array for loop:
       Inst 0 (0x00): DEC_REG R0 (0x02 0x20)
       Inst 1 (0x02): LOAD_REG R0 (0x01 0x10)
       Inst 2 (0x04): JNZ_IMM -> Inst 0 (0x04 0x90)
       Inst 3 (0x06): HALT (0x05 0xFF)
    */
    const uint8_t loop_bytecode[] = {
        0x02, 0x20, /* Inst 0: DEC_REG R0 */
        0x01, 0x10, /* Inst 1: LOAD_REG R0 */
        0x04, 0x90, /* Inst 2: JNZ -> Inst 0 */
        0x05, 0xFF  /* Inst 3: HALT */
    };

    lvl_vm_t vm;
    lvl_init(&vm, loop_bytecode, sizeof(loop_bytecode));

    const int iterations = 1000000;
    lvl_reg_set(&vm, 0, iterations); /* Set R0 = 1,000,000 */

    printf("Starting VM loop execution for N = %d iterations...\n", iterations);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    lvl_run(&vm);

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed_sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double total_instructions = (double)iterations * 3.0; /* 3 instructions per iteration */
    double mips = (total_instructions / elapsed_sec) / 1e6;
    double ns_per_inst = (elapsed_sec * 1e9) / total_instructions;

    printf("\n=== Benchmark Results ===\n");
    printf("Iterations     : %d\n", iterations);
    printf("Total Time     : %.4f seconds (%.2f ms)\n", elapsed_sec, elapsed_sec * 1000.0);
    printf("Instructions   : %.0f\n", total_instructions);
    printf("Throughput     : %.2f MIPS (Million Instructions / Sec)\n", mips);
    printf("Latency        : %.2f ns per instruction\n", ns_per_inst);
    printf("Final R0 Value : %d\n", lvl_reg_get(&vm, 0));
    printf("VM Exit Status : %d\n", lvl_get_status(&vm));

    return 0;
}
