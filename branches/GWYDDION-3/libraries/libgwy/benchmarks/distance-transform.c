/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
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
randomize(GwyMaskField *field, GRand *rng, gdouble p)
{

    for (guint i = 0; i < field->yres; i++) {
        GwyMaskIter iter;
        gwy_mask_field_iter_init(field, iter, 0, i);
        for (guint j = 0; j < field->xres; j++) {
            gboolean v = (g_rand_double(rng) <= p);
            gwy_mask_iter_set(iter, v);
            gwy_mask_iter_next(iter);
        }
    }
    gwy_mask_field_invalidate(field);
}

static void
squarize(GwyMaskField *field)
{
    gwy_mask_field_fill(field, NULL, TRUE);
    gwy_mask_field_set(field, field->xres/2, field->yres/2, FALSE);
    gwy_mask_field_invalidate(field);
}

void
distance_transform(guint xres,
                   GRand *rng)
{
    guint niter = MAX(10000000/(xres*xres + 8), 1);
    GwyMaskField *field = gwy_mask_field_new_sized(xres, xres, FALSE);

    gdouble trandom9 = 0.0;
    for (guint i = 0; i < niter; i++) {
        randomize(field, rng, 0.9);
        gwy_benchmark_timer_start();
        gwy_mask_field_distance_transform(field);
        gwy_benchmark_timer_stop();
        trandom9 += gwy_benchmark_timer_get_user();
    }

    gdouble trandom5 = 0.0;
    for (guint i = 0; i < niter; i++) {
        randomize(field, rng, 0.5);
        gwy_benchmark_timer_start();
        gwy_mask_field_distance_transform(field);
        gwy_benchmark_timer_stop();
        trandom5 += gwy_benchmark_timer_get_user();
    }

    gdouble tsquare = 0.0;
    for (guint i = 0; i < niter; i++) {
        squarize(field);
        gwy_benchmark_timer_start();
        gwy_mask_field_distance_transform(field);
        gwy_benchmark_timer_stop();
        tsquare += gwy_benchmark_timer_get_user();
    }

    printf("%u %g %g %g\n", xres, trandom9/niter, trandom5/niter, tsquare/niter);

    g_object_unref(field);
}

int
main(int argc, char *argv[])
{
    guint xres_min = 2, xres_max = 10000,
          rand_seed = 42;
    gdouble xres_step = 1.06;

    GOptionEntry entries[] = {
        { "xres-min",    's', 0, G_OPTION_ARG_INT,    &xres_min,  "Smallest field xres to test", "N",  },
        { "xres-max",    'S', 0, G_OPTION_ARG_INT,    &xres_max,  "Largest field xres to test",  "N",  },
        { "xres-step",   'q', 0, G_OPTION_ARG_DOUBLE, &xres_step, "Field xres step factor",      "Q",  },
        { "random-seed", 'r', 0, G_OPTION_ARG_INT,    &rand_seed, "Random seed",                 "R",  },
        { NULL,          0,   0, 0,                   NULL,       NULL,                          NULL, },
    };

    GError *error = NULL;
    GOptionContext *context = g_option_context_new("- measure distance transform speed");
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

    for (guint xres = xres_min;
         xres <= xres_max;
         xres = MAX(xres + 1, (guint)(xres_step*xres + 0.5))) {

        distance_transform(xres, rng);
    }

    g_rand_free(rng);

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
