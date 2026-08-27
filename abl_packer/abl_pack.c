#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static inline uint16_t r16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t r32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static size_t calc_pe_size(const uint8_t *data, size_t len) {
    if (len < 0x40) return len;
    uint16_t pe_ptr = r16(data + 0x3C);
    if ((size_t)pe_ptr + 0x58 > len) return len;
    if (data[pe_ptr] != 'P' || data[pe_ptr + 1] != 'E') return len;
    uint16_t num_sec  = r16(data + pe_ptr + 0x06);
    uint16_t opt_size = r16(data + pe_ptr + 0x14);
    size_t sec_table  = (size_t)pe_ptr + 0x18 + opt_size;
    size_t real_len = (size_t)r32(data + pe_ptr + 0x54);
    for (int i = 0; i < num_sec; i++) {
        size_t sec_off = sec_table + (size_t)i * 0x28;
        if (sec_off + 0x28 > len) break;
        uint32_t size_raw = r32(data + sec_off + 0x10);
        uint32_t ptr_raw  = r32(data + sec_off + 0x14);
        size_t end = (size_t)ptr_raw + size_raw;
        if (end > real_len) real_len = end;
    }
    if (real_len > len) real_len = len;
    return real_len;
}

static int find_largest_pe(const uint8_t *data, size_t len,
                            size_t *out_offset, size_t *out_size) {
    size_t best_off = 0, best_size = 0;
    for (size_t off = 0; off + 0x40 < len; off++) {
        if (data[off] != 'M' || data[off + 1] != 'Z') continue;
        uint16_t pe_ptr = r16(data + off + 0x3C);
        if ((size_t)pe_ptr + 2 > len - off) continue;
        if (data[off + pe_ptr] != 'P' || data[off + pe_ptr + 1] != 'E') continue;
        size_t remain = len - off;
        size_t sz = calc_pe_size(data + off, remain);
        if (sz > best_size) {
            best_size = sz;
            best_off = off;
        }
    }
    if (best_size == 0) return -1;
    *out_offset = best_off;
    *out_size = best_size;
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <original_abl.img> <patched_LinuxLoader.efi> <output_abl.img>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    fseek(f, 0, SEEK_END);
    long img_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *img = (uint8_t *)malloc((size_t)img_size);
    if (!img) { fprintf(stderr, "malloc failed\n"); fclose(f); return 1; }
    if ((long)fread(img, 1, (size_t)img_size, f) != img_size) {
        fprintf(stderr, "read failed\n"); free(img); fclose(f); return 1;
    }
    fclose(f);

    size_t pe_off = 0, pe_size = 0;
    if (find_largest_pe(img, (size_t)img_size, &pe_off, &pe_size) != 0) {
        fprintf(stderr, "ERROR: No PE/EFI file found in abl.img\n");
        free(img);
        return 1;
    }
    printf("[*] Found LinuxLoader.efi at offset 0x%zX, size 0x%zX (%zu bytes)\n",
           pe_off, pe_size, pe_size);

    f = fopen(argv[2], "rb");
    if (!f) { perror(argv[2]); free(img); return 1; }
    fseek(f, 0, SEEK_END);
    long patched_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if ((size_t)patched_size > pe_size) {
        fprintf(stderr, "ERROR: Patched file (%ld bytes) larger than original (%zu bytes)\n",
                patched_size, pe_size);
        fclose(f); free(img); return 1;
    }
    uint8_t *patched = (uint8_t *)malloc((size_t)patched_size);
    if (!patched) { fprintf(stderr, "malloc failed\n"); fclose(f); free(img); return 1; }
    if ((long)fread(patched, 1, (size_t)patched_size, f) != patched_size) {
        fprintf(stderr, "read patched failed\n"); free(patched); free(img); fclose(f); return 1;
    }
    fclose(f);

    if ((size_t)patched_size < pe_size) {
        memset(img + pe_off + patched_size, 0, pe_size - (size_t)patched_size);
        printf("[*] Patched file smaller by %zu bytes, zero-filled gap\n",
               pe_size - (size_t)patched_size);
    }
    memcpy(img + pe_off, patched, (size_t)patched_size);

    f = fopen(argv[3], "wb");
    if (!f) { perror(argv[3]); free(patched); free(img); return 1; }
    if ((long)fwrite(img, 1, (size_t)img_size, f) != img_size) {
        fprintf(stderr, "write failed\n"); fclose(f); free(patched); free(img); return 1;
    }
    fclose(f);

    printf("[+] Wrote patched abl.img to %s (%ld bytes)\n", argv[3], img_size);
    free(patched);
    free(img);
    return 0;
}
