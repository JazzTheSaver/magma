/*
    -- MAGMA (version 2.0.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date January 2016

       @generated from magmablas/zgerbt.h normal z -> d, Fri Jan 22 21:42:53 2016

       @author Adrien Remy
       @author Azzam Haidar
       
       Definitions used in dgerbt.cu dgerbt_batched.cu
*/

#ifndef DGERBT_H
#define DGERBT_H
/////////////////////////////////////
// classical prototypes
/////////////////////////////////////

__global__ void 
magmablas_delementary_multiplication_kernel(
    magma_int_t n,
    double *dA, magma_int_t offsetA, magma_int_t ldda, 
    double *du, magma_int_t offsetu, 
    double *dv, magma_int_t offsetv);

__global__ void 
magmablas_dapply_vector_kernel(
    magma_int_t n,
    double *du, magma_int_t offsetu,  double *db, magma_int_t offsetb );

__global__ void 
magmablas_dapply_transpose_vector_kernel(
    magma_int_t n,
    double *du, magma_int_t offsetu, double *db, magma_int_t offsetb );
/////////////////////////////////////
// batched prototypes
/////////////////////////////////////
__global__ void 
magmablas_delementary_multiplication_kernel_batched(
    magma_int_t n,
    double **dA_array, magma_int_t offsetA, magma_int_t ldda, 
    double *du, magma_int_t offsetu, 
    double *dv, magma_int_t offsetv);

__global__ void 
magmablas_dapply_vector_kernel_batched(
    magma_int_t n,
    double *du, magma_int_t offsetu, double **db_array, magma_int_t offsetb );

__global__ void 
magmablas_dapply_transpose_vector_kernel_batched(
    magma_int_t n,
    double *du, magma_int_t offsetu, double **db_array, magma_int_t offsetb );

#endif        //  #ifndef DGERBT_H
