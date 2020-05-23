#ifndef IKERN_GPU
#define IKERN_GPU
#include "ik_2.h"
#endif

__kernel void hotspotOpt1(__global float *p, __global float* tIn, __global float *tOut, float sdc,
                            int nx, int ny, int nz,
                            float ce, float cw, 
                            float cn, float cs,
                            float ct, float cb, 
                            float cc,
                            IKERN_PT_PARAM()
                            ) 
{

  INIT_PT_IKERN()
  PT_IKERN_START(){

  float amb_temp = 80.0;

  int i = PT_BT_ID_X;
  int j = PT_BT_ID_Y;
  //int i = get_global_id(0);
  //int j = get_global_id(1);
  int c = i + j * nx;
  int xy = nx * ny;

  int W = (i == 0)        ? c : c - 1;
  int E = (i == nx-1)     ? c : c + 1;
  int N = (j == 0)        ? c : c - nx;
  int S = (j == ny-1)     ? c : c + nx;

  float temp1, temp2, temp3;
  temp1 = temp2 = tIn[c];
  temp3 = tIn[c+xy];
  tOut[c] = cc * temp2 + cw * tIn[W] + ce * tIn[E] + cs * tIn[S]
    + cn * tIn[N] + cb * temp1 + ct * temp3 + sdc * p[c] + ct * amb_temp;
  c += xy;
  W += xy;
  E += xy;
  N += xy;
  S += xy;

  for (int k = 1; k < nz-1; ++k) {
      temp1 = temp2;
      temp2 = temp3;
      temp3 = tIn[c+xy];
      tOut[c] = cc * temp2 + cw * tIn[W] + ce * tIn[E] + cs * tIn[S]
        + cn * tIn[N] + cb * temp1 + ct * temp3 + sdc * p[c] + ct * amb_temp;
      c += xy;
      W += xy;
      E += xy;
      N += xy;
      S += xy;
  }
  temp1 = temp2;
  temp2 = temp3;
  tOut[c] = cc * temp2 + cw * tIn[W] + ce * tIn[E] + cs * tIn[S]
    + cn * tIn[N] + cb * temp1 + ct * temp3 + sdc * p[c] + ct * amb_temp;
  
  PT_IKERN_NEXT_BT()
  PT_IKERN_PREEMPT()
  }PT_IKERN_END()

  return;
}


