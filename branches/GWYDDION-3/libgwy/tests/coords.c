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

typedef void (*VectorTransformFunc)(gdouble *xy, const gdouble *parameters);
typedef void (*BitmaskTransformFunc)(gdouble *xy, guint bitmask);
typedef GwyCoords* (*NewCoordsFunc)(void);

/***************************************************************************
 *
 * Generic Coords functions
 *
 ***************************************************************************/

static void
coords_new_one(GType type,
               NewCoordsFunc func)
{
    g_assert(g_type_is_a(type, GWY_TYPE_COORDS));
    g_assert(G_TYPE_IS_INSTANTIATABLE(type));
    GwyCoords *coords = func();
    g_assert(coords);
    g_assert_cmpuint(G_OBJECT_TYPE(coords), ==, type);
    g_object_unref(coords);
}

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
    const guint *dimension_map = gwy_coords_dimension_map(coords);
    g_assert_cmpuint(shape_size, >, 0);
    for (guint i = 0; i < shape_size; i++) {
        guint mi = dimension_map[i];
        g_assert_cmpuint(mi, <, dimension);
        g_assert(gwy_coords_get_mapped_units(coords, i)
                 == gwy_coords_get_units(coords, mi));
        GwyUnit *unit = nth_unit(mi);
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

static void
record_index(GwyIntSet *indices, gint value)
{
    g_assert(!gwy_int_set_contains(indices, value));
    gwy_int_set_add(indices, value);
}

static void
assert_not_index(const gchar *signalname, gint value)
{
    g_critical("Signal \"%s\" emitted for value %d.", signalname, value);
    g_assert_not_reached();
}

static void
coords_new_subset_one(GType type)
{
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 150;

    for (guint iter = 0; iter < niter; iter++) {
        GwyCoords *coords = GWY_COORDS(g_object_newv(type, 0, NULL));
        coords_randomize(coords, rng);
        guint n = gwy_coords_size(coords);
        guint shape_size = gwy_coords_shape_size(coords);

        GwyIntSet *indices = gwy_int_set_new();
        if (n)
            int_set_randomize_range(indices, rng, 0, n-1);
        if (g_rand_int_range(rng, 0, 30) == 0)
            GWY_OBJECT_UNREF(indices);

        guint nsubset = indices ? gwy_int_set_size(indices) : n;

        GwyCoords *extracted = gwy_coords_new_subset(coords, indices);
        g_assert_cmpuint(gwy_coords_size(extracted), ==, nsubset);

        gint *values = indices ? gwy_int_set_values(indices, NULL) : NULL;

        for (guint i = 0; i < nsubset; i++) {
            gdouble xy[shape_size], xyref[shape_size];
            gwy_coords_get(extracted, i, xy);
            gwy_coords_get(coords, values ? (guint)values[i] : i, xyref);
            for (guint j = 0; j < shape_size; j++)
                g_assert_cmpfloat(xy[j], ==, xyref[j]);
        }

        GWY_FREE(values);
        GWY_OBJECT_UNREF(indices);
        g_object_unref(extracted);
        g_object_unref(coords);
    }
    g_rand_free(rng);
}

static void
coords_delete_subset_one(GType type)
{
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 150;

    for (guint iter = 0; iter < niter; iter++) {
        GwyCoords *coords = GWY_COORDS(g_object_newv(type, 0, NULL));
        coords_randomize(coords, rng);
        guint n = gwy_coords_size(coords);
        guint shape_size = gwy_coords_shape_size(coords);

        GwyIntSet *indices = gwy_int_set_new();
        if (n)
            int_set_randomize_range(indices, rng, 0, n-1);
        if (g_rand_int_range(rng, 0, 30) == 0)
            GWY_OBJECT_UNREF(indices);

        GwyIntSet *complement = gwy_int_set_new();
        if (indices) {
            for (guint i = 0; i < n; i++) {
                if (!gwy_int_set_contains(indices, i))
                    gwy_int_set_add(complement, i);
            }
            g_assert_cmpuint(gwy_int_set_size(complement),
                             ==,
                             n - gwy_int_set_size(indices));
        }
        guint nsubset = gwy_int_set_size(complement);

        GwyCoords *extracted = gwy_coords_duplicate(coords);
        g_assert_cmpuint(gwy_coords_size(extracted), ==, n);
        gwy_coords_delete_subset(extracted, indices);
        g_assert_cmpuint(gwy_coords_size(extracted), ==, nsubset);

        gint *values = gwy_int_set_values(complement, NULL);
        for (guint i = 0; i < nsubset; i++) {
            gdouble xy[shape_size], xyref[shape_size];
            gwy_coords_get(extracted, i, xy);
            gwy_coords_get(coords, values[i], xyref);
            for (guint j = 0; j < shape_size; j++)
                g_assert_cmpfloat(xy[j], ==, xyref[j]);
        }

        g_free(values);
        GWY_OBJECT_UNREF(indices);
        g_object_unref(complement);
        g_object_unref(extracted);
        g_object_unref(coords);
    }
    g_rand_free(rng);
}

static void
coords_transform_one(GType type,
                     VectorTransformFunc vector_transform_func,
                     BitmaskTransformFunc bitmask_transform_func,
                     GwyCoordsTransformFlags transform)
{
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;
    GwyCoordsClass *klass = g_type_class_ref(type);
    g_assert(gwy_coords_class_can_transform(klass, transform));
    g_type_class_unref(klass);

    for (guint iter = 0; iter < niter; iter++) {
        GwyCoords *coords = GWY_COORDS(g_object_newv(type, 0, NULL));
        g_assert(gwy_coords_can_transform(coords, transform));
        guint dimension = gwy_coords_dimension(coords);
        guint shape_size = gwy_coords_shape_size(coords);
        g_assert_cmpuint(dimension, >, 0);

        coords_randomize(coords, rng);
        GwyCoords *alltrans = gwy_coords_duplicate(coords);
        GwyCoords *reference = gwy_coords_duplicate(coords);

        guint n = gwy_coords_size(coords);
        g_assert_cmpuint(gwy_coords_size(alltrans), ==, n);
        g_assert_cmpuint(gwy_coords_size(reference), ==, n);

        GwyIntSet *indices = gwy_int_set_new();
        if (n)
            int_set_randomize_range(indices, rng, 0, n-1);

        gdouble parameters[dimension];
        gwy_clear(parameters, dimension);
        guint bitmask = 0;
        if (transform == GWY_COORDS_TRANSFORM_TRANSLATE) {
            for (guint i = 0; i < dimension; i++)
                parameters[i] = g_rand_double_range(rng, -5.0, 5.0);
        }
        else if (transform == GWY_COORDS_TRANSFORM_SCALE) {
            for (guint i = 0; i < dimension; i++)
                parameters[i] = exp(g_rand_double_range(rng, -5.0, 5.0));
        }
        else if (transform == GWY_COORDS_TRANSFORM_FLIP) {
            do {
                bitmask = g_rand_int_range(rng, 0, 1 << dimension);
            } while (!bitmask);
        }
        else {
            g_assert_not_reached();
        }

        GwyIntSet *coords_updated = gwy_int_set_new(),
                  *alltrans_updated = gwy_int_set_new();
        g_signal_connect_swapped(coords, "item-updated",
                                 G_CALLBACK(record_index), coords_updated);
        g_signal_connect_swapped(coords, "item-inserted",
                                 G_CALLBACK(assert_not_index),
                                 (gpointer)"item-inserted");
        g_signal_connect_swapped(coords, "item-inserted",
                                 G_CALLBACK(assert_not_index),
                                 (gpointer)"item-deleted");
        g_signal_connect_swapped(alltrans, "item-updated",
                                 G_CALLBACK(record_index), alltrans_updated);
        g_signal_connect_swapped(alltrans, "item-inserted",
                                 G_CALLBACK(assert_not_index),
                                 (gpointer)"item-inserted");
        g_signal_connect_swapped(alltrans, "item-inserted",
                                 G_CALLBACK(assert_not_index),
                                 (gpointer)"item-deleted");
        g_signal_connect_swapped(reference, "item-updated",
                                 G_CALLBACK(assert_not_index),
                                 (gpointer)"item-updated");
        g_signal_connect_swapped(reference, "item-inserted",
                                 G_CALLBACK(assert_not_index),
                                 (gpointer)"item-inserted");
        g_signal_connect_swapped(reference, "item-inserted",
                                 G_CALLBACK(assert_not_index),
                                 (gpointer)"item-deleted");

        if (transform == GWY_COORDS_TRANSFORM_TRANSLATE)
            gwy_coords_translate(coords, indices, parameters);
        else if (transform == GWY_COORDS_TRANSFORM_FLIP)
            gwy_coords_flip(coords, indices, bitmask);
        else if (transform == GWY_COORDS_TRANSFORM_SCALE)
            gwy_coords_scale(coords, indices, parameters);
        else {
            g_assert_not_reached();
        }
        int_set_assert_equal(coords_updated, indices);

        if (transform == GWY_COORDS_TRANSFORM_TRANSLATE)
            gwy_coords_translate(alltrans, NULL, parameters);
        else if (transform == GWY_COORDS_TRANSFORM_FLIP)
            gwy_coords_flip(alltrans, NULL, bitmask);
        else if (transform == GWY_COORDS_TRANSFORM_SCALE)
            gwy_coords_scale(alltrans, NULL, parameters);
        else {
            g_assert_not_reached();
        }
        g_assert_cmpuint(gwy_int_set_size(alltrans_updated), ==, n);

        for (guint i = 0; i < n; i++) {
            gdouble xy[shape_size], xyref[shape_size], xyall[shape_size],
                    xytrans[shape_size];

            gwy_coords_get(coords, i, xy);
            gwy_coords_get(reference, i, xyref);
            gwy_coords_get(alltrans, i, xyall);
            gwy_assign(xytrans, xyref, shape_size);
            if (transform == GWY_COORDS_TRANSFORM_TRANSLATE
                || transform == GWY_COORDS_TRANSFORM_SCALE)
                vector_transform_func(xytrans, parameters);
            else if (transform == GWY_COORDS_TRANSFORM_FLIP)
                bitmask_transform_func(xytrans, bitmask);
            else {
                g_assert_not_reached();
            }

            for (guint j = 0; j < shape_size; j++) {
                if (gwy_int_set_contains(indices, i)) {
                    g_assert_cmpfloat(xy[j], ==, xyall[j]);
                    gwy_assert_floatval(xy[j], xytrans[j], 1e-15);
                }
                else {
                    g_assert_cmpfloat(xy[j], ==, xyref[j]);
                }
                gwy_assert_floatval(xyall[j], xytrans[j], 1e-15);
                if (transform != GWY_COORDS_TRANSFORM_FLIP) {
                    g_assert_cmpfloat(xyall[j], !=, xyref[j]);
                }
            }
        }

        if (transform == GWY_COORDS_TRANSFORM_TRANSLATE) {
            for (guint i = 0; i < dimension; i++)
                parameters[i] = -parameters[i];
        }
        else if (transform == GWY_COORDS_TRANSFORM_FLIP) {
            // Nothing to do here.
        }
        else if (transform == GWY_COORDS_TRANSFORM_SCALE) {
            for (guint i = 0; i < dimension; i++)
                parameters[i] = 1.0/parameters[i];
        }
        else {
            g_assert_not_reached();
        }

        gwy_int_set_update(coords_updated, NULL, 0);
        gwy_int_set_update(alltrans_updated, NULL, 0);

        if (transform == GWY_COORDS_TRANSFORM_TRANSLATE)
            gwy_coords_translate(coords, indices, parameters);
        else if (transform == GWY_COORDS_TRANSFORM_FLIP)
            gwy_coords_flip(coords, indices, bitmask);
        else if (transform == GWY_COORDS_TRANSFORM_SCALE)
            gwy_coords_scale(coords, indices, parameters);
        else {
            g_assert_not_reached();
        }
        int_set_assert_equal(coords_updated, indices);

        if (transform == GWY_COORDS_TRANSFORM_TRANSLATE)
            gwy_coords_translate(alltrans, NULL, parameters);
        else if (transform == GWY_COORDS_TRANSFORM_FLIP)
            gwy_coords_flip(alltrans, NULL, bitmask);
        else if (transform == GWY_COORDS_TRANSFORM_SCALE)
            gwy_coords_scale(alltrans, NULL, parameters);
        else {
            g_assert_not_reached();
        }
        g_assert_cmpuint(gwy_int_set_size(alltrans_updated), ==, n);

        g_object_unref(alltrans_updated);
        g_object_unref(coords_updated);

        for (guint i = 0; i < n; i++) {
            gdouble xy[shape_size], xyref[shape_size], xyall[shape_size];

            gwy_coords_get(coords, i, xy);
            gwy_coords_get(reference, i, xyref);
            gwy_coords_get(alltrans, i, xyall);
            for (guint j = 0; j < shape_size; j++) {
                gwy_assert_floatval(xy[j], xyref[j], 1e-15);
                gwy_assert_floatval(xyall[j], xyref[j], 1e-15);
            }
        }

        g_object_unref(indices);
        g_object_unref(reference);
        g_object_unref(coords);
        g_object_unref(alltrans);
    }
    g_rand_free(rng);
}

static void
coords_constrain_translation_one(GType type)
{
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 100;
    GwyCoordsClass *klass = g_type_class_ref(type);
    g_assert(gwy_coords_class_can_transform(klass,
                                            GWY_COORDS_TRANSFORM_TRANSLATE));
    g_type_class_unref(klass);

    for (guint iter = 0; iter < niter; iter++) {
        GwyCoords *coords = GWY_COORDS(g_object_newv(type, 0, NULL));
        g_assert(gwy_coords_can_transform(coords,
                                          GWY_COORDS_TRANSFORM_TRANSLATE));
        guint dimension = gwy_coords_dimension(coords);
        g_assert_cmpuint(dimension, >, 0);

        coords_randomize(coords, rng);
        guint shape_size = gwy_coords_shape_size(coords);
        guint n = gwy_coords_size(coords);

        GwyIntSet *indices = gwy_int_set_new();
        for (guint i = 0; i < n; i++) {
            if (g_rand_boolean(rng))
                gwy_int_set_add(indices, i);
        }
        if (!gwy_int_set_size(indices)) {
            GWY_OBJECT_UNREF(indices);
        }

        gdouble offsets[dimension], lower[dimension], upper[dimension];
        for (guint i = 0; i < dimension; i++) {
            offsets[i] = g_rand_double_range(rng, -2.0, 2.0);
            lower[i] = g_rand_double_range(rng, -1.6, -1.0);
            upper[i] = g_rand_double_range(rng, 1.0, 1.6);
            GWY_ORDER(gdouble, lower[i], upper[i]);
        }

        gwy_coords_constrain_translation(coords, indices, offsets,
                                         lower, upper);
        gwy_coords_translate(coords, indices, offsets);

        const guint *dimension_map = gwy_coords_dimension_map(coords);
        for (guint i = 0; i < n; i++) {
            if (indices && !gwy_int_set_contains(indices, i))
                continue;

            gdouble data[shape_size];
            gwy_coords_get(coords, i, data);
            for (guint j = 0; j < shape_size; j++) {
                g_assert_cmpfloat(data[j], <=, upper[dimension_map[j]]);
                g_assert_cmpfloat(data[j], >=, lower[dimension_map[j]]);
            }
        }

        GWY_OBJECT_UNREF(indices);
        g_object_unref(coords);
    }
    g_rand_free(rng);
}

/***************************************************************************
 *
 * CoordsPoint
 *
 ***************************************************************************/

void
test_coords_point_new(void)
{
    coords_new_one(GWY_TYPE_COORDS_POINT, gwy_coords_point_new);
}

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

void
test_coords_point_new_subset(void)
{
    coords_new_subset_one(GWY_TYPE_COORDS_POINT);
}

void
test_coords_point_delete_subset(void)
{
    coords_delete_subset_one(GWY_TYPE_COORDS_POINT);
}

static void
translate_point(gdouble *xy, const gdouble *dxy)
{
    xy[0] += dxy[0];
    xy[1] += dxy[1];
}

void
test_coords_point_translate(void)
{
    coords_transform_one(GWY_TYPE_COORDS_POINT,
                         translate_point, NULL,
                         GWY_COORDS_TRANSFORM_TRANSLATE);
}

static void
flip_point(gdouble *xy, guint axes)
{
    xy[0] = (axes & 1) ? -xy[0] : xy[0];
    xy[1] = (axes & 2) ? -xy[1] : xy[1];
}

void
test_coords_point_flip(void)
{
    coords_transform_one(GWY_TYPE_COORDS_POINT,
                         NULL, flip_point,
                         GWY_COORDS_TRANSFORM_FLIP);
}

static void
scale_point(gdouble *xy, const gdouble *sxy)
{
    xy[0] *= sxy[0];
    xy[1] *= sxy[1];
}

void
test_coords_point_scale(void)
{
    coords_transform_one(GWY_TYPE_COORDS_POINT,
                         scale_point, NULL,
                         GWY_COORDS_TRANSFORM_SCALE);
}

void
test_coords_point_constrain_translation(void)
{
    coords_constrain_translation_one(GWY_TYPE_COORDS_POINT);
}

/***************************************************************************
 *
 * CoordsLine
 *
 ***************************************************************************/

void
test_coords_line_new(void)
{
    coords_new_one(GWY_TYPE_COORDS_LINE, gwy_coords_line_new);
}

void
test_coords_line_finished(void)
{
    coords_finished_one(GWY_TYPE_COORDS_LINE);
}

void
test_coords_line_units(void)
{
    coords_units_one(GWY_TYPE_COORDS_LINE);
}

void
test_coords_line_data(void)
{
    coords_data_one(GWY_TYPE_COORDS_LINE);
}

void
test_coords_line_serialize(void)
{
    coords_serialize_one(GWY_TYPE_COORDS_LINE);
}

void
test_coords_line_new_subset(void)
{
    coords_new_subset_one(GWY_TYPE_COORDS_LINE);
}

void
test_coords_line_delete_subset(void)
{
    coords_delete_subset_one(GWY_TYPE_COORDS_LINE);
}

static void
translate_line(gdouble *xy, const gdouble *dxy)
{
    xy[0] += dxy[0];
    xy[1] += dxy[1];
    xy[2] += dxy[0];
    xy[3] += dxy[1];
}

void
test_coords_line_translate(void)
{
    coords_transform_one(GWY_TYPE_COORDS_LINE,
                         translate_line, NULL,
                         GWY_COORDS_TRANSFORM_TRANSLATE);
}

static void
flip_line(gdouble *xy, guint axes)
{
    xy[0] = (axes & 1) ? -xy[0] : xy[0];
    xy[1] = (axes & 2) ? -xy[1] : xy[1];
    xy[2] = (axes & 1) ? -xy[2] : xy[2];
    xy[3] = (axes & 2) ? -xy[3] : xy[3];
}

void
test_coords_line_flip(void)
{
    coords_transform_one(GWY_TYPE_COORDS_LINE,
                         NULL, flip_line,
                         GWY_COORDS_TRANSFORM_FLIP);
}

static void
scale_line(gdouble *xy, const gdouble *sxy)
{
    xy[0] *= sxy[0];
    xy[1] *= sxy[1];
    xy[2] *= sxy[0];
    xy[3] *= sxy[1];
}

void
test_coords_line_scale(void)
{
    coords_transform_one(GWY_TYPE_COORDS_LINE,
                         scale_line, NULL,
                         GWY_COORDS_TRANSFORM_SCALE);
}

void
test_coords_line_constrain_translation(void)
{
    coords_constrain_translation_one(GWY_TYPE_COORDS_LINE);
}

/***************************************************************************
 *
 * CoordsRectangle
 *
 ***************************************************************************/

void
test_coords_rectangle_new(void)
{
    coords_new_one(GWY_TYPE_COORDS_RECTANGLE, gwy_coords_rectangle_new);
}

void
test_coords_rectangle_finished(void)
{
    coords_finished_one(GWY_TYPE_COORDS_RECTANGLE);
}

void
test_coords_rectangle_units(void)
{
    coords_units_one(GWY_TYPE_COORDS_RECTANGLE);
}

void
test_coords_rectangle_data(void)
{
    coords_data_one(GWY_TYPE_COORDS_RECTANGLE);
}

void
test_coords_rectangle_serialize(void)
{
    coords_serialize_one(GWY_TYPE_COORDS_RECTANGLE);
}

void
test_coords_rectangle_new_subset(void)
{
    coords_new_subset_one(GWY_TYPE_COORDS_RECTANGLE);
}

void
test_coords_rectangle_delete_subset(void)
{
    coords_delete_subset_one(GWY_TYPE_COORDS_RECTANGLE);
}

static void
translate_rectangle(gdouble *xy, const gdouble *dxy)
{
    xy[0] += dxy[0];
    xy[1] += dxy[1];
    xy[2] += dxy[0];
    xy[3] += dxy[1];
}

void
test_coords_rectangle_translate(void)
{
    coords_transform_one(GWY_TYPE_COORDS_RECTANGLE,
                         translate_rectangle, NULL,
                         GWY_COORDS_TRANSFORM_TRANSLATE);
}

static void
flip_rectangle(gdouble *xy, guint axes)
{
    xy[0] = (axes & 1) ? -xy[0] : xy[0];
    xy[1] = (axes & 2) ? -xy[1] : xy[1];
    xy[2] = (axes & 1) ? -xy[2] : xy[2];
    xy[3] = (axes & 2) ? -xy[3] : xy[3];
}

void
test_coords_rectangle_flip(void)
{
    coords_transform_one(GWY_TYPE_COORDS_RECTANGLE,
                         NULL, flip_rectangle,
                         GWY_COORDS_TRANSFORM_FLIP);
}

static void
scale_rectangle(gdouble *xy, const gdouble *sxy)
{
    xy[0] *= sxy[0];
    xy[1] *= sxy[1];
    xy[2] *= sxy[0];
    xy[3] *= sxy[1];
}

void
test_coords_rectangle_scale(void)
{
    coords_transform_one(GWY_TYPE_COORDS_RECTANGLE,
                         scale_rectangle, NULL,
                         GWY_COORDS_TRANSFORM_SCALE);
}

void
test_coords_rectangle_constrain_translation(void)
{
    coords_constrain_translation_one(GWY_TYPE_COORDS_RECTANGLE);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
