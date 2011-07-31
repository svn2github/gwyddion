/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
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

#include <stdlib.h>
#include "testlibgwy.h"

/***************************************************************************
 *
 * Random number generation
 *
 ***************************************************************************/

// NB: For any meaningful testing niter should be at least twice the state
// size.

#define P_500_25 .893250457965680
enum { bit_samples_taken = 1000 };

void
test_rand_seed_reproducibility(void)
{
    enum { niter = 1000, nseed = 1000 };

    GwyRand *rng0 = gwy_rand_new_with_seed(0);

    for (guint seed = 0; seed < nseed; seed++) {
        GwyRand *rng1 = gwy_rand_new_with_seed(seed);
        gwy_rand_set_seed(rng0, seed);

        for (guint i = 0; i < niter; i++) {
            guint64 x = gwy_rand_int(rng0);
            guint64 y = gwy_rand_int(rng1);
            g_assert_cmpuint(x, ==, y);

            x = gwy_rand_byte(rng0);
            y = gwy_rand_byte(rng1);
            g_assert_cmpuint(x, ==, y);

            x = gwy_rand_int64(rng0);
            y = gwy_rand_int64(rng1);
            g_assert_cmpuint(x, ==, y);

            x = gwy_rand_boolean(rng0);
            y = gwy_rand_boolean(rng1);
            g_assert_cmpuint(x, ==, y);

            gdouble xd = gwy_rand_double(rng0);
            gdouble yd = gwy_rand_double(rng1);
            g_assert_cmpfloat(xd, ==, yd);
        }
    }
}

static int
compare_uints(const void *va, const void *vb)
{
    const guint32 *pa = (const guint32*)va, *pb = (const guint32*)vb;
    guint32 a = *pa, b = *pb;

    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

void
test_rand_difference_seed(void)
{
    enum { niter = 16, nseed = 100000 };

    GwyRand *rng = gwy_rand_new_with_seed(0);

    guint32 *values = g_new(guint32, niter*nseed);
    for (guint seed = 0; seed < nseed; seed++) {
        gwy_rand_set_seed(rng, seed);
        for (guint i = 0; i < niter; i++)
            values[seed*niter + i] = gwy_rand_int(rng);
    }

    qsort(values, nseed, niter*sizeof(guint32), compare_uints);

    for (guint seed = 0; seed+1 < nseed; seed++) {
        guint32 *thisval = values + seed*niter,
                *nextval = thisval + niter;
        gboolean equal = TRUE;
        for (guint i = niter; i; i--) {
            if (*(thisval++) != *(nextval++)) {
                equal = FALSE;
                break;
            }
        }
        g_assert(!equal);
    }
}

void
test_rand_difference_auto(void)
{
    enum { niter = 16, nseed = 100000 };

    guint32 *values = g_new(guint32, niter*nseed);
    for (guint seed = 0; seed < nseed; seed++) {
        GwyRand *rng = gwy_rand_new();
        for (guint i = 0; i < niter; i++)
            values[seed*niter + i] = gwy_rand_int(rng);
        gwy_rand_free(rng);
    }

    qsort(values, nseed, niter*sizeof(guint32), compare_uints);

    for (guint seed = 0; seed+1 < nseed; seed++) {
        guint32 *thisval = values + seed*niter,
                *nextval = thisval + niter;
        gboolean equal = TRUE;
        for (guint i = niter; i; i--) {
            if (*(thisval++) != *(nextval++)) {
                equal = FALSE;
                break;
            }
        }
        g_assert(!equal);
    }
}

void
test_rand_copy(void)
{
    enum { niter = 10000 };

    GwyRand *rng = gwy_rand_new_with_seed(g_test_rand_int());
    GwyRand *rng0 = gwy_rand_copy(rng);

    for (guint i = 0; i < niter; i++) {
        guint x = gwy_rand_int(rng);
        guint y = gwy_rand_int(rng0);
        g_assert_cmpuint(x, ==, y);
    }

    GwyRand *rng1 = gwy_rand_copy(rng);

    for (guint i = 0; i < niter; i++) {
        guint x = gwy_rand_int(rng);
        guint y = gwy_rand_int(rng1);
        g_assert_cmpuint(x, ==, y);
    }

    gwy_rand_assign(rng0, rng);
    for (guint i = 0; i < niter; i++) {
        guint x = gwy_rand_int(rng);
        guint y = gwy_rand_int(rng0);
        g_assert_cmpuint(x, ==, y);
    }

    gwy_rand_free(rng1);
    gwy_rand_free(rng0);
    gwy_rand_free(rng);
}

void
test_rand_range_boolean(void)
{
    enum { niter = 10000 };
    GwyRand *rng = gwy_rand_new_with_seed(g_test_rand_int());

    for (guint i = 0; i < niter; i++) {
        guint x = gwy_rand_boolean(rng);
        g_assert_cmpuint(x, <=, 1);
    }

    gwy_rand_free(rng);
}

void
test_rand_range_byte(void)
{
    enum { niter = 100000 };
    GwyRand *rng = gwy_rand_new_with_seed(g_test_rand_int());

    for (guint i = 0; i < niter; i++) {
        guint x = gwy_rand_byte(rng);
        g_assert_cmpuint(x, <, 0x100);
    }

    gwy_rand_free(rng);
}

void
test_rand_range_int(void)
{
    enum { niter = 10000, nrange = 100 };
    GwyRand *rng = gwy_rand_new_with_seed(0);

    for (guint r = 0; r < nrange; r++) {
        gwy_rand_set_seed(rng, g_test_rand_int());
        gint64 lo = (gint64)(((guint64)g_test_rand_int() << 32)
                             | g_test_rand_int());
        gint64 hi = (gint64)(((guint64)g_test_rand_int() << 32)
                             | g_test_rand_int());
        if (hi == lo)
            continue;
        if (hi < lo)
            GWY_SWAP(gint64, hi, lo);

        for (guint i = 0; i < niter; i++) {
            gint64 x = gwy_rand_int_range(rng, lo, hi);
            g_assert_cmpint(x, >=, lo);
            g_assert_cmpint(x, <, hi);
        }
    }

    gwy_rand_free(rng);
}

void
test_rand_range_double(void)
{
    enum { niter = 100000 };
    GwyRand *rng = gwy_rand_new_with_seed(g_test_rand_int());

    for (guint i = 0; i < niter; i++) {
        gdouble x = gwy_rand_double(rng);
        g_assert_cmpfloat(x, >, 0.0);
        g_assert_cmpfloat(x, <, 1.0);
    }

    gwy_rand_free(rng);
}

static void
count_bits_in_int(guint64 x, guint *counts, guint nc)
{
    for (guint i = 0; i < nc; i++) {
        if (x & 1)
            counts[i]++;
        x >>= 1;
    }
}

static void
count_excesses(guint *counts, guint *excesses, guint nc)
{
    for (guint i = 0; i < nc; i++) {
        if (abs(counts[i] - bit_samples_taken/2) > 25)
            excesses[i]++;
    }
}

static void
check_excesses(const guint *excesses, guint nc)
{
    for (guint i = 0; i < nc; i++) {
        g_assert_cmpfloat(fabs(excesses[i]/10000.0/(1.0 - P_500_25) - 1.0),
                          <=, 0.1);
    }
}

void
test_rand_uniformity_boolean(void)
{
    enum { niter = 10000, nbits = 1 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    guint counts[nbits], excesses[nbits];

    gwy_clear(excesses, nbits);
    for (guint i = 0; i < niter; i++) {
        gwy_rand_set_seed(rng, g_test_rand_int());
        gwy_clear(counts, nbits);
        for (guint j = 0; j < bit_samples_taken; j++) {
            guint64 x = gwy_rand_boolean(rng);
            count_bits_in_int(x, counts, nbits);
        }
        count_excesses(counts, excesses, nbits);
    }
    check_excesses(excesses, nbits);
}

void
test_rand_uniformity_byte(void)
{
    enum { niter = 10000, nbits = 8 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    guint counts[nbits], excesses[nbits];

    gwy_clear(excesses, nbits);
    for (guint i = 0; i < niter; i++) {
        gwy_rand_set_seed(rng, g_test_rand_int());
        gwy_clear(counts, nbits);
        for (guint j = 0; j < bit_samples_taken; j++) {
            guint64 x = gwy_rand_byte(rng);
            count_bits_in_int(x, counts, nbits);
        }
        count_excesses(counts, excesses, nbits);
    }
    check_excesses(excesses, nbits);
}

void
test_rand_uniformity_int64(void)
{
    enum { niter = 10000, nbits = 64 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    guint counts[nbits], excesses[nbits];

    gwy_clear(excesses, nbits);
    for (guint i = 0; i < niter; i++) {
        gwy_rand_set_seed(rng, g_test_rand_int());
        gwy_clear(counts, nbits);
        for (guint j = 0; j < bit_samples_taken; j++) {
            guint64 x = gwy_rand_int64(rng);
            count_bits_in_int(x, counts, nbits);
        }
        count_excesses(counts, excesses, nbits);
    }
    check_excesses(excesses, nbits);
}

static gboolean
kolmogorov_test(GwyRand *rng,
                guint64 seed,
                gdouble (*generate)(GwyRand*),
                gdouble (*cdf)(gdouble),
                gdouble *workspace,
                guint nsamples)
{
    gwy_rand_set_seed(rng, seed);

    for (guint i = 0; i < nsamples; i++)
        workspace[i] = generate(rng);

    gwy_math_sort(workspace, NULL, nsamples);

    gdouble D = 0;
    for (guint i = 0; i < nsamples; i++) {
        gdouble F = cdf(workspace[i]);
        gdouble ym = i/(gdouble)nsamples, yp = (i + 1)/(gdouble)nsamples;
        gdouble dm = fabs(F - ym), dp = fabs(F - yp);

        if (dm > D)
            D = dm;
        if (dp > D)
            D = dp;
    }

    // This is really, really improbable.
    g_assert_cmpfloat(D*sqrt(nsamples), <=, 4.0);
    // Failure that can occur occasionally; make noise only if we get several
    // of them.
    return D*sqrt(nsamples) < 2.0;
}

static gdouble
cdf_uniform(gdouble x)
{
    return x;
}

void
test_rand_distribution_uniform(void)
{
    enum { nsamples = 1000, niter = 100 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    gdouble *data = g_new(gdouble, nsamples);
    guint nfailures = 0;

    for (guint i = 0; i < niter; i++) {
        if (!kolmogorov_test(rng, g_test_rand_int(),
                             gwy_rand_double, cdf_uniform,
                             data, nsamples))
            nfailures++;
    }
    g_assert_cmpuint(nfailures, <=, 2);

    g_free(data);
    gwy_rand_free(rng);
}

static gdouble
cdf_exp_positive(gdouble x)
{
    return 1.0 - exp(-x);
}

void
test_rand_distribution_exp_positive(void)
{
    enum { nsamples = 1000, niter = 100 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    gdouble *data = g_new(gdouble, nsamples);
    guint nfailures = 0;

    for (guint i = 0; i < niter; i++) {
        if (!kolmogorov_test(rng, g_test_rand_int(),
                             gwy_rand_exp_positive, cdf_exp_positive,
                             data, nsamples))
            nfailures++;
    }
    g_assert_cmpuint(nfailures, <=, 2);

    g_free(data);
    gwy_rand_free(rng);
}

static gdouble
cdf_exp(gdouble x)
{
    return x <= 0.0 ? 0.5*exp(x) : 1.0 - 0.5*exp(-x);
}

void
test_rand_distribution_exp(void)
{
    enum { nsamples = 1000, niter = 100 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    gdouble *data = g_new(gdouble, nsamples);
    guint nfailures = 0;

    for (guint i = 0; i < niter; i++) {
        if (!kolmogorov_test(rng, g_test_rand_int(),
                             gwy_rand_exp, cdf_exp,
                             data, nsamples))
            nfailures++;
    }
    g_assert_cmpuint(nfailures, <=, 2);

    g_free(data);
    gwy_rand_free(rng);
}

static gdouble
cdf_normal_positive(gdouble x)
{
    return erf(x/G_SQRT2);
}

void
test_rand_distribution_normal_positive(void)
{
    enum { nsamples = 1000, niter = 100 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    gdouble *data = g_new(gdouble, nsamples);
    guint nfailures = 0;

    for (guint i = 0; i < niter; i++) {
        if (!kolmogorov_test(rng, g_test_rand_int(),
                             gwy_rand_normal_positive, cdf_normal_positive,
                             data, nsamples))
            nfailures++;
    }
    g_assert_cmpuint(nfailures, <=, 2);

    g_free(data);
    gwy_rand_free(rng);
}

static gdouble
cdf_normal(gdouble x)
{
    return 0.5*(1.0 + erf(x/G_SQRT2));
}

void
test_rand_distribution_normal(void)
{
    enum { nsamples = 1000, niter = 100 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    gdouble *data = g_new(gdouble, nsamples);
    guint nfailures = 0;

    for (guint i = 0; i < niter; i++) {
        if (!kolmogorov_test(rng, g_test_rand_int(),
                             gwy_rand_normal, cdf_normal,
                             data, nsamples))
            nfailures++;
    }
    g_assert_cmpuint(nfailures, <=, 2);

    g_free(data);
    gwy_rand_free(rng);
}

static gdouble
cdf_triangle_positive(gdouble x)
{
    return x*(2.0 - x);
}

void
test_rand_distribution_triangle_positive(void)
{
    enum { nsamples = 1000, niter = 100 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    gdouble *data = g_new(gdouble, nsamples);
    guint nfailures = 0;

    for (guint i = 0; i < niter; i++) {
        if (!kolmogorov_test(rng, g_test_rand_int(),
                             gwy_rand_triangle_positive, cdf_triangle_positive,
                             data, nsamples))
            nfailures++;
    }
    g_assert_cmpuint(nfailures, <=, 2);

    g_free(data);
    gwy_rand_free(rng);
}

static gdouble
cdf_triangle(gdouble x)
{
    return 0.5*x*(2.0 - fabs(x)) + 0.5;
}

void
test_rand_distribution_triangle(void)
{
    enum { nsamples = 1000, niter = 100 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    gdouble *data = g_new(gdouble, nsamples);
    guint nfailures = 0;

    for (guint i = 0; i < niter; i++) {
        if (!kolmogorov_test(rng, g_test_rand_int(),
                             gwy_rand_triangle, cdf_triangle,
                             data, nsamples))
            nfailures++;
    }
    g_assert_cmpuint(nfailures, <=, 2);

    g_free(data);
    gwy_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
