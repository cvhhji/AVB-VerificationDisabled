/* SPDX-License-Identifier: GPL-2.0-only */
#include "arm64_util.h"
#include <string.h>
#include <stdio.h>

/* ---- instruction read/write (little-endian) ---- */
uint32_t arm64_read(const uint8_t *buf, int32_t off)
{
    if (off < 0 || off + 4 < 0) return 0;
    return (uint32_t)buf[off]
         | ((uint32_t)buf[off + 1] << 8)
         | ((uint32_t)buf[off + 2] << 16)
         | ((uint32_t)buf[off + 3] << 24);
}

void arm64_write(uint8_t *buf, int32_t off, uint32_t instr)
{
    buf[off]     = (uint8_t)(instr & 0xFF);
    buf[off + 1] = (uint8_t)((instr >> 8) & 0xFF);
    buf[off + 2] = (uint8_t)((instr >> 16) & 0xFF);
    buf[off + 3] = (uint8_t)((instr >> 24) & 0xFF);
}

/* ---- register accessors ---- */
uint8_t arm64_rd(uint32_t instr)  { return (uint8_t)(instr & 0x1F); }
uint8_t arm64_rn(uint32_t instr)  { return (uint8_t)((instr >> 5) & 0x1F); }
uint8_t arm64_rt(uint32_t instr)  { return (uint8_t)(instr & 0x1F); }
uint8_t arm64_rt2(uint32_t instr) { return (uint8_t)((instr >> 10) & 0x1F); }

/* ---- classification ---- */
bool arm64_is_adrp(uint32_t instr)
{
    /* ADRP: 1 immlo 1 0000 immhi Rd
     * bits 31=1, 28:24=10000 */
    return (instr & 0x9F000000) == 0x90000000;
}

bool arm64_is_add_imm(uint32_t instr)
{
    /* ADD (immediate) 64-bit: high byte 0x91.
     * Matches both shift=0 and shift=1 (LSL #12) variants. */
    return (instr & 0xFF000000) == 0x91000000;
}

bool arm64_is_sub_imm(uint32_t instr)
{
    /* SUB (immediate) 64-bit: 0xD1... */
    return (instr & 0xFF800000) == 0xD1000000;
}

bool arm64_is_ldr(uint32_t instr)
{
    /* LDR (immediate, unsigned offset) 64-bit: 11 111 0 01 01 imm12 Rn Rt
     * pattern 0xF9400000 */
    return (instr & 0xFFC00000) == 0xF9400000;
}

bool arm64_is_ldrb(uint32_t instr)
{
    /* LDRB (immediate, unsigned offset): 00 111 0 01 01 imm12 Rn Rt
     * pattern 0x39400000 */
    return (instr & 0xFFC00000) == 0x39400000;
}

bool arm64_is_strb(uint32_t instr)
{
    /* STRB (immediate, unsigned offset): 00 111 0 01 00 imm12 Rn Rt
     * pattern 0x39000000 */
    return (instr & 0xFFC00000) == 0x39000000;
}

bool arm64_is_movz(uint32_t instr)
{
    /* MOVZ: sf 10 100101 hw imm16 Rd -> 0xD2800000 (64-bit) / 0x52800000 (32-bit) */
    return (instr & 0xFF800000) == 0xD2800000 ||
           (instr & 0xFF800000) == 0x52800000;
}

bool arm64_is_movk(uint32_t instr)
{
    /* MOVK: sf 11 100101 hw imm16 Rd -> 0xF2800000 / 0x72800000 */
    return (instr & 0xFF800000) == 0xF2800000 ||
           (instr & 0xFF800000) == 0x72800000;
}

bool arm64_is_and_imm(uint32_t instr)
{
    /* AND (immediate) 64-bit: 10 0 100100 N immr imms Rn Rd
     * pattern 0x92000000 */
    return (instr & 0xFF800000) == 0x92000000;
}

bool arm64_is_tst_imm(uint32_t instr)
{
    /* TST = ANDS XZR, Xn, #imm: pattern 0xF2400000 (64-bit) */
    return (instr & 0xFF80001F) == 0xF240001F;
}

bool arm64_is_cbz(uint32_t instr)
{
    /* CBZ: 0 0 1 1 0 1 0 imm19 Rt  (32-bit 0x34000000, 64-bit 0xB4000000) */
    return (instr & 0x7E000000) == 0x34000000;
}

bool arm64_is_cbnz(uint32_t instr)
{
    /* CBNZ: 0 0 1 1 0 1 1 imm19 Rt (32-bit 0x35000000, 64-bit 0xB5000000) */
    return (instr & 0x7E000000) == 0x35000000;
}

bool arm64_is_b_cond(uint32_t instr)
{
    /* B.cond: 01010100 imm19 0 cond */
    return (instr & 0xFF000010) == 0x54000000;
}

bool arm64_is_b_uncond(uint32_t instr)
{
    /* B: 000101 imm26 */
    return (instr & 0xFC000000) == 0x14000000;
}

bool arm64_is_bl(uint32_t instr)
{
    /* BL: 100101 imm26 */
    return (instr & 0xFC000000) == 0x94000000;
}

bool arm64_is_ret(uint32_t instr)
{
    /* RET: 1101011 0 0 1 0 11111 00000 11110 00000 = 0xD65F03C0 */
    return instr == 0xD65F03C0;
}

bool arm64_is_nop(uint32_t instr)
{
    return instr == 0xD503201F;
}

bool arm64_is_stp(uint32_t instr)
{
    /* STP (64-bit, all addressing modes): high byte = 0xA9.
     * LDP (64-bit) has high byte 0xE9, 32-bit STP has 0x29,
     * so checking only the high byte is sufficient. */
    return (instr & 0xFF000000) == 0xA9000000;
}

bool arm64_is_cmp_imm(uint32_t instr)
{
    /* CMP = SUBS XZR, Xn, #imm: 64-bit pattern 0xF100001F (Rd=XZR=31) */
    return (instr & 0xFF80001F) == 0xF100001F;
}

bool arm64_is_cset(uint32_t instr)
{
    /* CSET: 0 1 0 1 1 0 1 0 1 0 0 1 1 1 1 1 0 0 cond Rd
     * pattern 0x1A9F0000 & 0xFFFF0000 = 0x1A9F0000? Actually:
     * CSET Xd, cond = CSINC Xd, XZR, XZR, cond
     * encoding: sf 0 0 1 1 0 1 0 1 0 0 1 1 1 1 1 1 0 0 cond Rd
     * 64-bit: 0x9A9F0000 & mask */
    return (instr & 0xFFFF0000) == 0x9A9F0000 ||
           (instr & 0xFFFF0000) == 0x1A9F0000;
}

/* ---- ADRP+ADD target calculation ---- */
int64_t arm64_calc_adrl_target(const uint8_t *buf, int32_t off, uint64_t load_base)
{
    uint32_t adrp = arm64_read(buf, off);
    uint32_t add  = arm64_read(buf, off + 4);

    if (!arm64_is_adrp(adrp) || !arm64_is_add_imm(add)) return -1;
    if (arm64_rd(adrp) != arm64_rn(add)) return -1;

    /* ADRP: immlo = bits 30:29, immhi = bits 23:5 */
    uint32_t immlo = (adrp >> 29) & 0x3;
    uint32_t immhi = (adrp >> 5) & 0x7FFFF;
    int64_t  imm   = (int64_t)((immhi << 2) | immlo);
    /* sign extend 21-bit */
    if (imm & (1LL << 20)) imm |= ~((1LL << 21) - 1);

    /* PC at runtime = load_base + off */
    uint64_t pc      = load_base + (uint64_t)off;
    uint64_t page    = (pc & ~(uint64_t)0xFFF) + (uint64_t)(imm << 12);

    /* ADD: imm12 = bits 21:10, shift = bit 22 */
    uint32_t add_imm  = (add >> 10) & 0xFFF;
    uint32_t add_shift = (add >> 22) & 0x1;
    uint64_t add_val  = (uint64_t)add_imm << (add_shift ? 12 : 0);

    return (int64_t)(page + add_val);
}

bool arm64_is_adrl_to(const uint8_t *buf, int32_t off, uint64_t load_base,
                      int64_t target)
{
    int64_t calc = arm64_calc_adrl_target(buf, off, load_base);
    return calc == target;
}

/* ---- branch target ---- */
int32_t arm64_branch_target(int32_t off, uint32_t instr)
{
    int64_t imm;
    if (arm64_is_b_uncond(instr) || arm64_is_bl(instr)) {
        imm = (int64_t)(instr & 0x3FFFFFF);
        if (imm & (1LL << 25)) imm |= ~((1LL << 26) - 1);
        return off + (int32_t)(imm << 2);
    }
    if (arm64_is_cbz(instr) || arm64_is_cbnz(instr)) {
        imm = (int64_t)((instr >> 5) & 0x7FFFF);
        if (imm & (1LL << 18)) imm |= ~((1LL << 19) - 1);
        return off + (int32_t)(imm << 2);
    }
    if (arm64_is_b_cond(instr)) {
        imm = (int64_t)((instr >> 5) & 0x7FFFF);
        if (imm & (1LL << 18)) imm |= ~((1LL << 19) - 1);
        return off + (int32_t)(imm << 2);
    }
    return -1;
}

/* ---- encodings ---- */
uint32_t arm64_nop(void)          { return 0xD503201F; }
uint32_t arm64_ret(void)          { return 0xD65F03C0; }
uint32_t arm64_mov_x0_zero(void)  { return 0xAA1F03E0; } /* ORR X0, XZR, XZR */

uint32_t arm64_movz_w(uint8_t rd, uint16_t imm)
{
    /* MOVZ Wd, #imm: 0 10 1 00101 hw=00 imm16 rd */
    return 0x52800000 | ((uint32_t)imm << 5) | (rd & 0x1F);
}

uint32_t arm64_b(int32_t from, int32_t to)
{
    int32_t diff = (to - from) >> 2;
    return 0x14000000 | ((uint32_t)diff & 0x3FFFFFF);
}

/* ---- function prologue detection ---- */
int32_t arm64_find_function_start(const uint8_t *buf, int32_t size,
                                  int32_t from, int32_t max_scan)
{
    int32_t start = from - max_scan;
    if (start < 0) start = 0;
    /* align down */
    start &= ~3;

    for (int32_t off = from - 4; off >= start; off -= 4) {
        if (off < 0 || off + 4 > size) break;
        uint32_t ins = arm64_read(buf, off);
        /* STP X?, X?, [SP, #-N]! or STP X?, X?, [SP, #N] */
        if (arm64_is_stp(ins) && arm64_rn(ins) == 31) {
            return off;
        }
        /* SUB SP, SP, #imm */
        if (arm64_is_sub_imm(ins) && arm64_rd(ins) == 31 && arm64_rn(ins) == 31) {
            return off;
        }
        /* MOV SP, Xn (unlikely but possible) */
    }
    return -1;
}
