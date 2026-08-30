#define LVLANG_IMPLEMENTATION
#include "lvlang.h"
#include <stdio.h>

/* Mock 3rd-Party Library Function: Calculates power(base, exp) */
void custom_3rd_party_power_fn(lvl_vm_t *vm) {
    int32_t base = 0, exp = 0;
    if (lvl_pop(vm, &exp) && lvl_pop(vm, &base)) {
        int32_t res = 1;
        for (int i = 0; i < exp; i++) res *= base;
        lvl_push(vm, res);
    }
}

int main(void) {
    /* Bytecode Program calling 3rd-party FFI function:
       1. PUSH 2 (base)
       2. PUSH 10 (exp)
       3. FFI_CALL LibID=1, FuncID=5 (0x0E 0x01 0x01 0x05)
       4. PRINT_NUM
       5. PRINT_NL
       6. HALT
    */
    uint8_t bytecode[] = {
        0x01, 0x82,             /* PUSH 2 */
        0x01, 0x8A,             /* PUSH 10 */
        0x0E, 0x01, 0x01, 0x05, /* FFI_CALL LibID=1, FuncID=5 */
        0x05, 0x01,             /* PRINT_NUM */
        0x05, 0x04,             /* PRINT_NL */
        0x05, 0xFF              /* HALT */
    };

    lvl_vm_t vm;
    lvl_init(&vm, bytecode, sizeof(bytecode));

    /* Register 3rd-party library function: LibID=1, FuncID=5 */
    lvl_register_ffi(&vm, 1, 5, custom_3rd_party_power_fn);

    printf("=== Executing LVLang 3rd-Party FFI Library Call ===\nOutput: ");
    lvl_run(&vm);
    printf("[VM Halted] Exit Status: %d\n", lvl_get_status(&vm));

    return 0;
}
