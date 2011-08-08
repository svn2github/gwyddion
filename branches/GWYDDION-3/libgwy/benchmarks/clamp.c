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

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "timer.h"
#include "libgwy/macros.h"
#include "libgwy/rand.h"

#define benchmark_gwy_clamp(x, low, hi) \
    (G_UNLIKELY((x) > (hi)) ? (hi) : (G_UNLIKELY((x) < (low)) ? (low) : (x)))

/*
 * The fraction is the fraction of values that are left unchanged (the rest
 * is clamped).  Similarly none, half and all refer to this fraction, except
 * they are used for hardcoded ranges (a big difference, apparently).
 */

static void
int_min_max(gdouble fraction, guint *min, guint *max)
{
    *min = 0.5*(1.0 - fraction)*G_MAXUINT32;
    *max = G_MAXUINT32 - *min;

    for (gboolean odd = FALSE; *max < *min; odd = !odd) {
        if (odd)
            (*max)++;
        else
            (*min)--;
    }
}

static void
double_min_max(gdouble fraction, gdouble *min, gdouble *max)
{
    *min = 0.5*(1.0 - fraction);
    *max = 1.0 - *min;
}

static guint*
int_random_data(guint n, GwyRand *rng)
{
    guint *data = g_new(guint, n);

    guint *d = data;
    for (guint i = n; i; i--, d++)
        *d = gwy_rand_int(rng);

    return data;
}

static gdouble*
double_random_data(guint n, GwyRand *rng)
{
    gdouble *data = g_new(gdouble, n);

    gdouble *d = data;
    for (guint i = n; i; i--, d++)
        *d = gwy_rand_double(rng);

    return data;
}

static guint
glib_int_clamp(guint n, guint niter, guint64 seed, gdouble fraction)
{
    GwyRand *rng = gwy_rand_new_with_seed(seed);

    guint min, max;
    int_min_max(fraction, &min, &max);

    guint *data = int_random_data(n, rng);
    gwy_benchmark_timer_start();
    guint sum = 0;
    for (guint iter = 0; iter < niter; iter++) {
        const guint *d = data;
        for (guint i = n; i; i--, d++) {
            guint x = *d;
            sum += CLAMP(x, min, max);
        }
    }
    gwy_benchmark_timer_stop();
    g_free(data);

    gwy_rand_free(rng);

    return sum;
}

static guint
glib_int_clamp_none(guint n, guint niter, guint64 seed)
{
    GwyRand *rng = gwy_rand_new_with_seed(seed);

    guint *data = int_random_data(n, rng);
    gwy_benchmark_timer_start();
    guint sum = 0;
    for (guint iter = 0; iter < niter; iter++) {
        const guint *d = data;
        for (guint i = n; i; i--, d++) {
            guint x = *d;
            sum += CLAMP(x, G_MAXUINT/2, G_MAXUINT/2+1);
        }
    }
    gwy_benchmark_timer_stop();
    g_free(data);

    gwy_rand_free(rng);

    return sum;
}

static guint
glib_int_clamp_half(guint n, guint niter, guint64 seed)
{
    GwyRand *rng = gwy_rand_new_with_seed(seed);

    guint *data = int_random_data(n, rng);
    gwy_benchmark_timer_start();
    guint sum = 0;
    for (guint iter = 0; iter < niter; iter++) {
        const guint *d = data;
        for (guint i = n; i; i--, d++) {
            guint x = *d;
            sum += CLAMP(x, G_MAXUINT/4, G_MAXUINT/4*3);
        }
    }
    gwy_benchmark_timer_stop();
    g_free(data);

    gwy_rand_free(rng);

    return sum;
}

static guint
glib_int_clamp_all(guint n, guint niter, guint64 seed)
{
    GwyRand *rng = gwy_rand_new_with_seed(seed);

    guint *data = int_random_data(n, rng);
    gwy_benchmark_timer_start();
    guint sum = 0;
    for (guint iter = 0; iter < niter; iter++) {
        const guint *d = data;
        for (guint i = n; i; i--, d++) {
            guint x = *d;
            sum += CLAMP(x, 1, G_MAXUINT-1);
        }
    }
    gwy_benchmark_timer_stop();
    g_free(data);

    gwy_rand_free(rng);

    return sum;
}

static guint
gwyd_int_clamp(guint n, guint niter, guint64 seed, gdouble fraction)
{
    GwyRand *rng = gwy_rand_new_with_seed(seed);

    guint min, max;
    int_min_max(fraction, &min, &max);

    guint *data = int_random_data(n, rng);
    gwy_benchmark_timer_start();
    guint sum = 0;
    for (guint iter = 0; iter < niter; iter++) {
        const guint *d = data;
        for (guint i = n; i; i--, d++) {
            guint x = *d;
            sum += benchmark_gwy_clamp(x, min, max);
        }
    }
    gwy_benchmark_timer_stop();
    g_free(data);

    gwy_rand_free(rng);

    return sum;
}

static guint
gwyd_int_clamp_none(guint n, guint niter, guint64 seed)
{
    GwyRand *rng = gwy_rand_new_with_seed(seed);

    guint *data = int_random_data(n, rng);
    gwy_benchmark_timer_start();
    guint sum = 0;
    for (guint iter = 0; iter < niter; iter++) {
        const guint *d = data;
        for (guint i = n; i; i--, d++) {
            guint x = *d;
            sum += benchmark_gwy_clamp(x, G_MAXUINT/2, G_MAXUINT/2+1);
        }
    }
    gwy_benchmark_timer_stop();
    g_free(data);

    gwy_rand_free(rng);

    return sum;
}

static guint
gwyd_int_clamp_half(guint n, guint niter, guint64 seed)
{
    GwyRand *rng = gwy_rand_new_with_seed(seed);

    guint *data = int_random_data(n, rng);
    gwy_benchmark_timer_start();
    guint sum = 0;
    for (guint iter = 0; iter < niter; iter++) {
        const guint *d = data;
        for (guint i = n; i; i--, d++) {
            guint x = *d;
            sum += benchmark_gwy_clamp(x, G_MAXUINT/4, G_MAXUINT/4*3);
        }
    }
    gwy_benchmark_timer_stop();
    g_free(data);

    gwy_rand_free(rng);

    return sum;
}

static guint
gwyd_int_clamp_all(guint n, guint niter, guint64 seed)
{
    GwyRand *rng = gwy_rand_new_with_seed(seed);

    guint *data = int_random_data(n, rng);
    gwy_benchmark_timer_start();
    guint sum = 0;
    for (guint iter = 0; iter < niter; iter++) {
        const guint *d = data;
        for (guint i = n; i; i--, d++) {
            guint x = *d;
            sum += benchmark_gwy_clamp(x, 1, G_MAXUINT-1);
        }
    }
    gwy_benchmark_timer_stop();
    g_free(data);

    gwy_rand_free(rng);

    return sum;
}

static gdouble
glib_double_clamp(guint n, guint niter, guint64 seed, gdouble fraction)
{
    GwyRand *rng = gwy_rand_new_with_seed(seed);

    gdouble min, max;
    double_min_max(fraction, &min, &max);

    gdouble *data = double_random_data(n, rng);
    gwy_benchmark_timer_start();
    gdouble sum = 0;
    for (guint iter = 0; iter < niter; iter++) {
        const gdouble *d = data;
        for (guint i = n; i; i--, d++) {
            gdouble x = *d;
            sum += CLAMP(x, min, max);
        }
    }
    gwy_benchmark_timer_stop();
    g_free(data);

    gwy_rand_free(rng);

    return sum;
}

static gdouble
glib_double_clamp_none(guint n, guint niter, guint64 seed)
{
    GwyRand *rng = gwy_rand_new_with_seed(seed);

    gdouble *data = double_random_data(n, rng);
    gwy_benchmark_timer_start();
    gdouble sum = 0;
    for (guint iter = 0; iter < niter; iter++) {
        const gdouble *d = data;
        for (guint i = n; i; i--, d++) {
            gdouble x = *d;
            sum += CLAMP(x, 0.499999999999999, 0.500000000000001);
        }
    }
    gwy_benchmark_timer_stop();
    g_free(data);

    gwy_rand_free(rng);

    return sum;
}

static gdouble
glib_double_clamp_half(guint n, guint niter, guint64 seed)
{
    GwyRand *rng = gwy_rand_new_with_seed(seed);

    gdouble *data = double_random_data(n, rng);
    gwy_benchmark_timer_start();
    gdouble sum = 0;
    for (guint iter = 0; iter < niter; iter++) {
        const gdouble *d = data;
        for (guint i = n; i; i--, d++) {
            gdouble x = *d;
            sum += CLAMP(x, 0.25, 0.75);
        }
    }
    gwy_benchmark_timer_stop();
    g_free(data);

    gwy_rand_free(rng);

    return sum;
}

static gdouble
glib_double_clamp_all(guint n, guint niter, guint64 seed)
{
    GwyRand *rng = gwy_rand_new_with_seed(seed);

    gdouble *data = double_random_data(n, rng);
    gwy_benchmark_timer_start();
    gdouble sum = 0;
    for (guint iter = 0; iter < niter; iter++) {
        const gdouble *d = data;
        for (guint i = n; i; i--, d++) {
            gdouble x = *d;
            sum += CLAMP(x, 0.000000000000001, 0.999999999999999);
        }
    }
    gwy_benchmark_timer_stop();
    g_free(data);

    gwy_rand_free(rng);

    return sum;
}

static gdouble
gwyd_double_clamp(guint n, guint niter, guint64 seed, gdouble fraction)
{
    GwyRand *rng = gwy_rand_new_with_seed(seed);

    gdouble min, max;
    double_min_max(fraction, &min, &max);

    gdouble *data = double_random_data(n, rng);
    gwy_benchmark_timer_start();
    gdouble sum = 0;
    for (guint iter = 0; iter < niter; iter++) {
        const gdouble *d = data;
        for (guint i = n; i; i--, d++) {
            gdouble x = *d;
            sum += benchmark_gwy_clamp(x, min, max);
        }
    }
    gwy_benchmark_timer_stop();
    g_free(data);

    gwy_rand_free(rng);

    return sum;
}

static gdouble
gwyd_double_clamp_none(guint n, guint niter, guint64 seed)
{
    GwyRand *rng = gwy_rand_new_with_seed(seed);

    gdouble *data = double_random_data(n, rng);
    gwy_benchmark_timer_start();
    gdouble sum = 0;
    for (guint iter = 0; iter < niter; iter++) {
        const gdouble *d = data;
        for (guint i = n; i; i--, d++) {
            gdouble x = *d;
            sum += benchmark_gwy_clamp(x, 0.499999999999999, 0.500000000000001);
        }
    }
    gwy_benchmark_timer_stop();
    g_free(data);

    gwy_rand_free(rng);

    return sum;
}

static gdouble
gwyd_double_clamp_half(guint n, guint niter, guint64 seed)
{
    GwyRand *rng = gwy_rand_new_with_seed(seed);

    gdouble *data = double_random_data(n, rng);
    gwy_benchmark_timer_start();
    gdouble sum = 0;
    for (guint iter = 0; iter < niter; iter++) {
        const gdouble *d = data;
        for (guint i = n; i; i--, d++) {
            gdouble x = *d;
            sum += benchmark_gwy_clamp(x, 0.25, 0.75);
        }
    }
    gwy_benchmark_timer_stop();
    g_free(data);

    gwy_rand_free(rng);

    return sum;
}

static gdouble
gwyd_double_clamp_all(guint n, guint niter, guint64 seed)
{
    GwyRand *rng = gwy_rand_new_with_seed(seed);

    gdouble *data = double_random_data(n, rng);
    gwy_benchmark_timer_start();
    gdouble sum = 0;
    for (guint iter = 0; iter < niter; iter++) {
        const gdouble *d = data;
        for (guint i = n; i; i--, d++) {
            gdouble x = *d;
            sum += benchmark_gwy_clamp(x, 0.000000000000001, 0.999999999999999);
        }
    }
    gwy_benchmark_timer_stop();
    g_free(data);

    gwy_rand_free(rng);

    return sum;
}

int
main(int argc, char *argv[])
{
    guint64 rand_seed = 42;
    guint size = 100000, niter = 2000;

    GOptionEntry entries[] = {
        { "size",        's', 0, G_OPTION_ARG_INT,   &size,      "Array size",           "N",  },
        { "n-iter",      'n', 0, G_OPTION_ARG_INT,   &niter,     "Number of iterations", "N",  },
        { "random-seed", 'r', 0, G_OPTION_ARG_INT64, &rand_seed, "Random seed",          "R",  },
        { NULL,           0,  0, 0,                  NULL,       NULL,                   NULL, },
    };

    GError *error = NULL;
    GOptionContext *context = g_option_context_new("- measure clam operation speed");
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("Arguments parsing failed: %s\n", error->message);
        return 1;
    }
    g_option_context_free(context);
    setvbuf(stdout, (char*)NULL, _IOLBF, 0);

    guint sint;
    gdouble sdbl;

    for (guint f = 0; f <= 5; f++) {
        gdouble fraction = f/5.0;

        sint = glib_int_clamp(size, niter, rand_seed, fraction);
        printf("GLIB int %.2f %g G/s (s = %u)\n",
               fraction, size*niter/gwy_benchmark_timer_get_total()/1e9, sint);

        sint = gwyd_int_clamp(size, niter, rand_seed, fraction);
        printf("GWYD int %.2f %g G/s (s = %u)\n",
               fraction, size*niter/gwy_benchmark_timer_get_total()/1e9, sint);
    }

    printf("\n");

    for (guint f = 0; f <= 5; f++) {
        gdouble fraction = f/5.0;

        sdbl = glib_double_clamp(size, niter, rand_seed, fraction);
        printf("GLIB double %.2f %g G/s (s = %g)\n",
               fraction, size*niter/gwy_benchmark_timer_get_total()/1e9, sdbl);

        sdbl = gwyd_double_clamp(size, niter, rand_seed, fraction);
        printf("GWYD double %.2f %g G/s (s = %g)\n",
               fraction, size*niter/gwy_benchmark_timer_get_total()/1e9, sdbl);
    }

    printf("\n");

    sint = glib_int_clamp_all(size, niter, rand_seed);
    printf("GLIB int ALL %g G/s (s = %u)\n",
           size*niter/gwy_benchmark_timer_get_total()/1e9, sint);

    sint = gwyd_int_clamp_all(size, niter, rand_seed);
    printf("GWYD int ALL %g G/s (s = %u)\n",
           size*niter/gwy_benchmark_timer_get_total()/1e9, sint);

    sint = glib_int_clamp_half(size, niter, rand_seed);
    printf("GLIB int HALF %g G/s (s = %u)\n",
           size*niter/gwy_benchmark_timer_get_total()/1e9, sint);

    sint = gwyd_int_clamp_half(size, niter, rand_seed);
    printf("GWYD int HALF %g G/s (s = %u)\n",
           size*niter/gwy_benchmark_timer_get_total()/1e9, sint);

    sint = glib_int_clamp_none(size, niter, rand_seed);
    printf("GLIB int NONE %g G/s (s = %u)\n",
           size*niter/gwy_benchmark_timer_get_total()/1e9, sint);

    sint = gwyd_int_clamp_none(size, niter, rand_seed);
    printf("GWYD int NONE %g G/s (s = %u)\n",
           size*niter/gwy_benchmark_timer_get_total()/1e9, sint);

    printf("\n");

    sdbl = glib_double_clamp_all(size, niter, rand_seed);
    printf("GLIB double ALL %g G/s (s = %g)\n",
           size*niter/gwy_benchmark_timer_get_total()/1e9, sdbl);

    sdbl = gwyd_double_clamp_all(size, niter, rand_seed);
    printf("GWYD double ALL %g G/s (s = %g)\n",
           size*niter/gwy_benchmark_timer_get_total()/1e9, sdbl);

    sdbl = glib_double_clamp_half(size, niter, rand_seed);
    printf("GLIB double HALF %g G/s (s = %g)\n",
           size*niter/gwy_benchmark_timer_get_total()/1e9, sdbl);

    sdbl = gwyd_double_clamp_half(size, niter, rand_seed);
    printf("GWYD double HALF %g G/s (s = %g)\n",
           size*niter/gwy_benchmark_timer_get_total()/1e9, sdbl);

    sdbl = glib_double_clamp_none(size, niter, rand_seed);
    printf("GLIB double NONE %g G/s (s = %g)\n",
           size*niter/gwy_benchmark_timer_get_total()/1e9, sdbl);

    sdbl = gwyd_double_clamp_none(size, niter, rand_seed);
    printf("GWYD double NONE %g G/s (s = %g)\n",
           size*niter/gwy_benchmark_timer_get_total()/1e9, sdbl);

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
