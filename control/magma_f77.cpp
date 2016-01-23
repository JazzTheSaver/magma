#include "magma.h"
#include "magma_mangling.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
    -- MAGMA (version 2.0.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date January 2016
*/

#define magmaf_init FORTRAN_NAME( magmaf_init, MAGMAF_INIT )
void magmaf_init( void )
{
    magma_init();
}

#define magmaf_finalize FORTRAN_NAME( magmaf_finalize, MAGMAF_FINALIZE )
void magmaf_finalize( void )
{
    magma_finalize();
}

#ifdef __cplusplus
}
#endif
