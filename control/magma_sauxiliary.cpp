/*
    -- MAGMA (version 2.0.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date January 2016
       
       @author Mark Gates
       @generated from control/magma_zauxiliary.cpp normal z -> s, Fri Jan 22 21:41:26 2016
*/
#include "magma_internal.h"

#define PRECISION_s

/**
    Purpose
    -------
    This deals with a subtle bug with returning lwork as a Float.
    If lwork > 2**24, then it will get rounded as a Float;
    we need to ensure it is rounded up instead of down,
    by multiplying by 1.+eps in Double precision:
        float( 16777217            ) == 16777216
        float( 16777217 * (1.+eps) ) == 16777218
    (Could use 1+2*eps in Single precision, but that can add more than necesary.)
    If lwork > 2**53, rounding would happen in Double, too, but that's 94M x 94M!

    @ingroup magma_util
    ********************************************************************/
float magma_smake_lwork( magma_int_t lwork )
{
    #if defined(PRECISION_s) || defined(PRECISION_c)
    real_Double_t one_eps = 1. + lapackf77_slamch("Epsilon");
    return MAGMA_S_MAKE( float(lwork*one_eps), 0 );
    #else
    return MAGMA_S_MAKE( float(lwork), 0 );
    #endif
}
