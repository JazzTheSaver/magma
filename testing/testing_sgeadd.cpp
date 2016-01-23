/*
    -- MAGMA (version 2.0.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date January 2016

       @generated from testing/testing_zgeadd.cpp normal z -> s, Fri Jan 22 21:42:35 2016
       @author Mark Gates
*/
// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// includes, project
#include "magma.h"
#include "magma_lapack.h"
#include "magma_operators.h"
#include "testings.h"


/* ////////////////////////////////////////////////////////////////////////////
   -- Testing sgeadd
*/
int main( int argc, char** argv)
{
    #define h_A(i_, j_) (h_A + (i_) + (j_)*lda)
    #define h_B(i_, j_) (h_B + (i_) + (j_)*lda)  // B uses lda
    
    TESTING_INIT();

    real_Double_t   gflops, gpu_perf, gpu_time, cpu_perf, cpu_time;
    float          Bnorm, error, work[1];
    float *h_A, *h_B, *d_A, *d_B;
    float alpha = MAGMA_S_MAKE( 3.1415, 2.71828 );
    float beta  = MAGMA_S_MAKE( 6.0221, 6.67408 );
    float c_neg_one = MAGMA_S_NEG_ONE;
    
    magma_int_t M, N, size, lda, ldda;
    magma_int_t ione = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t status = 0;

    magma_opts opts;
    opts.parse_opts( argc, argv );

    float tol = opts.tolerance * lapackf77_slamch("E");
    
    /* Uncomment these lines to check parameters.
     * magma_xerbla calls lapack's xerbla to print out error. */
    //magmablas_sgeadd( -1,  N, alpha, d_A, ldda, d_B, ldda );
    //magmablas_sgeadd(  M, -1, alpha, d_A, ldda, d_B, ldda );
    //magmablas_sgeadd(  M,  N, alpha, d_A, M-1,  d_B, ldda );
    //magmablas_sgeadd(  M,  N, alpha, d_A, ldda, d_B, N-1  );

    printf("%%   M     N   CPU Gflop/s (ms)    GPU Gflop/s (ms)    |Bl-Bm|/|Bl|\n");
    printf("%%========================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M = opts.msize[itest];
            N = opts.nsize[itest];
            lda    = M;
            ldda   = magma_roundup( M, opts.align );  // multiple of 32 by default
            size   = lda*N;
            gflops = 2.*M*N / 1e9;
            
            TESTING_MALLOC_CPU( h_A, float, lda *N );
            TESTING_MALLOC_CPU( h_B, float, lda *N );
            
            TESTING_MALLOC_DEV( d_A, float, ldda*N );
            TESTING_MALLOC_DEV( d_B, float, ldda*N );
            
            lapackf77_slarnv( &ione, ISEED, &size, h_A );
            lapackf77_slarnv( &ione, ISEED, &size, h_B );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            magma_ssetmatrix( M, N, h_A, lda, d_A, ldda );
            magma_ssetmatrix( M, N, h_B, lda, d_B, ldda );
            
            magmablasSetKernelStream( opts.queue );
            gpu_time = magma_sync_wtime( opts.queue );
            if ( opts.version == 1 ) {
                magmablas_sgeadd( M, N, alpha, d_A, ldda, d_B, ldda );
            }
            else {
                magmablas_sgeadd2( M, N, alpha, d_A, ldda, beta, d_B, ldda );
            }
            gpu_time = magma_sync_wtime( opts.queue ) - gpu_time;
            gpu_perf = gflops / gpu_time;
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            cpu_time = magma_wtime();
            if ( opts.version == 1 ) {
            for( int j = 0; j < N; ++j ) {
                blasf77_saxpy( &M, &alpha, &h_A[j*lda], &ione, &h_B[j*lda], &ione );
            }
            }
            else {
                for( int j = 0; j < N; ++j ) {
                    // daxpby
                    for( int i=0; i < M; ++i ) {
                        *h_B(i,j) = alpha * (*h_A(i,j)) + beta * (*h_B(i,j));
                    }
                }
            }
            cpu_time = magma_wtime() - cpu_time;
            cpu_perf = gflops / cpu_time;
            
            /* =====================================================================
               Check result
               =================================================================== */
            magma_sgetmatrix( M, N, d_B, ldda, h_A, lda );
            
            blasf77_saxpy( &size, &c_neg_one, h_B, &ione, h_A, &ione );
            Bnorm = lapackf77_slange( "F", &M, &N, h_B, &lda, work );
            error = lapackf77_slange( "F", &M, &N, h_A, &lda, work ) / Bnorm;
            
            printf("%5d %5d   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e   %s\n",
                   (int) M, (int) N,
                   cpu_perf, cpu_time*1000., gpu_perf, gpu_time*1000.,
                   error, (error < tol ? "ok" : "failed"));
            status += ! (error < tol);
            
            TESTING_FREE_CPU( h_A );
            TESTING_FREE_CPU( h_B );
            
            TESTING_FREE_DEV( d_A );
            TESTING_FREE_DEV( d_B );
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    opts.cleanup();
    TESTING_FINALIZE();
    return status;
}
