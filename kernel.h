#pragma once


/* ------------------------------------------------------------------
 * Type definitions
 * ------------------------------------------------------------------
 * These sizes assume a 32-bit RISC-V target (RV32), where:
 *   char = 8 bits, int = 32 bits, long = 32 bits, pointers = 32 bits.
 * (On RV32 "long" is 32 bits, the same width as a register. That is
 * why the SBI interface below uses "long" for register-sized values.)
 */

/* An unsigned 8-bit integer (one byte, range 0..255). Used to access
 * memory byte by byte. */
typedef unsigned char uint8_t;

/* An unsigned 32-bit integer */
typedef unsigned int uint32_t;

/* The type used for sizes and byte counts (what sizeof yields, and
 * what memset() takes as its length). */
typedef uint32_t size_t;


/* ------------------------------------------------------------------
 * SBI return value
 * ------------------------------------------------------------------
 * Every SBI call returns two register-sized values: a0 holds an
 * error code and a1 holds a result value. This struct mirrors that
 * pair, so sbi_call() can hand both back to the caller.
 *
 *   error  0 on success, a negative number on failure (the exact
 *          codes are defined by the SBI specification).
 *   value  the call's result, if it has one. Meaningless when the
 *          call fails, or for calls that return nothing.
 */
typedef struct sbiret {
    long error;
    long value;
} sbiret;