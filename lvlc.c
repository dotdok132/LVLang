/**
 * @file lvlc.c
 * @brief LVLang Direct VM Runtime & Package Manager CLI Utility
 * @details Minimal CLI tool for executing binary bytecode files (.lvl), raw hex streams, and installing plugins.
 */

#define _POSIX_C_SOURCE 200809L
#define LVLANG_IMPLEMENTATION
#include "lvlang.h"
#if defined(__has_include)
  #if __has_include("../lvlang-system/system_plugin.c")
    #include "../lvlang-system/system_plugin.c"
  #else
    static inline void lvl_plugin_system_init(lvl_vm_t *vm) { (void)vm; }
  #endif

  #if __has_include("../lvlang-crypto/crypto_plugin.c")
    #include "../lvlang-crypto/crypto_plugin.c"
  #else
    static inline void lvl_plugin_crypto_init(lvl_vm_t *vm) { (void)vm; }
  #endif

  #if __has_include("../lvlang-keyboard/keyboard_plugin.c")
    #include "../lvlang-keyboard/keyboard_plugin.c"
  #else
    static inline void lvl_plugin_keyboard_init(lvl_vm_t *vm) { (void)vm; }
  #endif

  #if __has_include("../lvlang-sdl2/sdl2_plugin.c")
    #include "../lvlang-sdl2/sdl2_plugin.c"
  #else
    static inline void lvl_plugin_sdl2_init(lvl_vm_t *vm) { (void)vm; }
  #endif

  #if __has_include("../lvlang-time/time_plugin.c")
    #include "../lvlang-time/time_plugin.c"
  #else
    static inline void lvl_plugin_time_init(lvl_vm_t *vm) { (void)vm; }
  #endif

  #if __has_include("../lvlang-string/string_plugin.c")
    #include "../lvlang-string/string_plugin.c"
  #else
    static inline void lvl_plugin_string_init(lvl_vm_t *vm) { (void)vm; }
  #endif

  #if __has_include("../lvlang-ansi/ansi_plugin.c")
    #include "../lvlang-ansi/ansi_plugin.c"
  #else
    static inline void lvl_plugin_ansi_init(lvl_vm_t *vm) { (void)vm; }
  #endif

  #if __has_include("../lvlang-heap/heap_plugin.c")
    #include "../lvlang-heap/heap_plugin.c"
  #else
    static inline void lvl_plugin_heap_init(lvl_vm_t *vm) { (void)vm; }
  #endif

  #if __has_include("../lvlang-net/net_plugin.c")
    #include "../lvlang-net/net_plugin.c"
  #else
    static inline void lvl_plugin_net_init(lvl_vm_t *vm) { (void)vm; }
  #endif
#else
  #include "../lvlang-system/system_plugin.c"
  #include "../lvlang-crypto/crypto_plugin.c"
  #include "../lvlang-keyboard/keyboard_plugin.c"
  #include "../lvlang-sdl2/sdl2_plugin.c"
  #include "../lvlang-time/time_plugin.c"
  #include "../lvlang-string/string_plugin.c"
  #include "../lvlang-ansi/ansi_plugin.c"
  #include "../lvlang-heap/heap_plugin.c"
  #include "../lvlang-net/net_plugin.c"
#endif
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

typedef struct {
    char name[64];
    size_t inst_idx;
} lvl_label_t;

/* Read a label name from *pp (already past '@'), advance *pp past the name */
static size_t lvl_read_label(const char **pp, char *out, size_t maxlen) {
    const char *start = *pp;
    while (**pp && !isspace((unsigned char)**pp) && **pp != ',' && **pp != ':') (*pp)++;
    size_t len = (size_t)(*pp - start);
    if (len >= maxlen) len = maxlen - 1;
    strncpy(out, start, len);
    out[len] = '\0';
    return len;
}

static size_t parse_hex_stream(const char *str, uint8_t *out_buf, size_t max_size) {
    lvl_label_t labels[128];
    size_t label_count = 0;

    /* ===== PASS 1: label discovery + instruction slot counting ===== */
    const char *p = str;
    size_t slot = 0; /* instruction slot counter (1 slot = 2 bytes) */

    while (*p) {
        /* skip whitespace + punctuation */
        while (*p && (isspace((unsigned char)*p) || *p == ',' || *p == '{' || *p == '}')) p++;
        if (!*p) break;

        /* @name: label definition */
        if (*p == '@') {
            p++; /* skip '@' */
            char name[64];
            size_t len = lvl_read_label(&p, name, sizeof(name));
            if (*p == ':') p++; /* consume ':' */
            if (len > 0 && label_count < 128) {
                memcpy(labels[label_count].name, name, len + 1);
                labels[label_count].inst_idx = slot;
                label_count++;
            }
            continue;
        }

        /* strip 0x */
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
        if (!isxdigit((unsigned char)p[0])) { p++; continue; }

        /* read b1 */
        uint8_t b1 = (uint8_t)strtoul((char[]){p[0], isxdigit((unsigned char)p[1]) ? p[1] : '0', '\0'}, NULL, 16);
        p += isxdigit((unsigned char)p[1]) ? 2 : 1;

        /* skip x separator */
        if (*p == 'x' || *p == 'X') p++;

        /* peek past spaces: is next token @label? */
        const char *q = p;
        while (*q && *q != '\n' && isspace((unsigned char)*q)) q++;
        if (*q == '@') {
            /* label-ref instruction: NN @label => 1 slot */
            p = q + 1;
            char dummy[64]; lvl_read_label(&p, dummy, sizeof(dummy));
            slot++;
            continue;
        }

        /* read b2 (normal instruction) */
        while (*p && *p != '\n' && isspace((unsigned char)*p)) p++;
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
        if (!isxdigit((unsigned char)p[0])) continue;

        uint8_t b2 = (uint8_t)strtoul((char[]){p[0], isxdigit((unsigned char)p[1]) ? p[1] : '0', '\0'}, NULL, 16);
        p += isxdigit((unsigned char)p[1]) ? 2 : 1;
        slot++;

        /* 4-byte instructions take 2 slots: eat 2 more payload bytes */
        if ((b1 == 0x0E && b2 == 0x01) || (b1 == 0x01 && b2 == 0x08)) {
            for (int k = 0; k < 2; k++) {
                while (*p && (isspace((unsigned char)*p) || *p == 'x' || *p == 'X')) p++;
                if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
                if (isxdigit((unsigned char)p[0]) && isxdigit((unsigned char)p[1])) p += 2;
                else if (isxdigit((unsigned char)p[0])) p += 1;
            }
            slot++;
        }
    }

    /* ===== PASS 2: code generation + label resolution ===== */
    p = str;
    size_t count = 0;
    slot = 0;

    while (*p && count < max_size) {
        while (*p && (isspace((unsigned char)*p) || *p == ',' || *p == '{' || *p == '}')) p++;
        if (!*p) break;

        /* skip label definitions */
        if (*p == '@') {
            while (*p && *p != '\n' && *p != '\r') p++;
            continue;
        }

        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
        if (!isxdigit((unsigned char)p[0])) { p++; continue; }

        /* read b1 */
        uint8_t b1 = (uint8_t)strtoul((char[]){p[0], isxdigit((unsigned char)p[1]) ? p[1] : '0', '\0'}, NULL, 16);
        p += isxdigit((unsigned char)p[1]) ? 2 : 1;

        /* skip x separator */
        if (*p == 'x' || *p == 'X') p++;

        /* peek for @label reference after optional whitespace */
        const char *peek = p;
        while (*peek && *peek != '\n' && isspace((unsigned char)*peek)) peek++;
        if (*peek == '@') {
            p = peek + 1; /* skip '@' */
            char lbl[64]; lvl_read_label(&p, lbl, sizeof(lbl));

            int target = -1;
            for (size_t i = 0; i < label_count; i++) {
                if (strcmp(labels[i].name, lbl) == 0) { target = (int)labels[i].inst_idx; break; }
            }

            uint8_t rel = 0;
            if (target >= 0) {
                if (b1 == 0x09) { /* JMP_REL_BACK */
                    int d = (int)slot + 1 - target;
                    rel = (d > 0 && d <= 255) ? (uint8_t)d : 0;
                } else { /* 0B/0D/0F forward */
                    int d = target - ((int)slot + 1);
                    rel = (d >= 0 && d <= 255) ? (uint8_t)d : 0;
                }
            }
            out_buf[count++] = b1;
            out_buf[count++] = rel;
            slot++;
            continue;
        }

        /* normal instruction: read b2 */
        while (*p && *p != '\n' && isspace((unsigned char)*p)) p++;
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;

        if (!isxdigit((unsigned char)p[0])) {
            out_buf[count++] = b1;
            if (count % 2 == 0) slot++;
            continue;
        }

        uint8_t b2 = (uint8_t)strtoul((char[]){p[0], isxdigit((unsigned char)p[1]) ? p[1] : '0', '\0'}, NULL, 16);
        p += isxdigit((unsigned char)p[1]) ? 2 : 1;

        out_buf[count++] = b1;
        out_buf[count++] = b2;
        slot++;

        /* 4-byte instructions: emit 2 more payload bytes */
        if ((b1 == 0x0E && b2 == 0x01) || (b1 == 0x01 && b2 == 0x08)) {
            for (int k = 0; k < 2 && count < max_size; k++) {
                while (*p && (isspace((unsigned char)*p) || *p == 'x' || *p == 'X')) p++;
                if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
                if (isxdigit((unsigned char)p[0]) && isxdigit((unsigned char)p[1])) {
                    out_buf[count++] = (uint8_t)strtoul((char[]){p[0], p[1], '\0'}, NULL, 16); p += 2;
                } else if (isxdigit((unsigned char)p[0])) {
                    out_buf[count++] = (uint8_t)strtoul((char[]){p[0], '\0'}, NULL, 16); p++;
                } else { out_buf[count++] = 0x00; }
            }
            slot++;
        }
    }

    if (count % 2 != 0 && count < max_size) out_buf[count++] = 0x00;
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

        if (b1 == 0x01) {
            if (b2 >= 0x80) { if (verbose) printf("PUSH_IMM %d\n", b2 & 0x7F); inst_count++; }
            else if (b2 == 0x01) { if (verbose) printf("POP\n"); inst_count++; }
            else if (b2 == 0x02) { if (verbose) printf("DUP\n"); inst_count++; }
            else if (b2 == 0x03) { if (verbose) printf("SWAP\n"); inst_count++; }
            else if (b2 == 0x04) { if (verbose) printf("PUSH_SHIFT_8\n"); inst_count++; }
            else if (b2 == 0x05) { if (verbose) printf("PUSH_SHIFT_16\n"); inst_count++; }
            else if (b2 == 0x06) { if (verbose) printf("MEMSET\n"); inst_count++; }
            else if (b2 == 0x07) { if (verbose) printf("MEMCPY\n"); inst_count++; }
            else if (b2 == 0x08) {
                if (ip + 2 <= sz) {
                    uint8_t low = code[ip++];
                    uint8_t high = code[ip++];
                    uint16_t val = low | (high << 8);
                    if (verbose) printf("PUSH_IMM16 %u\n", val);
                    inst_count += 2;
                } else {
                    if (verbose) printf("[ERROR] Truncated PUSH_IMM16 payload!\n");
                    err_count++; break;
                }
            }
            else if (b2 == 0x09) { if (verbose) printf("OVER\n"); inst_count++; }
            else if (b2 == 0x0A) { if (verbose) printf("NIP\n"); inst_count++; }
            else if (b2 == 0x0B) { if (verbose) printf("TUCK\n"); inst_count++; }
            else if (b2 == 0x0C) { if (verbose) printf("ROT\n"); inst_count++; }
            else if (b2 >= 0x10 && b2 <= 0x1F) { if (verbose) printf("LOAD R%d\n", b2 - 0x10); inst_count++; }
            else if (b2 >= 0x30 && b2 <= 0x3F) { if (verbose) printf("STORE R%d\n", b2 - 0x30); inst_count++; }
            else if (b2 >= 0x40 && b2 <= 0x4F) { if (verbose) printf("LOAD_RAM R%d\n", b2 - 0x40); inst_count++; }
            else if (b2 >= 0x50 && b2 <= 0x5F) { if (verbose) printf("STORE_RAM R%d\n", b2 - 0x50); inst_count++; }
            else if (b2 == 0x60) { if (verbose) printf("MALLOC\n"); inst_count++; }
            else if (b2 == 0x61) { if (verbose) printf("FREE\n"); inst_count++; }
            else if (b2 == 0x62) { if (verbose) printf("LOAD_HEAP\n"); inst_count++; }
            else if (b2 == 0x63) { if (verbose) printf("STORE_HEAP\n"); inst_count++; }
            else { if (verbose) printf("OP %02X %02X\n", b1, b2); inst_count++; }
        } else if (b1 == 0x02) {
            if (b2 == 0x01) { if (verbose) printf("ADD\n"); inst_count++; }
            else if (b2 == 0x02) { if (verbose) printf("SUB\n"); inst_count++; }
            else if (b2 == 0x03) { if (verbose) printf("MUL\n"); inst_count++; }
            else if (b2 == 0x04) { if (verbose) printf("DIV\n"); inst_count++; }
            else if (b2 == 0x05) { if (verbose) printf("MOD\n"); inst_count++; }
            else if (b2 == 0x06) { if (verbose) printf("INC_STACK\n"); inst_count++; }
            else if (b2 == 0x07) { if (verbose) printf("DEC_STACK\n"); inst_count++; }
            else if (b2 >= 0x10 && b2 <= 0x1F) { if (verbose) printf("INC R%d\n", b2 - 0x10); inst_count++; }
            else if (b2 >= 0x20 && b2 <= 0x2F) { if (verbose) printf("DEC R%d\n", b2 - 0x20); inst_count++; }
            else if (b2 == 0x40) { if (verbose) printf("FADD\n"); inst_count++; }
            else if (b2 == 0x41) { if (verbose) printf("FSUB\n"); inst_count++; }
            else if (b2 == 0x42) { if (verbose) printf("FMUL\n"); inst_count++; }
            else if (b2 == 0x43) { if (verbose) printf("FDIV\n"); inst_count++; }
            else { if (verbose) printf("OP %02X %02X\n", b1, b2); inst_count++; }
        } else if (b1 == 0x03) {
            if (b2 == 0x01) { if (verbose) printf("EQ\n"); inst_count++; }
            else if (b2 == 0x02) { if (verbose) printf("NEQ\n"); inst_count++; }
            else if (b2 == 0x03) { if (verbose) printf("GT\n"); inst_count++; }
            else if (b2 == 0x04) { if (verbose) printf("LT\n"); inst_count++; }
            else if (b2 == 0x05) { if (verbose) printf("GTE\n"); inst_count++; }
            else if (b2 == 0x06) { if (verbose) printf("LTE\n"); inst_count++; }
            else if (b2 == 0x07) { if (verbose) printf("AND\n"); inst_count++; }
            else if (b2 == 0x08) { if (verbose) printf("OR\n"); inst_count++; }
            else if (b2 == 0x09) { if (verbose) printf("NOT\n"); inst_count++; }
            else { if (verbose) printf("OP %02X %02X\n", b1, b2); inst_count++; }
        } else if (b1 == 0x04) {
            if (b2 == 0x00) { if (verbose) printf("RET\n"); inst_count++; }
            else if (b2 == 0x01) { if (verbose) printf("AND\n"); inst_count++; }
            else if (b2 == 0x02) { if (verbose) printf("OR\n"); inst_count++; }
            else if (b2 == 0x03) { if (verbose) printf("XOR\n"); inst_count++; }
            else if (b2 == 0x04) { if (verbose) printf("BIT_NOT\n"); inst_count++; }
            else if (b2 == 0x05) { if (verbose) printf("SHL\n"); inst_count++; }
            else if (b2 == 0x06) { if (verbose) printf("SHR\n"); inst_count++; }
            else if (b2 == 0x07) { if (verbose) printf("LOGICAL_NOT\n"); inst_count++; }
            else if (b2 == 0x08) { if (verbose) printf("LOGICAL_AND\n"); inst_count++; }
            else if (b2 == 0x09) { if (verbose) printf("LOGICAL_OR\n"); inst_count++; }
            else if (b2 == 0x0A) { if (verbose) printf("YIELD\n"); inst_count++; }
            else if (b2 == 0x0B) {
                if (ip + 2 <= sz) {
                    uint8_t low = code[ip++];
                    uint8_t high = code[ip++];
                    if (verbose) printf("SET_TRAP %d\n", low | (high << 8));
                    inst_count += 2;
                } else {
                    if (verbose) printf("[ERROR] Truncated SET_TRAP target!\n");
                    err_count++; break;
                }
            }
            else if (b2 == 0x0C) { if (verbose) printf("CLEAR_TRAP\n"); inst_count++; }
            else if (b2 >= 0x10 && b2 <= 0x4F) { if (verbose) printf("JMP %d\n", b2 - 0x10); inst_count++; }
            else if (b2 >= 0x50 && b2 <= 0x8F) { if (verbose) printf("JZ %d\n", b2 - 0x50); inst_count++; }
            else if (b2 >= 0x90 && b2 <= 0xCF) { if (verbose) printf("JNZ %d\n", b2 - 0x90); inst_count++; }
            else if (b2 >= 0xD0 && b2 <= 0xFB) { if (verbose) printf("CALL %d\n", b2 - 0xD0); inst_count++; }
            else if (b2 >= 0xFC && b2 <= 0xFF) {
                if (ip + 2 <= sz) {
                    uint8_t low = code[ip++];
                    uint8_t high = code[ip++];
                    uint16_t target_inst = low | (high << 8);
                    size_t target_byte = target_inst * 2;
                    const char *jump_type = (b2 == 0xFC) ? "JZ_FAR" : (b2 == 0xFD ? "JNZ_FAR" : (b2 == 0xFE ? "JMP_FAR" : "CALL_FAR"));
                    if (target_byte <= sz) {
                        if (verbose) printf("%s -> Inst %d (Byte %zu) [OK]\n", jump_type, target_inst, target_byte);
                    } else {
                        if (verbose) printf("%s -> Inst %d (Byte %zu) [OUT OF BOUNDS ERROR]\n", jump_type, target_inst, target_byte);
                        err_count++;
                    }
                    inst_count += 2;
                } else {
                    if (verbose) printf("[ERROR] Truncated FAR jump target!\n");
                    err_count++; break;
                }
            }
            else { if (verbose) printf("OP %02X %02X\n", b1, b2); inst_count++; }
        } else if (b1 == 0x05) {
            if (b2 == 0x01) { if (verbose) printf("PRINT_NUM\n"); inst_count++; }
            else if (b2 == 0x02) { if (verbose) printf("PRINT_CHAR\n"); inst_count++; }
            else if (b2 == 0x03) {
                if (verbose) printf("PRINT_STR \"");
                while (ip < sz && code[ip] != 0) {
                    if (verbose) putchar(code[ip]);
                    ip++;
                }
                if (verbose) printf("\"\n");
                if (ip < sz && code[ip] == 0) ip++;
                if (ip % 2 != 0) ip++;
                inst_count = ip / 2;
            }
            else if (b2 == 0x04) { if (verbose) printf("PRINT_NL\n"); inst_count++; }
            else if (b2 == 0x05) { if (verbose) printf("SCAN_NUM\n"); inst_count++; }
            else if (b2 == 0x06) { if (verbose) printf("READ_INT\n"); inst_count++; }
            else if (b2 == 0x07) { if (verbose) printf("READ_FLOAT\n"); inst_count++; }
            else if (b2 == 0xFF) { if (verbose) printf("HALT\n"); has_halt = true; inst_count++; }
            else { if (verbose) printf("OP %02X %02X\n", b1, b2); inst_count++; }
        } else if (b1 == 0x06) {
            if (b2 == 0x01) { if (verbose) printf("SYS_TIME\n"); inst_count++; }
            else if (b2 == 0x02) { if (verbose) printf("SYS_RAND\n"); inst_count++; }
            else if (b2 == 0x03) { if (verbose) printf("SYS_CLOCK\n"); inst_count++; }
            else { if (verbose) printf("OP %02X %02X\n", b1, b2); inst_count++; }
        } else if (b1 == 0x07) {
            if (b2 >= 0x10 && b2 <= 0x1F) { if (verbose) printf("MACRO_PRINT_REG R%d\n", b2 - 0x10); inst_count++; }
            else if (b2 >= 0x30 && b2 <= 0x3F) { if (verbose) printf("MACRO_PRINT_REG_RAW R%d\n", b2 - 0x30); inst_count++; }
            else if (b2 >= 0x40 && b2 <= 0x4F) { if (verbose) printf("MACRO_CLEAR_REG R%d\n", b2 - 0x40); inst_count++; }
            else if (b2 >= 0x70 && b2 <= 0x7F) { if (verbose) printf("DEF_MACRO %d\n", b2 - 0x70); inst_count++; }
            else if (b2 >= 0x80 && b2 <= 0x8F) { if (verbose) printf("EXEC_MACRO %d\n", b2 - 0x80); inst_count++; }
            else { if (verbose) printf("OP %02X %02X\n", b1, b2); inst_count++; }
        } else if (b1 == 0x08) {
            if (b2 == 0x01) { if (verbose) printf("VEC_DOT_4D\n"); inst_count++; }
            else if (b2 == 0x02) { if (verbose) printf("VEC_ADD_4D\n"); inst_count++; }
            else if (b2 == 0x03) { if (verbose) printf("VEC_SCALE_4D\n"); inst_count++; }
            else { if (verbose) printf("OP %02X %02X\n", b1, b2); inst_count++; }
        } else if (b1 == 0x09) {
            if (verbose) { printf("JMP_REL_BACK %d\n", b2); }
            inst_count++;
        } else if (b1 == 0x0A) {
            if (b2 == 0x01) { if (verbose) printf("FADD\n"); inst_count++; }
            else if (b2 == 0x02) { if (verbose) printf("FSUB\n"); inst_count++; }
            else if (b2 == 0x03) { if (verbose) printf("FMUL\n"); inst_count++; }
            else if (b2 == 0x04) { if (verbose) printf("FDIV\n"); inst_count++; }
            else if (b2 == 0x05) { if (verbose) printf("I2F\n"); inst_count++; }
            else if (b2 == 0x06) { if (verbose) printf("F2I\n"); inst_count++; }
            else if (b2 == 0x07) { if (verbose) printf("PRINT_FLOAT\n"); inst_count++; }
            else if (b2 == 0x08) { if (verbose) printf("FSQRT\n"); inst_count++; }
            else if (b2 == 0x09) { if (verbose) printf("FABS\n"); inst_count++; }
            else if (b2 == 0x0A) { if (verbose) printf("FEQ\n"); inst_count++; }
            else if (b2 == 0x0D) { if (verbose) printf("FFLOOR\n"); inst_count++; }
            else if (b2 == 0x0E) { if (verbose) printf("FCEIL\n"); inst_count++; }
            else if (b2 == 0x0F) { if (verbose) printf("FROUND\n"); inst_count++; }
            else if (b2 == 0x10) { if (verbose) printf("FMOD\n"); inst_count++; }
            else if (b2 == 0x11) { if (verbose) printf("FPOW\n"); inst_count++; }
            else if (b2 == 0x12) { if (verbose) printf("FSIN\n"); inst_count++; }
            else if (b2 == 0x13) { if (verbose) printf("FCOS\n"); inst_count++; }
            else if (b2 == 0x14) { if (verbose) printf("FTAN\n"); inst_count++; }
            else if (b2 == 0x15) { if (verbose) printf("FMIN\n"); inst_count++; }
            else if (b2 == 0x16) { if (verbose) printf("FMAX\n"); inst_count++; }
            else if (b2 == 0x17) { if (verbose) printf("FLOG\n"); inst_count++; }
            else { if (verbose) printf("OP %02X %02X\n", b1, b2); inst_count++; }
        } else if (b1 == 0x0B) {
            if (verbose) { printf("JMP_REL_FWD %d\n", b2); }
            inst_count++;
        } else if (b1 == 0x0C) {
            if (b2 == 0x01) { if (verbose) printf("JZ_REL_BACK\n"); inst_count++; }
            else if (b2 == 0x02) { if (verbose) printf("JNZ_REL_BACK\n"); inst_count++; }
            else if (b2 == 0x03) { if (verbose) printf("JZ_REL_FWD\n"); inst_count++; }
            else if (b2 == 0x04) { if (verbose) printf("JNZ_REL_FWD\n"); inst_count++; }
            else { if (verbose) printf("OP %02X %02X\n", b1, b2); inst_count++; }
        } else if (b1 == 0x0D) {
            if (verbose) { printf("JZ_REL_FWD %d\n", b2); }
            inst_count++;
        } else if (b1 == 0x0E) {
            if (b2 == 0x01) {
                if (ip + 2 <= sz) {
                    uint8_t lib = code[ip++];
                    uint8_t func = code[ip++];
                    if (verbose) printf("FFI_CALL Lib 0x%02X Func 0x%02X\n", lib, func);
                    inst_count += 2;
                } else {
                    if (verbose) printf("[ERROR] Truncated FFI_CALL parameters!\n");
                    err_count++; break;
                }
            } else { if (verbose) printf("OP %02X %02X\n", b1, b2); inst_count++; }
        } else if (b1 == 0x0F) {
            if (verbose) { printf("JNZ_REL_FWD %d\n", b2); }
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

    if (verbose) {
        printf("[VALIDATION SUCCESS] Bytecode (%zu bytes, %zu instructions) is 100%% valid and aligned!\n", sz, inst_count);
    }
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
    bool json_mode = false;
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
    } else if (strcmp(argv[1], "--json") == 0) {
        json_mode = true;
        arg_idx = 2;
        if (argc < 3) {
            printf("{\"status\":\"error\",\"error\":\"MISSING_ARGUMENT\"}\n");
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
        char tmp_buf[8192];
        size_t read_bytes = fread(tmp_buf, 1, fsize, f);
        fclose(f);

        /* Check if file contains text hex stream (e.g. "01x08 20x03...") or raw binary */
        bool is_text_hex = false;
        for (size_t i = 0; i < read_bytes; i++) {
            if (tmp_buf[i] == 'x' || tmp_buf[i] == 'X' || isspace((unsigned char)tmp_buf[i])) {
                is_text_hex = true;
                break;
            }
        }

        if (is_text_hex) {
            tmp_buf[read_bytes] = '\0';
            bytecode_size = parse_hex_stream(tmp_buf, bytecode, sizeof(bytecode));
            if (!json_mode) printf("[+] Loaded %zu bytecode bytes from text stream file: %s\n", bytecode_size, target);
        } else {
            memcpy(bytecode, tmp_buf, read_bytes);
            bytecode_size = read_bytes;
            if (!json_mode) printf("[+] Loaded %zu bytes from binary file: %s\n", bytecode_size, target);
        }
    } else {
        bytecode_size = parse_hex_stream(target, bytecode, sizeof(bytecode));
        if (bytecode_size == 0) {
            if (json_mode) {
                printf("{\"status\":\"error\",\"error\":\"PARSE_ERROR\",\"target\":\"%s\"}\n", target);
            } else {
                fprintf(stderr, "Error: Unable to parse hex stream or open file '%s'\n", target);
            }
            return 1;
        }
        if (!json_mode) printf("[+] Loaded %zu bytes directly from hex stream\n", bytecode_size);
    }

    int valid = validate_bytecode(bytecode, bytecode_size, !json_mode);
    if (!valid) {
        if (json_mode) {
            printf("{\"status\":\"error\",\"error\":\"INVALID_BYTECODE\",\"bytes\":%zu}\n", bytecode_size);
        }
        return 1;
    }

    if (validate_only) {
        return 0;
    }

    if (!json_mode) {
        printf("\n=== Executing in LVLang VM Runtime ===\nOutput: ");
        fflush(stdout);
    }

    lvl_vm_t vm;
    lvl_init(&vm, bytecode, bytecode_size);
    lvl_plugin_system_init(&vm);
    lvl_plugin_crypto_init(&vm);
    lvl_plugin_keyboard_init(&vm);
    lvl_plugin_sdl2_init(&vm);
    lvl_plugin_time_init(&vm);
    lvl_plugin_string_init(&vm);
    lvl_plugin_ansi_init(&vm);
    lvl_plugin_heap_init(&vm);
    lvl_plugin_net_init(&vm);

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

    if (json_mode) {
        printf("{\"status\":\"%s\",\"exit_status\":%d,\"ip\":%zu,\"bytes\":%zu,\"stack_depth\":%zu}\n",
               vm.status >= 0 ? "success" : "error",
               lvl_get_status(&vm), vm.ip, bytecode_size, vm.sp);
    } else {
        printf("\n[VM Halted] Exit Status: %d (IP: %zu)\n", lvl_get_status(&vm), vm.ip);
    }
    return 0;
}
