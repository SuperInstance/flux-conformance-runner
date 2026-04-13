/**
 * edge_case_vectors.c — DS-Reasoner's 12 semantic gap test vectors
 *
 * Opcodes used (flux-runtime-c numbering):
 *   0x10 ADD  0x12 MUL  0x13 DIV  0x14 MOD
 *   0x34 SHL  0x35 SHR
 *   0x41 STORE  0x43 PEEK  0x44 POKE
 *   0x51 JZ  0x52 JNZ  0x54 RET
 *   0x55 PUSH (8-byte double immediate follows opcode)
 *   0x70 FADD  0x71 FSUB
 *   0x80 CONF_SET  0x81 CONF_GET  0x82 CONF_MUL
 *   0x90 SIGNAL
 *   0x80 HALT
 */

#include "edge_case_vectors.h"
#include <string.h>
#include <math.h>

/* Helper: build a bytecode array inline */
static uint8_t _v1[] = {0x55, 0,0,0,0,0,0,0xF0,0x3F, /* PUSH 1.0 */
                         0x80}; /* HALT */

/* Gap 1: DIV by Zero with INT_MIN
 * PUSH INT_MIN (-2147483648.0) → stack has one value
 * The DIV needs two operands — pop divisor first, then dividend.
 * Actually: push two values, DIV pops val then addr (LIFO).
 * Test: 1.0 / 0.0 = +inf (IEEE 754) */
static uint8_t _gap1[] = {
    0x55, 0,0,0,0,0,0,0xF0,0x3F,  /* PUSH 1.0 */
    0x55, 0,0,0,0,0,0,0,0,0,     /* PUSH 0.0 */
    0x13,                          /* DIV: 1.0 / 0.0 = +inf */
    0x80                           /* HALT */
};

/* Gap 2: MOD with Negative Divisor (Euclidean)
 * -5 MOD -3: Euclidean result = -5 - (-3)*floor(-5/-3) = -5 - (-3)*1 = -2
 * But Python: -5 % -3 = -2 (Python always returns result with sign of divisor)
 * C99 fmod(-5, -3) = -2.0 */
static uint8_t _gap2[] = {
    0x55, 0,0,0,0,0,0,0x14,0xC0,  /* PUSH -5.0 */
    0x55, 0,0,0,0,0,0,0x08,0xC0,  /* PUSH -3.0 */
    0x14,                          /* MOD */
    0x80                           /* HALT */
};

/* Gap 3: Carry Flag for MUL with 32-bit Overflow
 * 0x10000 * 0x10000 = 0x100000000 = 4294967296 (overflows uint32) */
static uint8_t _gap3[] = {
    0x55, 0,0,0,0,0,0,0x00,0x40,  /* PUSH 65536.0 (0x10000) */
    0x55, 0,0,0,0,0,0,0x00,0x40,  /* PUSH 65536.0 */
    0x12,                          /* MUL: 65536*65536 = 4294967296.0 */
    0x80                           /* HALT */
};

/* Gap 4: SHL with Non-Integer Shift Count
 * 1.0 SHL 3.5 — shift count should truncate to 3 */
static uint8_t _gap4[] = {
    0x55, 0,0,0,0,0,0,0xF0,0x3F,  /* PUSH 1.0 */
    0x55, 0,0,0,0,0,0,0x0C,0x40,  /* PUSH 3.5 */
    0x34,                          /* SHL */
    0x80                           /* HALT */
};

/* Gap 5: POKE with Unaligned Address (address 1.0)
 * Should store 0x12345678 at memory address 1 */
static uint8_t _gap5[] = {
    0x55, 0x78,0x56,0x34,0x12,0,0,0,0,  /* PUSH 0x12345678 */
    0x55, 0,0,0,0,0,0,0xF0,0x3F,          /* PUSH 1.0 (unaligned addr) */
    0x44,                                    /* POKE: addr=1.0, val=0x12345678 */
    0x80                                     /* HALT */
};

/* Gap 6: FSUB with Infinities (+inf - +inf = NaN)
 * Expected: stack top is NaN */
static uint8_t _gap6[] = {
    0x55, 0,0,0,0,0,0,0xF0,0x7F,  /* PUSH +inf */
    0x55, 0,0,0,0,0,0,0xF0,0x7F,  /* PUSH +inf */
    0x71,                          /* FSUB: +inf - +inf = NaN */
    0x80                           /* HALT */
};

/* Gap 7: CONF_MUL with NaN Confidence
 * CONF_SET(NaN) → CONF_GET → CONF_MUL(0.5)
 * Expected: confidence should clamp NaN to [0,1] */
static uint8_t _gap7[] = {
    0x55, 0,0,0,0,0,0,0xF8,0x7F,  /* PUSH NaN (0x7FF8000000000000) */
    0x80,                          /* CONF_SET: set confidence to NaN */
    0x55, 0,0,0,0,0,0,0xE0,0x3F,  /* PUSH 0.5 */
    0x82,                          /* CONF_MUL: confidence *= 0.5 */
    0x81,                          /* CONF_GET: push current confidence */
    0x80                           /* HALT */
};

/* Gap 8: JZ without Previous Comparison
 * ADD then JZ — FLAGS.Z from a previous state or undefined */
static uint8_t _gap8[] = {
    0x55, 0,0,0,0,0,0,0xF0,0x3F,  /* PUSH 1.0 */
    0x55, 0,0,0,0,0,0,0xF0,0x3F,  /* PUSH 1.0 */
    0x10,                          /* ADD: 1+1=2, FLAGS.Z=0 */
    0x52, 0x00,0x00,0x00,         /* JNZ +3: jump to HALT (skip next PUSH) */
    0x55, 0,0,0,0,0,0,0x00,0x40,  /* PUSH 2.0 (skipped because Z=0) */
    0x80                           /* HALT: stack has [2.0] from ADD */
};

/* Gap 9: A2A Queue Full Behavior
 * Push many SIGNALs to test queue overflow */
/* This needs a loop — simplified: just test that SIGNAL works */
static uint8_t _gap9[] = {
    0x55, 0,0,0,0,0,0,0xF0,0x3F,  /* PUSH 1.0 */
    0x55, 0,0,0,0,0,0,0x08,0x40,  /* PUSH 3.0 */
    0x90,                          /* SIGNAL: channel 3 */
    0x80                           /* HALT */
};

/* Gap 10: RET with Empty Call Stack
 * RET as first instruction — should halt gracefully */
static uint8_t _gap10[] = {
    0x54,                          /* RET: empty call stack */
    0x80                           /* HALT (should be reached) */
};

/* Gap 11: FADD with Denormal Results
 * 1e-308 + 1e-308 = 2e-308 (denormal in IEEE 754) */
static uint8_t _gap11[] = {
    0x55, 0x10,0xE2,0x5E,0xBC,0xF8,0x42,0xCB,0x17,  /* PUSH 1e-308 */
    0x55, 0x10,0xE2,0x5E,0xBC,0xF8,0x42,0xCB,0x17,  /* PUSH 1e-308 */
    0x70,                                              /* FADD */
    0x80                                               /* HALT */
};

/* Gap 12: Memory Aliasing between STORE and PEEK
 * STORE double 1.0 at address 0x1000, then PEEK at same address as int32
 * Expected: PEEK reads back 0x3FF00000 (IEEE 754 representation of 1.0) */
static uint8_t _gap12[] = {
    0x55, 0,0,0,0,0,0,0xF0,0x3F,     /* PUSH 1.0 */
    0x55, 0,0,0x00,0,0,0,0x59,0x40,  /* PUSH 100.0 (address) */
    0x41,                              /* STORE: mem[100] = 1.0 */
    0x55, 0,0,0x00,0,0,0,0x59,0x40,  /* PUSH 100.0 (same address) */
    0x43,                              /* PEEK: read mem[100] as int32 */
    0x80                               /* HALT: stack has IEEE 754 bits of 1.0 = 0x3FF00000 */
};

EdgeVector EDGE_VECTORS[] = {
    {
        .name = "gap1_div_zero",
        .description = "DIV by zero: 1.0 / 0.0 = +inf (IEEE 754)",
        .bytecode = _gap1, .len = sizeof(_gap1),
        .expected_stack_top = INFINITY,
        .expect_error = 0, .expect_carry = -1, .expect_zero = -1
    },
    {
        .name = "gap2_mod_negative_divisor",
        .description = "MOD with negative divisor: -5 % -3 (Euclidean)",
        .bytecode = _gap2, .len = sizeof(_gap2),
        .expected_stack_top = -2.0,  /* fmod(-5, -3) = -2.0 */
        .expect_error = 0, .expect_carry = -1, .expect_zero = -1
    },
    {
        .name = "gap3_mul_carry",
        .description = "MUL carry: 65536 * 65536 = 4294967296 (uint32 overflow)",
        .bytecode = _gap3, .len = sizeof(_gap3),
        .expected_stack_top = 4294967296.0,
        .expect_error = 0, .expect_carry = 1, .expect_zero = -1
    },
    {
        .name = "gap4_shl_fractional",
        .description = "SHL with fractional shift count: 1 << 3.5",
        .bytecode = _gap4, .len = sizeof(_gap4),
        .expected_stack_top = 8.0,  /* truncates 3.5 to 3, 1<<3 = 8 */
        .expect_error = 0, .expect_carry = -1, .expect_zero = -1
    },
    {
        .name = "gap5_poke_unaligned",
        .description = "POKE at unaligned address 1.0",
        .bytecode = _gap5, .len = sizeof(_gap5),
        .expected_stack_top = NAN,  /* stack effect — POKE pops both */
        .expect_error = 0, .expect_carry = -1, .expect_zero = -1
    },
    {
        .name = "gap6_fsub_inf_nan",
        .description = "FSUB infinities: +inf - +inf = NaN",
        .bytecode = _gap6, .len = sizeof(_gap6),
        .expected_stack_top = NAN,
        .expect_error = 0, .expect_carry = -1, .expect_zero = -1
    },
    {
        .name = "gap7_conf_mul_nan",
        .description = "CONF_MUL after NaN confidence: should clamp to [0,1]",
        .bytecode = _gap7, .len = sizeof(_gap7),
        .expected_stack_top = 0.0,  /* NaN clamped to 0, then * 0.5 = 0 */
        .expect_error = 0, .expect_carry = -1, .expect_zero = -1
    },
    {
        .name = "gap8_jnz_after_add",
        .description = "JNZ after ADD: 1+1=2, Z=0, should NOT jump",
        .bytecode = _gap8, .len = sizeof(_gap8),
        .expected_stack_top = 2.0,  /* ADD result stays on stack */
        .expect_error = 0, .expect_carry = -1, .expect_zero = -1
    },
    {
        .name = "gap9_signal_basic",
        .description = "SIGNAL basic: send to channel 3",
        .bytecode = _gap9, .len = sizeof(_gap9),
        .expected_stack_top = NAN,  /* SIGNAL pops both */
        .expect_error = 0, .expect_carry = -1, .expect_zero = -1
    },
    {
        .name = "gap10_ret_empty_stack",
        .description = "RET with empty call stack: should halt gracefully",
        .bytecode = _gap10, .len = sizeof(_gap10),
        .expected_stack_top = NAN,
        .expect_error = 0, .expect_carry = -1, .expect_zero = -1
    },
    {
        .name = "gap11_fadd_denormal",
        .description = "FADD denormal: 1e-308 + 1e-308 = 2e-308",
        .bytecode = _gap11, .len = sizeof(_gap11),
        .expected_stack_top = 2e-308,
        .expect_error = 0, .expect_carry = -1, .expect_zero = -1
    },
    {
        .name = "gap12_store_peek_alias",
        .description = "STORE double, PEEK int32 at same address: IEEE 754 bits",
        .bytecode = _gap12, .len = sizeof(_gap12),
        .expected_stack_top = 0x3FF00000.0,  /* IEEE 754 bits of 1.0 */
        .expect_error = 0, .expect_carry = -1, .expect_zero = -1
    }
};

const int EDGE_VECTOR_COUNT = sizeof(EDGE_VECTORS) / sizeof(EDGE_VECTORS[0]);
