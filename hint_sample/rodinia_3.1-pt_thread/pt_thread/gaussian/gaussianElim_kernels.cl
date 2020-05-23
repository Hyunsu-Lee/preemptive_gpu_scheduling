//#pragma OPENCL EXTENSION cl_khr_byte_addressable_store : enable

#ifndef IKERN_GPU
#define IKERN_GPU
#include "ik_2.h"
#endif

typedef struct latLong
    {
        float lat;
        float lng;
    } LatLong;

__kernel void Fan1(__global float *m_dev,
                  __global float *a_dev,
                  __global float *b_dev,
                  const int size,
                  const int t,
                  IKERN_PT_PARAM()
                  ) {
    INIT_PT_IKERN()
    PT_IKERN_START(){

    int globalId = PT_BT_ID_X;
    //int globalId = get_global_id(0);
                              
    if (globalId < size-1-t) {
         *(m_dev + size * (globalId + t + 1)+t) = *(a_dev + size * (globalId + t + 1) + t) / *(a_dev + size * t + t);    
    }
    
    PT_IKERN_NEXT_BT()
    PT_IKERN_PREEMPT()
    }PT_IKERN_END()

}


__kernel void Fan2(__global float *m_dev,
                  __global float *a_dev,
                  __global float *b_dev,
                  const int size,
                  const int t,
                  IKERN_PT_PARAM()
                  ) {
     INIT_PT_IKERN()
     PT_IKERN_START(){

	 int globalId = PT_BT_ID_X;
	 //int globalId = get_global_id(0);
	 
	 int globalIdx = PT_BT_ID_X;
	 int globalIdy = PT_BT_ID_Y;
	 //int globalIdx = get_global_id(0);
	 //int globalIdy = get_global_id(1);
      if (globalIdx < size-1-t && globalIdy < size-t) {
         a_dev[size*(globalIdx+1+t)+(globalIdy+t)] -= m_dev[size*(globalIdx+1+t)+t] * a_dev[size*t+(globalIdy+t)];
 	 
 	    if(globalIdy == 0){
 		   b_dev[globalIdx+1+t] -= m_dev[size*(globalIdx+1+t)+(globalIdy+t)] * b_dev[t];
 	    }
 	 }
    
     PT_IKERN_NEXT_BT()
     PT_IKERN_PREEMPT()
     }PT_IKERN_END()

//   One dimensional
// 	 int globalIdx = globalId % size;
// 	 int globalIdy = globalId / size;
// 	 
// 	 if (globalIdx < size-1-t && globalIdy < size-t) {
//          a_dev[size*(globalIdx+1+t)+(globalIdy+t)] -= m_dev[size*(globalIdx+1+t)+t] * a_dev[size*t+(globalIdy+t)];
// 	 }
// 	 if(globalIdy == 0){
//  		   b_dev[globalIdx+1+t] -= m_dev[size*(globalIdx+1+t)+(globalIdy+t)] * b_dev[t];
//      }
}
