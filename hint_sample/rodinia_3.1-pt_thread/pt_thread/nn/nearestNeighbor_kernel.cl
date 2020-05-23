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

__kernel void NearestNeighbor(__global LatLong *d_locations,
							  __global float *d_distances,
							  const int numRecords,
							  const float lat,
							  const float lng
                              ,IKERN_PT_PARAM()
                              ) {

     INIT_PT_IKERN()
     PT_IKERN_START(){

	 int globalId = PT_GT_ID;//get_global_id(0);
	 //int globalId = get_global_id(0);
							  
     if (globalId < numRecords) {
         __global LatLong *latLong = d_locations+globalId;
    
         __global float *dist=d_distances+globalId;
         *dist = (float)sqrt((lat-latLong->lat)*(lat-latLong->lat)+(lng-latLong->lng)*(lng-latLong->lng));
	 }

     PT_IKERN_NEXT_BT()
     PT_IKERN_PREEMPT()
     }PT_IKERN_END()

}
