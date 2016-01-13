/*
    -- MAGMA (version 2.0.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date January 2016

       @author Raffaele Solca
       @author Azzam Haidar
       @author Mark Gates

       @generated from testing/testing_zheevdx_2stage.cpp normal z -> s, Wed Jan  6 17:59:50 2016

*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// includes, project
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"
#include "magma_threadsetting.h"

#define REAL

static magma_int_t check_orthogonality(magma_int_t M, magma_int_t N, float *Q, magma_int_t LDQ, float eps);
static magma_int_t check_reduction(magma_uplo_t uplo, magma_int_t N, magma_int_t bw, float *A, float *D, magma_int_t LDA, float *Q, float eps );
static magma_int_t check_solution(magma_int_t N, float *E1, float *E2, float eps);

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing ssygvdx
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t gpu_time;

    float *h_A, *h_R, *h_work;

    #ifdef COMPLEX
    float *rwork;
    magma_int_t lrwork;
    #endif

    /* Matrix size */
    float *w1, *w2;
    magma_int_t *iwork;
    magma_int_t N, n2, info, lwork, liwork;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t info_ortho     = 0;
    magma_int_t info_solution  = 0;
    magma_int_t info_reduction = 0;
    magma_int_t status = 0;

    magma_opts opts;
    opts.parse_opts( argc, argv );

    magma_range_t range = MagmaRangeAll;
    if (opts.fraction != 1)
        range = MagmaRangeI;

    if ( opts.check && opts.jobz == MagmaNoVec ) {
        fprintf( stderr, "%% WARNING checking all results requires vectors (option -JV);\n"
                 "%% setting jobz=V is recommended otherwise only eigenvalues are checked\n" );
    }

    // pass ngpu = -1 to test multi-GPU code using 1 gpu
    magma_int_t abs_ngpu = abs( opts.ngpu );
    
    printf("%% jobz = %s, range = %s, uplo = %s, fraction = %6.4f, ngpu %d\n",
           lapack_vec_const(opts.jobz), lapack_range_const(range), lapack_uplo_const(opts.uplo),
           opts.fraction, int(abs_ngpu) );

    printf("%%   N     M  GPU Time (sec)  ||I-Q'Q||_oo/N  ||A-QDQ'||_oo/(||A||_oo.N).  |D-D_magma|/(|D| * N)\n");
    printf("%%=======================================================================\n");
    magma_int_t threads = magma_get_parallel_numthreads();
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[itest];
            n2     = N*N;

            magma_ssyevdx_getworksize(N, threads, (opts.jobz == MagmaVec), 
                                     &lwork, 
                                     #ifdef COMPLEX
                                     &lrwork, 
                                     #endif
                                     &liwork);
            
            /* Allocate host memory for the matrix */
            TESTING_MALLOC_CPU( h_A,   float, n2 );
            TESTING_MALLOC_CPU( w1,    float, N );
            TESTING_MALLOC_CPU( w2,    float, N );
            TESTING_MALLOC_CPU( iwork, magma_int_t, liwork );
            
            TESTING_MALLOC_PIN( h_R,    float, n2    );
            TESTING_MALLOC_PIN( h_work, float, lwork );
            #ifdef COMPLEX
            TESTING_MALLOC_PIN( rwork, float, lrwork );
            #endif

            /* Initialize the matrix */
            lapackf77_slarnv( &ione, ISEED, &n2, h_A );
            magma_smake_symmetric( N, h_A, N );

            magma_int_t m1 = 0;
            float vl = 0;
            float vu = 0;
            magma_int_t il = 0;
            magma_int_t iu = 0;
            if (opts.fraction == 0) {
                il = max( 1, magma_int_t(0.1*N) );
                iu = max( 1, magma_int_t(0.3*N) );
            }
            else {
                il = 1;
                iu = max( 1, magma_int_t(opts.fraction*N) );
            }

            if (opts.warmup) {
                // ==================================================================
                // Warmup using MAGMA
                // ==================================================================
                lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_A, &N, h_R, &N );
                if (opts.ngpu == 1) {
                    //printf("calling ssyevdx_2stage 1 GPU\n");
                    magma_ssyevdx_2stage( opts.jobz, range, opts.uplo, N, 
                                          h_R, N, 
                                          vl, vu, il, iu, 
                                          &m1, w1, 
                                          h_work, lwork, 
                                          #ifdef COMPLEX
                                          rwork, lrwork, 
                                          #endif
                                          iwork, liwork, 
                                          &info );
                } else {
                    //printf("calling ssyevdx_2stage_m %d GPU\n", (int) opts.ngpu);
                    magma_ssyevdx_2stage_m( abs_ngpu, opts.jobz, range, opts.uplo, N, 
                                            h_R, N, 
                                            vl, vu, il, iu, 
                                            &m1, w1, 
                                            h_work, lwork, 
                                            #ifdef COMPLEX
                                            rwork, lrwork, 
                                            #endif
                                            iwork, liwork, 
                                            &info );
                }
            }

            // ===================================================================
            // Performs operation using MAGMA
            // ===================================================================
            lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_A, &N, h_R, &N );
            gpu_time = magma_wtime();
            if (opts.ngpu == 1) {
                //printf("calling ssyevdx_2stage 1 GPU\n");
                magma_ssyevdx_2stage( opts.jobz, range, opts.uplo, N, 
                                      h_R, N, 
                                      vl, vu, il, iu, 
                                      &m1, w1, 
                                      h_work, lwork, 
                                      #ifdef COMPLEX
                                      rwork, lrwork, 
                                      #endif
                                      iwork, liwork, 
                                      &info );
            } else {
                //printf("calling ssyevdx_2stage_m %d GPU\n", (int) opts.ngpu);
                magma_ssyevdx_2stage_m( abs_ngpu, opts.jobz, range, opts.uplo, N, 
                                        h_R, N, 
                                        vl, vu, il, iu, 
                                        &m1, w1, 
                                        h_work, lwork, 
                                        #ifdef COMPLEX
                                        rwork, lrwork, 
                                        #endif
                                        iwork, liwork, 
                                        &info );
            }
            gpu_time = magma_wtime() - gpu_time;
            
            printf("%5d %5d  %7.2f      ",
                   (int) N, (int) m1, gpu_time );

            if ( opts.check ) {
                info_solution  = 0;
                info_ortho     = 0;
                info_reduction = 0;
                //float eps   = lapackf77_slamch("E")*lapackf77_slamch("B");
                float eps   = lapackf77_slamch("E");
                //printf("\n");
                //printf("------ TESTS FOR MAGMA SSYEVD ROUTINE -------  \n");
                //printf("        Size of the Matrix %d by %d\n", (int) N, (int) N);
                //printf("\n");
                //printf(" The matrix A is randomly generated for each test.\n");
                //printf("============\n");
                //printf(" The relative machine precision (eps) is %8.2e\n", eps);
                //printf(" Computational tests pass if scaled residuals are less than 60.\n");
              
                /* Check the orthogonality, reduction and the eigen solutions */
                if (opts.jobz == MagmaVec) {
                    info_ortho = check_orthogonality(N, N, h_R, N, eps);
                    info_reduction = check_reduction(opts.uplo, N, 1, h_A, w1, N, h_R, eps);
                } else {
                    printf("  %24s", " ");
                }
                //printf("------ CALLING LAPACK SSYEVD TO COMPUTE only eigenvalue and verify elementswise -------  \n");
                lapackf77_ssyevd("N", "L", &N, 
                                h_A, &N, w2, 
                                h_work, &lwork, 
                                #ifdef COMPLEX
                                rwork, &lrwork, 
                                #endif
                                iwork, &liwork, 
                                &info);
                info_solution = check_solution(N, w2, w1, eps);
                
                bool okay = (info_solution == 0) && (info_ortho == 0) && (info_reduction == 0);
                status += ! okay;
                printf("  %s", (okay ? "ok" : "failed"));
            }
            printf("\n");

            TESTING_FREE_CPU( h_A   );
            TESTING_FREE_CPU( w1    );
            TESTING_FREE_CPU( w2    );
            TESTING_FREE_CPU( iwork );
            
            TESTING_FREE_PIN( h_R    );
            TESTING_FREE_PIN( h_work );
            #ifdef COMPLEX
            TESTING_FREE_PIN( rwork  );
            #endif
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }
printf("\n");

    opts.cleanup();
    TESTING_FINALIZE();
    return status;
}



/*-------------------------------------------------------------------
 * Check the orthogonality of Q
 */
static magma_int_t check_orthogonality(magma_int_t M, magma_int_t N, float *Q, magma_int_t LDQ, float eps)
{
    float d_one     =  1.0;
    float d_neg_one = -1.0;
    float c_zero    = MAGMA_S_ZERO;
    float c_one     = MAGMA_S_ONE;
    float  normQ, result;
    magma_int_t     info_ortho;
    magma_int_t     minMN = min(M, N);
    float *work;
    magma_smalloc_cpu( &work, minMN );

    /* Build the idendity matrix */
    float *Id;
    magma_smalloc_cpu( &Id, minMN*minMN );
    lapackf77_slaset("A", &minMN, &minMN, &c_zero, &c_one, Id, &minMN);

    /* Perform Id - Q'Q */
    if (M >= N)
        blasf77_ssyrk("U", "C", &N, &M, &d_one, Q, &LDQ, &d_neg_one, Id, &N);
    else
        blasf77_ssyrk("U", "N", &M, &N, &d_one, Q, &LDQ, &d_neg_one, Id, &M);

    normQ = safe_lapackf77_slansy("I", "U", &minMN, Id, &minMN, work);

    result = normQ / (minMN * eps);
    printf( "  %12.2e", normQ / minMN );
    //printf(" ======================================================\n");
    //printf(" ||Id-Q'*Q||_oo / (minMN*eps)          : %15.3E \n",  result );
    //printf(" ======================================================\n");

    if ( isnan(result) || isinf(result) || (result > 60.0) ) {
        //printf("-- Orthogonality is suspicious ! \n");
        info_ortho=1;
    }
    else {
        //printf("-- Orthogonality is CORRECT ! \n");
        info_ortho=0;
    }
    magma_free_cpu(work);
    magma_free_cpu(Id);
    return info_ortho;
}


/*------------------------------------------------------------
 *  Check the reduction 
 */
static magma_int_t check_reduction(magma_uplo_t uplo, magma_int_t N, magma_int_t bw, float *A, float *D, magma_int_t LDA, float *Q, float eps )
{
    float c_one     = MAGMA_S_ONE;
    float c_neg_one = MAGMA_S_NEG_ONE;
    float *TEMP, *Residual;
    float *work;
    float Anorm, Rnorm, result;
    magma_int_t info_reduction;
    magma_int_t i;
    magma_int_t ione=1;

    magma_smalloc_cpu( &TEMP, N*N );
    magma_smalloc_cpu( &Residual, N*N );
    magma_smalloc_cpu( &work, N );
    
    /* Compute TEMP =  Q * LAMBDA */
    lapackf77_slacpy("A", &N, &N, Q, &LDA, TEMP, &N);        
    for (i = 0; i < N; i++) {
        blasf77_sscal(&N, &D[i], &(TEMP[i*N]), &ione);
    }
    /* Compute Residual = A - Q * LAMBDA * Q^H */
    /* A is symmetric but both upper and lower 
     * are assumed valable here for checking 
     * otherwise it need to be symetrized before 
     * checking.
     */ 
    lapackf77_slacpy("A", &N, &N, A, &LDA, Residual, &N);        
    blasf77_sgemm("N", "C", &N, &N, &N, &c_neg_one, TEMP, &N, Q, &LDA, &c_one, Residual,     &N);

    // since A has been generated by larnv and we did not symmetrize, 
    // so only the uplo portion of A should be equal to Q*LAMBDA*Q^H 
    // for that Rnorm use slansy instead of slange
    Rnorm = safe_lapackf77_slansy("1", lapack_uplo_const(uplo), &N, Residual, &N, work);
    Anorm = safe_lapackf77_slansy("1", lapack_uplo_const(uplo), &N, A,        &LDA, work);

    result = Rnorm / ( Anorm * N * eps);
    printf("  %12.2e",  Rnorm / ( Anorm * N));
    //if ( uplo == MagmaLower ) {
    //    printf(" ======================================================\n");
    //    printf(" ||A-Q*LAMBDA*Q'||_oo/(||A||_oo.N.eps) : %15.3E \n",  result );
    //    printf(" ======================================================\n");
    //} else { 
    //    printf(" ======================================================\n");
    //    printf(" ||A-Q'*LAMBDA*Q||_oo/(||A||_oo.N.eps) : %15.3E \n",  result );
    //    printf(" ======================================================\n");
    //}

    if ( isnan(result) || isinf(result) || (result > 60.0) ) {
        //printf("-- Reduction is suspicious ! \n");
        info_reduction = 1;
    }
    else {
        //printf("-- Reduction is CORRECT ! \n");
        info_reduction = 0;
    }

    magma_free_cpu(TEMP);
    magma_free_cpu(Residual);
    magma_free_cpu(work);

    return info_reduction;
}


/*------------------------------------------------------------
 *  Check the eigenvalues 
 */
static magma_int_t check_solution(magma_int_t N, float *E1, float *E2, float eps)
{
    magma_int_t   info_solution, i;
    float unfl   = lapackf77_slamch("Safe minimum");
    float resid;
    float maxtmp;
    float maxdif = fabs( fabs(E1[0]) - fabs(E2[0]) );
    float maxeig = max( fabs(E1[0]), fabs(E2[0]) );
    for (i = 1; i < N; i++) {
        resid   = fabs(fabs(E1[i])-fabs(E2[i]));
        maxtmp  = max(fabs(E1[i]), fabs(E2[i]));

        /* Update */
        maxeig = max(maxtmp, maxeig);
        maxdif  = max(resid,  maxdif );
    }
    maxtmp = maxdif / max(unfl, eps*max(maxeig, maxdif));

    printf("  %12.2e", maxdif / (max(maxeig, maxdif)) );
    //printf(" ======================================================\n");
    //printf(" | D - eigcomputed | / (|D| * N * eps) : %15.3E \n",  maxtmp );
    //printf(" ======================================================\n");

    if ( isnan(maxtmp) || isinf(maxtmp) || (maxtmp > 100) ) {
        //printf("-- The eigenvalues are suspicious ! \n");
        info_solution = 1;
    }
    else {
        //printf("-- The eigenvalues are CORRECT ! \n");
        info_solution = 0;
    }
    return info_solution;
}
