/*
    -- MAGMA (version 2.0.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date January 2016

       @author Raffaele Solca
       @author Stan Tomov
       @author Azzam Haidar
       @author Mark Gates

       @generated from testing/testing_zhetrd_gpu.cpp normal z -> s, Fri Jan 22 21:42:46 2016

*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

#define REAL

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing ssytrd_gpu
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t    gflops, gpu_perf, cpu_perf, gpu_time, cpu_time;
    float           eps;
    float *h_A, *h_R, *h_Q, *h_work, *work;
    magmaFloat_ptr d_R, dwork;
    float *tau;
    float          *diag, *offdiag;
    float           result[2] = {0., 0.};
    magma_int_t N, n2, lda, ldda, lwork, info, nb, ldwork;
    magma_int_t ione     = 1;
    magma_int_t itwo     = 2;
    magma_int_t ithree   = 3;
    magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t status = 0;
    
    #ifdef COMPLEX
    float *rwork;
    #endif

    eps = lapackf77_slamch( "E" );

    magma_opts opts;
    opts.parse_opts( argc, argv );

    float tol = opts.tolerance * lapackf77_slamch("E");
    
    printf("%% Available versions (specify with --version):\n");
    printf("%% 1 - magma_ssytrd_gpu:   uses SSYMV from CUBLAS (default)\n");
    printf("%% 2 - magma_ssytrd2_gpu:  uses SSYMV from MAGMA BLAS that requires extra space\n\n");

    printf("%% uplo = %s, version %d\n", lapack_uplo_const(opts.uplo), int(opts.version) );
    printf("%% N     CPU Gflop/s (sec)   GPU Gflop/s (sec)   |A-QHQ^H|/N|A|   |I-QQ^H|/N\n");
    printf("%%==========================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[itest];
            lda    = N;
            ldda   = magma_roundup( N, opts.align );  // multiple of 32 by default
            n2     = lda*N;
            nb     = magma_get_ssytrd_nb(N);
            lwork  = N*nb;  /* We suppose the magma nb is bigger than lapack nb */
            gflops = FLOPS_SSYTRD( N ) / 1e9;
            ldwork = ldda*magma_ceildiv(N,64) + 2*ldda*nb;
            
            TESTING_MALLOC_CPU( h_A,     float, lda*N );
            TESTING_MALLOC_CPU( tau,     float, N     );
            TESTING_MALLOC_CPU( diag,    float, N   );
            TESTING_MALLOC_CPU( offdiag, float, N-1 );
            
            TESTING_MALLOC_PIN( h_R,     float, lda*N );
            TESTING_MALLOC_PIN( h_work,  float, lwork );
            
            TESTING_MALLOC_DEV( d_R,     float, ldda*N );
            TESTING_MALLOC_DEV( dwork,   float, ldwork );
            
            /* ====================================================================
               Initialize the matrix
               =================================================================== */
            lapackf77_slarnv( &ione, ISEED, &n2, h_A );
            magma_smake_symmetric( N, h_A, lda );
            magma_ssetmatrix( N, N, h_A, lda, d_R, ldda );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            gpu_time = magma_wtime();
            if (opts.version == 1) {
                magma_ssytrd_gpu( opts.uplo, N, d_R, ldda, diag, offdiag,
                                  tau, h_R, lda, h_work, lwork, &info );
            }
            else {
                magma_ssytrd2_gpu( opts.uplo, N, d_R, ldda, diag, offdiag,
                                   tau, h_R, lda, h_work, lwork, dwork, ldwork, &info );
            }
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gflops / gpu_time;
            if (info != 0) {
                printf("magma_ssytrd_gpu returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            }
            
            /* =====================================================================
               Check the factorization
               =================================================================== */
            if ( opts.check ) {
                TESTING_MALLOC_CPU( h_Q,  float, lda*N );
                TESTING_MALLOC_CPU( work, float, 2*N*N );
                #ifdef COMPLEX
                TESTING_MALLOC_CPU( rwork, float, N );
                #endif
                
                magma_sgetmatrix( N, N, d_R, ldda, h_R, lda );
                magma_sgetmatrix( N, N, d_R, ldda, h_Q, lda );
                lapackf77_sorgtr( lapack_uplo_const(opts.uplo), &N, h_Q, &lda, tau, h_work, &lwork, &info );
                
                lapackf77_ssyt21( &itwo, lapack_uplo_const(opts.uplo), &N, &ione,
                                  h_A, &lda, diag, offdiag,
                                  h_Q, &lda, h_R, &lda,
                                  tau, work,
                                  #ifdef COMPLEX
                                  rwork,
                                  #endif
                                  &result[0] );
                
                lapackf77_ssyt21( &ithree, lapack_uplo_const(opts.uplo), &N, &ione,
                                  h_A, &lda, diag, offdiag,
                                  h_Q, &lda, h_R, &lda,
                                  tau, work,
                                  #ifdef COMPLEX
                                  rwork,
                                  #endif
                                  &result[1] );
                result[0] *= eps;
                result[1] *= eps;
                
                TESTING_FREE_CPU( h_Q  );
                TESTING_FREE_CPU( work );
                #ifdef COMPLEX
                TESTING_FREE_CPU( rwork );
                #endif
            }
                        
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                cpu_time = magma_wtime();
                lapackf77_ssytrd( lapack_uplo_const(opts.uplo), &N, h_A, &lda, diag, offdiag, tau,
                                  h_work, &lwork, &info );
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                if (info != 0) {
                    printf("lapackf77_ssytrd returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                }
            }
            
            /* =====================================================================
               Print performance and error.
               =================================================================== */
            if ( opts.lapack ) {
                printf("%5d   %7.2f (%7.2f)   %7.2f (%7.2f)",
                       (int) N, cpu_perf, cpu_time, gpu_perf, gpu_time );
            } else {
                printf("%5d     ---   (  ---  )   %7.2f (%7.2f)",
                       (int) N, gpu_perf, gpu_time );
            }
            if ( opts.check ) {
                printf("   %8.2e        %8.2e   %s\n", result[0], result[1],
                        ((result[0] < tol && result[1] < tol) ? "ok" : "failed")  );
                status += ! (result[0] < tol && result[1] < tol);
            } else {
                printf("     ---             ---\n");
            }
            
            TESTING_FREE_CPU( h_A     );
            TESTING_FREE_CPU( tau     );
            TESTING_FREE_CPU( diag    );
            TESTING_FREE_CPU( offdiag );
            
            TESTING_FREE_PIN( h_R    );
            TESTING_FREE_PIN( h_work );
            
            TESTING_FREE_DEV( d_R   );
            TESTING_FREE_DEV( dwork );
            
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
