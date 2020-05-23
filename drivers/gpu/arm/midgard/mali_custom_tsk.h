#define _TSK_CUSTOM_PRINT_

/*
#define _TSK_TRACE_EVAL_
//#define _TSK_TRACE_PER_KERNEL_
//#define _TSK_TRACE_EVICTION_
*/

#ifdef CONFIG_GPU_TRACEPOINTS

#define _TSK_CUSTOM_TRACE_
#ifdef _TSK_CUSTOM_TRACE_
#include <mali_custom_eval.h>
#endif

#endif

#define _TSK_CUSTOM_SNAP_
#ifdef _TSK_CUSTOM_SNAP_
#include <mali_custom_snap.h>
#endif

#define _TSK_CUSTOM_SCHED_
#ifdef _TSK_CUSTOM_SCHED_
#include <mali_custom_sched.h>
#endif

#define _TSK_CUSTOM_PROC_
#ifdef _TSK_CUSTOM_PROC_
#include <mali_custom_proc.h>
#endif

#define _TSK_CUSTOM_IOCTL_
#ifdef _TSK_CUSTOM_IOCTL_ 
#include <mali_custom_ioctl.h>
#endif
