/*
 *  $Id$
 *  Copyright (C) 2011-2012 David Nečas (Yeti).
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
test_surface_units_assign(void)
{
    GwySurface *surface = gwy_surface_new(), *surface2 = gwy_surface_new();
    GwyUnit *xyunit = gwy_surface_get_xyunit(surface);
    GwyUnit *zunit = gwy_surface_get_zunit(surface);
    guint count_xy = 0;
    guint count_z = 0;

    g_signal_connect_swapped(xyunit, "changed",
                             G_CALLBACK(record_signal), &count_xy);
    g_signal_connect_swapped(zunit, "changed",
                             G_CALLBACK(record_signal), &count_z);

    gwy_unit_set_from_string(gwy_surface_get_xyunit(surface), "m", NULL);
    g_assert_cmpuint(count_xy, ==, 1);
    g_assert_cmpuint(count_z, ==, 0);

    gwy_unit_set_from_string(gwy_surface_get_zunit(surface), "s", NULL);
    g_assert_cmpuint(count_xy, ==, 1);
    g_assert_cmpuint(count_z, ==, 1);

    gwy_surface_assign(surface, surface2);
    g_assert_cmpuint(count_xy, ==, 2);
    g_assert_cmpuint(count_z, ==, 2);
    g_assert(gwy_surface_get_xyunit(surface) == xyunit);
    g_assert(gwy_surface_get_zunit(surface) == zunit);

    // Try again to see if the signal counts change.
    gwy_surface_assign(surface, surface2);
    g_assert_cmpuint(count_xy, ==, 2);
    g_assert_cmpuint(count_z, ==, 2);
    g_assert(gwy_surface_get_xyunit(surface) == xyunit);
    g_assert(gwy_surface_get_zunit(surface) == zunit);

    g_object_unref(surface2);
    g_object_unref(surface);
}

void
test_surface_get(void)
{
    enum { max_size = 255, niter = 40 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint res = g_rand_int_range(rng, 1, max_size);
        GwySurface *surface = gwy_surface_new_sized(res);
        surface_randomize(surface, rng);

        for (guint i = 0; i < res; i++) {
            GwyXYZ pt = gwy_surface_get(surface, i);
            g_assert_cmpfloat(surface->data[i].x, ==, pt.x);
            g_assert_cmpfloat(surface->data[i].y, ==, pt.y);
            g_assert_cmpfloat(surface->data[i].z, ==, pt.z);
        }
        g_object_unref(surface);
    }

    g_rand_free(rng);
}

void
test_surface_set(void)
{
    enum { max_size = 255, niter = 40 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint res = g_rand_int_range(rng, 1, max_size);
        GwySurface *surface = gwy_surface_new_sized(res);

        for (guint i = 0; i < res; i++) {
            GwyXYZ pt = { i, G_PI/(i + 1), -0.5*i*i };
            gwy_surface_set(surface, i, pt);
        }

        for (guint k = 0; k < res; k++) {
            GwyXYZ pt = gwy_surface_get(surface, k);
            g_assert_cmpfloat(pt.x, ==, k);
            g_assert_cmpfloat(pt.y, ==, G_PI/(k + 1));
            g_assert_cmpfloat(pt.z, ==, -0.5*k*k);
        }
        g_object_unref(surface);
    }

    g_rand_free(rng);
}

void
test_surface_serialize(void)
{
    enum { max_size = 55 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = g_test_slow() ? 50 : 20;

    for (guint iter = 0; iter < niter; iter++) {
        guint res = g_rand_int_range(rng, 1, max_size);
        GwySurface *original = gwy_surface_new_sized(res);
        surface_randomize(original, rng);
        GwySurface *copy;

        if (g_rand_int(rng) % 5)
            unit_randomize(gwy_surface_get_xyunit(original), rng);
        if (g_rand_int(rng) % 5)
            unit_randomize(gwy_surface_get_zunit(original), rng);

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
test_surface_serialize_failure_odd(void)
{
    GOutputStream *stream = g_memory_output_stream_new(NULL, 0,
                                                       g_realloc, g_free);
    GDataOutputStream *datastream = g_data_output_stream_new(stream);
    g_data_output_stream_set_byte_order(datastream,
                                        G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

    guint len = 5;
    data_stream_put_string0(datastream, "GwySurface", NULL, NULL);
    g_data_output_stream_put_uint64(datastream, 0, NULL, NULL);
    data_stream_put_string0(datastream, "data", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE_ARRAY,
                                  NULL, NULL);
    g_data_output_stream_put_uint64(datastream, len, NULL, NULL);
    for (guint i = 0; i < len; i++)
        data_stream_put_double(datastream, i, NULL, NULL);

    GwyErrorList *error_list = NULL;
    gwy_error_list_add(&error_list,
                       GWY_DESERIALIZE_ERROR, GWY_DESERIALIZE_ERROR_INVALID,
                       "Data length of ‘GwySurface’ is %lu which is not "
                       "a multiple of 3.",
                       (gulong)len);

    deserialize_assert_failure(G_MEMORY_OUTPUT_STREAM(stream), error_list);
    gwy_error_list_clear(&error_list);
    g_object_unref(datastream);
    g_object_unref(stream);
}

void
test_surface_props(void)
{
    static const GwyXYZ data[] = { { 1, 2, 3 }, { 4, 5, 6 } };
    GwySurface *surface = gwy_surface_new_from_data(data, G_N_ELEMENTS(data));
    guint name_changed = 0;
    g_signal_connect_swapped(surface, "notify::name",
                             G_CALLBACK(record_signal), &name_changed);

    guint n = 0;
    gchar *name;
    g_object_get(surface,
                 "n-points", &n,
                 "name", &name,
                 NULL);
    g_assert_cmpuint(n, ==, surface->n);
    g_assert_cmpuint(n, ==, G_N_ELEMENTS(data));
    g_assert(!name);
    g_assert_cmpuint(name_changed, ==, 0);

    gwy_surface_set_name(surface, "First");
    g_assert_cmpuint(name_changed, ==, 1);
    g_assert_cmpstr(gwy_surface_get_name(surface), ==, "First");

    // Do it twice to excersise no-change behaviour.
    gwy_surface_set_name(surface, "First");
    g_assert_cmpuint(name_changed, ==, 1);
    g_assert_cmpstr(gwy_surface_get_name(surface), ==, "First");

    g_object_set(surface,
                 "name", "Second",
                 NULL);
    g_assert_cmpuint(name_changed, ==, 2);
    g_assert_cmpstr(gwy_surface_get_name(surface), ==, "Second");

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

void
test_surface_regularize_full(void)
{
    enum { max_size = 27 };
    GRand *rng = g_rand_new_with_seed(42);
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
        g_assert(gwy_unit_equal(gwy_surface_get_xyunit(surface),
                                gwy_field_get_xunit(field)));
        g_assert(gwy_unit_equal(gwy_surface_get_xyunit(surface),
                                gwy_field_get_yunit(field)));
        g_assert(gwy_unit_equal(gwy_surface_get_zunit(surface),
                                gwy_field_get_zunit(field)));

        GwyField *newfield = gwy_surface_regularize_full
                                 (surface, GWY_SURFACE_REGULARIZATION_PREVIEW,
                                  0, 0);
        g_assert_cmpuint(newfield->xres*newfield->yres, ==, surface->n);
        g_assert_cmpfloat(fabs(newfield->xreal - field->xreal),
                          <=,
                          2e-14*field->xreal);
        g_assert_cmpfloat(fabs(newfield->yreal - field->yreal),
                          <=,
                          2e-14*field->yreal);
        g_assert_cmpfloat(fabs(newfield->xoff - field->xoff),
                          <=,
                          2e-14*fabs(field->xoff));
        g_assert_cmpfloat(fabs(newfield->yoff - field->yoff),
                          <=,
                          2e-14*fabs(field->yoff));
        g_assert(gwy_unit_equal(gwy_surface_get_xyunit(surface),
                                gwy_field_get_xunit(newfield)));
        g_assert(gwy_unit_equal(gwy_surface_get_xyunit(surface),
                                gwy_field_get_yunit(newfield)));
        g_assert(gwy_unit_equal(gwy_surface_get_zunit(surface),
                                gwy_field_get_zunit(newfield)));
        field_assert_numerically_equal(newfield, field, 1e-14);

        g_object_unref(newfield);
        g_object_unref(surface);

        surface = gwy_surface_new();
        gwy_surface_set_from_field(surface, field);
        g_assert_cmpuint(surface->n, ==, field->xres*field->yres);
        g_assert(gwy_unit_equal(gwy_surface_get_xyunit(surface),
                                gwy_field_get_xunit(field)));
        g_assert(gwy_unit_equal(gwy_surface_get_xyunit(surface),
                                gwy_field_get_yunit(field)));
        g_assert(gwy_unit_equal(gwy_surface_get_zunit(surface),
                                gwy_field_get_zunit(field)));

        newfield = gwy_surface_regularize_full
                                  (surface, GWY_SURFACE_REGULARIZATION_PREVIEW,
                                   0, 0);
        g_assert_cmpuint(newfield->xres*newfield->yres, ==, surface->n);
        g_assert_cmpfloat(fabs(newfield->xreal - field->xreal),
                          <=,
                          2e-14*field->xreal);
        g_assert_cmpfloat(fabs(newfield->yreal - field->yreal),
                          <=,
                          2e-14*field->yreal);
        g_assert_cmpfloat(fabs(newfield->xoff - field->xoff),
                          <=,
                          2e-14*fabs(field->xoff));
        g_assert_cmpfloat(fabs(newfield->yoff - field->yoff),
                          <=,
                          2e-14*fabs(field->yoff));
        g_assert(gwy_unit_equal(gwy_surface_get_xyunit(surface),
                                gwy_field_get_xunit(newfield)));
        g_assert(gwy_unit_equal(gwy_surface_get_xyunit(surface),
                                gwy_field_get_yunit(newfield)));
        g_assert(gwy_unit_equal(gwy_surface_get_zunit(surface),
                                gwy_field_get_zunit(newfield)));
        field_assert_numerically_equal(newfield, field, 1e-14);

        g_object_unref(newfield);
        g_object_unref(surface);
        g_object_unref(field);
    }

    g_rand_free(rng);
}

void
test_surface_regularize_part(void)
{
    enum { max_size = 37 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 50 : 10;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 6, max_size);
        guint yres = g_rand_int_range(rng, 6, max_size);
        guint width = g_rand_int_range(rng, 2, xres+1);
        guint height = g_rand_int_range(rng, 2, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
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
        g_assert(gwy_unit_equal(gwy_surface_get_xyunit(surface),
                                gwy_field_get_xunit(field)));
        g_assert(gwy_unit_equal(gwy_surface_get_xyunit(surface),
                                gwy_field_get_yunit(field)));
        g_assert(gwy_unit_equal(gwy_surface_get_zunit(surface),
                                gwy_field_get_zunit(field)));

        gdouble xfrom = (col + 0.5)*gwy_field_dx(field) + field->xoff,
                xto = xfrom + (width - 1)*gwy_field_dx(field),
                yfrom = (row + 0.5)*gwy_field_dy(field) + field->yoff,
                yto = yfrom + (height - 1)*gwy_field_dy(field);

        GwyFieldPart fpart = { col, row, width, height };
        GwyField *part = gwy_field_new_part(field, &fpart, TRUE);
        GwyField *newfield = gwy_surface_regularize
                                       (surface,
                                        GWY_SURFACE_REGULARIZATION_PREVIEW,
                                        xfrom, xto, yfrom, yto, width, height);
        g_assert_cmpuint(newfield->xres*newfield->yres, ==, width*height);
        g_assert_cmpfloat(fabs(newfield->xreal - part->xreal),
                          <=,
                          2e-14*part->xreal);
        g_assert_cmpfloat(fabs(newfield->yreal - part->yreal),
                          <=,
                          2e-14*part->yreal);
        g_assert_cmpfloat(fabs(newfield->xoff - part->xoff),
                          <=,
                          2e-14*fabs(part->xoff));
        g_assert_cmpfloat(fabs(newfield->yoff - part->yoff),
                          <=,
                          2e-14*fabs(part->yoff));
        g_assert(gwy_unit_equal(gwy_surface_get_xyunit(surface),
                                gwy_field_get_xunit(newfield)));
        g_assert(gwy_unit_equal(gwy_surface_get_xyunit(surface),
                                gwy_field_get_yunit(newfield)));
        g_assert(gwy_unit_equal(gwy_surface_get_zunit(surface),
                                gwy_field_get_zunit(newfield)));
        field_assert_numerically_equal(newfield, part, 1e-14);

        g_object_unref(part);
        g_object_unref(newfield);
        g_object_unref(surface);
        g_object_unref(field);
    }

    g_rand_free(rng);
}

void
test_surface_regularize_interpolation(void)
{
    enum { max_size = 37 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 50 : 10;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 2, max_size);
        guint yres = g_rand_int_range(rng, 2, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        gwy_field_set_xreal(field, xres);
        gwy_field_set_yreal(field, yres);

        GwySurface *surface = gwy_surface_new_from_field(field);
        g_assert_cmpuint(surface->n, ==, field->xres*field->yres);
        g_assert(gwy_unit_equal(gwy_surface_get_xyunit(surface),
                                gwy_field_get_xunit(field)));
        g_assert(gwy_unit_equal(gwy_surface_get_xyunit(surface),
                                gwy_field_get_yunit(field)));
        g_assert(gwy_unit_equal(gwy_surface_get_zunit(surface),
                                gwy_field_get_zunit(field)));

        GwyField *newfield = gwy_surface_regularize_full
                                           (surface,
                                            GWY_SURFACE_REGULARIZATION_PREVIEW,
                                            2*xres - 1, 2*yres - 1);
        g_assert_cmpuint(newfield->xres, ==, 2*xres - 1);
        g_assert_cmpuint(newfield->yres, ==, 2*yres - 1);
        // The new field should be a half-pixel smaller (in real units) because
        // the pixels are half the size now so the area that a single pixel
        // represents is also smaller.
        g_assert_cmpfloat(fabs(newfield->xreal + 0.5 - field->xreal),
                          <=,
                          2e-14*field->xreal);
        g_assert_cmpfloat(fabs(newfield->yreal + 0.5 - field->yreal),
                          <=,
                          2e-14*field->yreal);
        g_assert_cmpfloat(fabs(newfield->xoff - 0.25), <=, 2e-14);
        g_assert_cmpfloat(fabs(newfield->yoff - 0.25), <=, 2e-14);
        g_assert(gwy_unit_equal(gwy_surface_get_xyunit(surface),
                                gwy_field_get_xunit(newfield)));
        g_assert(gwy_unit_equal(gwy_surface_get_xyunit(surface),
                                gwy_field_get_yunit(newfield)));
        g_assert(gwy_unit_equal(gwy_surface_get_zunit(surface),
                                gwy_field_get_zunit(newfield)));

        gdouble *nfd = newfield->data, *fd = field->data;

        // Original values.
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++) {
                gdouble value = nfd[2*i*(2*xres - 1) + 2*j];
                gdouble reference = fd[i*xres + j];
                gwy_assert_floatval(value, reference, 2e-14);
            }
        }

        // Horizontally interpolated values.
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres-1; j++) {
                gdouble value = nfd[2*i*(2*xres - 1) + 2*j + 1];
                gdouble reference = 0.5*(fd[i*xres + j] + fd[i*xres + j+1]);
                gwy_assert_floatval(value, reference, 2e-14);
            }
        }

        // Vertically interpolated values.
        for (guint i = 0; i < yres-1; i++) {
            for (guint j = 0; j < xres; j++) {
                gdouble value = nfd[(2*i + 1)*(2*xres - 1) + 2*j];
                gdouble reference = 0.5*(fd[i*xres + j] + fd[(i+1)*xres + j]);
                gwy_assert_floatval(value, reference, 2e-14);
            }
        }

        // Cross-interpolated values.
        for (guint i = 0; i < yres-1; i++) {
            for (guint j = 0; j < xres-1; j++) {
                gdouble value = nfd[(2*i + 1)*(2*xres - 1) + 2*j + 1];
                gdouble reference = 0.25*(fd[i*xres + j]
                                          + fd[i*xres + j+1]
                                          + fd[(i+1)*xres + j]
                                          + fd[(i+1)*xres + j+1]);
                gwy_assert_floatval(value, reference, 2e-14);
            }
        }

        g_object_unref(newfield);
        g_object_unref(surface);
        g_object_unref(field);
    }

    g_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
