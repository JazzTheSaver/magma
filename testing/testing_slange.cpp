/*
    -- MAGMA (version 1.5.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date May 2014

       @generated from testing_zlange.cpp normal z -> s, Fri May 30 10:41:23 2014
       @author Mark Gates
*/
// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing slange
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t   gbytes, gpu_perf, gpu_time, cpu_perf, cpu_time;
    float *h_A;
    float *h_work;
    float *d_A;
    float *d_work;
    magma_int_t M, N, n2, lda, ldda;
    magma_int_t idist    = 3;  // normal distribution (otherwise max norm is always ~ 1)
    magma_int_t ISEED[4] = {0,0,0,1};
    float      error, norm_magma, norm_lapack;
    magma_int_t status = 0;

    magma_opts opts;
    parse_opts( argc, argv, &opts );
    
    float tol = opts.tolerance * lapackf77_slamch("E");
    
    // Only one norm supported for now, but leave this here for future support
    // of different norms. See similar code in testing_slansy.cpp.
    magma_norm_t norm[] = { MagmaInfNorm };
    
    printf("    M     N   norm   CPU GByte/s (ms)    GPU GByte/s (ms)    error   \n");
    printf("=====================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int inorm = 0; inorm < 1; ++inorm ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M   = opts.msize[itest];
            N   = opts.nsize[itest];
            lda = M;
            n2  = lda*N;
            ldda = roundup( M, opts.pad );
            // read whole matrix
            gbytes = M*N*sizeof(float) / 1e9;
            
            TESTING_MALLOC_CPU( h_A,    float, n2 );
            TESTING_MALLOC_CPU( h_work, float, M );
            
            TESTING_MALLOC_DEV( d_A,    float, ldda*N );
            TESTING_MALLOC_DEV( d_work, float, M );
            
            /* Initialize the matrix */
            lapackf77_slarnv( &idist, ISEED, &n2, h_A );
            magma_ssetmatrix( M, N, h_A, lda, d_A, ldda );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            gpu_time = magma_wtime();
            norm_magma = magmablas_slange( norm[inorm], M, N, d_A, ldda, d_work );
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gbytes / gpu_time;
            if (norm_magma < 0)
                printf("magmablas_slange returned error %f: %s.\n",
                       norm_magma, magma_strerror( (int) norm_magma ));
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            cpu_time = magma_wtime();
            norm_lapack = lapackf77_slange( lapack_norm_const(norm[inorm]), &M, &N, h_A, &lda, h_work );
            cpu_time = magma_wtime() - cpu_time;
            cpu_perf = gbytes / cpu_time;
            if (norm_lapack < 0)
                printf("lapackf77_slange returned error %f: %s.\n",
                       norm_lapack, magma_strerror( (int) norm_lapack ));
            
            /* =====================================================================
               Check the result compared to LAPACK
               =================================================================== */
            if ( norm[inorm] == MagmaMaxNorm )
                error = fabs( norm_magma - norm_lapack );
            else
                error = fabs( norm_magma - norm_lapack ) / norm_lapack;
            
            printf("%5d %5d   %4c   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e   %s\n",
                   (int) M, (int) N, lapacke_norm_const(norm[inorm]),
                   cpu_perf, cpu_time*1000., gpu_perf, gpu_time*1000.,
                   error, (error < tol ? "ok" : "failed") );
            status += ! (error < tol);
            
            TESTING_FREE_CPU( h_A    );
            TESTING_FREE_CPU( h_work );
            
            TESTING_FREE_DEV( d_A    );
            TESTING_FREE_DEV( d_work );
            fflush( stdout );
        }} // end inorm, iter
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    TESTING_FINALIZE();
    return status;
}