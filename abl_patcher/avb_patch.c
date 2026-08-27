/* SPDX-License-Identifier: GPL-2.0-only */
#include "avb_patch.h"
#include "arm64_util.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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

static inline uint64_t read_u64(const uint8_t *data, int32_t off)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)data[off + i] << (i * 8);
    return v;
}

static inline void write_u64(uint8_t *data, int32_t off, uint64_t v)
{
    for (int i = 0; i < 8; i++) data[off + i] = (uint8_t)(v >> (i * 8));
}

static inline uint16_t read_u16(const uint8_t *data, int32_t off)
{
    return (uint16_t)data[off] | ((uint16_t)data[off + 1] << 8);
}

static inline uint32_t read_u32(const uint8_t *data, int32_t off)
{
    return (uint32_t)data[off] | ((uint32_t)data[off + 1] << 8) |
           ((uint32_t)data[off + 2] << 16) | ((uint32_t)data[off + 3] << 24);
}

/* ---- PE/COFF detection and section parsing ---- */

typedef struct {
    int32_t data_start;
    int32_t data_end;
    int     is_pe;
} pe_info_t;

static int parse_pe_sections(const uint8_t *data, int32_t size, pe_info_t *info)
{
    memset(info, 0, sizeof(*info));
    if (size < 0x40 || data[0] != 'M' || data[1] != 'Z') return -1;

    uint32_t pe_ptr = read_u32(data, 0x3C);
    if (pe_ptr + 24 > (uint32_t)size) return -1;
    if (data[pe_ptr] != 'P' || data[pe_ptr + 1] != 'E') return -1;

    info->is_pe = 1;
    uint16_t num_sec = read_u16(data, pe_ptr + 6);
    uint16_t opt_size = read_u16(data, pe_ptr + 0x14);
    uint32_t sec_table = pe_ptr + 0x18 + opt_size;

    for (int i = 0; i < num_sec; i++) {
        uint32_t soff = sec_table + (uint32_t)i * 0x28;
        if (soff + 0x28 > (uint32_t)size) break;
        char name[9] = {0};
        memcpy(name, data + soff, 8);
        uint32_t raw_ptr = read_u32(data, soff + 20);
        uint32_t raw_size = read_u32(data, soff + 16);
        if (strcmp(name, ".data") == 0 || strcmp(name, ".rdata") == 0 ||
            strcmp(name, ".sd") == 0) {
            if (info->data_start == 0 || (int32_t)raw_ptr < info->data_start)
                info->data_start = (int32_t)raw_ptr;
            int32_t end = (int32_t)(raw_ptr + raw_size);
            if (end > info->data_end) info->data_end = end;
        }
    }
    if (info->data_start == 0) {
        /* fallback: use last 1/3 of binary as data */
        info->data_start = size * 2 / 3;
        info->data_end = size;
    }
    return 0;
}

/* ---- ADRL rewrite ---- */

static bool adrl_rewrite_target(uint8_t *data, int32_t off, uint64_t load_base,
                                int64_t new_target)
{
    uint32_t adrp = arm64_read(data, off);
    uint32_t add  = arm64_read(data, off + 4);
    if (!arm64_is_adrp(adrp) || !arm64_is_add_imm(add)) return false;
    if (arm64_rd(adrp) != arm64_rn(add)) return false;

    uint8_t  adrp_rd = arm64_rd(adrp);
    uint8_t  add_rd  = arm64_rd(add);
    uint64_t pc   = load_base + (uint64_t)off;
    uint64_t pc_page = pc & ~(uint64_t)0xFFF;
    uint64_t tgt_page = (uint64_t)new_target & ~(uint64_t)0xFFF;

    int64_t page_diff = (int64_t)((tgt_page - pc_page) >> 12);
    if (page_diff < -(1LL << 20) || page_diff >= (1LL << 20)) return false;
    uint32_t immlo = (uint32_t)(page_diff & 0x3);
    uint32_t immhi = (uint32_t)((page_diff >> 2) & 0x7FFFF);
    uint32_t new_adrp = 0x90000000 | (immlo << 29) | (immhi << 5) | adrp_rd;
    uint32_t add_imm = (uint32_t)((uint64_t)new_target & 0xFFF);
    uint32_t new_add = 0x91000000 | (add_imm << 10) | (adrp_rd << 5) | add_rd;

    arm64_write(data, off, new_adrp);
    arm64_write(data, off + 4, new_add);
    return true;
}

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
 * Patch 1: Boot state pointer array patching
 *
 * Many Qualcomm ABL builds store boot state names in a name-value
 * array in .data: { const char *name; uint64_t value; } with 16-byte
 * stride.  Entries for green/orange/yellow/red are looked up by name.
 *
 * Strategy: find 64-bit pointers to "orange"/"yellow"/"red" strings
 * in the data section and redirect them to "green".  If a 16-byte
 * stride is detected, also overwrite the adjacent value field with
 * green's value so both name and enum match.
 * ================================================================ */
static int32_t patch_boot_state_array(uint8_t *data, int32_t size,
                                      const pe_info_t *pe)
{
    int32_t patched = 0;

    /* Find green string (ASCII first, then UTF-16LE) */
    int32_t green_off = find_ascii(data, size, "green", 0);
    if (green_off < 0) green_off = find_utf16le(data, size, "green", 0);
    if (green_off < 0) {
        printf("[boot_state_array] 'green' not found, skipping\n");
        return 0;
    }
    uint64_t green_ptr = (uint64_t)green_off;
    printf("[boot_state_array] 'green' at 0x%X\n", green_off);

    /* Find green's value (search for pointer to green in data section,
     * then read the adjacent 8-byte value) */
    uint64_t green_value = 0;
    int32_t green_value_off = -1;
    for (int32_t off = pe->data_start; off + 16 <= pe->data_end; off += 8) {
        if (read_u64(data, off) == green_ptr) {
            green_value = read_u64(data, off + 8);
            green_value_off = off;
            printf("[boot_state_array] green entry at 0x%X, value=0x%llX\n",
                   off, (unsigned long long)green_value);
            break;
        }
    }

    /* For each non-green state, find ALL occurrences and check each for
     * data-section pointers.  Some ABLs have multiple "red"/"orange"
     * strings; only the boot-state one has a pointer in .data. */
    const char *states[] = { "orange", "yellow", "red" };
    for (int s = 0; s < 3; s++) {
        int32_t str_off = 0;
        int found = 0;
        while ((str_off = find_ascii(data, size, states[s], str_off)) >= 0) {
            uint64_t str_ptr = (uint64_t)str_off;
            /* Search data section for 64-bit pointers to this string */
            for (int32_t off = pe->data_start; off + 8 <= pe->data_end; off += 8) {
                if (read_u64(data, off) == str_ptr) {
                    printf("[boot_state_array] '%s' at 0x%X, pointer at 0x%X "
                           "-> redirect to green\n", states[s], str_off, off);
                    write_u64(data, off, green_ptr);
                    patched++;
                    found++;
                    /* If 16-byte name+value entry, also fix value */
                    if (green_value_off >= 0 && off + 16 <= size) {
                        uint64_t old_val = read_u64(data, off + 8);
                        if (old_val != green_value) {
                            write_u64(data, off + 8, green_value);
                            printf("     value 0x%llX -> 0x%llX\n",
                                   (unsigned long long)old_val,
                                   (unsigned long long)green_value);
                            patched++;
                        }
                    }
                }
            }
            str_off += (int32_t)strlen(states[s]) + 1;
        }
        /* Also try UTF-16LE */
        str_off = 0;
        while ((str_off = find_utf16le(data, size, states[s], str_off)) >= 0) {
            uint64_t str_ptr = (uint64_t)str_off;
            for (int32_t off = pe->data_start; off + 8 <= pe->data_end; off += 8) {
                if (read_u64(data, off) == str_ptr) {
                    printf("[boot_state_array] '%s' (UTF-16LE) at 0x%X, "
                           "pointer at 0x%X -> green\n", states[s], str_off, off);
                    write_u64(data, off, green_ptr);
                    patched++;
                    found++;
                    if (green_value_off >= 0 && off + 16 <= size) {
                        uint64_t old_val = read_u64(data, off + 8);
                        if (old_val != green_value) {
                            write_u64(data, off + 8, green_value);
                            patched++;
                        }
                    }
                }
            }
            str_off += (int32_t)strlen(states[s]) * 2 + 2;
        }
        if (!found) {
            printf("[boot_state_array] '%s': no data-section pointers found\n",
                   states[s]);
        }
    }
    return patched;
}

/* ================================================================
 * Patch 2: Force green via ADRL references (fallback)
 *
 * For ABL builds that reference boot state strings directly via
 * ADRP+ADD rather than through a pointer array.
 * ================================================================ */
static int32_t patch_force_green_adrl(uint8_t *data, int32_t size,
                                      uint64_t load_base)
{
    int32_t patched = 0;
    int32_t green_off = find_utf16le(data, size, "green", 0);
    if (green_off < 0) green_off = find_ascii(data, size, "green", 0);
    if (green_off < 0) return 0;
    int64_t green_target = (int64_t)green_off;

    const char *states[] = { "orange", "yellow", "red" };
    for (int s = 0; s < 3; s++) {
        int32_t off = 0;
        while ((off = find_utf16le(data, size, states[s], off)) >= 0) {
            int32_t refs[MAX_REFS];
            int32_t nrefs = find_adrl_refs(data, size, load_base,
                                           (int64_t)off, refs, MAX_REFS);
            for (int r = 0; r < nrefs && r < MAX_REFS; r++) {
                if (adrl_rewrite_target(data, refs[r], load_base, green_target)) {
                    printf("[force_green_adrl] repointed ADRL at 0x%X to green\n",
                           refs[r]);
                    patched++;
                }
            }
            off += (int32_t)strlen(states[s]) * 2 + 2;
        }
        off = 0;
        while ((off = find_ascii(data, size, states[s], off)) >= 0) {
            int32_t refs[MAX_REFS];
            int32_t nrefs = find_adrl_refs(data, size, load_base,
                                           (int64_t)off, refs, MAX_REFS);
            for (int r = 0; r < nrefs && r < MAX_REFS; r++) {
                if (adrl_rewrite_target(data, refs[r], load_base, green_target)) {
                    printf("[force_green_adrl] repointed ADRL at 0x%X to green\n",
                           refs[r]);
                    patched++;
                }
            }
            off += (int32_t)strlen(states[s]) + 1;
        }
    }
    return patched;
}

/* ================================================================
 * Patch 3: Short-circuit AVB verification entry
 * ================================================================ */
static int32_t patch_short_circuit_verify(uint8_t *data, int32_t size,
                                          uint64_t load_base)
{
    int32_t patched = 0;
    int32_t func_starts[16];
    int32_t nfuncs = 0;

    const uint8_t avb0[] = { 'A', 'V', 'B', '0' };
    int32_t off = 0;
    while ((off = find_mem(data, size, avb0, 4, off)) >= 0) {
        printf("[short_circuit] 'AVB0' at offset 0x%X\n", off);
        int32_t refs[MAX_REFS];
        int32_t nrefs = find_adrl_refs(data, size, load_base,
                                       (int64_t)off, refs, MAX_REFS);
        for (int r = 0; r < nrefs && r < MAX_REFS; r++) {
            int32_t fstart = arm64_find_function_start(
                data, size, refs[r], 4096);
            if (fstart >= 0) {
                bool dup = false;
                for (int i = 0; i < nfuncs; i++)
                    if (func_starts[i] == fstart) { dup = true; break; }
                if (!dup && nfuncs < 16) {
                    func_starts[nfuncs++] = fstart;
                    printf("  -> function at 0x%X (ref 0x%X)\n",
                           fstart, refs[r]);
                }
            }
        }
        off += 4;
    }

    /* Also try vbmeta / VerifiedBoot strings */
    const char *vb_strs[] = { "vbmeta", "VerifiedBoot", "androidboot.vbmeta" };
    for (int s = 0; s < 3; s++) {
        off = 0;
        while ((off = find_ascii(data, size, vb_strs[s], off)) >= 0) {
            int32_t refs[MAX_REFS];
            int32_t nrefs = find_adrl_refs(data, size, load_base,
                                           (int64_t)off, refs, MAX_REFS);
            for (int r = 0; r < nrefs && r < MAX_REFS; r++) {
                int32_t fstart = arm64_find_function_start(
                    data, size, refs[r], 4096);
                if (fstart >= 0) {
                    bool dup = false;
                    for (int i = 0; i < nfuncs; i++)
                        if (func_starts[i] == fstart) { dup = true; break; }
                    if (!dup && nfuncs < 16) {
                        func_starts[nfuncs++] = fstart;
                        printf("  -> '%s' ref 0x%X -> func 0x%X\n",
                               vb_strs[s], refs[r], fstart);
                    }
                }
            }
            off += (int32_t)strlen(vb_strs[s]) + 1;
        }
    }

    for (int i = 0; i < nfuncs; i++) {
        int32_t fstart = func_starts[i];
        if (fstart + 8 > size) continue;
        uint32_t orig0 = arm64_read(data, fstart);
        uint32_t orig1 = arm64_read(data, fstart + 4);
        arm64_write(data, fstart, arm64_mov_x0_zero());
        arm64_write(data, fstart + 4, arm64_ret());
        printf("[short_circuit] patched 0x%X: %08X %08X -> MOV X0,XZR ; RET\n",
               fstart, orig0, orig1);
        patched++;
    }
    if (nfuncs == 0) printf("[short_circuit] no verification functions found\n");
    return patched;
}

/* ================================================================
 * Patch 4: NOP branches to error/red state
 * ================================================================ */
static int32_t patch_nop_error_branches(uint8_t *data, int32_t size,
                                        uint64_t load_base)
{
    int32_t patched = 0;
    int32_t red_offs[16];
    int32_t nred = 0;
    int32_t off = 0;
    while ((off = find_utf16le(data, size, "red", off)) >= 0 && nred < 16) {
        red_offs[nred++] = off; off += 8;
    }
    off = 0;
    while ((off = find_ascii(data, size, "red", off)) >= 0 && nred < 16) {
        red_offs[nred++] = off; off += 4;
    }

    for (int ri = 0; ri < nred; ri++) {
        int32_t refs[MAX_REFS];
        int32_t nrefs = find_adrl_refs(data, size, load_base,
                                       (int64_t)red_offs[ri], refs, MAX_REFS);
        for (int r = 0; r < nrefs && r < MAX_REFS; r++) {
            for (int32_t scan = refs[r] - 4;
                 scan >= refs[r] - 256 && scan >= 0; scan -= 4) {
                uint32_t ins = arm64_read(data, scan);
                if (arm64_is_cbz(ins) || arm64_is_cbnz(ins) ||
                    arm64_is_b_cond(ins)) {
                    int32_t tgt = arm64_branch_target(scan, ins);
                    if (tgt >= refs[r] - 8 && tgt <= refs[r] + 32) {
                        printf("[nop_error] NOP branch at 0x%X -> 0x%X\n",
                               scan, tgt);
                        arm64_write(data, scan, arm64_nop());
                        patched++;
                        break;
                    }
                }
            }
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
    printf("Buffer: %d bytes (0x%X)\n", size, size);
    printf("Load base: 0x%llX\n", (unsigned long long)load_base);

    /* Detect PE/COFF and parse data section bounds */
    pe_info_t pe;
    if (parse_pe_sections(data, size, &pe) == 0) {
        printf("Format: PE/COFF (ARM64 EFI)\n");
        printf("Data section: 0x%X - 0x%X\n", pe.data_start, pe.data_end);
    } else {
        printf("Format: raw binary (ELF or flat)\n");
        pe.data_start = size * 2 / 3;
        pe.data_end = size;
        pe.is_pe = 0;
    }
    printf("\n");

    /* Count AVB0 */
    const uint8_t avb0[] = { 'A', 'V', 'B', '0' };
    int32_t off = 0;
    while ((off = find_mem(data, size, avb0, 4, off)) >= 0) {
        result->avb0_references++; off += 4;
    }
    printf("AVB0 occurrences: %d\n", result->avb0_references);
    if (find_ascii(data, size, "green", 0) >= 0 ||
        find_utf16le(data, size, "green", 0) >= 0) result->strings_found++;
    printf("Boot-state strings: %d\n\n", result->strings_found);

    /* Apply patches */
    result->verify_shortcircuited =
        patch_short_circuit_verify(data, size, load_base);
    printf("\n");

    /* Boot state array patching (primary for PE/COFF ABL) */
    result->green_forced = patch_boot_state_array(data, size, &pe);
    printf("\n");

    /* Fallback: ADRL-based green forcing */
    result->green_forced += patch_force_green_adrl(data, size, load_base);
    printf("\n");

    result->error_branches_noped =
        patch_nop_error_branches(data, size, load_base);
    printf("\n");

    printf("=== Patch Summary ===\n");
    printf("  Verify functions short-circuited: %d\n",
           result->verify_shortcircuited);
    printf("  Boot state forced to green: %d\n", result->green_forced);
    printf("  Error branches NOPed: %d\n", result->error_branches_noped);
    int32_t total = result->verify_shortcircuited +
                    result->green_forced +
                    result->error_branches_noped;
    printf("  Total patches: %d\n", total);
    return total > 0;
}
