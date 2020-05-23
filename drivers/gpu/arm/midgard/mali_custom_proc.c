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


#ifdef _TSK_CUSTOM_PROC_

struct kbase_device *proc_kbdev;
struct kbase_context *proc_kctx;

struct kbase_context * proc_ctx_search(long proc_ctx_id){

	int is_find=0;
	struct kbase_context *sctx=NULL;

	list_for_each_entry(sctx,&proc_kbdev->proc_ctx_list, proc_ctx_list_node){
		if(sctx->tInfo.ctx_id == (u32)proc_ctx_id){
			is_find = 1;                 
			break;                       
		}
	}

	if(!is_find){
		printk(KERN_ALERT"<gpu> proc_ctx_id(%ld) can't be found!\n", proc_ctx_id);
		return NULL;
	}
	return sctx;
	
}

static void *s_start(struct seq_file *m, loff_t *pos){

	int fcount=0;
	struct kbase_va_region *reg;
	struct kbase_cpu_mapping *map;
	struct rb_node *node = NULL;
	struct rb_root *rbroot = NULL;
	size_t s;

	if(proc_kctx==NULL){
		printk(KERN_ALERT"<gpu> proc_ctx_id can't be found!\n");
		return NULL;
	}
	if(*pos == 0){ 
		seq_printf(m,"|   CTX ID    : %05u            |\n", proc_kctx->tInfo.ctx_id);
	}

	rbroot = &proc_kctx->reg_rbtree;

	for(node = rb_first(rbroot) ; node ; node = rb_next(node)){  
		reg = rb_entry(node, struct kbase_va_region, rblink);
		s = kbase_reg_current_backed_size(reg);              
		if(s){
			if(fcount == *pos){
				seq_printf(m,"==================================\n");
				seq_printf(m,"|   region    :    %05d - %05zu |\n", reg->tInfo.reg_id, reg->alloc->nents);
				seq_printf(m,"|   PFN       : %16llx |\n", reg->start_pfn);
				map = list_entry(&reg->alloc->mappings, kbase_cpu_mapping, mappings_list);
				seq_printf(m,"|   MAP       : %16lx |\n", map->vm_start);
				if ((reg->flags & KBASE_REG_ZONE_MASK) == KBASE_REG_ZONE_SAME_VA){
					seq_printf(m,"|   type      :     ZONE_SAME_VA |\n");
				}else if ((reg->flags & KBASE_REG_ZONE_MASK) == KBASE_REG_ZONE_EXEC){
					seq_printf(m,"|   type      :     ZONE_EXEC_VA |\n");
				}else if((reg->flags & KBASE_REG_ZONE_MASK) == KBASE_REG_ZONE_CUSTOM_VA){
					seq_printf(m,"|   type      :     ZONE_CUST_VA |\n");
				}

				return reg;
			}
			fcount++;
		}
	}

	return NULL;
}

static void *s_next(struct seq_file *m, void *p, loff_t *pos){
	return NULL;
}

static void s_stop(struct seq_file *m, void *p){
}                                               


static int s_show(struct seq_file *m, void *p){
	struct kbase_va_region *reg;
	void *mapping;
	u32 *mapping_32;
	int i,j;

	//tmp
	u32 nr_limited_pages;

	reg = (struct kbase_va_region*)p;

	if(reg->alloc->nents < 10){
		nr_limited_pages = reg->alloc->nents; 
	}else{
		nr_limited_pages = 10;
	}
	for(i=0;i<nr_limited_pages;i++){//reg->alloc->nents
		mapping = kmap_atomic(pfn_to_page(PFN_DOWN(reg->alloc->pages[i])));
		mapping_32 = mapping;
		seq_printf(m, "==================================\n");
		seq_printf(m, "| page number : %05d            |\n", i);
		seq_printf(m, "==================================\n");
		for(j=0;j<1024;j++){
			
			seq_printf(m, "|%04x| 0x%08x ", j*4 ,*(mapping_32+j));
			
			if(((j+1)%10)==0)            
				seq_printf(m, "|\n");
		}
		seq_printf(m, "\n\n");
		__kunmap_atomic(mapping);
	}
	return 0;
}


static const struct seq_operations mali_mem_seq_op = {
	.start = s_start, 
	.next = s_next,
	.stop = s_stop,
	.show = s_show,
};                                                    


static int mali_mem_seq_open(struct inode *inode, struct file *filp)
{
	return seq_open(filp, &mali_mem_seq_op);                    
}

ssize_t mali_mem_write(struct file *f, const char __user *buffer, size_t count, loff_t * data){

	//struct kbase_device *kbdev = (struct kbase_device *)data;
	proc_kctx = proc_ctx_search(simple_strtol(buffer,NULL, 10));
	if(proc_kctx!=NULL)
		printk(KERN_ALERT"<gpu> select proc_ctx_id : %u\n", proc_kctx->tInfo.ctx_id);
	return count;
}

static const struct file_operations mali_mem_proc_op ={
	.open = mali_mem_seq_open,
	.read = seq_read,
	.write = mali_mem_write,
	.llseek = seq_lseek,
	.release = seq_release,
};

int kbase_mem_proc_init(struct kbase_device *kbdev){
	if(!proc_create_data("mali_mem_dump",S_IRWXU,NULL, &mali_mem_proc_op, kbdev))
		return 1;
	proc_kbdev = kbdev;
	proc_kctx = NULL;
	INIT_LIST_HEAD(&kbdev->proc_ctx_list);
	return 0; 
}
void kbase_mem_proc_exit(void){ 
	remove_proc_entry("mali_mem_dump", NULL);
}
#endif
