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

	atom_priority = katom->kctx->jctx.sched_info.runpool.policy_ctx.cfs.gpgpu_priority;

	slot = &katom->kctx->kbdev->jm_slots[js];
	jobs_submitted = slot->submitted_nr;

	//trace_gpu_custom("sched - start", ktime_to_ns(ktime_get()), katom->kctx->ctx_id, katom->atom_id, (u32)jobs_submitted, (u32) LIMIT_SUBMITTED_NR);
	for(i=LIMIT_SUBMITTED_NR-1;i>=0;i--){

		slot_atom = slot->submitted[(slot->submitted_head + i) & 15];

		if(slot_atom != NULL){

			if(atom_priority < slot_atom->kctx->jctx.sched_info.runpool.policy_ctx.cfs.gpgpu_priority){

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
							
							kbasep_job_slot_soft_or_hard_stop_do_action(kbdev, js, JSn_COMMAND_HARD_STOP, slot_atom->core_req, slot_atom);

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

								kbasep_job_slot_soft_or_hard_stop_do_action(kbdev, js, JSn_COMMAND_HARD_STOP, slot_atom->core_req, slot_atom);

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
