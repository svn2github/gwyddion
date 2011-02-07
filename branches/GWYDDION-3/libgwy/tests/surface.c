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

#include "testlibgwy.h"

/***************************************************************************
 *
 * Surface
 *
 ***************************************************************************/

void
surface_randomize(GwySurface *surface,
                  GRand *rng)
{
    GwyXYZ *d = surface->data;
    for (guint n = surface->n; n; n--, d++) {
        d->x = g_rand_double(rng);
        d->y = g_rand_double(rng);
        d->z = g_rand_double(rng);
    }
}

static void
surface_assert_equal(const GwySurface *result,
                     const GwySurface *reference)
{
    g_assert(GWY_IS_SURFACE(result));
    g_assert(GWY_IS_SURFACE(reference));
    g_assert_cmpuint(result->n, ==, reference->n);
    compare_properties(G_OBJECT(result), G_OBJECT(reference));

    for (guint i = 0; i < result->n; i++) {
        GwyXYZ resxy = result->data[i];
        GwyXYZ refxy = reference->data[i];
        g_assert_cmpfloat(resxy.x, ==, refxy.x);
        g_assert_cmpfloat(resxy.y, ==, refxy.y);
        g_assert_cmpfloat(resxy.z, ==, refxy.z);
    }
}

static void
surface_assert_equal_object(GObject *object, GObject *reference)
{
    surface_assert_equal(GWY_SURFACE(object), GWY_SURFACE(reference));
}

void
test_surface_serialize(void)
{
    enum { max_size = 55 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 50 : 10;

    for (guint iter = 0; iter < niter; iter++) {
        guint res = g_rand_int_range(rng, 1, max_size);
        GwySurface *original = gwy_surface_new_sized(res);
        surface_randomize(original, rng);
        GwySurface *copy;

        serializable_duplicate(GWY_SERIALIZABLE(original),
                               surface_assert_equal_object);
        serializable_assign(GWY_SERIALIZABLE(original),
                            surface_assert_equal_object);
        copy = GWY_SURFACE(serialize_and_back(G_OBJECT(original),
                                              surface_assert_equal_object));
        g_object_unref(copy);

        g_object_unref(original);
    }
    g_rand_free(rng);
}

void
test_surface_props(void)
{
    static const GwyXYZ data[] = { { 1, 2, 3 }, { 4, 5, 6 } };
    GwySurface *surface = gwy_surface_new_from_data(data, G_N_ELEMENTS(data));
    guint n = 0;
    g_object_get(surface,
                 "n-points", &n,
                 NULL);
    g_assert_cmpuint(n, ==, surface->n);
    g_assert_cmpuint(n, ==, G_N_ELEMENTS(data));
    g_object_unref(surface);
}

void
test_surface_data_changed(void)
{
    GwySurface *surface = gwy_surface_new();
    guint counter = 0;
    g_signal_connect_swapped(surface, "data-changed",
                             G_CALLBACK(record_signal), &counter);
    gwy_surface_data_changed(surface);
    g_assert_cmpuint(counter, ==, 1);
    g_object_unref(surface);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
