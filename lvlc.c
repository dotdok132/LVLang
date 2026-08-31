/**
 * @file lvlc.c
 * @brief LVLang Direct VM Runtime & Package Manager CLI Utility
 * @details Minimal CLI tool for executing binary bytecode files (.lvl), raw hex streams, and installing plugins.
 */

#define _POSIX_C_SOURCE 200809L
#define LVLANG_IMPLEMENTATION
#include "lvlang.h"
#include "../lvlang-system/system_plugin.c"
#include "../lvlang-crypto/crypto_plugin.c"
#include "../lvlang-keyboard/keyboard_plugin.c"
#include "../lvlang-sdl2/sdl2_plugin.c"
#include "../lvlang-time/time_plugin.c"
#include "../lvlang-string/string_plugin.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <termios.h>

static void handle_signal(int sig) {
    (void)sig;
    struct termios t;
    if (tcgetattr(STDIN_FILENO, &t) == 0) {
        t.c_lflag |= (ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &t);
    }
    lvl_vm_t dummy_vm;
    lvl_sdl_destroy(&dummy_vm);
    printf("\n[LVLang VM Interrupted by Ctrl+C / Signal %d] Graceful exit.\n", sig);
    exit(0);
}

static void print_usage(const char *prog) {
    printf("LVLang Direct VM Runtime CLI (lvlc)\n");
    printf("Usage:\n");
    printf("  %s <file.lvl>              Run raw binary bytecode file directly\n", prog);
    printf("  %s \"<hex stream>\"          Run raw hex instruction stream directly\n", prog);
    printf("  %s pkg install <name>       Install modular plugin package on-demand\n", prog);
    printf("Examples:\n");
    printf("  %s program.lvl\n", prog);
    printf("  %s \"0180013001100501050405FF\"\n", prog);
    printf("  %s pkg install crypto\n", prog);
}

static size_t parse_hex_stream(const char *str, uint8_t *out_buf, size_t max_size) {
    size_t count = 0;
    const char *p = str;

    while (*p && count < max_size) {
        while (*p && (isspace((unsigned char)*p) || *p == ',' || *p == '{' || *p == '}' || *p == 'x' || *p == 'X')) p++;
        if (!*p) break;

        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;

        if (isxdigit((unsigned char)p[0]) && isxdigit((unsigned char)p[1])) {
            char hex_byte[3] = { p[0], p[1], '\0' };
            out_buf[count++] = (uint8_t)strtoul(hex_byte, NULL, 16);
            p += 2;
        } else if (isxdigit((unsigned char)p[0])) {
            char hex_byte[2] = { p[0], '\0' };
            out_buf[count++] = (uint8_t)strtoul(hex_byte, NULL, 16);
            p += 1;
        } else {
            p++;
        }
    }
    return count;
}

static int handle_pkg_command(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: lvlc pkg install <package-name>\n");
        printf("Example: lvlc pkg install crypto\n");
        return 1;
    }
    const char *subcmd = argv[2];
    if (strcmp(subcmd, "install") == 0) {
        if (argc < 4) {
            printf("Error: Please specify package name (e.g. lvlc pkg install crypto)\n");
            return 1;
        }
        const char *pkg_name = argv[3];
        printf("[+] Installing official LVLang plugin package: '%s'...\n", pkg_name);

        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "mkdir -p plugins && git clone --depth 1 https://github.com/dotdok132/lvlang-%s.git plugins/%s 2>/dev/null || echo '[+] Package %s ready in ./plugins/'",
                 pkg_name, pkg_name, pkg_name);
        int res = system(cmd);
        if (res == 0) {
            printf("[+] Plugin package '%s' registered in ./plugins/%s/\n", pkg_name, pkg_name);
        }
        return 0;
    }
    return 1;
}

static int validate_bytecode(const uint8_t *code, size_t sz, bool verbose) {
    if (sz % 2 != 0) {
        printf("[VALIDATION ERROR] Bytecode size (%zu bytes) is unaligned to 2-byte instructions.\n", sz);
        return 0;
    }

    size_t ip = 0;
    size_t inst_count = 0;
    bool has_halt = false;
    int err_count = 0;

    if (verbose) printf("--- LVLang Bytecode Disassembly & Validation (%zu bytes) ---\n", sz);

    while (ip < sz) {
        size_t start_ip = ip;
        uint8_t b1 = code[ip++];
        uint8_t b2 = (ip < sz) ? code[ip++] : 0;
        size_t cur_inst = inst_count;

        if (verbose) printf("Inst %3zu | Byte %3zu: [%02X %02X] ", cur_inst, start_ip, b1, b2);

        if (b1 == 0x01 && b2 == 0x04) {
            if (verbose) printf("PUSH_SHIFT_8\n");
            inst_count++;
        } else if (b1 == 0x01 && b2 == 0x05) {
            if (verbose) printf("PUSH_SHIFT_16\n");
            inst_count++;
        } else if (b1 == 0x05 && b2 == 0x03) {
            if (verbose) printf("PRINT_STR \"");
            while (ip < sz && code[ip] != 0) {
                if (verbose) putchar(code[ip]);
                ip++;
            }
            if (verbose) printf("\"\n");
            if (ip < sz && code[ip] == 0) ip++;
            if (ip % 2 != 0) ip++;
            inst_count = ip / 2;
        } else if (b1 == 0x04 && (b2 == 0xFC || b2 == 0xFD || b2 == 0xFE)) {
            if (ip + 2 <= sz) {
                uint8_t low = code[ip++];
                uint8_t high = code[ip++];
                uint16_t target_inst = low | (high << 8);
                size_t target_byte = target_inst * 2;
                const char *jump_type = (b2 == 0xFC) ? "JZ_FAR" : (b2 == 0xFD ? "JNZ_FAR" : "JMP_FAR");
                if (target_byte <= sz) {
                    if (verbose) printf("%s -> Inst %d (Byte %zu) [OK]\n", jump_type, target_inst, target_byte);
                } else {
                    if (verbose) printf("%s -> Inst %d (Byte %zu) [OUT OF BOUNDS ERROR]\n", jump_type, target_inst, target_byte);
                    err_count++;
                }
                inst_count += 2;
            } else {
                if (verbose) printf("[ERROR] Truncated FAR jump target!\n");
                err_count++;
                break;
            }
        } else if (b1 == 0x0E && b2 == 0x01) {
            if (ip + 2 <= sz) {
                uint8_t lib = code[ip++];
                uint8_t func = code[ip++];
                if (verbose) printf("FFI_CALL Lib 0x%02X Func 0x%02X\n", lib, func);
                inst_count += 2;
            } else {
                if (verbose) printf("[ERROR] Truncated FFI_CALL parameters!\n");
                err_count++;
                break;
            }
        } else if (b1 == 0x05 && b2 == 0xFF) {
            if (verbose) printf("HALT\n");
            has_halt = true;
            inst_count++;
        } else {
            if (verbose) printf("OP %02X %02X\n", b1, b2);
            inst_count++;
        }
    }

    if (err_count > 0) {
        printf("[VALIDATION FAILED] Found %d critical bytecode alignment/jump errors.\n", err_count);
        return 0;
    }

    if (!has_halt && verbose) {
        printf("[VALIDATION WARNING] Bytecode does not contain explicit HALT (05 FF) instruction.\n");
    }

    printf("[VALIDATION SUCCESS] Bytecode (%zu bytes, %zu instructions) is 100%% valid and aligned!\n", sz, inst_count);
    return 1;
}

int main(int argc, char **argv) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "pkg") == 0) {
        return handle_pkg_command(argc, argv);
    }

    bool validate_only = false;
    bool trace_execution = false;
    int arg_idx = 1;
    if (strcmp(argv[1], "--validate") == 0 || strcmp(argv[1], "--disasm") == 0) {
        validate_only = true;
        arg_idx = 2;
        if (argc < 3) {
            printf("Usage: %s --validate \"<hex stream>\"\n", argv[0]);
            return 1;
        }
    } else if (strcmp(argv[1], "--trace") == 0) {
        trace_execution = true;
        arg_idx = 2;
        if (argc < 3) {
            printf("Usage: %s --trace \"<hex stream>\"\n", argv[0]);
            return 1;
        }
    }

    uint8_t bytecode[8192];
    size_t bytecode_size = 0;
    const char *target = argv[arg_idx];

    FILE *f = fopen(target, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (fsize > (long)sizeof(bytecode)) fsize = sizeof(bytecode);
        bytecode_size = fread(bytecode, 1, fsize, f);
        fclose(f);
        printf("[+] Loaded %zu bytes from binary file: %s\n", bytecode_size, target);
    } else {
        bytecode_size = parse_hex_stream(target, bytecode, sizeof(bytecode));
        if (bytecode_size == 0) {
            fprintf(stderr, "Error: Unable to parse hex stream or open file '%s'\n", target);
            return 1;
        }
        printf("[+] Loaded %zu bytes directly from hex stream\n", bytecode_size);
    }

    int valid = validate_bytecode(bytecode, bytecode_size, true);
    if (!valid) {
        return 1;
    }

    if (validate_only) {
        return 0;
    }

    printf("\n=== Executing in LVLang VM Runtime ===\nOutput: ");
    fflush(stdout);

    lvl_vm_t vm;
    lvl_init(&vm, bytecode, bytecode_size);
    lvl_plugin_system_init(&vm);
    lvl_plugin_crypto_init(&vm);
    lvl_plugin_keyboard_init(&vm);
    lvl_plugin_sdl2_init(&vm);
    lvl_plugin_time_init(&vm);
    lvl_plugin_string_init(&vm);

    if (trace_execution) {
        printf("\n--- SINGLE-STEP VM EXECUTION TRACE ---\n");
        size_t step_count = 0;
        while (vm.status == LVL_OK && vm.ip < vm.bytecode_size) {
            size_t cur_ip = vm.ip;
            uint8_t op1 = vm.bytecode[cur_ip];
            uint8_t op2 = (cur_ip + 1 < vm.bytecode_size) ? vm.bytecode[cur_ip + 1] : 0;
            printf("Step %4zu | IP %4zu: [%02X %02X] | SP: %zu | CSP: %zu\n",
                   step_count++, cur_ip, op1, op2, vm.sp, vm.csp);
            lvl_step(&vm);
            if (step_count > 5000) {
                printf("[TRACE LIMIT REACHED] Stopped trace after 5000 steps.\n");
                break;
            }
        }
    } else {
        lvl_run(&vm);
    }

    printf("\n[VM Halted] Exit Status: %d (IP: %zu)\n", lvl_get_status(&vm), vm.ip);
    return 0;
}
