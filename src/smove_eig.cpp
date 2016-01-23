/*
    -- MAGMA (version 2.0.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date January 2016

       @author Raffaele Solca
       @author Azzam Haidar

       @generated from src/dmove_eig.cpp normal d -> s, Fri Jan 22 21:41:43 2016

*/
#include "magma_internal.h"

extern "C" void
magma_smove_eig(
    magma_range_t range, magma_int_t n, float *w, magma_int_t *il,
    magma_int_t *iu, float vl, float vu, magma_int_t *mout)
{
    magma_int_t valeig, indeig, i;

    valeig = (range == MagmaRangeV);
    indeig = (range == MagmaRangeI);

    if (indeig) {
        *mout = *iu - *il + 1;
        if (*il > 1)
            for (i = 0; i < *mout; ++i)
                w[i] = w[*il - 1 + i];
    }
    else if (valeig) {
        *il=1;
        *iu=n;
        for (i = 0; i < n; ++i) {
            if (w[i] > vu) {
                *iu = i;
                break;
            }
            else if (w[i] < vl)
                ++*il;
            else if (*il > 1)
                w[i-*il+1]=w[i];
        }
        *mout = *iu - *il + 1;
    }
    else {
        *il = 1;
        *iu = n;
        *mout = n;
    }

    return;
}
