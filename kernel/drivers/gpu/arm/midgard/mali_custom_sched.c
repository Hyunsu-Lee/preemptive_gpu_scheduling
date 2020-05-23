#include <mali_kbase.h>
#include <mali_kbase_defs.h>
#include <mali_kbase_mem.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <mali_kbase_jm.h>

#ifdef _TSK_CUSTOM_SCHED_

void sc_hw_submit(kbase_device *kbdev, kbase_jd_atom *katom, int js){
	unsigned long flags;
	kbase_context *kctx;
	u32 cfg;
	u64 jc_head = katom->jc;

	KBASE_DEBUG_ASSERT(kbdev);
	KBASE_DEBUG_ASSERT(katom);
       
	kctx = katom->kctx;
	spin_lock_irqsave(&katom->run_lock, flags);

	if(katom->run_lock_flags){
		//trace_gpu_custom("submit atom - stop", ktime_to_ns(ktime_get()), kctx->ctx_id, katom->atom_id, (u32)0, (u32)katom->run_lock_flags);
		katom->run_lock_flags--;
		spin_unlock_irqrestore(&katom->run_lock, flags);
		return;
	}else{
		//trace_gpu_custom("submit atom - go", ktime_to_ns(ktime_get()), kctx->ctx_id, katom->atom_id, (u32)0, (u32)katom->run_lock_flags);
		katom->run_lock_flags++;
	}

	/* Command register must be available */                                               
	KBASE_DEBUG_ASSERT(kbasep_jm_is_js_free(kbdev, js, kctx));                             
	/* Affinity is not violating */                                                        
	//kbase_js_debug_log_current_affinities(kbdev);
	KBASE_DEBUG_ASSERT(!kbase_js_affinity_would_violate(kbdev, js, katom->affinity));      
	                                                                                       
	kbase_reg_write(kbdev, JOB_SLOT_REG(js, JSn_HEAD_NEXT_LO), jc_head & 0xFFFFFFFF, kctx);
	kbase_reg_write(kbdev, JOB_SLOT_REG(js, JSn_HEAD_NEXT_HI), jc_head >> 32, kctx);       

	kbase_reg_write(kbdev, JOB_SLOT_REG(js, JSn_AFFINITY_NEXT_LO), katom->affinity & 0xFFFFFFFF, kctx);
	kbase_reg_write(kbdev, JOB_SLOT_REG(js, JSn_AFFINITY_NEXT_HI), katom->affinity >> 32, kctx);       

	/* start MMU, medium priority, cache clean/flush on end, clean/flush on start */                                                                      
	cfg = kctx->as_nr | JSn_CONFIG_END_FLUSH_CLEAN_INVALIDATE | JSn_CONFIG_START_MMU | JSn_CONFIG_START_FLUSH_CLEAN_INVALIDATE | JSn_CONFIG_THREAD_PRI(8);

	if (kbase_hw_has_feature(kbdev, BASE_HW_FEATURE_JOBCHAIN_DISAMBIGUATION)) {
		if (!kbdev->jm_slots[js].job_chain_flag) {
			cfg |= JSn_CONFIG_JOB_CHAIN_FLAG;
			katom->atom_flags |= KBASE_KATOM_FLAGS_JOBCHAIN;
			kbdev->jm_slots[js].job_chain_flag = MALI_TRUE;
		} else {
			katom->atom_flags &= ~KBASE_KATOM_FLAGS_JOBCHAIN;
			kbdev->jm_slots[js].job_chain_flag = MALI_FALSE;
		}
	}

	kbase_reg_write(kbdev, JOB_SLOT_REG(js, JSn_CONFIG_NEXT), cfg, kctx);

	katom->start_timestamp = ktime_get();                                                                                                                 
	                                                                                                                                                      
	/* GO ! */
	KBASE_LOG(2, kbdev->dev, 
			"JS: Submitting atom %p from ctx %p to js[%d] with head=0x%llx, affinity=0x%llx", katom, kctx, js, jc_head, katom->affinity);
	                                                                                                                                                      
	KBASE_TRACE_ADD_SLOT_INFO(kbdev, JM_SUBMIT, kctx, katom, jc_head, js, (u32) katom->affinity); 

	kbase_reg_write(kbdev, JOB_SLOT_REG(js, JSn_COMMAND_NEXT), JSn_COMMAND_START, katom->kctx);
	
	spin_unlock_irqrestore(&katom->run_lock, flags);
#ifdef _TSK_CUSTOM_TRACE_
	job_trace_run_start(katom);
#endif
#ifdef _TSK_TRACE_EVICTION_
	if(kbdev->sw_trace == 3 && katom->eviction_stat == 3){//run preemption - launch
		trace_gpu_custom_bench("PRUN-LAUNCH",katom->eviction_stat, ktime_to_ns(ktime_sub(ktime_get(), katom->launch_time)), 
			ktime_to_ns(katom->p_delay), 0, 0, 0, 0, kbdev->run_evict, (u32)kbase_reg_read(kbdev, JOB_CONTROL_REG(JOB_IRQ_STATUS),NULL), (u32)katom->tInfo.atom_id);
	}else if(kbdev->sw_trace == 4 && katom->eviction_stat == 2){//snap cancel-launch
		trace_gpu_custom_bench("PSNAP-LAUNCH",katom->eviction_stat, ktime_to_ns(ktime_sub(ktime_get(), katom->launch_time)), 
			ktime_to_ns(katom->p_delay), 0, 0, 0, 0, kbdev->run_evict, (u32)kbase_reg_read(kbdev, JOB_CONTROL_REG(JOB_IRQ_STATUS),NULL), (u32)katom->tInfo.atom_id);
	}
#endif

	//trace_gpu_custom("submit atom", ktime_to_ns(ktime_get()), kctx->ctx_id, katom->atom_id, (u32)spin_is_locked(&katom->run_lock), (u32)0);

}

//lock 문제 발생 가능성 있지만, bit 단위로 보면 상관 없는 서로 다른 비트 접근한다.
u32 sc_active_jobs(kbase_jm_slot *slot){
	int i;
	u32 nr_jobs = 0;
	struct kbase_jm_slot *preempt_slot = &slot->kbdev->preempt_slot;

	nr_jobs += slot->submitted_nr;
	nr_jobs += preempt_slot->submitted_nr;

	//trace_gpu_custom("done - irq", ktime_to_ns(ktime_get()), 0, 0, (u32)preempt_slot->submitted_nr, (u32)slot->submitted_nr);
	for(i=0;i<slot->submitted_nr;i++){//active slot
		if(!(slot->submitted[(slot->submitted_head + i) &15]->sched_stat & (SCHED_STAT_DONE | SCHED_STAT_PRUN))){
	//		trace_gpu_custom("done - irq - 1", ktime_to_ns(ktime_get()), 0, 0, (u32)i, (u32)slot->submitted[(slot->submitted_head + i) &15]->sched_stat);
			nr_jobs--;
		}

	}
	
	return nr_jobs;
}

#ifdef _TSK_CUSTOM_SCHED_MFQ_
/* weight = 1.25 */
static const int weight_of_priority[] = {
	/*  -20 */ 11, 14, 18, 23,
	/*  -16 */ 29, 36, 45, 56,
	/*  -12 */ 70, 88, 110, 137,
	/*   -8 */ 171, 214, 268, 335,
	/*   -4 */ 419, 524, 655, 819,
	/*    0 */ 1024, 1280, 1600, 2000,
	/*    4 */ 2500, 3125, 3906, 4883,
	/*    8 */ 6104, 7630, 9538, 11923,
	/*   12 */ 14904, 18630, 23288, 29110,
	/*   16 */ 36388, 45485, 56856, 71070
};
#define WEIGHT_TABLE_SIZE       40
#define WEIGHT_0_NICE           (WEIGHT_TABLE_SIZE/2)
#define DEFAULT_TIMESLICE	500
	u64 time_delta_us;
int sc_mfq_policy_should_remove_ctx(kbase_jd_atom *katom){

	kbasep_js_policy_cfs_job *job_info;
	kbasep_js_policy_cfs_ctx *ctx_info;

	struct kbase_context *selctx, *kctx;
	struct list_head *scheduled_ctx_pool; 
	struct tsk_sched_mfq *selmfq;
	u64 queue_timeslice;
	int ret = 0;
	
	kctx = katom->kctx;
	scheduled_ctx_pool= &kctx->kbdev->js_data.policy.cfs.scheduled_ctxs_head;

	job_info = &katom->sched_info.cfs;
	ctx_info = &kctx->jctx.sched_info.runpool.policy_ctx.cfs;

	//Emergency Class의 경우 스케줄링에 영향을 주지 않고, 독자적으로 실행한다.
	//Preempted katom을 가진 kctx의 경우는 해당 작업을 수행하지 않음
	if(kctx->sched_info.gpgpu_priority == -20 || katom->sched_stat & SCHED_STAT_PMASK)
		return 0;

	//Runpool 순회하면서, 딜레이 시간과 런타임(time_slice 소모) 시간을 기록한다.
	//이때 delayed time의 경우 높은 우선순위 kctx를 따로 고려 안해도 된다.
	//왜냐하면 현재 cfs 큐에 의해서 높은 우선순위 kctx의 job이 없으면, runpool에 존재하지 않고
	//만약 중간에 높은 우선순위 커널이 들어오면, 딜레이 되는게 아니라 선점이 발생하기 때문에
	//위의 조건에 따라서 낮은 우선순위 커널의 런타임이 높은 우선순위 kctx의 딜레이로 집계되지 않는다.
	//또한, 딜레이 체크시에 job 개수를 따질 필요 없다. 왜냐하면 어차피 아무것도 없으면, 이전 스케줄링
	//타임에 runpool에서 쫓겨났을 것이기 때문이다.
	//같은 우선순위 작업이 중간에 들어왔다면, 런풀에 존재하게 된다.
	//선점된 작업의 경우도, 만약 아주 짧은 시간 높은 우선순위 커널이 실행되서 아직도 post-preempt 상태
	//이면, 딜레이 집계를 하지 않아도 되고, 실행 시간이 길었다면 이미 런풀에 들어와있을 가능성이 크기
	//때문에 무시해도 된다.(복구 과저에서 runpool 까지 복구하는지는 잘 모르겠음)
	
	list_for_each_entry(selctx, scheduled_ctx_pool, jctx.sched_info.runpool.policy_ctx.cfs.list){
		selmfq = &selctx->sched_info.mfq;

		if(selctx!=kctx){ // 종료된 커널 소속이 아닌 kctx들은 딜레이 추가
			//1차 버전
			//selmfq->delayed_time = ktime_add(selmfq->delayed_time, katom->tInfo.run_time);
			//2차 버전
			if(ktime_to_us(selmfq->delay_start)){
			selmfq->delayed_time = ktime_sub(ktime_get(), selmfq->delay_start);
		
			//Queue Delay Time
			queue_timeslice = (DEFAULT_TIMESLICE * weight_of_priority[WEIGHT_0_NICE + selctx->sched_info.gpgpu_priority]);
			if(queue_timeslice<ktime_to_us(selmfq->delayed_time)){

				selmfq->run_time = ktime_set(0, 0);
				selmfq->delayed_time = ktime_set(0, 0);

				if(kctx->sched_info.gpgpu_priority-1>-20){
					selctx->sched_info.gpgpu_priority--;
					//printk(KERN_ALERT"Increase Priority1(%u, %u): O:%d->G:%d\n", selctx->tInfo.ctx_id, kctx->tInfo.ctx_id,
					//		selctx->sched_info.mfq.original_priority, selctx->sched_info.gpgpu_priority);
					ret = 1;
				}

			}
			}//진짜로 딜레이 된 것이라면,
	
		}else{ // 자기 자신 커널이라면, runtime
			if(list_empty(&ctx_info->job_list_head[job_info->cached_variant_idx])){
				selmfq->delayed_time = ktime_set(0, 0);
				selmfq->delay_start = ktime_set(0, 0);
			}else{
				selmfq->delay_start = ktime_get();
			}
			selmfq->run_time = ktime_add(selmfq->run_time, katom->tInfo.run_time);
			
			queue_timeslice = (DEFAULT_TIMESLICE * weight_of_priority[WEIGHT_0_NICE + selctx->sched_info.gpgpu_priority]);

			if(selctx->sched_info.gpgpu_priority < selctx->sched_info.mfq.original_priority){

				if(queue_timeslice<ktime_to_us(selctx->sched_info.mfq.run_time)){

					selctx->sched_info.gpgpu_priority = selctx->sched_info.mfq.original_priority;

					//printk(KERN_ALERT"Decrease Priority2(%u): %d->%d\n", selctx->tInfo.ctx_id, 
					//		selctx->sched_info.mfq.original_priority,selctx->sched_info.gpgpu_priority);
					ret = 1;
				}
			}
		}
	}

	return ret;
}
#endif

//lock 문제 없음.
u8 sc_is_preempt(kbase_jm_slot *slot){

	kbase_jd_atom *katom;

	if(slot->kbdev->preempt_slot.submitted_nr)
		slot = &slot->kbdev->preempt_slot;

	katom = slot->submitted[slot->submitted_head & BASE_JM_SUBMIT_SLOTS_MASK];
	
	if(katom == NULL){
		//trace_gpu_custom("done error : atom null", ktime_to_ns(ktime_get()), 0, 0, (u32)slot->submitted_nr, (u32)0);
		return 1;
	}

	katom->sched_stat &= ~SCHED_STAT_RUN;

	if(katom->sched_stat & SCHED_STAT_PMASK){
		return 1;
	}else{
		katom->sched_stat |= SCHED_STAT_DONE;
		return 0;
	}
}

//head slot의 경우는 100프로 동시 접근 가능하다.
int sc_preempt(struct kbase_jd_atom *katom){

	unsigned long sched_flags, run_flags;
	struct kbase_device *kbdev = katom->kctx->kbdev;
	kbase_jm_slot *slot;
	u8 jobs_submitted;
	kbase_jm_slot *preempt_slot = &kbdev->preempt_slot;
	struct snapshot_kthread_context *sctx = &kbdev->snapshot_ctx.kthread_ctx[kbdev->snapshot_ctx.skthread_head];

	struct kbase_jd_atom *slot_atom, *dequeued_katom;
	int atom_priority;
	u32 preempt_mode = 0;
	int i, js=1;

#ifdef _TSK_TRACE_EVICTION_
	ktime_t p_delay = ktime_get();
#endif

	atom_priority = katom->kctx->sched_info.gpgpu_priority;


	slot = &katom->kctx->kbdev->jm_slots[js];
	jobs_submitted = slot->submitted_nr;

	//trace_gpu_custom("sched - start", ktime_to_ns(ktime_get()), katom->kctx->ctx_id, katom->atom_id, (u32)jobs_submitted, (u32) LIMIT_SUBMITTED_NR);
	for(i=LIMIT_SUBMITTED_NR-1;i>=0;i--){

		slot_atom = slot->submitted[(slot->submitted_head + i) & 15];

		if(slot_atom != NULL){

			if(atom_priority < slot_atom->kctx->sched_info.gpgpu_priority){

#ifdef _TSK_CUSTOM_TRACE_
				context_trace_preemption(katom->kctx);
#endif
				preempt_mode |=  (1<<i);
				atomic_inc(&slot_atom->kctx->process_preempt);

				spin_lock_irqsave(&sctx->sched_lock, sched_flags);

				switch(slot_atom->sched_stat & SCHED_STAT_SMASK){
				case SCHED_STAT_SLOT_READY:
					/*Job이 Slot에 대기하고 있는 경우(head가 아닌경우)*/
					
					slot_atom->sched_stat |= SCHED_STAT_PSLOT;
					slot_atom->kctx->tInfo.nr_preempted++;
					dequeued_katom = kbasep_jm_dequeue_tail_submit_slot(slot);
					slot->submitted[(slot->submitted_head + slot->submitted_nr+1) & BASE_JM_SUBMIT_SLOTS_MASK]=NULL;

					if(dequeued_katom != slot_atom){
						printk(KERN_ALERT"slot atom incorrect");
						//발생 안하겠지만 실수 방지용.
					}

					kbase_jd_done(dequeued_katom, js, NULL, KBASE_JS_ATOM_DONE_EVICTED_FROM_NEXT);
#ifdef _TSK_TRACE_EVICTION_
					katom->eviction_stat = 1;
					katom->patom[i] = dequeued_katom->atom_id;
#endif

					break;
				case SCHED_STAT_SNAP:

					atomic_inc(&slot_atom->kctx->process_preempt);
					/* snapshot thread와 enqueue 모두에서 -- 해줘야 정상 스케줄링 가능.
					 * 딱 이 경우만 동시 접근 가능하기 때문에 atomic 설정
					 * psnap, psced로 나눈 이유는 enqueue와 kthread 종료 수행을 독립적으로 하기 위해서.
					 * psnap은 enqueue시에 초기화, psced는 kthread에서 초기화.
					 * process_preempt는 양쪽에서 --해줘야 정상 preempt 된것으로 보고 스케줄링.
					 */

#ifdef _TSK_TRACE_EVICTION_
					slot_atom->snapc_stat = 1;
					slot_atom->snapc_time = ktime_get();
#endif
					set_bit(SCHED_STAT_PSCED_BIT, &slot_atom->sched_stat);

					//kthread와 동시 접근 가능한 비트이므로
					slot_atom->sched_stat |= SCHED_STAT_PSNAP;
					slot_atom->kctx->tInfo.nr_preempted++;

					katom->sched_stat |= SCHED_STAT_PCER;
					
					dequeued_katom = kbasep_jm_dequeue_tail_submit_slot(slot);
					slot->submitted[(slot->submitted_head + slot->submitted_nr+1) & BASE_JM_SUBMIT_SLOTS_MASK]=NULL;

					if(dequeued_katom != slot_atom){
						printk(KERN_ALERT"slot atom incorrect");
					}

					kbase_jd_done(dequeued_katom, js, NULL, KBASE_JS_ATOM_DONE_EVICTED_FROM_NEXT);
#ifdef _TSK_TRACE_EVICTION_
					katom->eviction_stat = 2;
					katom->patom[i] = dequeued_katom->atom_id;
#endif
					
					break;
				case SCHED_STAT_RUN:
					/*Job이 GPU에서 동작 중일 경우*/
					/*이 부분의 논리에 대해서 해석해보면
					 * 1. 전제 조건으로 무조건 슬롯 head에는 수행 대기라고 생각한다.
					 * 2. Command가 0이라면 이미 수행중이라고 생각한다.
					 * 3. Command가 1이라 할지라도 hwsubmit 호출된 놈이라고 생각하기 떄문에 수행을 하고 있는지 안하고 있는지를 판단한 것이다.
					 * 4. 먼저 커멘드를 없애고 넥스트 레지에 jc가 남아있다면 아직 수행은 하지 않은 것이다. 그러므로 슬롯에서 제거한다
					 * 5. 안남아있다면 잘 모르겠지만 GPU는 jc부터 가져가서 바로 수행하고 수행에 들어가면 next를 없애는 모양이다.
					 * 6. 
					 */
					spin_lock_irqsave(&slot_atom->run_lock, run_flags);
					katom->sched_stat |= SCHED_STAT_PCER;

					if(slot_atom->run_lock_flags == 0){
						slot_atom->run_lock_flags++;

						dequeued_katom = kbasep_jm_dequeue_tail_submit_slot(slot);
						slot->submitted[(slot->submitted_head + slot->submitted_nr+1) & BASE_JM_SUBMIT_SLOTS_MASK]=NULL;

						dequeued_katom->sched_stat &= ~SCHED_STAT_RUN;
						dequeued_katom->sched_stat |= SCHED_STAT_PSLOT;

						kbase_jd_done(dequeued_katom, js, NULL, KBASE_JS_ATOM_DONE_EVICTED_FROM_NEXT);

					}else{
						slot_atom->run_lock_flags--;

						if (kbase_reg_read(kbdev, JOB_SLOT_REG(js, JSn_COMMAND_NEXT), NULL) == 0){
							//진짜로 동작 중일 경우

							slot_atom->sched_stat |= SCHED_STAT_PRUN;
							slot_atom->kctx->tInfo.nr_preempted++;
						

#ifdef _TSK_TRACE_EVICTION_
							kbdev->run_evict++;
							kbdev->eviction_time = ktime_get();
#endif
#ifdef _TSK_CUSTOM_PT_                                                                                            
							if(slot_atom->ik_arg_info.is_ik == IK_ATOM_PT){//PT thread
								set_ikernel_pt_map(slot_atom, 1);
							}else{
#endif
							
							kbasep_job_slot_soft_or_hard_stop_do_action(kbdev, js, JSn_COMMAND_HARD_STOP, slot_atom->core_req, slot_atom);
#ifdef _TSK_CUSTOM_PT_                                   
							}
#endif

							dequeued_katom = kbasep_jm_dequeue_tail_submit_slot(slot);

							slot->submitted[(slot->submitted_head + slot->submitted_nr+1) & BASE_JM_SUBMIT_SLOTS_MASK]=NULL;
							preempt_slot->submitted[(preempt_slot->submitted_head + preempt_slot->submitted_nr) & 15] = dequeued_katom;
							preempt_slot->submitted_nr++;

						}else{
							kbase_reg_write(kbdev, JOB_SLOT_REG(js, JSn_COMMAND_NEXT), JSn_COMMAND_NOP, NULL);

							if (kbase_reg_read(kbdev, JOB_SLOT_REG(js, JSn_HEAD_NEXT_LO), NULL) != 0 || 
									kbase_reg_read(kbdev, JOB_SLOT_REG(js, JSn_HEAD_NEXT_HI), NULL) != 0){

								dequeued_katom = kbasep_jm_dequeue_tail_submit_slot(slot);
								slot->submitted[(slot->submitted_head + slot->submitted_nr+1) & BASE_JM_SUBMIT_SLOTS_MASK]=NULL;

								kbase_reg_write(kbdev, JOB_SLOT_REG(js, JSn_HEAD_NEXT_LO), 0, NULL);
								kbase_reg_write(kbdev, JOB_SLOT_REG(js, JSn_HEAD_NEXT_HI), 0, NULL);

								dequeued_katom->sched_stat &= ~SCHED_STAT_RUN;
								dequeued_katom->sched_stat |= SCHED_STAT_PSLOT;

								if (kbase_hw_has_feature(kbdev, BASE_HW_FEATURE_JOBCHAIN_DISAMBIGUATION)) 
									slot->job_chain_flag = !slot->job_chain_flag;

								kbase_jd_done(dequeued_katom, js, NULL, KBASE_JS_ATOM_DONE_EVICTED_FROM_NEXT);
							}else{
								//이 과정 수행도중에 job이 수행되버렸을 때
								
								slot_atom->sched_stat |= SCHED_STAT_PRUN;

#ifdef _TSK_CUSTOM_PT_                                                                                            
							if(slot_atom->ik_arg_info.is_ik == IK_ATOM_PT){//PT thread
								set_ikernel_pt_map(slot_atom, 1);
							}else{
#endif
								kbasep_job_slot_soft_or_hard_stop_do_action(kbdev, js, JSn_COMMAND_HARD_STOP, slot_atom->core_req, slot_atom);
#ifdef _TSK_CUSTOM_PT_                                   
							}
#endif

								dequeued_katom = kbasep_jm_dequeue_tail_submit_slot(slot);
								slot->submitted[(slot->submitted_head + slot->submitted_nr+1) & BASE_JM_SUBMIT_SLOTS_MASK]=NULL;        
								preempt_slot->submitted[(preempt_slot->submitted_head + preempt_slot->submitted_nr) & 15] = dequeued_katom;
								preempt_slot->submitted_nr++;

							}
						}
					}

					spin_unlock_irqrestore(&slot_atom->run_lock, run_flags);
#ifdef _TSK_TRACE_EVICTION_
					katom->eviction_stat = 3;
#endif
					break;
				}
				spin_unlock_irqrestore(&sctx->sched_lock, sched_flags);
			}	
		
		}else{//slot이 비어있는 경우
			preempt_mode |= (1<<i);
		}
	}

#ifdef _TSK_TRACE_EVICTION_
	katom->p_delay = ktime_sub(ktime_get(),p_delay);
	katom->launch_time = ktime_get();
#endif

	return preempt_mode;
}

u8 sc_resched(struct kbase_jd_atom *katom){

	bool_t need_to_try_schedule_context = false;
	struct kbase_context *kctx = katom->kctx;

	if(katom->sched_stat & SCHED_STAT_PMASK){

		if(katom->sched_stat & SCHED_STAT_PRUN)
			katom->sched_stat |= SCHED_STAT_RERUN;

		katom->event_code = BASE_JD_EVENT_DONE;
		katom->status = KBASE_JD_ATOM_STATE_IN_JS;
		need_to_try_schedule_context |= kbasep_js_add_job(kctx, katom);

	}else{
		need_to_try_schedule_context = jd_done_nolock(katom);
	}

	return need_to_try_schedule_context;
}
#endif
