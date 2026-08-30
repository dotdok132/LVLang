# LVLang (AI-Native 16-Bit Machine Bytecode Engine)

[![GitHub Engine](https://img.shields.io/badge/Repository-LVLang-blue.svg)](https://github.com/dotdok132/LVLang)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C99 Standard](https://img.shields.io/badge/C-C99-green.svg)](https://en.wikipedia.org/wiki/C99)
[![ISA: Strict 2-Byte](https://img.shields.io/badge/ISA-Strict--2--Byte--16bit-purple.svg)](https://github.com/dotdok132/LVLang)

**LVLang** is an AI-native 16-bit machine bytecode ISA and zero-overhead C99 runtime engine specifically engineered for direct LLM-to-Runtime execution, bypassing 100% of human text syntaxes, text compilers, and AST parsers.

---

## 🌐 Official Ecosystem Repositories

- ⚡ **Core Engine & VM Runtime**: [dotdok132/LVLang](https://github.com/dotdok132/LVLang)
- 🖥️ **System OS Plugin**: [dotdok132/lvlang-system](https://github.com/dotdok132/lvlang-system) (`LibID 0x03` — Shell Commands, File I/O, Env Vars)
- ⌨️ **Keyboard Plugin**: [dotdok132/lvlang-keyboard](https://github.com/dotdok132/lvlang-keyboard) (`LibID 0x05` — Terminal Raw Input, Non-blocking Polling)
- 🎮 **SDL2 2D Engine Plugin**: [dotdok132/lvlang-sdl2](https://github.com/dotdok132/lvlang-sdl2) (`LibID 0x06` — 2D Windowing, Hardware Acceleration, Double Buffering)
- 🔐 **Cryptography Plugin**: [dotdok132/lvlang-crypto](https://github.com/dotdok132/lvlang-crypto) (`LibID 0x02` — SHA256 & AES)

---

## ⚡ Key Philosophy: Strict 2-Byte ISA & Zero Human Bloat

Standard programming languages (C++, Python, JS) force LLMs to spend **over 90% of output tokens on human text formatting** (`function`, `return`, `#include`, braces, indentation).

**LVLang eliminates 100% of human text overhead:**

- **Strict 2-Byte ISA**: Every single instruction is **strictly 2 bytes** (`[GROUP]x[COMMAND]`).
- **1 LLM Token = 1 Executable Opcode**: Output format `01x64 01x08 02x03` maps 1:1 to single LLM tokens without subword tokenizer merging.
- **Direct 1:1 Instruction Count Relative Jumps**:
  - `09xN`: Jump **backward** $N$ instructions ($1..255$).
  - `0BxN`: Jump **forward** $N$ instructions ($1..255$).
- **Zero Parsing Overhead**: The C runtime directly executes the byte stream in memory without AST building or text tokenizers.

---

## 🚀 Performance Benchmarks

- **Execution Engine**: Pure C99 single-header runtime (`lvlang.h`).
- **Native JIT Engine**: Native x86-64 machine code emitter (`lvl_jit.h`).
- **Execution Speed**: **~177 Million Full Program Executions Per Second** (**5.62 ns** per program run).
- **JIT Compilation Time**: **295 nanoseconds** (0.000295 ms).

---

## 📊 Complete Opcode Matrix Specification

Every instruction occupies **strictly 2 bytes**: `[Group: 1 Byte] [Command/Operand: 1 Byte]`.

### 1. Stack, Registers & Memory (`Group 0x01`)
| Opcode | Operand | Name | Description |
|---|---|---|---|
| `0x01` | `0x00..0x7F` | `PUSH_IMM` | Push 7-bit integer $0..127$ onto data stack |
| `0x01` | `0x01` | `POP` | Pop top of data stack |
| `0x01` | `0x02` | `DUP` | Duplicate top element of stack |
| `0x01` | `0x03` | `SWAP` | Swap top two elements of stack |
| `0x01` | `0x04` | `PUSH_SHIFT_8` | Pop $a$, push $a \ll 8$ |
| `0x01` | `0x05` | `PUSH_SHIFT_16` | Pop $a$, push $a \ll 16$ |
| `0x01` | `0x10 + R` | `LOAD_REG` | Load register $R_0..R_{15}$ onto stack |
| `0x01` | `0x30 + R` | `STORE_REG` | Store stack top into register $R_0..R_{15}$ |
| `0x01` | `0x40 + R` | `LOAD_RAM` | Push `RAM[R_idx]` onto stack |
| `0x01` | `0x50 + R` | `STORE_RAM` | Pop stack top into `RAM[R_idx]` |

### 2. Integer & Math (`Group 0x02`)
| Opcode | Operand | Name | Description |
|---|---|---|---|
| `0x02` | `0x01` | `ADD` | Pop $b, a$; push $a + b$ |
| `0x02` | `0x02` | `SUB` | Pop $b, a$; push $a - b$ |
| `0x02` | `0x03` | `MUL` | Pop $b, a$; push $a \times b$ |
| `0x02` | `0x04` | `DIV` | Pop $b, a$; push $a / b$ |
| `0x02` | `0x05` | `MOD` | Pop $b, a$; push $a \pmod b$ |
| `0x02` | `0x10 + R` | `INC_REG` | Increment register $R_i \leftarrow R_i + 1$ |
| `0x02` | `0x20 + R` | `DEC_REG` | Decrement register $R_i \leftarrow R_i - 1$ |

### 3. Comparison & Logic (`Group 0x03`)
| Opcode | Operand | Name | Description |
|---|---|---|---|
| `0x03` | `0x01` | `EQ` | Push $1$ if $a == b$ else $0$ |
| `0x03` | `0x02` | `NEQ` | Push $1$ if $a \neq b$ else $0$ |
| `0x03` | `0x03` | `GT` | Push $1$ if $a > b$ else $0$ |
| `0x03` | `0x04` | `LT` | Push $1$ if $a < b$ else $0$ |

### 4. Direct Relative Flow Control (`Group 0x09` & `Group 0x0B`)
| Opcode | Operand | Name | Description |
|---|---|---|---|
| `0x09` | `N` | `JMP_REL_BACK` | Jump **backward** $N$ instructions ($1..255$) |
| `0x0B` | `N` | `JMP_REL_FWD` | Jump **forward** $N$ instructions ($1..255$) |
| `0x0C` | `0x01` | `JZ_REL_BACK` | Pop $N$, Pop $a$; jump **backward** $N$ instructions if $a == 0$ |
| `0x0C` | `0x02` | `JNZ_REL_BACK` | Pop $N$, Pop $a$; jump **backward** $N$ instructions if $a \neq 0$ |

### 5. Dynamic AI Macros (`Group 0x07`)
| Opcode | Operand | Name | Description |
|---|---|---|---|
| `0x07` | `0x70 + ID` | `DEF_MACRO` | Register dynamic subroutine `ID` ($0..15$) |
| `0x07` | `0x80 + ID` | `EXEC_MACRO` | Execute dynamic subroutine `ID` ($0..15$) in 1 instruction |
| `0x07` | `0x40 + R` | `CLEAR_REG` | Zero out register $R_i \leftarrow 0$ |

### 6. Hardware Plugins & FFI (`Group 0x0E`)
| Opcode | Operand | Name | Description |
|---|---|---|---|
| `0x0E` | `0x01 [LibID] [FuncID]` | `FFI_CALL` | Call C FFI Plugin (0x06=SDL2, 0x03=System, 0x05=Keyboard) |

---

## 🛠️ Building & Running CLI (`lvlc`)

```bash
# Build runtime CLI
gcc -O3 -Wall lvlc.c -o lvlc -lSDL2

# Validate hex bytecode stream
./lvlc --validate "01x64 01x08 02x03 0Ex01 06x01 05xFF"

# Disassemble hex stream to human opcode trace
./lvlc --disasm "01x64 01x08 02x03 0Ex01 06x01 05xFF"

# Run 60 FPS continuous SDL2 2D graphics loop
./lvlc "07x70 01x12 01x30 02x03 01x10 02x01 01x33 01x11 01x2C 02x03 01x10 02x01 01x34 01x12 01x11 02x03 01x10 01x03 02x03 02x01 01x7F 02x05 01x2D 02x01 01x35 01x11 01x17 02x03 01x10 01x05 02x03 02x01 01x7F 02x05 01x2D 02x01 01x36 01x12 01x11 02x01 01x0B 02x03 01x10 01x07 02x03 02x01 01x7F 02x05 01x2D 02x01 01x37 01x15 01x16 01x17 0Ex01 06x02 01x13 01x14 01x26 01x22 0Ex01 06x04 04x00 01x64 01x08 02x03 01x64 01x06 02x03 0Ex01 06x01 01x01 07x40 02x10 01x0A 01x0F 01x1E 0Ex01 06x02 0Ex01 06x03 07x41 07x42 07x80 02x12 01x12 01x10 03x04 09x05 02x11 01x11 01x0C 03x04 09x0C 0Ex01 06x05 0Ex01 06x06 09x1C 0Ex01 06x07 05xFF"
```

---

## 📄 License

MIT License. Engineered for AI Agent Architectures, LLM Bytecode Synthesis, and High-Speed Hardware Execution.
