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

DECLARE_SUM_TEST(GRand, guint32, g_rand, int);
DECLARE_SUM_TEST(GwyRand, guint32, gwy_rand, uint32);
DECLARE_SUM_TEST(GwyRand, guint64, gwy_rand, uint64);
DECLARE_SUM_TEST(GRand, double, g_rand, double);
DECLARE_SUM_TEST(GwyRand, double, gwy_rand, double);

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

    GRand *glib_rng = g_rand_new_with_seed(42);
    GwyRand *gwyd_rng = gwy_rand_new_with_seed(42);

    guint32 su32;
    guint64 su64;
    gdouble sdbl;

    su32 = run_g_rand_guint32(glib_rng, niter, rand_seed);
    printf("GLIB uint32 %g Mnum/s (s = %u)\n",
           niter/gwy_benchmark_timer_get_total()/1e6, su32);

    su32 = run_gwy_rand_guint32(gwyd_rng, niter, rand_seed);
    printf("GWYD uint32 %g Mnum/s (s = %u)\n",
           niter/gwy_benchmark_timer_get_total()/1e6, su32);

    su64 = run_g_rand_guint64(glib_rng, niter, rand_seed);
    printf("GLIB uint64 %g Mnum/s (s = %" G_GUINT64_FORMAT ")\n",
           niter/gwy_benchmark_timer_get_total()/1e6, su64);

    su64 = run_gwy_rand_guint64(gwyd_rng, niter, rand_seed);
    printf("GWYD uint64 %g Mnum/s (s = %" G_GUINT64_FORMAT ")\n",
           niter/gwy_benchmark_timer_get_total()/1e6, su64);

    sdbl = run_g_rand_double(glib_rng, niter, rand_seed);
    printf("GLIB double %g Mnum/s (s = %g)\n",
           niter/gwy_benchmark_timer_get_total()/1e6, sdbl/niter);

    sdbl = run_gwy_rand_double(gwyd_rng, niter, rand_seed);
    printf("GWYD double %g Mnum/s (s = %g)\n",
           niter/gwy_benchmark_timer_get_total()/1e6, sdbl/niter);

    g_rand_free(glib_rng);
    gwy_rand_free(gwyd_rng);

    return 0;
}

/*

   MT-32:
   GLIB uint32 70.9328 Mnum/s (s = 4268551034)
   GLIB uint64 28.4376 Mnum/s (s = 11933144865068391234)
   GLIB double 29.032 Mnum/s (s = 0.499964)

   MT-64:
   GWYD uint32 85.2644 Mnum/s (s = 1276514338)
   GWYD uint64 69.3587 Mnum/s (s = 6195566869960707510)
   GWYD double 31.5505 Mnum/s (s = 0.499963)

*/


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
