#include <mali_kbase.h>
#include <mali_kbase_mem.h>
#include <linux/ktime.h>


void device_trace_init(kbase_device *kbdev){
	kbdev->tInfo.nr_ctx_id = 0;
}

void context_trace_init(kbase_context *kctx){
	kctx->tInfo.ctx_id = kctx->kbdev->tInfo.nr_ctx_id;
	kctx->kbdev->tInfo.nr_ctx_id++;
	
	kctx->tInfo.nr_atom_id = 0;
	kctx->tInfo.nr_reg_id = 0;

	kctx->tInfo.task = current;
	if(!strcmp(current->comm, "micro_bench") || 
		!strcmp(current->comm, "kernel1") || 
		!strcmp(current->comm, "kernel2") || 
		!strcmp(current->comm, "kernel3") || 
		!strcmp(current->comm, "granularity"))
		kctx->tInfo.is_micro = 1;

	kctx->tInfo.app_start = ktime_get();

	kctx->tInfo.total_nr_pages = 0;
	kctx->tInfo.max_nr_pages = 0;
	kctx->tInfo.nr_spages = 0;
	kctx->tInfo.nr_preempt = 0;
}

void context_trace_init_reg(kbase_context *kctx, kbase_va_region *reg){
	reg->tInfo.reg_id = kctx->tInfo.nr_reg_id;
	kctx->tInfo.nr_reg_id++;
	reg->tInfo.start = ktime_get();
}
void context_trace_alloc_done_reg(kbase_context *kctx, kbase_va_region *reg){
	ktime_t alloc_reg_time;
	reg->tInfo.end = ktime_get();
	alloc_reg_time = ktime_sub(reg->tInfo.end, reg->tInfo.start);
	kctx->tInfo.mtimes = ktime_add(kctx->tInfo.mtimes, alloc_reg_time);
	
	kctx->tInfo.total_nr_pages += reg->alloc->nents;
	
	if(kctx->tInfo.total_nr_pages > kctx->tInfo.max_nr_pages)
		kctx->tInfo.max_nr_pages = kctx->tInfo.total_nr_pages;
}

void context_trace_release_reg(kbase_context *kctx, u32 nr_pages){
	kctx->tInfo.total_nr_pages -= nr_pages;
}

void context_trace_release(kbase_context *kctx){
	kctx->tInfo.app_end = ktime_get();
}

void context_trace_preemption(kbase_context *kctx){
	kctx->tInfo.nr_preempt++;
}

void job_trace_init(kbase_jd_atom *katom){

	if(katom->core_req & BASE_JD_REQ_SOFT_JOB)
		return;

	katom->tInfo.atom_id = katom->kctx->tInfo.nr_atom_id;
	katom->kctx->tInfo.nr_atom_id++;

	katom->tInfo.is_head = 0;
	katom->tInfo.nr_spages = 0;
	
	katom->tInfo.atom_req_start = ktime_get();
}

void job_trace_run_start(kbase_jd_atom *katom){
	u8 pos;
	kbase_jm_slot *slot = &katom->kctx->kbdev->jm_slots[1];
	pos = slot->submitted_head & BASE_JM_SUBMIT_SLOTS_MASK;

	if(slot->submitted[pos] == katom){
		katom->tInfo.is_head = 1;
	}else{
		katom->tInfo.is_head = 0;
	}
	katom->tInfo.atom_run_start = ktime_get();
}

void job_trace_run_int_end(kbase_device *kbdev){
	u8 pos;
	kbase_jd_atom *katom;
	kbase_jm_slot *slot;
	ktime_t run_time, delay_time;
	
	if(kbdev->preempt_slot.submitted_nr)
		    slot = &kbdev->preempt_slot;
	else
		    slot = &kbdev->jm_slots[1];

	
	pos = slot->submitted_head & BASE_JM_SUBMIT_SLOTS_MASK;
	katom = slot->submitted[pos];
	
	katom->tInfo.atom_run_end = ktime_get();
	
	if(!katom->tInfo.is_head){
		katom->tInfo.atom_run_start = kbdev->tInfo.prev_atom_end;

		//실제 종료 시간과 interrupt 핸들러 호출까지의 시간은 고려 되지 않음.
	}

	run_time = ktime_sub(katom->tInfo.atom_run_end, katom->tInfo.atom_run_start);
	delay_time = ktime_sub(katom->tInfo.atom_run_start, katom->tInfo.atom_req_start);
	
	katom->tInfo.delay_time = delay_time;
	katom->tInfo.run_time = run_time;
	
	katom->kctx->tInfo.kernel_sched_delay = ktime_add(katom->kctx->tInfo.kernel_sched_delay, delay_time);
	katom->kctx->tInfo.kernel_total_time = ktime_add(katom->kctx->tInfo.kernel_total_time, run_time);

	kbdev->tInfo.prev_atom_end = katom->tInfo.atom_run_end;
		/*trace_gpu_custom_bench(
				katom->kctx->tInfo.task->comm,
				katom->tInfo.atom_id,
				ktime_to_ns(katom->tInfo.atom_req_start),
				ktime_to_ns(katom->tInfo.atom_run_start),
				ktime_to_ns(katom->tInfo.atom_run_end),
				ktime_to_ns(delay_time),
				0,//kctx->nr_spages,
				0,//kctx->nr_mpages,
				0,
				katom->core_req,//kctx->nr_preempt,
			        (u8)katom->tInfo.is_head//kctx->nr_dep_job
				);*/
}

void job_trace_run_end(kbase_jd_atom *katom){
}

void job_trace_snapshot_start(kbase_jd_atom *katom){
	katom->tInfo.stimes_start = ktime_get();
}
void job_trace_snapshot_end(kbase_jd_atom *katom){
	katom->tInfo.stimes_end = ktime_get();
	katom->tInfo.stimes = ktime_sub(katom->tInfo.stimes_end, katom->tInfo.stimes_start);

	katom->kctx->tInfo.nr_spages+=katom->tInfo.nr_spages; //for total snapshot memory size
	katom->kctx->tInfo.stimes = ktime_add(katom->kctx->tInfo.stimes, katom->tInfo.stimes);
}
void job_trace_snapshot_pages(kbase_jd_atom *katom, u32 nr_pages){
	katom->tInfo.nr_spages+=nr_pages;
}


void printout_context_trace(kbase_context *kctx){
	if(!kctx->tInfo.is_micro){
		trace_gpu_custom_bench(
				kctx->tInfo.task->comm,
				kctx->tInfo.ctx_id,
				ktime_to_ns(ktime_sub(kctx->tInfo.app_end, kctx->tInfo.app_start)),
				ktime_to_ns(kctx->tInfo.stimes),
				ktime_to_ns(kctx->tInfo.kernel_sched_delay),
				ktime_to_ns(kctx->tInfo.kernel_total_time),
				kctx->tInfo.nr_spages,
				kctx->tInfo.max_nr_pages,//kctx->nr_mpages,
				kctx->tInfo.nr_atom_id,//kctx->nr_kernel,
				kctx->tInfo.nr_preempt,
				kctx->tInfo.nr_preempted
				);
	}
}

void printout_job_trace(kbase_jd_atom *katom){
		trace_gpu_custom_bench(
				katom->kctx->tInfo.task->comm,
				katom->tInfo.atom_id,
				ktime_to_ns(katom->tInfo.atom_req_start),
				ktime_to_ns(katom->tInfo.stimes_start),
				ktime_to_ns(katom->tInfo.stimes_end),
				ktime_to_ns(katom->tInfo.run_time),//time
				0,//kctx->nr_spages,
				0,//kctx->nr_mpages,
				0,
				katom->core_req,//kctx->nr_preempt,
			        (u8)katom->tInfo.is_head//kctx->nr_dep_job
				);
}
