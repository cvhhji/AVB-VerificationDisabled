/* SPDX-License-Identifier: GPL-2.0-only */
#include "arm64_util.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(arm64_is_cbz(0x34000000));   /* CBZ W0, . */
    assert(arm64_is_cbz(0xB4000000));   /* CBZ X0, . */
    assert(!arm64_is_cbz(0x35000000));  /* CBNZ W0, . */
    assert(!arm64_is_cbz(0xB5000000));  /* CBNZ X0, . */

    assert(arm64_is_cbnz(0x35000000));
    assert(arm64_is_cbnz(0xB5000000));
    assert(!arm64_is_cbnz(0x34000000));
    assert(!arm64_is_cbnz(0xB4000000));

    /* ANDS WZR/XZR aliases used by TST; N may be either value on X form. */
    assert(arm64_is_tst_imm(0x7200001F));
    assert(arm64_is_tst_imm(0xF200001F));
    assert(arm64_is_tst_imm(0xF240001F));
    assert(!arm64_is_tst_imm(0xF2000000)); /* Rd is not ZR */
    assert(!arm64_is_tst_imm(0x9200001F)); /* AND, not ANDS */

    puts("arm64_util instruction classification: PASS");
    return 0;
}
