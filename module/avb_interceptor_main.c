/ SPDX-License-Identifier: GPL-2.0-only
#include <linux/atomic.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/workqueue.h>
#include "avb_interceptor.h"
#include "bootconfig_proxy.h"
#include "exec_gate.h"
#include "vbmeta_proxy.h"
#define HOOK_TIMEOUT_SECONDS 120U
static atomic_t phase = ATOMIC_INIT(AVB_PHASE_FIRST_STAGE);
static atomic_t stop_reason = ATOMIC_INIT(AVB_STOP_TIMEOUT);
static struct work_struct stop_work;
static struct delayed_work timeout_work;

/*
 * Fake-relock green mode.  Set via module parameter "avb_keep_green=1"
 * (passed by avbinit when /avb_keep_green exists in ramdisk).  When true,
 * the bootconfig proxy skips the orange-state injection so userspace sees
 * verifiedbootstate=green (matching the fake-relock ABL).  vbmeta flags
 * patching is unaffected - libfs_avb still skips verification.
 */
static bool avb_keep_green = false;
module_param(avb_keep_green, bool, 0444);
MODULE_PARM_DESC(avb_keep_green,
	"Keep verifiedbootstate=green (fake-relock mode; skip orange injection)");

bool avb_interceptor_keep_green(void) { return READ_ONCE(avb_keep_green); }

enum avb_interceptor_phase avb_interceptor_phase_get(void) { return (enum avb_interceptor_phase)atomic_read(&phase); }
static void stop_hooks(struct work_struct *work)
{
 enum avb_interceptor_stop_reason reason; (void)work;
 if (atomic_read(&phase) == AVB_PHASE_DISABLED) return;
 reason = (enum avb_interceptor_stop_reason)atomic_read(&stop_reason);
 cancel_delayed_work(&timeout_work); exec_gate_unregister(); vbmeta_proxy_unregister(); bootconfig_proxy_unregister(); atomic_set(&phase, AVB_PHASE_DISABLED);
 pr_info("avb-interceptor: hooks removed (%s; vbmeta matches %llu, patches %llu, errors %llu; bootconfig matches %llu, injections %llu)\n", reason == AVB_STOP_SYSTEM_INIT ? "PID 1 entered /system/bin/init" : "120 second timeout", vbmeta_proxy_match_count(), vbmeta_proxy_patch_count(), vbmeta_proxy_error_count(), bootconfig_proxy_match_count(), bootconfig_proxy_injection_count());
}
void avb_interceptor_request_stop(enum avb_interceptor_stop_reason reason)
{
 if (atomic_cmpxchg(&phase, AVB_PHASE_FIRST_STAGE, AVB_PHASE_DRAINING) == AVB_PHASE_FIRST_STAGE) { atomic_set(&stop_reason, reason); schedule_work(&stop_work); }
}
static void on_timeout(struct work_struct *work) { (void)work; avb_interceptor_request_stop(AVB_STOP_TIMEOUT); }
static int __init avb_interceptor_init(void)
{
 int error; INIT_WORK(&stop_work, stop_hooks); INIT_DELAYED_WORK(&timeout_work, on_timeout);
 error = bootconfig_proxy_register(); if (error) return error;
 error = vbmeta_proxy_register(); if (error) { bootconfig_proxy_unregister(); return error; }
 error = exec_gate_register(); if (error) { vbmeta_proxy_unregister(); bootconfig_proxy_unregister(); return error; }
 schedule_delayed_work(&timeout_work, msecs_to_jiffies(HOOK_TIMEOUT_SECONDS * 1000U));
 pr_info("avb-interceptor: loaded for normal-system first-stage AVB window (keep_green=%s)\n",
	 avb_keep_green ? "on" : "off");
 return 0;
}
static void __exit avb_interceptor_exit(void)
{
 atomic_set(&phase, AVB_PHASE_DISABLED); cancel_delayed_work_sync(&timeout_work); cancel_work_sync(&stop_work); exec_gate_unregister(); cancel_work_sync(&stop_work); vbmeta_proxy_unregister(); bootconfig_proxy_unregister();
}
module_init(avb_interceptor_init); module_exit(avb_interceptor_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("cvhhji; extracted from DSU-Permissive by yangFenTuoZi");
MODULE_DESCRIPTION("Temporarily presents VerificationDisabled vbmeta to Android first-stage PID 1");
MODULE_INFO(avb_loader_path, "/avb_interceptor.ko");
MODULE_INFO(avb_ddk_target, AVB_DDK_TARGET);
MODULE_IMPORT_NS(ANDROID_GKI_VFS_EXPORT_ONLY);
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_VERSION("0.2.0");
