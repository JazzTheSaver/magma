/*
    -- MAGMA (version 2.0.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date January 2016

       @generated from testing/testing_ztrmm.cpp normal z -> c, Fri Jan 22 21:42:33 2016
       @author Chongxiao Cao
*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// includes, project
#include "testings.h"  // before magma.h, to include cublas_v2
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"


/* ////////////////////////////////////////////////////////////////////////////
   -- Testing ctrmm
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t   gflops, cublas_perf, cublas_time, cpu_perf, cpu_time;
    float          cublas_error, Cnorm, work[1];
    magma_int_t M, N;
    magma_int_t Ak;
    magma_int_t sizeA, sizeB;
    magma_int_t lda, ldb, ldda, lddb, lddc;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    
    magmaFloatComplex *h_A, *h_B, *h_Bcublas;
    magmaFloatComplex_ptr d_A, d_B, d_C;
    magmaFloatComplex c_neg_one = MAGMA_C_NEG_ONE;
    magmaFloatComplex alpha = MAGMA_C_MAKE(  0.29, -0.86 );
    magma_int_t status = 0;
    
    magma_opts opts;
    opts.parse_opts( argc, argv );
    opts.lapack |= opts.check;  // check (-c) implies lapack (-l)
    
    float tol = opts.tolerance * lapackf77_slamch("E");
    
    printf("%% If running lapack (option --lapack), CUBLAS error is computed\n"
           "%% relative to CPU BLAS result.\n\n");
    printf("%% side = %s, uplo = %s, transA = %s, diag = %s \n",
           lapack_side_const(opts.side), lapack_uplo_const(opts.uplo),
           lapack_trans_const(opts.transA), lapack_diag_const(opts.diag) );
    printf("%%   M     N   CUBLAS Gflop/s (ms)   CPU Gflop/s (ms)  CUBLAS error\n");
    printf("%%=================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M = opts.msize[itest];
            N = opts.nsize[itest];
            gflops = FLOPS_CTRMM(opts.side, M, N) / 1e9;

            if ( opts.side == MagmaLeft ) {
                lda = M;
                Ak = M;
            } else {
                lda = N;
                Ak = N;
            }
            
            ldb = M;
            
            ldda = magma_roundup( lda, opts.align );  // multiple of 32 by default
            lddb = magma_roundup( ldb, opts.align );  // multiple of 32 by default
            lddc = lddb;
            
            sizeA = lda*Ak;
            sizeB = ldb*N;
            
            TESTING_MALLOC_CPU( h_A,       magmaFloatComplex, lda*Ak );
            TESTING_MALLOC_CPU( h_B,       magmaFloatComplex, ldb*N  );
            TESTING_MALLOC_CPU( h_Bcublas, magmaFloatComplex, ldb*N  );
            
            TESTING_MALLOC_DEV( d_A, magmaFloatComplex, ldda*Ak );
            TESTING_MALLOC_DEV( d_B, magmaFloatComplex, lddb*N  );
            TESTING_MALLOC_DEV( d_C, magmaFloatComplex, lddc*N  );
            
            /* Initialize the matrices */
            lapackf77_clarnv( &ione, ISEED, &sizeA, h_A );
            lapackf77_clarnv( &ione, ISEED, &sizeB, h_B );
            
            /* =====================================================================
               Performs operation using CUBLAS
               =================================================================== */
            magma_csetmatrix( Ak, Ak, h_A, lda, d_A, ldda );
            magma_csetmatrix( M,  N,  h_B, ldb, d_B, lddb );
            
            // note cublas does trmm out-of-place (i.e., adds output matrix C),
            // but allows C=B to do in-place.
            magmablasSetKernelStream( opts.queue );  // opts.handle also uses opts.queue
            cublas_time = magma_sync_wtime( opts.queue );
            #ifdef HAVE_CUBLAS
                cublasCtrmm( opts.handle, cublas_side_const(opts.side), cublas_uplo_const(opts.uplo),
                             cublas_trans_const(opts.transA), cublas_diag_const(opts.diag),
                             M, N,
                             &alpha, d_A, ldda,
                                     d_B, lddb,
                                     d_C, lddc );  // output C; differs from BLAS standard
            #else
                magma_ctrmm( opts.side, opts.uplo, opts.transA, opts.diag,
                             M, N,
                             alpha, d_A, 0, ldda,
                                    d_B, 0, lddb, opts.queue );
            #endif
            cublas_time = magma_sync_wtime( opts.queue ) - cublas_time;
            cublas_perf = gflops / cublas_time;
            
            #ifdef HAVE_CUBLAS
                magma_cgetmatrix( M, N, d_C,    lddc, h_Bcublas, ldb );
            #else
                magma_cgetmatrix( M, N, d_B, 0, lddb, h_Bcublas, ldb, opts.queue );
            #endif
            
            /* =====================================================================
               Performs operation using CPU BLAS
               =================================================================== */
            if ( opts.lapack ) {
                cpu_time = magma_wtime();
                blasf77_ctrmm( lapack_side_const(opts.side), lapack_uplo_const(opts.uplo),
                               lapack_trans_const(opts.transA), lapack_diag_const(opts.diag),
                               &M, &N,
                               &alpha, h_A, &lda,
                                       h_B, &ldb );
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
            }
            
            /* =====================================================================
               Check the result
               =================================================================== */
            if ( opts.lapack ) {
                // compute relative error for both magma & cublas, relative to lapack,
                // |C_magma - C_lapack| / |C_lapack|
                Cnorm = lapackf77_clange( "M", &M, &N, h_B, &ldb, work );
                
                blasf77_caxpy( &sizeB, &c_neg_one, h_B, &ione, h_Bcublas, &ione );
                cublas_error = lapackf77_clange( "M", &M, &N, h_Bcublas, &ldb, work ) / Cnorm;
                
                printf("%5d %5d   %7.2f (%7.2f)   %7.2f (%7.2f)    %8.2e   %s\n",
                       (int) M, (int) N,
                       cublas_perf, 1000.*cublas_time,
                       cpu_perf,    1000.*cpu_time,
                       cublas_error, (cublas_error < tol ? "ok" : "failed"));
                status += ! (cublas_error < tol);
            }
            else {
                printf("%5d %5d   %7.2f (%7.2f)    ---   (  ---  )    ---     ---\n",
                       (int) M, (int) N,
                       cublas_perf, 1000.*cublas_time);
            }
            
            TESTING_FREE_CPU( h_A );
            TESTING_FREE_CPU( h_B );
            TESTING_FREE_CPU( h_Bcublas );
            
            TESTING_FREE_DEV( d_A );
            TESTING_FREE_DEV( d_B );
            TESTING_FREE_DEV( d_C );
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
