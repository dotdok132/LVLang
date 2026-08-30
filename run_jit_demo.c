#define LVLANG_IMPLEMENTATION
#include "lvlang.h"
#define LVL_JIT_IMPLEMENTATION
#include "lvl_jit.h"
#include <stdio.h>

int main(void) {
    /* LVLang Program: (50 + 25) * 4 = 300 */
    uint8_t bytecode[] = {
        0x01, 0xB2, /* PUSH 50 */
        0x01, 0x30, /* STORE R0 */
        0x01, 0x99, /* PUSH 25 */
        0x01, 0x31, /* STORE R1 */
        0x01, 0x10, /* LOAD R0 */
        0x01, 0x11, /* LOAD R1 */
        0x02, 0x01, /* ADD (50 + 25 = 75) */
        0x01, 0x84, /* PUSH 4 */
        0x02, 0x03, /* MUL (75 * 4 = 300) */
        0x01, 0x32, /* STORE R2 */
        0x05, 0xFF  /* HALT */
    };

    lvl_vm_t vm;
    lvl_init(&vm, bytecode, sizeof(bytecode));

    lvl_jit_engine_t jit;
    if (lvl_jit_init(&jit, 4096) != 0) {
        printf("Error initializing JIT memory.\n");
        return 1;
    }

    if (lvl_jit_compile(&jit, bytecode, sizeof(bytecode)) != 0) {
        printf("Error compiling bytecode to Native x86-64 machine code.\n");
        return 1;
    }

    printf("[+] JIT compiled 22 bytes of LVLang to %zu bytes of x86-64 machine code.\n", jit.size);
    printf("[+] Executing Native JIT on CPU hardware...\n");

    lvl_jit_run(&jit, &vm);

    printf("\n=== Execution Result ===\n");
    printf("R0 = %d\n", vm.registers[0]);
    printf("R1 = %d\n", vm.registers[1]);
    printf("R2 = %d (Expected: 300)\n", vm.registers[2]);

    lvl_jit_free(&jit);
    return 0;
}
