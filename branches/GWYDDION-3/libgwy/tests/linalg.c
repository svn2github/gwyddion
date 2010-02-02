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
 * Linear algebra
 *
 ***************************************************************************/

static void
test_linalg_make_vector(gdouble *d,
                        guint n,
                        GRand *rng)
{
    for (guint j = 0; j < n; j++)
        d[j] = g_rand_double_range(rng, -1.0, 1.0)
               * exp(g_rand_double_range(rng, -5.0, 5.0));
}

/* Multiply a vector with a matrix from left. */
static void
test_linalg_matvec(gdouble *a, const gdouble *m, const gdouble *v, guint n)
{
    for (guint i = 0; i < n; i++) {
        a[i] = 0.0;
        for (guint j = 0; j < n; j++)
            a[i] += m[i*n + j] * v[j];
    }
}

/* Multiply two square matrices. */
static void
test_linalg_matmul(gdouble *a, const gdouble *d1, const gdouble *d2, guint n)
{
    for (guint i = 0; i < n; i++) {
        for (guint j = 0; j < n; j++) {
            a[i*n + j] = 0.0;
            for (guint k = 0; k < n; k++)
                a[i*n + j] += d1[i*n + k] * d2[k*n + j];
        }
    }
}

/* Note the precision checks are very tolerant as the matrices we generate
 * are not always well-conditioned. */
void
test_math_linalg(void)
{
    guint nmax = 8, niter = 50;
    GRand *rng = g_rand_new();

    /* Make it realy reproducible. */
    g_rand_set_seed(rng, 42);

    for (guint n = 1; n < nmax; n++) {
        /* Use descriptive names for less cryptic g_assert() messages. */
        gdouble *matrix = g_new(gdouble, n*n);
        gdouble *unity = g_new(gdouble, n*n);
        gdouble *decomp = g_new(gdouble, n*n);
        gdouble *vector = g_new(gdouble, n);
        gdouble *rhs = g_new(gdouble, n);
        gdouble *solution = g_new(gdouble, n);

        for (guint iter = 0; iter < niter; iter++) {
            test_linalg_make_vector(matrix, n*n, rng);
            test_linalg_make_vector(vector, n, rng);
            gdouble eps;

            /* Solution */
            test_linalg_matvec(rhs, matrix, vector, n);
            memcpy(decomp, matrix, n*n*sizeof(gdouble));
            g_assert(gwy_linalg_solve(decomp, rhs, solution, n));
            eps = exp10(n - 13.0);
            for (guint j = 0; j < n; j++) {
                g_assert_cmpfloat(fabs(solution[j] - vector[j]),
                                  <=,
                                  eps * (fabs(solution[j]) + fabs(vector[j])));
            }
            /* Inversion */
            memcpy(unity, matrix, n*n*sizeof(gdouble));
            g_assert(gwy_linalg_invert(unity, decomp, n));
            /* Multiplication with inverted must give unity */
            test_linalg_matmul(unity, matrix, decomp, n);
            for (guint j = 0; j < n; j++) {
                g_assert_cmpfloat(fabs(unity[j*n + j] - 1.0), <=, eps);
                for (guint i = 0; i < n; i++) {
                    if (i == j)
                        continue;
                    g_assert_cmpfloat(fabs(unity[j*n + i]), <=, eps);
                }
            }
            /* Double inversion must give the original */
            g_assert(gwy_linalg_invert(decomp, unity, n));
            for (guint j = 0; j < n; j++) {
                for (guint i = 0; i < n; i++) {
                    g_assert_cmpfloat(fabs(matrix[j] - unity[j]),
                                      <=,
                                      eps * (fabs(matrix[j]) + fabs(unity[j])));
                }
            }
        }

        g_free(vector);
        g_free(rhs);
        g_free(solution);
        g_free(unity);
        g_free(matrix);
        g_free(decomp);
    }

    g_rand_free(rng);
}

typedef struct {
    gdouble K;
    guint n;
    guint degree;
} PolyData1;

// Fits (virtual) symmetric Kx³ data
static gboolean
poly1(guint i,
      gdouble *fvalues,
      gdouble *value,
      gpointer user_data)
{
    const PolyData1 *polydata = (const PolyData1*)user_data;
    gdouble x = (i/(gdouble)polydata->n - 1.0)*polydata->K;
    gdouble p = 1.0;
    for (guint j = 0; j <= polydata->degree; j++) {
        fvalues[j] = p;
        p *= x;
    }
    *value = powi(x, 3);

    return TRUE;
}

void
test_math_fit_poly(void)
{
    for (guint degree = 0; degree <= 4; degree++) {
        for (guint ndata = (degree + 2) | 1; ndata < 30; ndata += 2) {
            PolyData1 polydata = { 1.0, ndata/2, degree };
            gdouble coeffs[degree+1], residuum;
            gboolean ok = gwy_linear_fit(poly1, ndata, coeffs, degree+1,
                                         &residuum, &polydata);
            g_assert(ok);

            gdouble eps = 1e-15;
            for (guint i = 0; i <= degree; i += 2)
                g_assert_cmpfloat(fabs(coeffs[i]), <, eps);

            if (degree >= 3) {
                g_assert_cmpfloat(fabs(coeffs[1]), <, eps);
                g_assert_cmpfloat(fabs(coeffs[3] - polydata.K), <, eps);
            }
            else if (degree == 1 || degree == 2) {
                gdouble t = 1.0/(ndata/2);
                t = 3.0/5.0*(1.0 + t*(1.0 - t/3.0));
                g_assert_cmpfloat(fabs(coeffs[1] - t*polydata.K), <, eps);
            }
        }
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
