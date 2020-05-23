#include <mali_kbase.h>     
#include <mali_kbase_defs.h>
#include <mali_kbase_mem.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <mali_kbase_jm.h>
#include <linux/sched.h>


#ifdef _TSK_CUSTOM_SNAP_
/*start*/

void * memcpy_neon(void *, const void *, size_t);

static INLINE int is_snapshot_cancel(struct kbase_jd_atom *katom){
	
	//if(test_bit(SCHED_STAT_PSCED_BIT,&katom->sched_stat)){// & SCHED_STAT_PSCED){
	if(katom->sched_stat & SCHED_STAT_PSCED){
		return 1;
	}
	return 0;
}


void create_snapthread(struct kbase_device *kbdev, int id){
	struct sched_param param = {.sched_priority = 1};
	kbdev->snapshot_ctx.kthread_ctx[id].snapshot_kthread = kthread_create(snapshot_kthread,(void*)&kbdev->snapshot_ctx.kthread_ctx[id],"snapshot_kthread");
	sched_setscheduler(kbdev->snapshot_ctx.kthread_ctx[id].snapshot_kthread, SCHED_FIFO, &param);
	kthread_bind(kbdev->snapshot_ctx.kthread_ctx[id].snapshot_kthread, KTHREAD_CORE_BASE+id);
	wake_up_process(kbdev->snapshot_ctx.kthread_ctx[id].snapshot_kthread);
}

void destroy_snapthread(struct kbase_device *kbdev, int id){
	kthread_stop(kbdev->snapshot_ctx.kthread_ctx[id].snapshot_kthread);
}   

void init_snapshot_ctx(struct kbase_device *kbdev){                                  
	int i, j;
	kbdev->snapshot_ctx.skthread_head = 0;
	for(i=0;i<NR_SNAPSHOT_KTHREAD;i++){

		kbdev->snapshot_ctx.kthread_ctx[i].id = i;
		kbdev->snapshot_ctx.kthread_ctx[i].kbdev = kbdev;
		kbdev->snapshot_ctx.kthread_ctx[i].katom = NULL;
		kbdev->snapshot_ctx.kthread_ctx[i].snapshot_kthread = NULL;
		spin_lock_init(&kbdev->snapshot_ctx.kthread_ctx[i].sched_lock);
		spin_lock_init(&kbdev->snapshot_ctx.kthread_ctx[i].snap_thread_lock);

		kbdev->snapshot_ctx.kthread_ctx[i].curr_pos = 0;
		kbdev->snapshot_ctx.kthread_ctx[i].head_pos = 0;
		atomic_set(&kbdev->snapshot_ctx.kthread_ctx[i].nr_snap_atom,0);
		for(j=0;j<10;j++)
			kbdev->snapshot_ctx.kthread_ctx[i].snap_atom_list[j] = NULL;


		init_waitqueue_head(&kbdev->snapshot_ctx.kthread_ctx[i].snap_wqueue);
		create_snapthread(kbdev, i);
	}
				                                                                                     
}

void change_snapshot_thread(struct kbase_device *kbdev){

	int i;
	for(i=0;i<NR_SNAPSHOT_KTHREAD;i++){
		if(!atomic_read(&kbdev->snapshot_ctx.kthread_ctx[i].nr_snap_atom)){
			kbdev->snapshot_ctx.skthread_head = i;
			return;
		}
	}
	kbdev->snapshot_ctx.skthread_head = (kbdev->snapshot_ctx.skthread_head + 1) % NR_SNAPSHOT_KTHREAD;
}

void snapshot_ipi(void *snapshot_context){
	unsigned long flags;
	struct snapshot_kthread_context *sctx = (struct snapshot_kthread_context*)snapshot_context;
	struct kbase_device *kbdev = sctx->kbdev;

	int i, ret = 0;
	
	if(sctx->katom == NULL){
		//trace_gpu_custom("kthread - wakeup null", ktime_to_ns(ktime_get()), 0, 0, (u32)0, (u32)0);
		goto cancel;
	}
	//trace_gpu_custom("kthread - start", ktime_to_ns(ktime_get()), sctx->katom->kctx->ctx_id, sctx->katom->atom_id, (u32)0, (u32)0);
	
	if((ret = is_snapshot_cancel(sctx->katom)))
		goto cancel;

	if(!(sctx->katom->sched_stat & SCHED_STAT_RERUN))
		snapshot_store(sctx->katom);
	else{                           
		snapshot_restore(sctx->katom);
	}

	spin_lock_irqsave(&sctx->sched_lock, flags);
	if((ret = is_snapshot_cancel(sctx->katom))){
		spin_unlock_irqrestore(&sctx->sched_lock, flags);
		goto cancel;
	}
	
	sctx->katom->sched_stat ^= SCHED_STAT_SMASK; //~SCHED_STAT_SNAP & SCHED_STAT_RUN
	spin_unlock_irqrestore(&sctx->sched_lock, flags);

	//trace_gpu_custom("kthread - submit call", ktime_to_ns(ktime_get()), sctx->katom->kctx->ctx_id, sctx->katom->atom_id, (u32)0, (u32)0);
	sc_hw_submit(kbdev, sctx->katom, 1);

	sctx->katom->param.nr_restore = 0;
	for(i=sctx->katom->param.nr_restore;sctx->katom->param.param[i]!=NULL;i++)
		sctx->katom->param.nr_restore_pages[i]=0;

cancel:
	if(ret){
		atomic_dec(&sctx->katom->kctx->process_preempt);
		clear_bit(SCHED_STAT_PSCED_BIT, &sctx->katom->sched_stat);
#ifdef _TSK_TRACE_EVICTION_
		if(kbdev->sw_trace == 2)
			trace_gpu_custom_bench("SNAP-EVICTION",sctx->katom->atom_id, ktime_to_ns(ktime_sub(ktime_get(), sctx->katom->snapc_time)), 0,0,0,0,0,0,0,0);
#endif
	}
	
	//trace_gpu_custom("kthread - sleep-1", ktime_to_ns(ktime_get()), sctx->katom->kctx->ctx_id, sctx->katom->atom_id, (u32)0, (u32)0);
	sctx->katom = NULL;
	//trace_gpu_custom("kthread - exit", ktime_to_ns(ktime_get()), 0, 0, (u32)0, (u32)0);
	return;
}

void submit_snapshot(int cpu, struct snapshot_kthread_context* sctx){

	smp_call_function_single(cpu+4, snapshot_ipi, (void*)sctx, 0);
}

int snapshot_kthread(void *param){
	
	unsigned long flags;
	struct snapshot_kthread_context *sctx = (struct snapshot_kthread_context*)param;
	struct kbase_device *kbdev = sctx->kbdev;
	struct kbase_jd_atom *katom = NULL;

	int i, ret = 0;

	do{
		if(!atomic_read(&sctx->nr_snap_atom)){
			interruptible_sleep_on(&sctx->snap_wqueue);
		}

		katom = sctx->snap_atom_list[sctx->curr_pos];

		if(katom == NULL){
			//trace_gpu_custom("kthread - wakeup null", ktime_to_ns(ktime_get()), 0, 0, (u32)sctx->curr_pos, (u32)atomic_read(&sctx->nr_snap_atom));
			goto sleep;
		}
		//trace_gpu_custom("kthread - start", ktime_to_ns(ktime_get()), katom->kctx->tInfo.ctx_id, katom->tInfo.atom_id, (u32)katom->sched_stat, (u32)sctx->id);
		
		//snapshot
		if((ret = is_snapshot_cancel(katom)))
			goto sleep;

		if(!(katom->sched_stat & SCHED_STAT_RERUN)){
			snapshot_store(katom);
		}else{                           
			snapshot_restore(katom);
		}
		
		spin_lock_irqsave(&sctx->sched_lock, flags);
		if((ret = is_snapshot_cancel(katom))){
			spin_unlock_irqrestore(&sctx->sched_lock, flags);
			goto sleep;
		}


		katom->sched_stat ^= SCHED_STAT_SMASK; //~SCHED_STAT_SNAP & SCHED_STAT_RUN
		spin_unlock_irqrestore(&sctx->sched_lock, flags);

		sc_hw_submit(kbdev, katom, 1);
		//kbase_job_hw_submit(kbdev, sctx->katom, 1);

		katom->param.nr_restore = 0;
		for(i=katom->param.nr_restore;katom->param.param[i]!=NULL;i++)
			katom->param.nr_restore_pages[i]=0;


sleep:
		if(ret){
			ret = 0;
			atomic_dec(&katom->kctx->process_preempt);
			clear_bit(SCHED_STAT_PSCED_BIT, &katom->sched_stat);
#ifdef _TSK_TRACE_EVICTION_
			if(kbdev->sw_trace == 2)
				trace_gpu_custom_bench("SNAP-EVICTION",katom->atom_id, ktime_to_ns(ktime_sub(ktime_get(), katom->snapc_time)),0,0,0,0,0,0,0,0);
#endif
		}
	
		katom = NULL;	
		sctx->snap_atom_list[sctx->curr_pos] = NULL;
		atomic_dec(&sctx->nr_snap_atom);
		sctx->curr_pos = (sctx->curr_pos+1)%10;
		
	}while(1);

	return 0;
}

/*end*/

void snapshot_store(struct kbase_jd_atom* katom){
	int i, j;
	struct kbase_va_region* reg = NULL;
	//unsigned long mem_flag;
	
	u32 granularity = katom->kctx->kbdev->snap_granularity;
	int limit_pages_granularity;

	//kbase_gpu_vm_lock(katom->kctx);
	//spin_lock_irqsave(&katom->kctx->snap_lock, mem_flag);
#ifdef _TSK_CUSTOM_TRACE_
	job_trace_snapshot_start(katom);
#endif

	for(i=katom->param.nr_store;katom->param.param[i]!=NULL;i++){
		        reg = katom->param.param[i];
			limit_pages_granularity = reg->dreg.nr_pages/granularity;

			for(j=katom->param.nr_store_pages[i];j<limit_pages_granularity;j++){
				if(is_snapshot_cancel(katom))
					goto snapshot_cancel;

				memcpy_neon(reg->dreg.kdp+j*PAGE_SIZE*granularity, reg->dreg.ksp+j*PAGE_SIZE*granularity, PAGE_SIZE*granularity);
				katom->param.nr_store_pages[i]++;
			}

			if((reg->dreg.nr_pages%granularity != 0) && (katom->param.nr_store_pages[i] < limit_pages_granularity+1)){
				memcpy_neon(reg->dreg.kdp+j*PAGE_SIZE*granularity, reg->dreg.ksp+j*PAGE_SIZE*granularity, PAGE_SIZE*(reg->dreg.nr_pages%granularity));
				katom->param.nr_store_pages[i]++;
			}

			
			katom->param.nr_store++;
#ifdef _TSK_CUSTOM_TRACE_
			job_trace_snapshot_pages(katom, reg->dreg.nr_pages);
#endif
	}
snapshot_cancel:
#ifdef _TSK_CUSTOM_TRACE_
	job_trace_snapshot_end(katom);
#endif
	return;
        //spin_unlock_irqrestore(&katom->kctx->snap_lock, mem_flag);
	//kbase_gpu_vm_unlock(katom->kctx);
}

void snapshot_restore(struct kbase_jd_atom* katom){
	int i, j;
	struct kbase_va_region* reg;
	//unsigned long mem_flag;
	
	u32 granularity = katom->kctx->kbdev->snap_granularity;
	int limit_pages_granularity;                           

	//kbase_gpu_vm_lock(katom->kctx);
	//spin_lock_irqsave(&katom->kctx->snap_lock, mem_flag);
	reg = NULL;

	for(i=katom->param.nr_restore;katom->param.param[i]!=NULL;i++){
			reg = katom->param.param[i];
			limit_pages_granularity = reg->dreg.nr_pages/granularity;

			for(j=katom->param.nr_restore_pages[i];j<limit_pages_granularity;j++){
				if(is_snapshot_cancel(katom))
					goto snapshot_cancel;

				memcpy_neon(reg->dreg.ksp+j*PAGE_SIZE*granularity, reg->dreg.kdp+j*PAGE_SIZE*granularity, PAGE_SIZE*granularity);
				katom->param.nr_restore_pages[i]++;
			}

			if((reg->dreg.nr_pages%granularity != 0) && (katom->param.nr_restore_pages[i] < limit_pages_granularity+1)){
				memcpy_neon(reg->dreg.ksp+j*PAGE_SIZE*granularity, reg->dreg.kdp+j*PAGE_SIZE*granularity, PAGE_SIZE*(reg->dreg.nr_pages%granularity));
				katom->param.nr_restore_pages[i]++;
			}

			katom->param.nr_restore++;
	}
snapshot_cancel:
	return;
	//spin_unlock_irqrestore(&katom->kctx->snap_lock, mem_flag);
	//kbase_gpu_vm_unlock(katom->kctx);
}
#endif
