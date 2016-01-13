/*
    -- MAGMA (version 2.0.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date January 2016

       @generated from sparse-iter/testing/testing_zmatrixinfo.cpp normal z -> s, Wed Jan  6 17:59:51 2016
       @author Hartwig Anzt
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
#include "common_magmasparse.h"



/* ////////////////////////////////////////////////////////////////////////////
   -- testing any solver
*/
int main(  int argc, char** argv )
{
    magma_int_t info = 0;
    TESTING_INIT();

    magma_sopts zopts;
    magma_queue_t queue=NULL;
    magma_queue_create( &queue );
    
    magma_s_matrix Z={Magma_CSR};
    
    int i=1;
    CHECK( magma_sparse_opts( argc, argv, &zopts, &i, queue ));
    printf("matrixinfo = [ \n");
    printf("%%   size (n)   ||   nonzeros (nnz)   ||   nnz/n \n");
    printf("%%======================================================="
                        "======%%\n");
    while( i < argc ) {
        if ( strcmp("LAPLACE2D", argv[i]) == 0 && i+1 < argc ) {   // Laplace test
            i++;
            magma_int_t laplace_size = atoi( argv[i] );
            CHECK( magma_sm_5stencil(  laplace_size, &Z, queue ));
        } else {                        // file-matrix test
            CHECK( magma_s_csr_mtx( &Z,  argv[i], queue ));
        }

        printf("   %10d          %10d          %10d\n",
               int(Z.num_rows),  int(Z.nnz), int(Z.nnz/Z.num_rows) );

        magma_smfree(&Z, queue );

        i++;
    }
    printf("%%======================================================="
    "======%%\n");
    printf("];\n");
        
cleanup:
    magma_smfree(&Z, queue );
    magma_queue_destroy( queue );
    TESTING_FINALIZE();
    return info;
}
