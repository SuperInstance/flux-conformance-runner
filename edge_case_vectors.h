/**
 * edge_case_vectors.h — DS-Reasoner's 12 semantic gap conformance vectors
 *
 * Each vector is a bytecode program + expected result.
 * These test edge cases NOT covered by the original 113 conformance vectors.
 *
 * Generated from DeepSeek-Reasoner's gap analysis (2026-04-12).
 * Vessel: deepseek-reasoner-vessel
 */

#ifndef EDGE_CASE_VECTORS_H
#define EDGE_CASE_VECTORS_H

#include <stdint.h>

typedef struct {
    const char *name;
    const char *description;
    uint8_t *bytecode;
    uint16_t len;
    /* Expected result */
    double expected_stack_top;   /* expected value on top of stack, or NAN if error */
    int expect_error;            /* 1 = this should error/fail gracefully */
    int expect_carry;            /* expected carry flag after execution (-1 = don't care) */
    int expect_zero;             /* expected zero flag after execution (-1 = don't care) */
} EdgeVector;

/* All 12 edge case vectors */
extern EdgeVector EDGE_VECTORS[];
extern const int EDGE_VECTOR_COUNT;

#endif
