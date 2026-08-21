// SPDX-License-Identifier: GPL-2.0-only
#include <linux/binfmts.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/tracepoint.h>
#include "avb_interceptor.h"
#include "exec_gate.h"
#define SYSTEM_INIT_PATH "/system/bin/init"
static struct tracepoint *sched_exec_tracepoint;
static bool tracepoint_registered;
static void find_sched_exec_tracepoint(struct tracepoint *tp, void *private)
{
 struct tracepoint **result = private;
 if (!strcmp(tp->name, "sched_process_exec")) *result = tp;
}
static void on_sched_process_exec(void *unused, struct task_struct *task, pid_t old_pid, struct linux_binprm *bprm)
{
 (void)unused; (void)old_pid;
 if (task->pid == 1 && bprm && bprm->filename && !strcmp(bprm->filename, SYSTEM_INIT_PATH)) avb_interceptor_request_stop(AVB_STOP_SYSTEM_INIT);
}
int exec_gate_register(void)
{
 int error;
 if (tracepoint_registered) return 0;
 for_each_kernel_tracepoint(find_sched_exec_tracepoint, &sched_exec_tracepoint);
 if (!sched_exec_tracepoint) return -ENOENT;
 error = tracepoint_probe_register(sched_exec_tracepoint, (void *)on_sched_process_exec, NULL);
 if (error) { sched_exec_tracepoint = NULL; return error; }
 tracepoint_registered = true; return 0;
}
void exec_gate_unregister(void)
{
 if (!tracepoint_registered) return;
 tracepoint_probe_unregister(sched_exec_tracepoint, (void *)on_sched_process_exec, NULL);
 tracepoint_registered = false; sched_exec_tracepoint = NULL;
}
