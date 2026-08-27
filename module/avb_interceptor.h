* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AVB_INTERCEPTOR_H
#define AVB_INTERCEPTOR_H
#include <linux/types.h>
#include <linux/module.h>

enum avb_interceptor_phase { AVB_PHASE_FIRST_STAGE = 0, AVB_PHASE_DRAINING, AVB_PHASE_DISABLED };
enum avb_interceptor_stop_reason { AVB_STOP_SYSTEM_INIT = 0, AVB_STOP_TIMEOUT };

enum avb_interceptor_phase avb_interceptor_phase_get(void);
void avb_interceptor_request_stop(enum avb_interceptor_stop_reason reason);

/*
 * Fake-relock green mode: when true, bootconfig_proxy does NOT inject
 * androidboot.verifiedbootstate="orange".  The ABL already reports
 * locked+green (via gbl_root_canoe fake-relock + BL-stage AVB patcher),
 * and injecting orange would expose the unlocked state to userspace.
 * vbmeta flags patching still applies so libfs_avb skips verification.
 */
bool avb_interceptor_keep_green(void);

#endif
