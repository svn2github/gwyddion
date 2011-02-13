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

G_GNUC_UNUSED
static void
print_field(const gchar *name, const GwyField *field)
{
    g_printerr("=====[ %s ]=====\n", name);
    for (guint i = 0; i < field->yres; i++) {
        g_printerr("[%02u]", i);
        for (guint j = 0; j < field->xres; j++) {
            g_printerr(" % .4f", field->data[i*field->xres + j]);
        }
        g_printerr("\n");
    }
}

void
test_surface_regularize(void)
{
    enum { max_size = 27 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 50 : 10;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 2, max_size);
        guint yres = g_rand_int_range(rng, 2, max_size);
        gdouble step = g_rand_double_range(rng, 1, G_E)
                       * exp(g_rand_double_range(rng, -8, 8));
        gdouble xoff = g_rand_double_range(rng, -5, 5)*step;
        gdouble yoff = g_rand_double_range(rng, -5, 5)*step;
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        gwy_field_set_xreal(field, xres*step);
        gwy_field_set_yreal(field, yres*step);
        gwy_field_set_xoffset(field, xoff);
        gwy_field_set_yoffset(field, yoff);

        GwySurface *surface = gwy_surface_new_from_field(field);
        g_assert_cmpuint(surface->n, ==, field->xres*field->yres);
        g_assert(gwy_unit_equal(gwy_surface_get_unit_xy(surface),
                                gwy_field_get_unit_xy(field)));
        g_assert(gwy_unit_equal(gwy_surface_get_unit_z(surface),
                                gwy_field_get_unit_z(field)));

        GwyField *newfield = gwy_surface_regularize_full
                               (surface, GWY_SURFACE_REGULARIZE_PREVIEW, 0, 0);
        g_assert_cmpuint(newfield->xres*newfield->yres, ==, surface->n);
        g_assert_cmpfloat(fabs(newfield->xreal - field->xreal),
                          <=,
                          1e-14*field->xreal);
        g_assert_cmpfloat(fabs(newfield->yreal - field->yreal),
                          <=,
                          1e-14*field->yreal);
        g_assert_cmpfloat(fabs(newfield->xoff - field->xoff),
                          <=,
                          1e-14*fabs(field->xoff));
        g_assert_cmpfloat(fabs(newfield->yoff - field->yoff),
                          <=,
                          1e-14*fabs(field->yoff));
        g_assert(gwy_unit_equal(gwy_surface_get_unit_xy(surface),
                                gwy_field_get_unit_xy(field)));
        g_assert(gwy_unit_equal(gwy_surface_get_unit_z(surface),
                                gwy_field_get_unit_z(field)));
        field_assert_numerically_equal(newfield, field, 1e-14);

        g_object_unref(newfield);
        g_object_unref(surface);

        surface = gwy_surface_new();
        gwy_surface_set_from_field(surface, field);
        g_assert_cmpuint(surface->n, ==, field->xres*field->yres);
        g_assert(gwy_unit_equal(gwy_surface_get_unit_xy(surface),
                                gwy_field_get_unit_xy(field)));
        g_assert(gwy_unit_equal(gwy_surface_get_unit_z(surface),
                                gwy_field_get_unit_z(field)));

        newfield = gwy_surface_regularize_full
                               (surface, GWY_SURFACE_REGULARIZE_PREVIEW, 0, 0);
        g_assert_cmpuint(newfield->xres*newfield->yres, ==, surface->n);
        g_assert_cmpfloat(fabs(newfield->xreal - field->xreal),
                          <=,
                          1e-14*field->xreal);
        g_assert_cmpfloat(fabs(newfield->yreal - field->yreal),
                          <=,
                          1e-14*field->yreal);
        g_assert_cmpfloat(fabs(newfield->xoff - field->xoff),
                          <=,
                          1e-14*fabs(field->xoff));
        g_assert_cmpfloat(fabs(newfield->yoff - field->yoff),
                          <=,
                          1e-14*fabs(field->yoff));
        g_assert(gwy_unit_equal(gwy_surface_get_unit_xy(surface),
                                gwy_field_get_unit_xy(field)));
        g_assert(gwy_unit_equal(gwy_surface_get_unit_z(surface),
                                gwy_field_get_unit_z(field)));
        field_assert_numerically_equal(newfield, field, 1e-14);

        g_object_unref(newfield);
        g_object_unref(surface);
        g_object_unref(field);
    }

    g_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
