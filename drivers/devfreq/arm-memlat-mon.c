// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2018, 2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "arm-memlat-mon: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/cpu_pm.h>
#include <linux/cpu.h>
#include <linux/of_fdt.h>
#include "governor.h"
#include "governor_memlat.h"
#include <linux/perf_event.h>
#include <linux/of_device.h>
#include <soc/qcom/scm.h>

<<<<<<< Updated upstream
enum common_ev_idx {
=======
enum ev_index {
>>>>>>> Stashed changes
	INST_IDX,
	CM_IDX,
	CYC_IDX,
	STALL_CYC_IDX,
	NUM_EVENTS
};
#define INST_EV		0x08
#define L2DM_EV		0x17
#define CYC_EV		0x11

struct event_data {
	struct perf_event *pevent;
	unsigned long prev_count;
};

struct cpu_pmu_stats {
	struct event_data events[NUM_EVENTS];
	ktime_t prev_ts;
};

<<<<<<< Updated upstream
/**
 * struct memlat_mon - A specific consumer of cpu_grp generic counters.
 *
 * @is_active:			Whether or not this mon is currently running
 *				memlat.
 * @cpus:			CPUs this mon votes on behalf of. Must be a
 *				subset of @cpu_grp's CPUs. If no CPUs provided,
 *				defaults to using all of @cpu_grp's CPUs.
 * @miss_ev_id:			The event code corresponding to the @miss_ev
 *				perf event. Will be 0 for compute.
 * @access_ev_id:		The event code corresponding to the @access_ev
 *				perf event. Optional - only needed for writeback
 *				percent.
 * @wb_ev_id:			The event code corresponding to the @wb_ev perf
 *				event. Optional - only needed for writeback
 *				percent.
 * @miss_ev:			The cache miss perf event exclusive to this
 *				mon. Will be NULL for compute.
 * @access_ev:			The cache access perf event exclusive to this
 *				mon. Optional - only needed for writeback
 *				percent.
 * @wb_ev:			The cache writeback perf event exclusive to this
 *				mon. Optional - only needed for writeback
 *				percent.
 * @requested_update_ms:	The mon's desired polling rate. The lowest
 *				@requested_update_ms of all mons determines
 *				@cpu_grp's update_ms.
 * @hw:				The memlat_hwmon struct corresponding to this
 *				mon's specific memlat instance.
 * @cpu_grp:			The cpu_grp who owns this mon.
 */
struct memlat_mon {
	bool			is_active;
	cpumask_t		cpus;
	unsigned int		miss_ev_id;
	unsigned int		access_ev_id;
	unsigned int		wb_ev_id;
	unsigned int		requested_update_ms;
	struct event_data	*miss_ev;
	struct event_data	*access_ev;
	struct event_data	*wb_ev;
	struct memlat_hwmon	hw;

	struct memlat_cpu_grp	*cpu_grp;
};

/**
 * struct memlat_cpu_grp - A coordinator of both HW reads and devfreq updates
 * for one or more memlat_mons.
 *
 * @cpus:			The CPUs this cpu_grp will read events from.
 * @common_ev_ids:		The event codes of the events all mons need.
 * @cpus_data:			The cpus data array of length #cpus. Includes
 *				event_data of all the events all mons need as
 *				well as common computed cpu data like freq.
 * @last_update_ts:		Used to avoid redundant reads.
 * @last_ts_delta_us:		The time difference between the most recent
 *				update and the one before that. Used to compute
 *				effective frequency.
 * @work:			The delayed_work used for handling updates.
 * @update_ms:			The frequency with which @work triggers.
 * @num_mons:		The number of @mons for this cpu_grp.
 * @num_inited_mons:	The number of @mons who have probed.
 * @num_active_mons:	The number of @mons currently running
 *				memlat.
 * @mons:			All of the memlat_mon structs representing
 *				the different voters who share this cpu_grp.
 * @mons_lock:		A lock used to protect the @mons.
 */
struct memlat_cpu_grp {
	cpumask_t		cpus;
	unsigned int		common_ev_ids[NUM_COMMON_EVS];
	struct cpu_data		*cpus_data;
	ktime_t			last_update_ts;
	unsigned long		last_ts_delta_us;

	struct delayed_work	work;
	unsigned int		update_ms;

	unsigned int		num_mons;
	unsigned int		num_inited_mons;
	unsigned int		num_active_mons;
	struct memlat_mon	*mons;
	struct mutex		mons_lock;
=======
struct cpu_grp_info {
	cpumask_t cpus;
	unsigned long any_cpu_ev_mask;
	unsigned int event_ids[NUM_EVENTS];
	struct cpu_pmu_stats *cpustats;
	struct memlat_hwmon hw;
>>>>>>> Stashed changes
};

struct memlat_mon_spec {
	bool is_compute;
};

struct ipi_data {
	unsigned long cnts[NR_CPUS][NUM_EVENTS];
	struct task_struct *waiter_task;
	struct cpu_grp_info *cpu_grp;
	atomic_t cpus_left;
};

#define to_cpustats(cpu_grp, cpu) \
	(&cpu_grp->cpustats[cpu - cpumask_first(&cpu_grp->cpus)])
#define to_devstats(cpu_grp, cpu) \
	(&cpu_grp->hw.core_stats[cpu - cpumask_first(&cpu_grp->cpus)])
#define to_cpu_grp(hwmon) container_of(hwmon, struct cpu_grp_info, hw)


static unsigned long compute_freq(struct cpu_pmu_stats *cpustats,
						unsigned long cyc_cnt)
{
	ktime_t ts;
	unsigned int diff;
	uint64_t freq = 0;

	ts = ktime_get();
	diff = ktime_to_us(ktime_sub(ts, cpustats->prev_ts));
	if (!diff)
		diff = 1;
	cpustats->prev_ts = ts;
	freq = cyc_cnt;
	do_div(freq, diff);

	return freq;
}

#define MAX_COUNT_LIM 0xFFFFFFFFFFFFFFFF
static unsigned long read_event(struct cpu_pmu_stats *cpustats, int event_id)
{
	struct event_data *event = &cpustats->events[event_id];
	unsigned long ev_count;
	u64 total;

	if (!event->pevent || perf_event_read_local(event->pevent, &total, NULL, NULL))
		return 0;

	ev_count = total - event->prev_count;
	event->prev_count = total;
	return ev_count;
}

static void read_perf_counters(struct ipi_data *ipd, int cpu)
{
	struct cpu_grp_info *cpu_grp = ipd->cpu_grp;
	struct cpu_pmu_stats *cpustats = to_cpustats(cpu_grp, cpu);
	int ev;

	for (ev = 0; ev < NUM_EVENTS; ev++) {
		if (!(cpu_grp->any_cpu_ev_mask & BIT(ev)))
			ipd->cnts[cpu][ev] = read_event(cpustats, ev);
	}
}

static void read_evs_ipi(void *info)
{
	int cpu = raw_smp_processor_id();
	struct ipi_data *ipd = info;
	struct task_struct *waiter;

	read_perf_counters(ipd, cpu);

<<<<<<< Updated upstream
		for_each_cpu(cpu, &mon->cpus) {
			unsigned int mon_idx =
				cpu - cpumask_first(&mon->cpus);
			read_event(&mon->miss_ev[mon_idx]);

			if (mon->wb_ev_id && mon->access_ev_id) {
				read_event(&mon->wb_ev[mon_idx]);
				read_event(&mon->access_ev[mon_idx]);
			}
		}
=======
	/*
	 * Wake up the waiter task if we're the final CPU. The ipi_data pointer
	 * isn't safe to dereference once cpus_left reaches zero, so the waiter
	 * task_struct pointer must be cached before that. Also defend against
	 * the extremely unlikely possibility that the waiter task will have
	 * exited by the time wake_up_process() is reached.
	 */
	waiter = ipd->waiter_task;
	get_task_struct(waiter);
	if (atomic_fetch_andnot(BIT(cpu), &ipd->cpus_left) == BIT(cpu) &&
	    waiter->state != TASK_RUNNING)
		wake_up_process(waiter);
	put_task_struct(waiter);
}

static void read_any_cpu_events(struct ipi_data *ipd, unsigned long cpus)
{
	struct cpu_grp_info *cpu_grp = ipd->cpu_grp;
	int cpu, ev;

	if (!cpu_grp->any_cpu_ev_mask)
		return;

	for_each_cpu(cpu, to_cpumask(&cpus)) {
		struct cpu_pmu_stats *cpustats = to_cpustats(cpu_grp, cpu);

		for_each_set_bit(ev, &cpu_grp->any_cpu_ev_mask, NUM_EVENTS)
			ipd->cnts[cpu][ev] = read_event(cpustats, ev);
	}
}

static void compute_perf_counters(struct ipi_data *ipd, int cpu)
{
	struct cpu_grp_info *cpu_grp = ipd->cpu_grp;
	struct cpu_pmu_stats *cpustats = to_cpustats(cpu_grp, cpu);
	struct dev_stats *devstats = to_devstats(cpu_grp, cpu);
	unsigned long cyc_cnt, stall_cnt;

	devstats->inst_count = ipd->cnts[cpu][INST_IDX];
	devstats->mem_count = ipd->cnts[cpu][CM_IDX];
	cyc_cnt = ipd->cnts[cpu][CYC_IDX];
	devstats->freq = compute_freq(cpustats, cyc_cnt);
	if (cpustats->events[STALL_CYC_IDX].pevent) {
		stall_cnt = ipd->cnts[cpu][STALL_CYC_IDX];
		stall_cnt = min(stall_cnt, cyc_cnt);
		devstats->stall_pct = mult_frac(100, stall_cnt, cyc_cnt);
	} else {
		devstats->stall_pct = 100;
>>>>>>> Stashed changes
	}
}

static unsigned long get_cnt(struct memlat_hwmon *hw)
{
	struct cpu_grp_info *cpu_grp = to_cpu_grp(hw);
	unsigned long cpus_read_mask, tmp_mask;
	call_single_data_t csd[NR_CPUS];
	struct ipi_data ipd;
	int cpu, this_cpu;

	ipd.waiter_task = current;
	ipd.cpu_grp = cpu_grp;

<<<<<<< Updated upstream
		devstats->freq = cpu_data->freq;
		devstats->stall_pct = cpu_data->stall_pct;
		devstats->inst_count = common_evs[INST_IDX].last_delta;

		if (mon->miss_ev)
			devstats->mem_count =
				mon->miss_ev[mon_idx].last_delta;
		else {
			devstats->inst_count = 0;
			devstats->mem_count = 1;
		}

		if (mon->access_ev_id && mon->wb_ev_id)
			devstats->wb_pct =
				mult_frac(100, mon->wb_ev[mon_idx].last_delta,
					  mon->access_ev[mon_idx].last_delta);
		else
			devstats->wb_pct = 0;
=======
	/* Dispatch asynchronous IPIs to each CPU to read the perf events */
	cpus_read_lock();
	migrate_disable();
	this_cpu = raw_smp_processor_id();
	cpus_read_mask = *cpumask_bits(&cpu_grp->cpus);
	tmp_mask = cpus_read_mask & ~BIT(this_cpu);
	ipd.cpus_left = (atomic_t)ATOMIC_INIT(tmp_mask);
	for_each_cpu(cpu, to_cpumask(&tmp_mask)) {
		/*
		 * Some SCM calls take very long (20+ ms), so the IPI could lag
		 * on the CPU running the SCM call. Skip offline CPUs too.
		 */
		csd[cpu].flags = 0;
		if (under_scm_call(cpu) ||
		    generic_exec_single(cpu, &csd[cpu], read_evs_ipi, &ipd))
			cpus_read_mask &= ~BIT(cpu);
>>>>>>> Stashed changes
	}
	cpus_read_unlock();
	/* Read this CPU's events while the IPIs run */
	if (cpus_read_mask & BIT(this_cpu))
		read_perf_counters(&ipd, this_cpu);
	migrate_enable();

	/* Bail out if there weren't any CPUs available */
	if (!cpus_read_mask)
		return 0;

	/* Read any any-CPU events while the IPIs run */
	read_any_cpu_events(&ipd, cpus_read_mask);

	/* Clear out CPUs which were skipped */
	atomic_andnot(cpus_read_mask ^ tmp_mask, &ipd.cpus_left);

	/*
	 * Wait until all the IPIs are done reading their events, and compute
	 * each finished CPU's results while waiting since some CPUs may finish
	 * reading their events faster than others.
	 */
	for (tmp_mask = cpus_read_mask;;) {
		unsigned long cpus_done, cpus_left;

		set_current_state(TASK_UNINTERRUPTIBLE);
		cpus_left = (unsigned int)atomic_read(&ipd.cpus_left);
		if ((cpus_done = cpus_left ^ tmp_mask)) {
			for_each_cpu(cpu, to_cpumask(&cpus_done))
				compute_perf_counters(&ipd, cpu);
			if (!cpus_left)
				break;
			tmp_mask = cpus_left;
		} else {
			schedule();
		}
	}
	__set_current_state(TASK_RUNNING);

	return 0;
}

static void delete_events(struct cpu_pmu_stats *cpustats)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cpustats->events); i++) {
		cpustats->events[i].prev_count = 0;
		if (cpustats->events[i].pevent) {
			perf_event_release_kernel(cpustats->events[i].pevent);
			cpustats->events[i].pevent = NULL;
		}
	}
}

static void stop_hwmon(struct memlat_hwmon *hw)
{
	int cpu;
	struct cpu_grp_info *cpu_grp = to_cpu_grp(hw);
	struct dev_stats *devstats;

	for_each_cpu(cpu, &cpu_grp->cpus) {
		delete_events(to_cpustats(cpu_grp, cpu));

		/* Clear governor data */
		devstats = to_devstats(cpu_grp, cpu);
		devstats->inst_count = 0;
		devstats->mem_count = 0;
		devstats->freq = 0;
		devstats->stall_pct = 0;
	}
}

static struct perf_event_attr *alloc_attr(void)
{
	struct perf_event_attr *attr;

	attr = kzalloc(sizeof(struct perf_event_attr), GFP_KERNEL);
	if (!attr)
		return attr;

	attr->type = PERF_TYPE_RAW;
	attr->size = sizeof(struct perf_event_attr);
	attr->pinned = 1;
	attr->exclude_idle = 1;

	return attr;
}

static int set_events(struct cpu_grp_info *cpu_grp, int cpu)
{
	struct perf_event *pevent;
	struct perf_event_attr *attr;
	int err, i;
	unsigned int event_id;
	struct cpu_pmu_stats *cpustats = to_cpustats(cpu_grp, cpu);

	/* Allocate an attribute for event initialization */
	attr = alloc_attr();
	if (!attr)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(cpustats->events); i++) {
		event_id = cpu_grp->event_ids[i];
		if (!event_id)
			continue;

		attr->config = event_id;
		pevent = perf_event_create_kernel_counter(attr, cpu, NULL,
							  NULL, NULL);
		if (IS_ERR(pevent))
			goto err_out;
		cpustats->events[i].pevent = pevent;
		perf_event_enable(pevent);
		if (cpumask_equal(&pevent->readable_on_cpus, &CPU_MASK_ALL))
			cpu_grp->any_cpu_ev_mask |= BIT(i);
	}

	kfree(attr);
	return 0;

err_out:
	err = PTR_ERR(pevent);
	kfree(attr);
	return err;
}

static int start_hwmon(struct memlat_hwmon *hw)
{
	int cpu, ret = 0;
	struct cpu_grp_info *cpu_grp = to_cpu_grp(hw);

<<<<<<< Updated upstream
	if (!attr)
		return -ENOMEM;

	mutex_lock(&cpu_grp->mons_lock);
	should_init_cpu_grp = !(cpu_grp->num_active_mons++);
	if (should_init_cpu_grp) {
		ret = init_common_evs(cpu_grp, attr);
		if (ret)
			goto unlock_out;

		INIT_DEFERRABLE_WORK(&cpu_grp->work, &memlat_monitor_work);
	}

	if (mon->miss_ev) {
		for_each_cpu(cpu, &mon->cpus) {
			unsigned int idx = cpu - cpumask_first(&mon->cpus);

			ret = set_event(&mon->miss_ev[idx], cpu,
					mon->miss_ev_id, attr);
			if (ret)
				goto unlock_out;

			if (mon->access_ev_id && mon->wb_ev_id) {
				ret = set_event(&mon->access_ev[idx], cpu,
						mon->access_ev_id, attr);
				if (ret)
					goto unlock_out;

				ret = set_event(&mon->wb_ev[idx], cpu,
						mon->wb_ev_id, attr);
				if (ret)
					goto unlock_out;
			}
=======
	for_each_cpu(cpu, &cpu_grp->cpus) {
		ret = set_events(cpu_grp, cpu);
		if (ret) {
			pr_warn("Perf event init failed on CPU%d\n", cpu);
			break;
>>>>>>> Stashed changes
		}
	}

	return ret;
}

<<<<<<< Updated upstream
static void stop_hwmon(struct memlat_hwmon *hw)
{
	unsigned int cpu;
	struct memlat_mon *mon = to_mon(hw);
	struct memlat_cpu_grp *cpu_grp = mon->cpu_grp;

	mutex_lock(&cpu_grp->mons_lock);
	mon->is_active = false;
	cpu_grp->num_active_mons--;

	for_each_cpu(cpu, &mon->cpus) {
		unsigned int idx = cpu - cpumask_first(&mon->cpus);
		struct dev_stats *devstats = to_devstats(mon, cpu);

		if (mon->miss_ev)
			delete_event(&mon->miss_ev[idx]);
		devstats->inst_count = 0;
		devstats->mem_count = 0;
		devstats->freq = 0;
		devstats->stall_pct = 0;
		devstats->wb_pct = 0;
	}

	if (!cpu_grp->num_active_mons) {
		cancel_delayed_work(&cpu_grp->work);
		free_common_evs(cpu_grp);
	}
	mutex_unlock(&cpu_grp->mons_lock);
}

/**
 * We should set update_ms to the lowest requested_update_ms of all of the
 * active mons, or 0 (i.e. stop polling) if ALL active mons have 0.
 * This is expected to be called with cpu_grp->mons_lock taken.
 */
static void set_update_ms(struct memlat_cpu_grp *cpu_grp)
{
	struct memlat_mon *mon;
	unsigned int i, new_update_ms = UINT_MAX;

	for (i = 0; i < cpu_grp->num_mons; i++) {
		mon = &cpu_grp->mons[i];
		if (mon->is_active && mon->requested_update_ms)
			new_update_ms =
				min(new_update_ms, mon->requested_update_ms);
	}

	if (new_update_ms == UINT_MAX) {
		cancel_delayed_work(&cpu_grp->work);
	} else if (cpu_grp->update_ms == UINT_MAX) {
		queue_delayed_work(memlat_wq, &cpu_grp->work,
				   msecs_to_jiffies(new_update_ms));
	} else if (new_update_ms > cpu_grp->update_ms) {
		cancel_delayed_work(&cpu_grp->work);
		queue_delayed_work(memlat_wq, &cpu_grp->work,
				   msecs_to_jiffies(new_update_ms));
	}

	cpu_grp->update_ms = new_update_ms;
}

static void request_update_ms(struct memlat_hwmon *hw, unsigned int update_ms)
{
	struct devfreq *df = hw->df;
	struct memlat_mon *mon = to_mon(hw);
	struct memlat_cpu_grp *cpu_grp = mon->cpu_grp;

	mutex_lock(&df->lock);
	df->profile->polling_ms = update_ms;
	mutex_unlock(&df->lock);

	mutex_lock(&cpu_grp->mons_lock);
	mon->requested_update_ms = update_ms;
	set_update_ms(cpu_grp);
	mutex_unlock(&cpu_grp->mons_lock);
}

=======
>>>>>>> Stashed changes
static int get_mask_from_dev_handle(struct platform_device *pdev,
					cpumask_t *mask)
{
	struct device *dev = &pdev->dev;
	struct device_node *dev_phandle;
	struct device *cpu_dev;
	int cpu, i = 0;
	int ret = -ENOENT;

	dev_phandle = of_parse_phandle(dev->of_node, "qcom,cpulist", i++);
	while (dev_phandle) {
		for_each_possible_cpu(cpu) {
			cpu_dev = get_cpu_device(cpu);
			if (cpu_dev && cpu_dev->of_node == dev_phandle) {
				cpumask_set_cpu(cpu, mask);
				ret = 0;
				break;
			}
		}
		dev_phandle = of_parse_phandle(dev->of_node,
						"qcom,cpulist", i++);
	}

	return ret;
}

static struct device_node *parse_child_nodes(struct device *dev)
{
	struct device_node *of_child;
	int ddr_type_of = -1;
	int ddr_type = of_fdt_get_ddrtype();
	int ret;

	for_each_child_of_node(dev->of_node, of_child) {
		ret = of_property_read_u32(of_child, "qcom,ddr-type",
							&ddr_type_of);
		if (!ret && (ddr_type == ddr_type_of)) {
			dev_dbg(dev,
				"ddr-type = %d, is matching DT entry\n",
				ddr_type_of);
			return of_child;
		}
	}
	return NULL;
}

static int arm_memlat_mon_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct memlat_hwmon *hw;
	struct cpu_grp_info *cpu_grp;
	const struct memlat_mon_spec *spec;
	int cpu, ret;
	u32 event_id;

	cpu_grp = devm_kzalloc(dev, sizeof(*cpu_grp), GFP_KERNEL);
	if (!cpu_grp)
		return -ENOMEM;
	hw = &cpu_grp->hw;

	hw->dev = dev;
	hw->of_node = of_parse_phandle(dev->of_node, "qcom,target-dev", 0);
	if (!hw->of_node) {
		dev_err(dev, "Couldn't find a target device\n");
		return -ENODEV;
	}

	if (get_mask_from_dev_handle(pdev, &cpu_grp->cpus)) {
		dev_err(dev, "CPU list is empty\n");
		return -ENODEV;
	}

	hw->num_cores = cpumask_weight(&cpu_grp->cpus);
	hw->core_stats = devm_kzalloc(dev, hw->num_cores *
				sizeof(*(hw->core_stats)), GFP_KERNEL);
	if (!hw->core_stats)
		return -ENOMEM;

	cpu_grp->cpustats = devm_kzalloc(dev, hw->num_cores *
			sizeof(*(cpu_grp->cpustats)), GFP_KERNEL);
	if (!cpu_grp->cpustats)
		return -ENOMEM;

	cpu_grp->event_ids[CYC_IDX] = CYC_EV;

	for_each_cpu(cpu, &cpu_grp->cpus)
		to_devstats(cpu_grp, cpu)->id = cpu;

	hw->start_hwmon = &start_hwmon;
	hw->stop_hwmon = &stop_hwmon;
	hw->get_cnt = &get_cnt;
	if (of_get_child_count(dev->of_node))
		hw->get_child_of_node = &parse_child_nodes;

	spec = of_device_get_match_data(dev);
	if (spec && spec->is_compute) {
		ret = register_compute(dev, hw);
		if (ret)
			pr_err("Compute Gov registration failed\n");

		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "qcom,cachemiss-ev",
				   &event_id);
	if (ret) {
		dev_dbg(dev, "Cache Miss event not specified. Using def:0x%x\n",
			L2DM_EV);
		event_id = L2DM_EV;
	}
	cpu_grp->event_ids[CM_IDX] = event_id;

	ret = of_property_read_u32(dev->of_node, "qcom,inst-ev", &event_id);
	if (ret) {
		dev_dbg(dev, "Inst event not specified. Using def:0x%x\n",
			INST_EV);
		event_id = INST_EV;
	}
	cpu_grp->event_ids[INST_IDX] = event_id;

	ret = of_property_read_u32(dev->of_node, "qcom,stall-cycle-ev",
				   &event_id);
	if (ret)
		dev_dbg(dev, "Stall cycle event not specified. Event ignored.\n");
	else
		cpu_grp->event_ids[STALL_CYC_IDX] = event_id;

	ret = register_memlat(dev, hw);
	if (ret)
		pr_err("Mem Latency Gov registration failed\n");

<<<<<<< Updated upstream
	mutex_init(&cpu_grp->mons_lock);
	cpu_grp->update_ms = DEFAULT_UPDATE_MS;

	dev_set_drvdata(dev, cpu_grp);

	return 0;
}

static int memlat_mon_probe(struct platform_device *pdev, bool is_compute)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	struct memlat_cpu_grp *cpu_grp;
	struct memlat_mon *mon;
	struct memlat_hwmon *hw;
	unsigned int event_id, num_cpus, cpu;

	if (!memlat_wq)
		memlat_wq = create_freezable_workqueue("memlat_wq");

	if (!memlat_wq) {
		dev_err(dev, "Couldn't create memlat workqueue.\n");
		return -ENOMEM;
	}

	cpu_grp = dev_get_drvdata(dev->parent);
	if (!cpu_grp) {
		dev_err(dev, "Mon initialized without cpu_grp.\n");
		return -ENODEV;
	}

	mutex_lock(&cpu_grp->mons_lock);
	mon = &cpu_grp->mons[cpu_grp->num_inited_mons];
	mon->is_active = false;
	mon->requested_update_ms = 0;
	mon->cpu_grp = cpu_grp;

	if (get_mask_from_dev_handle(pdev, &mon->cpus)) {
		cpumask_copy(&mon->cpus, &cpu_grp->cpus);
	} else {
		if (!cpumask_subset(&mon->cpus, &cpu_grp->cpus)) {
			dev_err(dev,
				"Mon CPUs must be a subset of cpu_grp CPUs. mon=%*pbl cpu_grp=%*pbl\n",
				mon->cpus, cpu_grp->cpus);
			ret = -EINVAL;
			goto unlock_out;
		}
	}

	num_cpus = cpumask_weight(&mon->cpus);

	hw = &mon->hw;
	hw->of_node = of_parse_phandle(dev->of_node, "qcom,target-dev", 0);
	if (!hw->of_node) {
		dev_err(dev, "Couldn't find a target device.\n");
		ret = -ENODEV;
		goto unlock_out;
	}
	hw->dev = dev;
	hw->num_cores = num_cpus;
	hw->should_ignore_df_monitor = true;
	hw->core_stats = devm_kzalloc(dev, num_cpus * sizeof(*(hw->core_stats)),
				      GFP_KERNEL);
	if (!hw->core_stats) {
		ret = -ENOMEM;
		goto unlock_out;
	}

	for_each_cpu(cpu, &mon->cpus)
		to_devstats(mon, cpu)->id = cpu;

	hw->start_hwmon = &start_hwmon;
	hw->stop_hwmon = &stop_hwmon;
	hw->get_cnt = &get_cnt;
	if (of_get_child_count(dev->of_node))
		hw->get_child_of_node = &parse_child_nodes;
	hw->request_update_ms = &request_update_ms;

	/*
	 * Compute mons rely solely on common events.
	 */
	if (is_compute) {
		mon->miss_ev_id = 0;
		mon->access_ev_id = 0;
		mon->wb_ev_id = 0;
		ret = register_compute(dev, hw);
	} else {
		mon->miss_ev =
			devm_kzalloc(dev, num_cpus * sizeof(*mon->miss_ev),
				     GFP_KERNEL);
		if (!mon->miss_ev) {
			ret = -ENOMEM;
			goto unlock_out;
		}

		ret = of_property_read_u32(dev->of_node, "qcom,cachemiss-ev",
						&event_id);
		if (ret) {
			dev_err(dev, "Cache miss event missing for mon: %d\n",
					ret);
			ret = -EINVAL;
			goto unlock_out;
		}
		mon->miss_ev_id = event_id;

		ret = of_property_read_u32(dev->of_node, "qcom,access-ev",
					   &event_id);
		if (ret)
			dev_dbg(dev, "Access event not specified. Skipping.\n");
		else
			mon->access_ev_id = event_id;

		ret = of_property_read_u32(dev->of_node, "qcom,wb-ev",
					   &event_id);
		if (ret)
			dev_dbg(dev, "WB event not specified. Skipping.\n");
		else
			mon->wb_ev_id = event_id;

		if (mon->wb_ev_id && mon->access_ev_id) {
			mon->access_ev =
				devm_kzalloc(dev, num_cpus *
					     sizeof(*mon->access_ev),
					     GFP_KERNEL);
			if (!mon->access_ev) {
				ret = -ENOMEM;
				goto unlock_out;
			}

			mon->wb_ev =
				devm_kzalloc(dev, num_cpus *
					     sizeof(*mon->wb_ev), GFP_KERNEL);
			if (!mon->wb_ev) {
				ret = -ENOMEM;
				goto unlock_out;
			}
		}

		ret = register_memlat(dev, hw);
	}

	if (!ret)
		cpu_grp->num_inited_mons++;

unlock_out:
	mutex_unlock(&cpu_grp->mons_lock);
=======
>>>>>>> Stashed changes
	return ret;
}

static const struct memlat_mon_spec spec[] = {
	[0] = { false },
	[1] = { true },
};

static const struct of_device_id memlat_match_table[] = {
	{ .compatible = "qcom,arm-memlat-mon", .data = &spec[0] },
	{ .compatible = "qcom,arm-cpu-mon", .data = &spec[1] },
	{}
};

static struct platform_driver arm_memlat_mon_driver = {
	.probe = arm_memlat_mon_driver_probe,
	.driver = {
		.name = "arm-memlat-mon",
		.of_match_table = memlat_match_table,
		.suppress_bind_attrs = true,
	},
};

module_platform_driver(arm_memlat_mon_driver);
