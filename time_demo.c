#define _POSIX_C_SOURCE 199309L
#define LVLANG_IMPLEMENTATION
#include "lvlang.h"
#define LVL_JIT_IMPLEMENTATION
#include "lvl_jit.h"
#include <stdio.h>
#include <time.h>

static double get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

int main(void) {
    uint8_t bytecode[] = {
        0x01, 0xB2, /* PUSH 50 */
        0x01, 0x30, /* STORE R0 */
        0x01, 0x99, /* PUSH 25 */
        0x01, 0x31, /* STORE R1 */
        0x01, 0x10, /* LOAD R0 */
        0x01, 0x11, /* LOAD R1 */
        0x02, 0x01, /* ADD */
        0x01, 0x84, /* PUSH 4 */
        0x02, 0x03, /* MUL */
        0x01, 0x32, /* STORE R2 */
        0x05, 0xFF  /* HALT */
    };

    lvl_vm_t vm;
    lvl_init(&vm, bytecode, sizeof(bytecode));

    lvl_jit_engine_t jit;
    lvl_jit_init(&jit, 4096);

    /* Measure JIT Compilation Time */
    double t0 = get_time_ns();
    lvl_jit_compile(&jit, bytecode, sizeof(bytecode));
    double t1 = get_time_ns();

    double compile_time_ns = t1 - t0;

    /* Measure 10,000,000 executions for exact nanosecond precision */
    const int N = 10000000;
    double t2 = get_time_ns();
    for (int i = 0; i < N; i++) {
        jit.exec_fn(&vm);
    }
    double t3 = get_time_ns();

    double exec_time_per_run_ns = (t3 - t2) / N;

    printf("=== EXACT TIMING RESULTS ===\n");
    printf("1. JIT Compilation Time : %.2f ns (%.3f us)\n", compile_time_ns, compile_time_ns / 1000.0);
    printf("2. Hardware CPU Exec Time: %.2f ns per program run\n", exec_time_per_run_ns);
    printf("   (That is %.2f MILLION executions PER SECOND!)\n", 1e9 / exec_time_per_run_ns);

    lvl_jit_free(&jit);
    return 0;
}
