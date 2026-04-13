# flux-conformance-runner

C11 conformance runner for the FLUX instruction set.

**113/113 test vectors passing** from SuperInstance/flux-conformance.

## Build

```bash
gcc -std=c11 -O2 -o runner flux-conformance-runner.c -lm
```

## Run

```bash
./runner vectors.json
```

## Zero Dependencies

Compiles on any platform with a C11 compiler and -lm. No malloc, no external libs.

## Architecture

- Double stack (matches Python reference VM)
- Flag-based conditionals (JZ/JNZ check FLAGS.Z, not stack)
- Euclidean modulo (Python-style, not C-style)
- Carry flag on unsigned 32-bit overflow
- A2A signal queues per channel (FIFO)
- Confidence clamped to [0, 1]

## Opcodes Implemented

| Category | Opcodes |
|----------|---------|
| System | HALT, NOP, BREAK |
| Arithmetic | ADD, SUB, MUL, DIV, MOD, NEG, INC, DEC |
| Comparison | EQ, NE, LT, LE, GT, GE |
| Logic | AND, OR, XOR, NOT, SHL, SHR |
| Memory | LOAD, STORE, PEEK, POKE |
| Control | JMP, JZ, JNZ, CALL, RET, PUSH, POP |
| Stack | DUP, SWAP, OVER, ROT |
| Float | FADD, FSUB, FMUL, FDIV |
| Confidence | CONF_GET, CONF_SET, CONF_MUL |
| A2A | SIGNAL, BROADCAST, LISTEN |

## Key Lessons

- **JZ/JNZ check FLAGS.Z** from previous comparison, not the stack value
- **POKE pops addr then val** (reverse of POKE's logical order)
- **PEEK reads 4-byte signed int** from memory, not 1 byte
- **Confidence ops clamp to [0,1]** on set and multiply
- **Euclidean modulo**: result has sign of divisor, not dividend
- **Carry flag**: unsigned 32-bit overflow, not signed

---

Part of the [FLUX runtime ecosystem](https://github.com/Lucineer/flux-runtime-c).
Companion to [flux-runtime-c](https://github.com/Lucineer/flux-runtime-c) and [flux-isa-v3](https://github.com/Lucineer/flux-isa-v3).
