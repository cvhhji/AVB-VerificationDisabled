* SPDX-License-Identifier: GPL-2.0-only
 *
 * Core logic for patching Qualcomm ABL (Android Bootloader) to disable
 * AVB verification at BL stage while preserving fake-relock (locked/green).
 *
 * Patches applied:
 *   1. Force verifiedbootstate=green  (never orange/red, hides unlock)
 *   2. Short-circuit AVB verification entry  (return EFI_SUCCESS)
 *   3. NOP branches to red/error state  (prevent boot failure screen)
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
