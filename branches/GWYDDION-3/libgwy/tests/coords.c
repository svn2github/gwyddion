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

#include "testlibgwy.h"

/***************************************************************************
 *
 * Generic Coords functions
 *
 ***************************************************************************/

void
coords_assert_equal(const GwyCoords *result,
                    const GwyCoords *reference)
{
    g_assert(GWY_IS_COORDS(result));
    g_assert(GWY_IS_COORDS(reference));
    g_assert_cmpuint(gwy_coords_shape_size(result),
                     ==,
                     gwy_coords_shape_size(reference));
    g_assert_cmpuint(gwy_coords_size(result), ==, gwy_coords_size(reference));
    compare_properties(G_OBJECT(result), G_OBJECT(reference));

    guint n = gwy_coords_size(reference);
    guint shape_size = gwy_coords_shape_size(reference);
    for (guint i = 0; i < n; i++) {
        gdouble result_shape[shape_size], reference_shape[shape_size];
        gwy_coords_get(reference, i, reference_shape);
        gwy_coords_get(result, i, result_shape);
        for (guint j = 0; j < shape_size; j++)
            g_assert_cmpfloat(result_shape[j], ==, reference_shape[j]);
    }
}

static void
coords_assert_equal_object(GObject *object, GObject *reference)
{
    coords_assert_equal(GWY_COORDS(object), GWY_COORDS(reference));
}

static void
coords_finished_one(GType type)
{
    GwyCoords *coords = GWY_COORDS(g_object_newv(type, 0, NULL));
    guint counter = 0;
    g_signal_connect_swapped(coords, "finished",
                             G_CALLBACK(record_signal), &counter);
    gwy_coords_finished(coords);
    g_assert_cmpuint(counter, ==, 1);
    g_object_unref(coords);
}

static GwyUnit*
nth_unit(guint n)
{
    if (n == 0)
        return gwy_unit_new();
    if (n == 1)
        return gwy_unit_new_from_string("m", NULL);
    if (n == 2)
        return gwy_unit_new_from_string("A", NULL);
    if (n == 3)
        return gwy_unit_new_from_string("s", NULL);
    if (n == 4)
        return gwy_unit_new_from_string("kg", NULL);
    if (n == 5)
        return gwy_unit_new_from_string("N", NULL);
    if (n == 6)
        return gwy_unit_new_from_string("V", NULL);
    g_return_val_if_reached(gwy_unit_new());
}

static void
coords_units_one(GType type)
{
    GwyCoords *coords = GWY_COORDS(g_object_newv(type, 0, NULL));
    guint dimension = gwy_coords_dimension(coords);
    g_assert_cmpuint(dimension, >, 0);

    for (guint i = 0; i < dimension; i++) {
        GwyUnit *coord_unit = gwy_coords_get_units(coords, i);
        g_assert(GWY_IS_UNIT(coord_unit));
        GwyUnit *unit = nth_unit(i);
        gwy_unit_assign(coord_unit, unit);
        g_object_unref(unit);
    }

    guint shape_size = gwy_coords_shape_size(coords);
    const guint *unit_map = gwy_coords_unit_map(coords);
    g_assert_cmpuint(shape_size, >, 0);
    for (guint i = 0; i < shape_size; i++) {
        guint mi = unit_map[i];
        g_assert_cmpuint(mi, <, dimension);
        g_assert(gwy_coords_get_mapped_units(coords, i)
                 == gwy_coords_get_units(coords, mi));
        GwyUnit *unit = nth_unit(i);
        g_assert(gwy_unit_equal(gwy_coords_get_mapped_units(coords, i), unit));
        g_object_unref(unit);
    }

    g_object_unref(coords);
}

static void
coords_data_one(GType type)
{
    GwyCoords *coords = GWY_COORDS(g_object_newv(type, 0, NULL));
    guint shape_size = gwy_coords_shape_size(coords);

    GRand *rng = g_rand_new_with_seed(42);
    guint n = g_rand_int_range(rng, 0, 13);

    gdouble *data = g_new(gdouble, n*shape_size);
    for (guint i = 0; i < n*shape_size; i++)
        data[i] = g_rand_double(rng);

    gwy_coords_set_data(coords, n, data);
    g_assert_cmpuint(gwy_coords_size(coords), ==, n);

    for (guint i = 0; i < n; i++) {
        gdouble shape[shape_size];
        gwy_coords_get(coords, i, shape);
        for (guint j = 0; j < shape_size; j++)
            g_assert_cmpfloat(shape[j], ==, data[i*shape_size + j]);
    }

    gdouble *gotdata = g_new(gdouble, n*shape_size);
    gwy_coords_get_data(coords, gotdata);
    for (guint i = 0; i < n*shape_size; i++)
        g_assert_cmpfloat(gotdata[i], ==, data[i]);

    g_rand_free(rng);
    g_free(gotdata);
    g_free(data);
    g_object_unref(coords);
}

static void
coords_randomize(GwyCoords *coords,
                 GRand *rng)
{
    guint shape_size = gwy_coords_shape_size(coords);
    guint n = g_rand_int_range(rng, 0, 13);

    gdouble *data = g_new(gdouble, n*shape_size);
    for (guint i = 0; i < n*shape_size; i++)
        data[i] = g_rand_double(rng);

    gwy_coords_set_data(coords, n, data);
    g_assert_cmpuint(gwy_coords_size(coords), ==, n);
    g_free(data);
}

static void
coords_serialize_one(GType type)
{
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        GwyCoords *original = GWY_COORDS(g_object_newv(type, 0, NULL));
        coords_randomize(original, rng);
        GwyCoords *copy;

        serializable_duplicate(GWY_SERIALIZABLE(original),
                               coords_assert_equal_object);
        serializable_assign(GWY_SERIALIZABLE(original),
                            coords_assert_equal_object);
        copy = GWY_COORDS(serialize_and_back(G_OBJECT(original),
                                            coords_assert_equal_object));
        g_object_unref(copy);
        g_object_unref(original);
    }
    g_rand_free(rng);
}

/***************************************************************************
 *
 * CoordsPoint
 *
 ***************************************************************************/

void
test_coords_point_finished(void)
{
    coords_finished_one(GWY_TYPE_COORDS_POINT);
}

void
test_coords_point_units(void)
{
    coords_units_one(GWY_TYPE_COORDS_POINT);
}

void
test_coords_point_data(void)
{
    coords_data_one(GWY_TYPE_COORDS_POINT);
}

void
test_coords_point_serialize(void)
{
    coords_serialize_one(GWY_TYPE_COORDS_POINT);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
