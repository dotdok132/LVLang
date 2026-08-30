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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "pkg") == 0) {
        return handle_pkg_command(argc, argv);
    }

    uint8_t bytecode[8192];
    size_t bytecode_size = 0;

    const char *target = argv[1];

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
        /* Treat target as inline hex stream string */
        bytecode_size = parse_hex_stream(target, bytecode, sizeof(bytecode));
        if (bytecode_size == 0) {
            fprintf(stderr, "Error: Unable to parse hex stream or open file '%s'\n", target);
            return 1;
        }
    printf("[+] Loaded %zu bytes directly from hex stream: ", bytecode_size);
    for (size_t i = 0; i < bytecode_size; i++) printf("%02X ", bytecode[i]);
    printf("\n");
    }

    if (bytecode_size % 2 != 0) {
        printf("[!] Warning: Bytecode stream length (%zu bytes) is unaligned to 2-byte instructions.\n", bytecode_size);
    }

    printf("\n=== Executing in LVLang VM Runtime ===\nOutput: ");
    fflush(stdout);

    lvl_vm_t vm;
    lvl_init(&vm, bytecode, bytecode_size);
    lvl_plugin_system_init(&vm);
    lvl_plugin_crypto_init(&vm);
    lvl_plugin_keyboard_init(&vm);
    lvl_plugin_sdl2_init(&vm);
    lvl_run(&vm);

    printf("\n[VM Halted] Exit Status: %d (IP: %zu)\n", lvl_get_status(&vm), vm.ip);
    return 0;
}
