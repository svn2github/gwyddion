/*
 *  $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
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
#include "libgwy/libgwy.h"

static void
randomize(gdouble *data, guint size, GRand *rng)
{
    for (guint n = size; n; n--, data++)
        *data = g_rand_double(rng);
}

static void
sum_vertical(const gdouble *data, guint n, gdouble *sums)
{
    for (guint j = 0; j < n; j++) {
        const gdouble *column = data + j;
        double s = 0.0;
        for (guint i = n; i; i--, column += n)
            s += *column;
        sums[j] += s;
    }
}

static void
sum_horizontal(const gdouble *data, guint n, gdouble *sums)
{
    for (guint i = 0; i < n; i++) {
        const gdouble *row = data + i*n;
        double *s = sums;
        for (guint j = n; j; j--, row++, s++)
            *s += *row;
    }
}

int
main(int argc, char *argv[])
{
    guint size_min = 12, size_max = 4000,
          rand_seed = 42;
    gdouble size_step = 1.06;

    GOptionEntry entries[] = {
        { "size-min",    's', 0, G_OPTION_ARG_INT,    &size_min,  "Smallest field size to test", "N",  },
        { "size-max",    'S', 0, G_OPTION_ARG_INT,    &size_max,  "Largest field size to test",  "N",  },
        { "size-step",   'q', 0, G_OPTION_ARG_DOUBLE, &size_step, "Field size step factor",      "Q",  },
        { "random-seed", 'r', 0, G_OPTION_ARG_INT,    &rand_seed, "Random seed",                 "R",  },
        { NULL,          0,   0, 0,                   NULL,       NULL,                          NULL, },
    };

    GError *error = NULL;
    GOptionContext *context = g_option_context_new("- compare efficiency of memory access patterns");
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("Arguments parsing failed: %s\n", error->message);
        return 1;
    }
    g_option_context_free(context);
    gwy_type_init();
    setvbuf(stdout, (char*)NULL, _IOLBF, 0);

    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, rand_seed);

    gdouble *data = g_new(gdouble, size_max*size_max);
    gdouble *sums = g_new0(gdouble, size_max);
    randomize(data, size_max*size_max, rng);

    for (guint size = size_min;
         size <= size_max;
         size = MAX(size + 1, (guint)(size_step*size + 0.5))) {
        gulong niters, i;
        gdouble th, tv;

        niters = 10000000UL/gwy_round(size*size*log10(size));
        niters = MAX(niters, 1);
        i = 0;
        gwy_benchmark_timer_start();
        do {
            niters *= 2;
            while (i < niters) {
                sum_horizontal(data, size, sums);
                i++;
            }
            gwy_benchmark_timer_stop();
        } while ((th = gwy_benchmark_timer_get_total()) < 1.0);
        th /= niters*size*size;

        niters = 10000000UL/gwy_round(size*size*log10(size));
        niters = MAX(niters, 1);
        i = 0;
        gwy_benchmark_timer_start();
        do {
            niters *= 2;
            while (i < niters) {
                sum_vertical(data, size, sums);
                i++;
            }
            gwy_benchmark_timer_stop();
        } while ((tv = gwy_benchmark_timer_get_total()) < 1.0);
        tv /= niters*size*size;

        printf("%u %g %g\n", size, th, tv);
    }

    g_rand_free(rng);

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
