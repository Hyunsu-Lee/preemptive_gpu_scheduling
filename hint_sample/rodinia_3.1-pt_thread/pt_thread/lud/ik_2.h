
#define PT_WG_SIZE 128
#define NR_SHADER_CORE 4
#define PT_TOTAL_SIZE_LIMIT (PT_WG_SIZE*NR_SHADER_CORE)
//For debugging
//Minimum 2
#define PT_INFO_SIZE (4)

#ifdef IKERN_GPU
/* GPU Macro */

#define IKERN_PARAM(var) \
    __global unsigned int* var##_rop

#define RAF_ROP(index, var) \
	var##_raf[index] |= !var##_waf[index];

#define WAF_ROP(index, var) \
	var##_waf[index] = 255;

#define REPLACE_ROP(index_a, index, var) \
	condition = !(var##_raf[index] && var##_waf[index]); \
    var##_a[index_a] = (typeof(var))((unsigned long)var*condition + (unsigned long)var##_r*!condition); 

#define RWAF_ROP(index_a, index, var) \
	var##_r[index] = var[index]; \
	RAF_ROP(index, var) \
	WAF_ROP(index, var) \
	var##_a[index_a] = var##_r;

#define INIT_IKERN0() \
	unsigned char condition; 

#define INIT_IKERN(size, var) \
    __global unsigned long *var##_rop_meta = (__global unsigned long*)var##_rop; \
    unsigned long var##_buf_size = var##_rop_meta[0]; \
    var##_rop_meta[2] = (unsigned long)var##_rop; \
    var##_rop_meta[3] = (unsigned long)var; \
    typeof(var) var##_r = (typeof(var))((__global unsigned long*)var##_rop+4); \
    __global unsigned char* var##_raf = (__global unsigned char*)(var##_r+var##_buf_size); \
    __global unsigned char *var##_waf = (var##_raf+var##_buf_size); \
    typeof(var) var##_a[size]; \
	for(int var##_i = 0; var##_i<size; var##_i++){ \
		var##_a[var##_i] = var; \
    }  

/* For supporting persistent kernel */
#define IKERN_PT_PARAM() \
    __global int* pt_info, \
    int nr_pt, \
    unsigned int preempt_stat, \
    unsigned int pt_ng_x, \
    unsigned int pt_ng_y, \
    unsigned int pt_ng_z, \
    unsigned int pt_nl_x, \
    unsigned int pt_nl_y, \
    unsigned int pt_nl_z

#define INIT_PT_IKERN() \
    int pt_wg_id = get_group_id(0); \
    int pt_lt_gid = get_local_id(0); \
    if(!pt_info[pt_wg_id*PT_INFO_SIZE]){ \
        if(pt_lt_gid==0){ \
            int nr_bt = 0; \
            if(nr_pt>PT_TOTAL_SIZE_LIMIT){ \
                int nr_rpt = (nr_pt-PT_TOTAL_SIZE_LIMIT)/PT_WG_SIZE; \
                nr_bt = nr_rpt/NR_SHADER_CORE; \
                if(pt_wg_id+1<= nr_rpt%NR_SHADER_CORE){ \
                    nr_bt++; \
                } \
            } \
            pt_info[pt_wg_id*PT_INFO_SIZE] = nr_bt+1; \
        } \
        barrier(CLK_GLOBAL_MEM_FENCE); \
    }

// Replacement define
// 여기는 다시 수정할 것.dimension 구현 결정 이후에.
#define PT_GT_ID (PT_WG_SIZE*PT_BT_ID+pt_lt_gid)

#define PT_WG_ID (pt_wg_id)
#define PT_BT_ID (pt_info[PT_WG_ID*PT_INFO_SIZE+1]*NR_SHADER_CORE+pt_wg_id)
#define PT_BT_ID_X (PT_BT_ID % (pt_ng_x / pt_nl_x))
#define PT_BT_ID_Y (PT_BT_ID / (pt_ng_x / pt_nl_x))
#define PT_BT_ID_Z (PT_BT_ID / ((pt_ng_x/pt_nl_x) * (pt_ng_y/pt_nl_y)))


#define PT_LT_ID (pt_lt_gid)
#define PT_LT_ID_X (PT_LT_ID % pt_nl_x)
#define PT_LT_ID_Y (PT_LT_ID / pt_nl_x)
#define PT_LT_ID_Z (PT_LT_ID / (pt_nl_x * pt_nl_y))


#define PT_IKERN_START() \
    while(pt_info[pt_wg_id*PT_INFO_SIZE+1] != pt_info[pt_wg_id*PT_INFO_SIZE])

#define PT_IKERN_NEXT_BT() \
    if(pt_lt_gid==0){ \
            pt_info[pt_wg_id*PT_INFO_SIZE+1]++; \
    } \
    barrier(CLK_GLOBAL_MEM_FENCE); \

#define PT_IKERN_PREEMPT() \
    if(preempt_stat > 0) \
        return;

#define PT_IKERN_END() \
    if(pt_lt_gid==0){ \
        pt_info[pt_wg_id*PT_INFO_SIZE] = 0; \
        pt_info[pt_wg_id*PT_INFO_SIZE+1] = 0; \
    } \
    barrier(CLK_GLOBAL_MEM_FENCE);
        
        //pt_info[pt_wg_id*PT_INFO_SIZE+2] = pt_info[pt_wg_id*PT_INFO_SIZE]; \
        pt_info[pt_wg_id*PT_INFO_SIZE+3] = pt_info[pt_wg_id*PT_INFO_SIZE+1]; \

#endif

#ifdef IKERN_HOST
/* Host Macro */
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>

#define NON_IK_KERNEL (0)
#define IK_KERNEL (1)

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

extern FILE *ik_fp;

#define INIT_IKERNEL_HOST() \
    FILE *ik_fp;

inline void ikernel_hint(unsigned char ik){
    unsigned char is_ik = ik | 1<<3;

    ik_fp = fopen("/proc/mali_ikernel", "wr");
    
    fwrite((void *)&is_ik, sizeof(unsigned char), sizeof(is_ik), ik_fp);
    fflush(ik_fp);
}

inline void ikernel_set_command(unsigned char ik){

    unsigned char is_ik = ik;

    fwrite((void *)&is_ik, sizeof(unsigned char), sizeof(is_ik), ik_fp);
    fflush(ik_fp);
}

inline void ikernel_hint_close()
{
    fclose(ik_fp);
}

#define INIT_ROP_HOST() \
    static cl_ulong *rop_pin; \

#define CREATE_ROP_BUFFER(context, command, size, type, var) \
    static size_t var##_rop_size = sizeof(cl_ulong)*4+size*sizeof(type)+size*2; \
    cl_mem var##_rop = clCreateBuffer(context, CL_MEM_READ_WRITE|CL_MEM_ALLOC_HOST_PTR, \
        var##_rop_size, NULL, NULL); \
    rop_pin = (cl_ulong *)clEnqueueMapBuffer(command, var##_rop, CL_TRUE, CL_MAP_WRITE, \
            0, sizeof(cl_ulong)*4, 0, NULL, NULL, NULL);\
    rop_pin[0] = size; \
    rop_pin[1] = sizeof(type); \
    rop_pin[2] = 0; \
    rop_pin[3] = 0; \
    clEnqueueUnmapMemObject(command, var##_rop, rop_pin, 0, NULL, NULL);

#define DESTROY_ROP_BUFFER(var) \
    clReleaseMemObject(var##_rop); 

#define SET_ROP_ARG(kernel, idx, var) \
    clSetKernelArg(kernel, idx, sizeof(cl_mem), (void *)&var##_rop); 

// For supporting persistent thread

#define INIT_PT_VARIABLE() \
    cl_int *pt_info_p = NULL; \
    cl_mem pt_info = NULL; 


#define CREATE_PT_BUFFER(context, command) \
    if(!pt_info){ \
        pt_info = clCreateBuffer(context, CL_MEM_READ_WRITE|CL_MEM_ALLOC_HOST_PTR, \
            sizeof(cl_int)*NR_SHADER_CORE*PT_INFO_SIZE, NULL, NULL); \
        pt_info_p = (cl_int*)clEnqueueMapBuffer(command, pt_info, CL_TRUE, CL_MAP_WRITE, 0, \
                sizeof(cl_int)*NR_SHADER_CORE*PT_INFO_SIZE, 0, NULL, NULL, NULL); \
        for(int i=0;i<NR_SHADER_CORE;i++) \
            for(int j=0;j<PT_INFO_SIZE;j++) \
                pt_info_p[i*NR_SHADER_CORE+j] = 0; \
        clEnqueueUnmapMemObject(command, pt_info, pt_info_p, 0, NULL, NULL);\
    }

#define PT_BUFFER_DEBUG(command) \
    pt_info_p = (cl_int*)clEnqueueMapBuffer(command, pt_info, CL_TRUE, CL_MAP_WRITE, 0, \
            sizeof(cl_int)*NR_SHADER_CORE*PT_INFO_SIZE, 0, NULL, NULL, NULL); \
    printf("%d, %d, %d, %d\n", pt_info_p[0], pt_info_p[1], pt_info_p[2], pt_info_p[3]); \
    clEnqueueUnmapMemObject(command, pt_info, pt_info_p, 0, NULL, NULL);

#define INIT_PT_WORK_ITEM() \
    int nr_pt = 1, nr_lpt = 1; \
    int gdimension, ldimension; \
    size_t pt_gwork[3]={1,}, pt_lwork[3]={1,}; \
    size_t pt_work_info[2][3] = {{1,},{1,}}; \
    unsigned int preempt_stat = 0xF0F0F0F0; 
    
//unsigned int preempt_stat = 0xF0F0F0F0; 

#define SET_PT_WORK_ITEM(gvar, lvar) \
    nr_pt = 1; nr_lpt = 1; \
    gdimension = sizeof(gvar)/sizeof(size_t); \
    for(int i=0;i<gdimension;i++){ \
        nr_pt *= *(gvar+i); \
        pt_work_info[0][i] = *(gvar+i); \
    } \
    ldimension = sizeof(lvar)/sizeof(size_t); \
    for(int i=0;i<ldimension;i++){ \
        nr_lpt *= *(lvar+i); \
        pt_work_info[1][i] = *(lvar+i); \
    }\
    pt_gwork[0] = nr_pt; \
    pt_lwork[0] = nr_lpt; \
    if(nr_pt>PT_TOTAL_SIZE_LIMIT) \
        pt_gwork[0] = PT_TOTAL_SIZE_LIMIT;

#define PT_WORK_DIM (1)
#define PT_GWORK (pt_gwork)
#define PT_LWORK (pt_lwork)

#define SET_PT_ARG(kernel, idx) \
    clSetKernelArg(kernel, idx, sizeof(cl_mem), (void *)&pt_info); \
    clSetKernelArg(kernel, idx+1, sizeof(cl_int), (void *)&nr_pt); \
    clSetKernelArg(kernel, idx+2, sizeof(cl_uint), (void *)&preempt_stat); \
    clSetKernelArg(kernel, idx+3, sizeof(cl_uint), (void *)&pt_work_info[0][0]); \
    clSetKernelArg(kernel, idx+4, sizeof(cl_uint), (void *)&pt_work_info[0][1]); \
    clSetKernelArg(kernel, idx+5, sizeof(cl_uint), (void *)&pt_work_info[0][2]); \
    clSetKernelArg(kernel, idx+6, sizeof(cl_uint), (void *)&pt_work_info[1][0]); \
    clSetKernelArg(kernel, idx+7, sizeof(cl_uint), (void *)&pt_work_info[1][1]); \
    clSetKernelArg(kernel, idx+8, sizeof(cl_uint), (void *)&pt_work_info[1][2]);


#define DESTROY_PT_BUFFER() \
    clReleaseMemObject(pt_info); 
#endif
