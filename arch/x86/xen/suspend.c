// SPDX-License-Identifier: GPL-2.0
#include <linux/types.h>
#include <linux/tick.h>
#include <linux/percpu-defs.h>

#include <xen/xen.h>
#include <xen/interface/xen.h>
#include <xen/grant_table.h>
#include <xen/events.h>
#include <xen/hvc-console.h>

#include <asm/cpufeatures.h>
#include <asm/msr-index.h>
#include <asm/xen/hypercall.h>
#include <asm/xen/page.h>
#include <asm/fixmap.h>
#include <asm/msr.h>

#include "xen-ops.h"

static DEFINE_PER_CPU(u64, spec_ctrl);

static void xen_vcpu_notify_restore(void *data)
{
	if (xen_pv_domain() && boot_cpu_has(X86_FEATURE_SPEC_CTRL))
		wrmsrq(MSR_IA32_SPEC_CTRL, this_cpu_read(spec_ctrl));

	/* Boot processor notified via generic timekeeping_resume() */
	if (smp_processor_id() == 0)
		return;

	tick_resume_local();
}

static void xen_vcpu_notify_suspend(void *data)
{
	u64 tmp;

	tick_suspend_local();

	if (xen_pv_domain() && boot_cpu_has(X86_FEATURE_SPEC_CTRL)) {
		rdmsrq(MSR_IA32_SPEC_CTRL, tmp);
		this_cpu_write(spec_ctrl, tmp);
		wrmsrq(MSR_IA32_SPEC_CTRL, 0);
	}
}

static RAW_NOTIFIER_HEAD(xen_resume_notifier);

void xen_resume_notifier_register(struct notifier_block *nb)
{
	raw_notifier_chain_register(&xen_resume_notifier, nb);
}
EXPORT_SYMBOL_GPL(xen_resume_notifier_register);

void xen_arch_resume(void)
{
	int cpu;

	/* Resume console as early as possible. */
	//if (!si.cancelled)
		xen_console_resume();

	raw_notifier_call_chain(&xen_resume_notifier, 0, NULL);

	on_each_cpu(xen_vcpu_notify_restore, NULL, 1);

	for_each_online_cpu(cpu)
		xen_pmu_init(cpu);
}

int xen_arch_suspend(void)
{
	int cpu;

	for_each_online_cpu(cpu)
		xen_pmu_finish(cpu);

	on_each_cpu(xen_vcpu_notify_suspend, NULL, 1);

	return 0;
}
