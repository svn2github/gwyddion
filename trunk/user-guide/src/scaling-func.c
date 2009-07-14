#include <string.h>
#include <math.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_matrix_double.h>
#include <gsl/gsl_linalg.h>
#include "wavelet.h"

static int inital_guess(const long int K,
                        const double *c,
                        double *g);

/* compute scaling function and orthogonal wavelet from K coefficients c,
 * storing scaling function into array phi of size K<<scaling_steps
 * and wavelet into array psi of the same size */
int
scaling_function(const long int scaling_steps, double *phi, double *psi,
                 const long int K, const double *c)
{
    const long int unity = 1 << scaling_steps;
    const long int n = K*unity;
    long int step, i, k;

    /* compute the scaling function
     * since we need only values at even positions to compute next iteration,
     * we can start with values spaced <unity> and then gradually refine to
     * <unity>/2, <unity>/4, ..., 2, 1
     */
    {
        double guess[K-2];
        inital_guess(K, c, guess);
        for (i = 1; i < K-1; i++)
            phi[i*unity] = guess[i-1];
        phi[0] = phi[K-1] = 0;
    }
    for (step = unity/2; step > 0; step >>= 1) {
        /* zero psi */
        for (i = 0; i < n; i += step)
            psi[i] = 0;
        /* compute convolution */
        for (k = 0; k < K; k++) {
            const long int shift = unity*k/2;
            for (i = shift; i < shift + n/2; i += step)
                psi[i] += c[k]*phi[2*i - unity*k];
        }
        /* move psi to phi */
        {
            double *tmp = phi;
            phi = psi;
            psi = tmp;
        }
    }

    /* one more step to improve convergence */
    /* zero psi */
    for (i = 0; i < n; i++) psi[i] = 0;
    /* compute convolution storing it into psi */
    for (k = 0; k < K; k++) {
        const long int shift = unity*k/2;
        for (i = shift; i < shift + n/2; i++)
            psi[i] += c[k]*phi[2*i - unity*k];
    }
    /* move psi to phi */
    if (scaling_steps & 1) {
        double *tmp = phi;
        phi = psi;
        psi = tmp;
    }

    /* renormalize integral to 1 */
    {
        double s;
        for (s = i = 0; i < n; i++) s += phi[i];
        s /= unity;
        for (i = 0; i < n; i++) phi[i] /= s;
    }

    /* compute wavelet and put it to psi
     * FIXME: this works _ONLY_ for orthogonal wavelets, so splines are out */
    {
        const long int Keven = (K+1)/2*2;
        /* zero psi */
        for (i = 0; i < n; i++) psi[i] = 0;
        /* compute convolution */
        for (k = 0; k < Keven; k += 2) {
            const long int shift = unity*k/2;
            const long int shift2 = unity*(k+1)/2;
            if (Keven < K+k+1) {
                for (i = shift; i < shift + n/2; i++)
                    psi[i] += c[Keven-1 - k]*phi[2*i - unity*k];
            }
            for (i = shift2; i < shift2 + n/2; i++)
                psi[i] -= c[Keven-1 - (k+1)]*phi[2*i - unity*(k+1)];
        }
    }

    return 0;
}

static int
inital_guess(const long int K, const double *c, double *g)
{
    const long int n = K-2;
    double *A = (double*)xmalloc(n*n*sizeof(double));
    double *rhs = (double*)xmalloc(n*sizeof(double));
    gsl_matrix_view A_mat = gsl_matrix_view_array(A, n, n);
    gsl_vector_view rhs_vec = gsl_vector_view_array(rhs, n);
    gsl_vector_view g_vec = gsl_vector_view_array(g, n);
    gsl_permutation *P = gsl_permutation_alloc(n);
    int i, j, sgn;

    /* construct equations for phi[k], k \in Z drived from scaling equation
     * note since phi[0] = phi[K-1] = 0, we have only N-2 equations */
    for (i = 0; i < n; i++) {
        double *Ai = A + n*i;
        const long int shift = 2*(i+1);
        for (j = 0; j < shift - K; j++)
            Ai[j] = 0;
        for (j = GSL_MAX(0, shift - K); j < GSL_MIN(shift, n); j++)
            Ai[j] = c[shift-1 - j];
        for (j = shift; j < n; j++)
            Ai[j] = 0;
    }
    /* eigenvalue 1 */
    for (i = 0; i < n; i++)
        A[i*n + i] -= 1;
    /* one equation is redundant, replace the last with \sum_k phi[k] = 1 */
    for (i = 0; i < n; i++)
        A[n*(n-1) + i] = 1;
    /* rhs */
    for (i = 0; i < n; i++)
        rhs[i] = 0;
    rhs[n-1] = 1;

    /* solve it */
    gsl_linalg_LU_decomp(&A_mat.matrix, P, &sgn);
    gsl_linalg_LU_solve(&A_mat.matrix, P, &rhs_vec.vector, &g_vec.vector);

    gsl_permutation_free(P);
    A = xfree(A);
    rhs = xfree(rhs);

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
