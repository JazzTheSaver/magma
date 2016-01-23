/*
    -- MAGMA (version 2.0.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date January 2016

       @generated from sparse-iter/blas/magma_zmconjugate.cu normal z -> s, Fri Jan 22 21:42:17 2016
       @author Hartwig Anzt

*/
#include "common_magmasparse.h"

#define BLOCK_SIZE 256


__global__ void 
magma_smconjugate_kernel(  
    int num_rows,
    magma_index_t *rowptr, 
    float *values )
{
    int row = blockDim.x * blockIdx.x + threadIdx.x;

    if(row < num_rows ){
        for( int i = rowptr[row]; i < rowptr[row+1]; i++){
            values[i] = MAGMA_S_CNJG( values[i] );
        }
    }
}



/**
    Purpose
    -------

    This function conjugates a matrix. For a real matrix, no value is changed.

    Arguments
    ---------

    @param[in,out]
    A           magma_s_matrix*
                input/output matrix
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_saux
    ********************************************************************/

extern "C" magma_int_t
magma_smconjugate(
    magma_s_matrix *A,
    magma_queue_t queue )
{
    magma_int_t info = 0;

    dim3 grid( magma_ceildiv( A->num_rows, BLOCK_SIZE ));
    magma_smconjugate_kernel<<< grid, BLOCK_SIZE, 0, queue->cuda_stream() >>> 
                                    ( A->num_rows, A->drow, A->dval );
        
    return info;
}
