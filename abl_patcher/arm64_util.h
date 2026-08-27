* SPDX-License-Identifier: GPL-2.0-only
 *
 * arm64 instruction utilities for ABL binary patching.
 * Based on patterns observed in Qualcomm ABL (EDK2) arm64 code.
 */
#ifndef ARM64_UTIL_H
#define ARM64_UTIL_H

#include <stdint.h>
#include <stdbool.h>

/* ---- instruction read/write ---- */
uint32_t arm64_read(const uint8_t *buf, int32_t off);
void     arm64_write(uint8_t *buf, int32_t off, uint32_t instr);

/* ---- instruction classification ---- */
bool arm64_is_adrp(uint32_t instr);
bool arm64_is_add_imm(uint32_t instr);      /* ADD Xd, Xn, #imm */
bool arm64_is_sub_imm(uint32_t instr);      /* SUB Xd, Xn, #imm */
bool arm64_is_ldr(uint32_t instr);           /* LDR Xt, [Xn,#imm] */
bool arm64_is_ldrb(uint32_t instr);          /* LDRB Wt, [Xn,#imm] */
bool arm64_is_strb(uint32_t instr);          /* STRB Wt, [Xn,#imm] */
bool arm64_is_movz(uint32_t instr);          /* MOVZ Xd, #imm */
bool arm64_is_movk(uint32_t instr);          /* MOVK Xd, #imm */
bool arm64_is_and_imm(uint32_t instr);       /* AND Xd, Xn, #imm */
bool arm64_is_tst_imm(uint32_t instr);       /* TST Xn, #imm (ANDS XZR,...) */
bool arm64_is_cbz(uint32_t instr);           /* CBZ Xt, label */
bool arm64_is_cbnz(uint32_t instr);          /* CBNZ Xt, label */
bool arm64_is_b_cond(uint32_t instr);        /* B.cond label */
bool arm64_is_b_uncond(uint32_t instr);      /* B label */
bool arm64_is_bl(uint32_t instr);            /* BL label */
bool arm64_is_ret(uint32_t instr);           /* RET */
bool arm64_is_nop(uint32_t instr);           /* NOP */
bool arm64_is_stp(uint32_t instr);           /* STP Xt, Xt2, [SP,#imm] */
bool arm64_is_cmp_imm(uint32_t instr);       /* CMP Xn, #imm (SUBS XZR,...) */
bool arm64_is_cset(uint32_t instr);          /* CSET Xd, cond */

/* ---- register accessors ---- */
uint8_t arm64_rd(uint32_t instr);
uint8_t arm64_rn(uint32_t instr);
uint8_t arm64_rt(uint32_t instr);
uint8_t arm64_rt2(uint32_t instr);

/* ---- ADRP+ADD pair ----
 * Returns the target address calculated from ADRP at off and ADD at off+4.
 * load_base is the runtime address corresponding to file offset 0.
 * Returns -1 on failure.
 */
int64_t arm64_calc_adrl_target(const uint8_t *buf, int32_t off, uint64_t load_base);

/* Returns true if the instruction at off is ADRP and off+4 is ADD with
 * matching Rd/Rn, and the computed target matches `target`. */
bool arm64_is_adrl_to(const uint8_t *buf, int32_t off, uint64_t load_base,
                      int64_t target);

/* ---- branch target ---- */
int32_t arm64_branch_target(int32_t off, uint32_t instr);

/* ---- common encodings ---- */
uint32_t arm64_nop(void);
uint32_t arm64_ret(void);
uint32_t arm64_mov_x0_zero(void);    /* MOV X0, XZR -> 0xAA1F03E0 */
uint32_t arm64_movz_w(uint8_t rd, uint16_t imm);
uint32_t arm64_b(int32_t from, int32_t to);

/* ---- function prologue detection ----
 * Scans backwards from `from` to find a likely function start (STP with SP,
 * or SUB SP,SP,#imm). Returns the offset, or -1 if not found within
 * max_scan bytes.
 */
int32_t arm64_find_function_start(const uint8_t *buf, int32_t size,
                                  int32_t from, int32_t max_scan);

#endif /* ARM64_UTIL_H */
