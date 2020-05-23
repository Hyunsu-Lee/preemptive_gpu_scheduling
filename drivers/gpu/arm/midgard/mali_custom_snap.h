#define KTHREAD_CORE_BASE (4)
#define NR_SNAPSHOT_KTHREAD (4)
#define BASE_SNAP_GRANULARITY (1280)

typedef struct kernel_live_context_param{
    struct list_head param_node;
    struct kbase_va_region* param[20];
    u32 nr_store, nr_restore;
    u32 nr_store_pages[20];
    u32 nr_restore_pages[20];
}kernel_lparam;


typedef struct custom_dump_region{
    struct list_head dreg_node;

    struct kbase_context *kctx;
    struct kbase_va_region *reg;
        
    size_t nr_pages;
    struct page **sp;
    struct page **dp;
    void *ksp;
    void *kdp;
    u8 is_vmalloc;

}custom_dump_region;

typedef struct snapshot_kthread_context{

    int id;
    struct kbase_device *kbdev;
    struct kbase_jd_atom *katom;
    struct task_struct *snapshot_kthread;
    wait_queue_head_t snap_wqueue;

    u8 curr_pos, head_pos;
    atomic_t nr_snap_atom;
    struct kbase_jd_atom *snap_atom_list[10];

	spinlock_t sched_lock, snap_thread_lock;

}snapshot_kthread_context;

typedef struct snapshot_kthreads{
    u32 skthread_head;
    snapshot_kthread_context kthread_ctx[NR_SNAPSHOT_KTHREAD];

}snapshot_kthreads;


void init_snapshot_ctx(struct kbase_device *kbdev);
void change_snapshot_thread(struct kbase_device *kbdev);

void submit_snapshot(int cpu, struct snapshot_kthread_context* sctx);
int snapshot_kthread(void *param);
void create_snapthread(struct kbase_device *kbdev, int id);
void destroy_snapthread(struct kbase_device *kbdev,int id);
void snapshot_store(struct kbase_jd_atom* katom);
void snapshot_restore(struct kbase_jd_atom* katom);
