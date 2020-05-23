#define LIMIT_SUBMITTED_NR (2)

#define SCHED_RT_PRIORITY (-19)

//stat
#define SCHED_STAT_INIT (0UL)     //(0<<0)
#define SCHED_STAT_POOL (1UL)     //(1<<0)
#define SCHED_STAT_SLOT (2UL)     //(1<<1)
#define SCHED_STAT_RMASK (3UL)    //((1<<0)|(1<<1))=(SCHED_STAT_POOL|SCHED_STAT_SLOT)


#define SCHED_STAT_SNAP (4UL)     //(1<<2)
#define SCHED_STAT_RUN  (8UL)     //(1<<3)
#define SCHED_STAT_DONE (16UL)    //(1<<4)
#define SCHED_STAT_RERUN (32UL)   //(1<<5)
#define SCHED_STAT_SMASK (12UL)   //((1<<2)|(1<<3))=(|SCHED_STAT_SNAP|SCHED_STAT_RUN)
#define SCHED_STAT_SLOT_READY (0UL)
//#define SCHED_STAT_RMASK (28)   //((1<<2)|(1<<3)|(1<<4))=(SCHED_STAT_SNAP|SCHED_STAT_RUN|SCHED_STAT_DONE)

#define SCHED_STAT_PSLOT (64UL)   //(1<<6)
#define SCHED_STAT_PSNAP (128UL)  //(1<<7)
#define SCHED_STAT_PRUN (256UL)   //(1<<8)
#define SCHED_STAT_PMASK (448UL)  //((1<<6)|(1<<7)|(1<<8))=(SCHED_STAT_PSLOT|SCHED_STAT_PSNAP|SCHED_STAT_PRUN)

#define SCHED_STAT_PCER (512UL)  //(1<<9)
#define SCHED_STAT_PSCED (1024UL) //(1<<10)
#define SCHED_STAT_WAKE (2048UL) //(1<<11)

//bit
#define SCHED_STAT_POOL_BIT (0)
#define SCHED_STAT_SLOT_BIT (1)

#define SCHED_STAT_SNAP_BIT (2)
#define SCHED_STAT_RUN_BIT  (3)
#define SCHED_STAT_DONE_BIT (4)
#define SCHED_STAT_RERUN_BIT (5)

#define SCHED_STAT_PSLOT_BIT (6)
#define SCHED_STAT_PSNAP_BIT (7)
#define SCHED_STAT_PRUN_BIT (8)

#define SCHED_STAT_PSCER_BIT (9)
#define SCHED_STAT_PSCED_BIT (10)
#define SCHED_STAT_WAKE_BIT (11)



void sc_hw_submit(kbase_device *kbdev, kbase_jd_atom *katom, int js);
int sc_preempt(struct kbase_jd_atom *katom);
u32 sc_active_jobs(kbase_jm_slot *slot);
u8 sc_is_preempt(kbase_jm_slot *slot);
u8 sc_resched(struct kbase_jd_atom *katom);

