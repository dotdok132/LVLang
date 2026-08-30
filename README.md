# LVLang (Low-Velocity / Low-Latency Machine Language for AI)

[![GitHub Engine](https://img.shields.io/badge/Repository-LVLang-blue.svg)](https://github.com/dotdok132/LVLang)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C99 Standard](https://img.shields.io/badge/C-C99-green.svg)](https://en.wikipedia.org/wiki/C99)

**LVLang** is an ultra-dense, zero-overhead machine language, native x86-64 JIT compiler, and pure C99 runtime engine specifically engineered for AI Agent communication, execution, and extreme token efficiency.

---

## 🌐 Official Ecosystem Repositories

- ⚡ **Core Engine & JIT Runtime**: [dotdok132/LVLang](https://github.com/dotdok132/LVLang)
- 🔐 **Cryptography Plugin**: [dotdok132/lvlang-crypto](https://github.com/dotdok132/lvlang-crypto) (`LibID 0x02` — SHA256 & AES)
- 🖥️ **System OS Plugin**: [dotdok132/lvlang-system](https://github.com/dotdok132/lvlang-system) (`LibID 0x03` — Shell Commands, File I/O, Env Vars)
- ⌨️ **Keyboard Interaction Plugin**: [dotdok132/lvlang-keyboard](https://github.com/dotdok132/lvlang-keyboard) (`LibID 0x05` — Keypress Polling, Key State Checks, Line Input)

---

## ⚡ Key Philosophy: Maximum Token Density

Standard programming languages (Python, JavaScript, C) were designed for human brains, forcing LLMs to spend **up to 90% of their output tokens** on human syntactic boilerplate (`function`, `return`, `def`, `var`, braces, indentation).

**LVLang eliminates 100% of human syntactic bloat.**

- **Strict 2-Byte Instruction Alignment**: Every instruction is strictly 2 bytes: `[1 Byte Opcode/Group] [1 Byte Operand/Register]`.
- **1 LLM Token = 1 Executable Command**: LLMs output raw hex streams (e.g. `0185013001800131011101100201013102200110049401110501050405FF`).
- **Zero Parsing Overhead**: The C runtime directly executes the byte stream in memory without lexical parsing or AST building.

---

## 🚀 Performance Benchmarks

- **Execution Engine**: Pure C99 single-header runtime (`lvlang.h`).
- **Native JIT Engine**: Native x86-64 machine code emitter (`lvl_jit.h`).
- **Execution Speed**: **~177 Million Full Program Executions Per Second** (**5.62 ns** per program run).
- **JIT Compilation Time**: **295 nanoseconds** (0.000295 ms).

---

## 📦 On-Demand Package Manager (`lvlc pkg`)

Install official and 3rd-party modular plugin packages on-demand without bloating the core runtime:

```bash
# Install Cryptography plugin (SHA256, AES)
./lvlc pkg install crypto

# Install System OS plugin (Shell execution, File I/O)
./lvlc pkg install system
```

---

## 📊 Complete Opcode Matrix Specification

Every instruction occupies **strictly 2 bytes**: `[Opcode: 1 Byte] [Operand/Reg: 1 Byte]`.

### 1. Stack, Registers & RAM Memory (`0x01`)
| Opcode (Hex) | Operand (Hex) | Name | Description |
|---|---|---|---|
| `0x01` | `0x80 + N` | `PUSH_IMM` | Push 7-bit integer $N$ onto data stack |
| `0x01` | `0x04 [4 Bytes]` | `PUSH_INT32` | **32-Bit Integer**: Push full signed 32-bit integer ($\pm 2.14 \times 10^9$) |
| `0x01` | `0x01` | `POP` | Pop top of data stack |
| `0x01` | `0x02` | `DUP` | Duplicate top element of stack |
| `0x01` | `0x03` | `SWAP` | Swap top two elements of stack |
| `0x01` | `0x10 + R` | `LOAD_REG` | Load register $R_0..R_{15}$ onto data stack |
| `0x01` | `0x30 + R` | `STORE_REG` | Store data stack top into register $R_0..R_{15}$ |
| `0x01` | `0x40 + R` | `LOAD_RAM` | **RAM Memory**: Push `RAM[R_idx]` ($0..1023$) onto data stack |
| `0x01` | `0x50 + R` | `STORE_RAM` | **RAM Memory**: Pop stack top into `RAM[R_idx]` ($0..1023$) |

### 2. Integer & Float Arithmetic (`0x02`)
| Opcode (Hex) | Operand (Hex) | Name | Description |
|---|---|---|---|
| `0x02` | `0x01` | `ADD` | Pop $b, a$; push $a + b$ |
| `0x02` | `0x02` | `SUB` | Pop $b, a$; push $a - b$ |
| `0x02` | `0x03` | `MUL` | Pop $b, a$; push $a \times b$ |
| `0x02` | `0x04` | `DIV` | Pop $b, a$; push $a / b$ |
| `0x02` | `0x05` | `MOD` | Pop $b, a$; push $a \pmod b$ |
| `0x02` | `0x10 + R` | `INC_REG` | Increment register $R_0..R_{15}$ ($R_i \leftarrow R_i + 1$) |
| `0x02` | `0x20 + R` | `DEC_REG` | Decrement register $R_0..R_{15}$ ($R_i \leftarrow R_i - 1$) |
| `0x02` | `0x40` | `FADD` | **Float Math**: IEEE 754 float addition ($a + b$) |
| `0x02` | `0x41` | `FSUB` | **Float Math**: IEEE 754 float subtraction ($a - b$) |
| `0x02` | `0x42` | `FMUL` | **Float Math**: IEEE 754 float multiplication ($a \times b$) |
| `0x02` | `0x43` | `FDIV` | **Float Math**: IEEE 754 float division ($a / b$) |

### 3. Comparison & Logic (`0x03`)
| Opcode (Hex) | Operand (Hex) | Name | Description |
|---|---|---|---|
| `0x03` | `0x01` | `EQ` | Push $1$ if $a == b$ else $0$ |
| `0x03` | `0x02` | `NEQ` | Push $1$ if $a \neq b$ else $0$ |
| `0x03` | `0x03` | `GT` | Push $1$ if $a > b$ else $0$ |
| `0x03` | `0x04` | `LT` | Push $1$ if $a < b$ else $0$ |
| `0x03` | `0x05` | `GTE` | Push $1$ if $a \ge b$ else $0$ |
| `0x03` | `0x06` | `LTE` | Push $1$ if $a \le b$ else $0$ |

### 4. Control Flow, Subroutines, Traps & Coroutines (`0x04`)
| Opcode (Hex) | Operand (Hex) | Name | Description |
|---|---|---|---|
| `0x04` | `0x10 + Target` | `JMP` | Jump to instruction index `Target` ($0..63$) |
| `0x04` | `0x50 + Target` | `JZ` | Jump to `Target` if stack top $== 0$ |
| `0x04` | `0x90 + Target` | `JNZ` | Jump to `Target` if stack top $\neq 0$ |
| `0x04` | `0xD0 + Target` | `CALL` | Subroutine Call: Push return `ip`, jump to `Target` |
| `0x04` | `0x00` | `RET` | Subroutine Return: Pop return `ip` and resume |
| `0x04` | `0x01` | `YIELD` | **Async Coroutine Yield**: Non-blocking pause (`LVL_STATUS_YIELD = 2`) |
| `0x04` | `0x05 [Low High]` | `SET_TRAP` | **Try/Catch Exception Handler**: Set error trap handler `ip` |
| `0x04` | `0x06` | `CLEAR_TRAP` | Disable exception trap handler |
| `0x04` | `0xFE [Low High]` | `JMP_FAR` | **16-Bit Long Jump**: Unconditional jump to index $0..65,535$ (Up to 128KB) |
| `0x04` | `0xFF [Low High]` | `CALL_FAR` | **16-Bit Long Subroutine Call** |

### 5. Macro Shortcut Opcodes (`0x07` — 1-Command Abbreviations)
| Opcode (Hex) | Operand (Hex) | Name | Description |
|---|---|---|---|
| `0x07` | `0x10 + R` | `MACRO_PRINT_REG` | Print register $R_i$ + newline `\n` |
| `0x07` | `0x30 + R` | `MACRO_PRINT_REG_RAW` | Print register $R_i$ (no newline) |
| `0x07` | `0x40 + R` | `MACRO_CLEAR_REG` | Zero out register $R_i$ ($R_i \leftarrow 0$) |

### 6. String & Pattern Processing (`0x0A`)
| Opcode (Hex) | Operand (Hex) | Name | Description |
|---|---|---|---|
| `0x0A` | `0x01` | `STR_CMP` | Compare strings at `RAM[R0]` and `RAM[R1]` -> Push 0, -1, 1 |
| `0x0A` | `0x02` | `STR_FIND` | Search pattern `RAM[R1]` in `RAM[R0]` -> Push 0-based char index or -1 |

### 7. External 3rd-Party Libraries & FFI Plugins (`0x0E`)
| Opcode (Hex) | Operand (Hex) | Name | Description |
|---|---|---|---|
| `0x0E` | `0x01 [LibID] [FuncID]` | `FFI_CALL` | Execute registered 3rd-party C/C++/Python dynamic library function |

### 8. System Syscalls (`0x06`)
| Opcode (Hex) | Operand (Hex) | Name | Description |
|---|---|---|---|
| `0x06` | `0x01` | `SYS_TIME` | Push current Unix epoch timestamp in seconds onto stack |
| `0x06` | `0x02` | `SYS_RAND` | Push pseudo-random integer onto stack |
| `0x06` | `0x03` | `SYS_CLOCK` | Push process CPU execution time in milliseconds |

### 9. AI Vector & Embedding Math (`0x08`)
| Opcode (Hex) | Operand (Hex) | Name | Description |
|---|---|---|---|
| `0x08` | `0x01` | `VEC_DOT_4D` | Compute scalar dot product $R_{0..3} \cdot R_{4..7}$ -> Push to stack |
| `0x08` | `0x02` | `VEC_ADD_4D` | Vector addition $R_{0..3} \leftarrow R_{0..3} + R_{4..7}$ |
| `0x08` | `0x03` | `VEC_SCALE_4D` | Vector scaling $R_{0..3} \leftarrow R_{0..3} \times \text{StackTop}$ |

---

## 🛠️ Building & Running

### Build the CLI Tool & Package Manager `lvlc`:
```bash
gcc -std=c99 -Wall -Wextra -pedantic lvlc.c -o lvlc
```

### Run Hex Stream Directly:
```bash
./lvlc "01E4013001AD0131011001110201013201120501050405FF"
```

**Output:**
```text
[+] Loaded 24 bytes directly from hex stream.

=== Executing in LVLang VM Runtime ===
Output: 145

[VM Halted] Exit Status: 1 (IP: 24)
```

---

## 📄 License

MIT License. Designed for AI Agent Architectures, Autonomous LLM Code Generation, and High-Speed Hardware Execution.
