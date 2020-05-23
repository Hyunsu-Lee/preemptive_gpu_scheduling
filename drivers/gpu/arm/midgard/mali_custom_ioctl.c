#include <mali_kbase.h>
#include <mali_kbase_defs.h>
#include <mali_kbase_mem.h>

#ifdef _TSK_CUSTOM_IOCTL_

void kbase_custom_ctx(struct kbase_context *kctx, u32 ctx_id);
struct kbase_context* kbase_custom_ctx_find(struct kbase_context *kctx, u32 ctx_id);
void kbase_debug_dump_registers(kbase_device *kbdev);

void kbase_custom_ioctl(struct kbase_context *kctx, unsigned int cmd, unsigned long arg){

	u32 nr_ioc = _IOC_NR(cmd);
	u32 size = _IOC_SIZE(cmd);
	gpu_umsg umsg;
	struct kbase_device *kbdev = kctx->kbdev;

	switch(nr_ioc){
		case 0:
			printk(KERN_ALERT"<gpu> ioctl00\n");
			break;
		case 1:
			printk(KERN_ALERT"<gpu> ioctl01\n");

			break;
		case 2:
			printk(KERN_ALERT"<gpu> ioctl02\n");
			break;
		case 3:
			printk(KERN_ALERT"<gpu> ioctl03\n");
			break;
		case 4:
			printk(KERN_ALERT"<gpu> ioctl04\n");
			kbase_js_try_run_jobs_on_slot(kbdev, 1); 
			break;
		case 5:
			printk(KERN_ALERT"<gpu> ioctl05\n");
			if(copy_from_user((void*)&umsg, (const void*)arg, size)==0){
				struct kbase_context *selctx = NULL;
				struct kbase_jd_atom *selatom = NULL;
				int is_atom = 0;
				int js, pos;

				printk(KERN_ALERT"<gpu>Try to hardstop : ctx <%u> atom <%llu>\n", umsg.ctx_id, umsg.atom_id);
				selctx = kbase_custom_ctx_find(kctx, umsg.ctx_id);
				if(selctx == NULL){

					printk(KERN_ALERT"<gpu> There is no ctx\n");
					break;
				}

				for (js = 0; js < kctx->kbdev->gpu_props.num_job_slots; ++js){
					for(pos =0; pos < 16; pos++){
						if(!(kctx->kbdev->jm_slots[js].submitted[pos]==NULL)){
							 selatom = kctx->kbdev->jm_slots[js].submitted[pos];
							 if(selatom->tInfo.atom_id == umsg.atom_id){
								is_atom = 1;
								//selatom->is_not_preempt = 0;
								printk(KERN_ALERT"hardstop success : ctx[%u] atom[%llu]\n",selctx->tInfo.ctx_id, selatom->tInfo.atom_id);
								kbase_job_slot_hardstop(selctx, js, selatom);
								break;
							 }
						}
					}
				}
			}
			break;
		case 6:
			printk(KERN_ALERT"<gpu> ioctl06\n");
			if(copy_from_user((void*)&umsg, (const void*)arg, size)==0){
				kbdev->snap_granularity = (u32)umsg.data1;
				printk(KERN_ALERT"SET snap_granularity : %u (pages)\n", kbdev->snap_granularity);

			}
			break;
		case 7:
			printk(KERN_ALERT"<gpu> ioctl07\n");
			kbase_debug_dump_registers(kbdev);
			break;
		case 8:
			printk(KERN_ALERT"<gpu> ioctl08\n");
			if(copy_from_user((void*)&umsg, (const void*)arg, size)==0){
				kbase_custom_ctx(kctx,umsg.ctx_id);
			}
			break;
		case 9:
			//for preempt trace(1:eviction latency, 2:preempt & launch delay)
			printk(KERN_ALERT"<gpu> ioctl09\n");
#ifdef _TSK_TRACE_EVICTION_
			if(copy_from_user((void*)&umsg, (const void*)arg, size)==0){
				kbdev->sw_trace  = (u8)umsg.data1;
			}
			printk(KERN_ALERT"switching sched trace : %u\n", kbdev->sw_trace);
#endif
			
			break;
		default:
			break;
	}
}

struct kbase_context* kbase_custom_ctx_find(struct kbase_context *kctx, u32 ctx_id){
	int is_ctx = 0;
	struct kbase_context *selctx;
	struct list_head *ctx_pool = &kctx->kbdev->js_data.policy.cfs.ctx_queue_head;               
	struct list_head *scheduled_ctx_pool = &kctx->kbdev->js_data.policy.cfs.scheduled_ctxs_head;

	list_for_each_entry(selctx, ctx_pool, jctx.sched_info.runpool.policy_ctx.cfs.list){
		if(selctx->tInfo.ctx_id == ctx_id){
			is_ctx = 1;
			break;
		}
	}

	if(!is_ctx){
		list_for_each_entry(selctx, scheduled_ctx_pool, jctx.sched_info.runpool.policy_ctx.cfs.list){
			if(selctx->tInfo.ctx_id == ctx_id){
				is_ctx = 1;
				break;
			}
		}
	}

	if(!is_ctx){
		return NULL;
	}

	return selctx;
}

void kbase_custom_ctx(struct kbase_context *kctx, u32 ctx_id){

	struct kbase_device *kbdev;
	kbase_jd_atom *a;
	kbase_context *k;
	u32 cid=~(u32)0x0;
	kbase_context *selk=NULL;

	kbasep_js_policy_cfs_ctx *ctx_info;
	kbasep_js_policy *js_policy;
	kbasep_js_policy_cfs *policy_info;
	kbase_jm_slot *jm_slots;
	                                   
	struct list_head *job_list;
	struct list_head *queue_head;
	int i, j;

	kbdev = kctx->kbdev;
	cid = ctx_id;

	printk(KERN_ALERT"<gpu> ++++++++GPU Queue INFO+++++++\n");

	printk(KERN_ALERT"<gpu> [KBDEV - slot]\n");

	jm_slots = kctx->kbdev->jm_slots;

	for(i=0;i<3;i++){
		for(j=0;j<16;j++){
			if(!(jm_slots[i].submitted[j]==NULL)){
				a = jm_slots[i].submitted[j];
				printk(KERN_ALERT"<gpu> slot <%d> - submitted <%d> - ctx[%u] atom <%llu> - jc <0x%016llx>\n",i, j, a->kctx->tInfo.ctx_id, a->tInfo.atom_id, a->jc);
			}
		}                                                                                                    
	}

	printk(KERN_ALERT"<gpu> [KBDEV - ctx runpool]\n");

	js_policy = &kctx->kbdev->js_data.policy;
	policy_info = &js_policy->cfs;
	queue_head = &policy_info->ctx_queue_head;

	list_for_each_entry(k,queue_head, jctx.sched_info.runpool.policy_ctx.cfs.list){
		if(k->tInfo.ctx_id == cid)
			selk = k;
		printk(KERN_ALERT"<gpu> there is the <%u> ctx\n",k->tInfo.ctx_id);
	}

	printk(KERN_ALERT"<gpu> [KBDEV - ctx scheduled runpool]\n");

	queue_head = &policy_info->scheduled_ctxs_head;

	list_for_each_entry(k,queue_head, jctx.sched_info.runpool.policy_ctx.cfs.list){
		if(k->tInfo.ctx_id == cid)
			selk = k;
		printk(KERN_ALERT"<gpu> there is the <%u> ctx[%s] (%u)\n",k->tInfo.ctx_id, k->tInfo.task->comm, (u32)atomic_read(&k->process_preempt));
	}

	printk(KERN_ALERT"<gpu> <ioctl> [KCTX - atom pool per slot]\n");

	if(selk==NULL)
		selk = kctx;

	ctx_info = &selk->jctx.sched_info.runpool.policy_ctx.cfs;
	for(i=0;i<7;i++){
		job_list = &ctx_info->job_list_head[i];
		
		if(list_empty(job_list))
			printk(KERN_ALERT"<gpu> <ioctl> job_list <%d> empty\n", i);
		else{
			list_for_each_entry(a,job_list, sched_info.cfs.list){
				printk(KERN_ALERT"<gpu> <ioctl> there is the <%llu> atom - jc <0x%016llx>\n", a->tInfo.atom_id, a->jc);
			}
		}
	}
	
	printk(KERN_ALERT"<gpu> <ioctl> [Register Dump]\n");
	printk(KERN_ALERT"<gpu> JOB_SLOT STATUS : 0x%08x | JOB_IRQ_RAWSTAT=0x%08x\n"
				, kbase_reg_read(kbdev, JOB_SLOT_REG(1, JSn_STATUS),NULL)
				, kbase_reg_read(kbdev, JOB_CONTROL_REG(JOB_IRQ_RAWSTAT), NULL));
	printk(KERN_ALERT"<gpu> JSn_HEAD : 0x%llx\n", ((u64) kbase_reg_read(kbdev, JOB_SLOT_REG(1, JSn_HEAD_LO), NULL))
			        | (((u64) kbase_reg_read(kbdev, JOB_SLOT_REG(1, JSn_HEAD_HI), NULL)) << 32));
	printk(KERN_ALERT"<gpu> JSn_HEAD_NEXT : 0x%llx\n", ((u64) kbase_reg_read(kbdev, JOB_SLOT_REG(1, JSn_HEAD_NEXT_LO), NULL))
			        | (((u64) kbase_reg_read(kbdev, JOB_SLOT_REG(1, JSn_HEAD_NEXT_HI), NULL)) << 32));
	printk(KERN_ALERT"<gpu> JSn_COMMAND : 0x%08x | JSn_COMMAND_NEXT : 0x%08x\n"
			        , kbase_reg_read(kbdev, JOB_SLOT_REG(1, JSn_COMMAND), NULL)
				, kbase_reg_read(kbdev, JOB_SLOT_REG(1, JSn_COMMAND_NEXT), NULL));



	printk(KERN_ALERT"<gpu> +++++++++++++++++++++++++++++\n"); 
}
#endif
