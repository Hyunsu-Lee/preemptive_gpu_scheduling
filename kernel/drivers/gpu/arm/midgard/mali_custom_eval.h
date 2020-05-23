#include <linux/ktime.h>
#include <linux/sched.h>

typedef struct kbase_va_region kbase_va_region;

typedef struct device_trace{
    u32 nr_ctx_id;
    ktime_t prev_atom_end;
}device_trace;

typedef struct context_trace{
    u32 ctx_id;
    u64 nr_atom_id;
    u32 nr_reg_id;

    struct task_struct *task;
    u8 is_micro;
    
    ktime_t app_start, app_end;
    ktime_t kernel_total_time;
    ktime_t kernel_sched_delay;
    ktime_t stimes;
    ktime_t mtimes;
   
    u32 total_nr_pages;
    u32 max_nr_pages;

    u32 nr_spages; 
    u32 nr_preempt;
    u32 nr_preempted;

}context_trace;

typedef struct job_trace{
    u64 atom_id;

    u8 is_head;
    
    ktime_t atom_req_start, atom_req_end;
    ktime_t atom_run_start, atom_run_end;
    ktime_t delay_time, run_time;
    ktime_t stimes, stimes_start, stimes_end;
    
    u32 nr_spages;

}job_trace;

typedef struct reg_trace{
    u32 reg_id;
    ktime_t start, end;
}reg_trace;


void device_trace_init(kbase_device *kbdev);
void context_trace_init(kbase_context *kctx);
void job_trace_init(kbase_jd_atom *katom);

void context_trace_release(kbase_context *kctx);

void job_trace_run_start(kbase_jd_atom *katom);
void job_trace_run_int_end(kbase_device *kbdev);
void job_trace_run_end(kbase_jd_atom *katom);
void job_trace_snapshot_start(kbase_jd_atom *katom);
void job_trace_snapshot_end(kbase_jd_atom *katom);
void job_trace_snapshot_pages(kbase_jd_atom *katom, u32 nr_pages);


void context_trace_init_reg(kbase_context *kctx, kbase_va_region *reg);
void context_trace_alloc_done_reg(kbase_context *kctx, kbase_va_region *reg);
void context_trace_release_reg(kbase_context *kctx, u32 nr_pages);

void context_trace_preemption(kbase_context *kctx);


void printout_context_trace(kbase_context *kctx);
void printout_job_trace(kbase_jd_atom *katom);
