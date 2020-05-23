#define IKERNEL_KTHREAD_CORE_BASE (6)
#define NR_IKERNEL_KTHREAD (2)

#define KI_NK (0)
#define KI_IK (1)

#define NOT_IK_CTX (0)
#define IK_CTX_PARTAIL (1)
#define IK_CTX_F_NTRANS (2)
#define IK_CTX_F_NTRANS_EVAL (3)
#define IK_CTX_F_TRANS (4)
#define IK_CTX_PT (5)

#define NOT_IK_ATOM (0)
#define IK_ATOM_NTRANS (1)
#define IK_ATOM_TRANS (2)
#define IK_ATOM_PT (3)


typedef struct ikernel_buffer ikernel_buffer;
typedef struct ikernel_info ikernel_info;

typedef struct ikernel_arg{    
    u32 size;
    u64 addr;
    void *omap;
    void *rmap;
    void *rafmap;
    void *wafmap;

    //unmap 후에 해제해야됨
}ikernel_arg;

//for Katom
typedef struct ikernel_arg_info{
    u64 ik_id;
    u8 is_ik;
    kbase_context *ctx;
	ikernel_info *ik_info;
	u8 nr_arg; //using in create kernel
    u64 arg[30];
    //For ROP Dep wait
    kbase_jd_atom *rop_dep_atom;
#ifdef _TSK_CUSTOM_PT_
    u32 pt_arg_idx;
    u32 *pt_arg_map;
#endif
}ikernel_arg_info;

typedef struct ikernel_info_arg{
    int arg_idx;
    ikernel_buffer *rop_buf;
    ikernel_buffer *origin_buf;
    u64 rop_addr;
    u64 origin_addr;

}ikernel_info_arg;

typedef struct ikernel_info{
    u8 flags; // 1: set, 0: no-set // 사용 안함?

    u8 is_ik;
    u32 ik_unique_id;
	u8 nr_ik_id;

	u32 ik_id[5];
    u8 nr_arg;
    ikernel_info_arg arg[10];

}ikernel_info;

typedef struct ikernel_reg_va{
    u64 start_addr;
    u64 page_num;
    void *page_va;

}ikernel_reg_va;

typedef struct ikernel_reg_info{
    struct list_head ik_reg_list;
    struct list_head ik_reg_buf_head;
    spinlock_t ik_reg_buf_lock;
}ikernel_reg_info;

typedef struct ikernel_buffer{
    u8 flags; //0=empty, 1=rop, 2=origin
    u64 addr;
    u64 offset;
    u64 page_idx;
    struct list_head ik_buf_list;
    struct list_head ik_reg_buf_list;
    kbase_va_region *reg;
    size_t size;
    size_t type;
    void *one_page_map;

    struct page **buffer_page;
    void *buffer_map;
    void *buffer_map_start;

}ikernel_buffer;

typedef struct ikernel_ctx_info{
    struct file *fd;
	u8 is_ik;
    struct list_head ik_ctx_list;
    spinlock_t ik_reg_list_lock;
    struct list_head ik_reg_list_head;
    struct list_head ik_reg_list_head_full; //보류

    u32 cur_offset;
    u8 nr_ik;
	ikernel_info ik_info[10];

    spinlock_t ik_buf_list_lock;
    struct list_head ik_buf_head;

}ikernel_ctx_info;

void init_ikernel_ctx(kbase_context *ctx);
int delete_ikernel_ctx(struct file *fd);

void init_ikernel_region(kbase_va_region *ik_reg);
void release_ikernel_region(kbase_va_region *ik_reg);
void add_ikernel_region(kbase_context *ctx, kbase_va_region *ik_reg);

u8 ikernel_submit_atom(kbase_jd_atom *katom);
void ikernel_classify_param(kbase_jd_atom *katom);
int ikernel_is_dep(kbase_jd_atom *first, kbase_jd_atom *second);
void release_ikernel_buffer_map(kbase_va_region *reg);

void ikernel_restore(kbase_jd_atom *katom);

int abort_ikernel_delete(struct file *fd);
int kbase_ikernel_proc_init(struct kbase_device *kbdev);
void kbase_ikernel_proc_exit(void);

#ifdef _TSK_CUSTOM_PT_
void init_ikernel_pt_info(kbase_va_region *reg);
void release_ikernel_pt_info(kbase_va_region *reg);
int get_ikernel_pt_map(kbase_jd_atom *katom);
void set_ikernel_pt_map(kbase_jd_atom *katom, u32 value);
#endif                                                   
