/*
    -- MAGMA (version 2.0.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date January 2016

       @author Mark Gates

       @generated from src/zunglq.cpp normal z -> c, Wed Jan  6 17:59:32 2016

*/
#include "magma_internal.h"

/**
    Purpose:
    ---------
    CUNGLQ generates an M-by-N complex matrix Q with orthonormal rows,
    which is defined as the first M rows of a product of K elementary
    reflectors of order N

        Q  =  H(k)**H . . . H(2)**H H(1)**H

    as returned by CGELQF.

    Arguments:
    ---------
    @param[in]
    m       INTEGER
            The number of rows of the matrix Q. M >= 0.
    
    @param[in]
    n       INTEGER
            The number of columns of the matrix Q. N >= M.
    
    @param[in]
    k       INTEGER
            The number of elementary reflectors whose product defines the
            matrix Q. M >= K >= 0.
    
    @param[in,out]
    A       COMPLEX array, dimension (LDA,N)
            On entry, the i-th row must contain the vector which defines
            the elementary reflector H(i), for i = 1,2,...,k, as returned
            by CGELQF in the first k rows of its array argument A.
            On exit, the M-by-N matrix Q.
    
    @param[in]
    lda     INTEGER
            The first dimension of the array A. LDA >= max(1,M).
    
    @param[in]
    tau     COMPLEX array, dimension (K)
            TAU(i) must contain the scalar factor of the elementary
            reflector H(i), as returned by CGELQF.
    
    @param[out]
    work    COMPLEX array, dimension (MAX(1,LWORK))
            On exit, if INFO = 0, WORK(1) returns the optimal LWORK.
    
    @param[in]
    lwork   INTEGER
            The dimension of the array WORK. LWORK >= NB*NB, where NB is
            the optimal blocksize.
    
            If LWORK = -1, a workspace query is assumed; the routine
            only calculates the optimal size of the WORK array, returns
            this value as the first entry of the WORK array, and no error
            message related to LWORK is issued by XERBLA.
    
    @param[out]
    info    INTEGER
      -     = 0:  successful exit;
      -     < 0:  if INFO = -i, the i-th argument had an illegal value

    @ingroup magma_cgelqf_comp
    ********************************************************************/
extern "C" magma_int_t
magma_cunglq(
    magma_int_t m, magma_int_t n, magma_int_t k,
    magmaFloatComplex *A, magma_int_t lda,
    magmaFloatComplex *tau,
    magmaFloatComplex *work, magma_int_t lwork,
    magma_int_t *info)
{
    #define  A(i_,j_)  ( A + (i_) + (j_)*lda)
    #define dA(i_,j_)  (dA + (i_) + (j_)*ldda)
    #define tau(i_)    (tau + (i_))
    
    // Constants
    const magmaFloatComplex c_zero = MAGMA_C_ZERO;
    const magmaFloatComplex c_one  = MAGMA_C_ONE;
    
    // Local variables
    bool lquery;
    magma_int_t i, ib, ki, ldda, lddwork, lwkopt, mib, nb, n_i;
    magma_queue_t queue = NULL;
    magmaFloatComplex_ptr dA = NULL;
    magmaFloatComplex* work2 = NULL;
    
    // Test the input arguments
    *info = 0;
    nb = magma_get_cgelqf_nb( m, n );
    lwkopt = nb*nb;
    work[0] = MAGMA_C_MAKE( lwkopt, 0 );
    lquery = (lwork == -1);
    if (m < 0) {
        *info = -1;
    } else if (n < 0 || n < m) {
        *info = -2;
    } else if (k < 0 || k > m) {
        *info = -3;
    } else if (lda < max( 1, m )) {
        *info = -5;
    } else if (lwork < max( 1, lwkopt ) && ! lquery) {
        *info = -8;
        //printf( "m %d, n %d, nb %d: lwork %d, required %d\n", m, n, nb, lwork, lwkopt );
        //*info = 0;
    }
    
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }
    else if (lquery) {
        return *info;
    }
    
    // Quick return if possible
    if (m <= 0) {
        work[0] = MAGMA_C_MAKE( 1, 0 );
        return *info;
    }
    
    //if (lwork < lwkopt) {
    //    magma_cmalloc_cpu( &work2, lwkopt );
    //}
    //else {
    //    work2 = work;
    //}
    work2 = work;
    
    // Allocate GPU work space
    // ldda*n     for matrix dA
    // nb*n       for dV
    // lddwork*nb for dW larfb workspace
    ldda    = magma_roundup( m, 32 );
    lddwork = magma_roundup( m, 32 );
    if (MAGMA_SUCCESS != magma_cmalloc( &dA, ldda*n + n*nb + lddwork*nb + nb*nb )) {
        *info = MAGMA_ERR_DEVICE_ALLOC;
        goto cleanup;
    }
    
    magmaFloatComplex_ptr dV; dV = dA + ldda*n;
    magmaFloatComplex_ptr dW; dW = dA + ldda*n + n*nb;
    magmaFloatComplex_ptr dT; dT = dA + ldda*n + n*nb + lddwork*nb;
    
    magma_device_t cdev;
    magma_getdevice( &cdev );
    magma_queue_create( cdev, &queue );
    
    magmablas_claset( MagmaFull, m, n, MAGMA_C_NAN, MAGMA_C_NAN, dA, ldda, queue );
    
    // all columns are handled by blocked method.
    // ki is start of last (partial) block
    ki = ((k - 1) / nb) * nb;
    
    // Use blocked code
    for( i=ki; i >= 0; i -= nb ) {
        ib = min( nb, k-i );
        // first block has extra rows to update
        mib = ib;
        if ( i == ki ) {
            mib = m - i;
        }
        
        // Send current panel of V (block row) to the GPU
        lapackf77_claset( "Lower", &ib, &ib, &c_zero, &c_one, A(i,i), &lda );
        // TODO: having this _async was causing numerical errors. Why?
        magma_csetmatrix( ib, n-i,
                                A(i,i), lda,
                                dV,     nb, queue );
        
        // Form the triangular factor of the block reflector
        // H = H(i) H(i+1) . . . H(i+ib-1)
        n_i = n - i;
        lapackf77_clarft( MagmaForwardStr, MagmaRowwiseStr, &n_i, &ib,
                          A(i,i), &lda, &tau[i], work2, &nb );
        magma_csetmatrix_async( ib, ib,
                                work2, nb,
                                dT,   nb, queue );
        
        // set panel of A (block row) to identity
        magmablas_claset( MagmaFull, mib, i,   c_zero, c_zero, dA(i,0), ldda, queue );
        magmablas_claset( MagmaFull, mib, n-i, c_zero, c_one,  dA(i,i), ldda, queue );
        
        if (i < m) {
            // Apply H**H to A(i:m,i:n) from the right
            magma_clarfb_gpu( MagmaRight, MagmaConjTrans, MagmaForward, MagmaRowwise,
                              m-i, n-i, ib,
                              dV, nb,        dT, nb,
                              dA(i,i), ldda, dW, lddwork, queue );
        }
    }

    // copy result back to CPU
    magma_cgetmatrix( m, n,
                      dA(0,0), ldda, A(0,0), lda, queue );

cleanup:
    magma_queue_destroy( queue );
    magma_free( dA );
    
    //if (work2 != work) {
    //    magma_free_cpu( work2 );
    //}
    
    work[0] = MAGMA_C_MAKE( lwkopt, 0 );
    return *info;
}
