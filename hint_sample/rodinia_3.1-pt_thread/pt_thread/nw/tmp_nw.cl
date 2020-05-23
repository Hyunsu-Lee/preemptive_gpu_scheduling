#ifndef IKERN_GPU
#define IKERN_GPU
#include "ik_2.h"
#endif


#define SCORE(i, j) input_itemsets_l[j + i * (BLOCK_SIZE+1)]
#define REF(i, j)   reference_l[j + i * BLOCK_SIZE]

#define BLOCK_SIZE 128
#define BLOCK_SIZE_LIMIT (BLOCK_SIZE*4)

int maximum( int a,
		 int b,
		 int c){

	int k;
	if( a <= b )
		k = b;
	else 
	k = a;

	if( k <=c )
	return(c);
	else
	return(k);
}

__kernel void 
nw_kernel1(__global int  * reference_d, 
		   __global int  * input_itemsets_d, 
		   __global int  * output_itemsets_d, 
		   __local	int  * input_itemsets_l,
		   __local	int  * reference_l,
           int cols,
           int penalty,
           int blk,
           int block_width,
           int worksize,
           int offset_r,
           int offset_c,
           IKERN_PT_PARAM()
           //int eviction_state
    )
{  

    //if not eviction state(initial state)
    int nr_bt = 0;
    int tb_id = get_group_id(0);
    int tx = get_local_id(0);

    if(!pthread_d[tb_id*4]){
        if(tx==0){
            if(nr_pthread>BLOCK_SIZE_LIMIT){
                int rthread = (nr_pthread-BLOCK_SIZE_LIMIT)/BLOCK_SIZE;
                nr_bt = rthread/4;
                if(tb_id+1<= rthread%4){
                    nr_bt++;
                }
            }
            pthread_d[tb_id*4] = nr_bt+1;
        }
        barrier(CLK_GLOBAL_MEM_FENCE);
    }
    /*
    while(pthread_d[tb_id*4] != pthread_d[tb_id*4+1]){
        int bx = pthread_d[tb_id*4+1]*4+tb_id;
        int b_index_x = bx;
	    int b_index_y = blk - 1 - bx;
        if(tx==0){
            pthread_d[tb_id*4+2+pthread_d[tb_id*4+1]] = b_index_x;
            pthread_d[tb_id*4+1]++;
        }
        barrier(CLK_GLOBAL_MEM_FENCE);
    }

    if(tx==0){
        //pthread_d[tb_id*4+2] = blk;//pthread_d[tb_id*4];
        //pthread_d[tb_id*4+3] = pthread_d[tb_id*4+1];
        pthread_d[tb_id*4] = 0;
        pthread_d[tb_id*4+1] = 0;
    }
    barrier(CLK_GLOBAL_MEM_FENCE);
    return;
    */

    while(pthread_d[tb_id*4+1] != pthread_d[tb_id*4]){
	
    // Block index
    int bx = pthread_d[tb_id*4+1]*4+tb_id;
    //int bx = get_group_id(0);
	//int bx = get_global_id(0)/BLOCK_SIZE;
   
    // Thread index
    //int tx = get_local_id(0);
    
    // Base elements
    int base = offset_r * cols + offset_c;
   
    //blk가 문제가 됨. 다시 계산할 것.
    int b_index_x = bx;
	int b_index_y = blk - 1 - bx;
	
	
	int index   =   base + cols * BLOCK_SIZE * b_index_y + BLOCK_SIZE * b_index_x + tx + ( cols + 1 );
	int index_n   = base + cols * BLOCK_SIZE * b_index_y + BLOCK_SIZE * b_index_x + tx + ( 1 );
	int index_w   = base + cols * BLOCK_SIZE * b_index_y + BLOCK_SIZE * b_index_x + ( cols );
	int index_nw =  base + cols * BLOCK_SIZE * b_index_y + BLOCK_SIZE * b_index_x;
    
	if (tx == 0){
		SCORE(tx, 0) = input_itemsets_d[index_nw + tx];
	}

	barrier(CLK_LOCAL_MEM_FENCE);

	for ( int ty = 0 ; ty < BLOCK_SIZE ; ty++)
		REF(ty, tx) =  reference_d[index + cols * ty];

	barrier(CLK_LOCAL_MEM_FENCE);


    //if(tx == 0){
    //pthread_d[tb_id*4+3] = (tx + 1)*(BLOCK_SIZE+1);
    //pthread_d[tb_id*4+2] = input_itemsets_l[257];
    //input_itemsets_l[(tx + 1)*(BLOCK_SIZE+1)] = 1;
    //pthread_d[tb_id*4+2] = input_itemsets_l[(tx + 1)*(BLOCK_SIZE+1)]+1; 
    //}
	//barrier(CLK_GLOBAL_MEM_FENCE);
    // 바로 아래에서 error남//out of resource (-5)
	SCORE((tx + 1), 0) = input_itemsets_d[index_w + cols * tx];

	barrier(CLK_LOCAL_MEM_FENCE);

	SCORE(0, (tx + 1)) = input_itemsets_d[index_n];
  
	barrier(CLK_LOCAL_MEM_FENCE);
	
	
	for( int m = 0 ; m < BLOCK_SIZE ; m++){
	
	  if ( tx <= m ){
	  
		  int t_index_x =  tx + 1;
		  int t_index_y =  m - tx + 1;
			
		  SCORE(t_index_y, t_index_x) = maximum( SCORE((t_index_y-1), (t_index_x-1)) + REF((t_index_y-1), (t_index_x-1)),
		                                         SCORE((t_index_y),   (t_index_x-1)) - (penalty), 
												 SCORE((t_index_y-1), (t_index_x))   - (penalty));
	  }
	  barrier(CLK_LOCAL_MEM_FENCE);
    }
    
     barrier(CLK_LOCAL_MEM_FENCE);
    
	for( int m = BLOCK_SIZE - 2 ; m >=0 ; m--){
   
	  if ( tx <= m){
 
		  int t_index_x =  tx + BLOCK_SIZE - m ;
		  int t_index_y =  BLOCK_SIZE - tx;

         SCORE(t_index_y, t_index_x) = maximum(  SCORE((t_index_y-1), (t_index_x-1)) + REF((t_index_y-1), (t_index_x-1)),
		                                         SCORE((t_index_y),   (t_index_x-1)) - (penalty), 
		 										 SCORE((t_index_y-1), (t_index_x))   - (penalty));
	   
	  }

	  barrier(CLK_LOCAL_MEM_FENCE);
	}
	

    for ( int ty = 0 ; ty < BLOCK_SIZE ; ty++)
        input_itemsets_d[index + cols * ty] = SCORE((ty+1), (tx+1));
    /* 바로 아래에서 error남//out of resource (-5)
*/
        if(tx==0){
            pthread_d[tb_id*4+1]++;
        }
        barrier(CLK_GLOBAL_MEM_FENCE);

    }//pthread done
    if(tx==0){
        pthread_d[tb_id*4+2] = pthread_d[tb_id*4];
        pthread_d[tb_id*4+3] = pthread_d[tb_id*4+1];
        pthread_d[tb_id*4] = 0;
        pthread_d[tb_id*4+1] = 0;
    }
    barrier(CLK_GLOBAL_MEM_FENCE);
    return;
   
}

__kernel void 
nw_kernel2(__global int  * reference_d, 
		   __global int  * input_itemsets_d, 
		   __global int  * output_itemsets_d, 
		   __local	int  * input_itemsets_l,
		   __local	int  * reference_l,
           int cols,
           int penalty,
           int blk,
           int block_width,
           int worksize,
           int offset_r,
           int offset_c
    )
{  

	int bx = get_group_id(0);	
	//int bx = get_global_id(0)/BLOCK_SIZE;
   
    // Thread index
    int tx = get_local_id(0);
    
    // Base elements
    int base = offset_r * cols + offset_c;
    
    int b_index_x = bx + block_width - blk  ;
	int b_index_y = block_width - bx -1;
	
	
	int index   =   base + cols * BLOCK_SIZE * b_index_y + BLOCK_SIZE * b_index_x + tx + ( cols + 1 );
	int index_n   = base + cols * BLOCK_SIZE * b_index_y + BLOCK_SIZE * b_index_x + tx + ( 1 );
	int index_w   = base + cols * BLOCK_SIZE * b_index_y + BLOCK_SIZE * b_index_x + ( cols );
	int index_nw =  base + cols * BLOCK_SIZE * b_index_y + BLOCK_SIZE * b_index_x;
    
	if (tx == 0)
		SCORE(tx, 0) = input_itemsets_d[index_nw];

	for ( int ty = 0 ; ty < BLOCK_SIZE ; ty++)
		REF(ty, tx) =  reference_d[index + cols * ty];

	barrier(CLK_LOCAL_MEM_FENCE);

	SCORE((tx + 1), 0) = input_itemsets_d[index_w + cols * tx];

	barrier(CLK_LOCAL_MEM_FENCE);

	SCORE(0, (tx + 1)) = input_itemsets_d[index_n];
  
	barrier(CLK_LOCAL_MEM_FENCE);
  
	for( int m = 0 ; m < BLOCK_SIZE ; m++){
	
	  if ( tx <= m ){
	  
		  int t_index_x =  tx + 1;
		  int t_index_y =  m - tx + 1;

         SCORE(t_index_y, t_index_x) = maximum(  SCORE((t_index_y-1), (t_index_x-1)) + REF((t_index_y-1), (t_index_x-1)),
		                                         SCORE((t_index_y),   (t_index_x-1)) - (penalty), 
		 										 SCORE((t_index_y-1), (t_index_x))   - (penalty));
	  }
	  barrier(CLK_LOCAL_MEM_FENCE);
    }

	for( int m = BLOCK_SIZE - 2 ; m >=0 ; m--){
   
	  if ( tx <= m){
 
		  int t_index_x =  tx + BLOCK_SIZE - m ;
		  int t_index_y =  BLOCK_SIZE - tx;

          SCORE(t_index_y, t_index_x) = maximum( SCORE((t_index_y-1), (t_index_x-1)) + REF((t_index_y-1), (t_index_x-1)),
		                                         SCORE((t_index_y),   (t_index_x-1)) - (penalty), 
		 										 SCORE((t_index_y-1), (t_index_x))   - (penalty));
	   
	  }

	  barrier(CLK_LOCAL_MEM_FENCE);
	}

	for ( int ty = 0 ; ty < BLOCK_SIZE ; ty++)
		input_itemsets_d[index + ty * cols] = SCORE((ty+1), (tx+1));
	
    
    return;
  
}
