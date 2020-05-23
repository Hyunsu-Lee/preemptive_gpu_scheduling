#undef TRACE_SYSTEM
#define TRACE_SYSTEM gpu

#if !defined(_TRACE_GPU_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_GPU_H

#include <linux/tracepoint.h>
#include <linux/time.h>

#define show_secs_from_ns(ns) \
	({ \
		u64 t = ns + (NSEC_PER_USEC / 2); \
		do_div(t, NSEC_PER_SEC); \
		t; \
	})

#define show_usecs_from_ns(ns) \
	({ \
		u64 t = ns + (NSEC_PER_USEC / 2) ; \
		u32 rem; \
		do_div(t, NSEC_PER_USEC); \
		rem = do_div(t, USEC_PER_SEC); \
	})

/*
 * The gpu_sched_switch event indicates that a switch from one GPU context to
 * another occurred on one of the GPU hardware blocks.
 *
 * The gpu_name argument identifies the GPU hardware block.  Each independently
 * scheduled GPU hardware block should have a different name.  This may be used
 * in different ways for different GPUs.  For example, if a GPU includes
 * multiple processing cores it may use names "GPU 0", "GPU 1", etc.  If a GPU
 * includes a separately scheduled 2D and 3D hardware block, it might use the
 * names "2D" and "3D".
 *
 * The timestamp argument is the timestamp at which the switch occurred on the
 * GPU. These timestamps are in units of nanoseconds and must use
 * approximately the same time as sched_clock, though they need not come from
 * any CPU clock. The timestamps for a single hardware block must be
 * monotonically nondecreasing.  This means that if a variable compensation
 * offset is used to translate from some other clock to the sched_clock, then
 * care must be taken when increasing that offset, and doing so may result in
 * multiple events with the same timestamp.
 *
 * The next_ctx_id argument identifies the next context that was running on
 * the GPU hardware block.  A value of 0 indicates that the hardware block
 * will be idle.
 *
 * The next_prio argument indicates the priority of the next context at the
 * time of the event.  The exact numeric values may mean different things for
 * different GPUs, but they should follow the rule that lower values indicate a
 * higher priority.
 *
 * The next_job_id argument identifies the batch of work that the GPU will be
 * working on.  This should correspond to a job_id that was previously traced
 * as a gpu_job_enqueue event when the batch of work was created.
 */
TRACE_EVENT(gpu_custom,
        TP_PROTO(const char *func_name, u64 timestamp, u32 ctx_id, u64 atom_id, u32 data1, u32 data2),

        TP_ARGS(func_name, timestamp, ctx_id, atom_id, data1, data2),

        TP_STRUCT__entry(
            __string(       func_name,      func_name       )
            __field(        u64,            timestamp       )
            __field(        u32,            ctx_id          )
            __field(        u64,            atom_id         )
            __field(        u32,            data1           )
            __field(        u32,            data2           )
        ),
        TP_fast_assign(
            __assign_str(func_name, func_name);
            __entry->timestamp = timestamp;
            __entry->ctx_id = ctx_id;
            __entry->atom_id = atom_id;
            __entry->data1 = data1;
            __entry->data2 = data2;
        ),
        TP_printk("[%llu.%06lu] - [%s] : ctx[ %u ] atom[ %llu ] data1[ %u ] data2[ %u ]",
            (unsigned long long)show_secs_from_ns(__entry->timestamp),
            (unsigned long)show_usecs_from_ns(__entry->timestamp),
            __get_str(func_name),
            __entry->ctx_id,
            __entry->atom_id,
            __entry->data1,
            __entry->data2)
);

TRACE_EVENT(gpu_custom_bench,
        TP_PROTO(const char*func_name, u32 ctx_id, u64 using_time, u64 snapshot_time, u64 matimes, u64 ktimes, u32 snapshot_pages, u32 mapages, u32 nr_kernel, u32 preemption1, u32 preemption2),
        
        TP_ARGS(func_name, ctx_id, using_time, snapshot_time, matimes, ktimes, snapshot_pages, mapages, nr_kernel, preemption1, preemption2),
        
        TP_STRUCT__entry(
            __string(   func_name, func_name    )
            __field(    u32,    ctx_id  )
            __field(    u64,    using_time  )
            __field(    u64,    snapshot_time   )
            __field(    u64,    matimes   )
            __field(    u64,    ktimes   )
            __field(    u32,    snapshot_pages  )
            __field(    u32,    mapages  )
            __field(    u32,    nr_kernel  )
            __field(    u32,    preemption1 )
            __field(    u32,    preemption2 )
        ),

        TP_fast_assign(
            __assign_str(func_name,func_name);
            __entry->ctx_id = ctx_id;
            __entry->using_time = using_time;
            __entry->snapshot_time = snapshot_time;
            __entry->matimes = matimes;
            __entry->ktimes = ktimes;
            __entry->snapshot_pages = snapshot_pages;
            __entry->mapages = mapages;
            __entry->nr_kernel = nr_kernel;
            __entry->preemption1 = preemption1;
            __entry->preemption2 = preemption2;
        ),

        TP_printk("[ %s ]-(%03u) : [ %llu.%06lu ] [ %llu.%06lu ] [ %llu.%06lu ] [ %llu.%06lu ] [ %u ] [ %u ] [ %u ] [ %u ] [ %u ]",
            __get_str(func_name),
            __entry->ctx_id,
            (unsigned long long)show_secs_from_ns(__entry->using_time),
            (unsigned long)show_usecs_from_ns(__entry->using_time),
            (unsigned long long)show_secs_from_ns(__entry->snapshot_time),
            (unsigned long)show_usecs_from_ns(__entry->snapshot_time),
            (unsigned long long)show_secs_from_ns(__entry->matimes),
            (unsigned long)show_usecs_from_ns(__entry->matimes),
            (unsigned long long)show_secs_from_ns(__entry->ktimes),
            (unsigned long)show_usecs_from_ns(__entry->ktimes),
            __entry->snapshot_pages,
            __entry->mapages,
            __entry->nr_kernel, 
            __entry->preemption1,
            __entry->preemption2)
);

TRACE_EVENT(gpu_sched_switch,

	TP_PROTO(const char *gpu_name, u64 timestamp,
		u32 next_ctx_id, s32 next_prio, u32 next_job_id),

	TP_ARGS(gpu_name, timestamp, next_ctx_id, next_prio, next_job_id),

	TP_STRUCT__entry(
		__string(       gpu_name,       gpu_name        )
		__field(        u64,            timestamp       )
		__field(        u32,            next_ctx_id     )
		__field(        s32,            next_prio       )
		__field(        u32,            next_job_id     )
	),

	TP_fast_assign(
		__assign_str(gpu_name, gpu_name);
		__entry->timestamp = timestamp;
		__entry->next_ctx_id = next_ctx_id;
		__entry->next_prio = next_prio;
		__entry->next_job_id = next_job_id;
	),

	TP_printk("gpu_name=%s ts=%llu.%06lu next_ctx_id=%lu next_prio=%ld "
		"next_job_id=%lu",
		__get_str(gpu_name),
		(unsigned long long)show_secs_from_ns(__entry->timestamp),
		(unsigned long)show_usecs_from_ns(__entry->timestamp),
		(unsigned long)__entry->next_ctx_id,
		(long)__entry->next_prio,
		(unsigned long)__entry->next_job_id)
);

/*
 * The gpu_job_enqueue event indicates that a batch of work has been queued up
 * to be processed by the GPU.  This event is not intended to indicate that
 * the batch of work has been submitted to the GPU hardware, but rather that
 * it has been submitted to the GPU kernel driver.
 *
 * This event should be traced on the thread that initiated the work being
 * queued.  For example, if a batch of work is submitted to the kernel by a
 * userland thread, the event should be traced on that thread.
 *
 * The ctx_id field identifies the GPU context in which the batch of work
 * being queued is to be run.
 *
 * The job_id field identifies the batch of work being queued within the given
 * GPU context.  The first batch of work submitted for a given GPU context
 * should have a job_id of 0, and each subsequent batch of work should
 * increment the job_id by 1.
 *
 * The type field identifies the type of the job being enqueued.  The job
 * types may be different for different GPU hardware.  For example, a GPU may
 * differentiate between "2D", "3D", and "compute" jobs.
 */
TRACE_EVENT(gpu_job_enqueue,

	TP_PROTO(u32 ctx_id, u32 job_id, const char *type),

	TP_ARGS(ctx_id, job_id, type),

	TP_STRUCT__entry(
		__field(        u32,            ctx_id          )
		__field(        u32,            job_id          )
		__string(       type,           type            )
	),

	TP_fast_assign(
		__entry->ctx_id = ctx_id;
		__entry->job_id = job_id;
		__assign_str(type, type);
	),

	TP_printk("ctx_id=%lu job_id=%lu type=%s",
		(unsigned long)__entry->ctx_id,
		(unsigned long)__entry->job_id,
		__get_str(type))
);

#undef show_secs_from_ns
#undef show_usecs_from_ns

#endif /* _TRACE_GPU_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
