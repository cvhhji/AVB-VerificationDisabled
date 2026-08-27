/* SPDX-License-Identifier: GPL-2.0-only
 *
 * patch_abl_avb - Patch a Qualcomm ABL ELF to disable AVB verification
 * at BL stage while preserving fake-relock (locked/green state).
 *
 * Usage:
 *   patch_abl_avb <input_abl.elf> <output_abl.elf> [--load-base 0xADDR]
 *
 * The input should be an ABL ELF extracted from the abl partition firmware
 * volume (e.g. via gbl_root_canoe's extractfv). The patched output can be
 * repacked and flashed alongside the gbl_root_canoe fake-relock BDS.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "avb_patch.h"

static int read_file(const char *path, uint8_t **data, int32_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open %s\n", path);
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long len = ftell(f);
    if (len <= 0 || len > 64 * 1024 * 1024) {
        fprintf(stderr, "Error: invalid file size %ld\n", len);
        fclose(f);
        return -1;
    }
    rewind(f);
    uint8_t *buf = (uint8_t *)malloc((size_t)len);
    if (!buf) { fclose(f); return -1; }
    size_t rd = fread(buf, 1, (size_t)len, f);
    fclose(f);
    if (rd != (size_t)len) {
        fprintf(stderr, "Error: short read (%zu of %ld)\n", rd, len);
        free(buf);
        return -1;
    }
    *data = buf;
    *size = (int32_t)len;
    return 0;
}

static int write_file(const char *path, const uint8_t *data, int32_t size)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "Error: cannot create %s\n", path);
        return -1;
    }
    size_t wr = fwrite(data, 1, (size_t)size, f);
    fclose(f);
    if (wr != (size_t)size) {
        fprintf(stderr, "Error: short write (%zu of %d)\n", wr, size);
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    const char *input = NULL;
    const char *output = NULL;
    uint64_t load_base = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--load-base") == 0 && i + 1 < argc) {
            load_base = strtoull(argv[++i], NULL, 0);
        } else if (!input) {
            input = argv[i];
        } else if (!output) {
            output = argv[i];
        } else {
            fprintf(stderr, "Error: unexpected argument %s\n", argv[i]);
            return 1;
        }
    }

    if (!input || !output) {
        fprintf(stderr,
            "Usage: %s <input_abl.elf> <output_abl.elf> [--load-base 0xADDR]\n"
            "\n"
            "Patches a Qualcomm ABL ELF to disable AVB verification at BL\n"
            "stage while keeping fake-relock (locked/green) state intact.\n"
            "\n"
            "  --load-base  Runtime address of file offset 0 (default 0).\n"
            "               Use 0 for ABL ELF extracted from FV.\n",
            argv[0]);
        return 1;
    }

    uint8_t *data = NULL;
    int32_t size = 0;
    if (read_file(input, &data, &size) != 0) return 1;

    printf("Input:  %s (%d bytes)\n", input, size);
    printf("Output: %s\n\n", output);

    avb_patch_result_t result;
    bool ok = avb_patch_abl(data, size, load_base, &result);

    if (!ok) {
        fprintf(stderr,
            "\nWarning: no patches were applied. The ABL may use a different\n"
            "code layout than expected. Provide --load-base if the ELF is\n"
            "position-dependent, or check the log above for missing strings.\n");
        free(data);
        return 2;
    }

    if (write_file(output, data, size) != 0) {
        free(data);
        return 1;
    }

    printf("\nPatched ABL written to %s\n", output);
    free(data);
    return 0;
}
