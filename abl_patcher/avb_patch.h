/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Core logic for routing Qualcomm ABL around AVB2 verification.
 *
 * Fake-relock/green handling is applied separately by gbl_root_canoe.
 */
#ifndef AVB_PATCH_H
#define AVB_PATCH_H

#include <stdint.h>
#include <stdbool.h>

/* Result counters returned to caller. */
typedef struct {
    int32_t green_forced;       /* ADRL pairs repointed to green */
    int32_t verify_shortcircuited; /* verification functions patched */
    int32_t error_branches_noped;  /* branches to red/error NOPed */
    int32_t avb0_references;    /* AVB0 magic references found */
    int32_t strings_found;      /* boot-state strings found */
} avb_patch_result_t;

/*
 * Patch an ABL ELF binary in-place.
 * load_base: runtime address corresponding to file offset 0 (usually 0 for
 *            position-dependent ABL ELF extracted from FV).
 * Returns true if at least one patch was applied.
 */
bool avb_patch_abl(uint8_t *data, int32_t size, uint64_t load_base,
                   avb_patch_result_t *result);

#endif /* AVB_PATCH_H */
