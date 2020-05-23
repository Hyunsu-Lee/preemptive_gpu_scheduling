#define IOCTL_GPU_MAGIC (100)

#define IOCTL_0 IOWR (IOCTL_GPU_MAGIC, 0, gpu_umsg )
#define IOCTL_1 IOWR (IOCTL_GPU_MAGIC, 1, gpu_umsg )
#define IOCTL_2 IOWR (IOCTL_GPU_MAGIC, 2, gpu_umsg )
#define IOCTL_3 IOWR (IOCTL_GPU_MAGIC, 3, gpu_umsg )
#define IOCTL_4 IOWR (IOCTL_GPU_MAGIC, 4, gpu_umsg )
#define IOCTL_5 IOWR (IOCTL_GPU_MAGIC, 5, gpu_umsg )
#define IOCTL_6 IOWR (IOCTL_GPU_MAGIC, 6, gpu_umsg )
#define IOCTL_7 IOWR (IOCTL_GPU_MAGIC, 7, gpu_umsg )
#define IOCTL_8 IOWR (IOCTL_GPU_MAGIC, 8, gpu_umsg )
#define IOCTL_9 IOWR (IOCTL_GPU_MAGIC, 9, gpu_umsg )

#define IOCTL_GPU_MAXNR 10

typedef struct{
    u32 ctx_id;
    u64 atom_id;
    u64 data1;
    u64 data2;
}__attribute__((packed)) gpu_umsg;


void kbase_custom_ioctl(struct kbase_context *kctx, unsigned int cmd, unsigned long arg);
