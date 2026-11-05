// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/fpsimd.h>
#include <asm/neon.h>
#include <linux/hashtable.h>
#include <linux/slab.h>

#include "is-hw.h"
#include "is-config.h"

#if defined(ENABLE_FPSIMD_FOR_USER)
#if defined(LOCAL_FPSIMD_API)
#define NUM_OF_MAX_CORE		8
/* func -> isr, plus spares for a DDK that re-enables IRQs inside a call */
#define IS_FPSIMD_MAX_DEPTH	4

struct is_fpsimd_stack {
	struct is_fpsimd_state	state[IS_FPSIMD_MAX_DEPTH];
	unsigned long		irq_flags[IS_FPSIMD_MAX_DEPTH];
	bool			saved[IS_FPSIMD_MAX_DEPTH];
	bool			func[IS_FPSIMD_MAX_DEPTH];
	int			depth;
};

static struct is_fpsimd_stack fpsimd_stack[NUM_OF_MAX_CORE];

/* Outside IRQ context the user state goes to the task, so a DDK that sleeps cannot leak its registers */
static bool is_fpsimd_flush(void)
{
	if (in_interrupt() || irqs_disabled())
		return false;

	local_bh_disable();
	fpsimd_save_and_flush_cpu_state();
	local_bh_enable();
	return true;
}

static int is_fpsimd_push(bool flushed, bool func)
{
	struct is_fpsimd_stack *s = &fpsimd_stack[smp_processor_id()];
	int idx = s->depth;

	/*
	 * Always count the frame, even when it does not fit: the matching put
	 * will pop it, and an unaccounted frame would make that pop eat the
	 * caller's frame instead.
	 */
	s->depth = idx + 1;
	barrier();

	if (WARN_ONCE(idx >= IS_FPSIMD_MAX_DEPTH, "is_fpsimd: depth %d overflow\n", idx))
		return -1;

	s->saved[idx] = !flushed;
	s->func[idx] = func;
	if (!flushed)
		is_fpsimd_save_state(&s->state[idx]);

	return idx;
}

static void is_fpsimd_pop(void)
{
	struct is_fpsimd_stack *s = &fpsimd_stack[smp_processor_id()];
	int idx = s->depth - 1;

	if (WARN_ONCE(idx < 0, "is_fpsimd: unbalanced put\n"))
		return;

	/* frames past the array (overflow) have nothing saved */
	if (idx < IS_FPSIMD_MAX_DEPTH && s->saved[idx])
		is_fpsimd_load_state(&s->state[idx]);
	barrier();
	s->depth = idx;
}

void is_fpsimd_get_isr(void)
{
	is_fpsimd_push(false, false);
}

void is_fpsimd_put_isr(void)
{
	is_fpsimd_pop();
}

struct is_fpsimd_func_ctx {
	struct hlist_node	node;
	struct task_struct	*task;
	int			depth;
	struct is_fpsimd_task	t;
};

static DEFINE_HASHTABLE(fpsimd_func_ctx, 4);
static DEFINE_SPINLOCK(fpsimd_func_lock);

static void is_fpsimd_notifier_on(void);

static struct is_fpsimd_func_ctx *is_fpsimd_func_ctx_find(void)
{
	struct is_fpsimd_func_ctx *ctx;

	hash_for_each_possible(fpsimd_func_ctx, ctx, node, (unsigned long)current)
		if (ctx->task == current)
			return ctx;

	return NULL;
}

/* A13 DDK writes the sensor over i2c from inside a shot, so a call from a sleepable context must stay sleepable */
static bool is_fpsimd_get_func_task(void)
{
	struct is_fpsimd_func_ctx *ctx;
	unsigned long flags;

	spin_lock_irqsave(&fpsimd_func_lock, flags);
	ctx = is_fpsimd_func_ctx_find();
	if (ctx)
		ctx->depth++;
	spin_unlock_irqrestore(&fpsimd_func_lock, flags);
	if (ctx)
		return true;

	/*
	 * Do not fall back to the atomic path on failure: the caller is here
	 * because the DDK may sleep, and a sleep with IRQs/preemption off is
	 * worse than waiting for a small allocation.
	 */
	ctx = kmalloc(sizeof(*ctx), GFP_KERNEL | __GFP_NOFAIL);
	if (!ctx)
		return false;

	ctx->task = current;
	ctx->depth = 1;
	is_fpsimd_get_task(&ctx->t);

	spin_lock_irqsave(&fpsimd_func_lock, flags);
	hash_add(fpsimd_func_ctx, &ctx->node, (unsigned long)current);
	spin_unlock_irqrestore(&fpsimd_func_lock, flags);
	return true;
}

static void is_fpsimd_put_func_task(void)
{
	struct is_fpsimd_func_ctx *ctx;
	unsigned long flags;
	bool found;

	spin_lock_irqsave(&fpsimd_func_lock, flags);
	ctx = is_fpsimd_func_ctx_find();
	found = !!ctx;
	if (ctx && --ctx->depth == 0)
		hash_del(&ctx->node);
	else
		ctx = NULL;
	spin_unlock_irqrestore(&fpsimd_func_lock, flags);

	WARN_ONCE(!found, "is_fpsimd: put_func without matching get_func (task %s)\n",
		  current->comm);

	if (ctx) {
		is_fpsimd_put_task(&ctx->t);
		kfree(ctx);
	}
}

void is_fpsimd_get_func(void)
{
	unsigned long flags;
	bool flushed;
	int idx;

	if (preemptible() && is_fpsimd_get_func_task())
		return;

	preempt_disable();
	flushed = is_fpsimd_flush();
	local_irq_save(flags);

	idx = is_fpsimd_push(flushed, true);
	if (idx >= 0) {
		fpsimd_stack[smp_processor_id()].irq_flags[idx] = flags;
	} else {
		/*
		 * Overflow: no slot to keep the IRQ flags or the state in.
		 * Run the DDK with IRQs as they were, but keep the preempt count
		 * so put_func still lands on this CPU's stack.
		 */
		local_irq_restore(flags);
	}
}

void is_fpsimd_put_func(void)
{
	struct is_fpsimd_stack *s;
	unsigned long flags;
	bool atomic;
	int idx;

	preempt_disable();
	s = &fpsimd_stack[smp_processor_id()];
	idx = s->depth - 1;

	if (idx >= IS_FPSIMD_MAX_DEPTH) {
		/* get_func overflowed: only its preempt count is outstanding */
		is_fpsimd_pop();
		preempt_enable();	/* ours */
		preempt_enable();	/* get_func's */
		return;
	}

	atomic = idx >= 0 && s->func[idx];
	preempt_enable();

	if (!atomic) {
		is_fpsimd_put_func_task();
		return;
	}

	flags = s->irq_flags[idx];
	WARN_ONCE(!irqs_disabled(), "is_fpsimd: DDK returned with IRQs enabled\n");

	is_fpsimd_pop();

	local_irq_restore(flags);
	preempt_enable();
}

/* The DDK sleeps in task context, so its registers follow the thread across switches like 4.19 fpsimd_set_task_using() */
static void is_fpsimd_task_sched_in(struct preempt_notifier *pn, int cpu)
{
	struct is_fpsimd_task *t = container_of(pn, struct is_fpsimd_task, pn);
	unsigned long flags;

	local_irq_save(flags);
	fpsimd_save_and_flush_cpu_state();
	is_fpsimd_load_state(&t->state);
	local_irq_restore(flags);
}

static void is_fpsimd_task_sched_out(struct preempt_notifier *pn,
				     struct task_struct *next)
{
	struct is_fpsimd_task *t = container_of(pn, struct is_fpsimd_task, pn);

	is_fpsimd_save_state(&t->state);
}

static struct preempt_ops is_fpsimd_task_ops = {
	.sched_in	= is_fpsimd_task_sched_in,
	.sched_out	= is_fpsimd_task_sched_out,
};

void is_fpsimd_get_task(struct is_fpsimd_task *t)
{
	/* the key must be on before the first registration, and inc may sleep */
	is_fpsimd_notifier_on();

	preempt_disable();
	is_fpsimd_flush();
	preempt_notifier_init(&t->pn, &is_fpsimd_task_ops);
	preempt_notifier_register(&t->pn);
	preempt_enable();
}

void is_fpsimd_put_task(struct is_fpsimd_task *t)
{
	preempt_disable();
	preempt_notifier_unregister(&t->pn);
	preempt_enable();
}

static void is_fpsimd_notifier_on(void)
{
	static atomic_t notifier_on = ATOMIC_INIT(0);

	if (!atomic_xchg(&notifier_on, 1))
		preempt_notifier_inc();
}

void is_fpsimd_set_task_using(struct task_struct *t)
{
	is_fpsimd_notifier_on();
}
#else
void is_fpsimd_get_isr(void)
{
	kernel_neon_begin();
}

void is_fpsimd_put_isr(void)
{
	kernel_neon_end();
}

void is_fpsimd_get_func(void)
{
	fpsimd_get();
}

void is_fpsimd_put_func(void)
{
	fpsimd_put();
}

void is_fpsimd_get_task(struct is_fpsimd_task *t)
{
}

void is_fpsimd_put_task(struct is_fpsimd_task *t)
{
}

void is_fpsimd_set_task_using(struct task_struct *t)
{
	fpsimd_set_task_using(t);
}
#endif
#endif
