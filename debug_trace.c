#include "lvlang.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

extern void lvl_plugin_sdl2_init(lvl_vm_t *vm);
extern void lvl_plugin_keyboard_init(lvl_vm_t *vm);

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

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <hex_stream>\n", argv[0]);
        return 1;
    }
    uint8_t code[4096];
    size_t sz = parse_hex_stream(argv[1], code, sizeof(code));
    printf("--- DISASSEMBLY TRACE (%zu bytes) ---\n", sz);
    size_t ip = 0;
    size_t inst_idx = 0;
    while (ip < sz) {
        size_t start_ip = ip;
        uint8_t b1 = code[ip++];
        uint8_t b2 = (ip < sz) ? code[ip++] : 0;
        printf("Inst %3zu | Byte %3zu: [%02X %02X]", inst_idx, start_ip, b1, b2);
        if (b1 == 0x01 && b2 == 0x04 && ip + 4 <= sz) {
            int32_t val = (int32_t)(code[ip] | (code[ip+1]<<8) | (code[ip+2]<<16) | (code[ip+3]<<24));
            printf(" PUSH_INT32 %d (bytes: %02X %02X %02X %02X)\n", val, code[ip], code[ip+1], code[ip+2], code[ip+3]);
            ip += 4;
            inst_idx += 3;
        } else if (b1 == 0x04 && b2 == 0xFD && ip + 2 <= sz) {
            uint8_t low = code[ip++];
            uint8_t high = code[ip++];
            uint16_t target = low | (high << 8);
            printf(" JNZ_FAR -> Inst %d (Byte %d)\n", target, target * 2);
            inst_idx += 2;
        } else if (b1 == 0x04 && b2 == 0xFC && ip + 2 <= sz) {
            uint8_t low = code[ip++];
            uint8_t high = code[ip++];
            uint16_t target = low | (high << 8);
            printf(" JZ_FAR -> Inst %d (Byte %d)\n", target, target * 2);
            inst_idx += 2;
        } else if (b1 == 0x0E && b2 == 0x01 && ip + 2 <= sz) {
            uint8_t lib = code[ip++];
            uint8_t func = code[ip++];
            printf(" FFI_CALL Lib 0x%02X Func 0x%02X\n", lib, func);
            inst_idx += 2;
        } else {
            printf("\n");
            inst_idx++;
        }
    }
    return 0;
}
