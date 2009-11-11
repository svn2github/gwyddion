#include <string.h>
#include <math.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_matrix_double.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_permute_double.h>
#include <gsl/gsl_multiroots.h>
#include "wavelet.h"

typedef struct daubechies_param_t {
    long int p;
    double **F;
} daubechies_param_t;

static int daubechies_fdf(const gsl_vector *x,
                          void *params,
                          gsl_vector *f,
                          gsl_matrix *j);
static int daubechies_f  (const gsl_vector *x,
                          void *params,
                          gsl_vector *f);
static int daubechies_df (const gsl_vector *x,
                          void *params,
                          gsl_matrix *j);

void
print_matrix(double *matrix, long int rows, long int cols)
{
    long int i,j;
    for (i = 0; i < rows; i++) {
        double *mi = matrix + i*cols;
        for (j = 0; j < cols; j++) {
            fprintf(stderr, "% 0.0g\t", mi[j]);
        }
        fputs("\n", stderr);
    }
    fputs("\n", stderr);
}

/* compute coefficitens for Daubechies spline of (even) order n, storing it
 * into array c[] */
int
coefficients_daubechies(const long int n, double *c)
{
    const long int p = n/2;
    const long int size = n+1;
    double *A;
    double *Q;
    double **F;
    long int i, j, k;
    gsl_permutation *permutation = gsl_permutation_calloc(n+1);
    size_t *P = gsl_permutation_data(permutation);
    int err;

    if (2*p != n)
        GSL_ERROR("n must be even", GSL_EINVAL);

    /* construct the p+1 linear equations
     * minus rhs is in (n+1)-th column, we do most transformations together */
    A = (double*)xmalloc((p+1)*size*sizeof(double));
    for (j = 0; j < n; j++)
        A[j] = 1;
    for (j = 0; j < n; j++) {
        double jpow = 1 - 2*(j%2); /* (-1)^j */
        for (i = 1; i < p+1; i++) {
            A[i*size + j] = jpow;
            jpow *= j;
        }
    }
    A[n] = -2;
    for (i = 1; i < p+1; i++)
        A[size*i + n] = 0;

    /* express dependent coefficients using the independent ones
     * FIXME: gsl can do singular decomposition but I don't know how (if) the
     * result can be used for my needs, ie. reduction of degrees of freedom
     *
     * we do column pivoting; row pivoting should be `automatic' due to
     * downto <k> loop*/
    for (k = p; k >= 0; k--) {
        double *Ak = A + k*size;
        long int max = 0;
        long int maxP;
        /* find column pivot */
        for (j = 1; j < n; j++)
            if (fabs(Ak[P[j]]) > fabs(Ak[P[max]]))
                max = j;
        maxP = P[max];
        /* eliminate */
        for (i = 0; i < k; i++) {
            double *Ai = A + i*size;
            double z = Ai[maxP]/Ak[maxP];
            for (j = p-k; j < n; j++)
                Ai[P[j]] -= z*Ak[P[j]];
            Ai[maxP] = 0; /* fix rounding */
            /* rhs: always subtracts zero here */
        }
        {
            double z = Ak[maxP];
            for (j = 0; j < size; j++)
                Ak[P[j]] /= z;
        }
        gsl_permutation_swap(permutation, p-k, max);
    }
    /* back-substitute to completely A to an identity block and a regular block */
    for (k = 0; k < p; k++) {
        double *Ak = A + k*size;
        for (i = k+1; i < p+1; i++) {
            double *Ai = A + i*size;
            double z = Ai[P[p-k]];
            for (j = p+1; j < size; j++)
                Ai[P[j]] -= z*Ak[P[j]];
            for (j = p-k; j < p+1; j++)
                Ai[P[j]] = 0; /* never accessed? */
        }
    }

    /* create the actual coefficient-number-reduction matrix Q
     *
     *        / 0 0 ... 0 1 x x x x \
     *        | 0 0 ... 1 0 x x x x |
     *  P.A = | ... ... ... x x x x |
     *        | 0 1 ... 0 0 x x x x |
     *        \ 1 0 ... 0 0 x x x x /
     *
     *        / x x x x \
     *        | x x x x |
     *        | ....... |
     *  Q.P = | x x x x |
     *        | 1 0 0 0 |
     *        | 0 1 0 0 |
     *        | ....... |
     *        \ 0 0 0 1 /
     */
    Q = (double*)xmalloc(size*p*sizeof(double));
    /* upper regular block */
    for (i = 0; i < p+1; i++) {
        double *Qi = Q + p*(p - i);
        double *Ai = A + size*i;
        for (j = 0; j < p; j++)
            Qi[j] = -Ai[P[j + p+1]];
    }
    /* lower identity block */
    for (i = p+1; i < size; i++) {
        double *Qi = Q + p*i;
        for (j = 0; j < p; j++)
            Qi[j] = i-(p+1) == j ? 1 : 0;
    }
    /* apply permutation inverse to rows */
    for (j = 0; j < p; j++)
        gsl_permute_inverse(P, Q+j, p, size);

    /* construct the p-1 quadratic equations (expressed as quadratic forms)
     * Q^T F Q
     * last column and row of F contain absolute terms (used separately later) */
    F = (double**)xmalloc((p-1)*sizeof(double*));
    for (k = 0; k < p-1; k++)
        F[k] = (double*)xmalloc(p*p*sizeof(double));
    {
        double *M = (double*)xmalloc(size*size*sizeof(double));
        double *tmp = (double*)xmalloc(size*p*sizeof(double));
        gsl_matrix_view M_mat = gsl_matrix_view_array(M, size, size);
        gsl_matrix_view tmp_mat = gsl_matrix_view_array(tmp, size, p);
        gsl_matrix_view Q_mat = gsl_matrix_view_array(Q, size, p);
        long int step;

        /* the nonlinear equations have form
         * \sum_k a_k a_{k+2*l} = 0,
         * below step = 2*l, we do not include the equation for l = 0 */
        for (step = 2; step < n; step += 2) {
            k = step/2 - 1;
            gsl_matrix_view f_mat = gsl_matrix_view_array(F[k], p, p);
            for (k = 0; k < p-1; k++) {
                for (i = 0; i < size*size; i++) M[i] = 0.0;
                for (i = 0; i < size-1 - step; i++)
                    M[size*i + i+step] = M[size*(i + step) + i] = 1.0;
            }
            /* reduce size-by-size M to p-by-p F using Q */
            gsl_blas_dgemm(CblasNoTrans, CblasNoTrans,
                           1.0, &M_mat.matrix, &Q_mat.matrix,
                           0.0, &tmp_mat.matrix);
            gsl_blas_dgemm(CblasTrans, CblasNoTrans,
                           1.0, &Q_mat.matrix, &tmp_mat.matrix,
                           0.0, &f_mat.matrix);
        }
        M = xfree(M);
        tmp = xfree(tmp);
    }

    /* find root of the quadratic forms */
    {
        gsl_multiroot_function_fdf fdf;
        daubechies_param_t dpar;
        const gsl_multiroot_fdfsolver_type *T = gsl_multiroot_fdfsolver_hybridsj;
        gsl_multiroot_fdfsolver *solver = gsl_multiroot_fdfsolver_alloc(T, p-1);

        fdf.f = &daubechies_f;
        fdf.df = &daubechies_df;
        fdf.fdf = &daubechies_fdf;
        fdf.n = p-1;
        fdf.params = &dpar;
        dpar.F = F;
        dpar.p = p;

        {
            double x[p-1];
            gsl_vector_view x_vec = gsl_vector_view_array(x, p-1);
            for (j = 0; j < p-1; j++)
                x[j] = 0.0;
            gsl_multiroot_fdfsolver_set(solver, &fdf, &x_vec.vector);
        }

        do {
            err = gsl_multiroot_fdfsolver_iterate(solver);
        } while (!err
                 && gsl_multiroot_test_residual(solver->f, 1e-16) != GSL_SUCCESS);

        {
            double tmp[p];
            double cm[size];
            gsl_vector *xmin = gsl_multiroot_fdfsolver_root(solver);
            gsl_vector_view tmp_vec = gsl_vector_view_array(tmp, p);
            gsl_matrix_view Q_mat = gsl_matrix_view_array(Q, size, p);
            gsl_vector_view cm_vec = gsl_vector_view_array(cm, size);

            for (j = 0; j < p-1; j++)
                tmp[j] = gsl_vector_get(xmin, j);
            tmp[p-1] = 1;
            gsl_blas_dgemv(CblasNoTrans,
                           1.0, &Q_mat.matrix, &tmp_vec.vector,
                           0.0, &cm_vec.vector);
            for (j = 0; j < n; j++)
                c[j] = cm[n-1 - j];

            gsl_multiroot_fdfsolver_free(solver);
        }

    }

    for (k = 0; k < p-1; k++)
        F[k] = xfree(F[k]);
    F = xfree(F);
    Q = xfree(Q);
    A = xfree(A);

    return 0;
}

/* the nonlinear expression values and derivatives
 * for gsl multiroot fdf solver */
static int
daubechies_fdf(const gsl_vector *x, void *params, gsl_vector *f, gsl_matrix *j)
{
    const daubechies_param_t *dpar = (daubechies_param_t *)params;
    const long int p = dpar->p;
    double **F = dpar->F;

    long int k;

    for (k = 0; k < p-1; k++) {
        gsl_matrix_view f_mat = gsl_matrix_view_array_with_tda(F[k], p-1, p-1, p);
        gsl_vector_view f_lin = gsl_vector_view_array(F[k] + p*(p-1), p-1);
        gsl_vector_view g = gsl_matrix_row(j, k);
        double f_abs = F[k][p*p - 1];
        double fval, linval;

        gsl_blas_dgemv(CblasNoTrans, 1.0, &f_mat.matrix, x, 0.0, &g.vector);
        gsl_blas_ddot(&g.vector, x, &fval);
        gsl_blas_ddot(&f_lin.vector, x, &linval);
        gsl_vector_set(f, k, fval + 2.0*linval + f_abs);

        gsl_blas_daxpy(1.0, &f_lin.vector, &g.vector);
        gsl_blas_dscal(2.0, &g.vector);
    }

    return 0;
}

static int
daubechies_f(const gsl_vector *x, void *params, gsl_vector *f)
{
    const daubechies_param_t *dpar = (daubechies_param_t *)params;
    const long int p = dpar->p;
    double **F = dpar->F;

    double g[p-1];
    gsl_vector_view g_vec = gsl_vector_view_array(g, p-1);
    long int k;

    for (k = 0; k < p-1; k++) {
        gsl_matrix_view f_mat = gsl_matrix_view_array_with_tda(F[k], p-1, p-1, p);
        gsl_vector_view f_lin = gsl_vector_view_array(F[k] + p*(p-1), p-1);
        double f_abs = F[k][p*p - 1];
        double fval, linval;

        gsl_blas_dgemv(CblasNoTrans, 1.0, &f_mat.matrix, x, 0.0, &g_vec.vector);
        gsl_blas_ddot(&g_vec.vector, x, &fval);
        gsl_blas_ddot(&f_lin.vector, x, &linval);
        gsl_vector_set(f, k, fval + 2.0*linval + f_abs);
    }

    return 0;
}

static int
daubechies_df(const gsl_vector *x, void *params, gsl_matrix *j)
{
    const daubechies_param_t *dpar = (daubechies_param_t *)params;
    const long int p = dpar->p;
    double **F = dpar->F;

    long int k;

    for (k = 0; k < p-1; k++) {
        gsl_matrix_view f_mat = gsl_matrix_view_array_with_tda(F[k], p-1, p-1, p);
        gsl_vector_view f_lin = gsl_vector_view_array(F[k] + p*(p-1), p-1);
        gsl_vector_view g = gsl_matrix_row(j, k);

        gsl_blas_dgemv(CblasNoTrans, 2.0, &f_mat.matrix, x, 0.0, &g.vector);
        gsl_blas_daxpy(2.0, &f_lin.vector, &g.vector);
    }

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
