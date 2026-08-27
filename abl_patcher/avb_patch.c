* SPDX-License-Identifier: GPL-2.0-only */
#include "avb_patch.h"
#include "arm64_util.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ---- string search helpers ---- */

static int32_t find_mem(const uint8_t *data, int32_t size,
                        const uint8_t *needle, int32_t nlen, int32_t start)
{
    if (nlen <= 0 || start < 0 || start + nlen > size) return -1;
    for (int32_t i = start; i + nlen <= size; i++) {
        if (memcmp(data + i, needle, (size_t)nlen) == 0) return i;
    }
    return -1;
}

/* Search UTF-16LE string (each char followed by 0x00). */
static int32_t find_utf16le(const uint8_t *data, int32_t size,
                            const char *str, int32_t start)
{
    int32_t nlen = (int32_t)strlen(str);
    if (nlen <= 0) return -1;
    uint8_t *buf = (uint8_t *)malloc((size_t)nlen * 2);
    if (!buf) return -1;
    for (int32_t i = 0; i < nlen; i++) {
        buf[i * 2]     = (uint8_t)str[i];
        buf[i * 2 + 1] = 0x00;
    }
    int32_t r = find_mem(data, size, buf, nlen * 2, start);
    free(buf);
    return r;
}

static int32_t find_ascii(const uint8_t *data, int32_t size,
                          const char *str, int32_t start)
{
    return find_mem(data, size, (const uint8_t *)str,
                    (int32_t)strlen(str), start);
}

/* ---- ADRL rewrite ----
 * Rewrite the ADRP+ADD pair at `off` to compute `new_target`.
 * Preserves Rd of ADRP and Rn/Rd of ADD (must match: ADRP.Rd == ADD.Rn).
 */
static bool adrl_rewrite_target(uint8_t *data, int32_t off, uint64_t load_base,
                                int64_t new_target)
{
    uint32_t adrp = arm64_read(data, off);
    uint32_t add  = arm64_read(data, off + 4);
    if (!arm64_is_adrp(adrp) || !arm64_is_add_imm(add)) return false;
    if (arm64_rd(adrp) != arm64_rn(add)) return false;

    uint8_t  adrp_rd = arm64_rd(adrp);
    uint8_t  add_rd  = arm64_rd(add);  /* preserve original destination */
    uint64_t pc   = load_base + (uint64_t)off;
    uint64_t pc_page = pc & ~(uint64_t)0xFFF;
    uint64_t tgt_page = (uint64_t)new_target & ~(uint64_t)0xFFF;

    int64_t page_diff = (int64_t)((tgt_page - pc_page) >> 12);
    /* 21-bit signed immediate */
    if (page_diff < -(1LL << 20) || page_diff >= (1LL << 20)) {
        printf("  [adrl_rewrite] page_diff %lld out of range\n",
               (long long)page_diff);
        return false;
    }
    uint32_t immlo = (uint32_t)(page_diff & 0x3);
    uint32_t immhi = (uint32_t)((page_diff >> 2) & 0x7FFFF);
    uint32_t new_adrp = 0x90000000 | (immlo << 29) | (immhi << 5) | adrp_rd;

    uint32_t add_imm = (uint32_t)((uint64_t)new_target & 0xFFF);
    /* ADD X{add_rd}, X{adrp_rd}, #add_imm - preserve both registers */
    uint32_t new_add = 0x91000000 | (add_imm << 10) | (adrp_rd << 5) | add_rd;

    arm64_write(data, off, new_adrp);
    arm64_write(data, off + 4, new_add);
    return true;
}

/* ---- find all ADRL references to a target address ---- */
#define MAX_REFS 64
static int32_t find_adrl_refs(const uint8_t *data, int32_t size,
                              uint64_t load_base, int64_t target,
                              int32_t *refs, int32_t max_refs)
{
    int32_t count = 0;
    for (int32_t off = 0; off + 8 <= size; off += 4) {
        if (arm64_is_adrl_to(data, off, load_base, target)) {
            if (count < max_refs) refs[count] = off;
            count++;
        }
    }
    return count;
}

/* ================================================================
 * Patch 1: Force verifiedbootstate=green
 *
 * Strategy: find "orange", "yellow", "red" UTF-16LE strings, locate
 * ADRP+ADD pairs that load them, and repoint those pairs to "green".
 * This makes the ABL always report green regardless of internal
 * verification result, hiding the unlocked/disabled state.
 * ================================================================ */
static int32_t patch_force_green(uint8_t *data, int32_t size, uint64_t load_base)
{
    int32_t patched = 0;
    int32_t green_off = find_utf16le(data, size, "green", 0);
    if (green_off < 0) {
        /* try ASCII */
        green_off = find_ascii(data, size, "green", 0);
    }
    if (green_off < 0) {
        printf("[force_green] 'green' string not found, skipping\n");
        return 0;
    }
    int64_t green_target = (int64_t)green_off;
    printf("[force_green] 'green' at file offset 0x%X\n", green_off);

    const char *states[] = { "orange", "yellow", "red" };
    for (int s = 0; s < 3; s++) {
        int32_t off = 0;
        int32_t found = 0;
        while ((off = find_utf16le(data, size, states[s], off)) >= 0) {
            found++;
            int32_t refs[MAX_REFS];
            int32_t nrefs = find_adrl_refs(data, size, load_base,
                                           (int64_t)off, refs, MAX_REFS);
            printf("[force_green] '%s' at 0x%X, %d ADRL reference(s)\n",
                   states[s], off, nrefs);
            for (int r = 0; r < nrefs && r < MAX_REFS; r++) {
                if (adrl_rewrite_target(data, refs[r], load_base, green_target)) {
                    printf("  -> repointed ADRL at 0x%X to green\n", refs[r]);
                    patched++;
                }
            }
            off += (int32_t)strlen(states[s]) * 2 + 2;
        }
        /* also try ASCII variant */
        off = 0;
        while ((off = find_ascii(data, size, states[s], off)) >= 0) {
            int32_t refs[MAX_REFS];
            int32_t nrefs = find_adrl_refs(data, size, load_base,
                                           (int64_t)off, refs, MAX_REFS);
            if (nrefs > 0) {
                printf("[force_green] '%s' (ASCII) at 0x%X, %d ADRL ref(s)\n",
                       states[s], off, nrefs);
                for (int r = 0; r < nrefs && r < MAX_REFS; r++) {
                    if (adrl_rewrite_target(data, refs[r], load_base,
                                            green_target)) {
                        printf("  -> repointed ADRL at 0x%X to green\n",
                               refs[r]);
                        patched++;
                    }
                }
            }
            off += (int32_t)strlen(states[s]) + 1;
        }
        if (found == 0) {
            printf("[force_green] '%s' not found\n", states[s]);
        }
    }
    return patched;
}

/* ================================================================
 * Patch 2: Short-circuit AVB verification entry
 *
 * Strategy: locate the "AVB0" magic constant or vbmeta-related
 * strings, trace back to the enclosing function, and patch the
 * function prologue to:  MOV X0, XZR  (return 0 = EFI_SUCCESS)
 *                       RET
 *
 * This makes the ABL skip all partition verification (boot,
 * init_boot, vendor_boot, dtbo, system, vendor, etc.) while
 * continuing the boot flow normally.
 * ================================================================ */
static int32_t patch_short_circuit_verify(uint8_t *data, int32_t size,
                                          uint64_t load_base)
{
    int32_t patched = 0;
    int32_t func_starts[16];
    int32_t nfuncs = 0;

    /* Search for "AVB0" magic bytes */
    const uint8_t avb0[] = { 'A', 'V', 'B', '0' };
    int32_t off = 0;
    while ((off = find_mem(data, size, avb0, 4, off)) >= 0) {
        printf("[short_circuit] 'AVB0' at offset 0x%X\n", off);
        /* Find code references to this offset via ADRL */
        int32_t refs[MAX_REFS];
        int32_t nrefs = find_adrl_refs(data, size, load_base,
                                       (int64_t)off, refs, MAX_REFS);
        for (int r = 0; r < nrefs && r < MAX_REFS; r++) {
            int32_t fstart = arm64_find_function_start(
                data, size, refs[r], 1024);
            if (fstart >= 0) {
                /* deduplicate */
                bool dup = false;
                for (int i = 0; i < nfuncs; i++) {
                    if (func_starts[i] == fstart) { dup = true; break; }
                }
                if (!dup && nfuncs < 16) {
                    func_starts[nfuncs++] = fstart;
                    printf("  -> function start at 0x%X (ref at 0x%X)\n",
                           fstart, refs[r]);
                }
            }
        }
        off += 4;
    }

    /* Also search for "vbmeta" string references */
    off = 0;
    while ((off = find_ascii(data, size, "vbmeta", off)) >= 0) {
        int32_t refs[MAX_REFS];
        int32_t nrefs = find_adrl_refs(data, size, load_base,
                                       (int64_t)off, refs, MAX_REFS);
        for (int r = 0; r < nrefs && r < MAX_REFS; r++) {
            int32_t fstart = arm64_find_function_start(
                data, size, refs[r], 1024);
            if (fstart >= 0) {
                bool dup = false;
                for (int i = 0; i < nfuncs; i++) {
                    if (func_starts[i] == fstart) { dup = true; break; }
                }
                if (!dup && nfuncs < 16) {
                    func_starts[nfuncs++] = fstart;
                    printf("[short_circuit] 'vbmeta' ref at 0x%X -> func 0x%X\n",
                           refs[r], fstart);
                }
            }
        }
        off += 6;
    }

    /* Also search for "VerifiedBoot" or "verified_boot" strings */
    const char *vb_strings[] = { "VerifiedBoot", "verified_boot",
                                 "AVB ", "avb_", "Avb" };
    for (int s = 0; s < 5; s++) {
        off = 0;
        while ((off = find_ascii(data, size, vb_strings[s], off)) >= 0) {
            int32_t refs[MAX_REFS];
            int32_t nrefs = find_adrl_refs(data, size, load_base,
                                           (int64_t)off, refs, MAX_REFS);
            for (int r = 0; r < nrefs && r < MAX_REFS; r++) {
                int32_t fstart = arm64_find_function_start(
                    data, size, refs[r], 1024);
                if (fstart >= 0) {
                    bool dup = false;
                    for (int i = 0; i < nfuncs; i++) {
                        if (func_starts[i] == fstart) { dup = true; break; }
                    }
                    if (!dup && nfuncs < 16) {
                        func_starts[nfuncs++] = fstart;
                        printf("[short_circuit] '%s' ref at 0x%X -> func 0x%X\n",
                               vb_strings[s], refs[r], fstart);
                    }
                }
            }
            off += (int32_t)strlen(vb_strings[s]) + 1;
        }
    }

    /* Patch each unique function */
    for (int i = 0; i < nfuncs; i++) {
        int32_t fstart = func_starts[i];
        if (fstart + 8 > size) continue;
        uint32_t orig0 = arm64_read(data, fstart);
        uint32_t orig1 = arm64_read(data, fstart + 4);
        /* MOV X0, XZR ; RET */
        arm64_write(data, fstart, arm64_mov_x0_zero());
        arm64_write(data, fstart + 4, arm64_ret());
        printf("[short_circuit] patched function at 0x%X: "
               "%08X %08X -> MOV X0,XZR ; RET\n",
               fstart, orig0, orig1);
        patched++;
    }

    if (nfuncs == 0) {
        printf("[short_circuit] no verification functions located\n");
    }
    return patched;
}

/* ================================================================
 * Patch 3: NOP branches to error/red state
 *
 * Strategy: find conditional branches (CBZ/CBNZ/B.cond) that target
 * code referencing "red" or error strings, and NOP them so the
 * boot flow never enters the failure path.
 * ================================================================ */
static int32_t patch_nop_error_branches(uint8_t *data, int32_t size,
                                        uint64_t load_base)
{
    int32_t patched = 0;

    /* Find "red" string addresses (UTF-16LE and ASCII) */
    int32_t red_offs[16];
    int32_t nred = 0;
    int32_t off = 0;
    while ((off = find_utf16le(data, size, "red", off)) >= 0 && nred < 16) {
        red_offs[nred++] = off;
        off += 8;
    }
    off = 0;
    while ((off = find_ascii(data, size, "red", off)) >= 0 && nred < 16) {
        red_offs[nred++] = off;
        off += 4;
    }

    if (nred == 0) {
        printf("[nop_error] no 'red' string found, skipping\n");
        return 0;
    }

    /* For each red string, find ADRL refs, then find branches that
     * target into the red-handling code region. */
    for (int ri = 0; ri < nred; ri++) {
        int32_t refs[MAX_REFS];
        int32_t nrefs = find_adrl_refs(data, size, load_base,
                                       (int64_t)red_offs[ri], refs, MAX_REFS);
        for (int r = 0; r < nrefs && r < MAX_REFS; r++) {
            int32_t red_code_start = refs[r];
            /* Scan backwards up to 64 instructions for a conditional branch
             * that targets at or after red_code_start. */
            for (int32_t scan = red_code_start - 4;
                 scan >= red_code_start - 256 && scan >= 0;
                 scan -= 4) {
                uint32_t ins = arm64_read(data, scan);
                if (arm64_is_cbz(ins) || arm64_is_cbnz(ins) ||
                    arm64_is_b_cond(ins)) {
                    int32_t tgt = arm64_branch_target(scan, ins);
                    if (tgt >= red_code_start - 8 && tgt <= red_code_start + 32) {
                        printf("[nop_error] NOP branch at 0x%X -> 0x%X "
                               "(red handler at 0x%X)\n",
                               scan, tgt, red_code_start);
                        arm64_write(data, scan, arm64_nop());
                        patched++;
                        break; /* one per ref is enough */
                    }
                }
            }
        }
    }

    /* Also look for "error" / "ERROR" / "failed" strings and NOP
     * branches to their handlers. */
    const char *err_strs[] = { "ERROR", "error", "failed", "FAILED",
                               "Verification failed", "boot failure" };
    for (int s = 0; s < 6; s++) {
        off = 0;
        while ((off = find_ascii(data, size, err_strs[s], off)) >= 0) {
            int32_t refs[MAX_REFS];
            int32_t nrefs = find_adrl_refs(data, size, load_base,
                                           (int64_t)off, refs, MAX_REFS);
            for (int r = 0; r < nrefs && r < MAX_REFS; r++) {
                for (int32_t scan = refs[r] - 4;
                     scan >= refs[r] - 256 && scan >= 0;
                     scan -= 4) {
                    uint32_t ins = arm64_read(data, scan);
                    if (arm64_is_cbz(ins) || arm64_is_cbnz(ins) ||
                        arm64_is_b_cond(ins)) {
                        int32_t tgt = arm64_branch_target(scan, ins);
                        if (tgt >= refs[r] - 8 && tgt <= refs[r] + 32) {
                            printf("[nop_error] NOP branch at 0x%X -> 0x%X "
                                   "('%s' handler)\n",
                                   scan, tgt, err_strs[s]);
                            arm64_write(data, scan, arm64_nop());
                            patched++;
                            break;
                        }
                    }
                }
            }
            off += (int32_t)strlen(err_strs[s]) + 1;
        }
    }

    return patched;
}

/* ================================================================
 * Main entry
 * ================================================================ */
bool avb_patch_abl(uint8_t *data, int32_t size, uint64_t load_base,
                   avb_patch_result_t *result)
{
    if (!data || size < 4096) {
        printf("[avb_patch] buffer too small (%d)\n", size);
        return false;
    }
    memset(result, 0, sizeof(*result));

    printf("=== AVB BL-Stage Patcher ===\n");
    printf("Buffer size: %d bytes (0x%X)\n", size, size);
    printf("Load base: 0x%llX\n\n", (unsigned long long)load_base);

    /* Count strings found */
    if (find_utf16le(data, size, "green", 0) >= 0 ||
        find_ascii(data, size, "green", 0) >= 0) result->strings_found++;
    if (find_utf16le(data, size, "orange", 0) >= 0 ||
        find_ascii(data, size, "orange", 0) >= 0) result->strings_found++;

    /* Count AVB0 references */
    const uint8_t avb0[] = { 'A', 'V', 'B', '0' };
    int32_t off = 0;
    while ((off = find_mem(data, size, avb0, 4, off)) >= 0) {
        result->avb0_references++;
        off += 4;
    }
    printf("AVB0 occurrences: %d\n", result->avb0_references);
    printf("Boot-state strings found: %d\n\n", result->strings_found);

    /* Apply patches in order: short-circuit first, then force green,
     * then NOP error branches. Short-circuit may eliminate the need
     * for the others, but we apply all for robustness. */
    result->verify_shortcircuited =
        patch_short_circuit_verify(data, size, load_base);
    printf("\n");

    result->green_forced =
        patch_force_green(data, size, load_base);
    printf("\n");

    result->error_branches_noped =
        patch_nop_error_branches(data, size, load_base);
    printf("\n");

    printf("=== Patch Summary ===\n");
    printf("  Verification functions short-circuited: %d\n",
           result->verify_shortcircuited);
    printf("  State strings forced to green: %d\n",
           result->green_forced);
    printf("  Error branches NOPed: %d\n",
           result->error_branches_noped);

    int32_t total = result->verify_shortcircuited +
                    result->green_forced +
                    result->error_branches_noped;
    printf("  Total patches: %d\n", total);

    return total > 0;
}
