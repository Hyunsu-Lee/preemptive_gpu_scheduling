#include <mali_kbase.h>
#include <mali_kbase_defs.h>
#include <mali_kbase_mem.h>

#include <linux/highmem.h>
#include <linux/mempool.h>
#include <linux/mm.h>
#include <linux/atomic.h> 
  
#include <linux/rbtree.h>   
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>


#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/sched.h>


#ifdef _TSK_CUSTOM_IKERNEL_

void * memcpy_neon(void *, const void *, size_t);

struct kbase_device *proc_ikernel_kbdev;
struct list_head ikernel_list_head;
spinlock_t ikernel_ctx_list_lock;


/* 
 *
 * mali_custom_proc 이 먼저 수행되어야 한다.
 * kbase_mem_proc_init : kbdev->proc_ctx_list 초기화
 * create_context : proc_ctx_list 에 열리는 context 추가
 *
 */
//ikernel_arg_info의 경우 katom에도 고유 변수로 넣어야됨


/*
 * 이 함수의 논리는 몇 가지 사실이 고려되어 있지 않음
 * 1. CreateKernel 전에 할당받은 SAME의 경우는 어떻게 할지?
 * 2. Release Kernel 하는 경우는?
 * 3. 아예 논리를 갈아 엎어야됨
 * - 일단 마이크로 벤치마크와 BFS가 동작하므로 보류한다.
 */

void ikernel_create(kbase_context *ctx, u8 is_ik){
	unsigned long flags;
	kbase_va_region *sreg;
	u32 base_addr = 0;
	u32 addr;
	
	//일단은 한번의 요청에 의해 생성되는 것은 하나의 커널 내역만이라고 가정한다.
	///전역적인 내용 가져옴
	u8 cur_ik_idx = ctx->ik_ctx_info.nr_ik;
	u32 cur_offset = ctx->ik_ctx_info.cur_offset;

	//현재 추가될 ik 정보 고정 및 초기화
	ikernel_info *ik_info = &ctx->ik_ctx_info.ik_info[cur_ik_idx];
	ik_info->is_ik = is_ik;
	
	//trace_gpu_custom_ik("++++iKernel INFO CREATION++++:", ctx->tInfo.ctx_id, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	
	spin_lock_irqsave(&ctx->ik_ctx_info.ik_reg_list_lock, flags);

	list_for_each_entry(sreg, &ctx->ik_ctx_info.ik_reg_list_head,ik_reg_info.ik_reg_list){
		base_addr = (u32)(sreg->start_pfn<<PAGE_SHIFT);
		//trace_gpu_custom_ik("++++iKernel INFO CREATION++++: Resion Search(s)", ctx->tInfo.ctx_id, 0, 
		//		base_addr,(u64)(*((u32*)(base_addr+cur_offset))), 0, 0, 0, 0, 0, 0, 0);
		for(addr = base_addr+cur_offset;addr>=base_addr;addr-=0x40){
			if(!*(u32 *)addr){
				//현재 가리키고 있는 주소의 위치에 값이 없다면 종료
				if(cur_offset != ctx->ik_ctx_info.cur_offset){
					//탐색 작업시 어떤 값들이 존재했었다면
					//초기부터 아무것도 없어서 종료된 경우에 기존 값을 유지하기 위한것
					ctx->ik_ctx_info.cur_offset = cur_offset;
					ctx->ik_ctx_info.nr_ik++;
				}
				//trace_gpu_custom_ik("++++iKernel INFO CREATION++++: Resion Search(done)", ctx->tInfo.ctx_id, 0, 
				//		addr, cur_offset, *(u32*)addr, *((u32*)addr+4), 0, 0, 0, 0, ctx->ik_ctx_info.nr_ik);
				spin_unlock_irqrestore(&ctx->ik_ctx_info.ik_reg_list_lock, flags);
				return;
			}
			//trace_gpu_custom_ik("++++iKernel INFO CREATION++++: Resion Search(for)", ctx->tInfo.ctx_id, 0, 
			//			addr, cur_offset, *(u32*)addr, *((u32*)addr+4), 0, 0, 0, 0, ctx->ik_ctx_info.nr_ik);
			if(ik_info->ik_unique_id != *((u32*)addr+4)){
				ik_info->ik_unique_id = *((u32*)addr+4);
			}
			cur_offset-=0x40;

		}
		/*  for가 끝나면 일단 갱신함. 
		 *  왜냐하면 한 페이지 처음 시작부터 끝까지 한 커널이 다 채우는 경우,
		 *  전역 오프셋이 0xfc0인채로 다음 조건에 의해 로컬 오프셋도 0xfc0가 되기 때문에
		 *  다음 region 탐색시 만약 처음부터 아무것도 없다면, 다음 내역이 갱신이 안됨
		 */

		ctx->ik_ctx_info.cur_offset = cur_offset;

		if(addr<base_addr){
			//아직 탐색 종료가 안된 상태
			//다음 region 탐색을 위해 초기화
			//탐색이 끝났었다면, for문 내에서 이미 return되서 종료됨
			cur_offset = 0xfc0;
		}
	}

	spin_unlock_irqrestore(&ctx->ik_ctx_info.ik_reg_list_lock, flags);
	return;
}

ikernel_buffer *create_ikernel_buffer_info(kbase_va_region *reg, u64 gpu_addr){

	unsigned long flags;

	ikernel_buffer *ik_buf = NULL;
	ik_buf = (ikernel_buffer*)kmalloc(sizeof(ikernel_buffer), GFP_KERNEL);
	ik_buf->flags = 0;
	ik_buf->addr = gpu_addr;
	ik_buf->offset = ik_buf->addr&0xfff;
	ik_buf->page_idx = (ik_buf->addr>>PAGE_SHIFT)-reg->start_pfn;
	ik_buf->reg = reg;
	ik_buf->size = 0;
	ik_buf->type = 0;
	ik_buf->one_page_map = kmap(pfn_to_page(PFN_DOWN(reg->alloc->pages[ik_buf->page_idx])));

	ik_buf->buffer_page = NULL;  
	ik_buf->buffer_map = NULL;  
	ik_buf->buffer_map_start = NULL;  
	
	INIT_LIST_HEAD(&ik_buf->ik_buf_list);

	
	spin_lock_irqsave(&reg->ik_reg_info.ik_reg_buf_lock, flags);
	list_add_tail(&ik_buf->ik_reg_buf_list, &reg->ik_reg_info.ik_reg_buf_head);
	spin_unlock_irqrestore(&reg->ik_reg_info.ik_reg_buf_lock, flags);
	
	//trace_gpu_custom_ik("++++ALLOC IK BUFFER++++:", reg->kctx->tInfo.ctx_id, reg->tInfo.reg_id, 
	//		ik_buf->addr, ik_buf->offset, ik_buf->page_idx, *(u64 *)(ik_buf->one_page_map+ik_buf->offset), 
	//		(u64)(ik_buf->addr>>PAGE_SHIFT), 0, 0, 0, 0);
	

	return ik_buf;
}

void ikernel_classify_param(kbase_jd_atom *katom){
	unsigned long flags;
	int i;
	u64 addr;
	u64 *meta;
	ikernel_buffer *ik_buf;
	ikernel_info *ik_info;
	kbase_context *ctx = katom->kctx;


	//trace_gpu_custom_ik("++++DONE JOB CLASSIFY++++:", ctx->tInfo.ctx_id, katom->tInfo.atom_id, 
	//		katom->ik_arg_info.ik_id, 0, 0, 0, 0, 0, 0, 0, 0);

	if(katom->ik_arg_info.ik_info){
		ik_info = katom->ik_arg_info.ik_info;
		if(!ik_info->flags)
			goto IK_TRACK_ADDR;
		else
			goto NO_SEARCH;
	}
	goto ERROR;

IK_TRACK_ADDR:
	for(i=0;i<katom->ik_arg_info.nr_arg;i++){
		addr = katom->ik_arg_info.arg[i];
		ik_info->flags = 1;
		spin_lock_irqsave(&ctx->ik_ctx_info.ik_buf_list_lock, flags);
		list_for_each_entry(ik_buf, &ctx->ik_ctx_info.ik_buf_head, ik_buf_list){
			if(ik_buf->addr == addr){
				meta = (u64 *)(ik_buf->one_page_map+ik_buf->offset);
				__cpuc_flush_dcache_area((void  *)meta, 4);
				if(addr == meta[2]){ //if this region is rop region,
					ik_buf->flags = 1; //rop
					ik_info->arg[ik_info->nr_arg].arg_idx = -1;
					ik_info->arg[ik_info->nr_arg].rop_buf = ik_buf;
					ik_info->arg[ik_info->nr_arg].rop_addr = meta[2];
					ik_info->arg[ik_info->nr_arg].origin_addr = meta[3];
					ik_info->nr_arg++;
					ik_buf->size = meta[0];
					ik_buf->type = meta[1];
					//trace_gpu_custom_ik("++++DONE JOB CLASSIFY++++: ROP", ctx->tInfo.ctx_id, katom->tInfo.atom_id, 
					//	addr, meta[2], meta[3], 0, 0, i, 0, 0, 0);

				}else{
					//trace_gpu_custom_ik("++++DONE JOB CLASSIFY++++: OTHER", ctx->tInfo.ctx_id, katom->tInfo.atom_id, 
					//	addr, meta[2], meta[3], 0, 0, i, 0, 0, 0);
				}
			}
		}
		spin_unlock_irqrestore(&ctx->ik_ctx_info.ik_buf_list_lock, flags);
	}
	return;

NO_SEARCH:
	//trace_gpu_custom_ik("++++DONE JOB CLASSIFY++++: Already", ctx->tInfo.ctx_id, katom->tInfo.atom_id, katom->ik_arg_info.ik_id, 0, 0, 0, 0, 0, 0, 0, 0);
	return;
ERROR:
	//trace_gpu_custom_ik("++++DONE JOB CLASSIFY++++: no have ikernel", ctx->tInfo.ctx_id, katom->tInfo.atom_id, katom->ik_arg_info.ik_id, 0, 0, 0, 0, 0, 0, 0, 0);
	return;
}

int ikernel_is_dep(kbase_jd_atom *first, kbase_jd_atom *second){
	int i, j;
	ikernel_info *ik_info = first->ik_arg_info.ik_info;
	if(!ik_info){
		//trace_gpu_custom_ik("++++DONE JOB++++: not found ik_info", first->kctx->tInfo.ctx_id, first->tInfo.atom_id, 
		//		0, 0, 0, 0, 0, (u32)second->tInfo.atom_id, 0, 0, 0);
		return 0;
	}

	if(first->kctx == second->kctx){
		for(i=0;i<ik_info->nr_arg;i++){
			for(j=0; second->ik_arg_info.nr_arg; j++){
				if(ik_info->arg[i].origin_addr == second->ik_arg_info.arg[j]){
					//trace_gpu_custom_ik("++++DONE JOB++++: ikernel dep kernel", first->kctx->tInfo.ctx_id, first->tInfo.atom_id, 
					//	0, 0, 0, 0, 0, (u32)second->tInfo.atom_id, 0, 0, 0);

					return 1;
				}
			}
		}
	}
	return 0;
}
//vmap 할때 락 관련 오류가 생긴다면, 검색 부분을 lock free로 만들것.
void release_ikernel_buffer_map(kbase_va_region *reg){
	unsigned long flags1, flags2;
	kbase_context *ctx = reg->kctx;
	struct list_head *pos, *q;
	ikernel_buffer *ik_buf;
	
	spin_lock_irqsave(&reg->ik_reg_info.ik_reg_buf_lock, flags1);
	//list_for_each_entry(ik_buf, &reg->ik_reg_info.ik_reg_buf_head, ik_reg_buf_list){
	list_for_each_safe(pos, q, &reg->ik_reg_info.ik_reg_buf_head){
		ik_buf = list_entry(pos, struct ikernel_buffer, ik_reg_buf_list);
		list_del(&ik_buf->ik_reg_buf_list);

		spin_lock_irqsave(&ctx->ik_ctx_info.ik_buf_list_lock, flags2);
		list_del(&ik_buf->ik_buf_list);
		spin_unlock_irqrestore(&ctx->ik_ctx_info.ik_buf_list_lock, flags2);
		//trace_gpu_custom_ik("++++RELEASE IK BUF++++:", ctx->tInfo.ctx_id, reg->tInfo.reg_id, 
		//		ik_buf->addr, ik_buf->offset, ik_buf->page_idx, 0, 0, 0, 0, 0, 0);
		
		if(ik_buf->one_page_map){
			kunmap(pfn_to_page(PFN_DOWN(reg->alloc->pages[ik_buf->page_idx])));
			//kunmap(ik_buf->one_page_map);
		}
		if(ik_buf->buffer_map){
			vunmap(ik_buf->buffer_map);
			kfree(ik_buf->buffer_page);
		}
		//kmalloc 삭제
		kfree(ik_buf);

	}
	spin_unlock_irqrestore(&reg->ik_reg_info.ik_reg_buf_lock, flags1);

}
void destroy_ikernel_buffer(kbase_context *ctx){
	unsigned long flags1, flags2;
	ikernel_buffer *ik_buf;
	struct list_head *pos, *q;
	u64 *meta;

	spin_lock_irqsave(&ctx->ik_ctx_info.ik_buf_list_lock, flags1);
	list_for_each_safe(pos, q, &ctx->ik_ctx_info.ik_buf_head){
		ik_buf = list_entry(pos, struct ikernel_buffer, ik_buf_list);
		list_del(&ik_buf->ik_buf_list);
	
		spin_lock_irqsave(&ik_buf->reg->ik_reg_info.ik_reg_buf_lock, flags2);
		list_del(&ik_buf->ik_reg_buf_list);
		spin_unlock_irqrestore(&ik_buf->reg->ik_reg_info.ik_reg_buf_lock, flags2);
		
		meta = (u64 *)(ik_buf->one_page_map+ik_buf->offset);
		if(ik_buf->addr == meta[1]){
			ik_buf->flags = 1; //rop
			//trace_gpu_custom_ik("++++DESTROY IK BUFFER++++: ROP", ctx->tInfo.ctx_id, 0, 
			//		ik_buf->addr, ik_buf->offset, meta[2], meta[3], 0, (u32)meta[0], (u32)meta[1], 0, 0);
		}else{
			//trace_gpu_custom_ik("++++DESTROY IK BUFFER++++: OTHER", ctx->tInfo.ctx_id, 0, 
			//		ik_buf->addr, ik_buf->offset, meta[2], meta[3], 0, (u32)meta[0], (u32)meta[1], 0, 0);
		}
		if(ik_buf->one_page_map){
			kunmap(pfn_to_page(PFN_DOWN(ik_buf->reg->alloc->pages[ik_buf->page_idx])));
		}
		if(ik_buf->buffer_map){
			vunmap(ik_buf->buffer_map);
			kfree(ik_buf->buffer_page);
		}
		kfree(ik_buf);
	}
	spin_unlock_irqrestore(&ctx->ik_ctx_info.ik_buf_list_lock, flags1);
}

#ifdef _TSK_CUSTOM_PT_                             
void init_ikernel_pt_info(kbase_va_region *reg){
	        reg->ik_pt_info_map = NULL;
}

void release_ikernel_pt_info(kbase_va_region *reg){
	        if(reg->ik_pt_info_map)
			                vunmap(reg->ik_pt_info_map);
}
u32 *create_ikernel_pt_map(kbase_context *kctx, kbase_jd_atom *katom){
	struct page *tmp_page[1];
	kbase_va_region *tmp_reg;                                                                                             
	u32 *klc_map;

	tmp_reg = kbase_region_tracker_find_region_enclosing_address(kctx, katom->jc_indirect);
	tmp_page[0] = pfn_to_page(PFN_DOWN(tmp_reg->alloc->pages[0]));
	klc_map = (u32*)vmap(tmp_page, 1,  VM_MAP, PAGE_KERNEL);
	tmp_reg->ik_pt_info_map = klc_map;

	return (klc_map+(katom->jc_indirect&0xfff)/4+(katom->ik_arg_info.pt_arg_idx)/4);

}

int get_ikernel_pt_map(kbase_jd_atom *katom){
	if(katom->ik_arg_info.pt_arg_map == NULL)
		return -1;
	__cpuc_flush_dcache_area((void*)(katom->ik_arg_info.pt_arg_map), 4); 
	return *(katom->ik_arg_info.pt_arg_map);
}
void set_ikernel_pt_map(kbase_jd_atom *katom, u32 value){
	*(katom->ik_arg_info.pt_arg_map) = value;
	__cpuc_flush_dcache_area((void*)(katom->ik_arg_info.pt_arg_map), 4); 
}
#endif

u8 ikernel_submit_atom(kbase_jd_atom *katom){
	int i;
	u32 start;
	u32* klc_info;
	u32* param_end_info;
	u32 nr_sparam = 0;
	u64* sparam;
	struct kbase_va_region* sreg;

	kbase_context *ctx = katom->kctx;

	unsigned long flags;
	ikernel_buffer *ik_buf;
	u8 is_ik_buf = 0;

	//변형
	u32 addr_diff_min = ~0U;     
	u32 addr_diff_curr = 0U;
	ikernel_info *ik_info = NULL;

	katom->ik_arg_info.ik_info = NULL;
	katom->ik_arg_info.ctx = ctx;
	katom->ik_arg_info.is_ik = NOT_IK_ATOM;
	katom->ik_arg_info.nr_arg = 0;

#ifdef _TSK_CUSTOM_PT_                    
	katom->ik_arg_info.pt_arg_idx = 0;
#endif

	if(ctx->ik_ctx_info.is_ik == NOT_IK_CTX){
		katom->ik_arg_info.is_ik = NOT_IK_ATOM;
		return NOT_IK_ATOM;
	}

	klc_info = (u32*)((u32)katom->jc); 
	katom->jc_indirect = katom->jc; 
 
	if( (*(klc_info+4) & 0x0000000f) == 0x00000005 ){
		katom->jc_indirect = (u64)(*(klc_info+6));
		klc_info = (u32*)(*(klc_info+6));
	}

	if(ctx->ik_ctx_info.is_ik == IK_CTX_F_NTRANS){
		//printk(KERN_ALERT"SUMIT - IK_CTX_F_NTRANS\n");
		katom->ik_arg_info.is_ik = IK_ATOM_NTRANS;
		return IK_ATOM_NTRANS;
	}

	param_end_info = (u32*)(*(klc_info+20));
	nr_sparam =(((*param_end_info & 0x000fff00) >> 8) - (*(klc_info+23) & 0x00000fff))/8;
	sparam = (u64*)(*(klc_info+23));
	//PT 삽입 포인트
#ifdef _TSK_CUSTOM_PT_
	if(ctx->ik_ctx_info.is_ik == IK_CTX_PT){
		u32* pparam = (u32*)sparam;

		for(start = 0;start<nr_sparam*2;start++){
			if(*(pparam+start) == 0xf0f0f0f0){
				katom->ik_arg_info.pt_arg_idx = ((u32)(pparam+start))&0xfff;
				katom->ik_arg_info.pt_arg_map = create_ikernel_pt_map(ctx, katom);
				set_ikernel_pt_map(katom, 0);
				break;
			}
									                
		}
		if(katom->ik_arg_info.pt_arg_idx != 0)
			katom->ik_arg_info.is_ik = IK_ATOM_PT;
		else
			printk(KERN_ALERT"Cannot find preempt state!!\n");

		return IK_ATOM_PT;
	}
#endif
	
	katom->ik_arg_info.ik_id = *((u32 *)(*(klc_info+24))+4);
	//printk(KERN_ALERT"submit atom ik id: %u\n",*((u32 *)(*(klc_info+24))+4));
	
	//trace_gpu_custom_ik("++++SUBMIT ATOM++++:", ctx->tInfo.ctx_id, katom->tInfo.atom_id, katom->ik_arg_info.ik_id, 0, 0, 0, 0, 0, 0, 0, 0);
	for(i=0;i<ctx->ik_ctx_info.nr_ik;i++){
		if(katom->ik_arg_info.ik_id == ctx->ik_ctx_info.ik_info[i].ik_unique_id){
			//trace_gpu_custom_ik("++++SUBMIT ATOM++++: (found ik)", ctx->tInfo.ctx_id, katom->tInfo.atom_id, 
			//	katom->ik_arg_info.ik_id, 0, 0, 0, 0, 0, 0, 0, i);
			katom->ik_arg_info.ik_info = &ctx->ik_ctx_info.ik_info[i];
			katom->ik_arg_info.is_ik = katom->ik_arg_info.ik_info->is_ik;
			goto IK_REGION;
		}else{
			//diff 떠서 가장 가까운 값으로 한다.
			if(katom->ik_arg_info.ik_id > ctx->ik_ctx_info.ik_info[i].ik_unique_id)
				addr_diff_curr = katom->ik_arg_info.ik_id - ctx->ik_ctx_info.ik_info[i].ik_unique_id;
			else
				addr_diff_curr = ctx->ik_ctx_info.ik_info[i].ik_unique_id - katom->ik_arg_info.ik_id;

			if(addr_diff_curr < addr_diff_min){
				ik_info = &ctx->ik_ctx_info.ik_info[i];
				addr_diff_min = addr_diff_curr;
			}
		}
		/*
		for(j=0;j<ctx->ik_ctx_info.ik_info[i].nr_ik_id;j++){
			trace_gpu_custom_ik("++++SUBMIT ATOM++++: (search)", ctx->tInfo.ctx_id, katom->tInfo.atom_id, 
					katom->ik_arg_info.ik_id, ctx->ik_ctx_info.ik_info[i].ik_id[j], 0, 0, 0, i, j, 0, 0);
			if(ctx->ik_ctx_info.ik_info[i].ik_id[j] == katom->ik_arg_info.ik_id){
				trace_gpu_custom_ik("++++SUBMIT ATOM++++: (found ik)", ctx->tInfo.ctx_id, katom->tInfo.atom_id, 
						katom->ik_arg_info.ik_id, 0, 0, 0, 0, i, j, 0, 0);
				katom->ik_arg_info.ik_info = &ctx->ik_ctx_info.ik_info[i];
				// ik 분류만을 위해 추가된 부분 start
				katom->ik_arg_info.is_ik = katom->ik_arg_info.ik_info->is_ik;
				printk(KERN_ALERT"submit atom:%llu, %u\n", katom->tInfo.atom_id, katom->ik_arg_info.is_ik);
				return katom->ik_arg_info.is_ik; //이 부분은 하위 ik_buf 생성 없이 가기 위한 부분임
				// ik 분류만을 위해 추가된 부분 end
				goto IK_REGION;
			}
		}
		*/
	}
	katom->ik_arg_info.ik_info = ik_info;
	katom->ik_arg_info.is_ik = ik_info->is_ik;

	ctx->ik_ctx_info.ik_info[ctx->ik_ctx_info.nr_ik].ik_unique_id = katom->ik_arg_info.ik_id;
	ctx->ik_ctx_info.ik_info[ctx->ik_ctx_info.nr_ik].is_ik = ik_info->is_ik;
	ctx->ik_ctx_info.nr_ik++;
	
IK_REGION:
	if(ctx->ik_ctx_info.is_ik == IK_CTX_F_TRANS){

	for(start = 0;start<nr_sparam;start++){
		sreg = kbase_region_tracker_find_region_enclosing_address(ctx, *(sparam+start));
		if(sreg)
			if(sreg->tInfo.reg_id!=0 && 
				((sreg->flags & KBASE_REG_ZONE_MASK) == KBASE_REG_ZONE_CUSTOM_VA) &&
				sreg->alloc->nents>=64){

				katom->ik_arg_info.arg[katom->ik_arg_info.nr_arg] = *(sparam+start);
				katom->ik_arg_info.nr_arg++;

	//trace_gpu_custom_ik("++++SUBMIT ATOM++++: PARAM", ctx->tInfo.ctx_id, katom->tInfo.atom_id, 
	//		*(sparam+start), katom->ik_arg_info.arg[katom->ik_arg_info.nr_arg-1], sreg->tInfo.reg_id,  sreg->nr_pages, sreg->alloc->nents, 0, 0, 0, 0);

				//이전 katom submit 에서 수행해서 이미 있다면 스킵
				spin_lock_irqsave(&ctx->ik_ctx_info.ik_buf_list_lock, flags);
				list_for_each_entry(ik_buf, &ctx->ik_ctx_info.ik_buf_head,ik_buf_list){
					if(ik_buf->addr == *(sparam+start)){
						is_ik_buf = 1;
						break;
					}
				}
				spin_unlock_irqrestore(&ctx->ik_ctx_info.ik_buf_list_lock, flags);

				if(!is_ik_buf){
					//buf 정보가 없다면, 새로 할당받음
					is_ik_buf = 0;
					
					ik_buf = create_ikernel_buffer_info(sreg, *(sparam+start));

					spin_lock_irqsave(&ctx->ik_ctx_info.ik_buf_list_lock, flags);
					list_add_tail(&ik_buf->ik_buf_list, &ctx->ik_ctx_info.ik_buf_head);
					spin_unlock_irqrestore(&ctx->ik_ctx_info.ik_buf_list_lock, flags);
				}
				
			}
	}//for
	}//if
	return katom->ik_arg_info.is_ik;
}

//왜 인지 inline 안됨 확인할 것
void init_ikernel_ctx(kbase_context *kctx){
	int i,j;

	ikernel_ctx_info *ik_ctx_info = &kctx->ik_ctx_info;

	ik_ctx_info->is_ik = 0;
	ik_ctx_info->cur_offset = 0xfc0;
	ik_ctx_info->nr_ik = 0;
	
	INIT_LIST_HEAD(&ik_ctx_info->ik_ctx_list);
	INIT_LIST_HEAD(&ik_ctx_info->ik_reg_list_head);
	INIT_LIST_HEAD(&ik_ctx_info->ik_reg_list_head_full);
	INIT_LIST_HEAD(&ik_ctx_info->ik_buf_head);
	spin_lock_init(&ik_ctx_info->ik_buf_list_lock);
	spin_lock_init(&ik_ctx_info->ik_reg_list_lock);
	//여기까지는 검증
	for(i=0;i<10;i++){
		ik_ctx_info->ik_info[i].flags = 0;
		ik_ctx_info->ik_info[i].nr_ik_id = 0;
		ik_ctx_info->ik_info[i].nr_arg = 0;
		ik_ctx_info->ik_info[i].ik_unique_id = 0;
		ik_ctx_info->ik_info[i].is_ik = 0;

		for(j=0;j<5;j++)
			ik_ctx_info->ik_info[i].ik_id[j] = 0;

		for(j=0;j<10;j++){
			ik_ctx_info->ik_info[i].arg[j].rop_buf = NULL;
			ik_ctx_info->ik_info[i].arg[j].origin_buf = NULL;
			ik_ctx_info->ik_info[i].arg[j].rop_addr = 0;
			ik_ctx_info->ik_info[i].arg[j].origin_addr = 0;
		}
	}
}

int release_ikernel_ctx(kbase_context *kctx){
	if(delete_ikernel_ctx(kctx->ik_ctx_info.fd))
		return 1;
	return 0;
}
void init_ikernel_region(kbase_va_region *ik_reg){
	INIT_LIST_HEAD(&ik_reg->ik_reg_info.ik_reg_list);
	INIT_LIST_HEAD(&ik_reg->ik_reg_info.ik_reg_buf_head);
	spin_lock_init(&ik_reg->ik_reg_info.ik_reg_buf_lock);
}

void add_ikernel_region(kbase_context *ctx, kbase_va_region *ik_reg){
	unsigned long flags;
	spin_lock_irqsave(&ctx->ik_ctx_info.ik_reg_list_lock, flags);
	list_add_tail(&ik_reg->ik_reg_info.ik_reg_list, &ctx->ik_ctx_info.ik_reg_list_head);
	spin_unlock_irqrestore(&ctx->ik_ctx_info.ik_reg_list_lock, flags);
}

void release_ikernel_region(kbase_va_region *ik_reg){
	unsigned long flags;
	if(!list_empty(&ik_reg->ik_reg_info.ik_reg_list)){
		spin_lock_irqsave(&ik_reg->kctx->ik_ctx_info.ik_reg_list_lock, flags);
		list_del(&ik_reg->ik_reg_info.ik_reg_list);
		spin_unlock_irqrestore(&ik_reg->kctx->ik_ctx_info.ik_reg_list_lock, flags);
	}
}

#define IK_VMAP_ORIGIN (0)
#define IK_VMAP_ROP (1)

void ikernel_vmap_alloc(ikernel_buffer *ik_buf, int mod){
	int i;
	size_t size = 0;

	if(mod == IK_VMAP_ORIGIN)
		size = (size_t)((ik_buf->size*(ik_buf->type)>>PAGE_SHIFT)+1);
	else if(mod == IK_VMAP_ROP)
		size = (size_t)(((sizeof(u64)*4+ik_buf->size*(ik_buf->type+2))>>PAGE_SHIFT)+1);
	
	if(ik_buf->buffer_map && !size)
		return;

	ik_buf->buffer_page = kmalloc(sizeof(struct page*) * size, GFP_KERNEL);
	for(i=0;i<size;i++){
		ik_buf->buffer_page[i] = pfn_to_page(PFN_DOWN(ik_buf->reg->alloc->pages[i+ik_buf->page_idx]));
	}
	ik_buf->buffer_map = vmap(ik_buf->buffer_page, size, VM_MAP, PAGE_KERNEL);
	ik_buf->buffer_map_start = ik_buf->buffer_map+ik_buf->offset;
}

void ikernel_vmap(u64 atom_id, kbase_context *ctx, ikernel_info *ik_info, ikernel_arg_info *arg_info){
	int i, j;
	unsigned long flags;
	ikernel_buffer *rop_buf, *origin_buf;
	u64 origin_addr;
	//trace_gpu_custom_ik("++++VMAP PROCESS++++:", ctx->tInfo.ctx_id, atom_id, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	for(i=0;i<ik_info->nr_arg;i++){
		rop_buf = ik_info->arg[i].rop_buf;
		//trace_gpu_custom_ik("++++VMAP PROCESS++++: rop_buffer", ctx->tInfo.ctx_id, atom_id, 
		//			rop_buf->addr, 0, 0, 0, 0, 0, 0, 0, 0);
		ikernel_vmap_alloc(rop_buf, IK_VMAP_ROP);
	
		if(ik_info->arg[i].arg_idx<0){
			for(j=0;j<arg_info->nr_arg;j++){
				if(ik_info->arg[i].origin_addr == arg_info->arg[j]){
					ik_info->arg[i].arg_idx = j;
					break;
				}	
			}

		}	
		if(ik_info->arg[i].origin_buf
				&& ik_info->arg[i].origin_addr == arg_info->arg[ik_info->arg[i].arg_idx]){
			origin_buf = ik_info->arg[i].origin_buf;
			//trace_gpu_custom_ik("++++VMAP PROCESS++++: origin(1)", ctx->tInfo.ctx_id, atom_id, 
			//		ik_info->arg[i].origin_addr, 0, 0, 0, 0, 0, 0, 0, 0);
			ikernel_vmap_alloc(origin_buf, IK_VMAP_ORIGIN);
		}else{
			origin_addr = arg_info->arg[ik_info->arg[i].arg_idx];
			spin_lock_irqsave(&ctx->ik_ctx_info.ik_buf_list_lock, flags);
			list_for_each_entry(origin_buf, &ctx->ik_ctx_info.ik_buf_head, ik_buf_list){
				if(origin_buf->addr == origin_addr){
					//trace_gpu_custom_ik("++++VMAP PROCESS++++: origin(2)", ctx->tInfo.ctx_id, atom_id, 
					//	origin_addr, 0, 0, 0, 0, 0, 0, 0, 0);
						
						origin_buf->size = rop_buf->size;
						origin_buf->type = rop_buf->type;
						ik_info->arg[i].origin_addr = arg_info->arg[ik_info->arg[i].arg_idx];
						ik_info->arg[i].origin_buf = origin_buf;

						ikernel_vmap_alloc(origin_buf, IK_VMAP_ORIGIN);
				}
			}
			spin_unlock_irqrestore(&ctx->ik_ctx_info.ik_buf_list_lock, flags);
		}
	}
}

void ikernel_restore(kbase_jd_atom *katom){
	int i;
	ikernel_arg_info *arg_info = &katom->ik_arg_info;
	ikernel_info *ik_info = arg_info->ik_info;
	kbase_context *ctx = arg_info->ctx;
	ikernel_buffer *rop_buf, *origin_buf;
	unsigned char *raf, *waf;
	size_t size, type, count;
	void *o_addr, *r_addr;

	size_t start_pos, end_pos;

	ikernel_vmap(katom->tInfo.atom_id, ctx, ik_info, arg_info);

	for(i=0;i<ik_info->nr_arg;i++){
		rop_buf = ik_info->arg[i].rop_buf;
		origin_buf = ik_info->arg[i].origin_buf;
		size = rop_buf->size;
		type = rop_buf->type;
		
		o_addr = origin_buf->buffer_map_start;
		r_addr = rop_buf->buffer_map_start+sizeof(u64)*4;

		raf = (unsigned char *)r_addr+size*type;
		waf = raf+size;

		//trace_gpu_custom_ik("++++IK RESTORE++++:start", ctx->tInfo.ctx_id, katom->tInfo.atom_id, 
		//		(u64)(u32)o_addr, (u64)(u32)r_addr, (u64)(u32)raf, (u64)(u32)waf, rop_buf->addr, (u64)i, size, type, 0);
		//printk(KERN_ALERT"IK_RESTORE(%d): o_addr(0x%x) r_addr(0x%x) raf(0x%x) waf(0x%x) size_type(%zu | %zu)  \n",i,
		//		(u32)o_addr, (u32)r_addr, (u32)raf, (u32)waf, size, type);


		count = 0;
		start_pos = 0;
		end_pos = 0;
		while(!(raf[count] && waf[count])){
			raf[count] = 0;
			waf[count] = 0;
			count++;
			if(count==size)
				break;
		}
		start_pos = count;

		while(count<size){
			while(raf[count] && waf[count]){
				raf[count] = 0;
				waf[count] = 0;
				count++;
				if(count==size)
					break;
			}
			end_pos = count;

			if(end_pos-start_pos){
				memcpy_neon(o_addr+start_pos*type, r_addr+start_pos*type, (end_pos-start_pos)*type);
				//trace_gpu_custom_ik("++++IK RESTORE++++:memcpy_nano", ctx->tInfo.ctx_id, katom->tInfo.atom_id, 
				//	(u64)raf[count-1], (u64)waf[count-1], 0, 0, 0, start_pos, end_pos, end_pos-start_pos, count);
				//printk(KERN_ALERT"RESTORE: start(%zu)~end(%zu)-1| (size:%zu), (curr_count:%zu) (%x, %x)\n", 
				//		start_pos, end_pos, end_pos-start_pos, count, raf[count-1], waf[count-1]);
			}

			while(!(raf[count] && waf[count]) && count<size){
				raf[count] = 0;
				waf[count] = 0;
				count++;
				if(count==size)
					break;
			}
			start_pos = count;
		}
		//trace_gpu_custom_ik("++++IK RESTORE++++:end(1)", ctx->tInfo.ctx_id, katom->tInfo.atom_id, 
		//		(u64)(u32)o_addr, (u64)(u32)r_addr, (u64)(u32)raf, (u64)(u32)waf, rop_buf->addr, (u64)sizeof(u64), (u64)(sizeof(u64)*4), (u64)(size*(type+2)), (u64)(sizeof(u64)*4+size*(type+2)));
		//trace_gpu_custom_ik("++++IK RESTORE++++:end(2)", ctx->tInfo.ctx_id, katom->tInfo.atom_id, 
		//		(u64)(u32)o_addr, (u64)(u32)r_addr, (u64)(u32)raf, (u64)(u32)waf, rop_buf->addr, (u64)sizeof(u64), (u64)size*type, (u64)sizeof(unsigned long long), (u64)0);
		__cpuc_flush_dcache_area(o_addr, size*type-1);
		__cpuc_flush_dcache_area(r_addr, sizeof(u64)*4+size*(type+2));
	}
}

struct kbase_context * proc_ikernel_pid_search(pid_t pid){ 
 
	struct kbase_context *sctx=NULL;

	list_for_each_entry(sctx, &proc_ikernel_kbdev->proc_ctx_list, proc_ctx_list_node){
		if(sctx->pid == pid)
			return sctx;
	}
	return NULL;
}

struct kbase_context *proc_ikernel_fd_search(struct file *fd, int mod){
	unsigned long flags;
	struct kbase_context *sctx=NULL;
	spin_lock_irqsave(&ikernel_ctx_list_lock, flags);
	list_for_each_entry(sctx, &ikernel_list_head, ik_ctx_info.ik_ctx_list){
		//trace_gpu_custom_ik("++++iKernel FD Search++++", sctx->tInfo.ctx_id, 0, 0, 0, 0, 0, 0, (u32)fd, (u32)(sctx->ik_ctx_info.fd), 0, 0);
		if(sctx->ik_ctx_info.fd == fd){
			switch(mod){
				case 0: //delette
					list_del(&sctx->ik_ctx_info.ik_ctx_list);
					break;
				case 1: //write
					break;
			}
			spin_unlock_irqrestore(&ikernel_ctx_list_lock, flags);
			return sctx;
		}
	}
	spin_unlock_irqrestore(&ikernel_ctx_list_lock, flags);
	return NULL;
}

int delete_ikernel_ctx(struct file* fd){
	int ret = 1;
	struct kbase_context *ctx;
	if((ctx = proc_ikernel_fd_search(fd, 0))!=NULL){
		//trace_gpu_custom_ik("++++iKernel Context Destory++++", ctx->tInfo.ctx_id, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		destroy_ikernel_buffer(ctx);
		ret = 0;
	}

	return ret;
}

static int mali_ikernel_open(struct inode *inode, struct file *filp) 
{
	unsigned long flags;
	struct kbase_context *ctx;
	

	ctx = proc_ikernel_pid_search(current->pid);
	if(ctx){
		spin_lock_irqsave(&ikernel_ctx_list_lock, flags);
		list_add(&ctx->ik_ctx_info.ik_ctx_list, &ikernel_list_head);
		spin_unlock_irqrestore(&ikernel_ctx_list_lock, flags);

		ctx->ik_ctx_info.fd = filp;
		ctx->ik_ctx_info.is_ik = 1;
		//trace_gpu_custom_ik("++++iKernel Open++++", ctx->tInfo.ctx_id, 0, 0, 0, 0, 0, 0, ctx->pid, (u32)filp, 0, 0);
	}else{
		//trace_gpu_custom_ik("++++iKernel Open++++ : Can't be found", 0, 0, 0, 0, 0, 0, 0, current->pid, 0, 0, 0);
	}
	
	return 0;                                                                       
}
int mali_ikernel_release(struct inode * inode, struct file *filp){

	int ret;
	//trace_gpu_custom_ik("++++iKernel Release++++", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

	ret = delete_ikernel_ctx(filp);

	return ret;
}

                             
ssize_t mali_ikernel_write(struct file *f, const char __user *buffer, size_t count, loff_t * data){
	kbase_context *ctx;

	ctx = proc_ikernel_fd_search(f, 1);
	if(ctx){
		//trace_gpu_custom_ik("iKernel Proc Write", ctx->tInfo.ctx_id, 0,
		//		1, 2, 3, 4, 5, 
		//		6, 7, 8, 9);
		u8 cmd = (*(u8 *)buffer)>>3;
		if(cmd){
			printk(KERN_ALERT"ctx cmd: %u, %u\n", *(u8 *)buffer, (*(u8 *)buffer)&7);
			ctx->ik_ctx_info.is_ik = (*(u8 *)buffer)&7;

		}else{
			printk(KERN_ALERT"kernel cmd: %u, %u\n", *(u8 *)buffer, (*(u8 *)buffer)&7);
			ikernel_create(ctx, (*(u8 *)buffer)&7);
		}
	}else{
		//trace_gpu_custom_ik("iKernel Proc Write - Not found ctx!!", 0, 0,
		//		0, 0, 0, 0, 0, 
		//		0, 0, 0, 0);
	}
	return count;
}

static const struct file_operations mali_ikernel_proc_op ={
	.open = mali_ikernel_open,
	.write = mali_ikernel_write,
	.release = mali_ikernel_release,
};

int kbase_ikernel_proc_init(struct kbase_device *kbdev){ 
 
	if(!proc_create_data("mali_ikernel",S_IRWXU,NULL, &mali_ikernel_proc_op, kbdev))
		return 1;

	proc_ikernel_kbdev = kbdev;

	spin_lock_init(&ikernel_ctx_list_lock);

	INIT_LIST_HEAD(&ikernel_list_head);

	return 0; 
}

void kbase_ikernel_proc_exit(void){
	remove_proc_entry("mali_ikernel", NULL);
}
#endif
