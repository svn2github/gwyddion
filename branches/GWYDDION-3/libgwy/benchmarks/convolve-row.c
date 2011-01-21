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

void
convolve_rows(guint xres, guint yres,
              guint kresmin, guint kresmax,
              GRand *rng)
{
    GwyField *source = gwy_field_new_sized(xres, yres, FALSE);
    randomize(source->data, xres*yres, rng);

    GwyField *field = gwy_field_new_alike(source, FALSE);
    for (guint kres = kresmin; kres <= kresmax; kres++) {
        GwyLine *kernel = gwy_line_new_sized(kres, FALSE);
        randomize(kernel->data, kres, rng);

        gwy_tune_algorithms("convolution-method", "direct");
        gwy_benchmark_timer_start();
        gwy_field_row_convolve(source, NULL, field, kernel,
                               GWY_EXTERIOR_MIRROR_EXTEND, 0.0);
        gwy_benchmark_timer_stop();
        gdouble tdirect = gwy_benchmark_timer_get_total()/yres;

        gwy_field_copy_full(source, field);
        gwy_tune_algorithms("convolution-method", "fft");
        gwy_benchmark_timer_start();
        gwy_field_row_convolve(source, NULL, field, kernel,
                               GWY_EXTERIOR_MIRROR_EXTEND, 0.0);
        gwy_benchmark_timer_stop();
        gdouble tfft = gwy_benchmark_timer_get_total()/yres;

        printf("%u %u %g %g\n", xres, kres, tdirect, tfft);
        g_object_unref(kernel);
    }
    // for gnuplot
    printf("\n");

    g_object_unref(field);
    g_object_unref(source);
}

int
main(int argc, char *argv[])
{
    guint xres_min = 2, xres_max = 10000,
          kres_min = 1, kres_max = 299,
          rand_seed = 42;
    gdouble xres_step = 1.06;

    GOptionEntry entries[] = {
        { "xres-min",    's', 0, G_OPTION_ARG_INT,    &xres_min,  "Smallest field xres to test", "N",  },
        { "xres-max",    'S', 0, G_OPTION_ARG_INT,    &xres_max,  "Largest field xres to test",  "N",  },
        { "xres-step",   'q', 0, G_OPTION_ARG_DOUBLE, &xres_step, "Field xres step factor",      "Q",  },
        { "kres-min",    'k', 0, G_OPTION_ARG_INT,    &kres_min,  "Smallest kernel res to test", "N",  },
        { "kernel-max",  'K', 0, G_OPTION_ARG_INT,    &kres_max,  "Largest kernel res to test",  "N",  },
        { "random-seed", 'r', 0, G_OPTION_ARG_INT,    &rand_seed, "Random seed",                 "R",  },
        { NULL,          0,   0, 0,                   NULL,       NULL,                          NULL, },
    };

    GError *error = NULL;
    GOptionContext *context = g_option_context_new("- compare direct and FFT convolution performance");
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

    gwy_fft_load_wisdom();

    for (guint xres = xres_min;
         xres <= xres_max;
         xres = MAX(xres + 1, (guint)(xres_step*xres + 0.5))) {

        guint yres = MAX(100, 100000/xres);
        convolve_rows(xres, yres, kres_min, kres_max, rng);
    }

    g_rand_free(rng);
    gwy_fft_save_wisdom();

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
