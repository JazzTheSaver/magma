/*
    -- MAGMA (version 2.0.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date January 2016

       @author Hartwig Anzt

       @generated from sparse-iter/src/zpcgs_merge.cpp normal z -> s, Fri Jan 22 21:42:27 2016
*/

#include "magmasparse_internal.h"

#define RTOLERANCE     lapackf77_slamch( "E" )
#define ATOLERANCE     lapackf77_slamch( "E" )


/**
    Purpose
    -------

    Solves a system of linear equations
       A * X = B
    where A is a real matrix A.
    This is a GPU implementation of the preconditioned Conjugate
    Gradient Squared (CGS) method.

    Arguments
    ---------

    @param[in]
    A           magma_s_matrix
                input matrix A

    @param[in]
    b           magma_s_matrix
                RHS b

    @param[in,out]
    x           magma_s_matrix*
                solution approximation

    @param[in,out]
    solver_par  magma_s_solver_par*
                solver parameters

    @param[in]
    precond_par magma_s_preconditioner*
                preconditioner
                
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_sgesv
    ********************************************************************/

extern "C" magma_int_t
magma_spcgs_merge(
    magma_s_matrix A, magma_s_matrix b, magma_s_matrix *x,
    magma_s_solver_par *solver_par,
    magma_s_preconditioner *precond_par,
    magma_queue_t queue )
{
    magma_int_t info = MAGMA_NOTCONVERGED;
    
    // prepare solver feedback
    solver_par->solver = Magma_PCGS;
    solver_par->numiter = 0;
    solver_par->spmv_count = 0;
    
    // local variables
    float c_zero = MAGMA_S_ZERO, c_one = MAGMA_S_ONE;
    // solver variables
    float nom0, r0,  res, nomb;
    float rho, rho_l = c_one, alpha, beta;
    
    magma_int_t dofs = A.num_rows* b.num_cols;

    // GPU workspace
    magma_s_matrix r={Magma_CSR}, rt={Magma_CSR}, r_tld={Magma_CSR},
                    p={Magma_CSR}, q={Magma_CSR}, u={Magma_CSR}, v={Magma_CSR},  t={Magma_CSR},
                    p_hat={Magma_CSR}, q_hat={Magma_CSR}, u_hat={Magma_CSR}, v_hat={Magma_CSR};
    CHECK( magma_svinit( &r, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_svinit( &rt,Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_svinit( &r_tld,Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_svinit( &p, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_svinit( &p_hat, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_svinit( &q, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_svinit( &q_hat, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_svinit( &u, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_svinit( &u_hat, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_svinit( &v, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_svinit( &v_hat, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_svinit( &t, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));

    // solver setup
    CHECK(  magma_sresidualvec( A, b, *x, &r, &nom0, queue));
    magma_scopy( dofs, r.dval, 1, r_tld.dval, 1, queue );   

    solver_par->init_res = nom0;
            
    nomb = magma_snrm2( dofs, b.dval, 1, queue );
    if ( nomb == 0.0 ){
        nomb=1.0;
    }       
    if ( (r0 = nomb * solver_par->rtol) < ATOLERANCE ){
        r0 = ATOLERANCE;
    }
    solver_par->final_res = solver_par->init_res;
    solver_par->iter_res = solver_par->init_res;
    if ( solver_par->verbose > 0 ) {
        solver_par->res_vec[0] = (real_Double_t)nom0;
        solver_par->timing[0] = 0.0;
    }
    if ( nom0 < r0 ) {
        info = MAGMA_SUCCESS;
        goto cleanup;
    }

    //Chronometry
    real_Double_t tempo1, tempo2, tempop1, tempop2;
    tempo1 = magma_sync_wtime( queue );
    
    solver_par->numiter = 0;
    solver_par->spmv_count = 0;
    // start iteration
    do
    {
        solver_par->numiter++;
        
        rho = magma_sdot( dofs, r.dval, 1, r_tld.dval, 1, queue );
                                                            // rho = < r,r_tld>    
        if ( MAGMA_S_ABS(rho) == 0.0 ) {
            goto cleanup;
        }
        
        if ( solver_par->numiter > 1 ) {                        // direction vectors
            beta = rho / rho_l;            
            magma_scgs_1(  
            r.num_rows, 
            r.num_cols, 
            beta,
            r.dval,
            q.dval, 
            u.dval,
            p.dval,
            queue );
          //u = r + beta*q;
          //p = u + beta*( q + beta*p );
        }
        else{
            magma_scgs_2(  
            r.num_rows, 
            r.num_cols, 
            r.dval,
            u.dval,
            p.dval,
            queue );
            // u = r
            // p = r
        }
        // preconditioner
        tempop1 = magma_sync_wtime( queue );
        CHECK( magma_s_applyprecond_left( MagmaNoTrans, A, p, &rt, precond_par, queue ));
        CHECK( magma_s_applyprecond_right( MagmaNoTrans, A, rt, &p_hat, precond_par, queue ));
        tempop2 = magma_sync_wtime( queue );
        precond_par->runtime += tempop2-tempop1;
        
        CHECK( magma_s_spmv( c_one, A, p_hat, c_zero, v_hat, queue ));   // v = A p
        solver_par->spmv_count++;
        alpha = rho / magma_sdot( dofs, r_tld.dval, 1, v_hat.dval, 1, queue );
        
        magma_scgs_3(  
        r.num_rows, 
        r.num_cols, 
        alpha,
        v_hat.dval,
        u.dval, 
        q.dval,
        t.dval, 
        queue );
        // q = u - alpha v_hat
        // t = u + q
        
        // preconditioner
        tempop1 = magma_sync_wtime( queue );
        CHECK( magma_s_applyprecond_left( MagmaNoTrans, A, t, &rt, precond_par, queue ));
        CHECK( magma_s_applyprecond_right( MagmaNoTrans, A, rt, &u_hat, precond_par, queue ));
        tempop2 = magma_sync_wtime( queue );
        precond_par->runtime += tempop2-tempop1;
        
        CHECK( magma_s_spmv( c_one, A, u_hat, c_zero, t, queue ));   // t = A u_hat
        solver_par->spmv_count++;
        magma_scgs_4(  
        r.num_rows, 
        r.num_cols, 
        alpha,
        u_hat.dval,
        t.dval,
        x->dval, 
        r.dval,
        queue );
        // r = r -alpha*A u_hat
        // x = x + alpha u_hat
        
        res = magma_snrm2( dofs, r.dval, 1, queue );
        if ( solver_par->verbose > 0 ) {
            tempo2 = magma_sync_wtime( queue );
            if ( (solver_par->numiter)%solver_par->verbose == 0 ) {
                solver_par->res_vec[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) res;
                solver_par->timing[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) tempo2-tempo1;
            }
        }

        if ( res/nomb <= solver_par->rtol || res <= solver_par->atol ){
            break;
        }
        rho_l = rho;
    }
    while ( solver_par->numiter+1 <= solver_par->maxiter );
    
    tempo2 = magma_sync_wtime( queue );
    solver_par->runtime = (real_Double_t) tempo2-tempo1;
    float residual;
    CHECK(  magma_sresidualvec( A, b, *x, &r, &residual, queue));
    solver_par->iter_res = res;
    solver_par->final_res = residual;

    if ( solver_par->numiter < solver_par->maxiter ) {
        info = MAGMA_SUCCESS;
    } else if ( solver_par->init_res > solver_par->final_res ) {
        if ( solver_par->verbose > 0 ) {
            if ( (solver_par->numiter)%solver_par->verbose == 0 ) {
                solver_par->res_vec[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) res;
                solver_par->timing[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) tempo2-tempo1;
            }
        }
        info = MAGMA_SLOW_CONVERGENCE;
        if( solver_par->iter_res < solver_par->rtol*solver_par->init_res ||
            solver_par->iter_res < solver_par->atol ) {
            info = MAGMA_SUCCESS;
        }
    }
    else {
        if ( solver_par->verbose > 0 ) {
            if ( (solver_par->numiter)%solver_par->verbose == 0 ) {
                solver_par->res_vec[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) res;
                solver_par->timing[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) tempo2-tempo1;
            }
        }
        info = MAGMA_DIVERGENCE;
    }
    
cleanup:
    magma_smfree(&r, queue );
    magma_smfree(&rt, queue );
    magma_smfree(&r_tld, queue );
    magma_smfree(&p, queue );
    magma_smfree(&q, queue );
    magma_smfree(&u, queue );
    magma_smfree(&v, queue );
    magma_smfree(&t, queue );
    magma_smfree(&p_hat, queue );
    magma_smfree(&q_hat, queue );
    magma_smfree(&u_hat, queue );
    magma_smfree(&v_hat, queue );

    solver_par->info = info;
    return info;
}   /* magma_spcgs_merge */
