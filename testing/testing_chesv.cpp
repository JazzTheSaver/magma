/*
    -- MAGMA (version 2.0.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date January 2016

       @generated from testing/testing_zhesv.cpp normal z -> c, Fri Jan 22 21:42:39 2016
       @author Ichitaro Yamazaki
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

/* ================================================================================================== */

// Initialize matrix to random & symmetrize.
// Having this in separate function ensures the same ISEED is always used,
// so we can re-generate the identical matrix.
void init_matrix( int m, int n, magmaFloatComplex *h_A, magma_int_t lda )
{
    assert( m == n );
    magma_int_t ione = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t n2 = lda*n;
    lapackf77_clarnv( &ione, ISEED, &n2, h_A );
    magma_cmake_hermitian( n, h_A, lda );
}


// On input, A and ipiv is LU factorization of A. On output, A is overwritten.
// Requires m == n.
// Uses init_matrix() to re-generate original A as needed.
// Generates random RHS b and solves Ax=b.
// Returns residual, |Ax - b| / (n |A| |x|).
float get_residual(
    magma_uplo_t uplo, magma_int_t n, magma_int_t nrhs,
    magmaFloatComplex *A, magma_int_t lda, magma_int_t *ipiv,
    magmaFloatComplex *x, magma_int_t ldx,
    magmaFloatComplex *b, magma_int_t ldb)
{
    const magmaFloatComplex c_one     = MAGMA_C_ONE;
    const magmaFloatComplex c_neg_one = MAGMA_C_NEG_ONE;
    const magma_int_t ione = 1;
    
    // reset to original A
    init_matrix( n, n, A, lda );
    
    // compute r = Ax - b, saved in b
    blasf77_cgemv( "Notrans", &n, &n, &c_one, A, &lda, x, &ione, &c_neg_one, b, &ione );
    
    // compute residual |Ax - b| / (n*|A|*|x|)
    float norm_x, norm_A, norm_r, work[1];
    norm_A = lapackf77_clange( MagmaFullStr, &n, &n, A, &lda, work );
    norm_r = lapackf77_clange( MagmaFullStr, &n, &ione, b, &n, work );
    norm_x = lapackf77_clange( MagmaFullStr, &n, &ione, x, &n, work );
    
    //printf( "r=\n" ); magma_cprint( 1, n, b, 1 );
    //printf( "r=%.2e, A=%.2e, x=%.2e, n=%d\n", norm_r, norm_A, norm_x, n );
    return norm_r / (n * norm_A * norm_x);
}


/* ////////////////////////////////////////////////////////////////////////////
   -- Testing chesv
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    magmaFloatComplex *h_A, *h_B, *h_X, *work, temp;
    real_Double_t   gflops, gpu_perf, gpu_time = 0.0, cpu_perf=0, cpu_time=0;
    float          error, error_lapack = 0.0;
    magma_int_t     *ipiv;
    magma_int_t     N, n2, lda, ldb, sizeB, lwork, info;
    magma_int_t     status = 0, ione = 1;
    magma_int_t     ISEED[4] = {0,0,0,1};

    magma_opts opts;
    opts.parse_opts( argc, argv );
    
    float tol = opts.tolerance * lapackf77_slamch("E");

    printf("%%   M     N   CPU Gflop/s (sec)   GPU Gflop/s (sec)   |Ax-b|/(N*|A|*|x|)\n");
    printf("%%========================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[itest];
            ldb    = N;
            lda    = N;
            n2     = lda*N;
            sizeB  = ldb*opts.nrhs;
            gflops = ( FLOPS_CPOTRF( N ) + FLOPS_CPOTRS( N, opts.nrhs ) ) / 1e9;
            
            TESTING_MALLOC_CPU( ipiv, magma_int_t, N );
            TESTING_MALLOC_PIN( h_A,  magmaFloatComplex, n2 );
            TESTING_MALLOC_PIN( h_B,  magmaFloatComplex, sizeB );
            TESTING_MALLOC_PIN( h_X,  magmaFloatComplex, sizeB );
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                lwork = -1;
                lapackf77_chesv(lapack_uplo_const(opts.uplo), &N, &opts.nrhs,
                                h_A, &lda, ipiv, h_X, &ldb, &temp, &lwork, &info);
                lwork = (int)MAGMA_C_REAL(temp);
                TESTING_MALLOC_CPU( work, magmaFloatComplex, lwork );

                init_matrix( N, N, h_A, lda );
                lapackf77_clarnv( &ione, ISEED, &sizeB, h_B );
                lapackf77_clacpy( MagmaFullStr, &N, &opts.nrhs, h_B, &ldb, h_X, &ldb );

                cpu_time = magma_wtime();
                lapackf77_chesv(lapack_uplo_const(opts.uplo), &N, &opts.nrhs,
                                h_A, &lda, ipiv, h_X, &ldb, work, &lwork, &info);
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                if (info != 0) {
                    printf("lapackf77_chesv returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                }
                error_lapack = get_residual( opts.uplo, N, opts.nrhs, h_A, lda, ipiv, h_X, ldb, h_B, ldb );

                TESTING_FREE_CPU( work );
            }
           
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            init_matrix( N, N, h_A, lda );
            lapackf77_clarnv( &ione, ISEED, &sizeB, h_B );
            lapackf77_clacpy( MagmaFullStr, &N, &opts.nrhs, h_B, &ldb, h_X, &ldb );

            magma_setdevice(0);
            gpu_time = magma_wtime();
            magma_chesv( opts.uplo, N, opts.nrhs, h_A, lda, ipiv, h_X, ldb, &info);
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gflops / gpu_time;
            if (info != 0) {
                printf("magma_chesv returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            }
            
            /* =====================================================================
               Check the factorization
               =================================================================== */
            if ( opts.lapack ) {
                printf("%5d %5d   %7.2f (%7.2f)   %7.2f (%7.2f)",
                       (int) N, (int) N, cpu_perf, cpu_time, gpu_perf, gpu_time );
            }
            else {
                printf("%5d %5d     ---   (  ---  )   %7.2f (%7.2f)",
                       (int) N, (int) N, gpu_perf, gpu_time );
            }
            if ( opts.check == 0 ) {
                printf("     ---   \n");
            } else {
                error = get_residual( opts.uplo, N, opts.nrhs, h_A, lda, ipiv, h_X, ldb, h_B, ldb );
                printf("   %8.2e   %s", error, (error < tol ? "ok" : "failed"));
                if (opts.lapack)
                    printf(" (lapack rel.res. = %8.2e)", error_lapack);
                printf("\n");
                status += ! (error < tol);
            }
            
            TESTING_FREE_CPU( ipiv );
            TESTING_FREE_PIN( h_X  );
            TESTING_FREE_PIN( h_B  );
            TESTING_FREE_PIN( h_A  );
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
