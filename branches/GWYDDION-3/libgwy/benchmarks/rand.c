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

#define GWY_SQRT6 2.449489742783178098197284074705

#define DECLARE_SUM_TEST(RNG, TYPE, prefix, get) \
    static inline TYPE \
    run##_##prefix##_##TYPE(RNG *rng, guint64 n, TYPE seed) \
    { \
        prefix##_set_seed(rng, seed); \
        TYPE s = 0; \
        gwy_benchmark_timer_start(); \
        while (n--) \
            s += prefix##_##get(rng); \
        gwy_benchmark_timer_stop(); \
        return s; \
    } \
    /* Give the compiler something to accept semicolon after. */ \
    G_GNUC_UNUSED static const gboolean run##_##prefix##_##TYPE##_ = TRUE

#define DECLARE_DIST_TEST(RNG, prefix, get) \
    static inline gdouble \
    run##_##prefix##_##get(RNG *rng, guint64 n, guint64 seed) \
    { \
        prefix##_set_seed(rng, seed); \
        gdouble s = 0; \
        gwy_benchmark_timer_start(); \
        while (n--) \
            s += prefix##_##get(rng); \
        gwy_benchmark_timer_stop(); \
        return s; \
    } \
    /* Give the compiler something to accept semicolon after. */ \
    G_GNUC_UNUSED static const gboolean run##_##prefix##_##get##_ = TRUE

DECLARE_SUM_TEST(GRand, guint32, g_rand, int);
DECLARE_SUM_TEST(GwyRand, guint32, gwy_rand, int);
DECLARE_SUM_TEST(GwyRand, guint64, gwy_rand, int64);
DECLARE_SUM_TEST(GRand, gdouble, g_rand, double);
DECLARE_SUM_TEST(GwyRand, gdouble, gwy_rand, double);
DECLARE_SUM_TEST(GRand, gboolean, g_rand, boolean);
DECLARE_SUM_TEST(GwyRand, gboolean, gwy_rand, boolean);
DECLARE_SUM_TEST(GwyRand, guint8, gwy_rand, byte);

static inline guint64
run_g_rand_guint64(GRand *rng, guint64 n, guint32 seed)
{
    g_rand_set_seed(rng, seed);
    guint64 s = 0;
    gwy_benchmark_timer_start();
    while (n--) {
        guint64 lo = g_rand_int(rng);
        guint64 hi = g_rand_int(rng);
        s += (hi << 32) | lo;
    }
    gwy_benchmark_timer_stop();
    return s;
}

static inline guint8
run_g_rand_guint8(GRand *rng, guint64 n, guint32 seed)
{
    g_rand_set_seed(rng, seed);
    guint8 s = 0;
    gwy_benchmark_timer_start();
    while (n--) {
        s += g_rand_int(rng) & 0xff;
    }
    gwy_benchmark_timer_stop();
    return s;
}

static inline gdouble
g_rand_triangle(GRand *rng)
{
    gdouble x;
    do {
        x = g_rand_double(rng);
    } while (G_UNLIKELY(x == 0.0));
    return (x <= 0.5 ? sqrt(2.0*x) - 1.0 : 1.0 - sqrt(2.0*(1.0 - x)))*GWY_SQRT6;
}

static inline gdouble
g_rand_normal(GRand *rng)
{
    static gboolean have_spare = FALSE;
    static gdouble spare;

    gdouble x, y, w;

    /* Calling with NULL rng just clears the spare random value. */
    if (have_spare || G_UNLIKELY(!rng)) {
        have_spare = FALSE;
        return spare;
    }

    do {
        x = -1.0 + 2.0*g_rand_double(rng);
        y = -1.0 + 2.0*g_rand_double(rng);
        w = x*x + y*y;
    } while (w >= 1.0 || G_UNLIKELY(w == 0.0));

    w = sqrt(-2.0*log(w)/w);
    spare = y*w;
    have_spare = TRUE;

    return x*w;
}

static inline gdouble
g_rand_exp(GRand *rng)
{
    static guint spare_bits = 0;
    static guint32 spare;

    gdouble x;
    gboolean sign;

    /* Calling with NULL rng just clears the spare random value. */
    if (G_UNLIKELY(!rng)) {
        spare_bits = 0;
        return 0.0;
    }

    x = g_rand_double(rng);
    /* This is how we get exact 0.0 at least sometimes */
    if (G_UNLIKELY(x == 0.0))
        return 0.0;

    if (!spare_bits) {
        spare = g_rand_int(rng);
        spare_bits = 32;
    }

    sign = spare & 1;
    spare >>= 1;
    spare_bits--;

    if (sign)
        return -log(x)/G_SQRT2;
    else
        return log(x)/G_SQRT2;
}

DECLARE_DIST_TEST(GRand, g_rand, normal);
DECLARE_DIST_TEST(GwyRand, gwy_rand, normal);
DECLARE_DIST_TEST(GRand, g_rand, exp);
DECLARE_DIST_TEST(GwyRand, gwy_rand, exp);
DECLARE_DIST_TEST(GRand, g_rand, triangle);
DECLARE_DIST_TEST(GwyRand, gwy_rand, triangle);

int
main(int argc, char *argv[])
{
    guint64 niter = G_GUINT64_CONSTANT(100000000), rand_seed = 42;

    GOptionEntry entries[] = {
        { "n-iter",      'n', 0, G_OPTION_ARG_INT64, &niter,     "Number of iterations",          "N",  },
        { "random-seed", 'r', 0, G_OPTION_ARG_INT64, &rand_seed, "Random seed",                   "R",  },
        { NULL,            0,   0, 0,                   NULL,           NULL,                          NULL, },
    };

    GError *error = NULL;
    GOptionContext *context = g_option_context_new("- measure random number generation speed");
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("Arguments parsing failed: %s\n", error->message);
        return 1;
    }
    g_option_context_free(context);
    setvbuf(stdout, (char*)NULL, _IOLBF, 0);

    guint64 rniter = (niter + 9)/10;
    GRand *glib_rng = g_rand_new_with_seed(42);
    GwyRand *gwyd_rng = gwy_rand_new_with_seed(42);

    gboolean sboo;
    guint8 sbyt;
    guint32 su32;
    guint64 su64;
    gdouble sdbl;

    su32 = run_g_rand_guint32(glib_rng, niter, rand_seed);
    printf("GLIB uint32 %g Mnum/s (s = %u)\n",
           niter/gwy_benchmark_timer_get_total()/1e6, su32);

    su32 = run_gwy_rand_guint32(gwyd_rng, niter, rand_seed);
    printf("GWY3 uint32 %g Mnum/s (s = %u)\n",
           niter/gwy_benchmark_timer_get_total()/1e6, su32);

    su64 = run_g_rand_guint64(glib_rng, niter, rand_seed);
    printf("GLIB uint64 %g Mnum/s (s = %" G_GUINT64_FORMAT ")\n",
           niter/gwy_benchmark_timer_get_total()/1e6, su64);

    su64 = run_gwy_rand_guint64(gwyd_rng, niter, rand_seed);
    printf("GWY3 uint64 %g Mnum/s (s = %" G_GUINT64_FORMAT ")\n",
           niter/gwy_benchmark_timer_get_total()/1e6, su64);

    sdbl = run_g_rand_gdouble(glib_rng, niter, rand_seed);
    printf("GLIB double %g Mnum/s (s = %g)\n",
           niter/gwy_benchmark_timer_get_total()/1e6, sdbl/niter);

    sdbl = run_gwy_rand_gdouble(gwyd_rng, niter, rand_seed);
    printf("GWY3 double %g Mnum/s (s = %g)\n",
           niter/gwy_benchmark_timer_get_total()/1e6, sdbl/niter);

    sboo = run_g_rand_gboolean(glib_rng, niter, rand_seed);
    printf("GLIB boolean %g Mnum/s (s = %g)\n",
           niter/gwy_benchmark_timer_get_total()/1e6, sboo/(gdouble)niter);

    sboo = run_gwy_rand_gboolean(gwyd_rng, niter, rand_seed);
    printf("GWY3 boolean %g Mnum/s (s = %g)\n",
           niter/gwy_benchmark_timer_get_total()/1e6, sboo/(gdouble)niter);

    sbyt = run_g_rand_guint8(glib_rng, niter, rand_seed);
    printf("GLIB byte %g Mnum/s (s = %u)\n",
           niter/gwy_benchmark_timer_get_total()/1e6, sbyt);

    sbyt = run_gwy_rand_guint8(gwyd_rng, niter, rand_seed);
    printf("GWY3 byte %g Mnum/s (s = %u)\n",
           niter/gwy_benchmark_timer_get_total()/1e6, sbyt);

    sdbl = run_g_rand_normal(glib_rng, rniter, rand_seed);
    printf("GWY2 normal %g Mnum/s (s = %g)\n",
           rniter/gwy_benchmark_timer_get_total()/1e6, sdbl/rniter);

    sdbl = run_gwy_rand_normal(gwyd_rng, rniter, rand_seed);
    printf("GWY3 normal %g Mnum/s (s = %g)\n",
           rniter/gwy_benchmark_timer_get_total()/1e6, sdbl/rniter);

    sdbl = run_g_rand_exp(glib_rng, rniter, rand_seed);
    printf("GWY2 exp %g Mnum/s (s = %g)\n",
           rniter/gwy_benchmark_timer_get_total()/1e6, sdbl/rniter);

    sdbl = run_gwy_rand_exp(gwyd_rng, rniter, rand_seed);
    printf("GWY3 exp %g Mnum/s (s = %g)\n",
           rniter/gwy_benchmark_timer_get_total()/1e6, sdbl/rniter);

    sdbl = run_g_rand_triangle(glib_rng, rniter, rand_seed);
    printf("GWY2 triangle %g Mnum/s (s = %g)\n",
           rniter/gwy_benchmark_timer_get_total()/1e6, sdbl/rniter);

    sdbl = run_gwy_rand_triangle(gwyd_rng, rniter, rand_seed);
    printf("GWY3 triangle %g Mnum/s (s = %g)\n",
           rniter/gwy_benchmark_timer_get_total()/1e6, sdbl/rniter);

    g_rand_free(glib_rng);
    gwy_rand_free(gwyd_rng);

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
