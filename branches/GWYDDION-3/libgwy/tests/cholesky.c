/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "testlibgwy.h"

/***************************************************************************
 *
 * Cholesky
 *
 ***************************************************************************/

#define CHOLESKY_MATRIX_LEN(n) (((n) + 1)*(n)/2)
#define SLi gwy_lower_triangular_matrix_index

/* Square a triangular matrix. */
static void
test_cholesky_matsquare(gdouble *a, const gdouble *d, guint n)
{
    for (guint i = 0; i < n; i++) {
        for (guint j = 0; j <= i; j++) {
            SLi(a, i, j) = SLi(d, i, 0) * SLi(d, j, 0);
            for (guint k = 1; k <= MIN(i, j); k++)
                SLi(a, i, j) += SLi(d, i, k) * SLi(d, j, k);
        }
    }
}

/* Multiply two symmetrical matrices (NOT triangular). */
static void
test_cholesky_matmul(gdouble *a, const gdouble *d1, const gdouble *d2, guint n)
{
    for (guint i = 0; i < n; i++) {
        for (guint j = 0; j <= i; j++) {
            SLi(a, i, j) = 0.0;
            for (guint k = 0; k < n; k++) {
                guint ik = MAX(i, k);
                guint ki = MIN(i, k);
                guint jk = MAX(j, k);
                guint kj = MIN(j, k);
                SLi(a, i, j) += SLi(d1, ik, ki) * SLi(d2, jk, kj);
            }
        }
    }
}

/* Multiply a vector with a symmetrical matrix (NOT triangular) from left. */
static void
test_cholesky_matvec(gdouble *a, const gdouble *m, const gdouble *v, guint n)
{
    for (guint i = 0; i < n; i++) {
        a[i] = 0.0;
        for (guint j = 0; j < n; j++) {
            guint ij = MAX(i, j);
            guint ji = MIN(i, j);
            a[i] += SLi(m, ij, ji) * v[j];
        }
    }
}

/* Generate the decomposition.  As long as it has positive numbers on the
 * diagonal, the matrix is positive-definite.   Though maybe not numerically.
 */
static void
test_cholesky_make_matrix(gdouble *d,
                          guint n,
                          GRand *rng)
{
    for (guint j = 0; j < n; j++) {
        SLi(d, j, j) = exp(g_rand_double_range(rng, -5.0, 5.0));
        for (guint k = 0; k < j; k++) {
            SLi(d, j, k) = (g_rand_double_range(rng, -1.0, 1.0)
                            + g_rand_double_range(rng, -1.0, 1.0)
                            + g_rand_double_range(rng, -1.0, 1.0))/5.0;
        }
    }
}

static void
test_cholesky_make_vector(gdouble *d,
                          guint n,
                          GRand *rng)
{
    for (guint j = 0; j < n; j++)
        d[j] = g_rand_double_range(rng, -1.0, 1.0)
               * exp(g_rand_double_range(rng, -5.0, 5.0));
}

/* Note the precision checks are very tolerant as the matrices we generate
 * are not always well-conditioned. */
void
test_math_cholesky(void)
{
    guint nmax = 8, niter = 50;
    GRand *rng = g_rand_new();

    /* Make it realy reproducible. */
    g_rand_set_seed(rng, 42);

    for (guint n = 1; n < nmax; n++) {
        guint matlen = CHOLESKY_MATRIX_LEN(n);
        /* Use descriptive names for less cryptic g_assert() messages. */
        gdouble *matrix = g_new(gdouble, matlen);
        gdouble *decomp = g_new(gdouble, matlen);
        gdouble *inverted = g_new(gdouble, matlen);
        gdouble *unity = g_new(gdouble, matlen);
        gdouble *vector = g_new(gdouble, n);
        gdouble *solution = g_new(gdouble, n);

        for (guint iter = 0; iter < niter; iter++) {
            test_cholesky_make_matrix(decomp, n, rng);
            test_cholesky_make_vector(vector, n, rng);
            gdouble eps;

            /* Decomposition */
            test_cholesky_matsquare(matrix, decomp, n);
            test_cholesky_matvec(solution, matrix, vector, n);
            g_assert(gwy_cholesky_decompose(matrix, n));
            for (guint j = 0; j < matlen; j++) {
                eps = gwy_powi(10.0, (gint)j - 15);
                g_assert_cmpfloat(fabs(matrix[j] - decomp[j]),
                                  <=,
                                  eps * (fabs(matrix[j]) + fabs(decomp[j])));
            }

            /* Solution */
            eps = gwy_powi(10.0, (gint)n - 11);
            gwy_cholesky_solve(matrix, solution, n);
            for (guint j = 0; j < n; j++) {
                g_assert_cmpfloat(fabs(solution[j] - vector[j]),
                                  <=,
                                  eps * (fabs(solution[j]) + fabs(vector[j])));
            }

            /* Inversion */
            test_cholesky_matsquare(matrix, decomp, n);
            memcpy(inverted, matrix, matlen*sizeof(gdouble));
            g_assert(gwy_cholesky_invert(inverted, n));
            /* Multiplication with inverted must give unity */
            test_cholesky_matmul(unity, matrix, inverted, n);
            for (guint j = 0; j < n; j++) {
                eps = gwy_powi(10.0, (gint)n - 10);
                for (guint k = 0; k < j; k++) {
                    g_assert_cmpfloat(fabs(SLi(unity, j, k)), <=, eps);
                }
                eps = gwy_powi(10.0, (gint)n - 11);
                g_assert_cmpfloat(fabs(SLi(unity, j, j) - 1.0), <=, eps);
            }
            /* Double inversion must give the original */
            eps = gwy_powi(10.0, (gint)n - 12);
            g_assert(gwy_cholesky_invert(inverted, n));
            for (guint j = 0; j < matlen; j++) {
                g_assert_cmpfloat(fabs(matrix[j] - inverted[j]),
                                  <=,
                                  eps * (fabs(matrix[j]) + fabs(inverted[j])));
            }
        }

        g_free(vector);
        g_free(solution);
        g_free(matrix);
        g_free(decomp);
        g_free(inverted);
        g_free(unity);
    }

    g_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
