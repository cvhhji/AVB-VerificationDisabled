/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AVB_INTERCEPTOR_H
#define AVB_INTERCEPTOR_H
#include <linux/types.h>
enum avb_interceptor_phase { AVB_PHASE_FIRST_STAGE = 0, AVB_PHASE_DRAINING, AVB_PHASE_DISABLED };
enum avb_interceptor_stop_reason { AVB_STOP_SYSTEM_INIT = 0, AVB_STOP_TIMEOUT };
enum avb_interceptor_phase avb_interceptor_phase_get(void);
void avb_interceptor_request_stop(enum avb_interceptor_stop_reason reason);
#endif
