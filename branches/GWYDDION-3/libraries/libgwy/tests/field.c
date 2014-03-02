/*
 *  $Id$
 *  Copyright (C) 2009-2014 David Nečas (Yeti).
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

#include <stdlib.h>
#include <errno.h>
#include "testlibgwy.h"

/***************************************************************************
 *
 * Field
 *
 ***************************************************************************/

void
field_randomize(GwyField *field,
                GRand *rng)
{
    gdouble *d = field->data;
    for (guint n = field->xres*field->yres; n; n--, d++)
        *d = g_rand_double(rng);
    gwy_field_invalidate(field);
}

void
field_assert_equal(const GwyField *result,
                   const GwyField *reference)
{
    g_assert(GWY_IS_FIELD(result));
    g_assert(GWY_IS_FIELD(reference));
    g_assert_cmpuint(result->xres, ==, reference->xres);
    g_assert_cmpuint(result->yres, ==, reference->yres);
    compare_properties(G_OBJECT(result), G_OBJECT(reference));

    for (guint i = 0; i < result->yres; i++) {
        gdouble *result_row = result->data + i*result->xres;
        gdouble *reference_row = reference->data + i*reference->xres;
        for (guint j = 0; j < result->xres; j++)
            g_assert_cmpfloat(result_row[j], ==, reference_row[j]);
    }
}

static void
field_assert_equal_object(GObject *object, GObject *reference)
{
    field_assert_equal(GWY_FIELD(object), GWY_FIELD(reference));
}

void
field_assert_numerically_equal(const GwyField *result,
                               const GwyField *reference,
                               gdouble eps)
{
    g_assert(GWY_IS_FIELD(result));
    g_assert(GWY_IS_FIELD(reference));
    g_assert_cmpuint(result->xres, ==, reference->xres);
    g_assert_cmpuint(result->yres, ==, reference->yres);

    guint xres = result->xres, yres = result->yres;
    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++) {
            gdouble value = result->data[i*xres + j],
                    ref = reference->data[i*xres + j];
            //g_printerr("[%u,%u] %g %g\n", j, i, value, ref);
            gwy_assert_floatval(value, ref, eps);
        }
    }
}

void
field_print_row(const gchar *name, const gdouble *data, gsize size)
{
    g_printerr("%s", name);
    while (size--)
        g_printerr(" %g", *(data++));
    g_printerr("\n");
}

void
field_print(const gchar *name, const GwyField *field)
{
    g_printerr("=====[ %s ]=====\n", name);
    g_printerr("size %u x %u, real %g x %g, offsets %g x %g\n",
               field->xres, field->yres,
               field->xreal, field->yreal,
               field->xoff, field->yoff);
    for (guint i = 0; i < field->yres; i++) {
        g_printerr("[%02u]", i);
        for (guint j = 0; j < field->xres; j++) {
            g_printerr(" % .4f", field->data[i*field->xres + j]);
        }
        g_printerr("\n");
    }
}

void
field_write_gsf(const gchar *name, const GwyField *field)
{
    const gchar header_template[] =
        "Gwyddion Simple Field 1.0\n"
        "XRes = %u\n"
        "YRes = %u\n"
        "XReal = %g\n"
        "YReal = %g\n"
        "XOffset = %g\n"
        "YOffset = %g\n"
        "XYUnits = %s\n"
        "ZUnits = %s\n"
        "Title = %s\n";
    const gchar padding[4] = { 0, 0, 0, 0 };
    guint xres = field->xres, yres = field->yres;

    gchar *filename = g_strdup_printf("%s.gsf", name);
    FILE *fh = fopen(filename, "wb");

    if (!fh) {
        fprintf(stderr, "Cannot open %s: %s\n", filename, strerror(errno));
        g_free(filename);
        return;
    }
    g_free(filename);

    gchar *xunit = gwy_unit_to_string(gwy_field_get_xunit(field),
                                      GWY_VALUE_FORMAT_PLAIN);
    gchar *zunit = gwy_unit_to_string(gwy_field_get_zunit(field),
                                      GWY_VALUE_FORMAT_PLAIN);
    gchar *header = g_strdup_printf(header_template,
                                    xres, yres,
                                    field->xreal, field->yreal,
                                    field->xoff, field->yoff,
                                    xunit, zunit, name);
    g_free(zunit);
    g_free(xunit);

    gsize len = strlen(header);
    gsize npad = 4 - (len % 4);
    gfloat *fltdata = g_new(gfloat, xres*yres);

    for (gsize i = 0; i < xres*yres; i++)
        fltdata[i] = field->data[i];
    fwrite(header, len, 1, fh);
    fwrite(padding, npad, 1, fh);
    fwrite(fltdata, sizeof(gfloat), xres*yres, fh);
    fclose(fh);

    g_free(fltdata);
    g_free(header);
}

void
test_field_props(void)
{
    GwyField *field = gwy_field_new_sized(41, 37, FALSE);
    guint xres_changed = 0, yres_changed = 0,
          xreal_changed = 0, yreal_changed = 0,
          xoff_changed = 0, yoff_changed = 0,
          name_changed = 0;
    g_signal_connect_swapped(field, "notify::x-res",
                             G_CALLBACK(record_signal), &xres_changed);
    g_signal_connect_swapped(field, "notify::y-res",
                             G_CALLBACK(record_signal), &yres_changed);
    g_signal_connect_swapped(field, "notify::x-real",
                             G_CALLBACK(record_signal), &xreal_changed);
    g_signal_connect_swapped(field, "notify::y-real",
                             G_CALLBACK(record_signal), &yreal_changed);
    g_signal_connect_swapped(field, "notify::x-offset",
                             G_CALLBACK(record_signal), &xoff_changed);
    g_signal_connect_swapped(field, "notify::y-offset",
                             G_CALLBACK(record_signal), &yoff_changed);
    g_signal_connect_swapped(field, "notify::name",
                             G_CALLBACK(record_signal), &name_changed);

    guint xres, yres;
    gdouble xreal, yreal, xoff, yoff;
    gchar *name;
    g_object_get(field,
                 "x-res", &xres,
                 "y-res", &yres,
                 "x-real", &xreal,
                 "y-real", &yreal,
                 "x-offset", &xoff,
                 "y-offset", &yoff,
                 "name", &name,
                 NULL);
    g_assert_cmpuint(xres, ==, field->xres);
    g_assert_cmpuint(yres, ==, field->yres);
    g_assert_cmpfloat(xreal, ==, field->xreal);
    g_assert_cmpfloat(yreal, ==, field->yreal);
    g_assert_cmpfloat(xoff, ==, field->xoff);
    g_assert_cmpfloat(yoff, ==, field->yoff);
    g_assert(!name);
    g_assert_cmpuint(xres_changed, ==, 0);
    g_assert_cmpuint(yres_changed, ==, 0);
    g_assert_cmpuint(xreal_changed, ==, 0);
    g_assert_cmpuint(yreal_changed, ==, 0);
    g_assert_cmpuint(xoff_changed, ==, 0);
    g_assert_cmpuint(yoff_changed, ==, 0);
    g_assert_cmpuint(name_changed, ==, 0);

    xreal = 5.0;
    yreal = 7.0e-14;
    xoff = -3;
    yoff = 1e-15;
    gwy_field_set_xreal(field, xreal);
    g_assert_cmpuint(xreal_changed, ==, 1);
    gwy_field_set_yreal(field, yreal);
    g_assert_cmpuint(yreal_changed, ==, 1);
    gwy_field_set_xoffset(field, xoff);
    g_assert_cmpuint(xoff_changed, ==, 1);
    gwy_field_set_yoffset(field, yoff);
    g_assert_cmpuint(yoff_changed, ==, 1);
    gwy_field_set_name(field, "First");
    g_assert_cmpuint(name_changed, ==, 1);
    g_assert_cmpuint(xres_changed, ==, 0);
    g_assert_cmpuint(yres_changed, ==, 0);
    g_assert_cmpfloat(xreal, ==, field->xreal);
    g_assert_cmpfloat(yreal, ==, field->yreal);
    g_assert_cmpfloat(xoff, ==, field->xoff);
    g_assert_cmpfloat(yoff, ==, field->yoff);
    g_assert_cmpstr(gwy_field_get_name(field), ==, "First");

    // Do it twice to excersise no-change behaviour.
    gwy_field_set_xreal(field, xreal);
    gwy_field_set_yreal(field, yreal);
    gwy_field_set_xoffset(field, xoff);
    gwy_field_set_yoffset(field, yoff);
    gwy_field_set_name(field, "First");
    g_assert_cmpuint(xres_changed, ==, 0);
    g_assert_cmpuint(yres_changed, ==, 0);
    g_assert_cmpuint(xreal_changed, ==, 1);
    g_assert_cmpuint(yreal_changed, ==, 1);
    g_assert_cmpuint(xoff_changed, ==, 1);
    g_assert_cmpuint(yoff_changed, ==, 1);
    g_assert_cmpuint(name_changed, ==, 1);
    g_assert_cmpfloat(xreal, ==, field->xreal);
    g_assert_cmpfloat(yreal, ==, field->yreal);
    g_assert_cmpfloat(xoff, ==, field->xoff);
    g_assert_cmpfloat(yoff, ==, field->yoff);
    g_assert_cmpstr(gwy_field_get_name(field), ==, "First");

    xreal = 0.1;
    yreal = 6.0;
    xoff = 1e20;
    yoff = 2e17;
    g_object_set(field,
                 "x-real", xreal,
                 "y-real", yreal,
                 "x-offset", xoff,
                 "y-offset", yoff,
                 "name", "Second",
                 NULL);
    g_assert_cmpuint(xres_changed, ==, 0);
    g_assert_cmpuint(yres_changed, ==, 0);
    g_assert_cmpuint(xreal_changed, ==, 2);
    g_assert_cmpuint(yreal_changed, ==, 2);
    g_assert_cmpuint(xoff_changed, ==, 2);
    g_assert_cmpuint(yoff_changed, ==, 2);
    g_assert_cmpuint(name_changed, ==, 2);
    g_assert_cmpfloat(xreal, ==, field->xreal);
    g_assert_cmpfloat(yreal, ==, field->yreal);
    g_assert_cmpfloat(xoff, ==, field->xoff);
    g_assert_cmpfloat(yoff, ==, field->yoff);
    g_assert_cmpstr(gwy_field_get_name(field), ==, "Second");

    g_object_unref(field);
}

void
test_field_data_changed(void)
{
    // Plain emission
    GwyField *field = gwy_field_new();
    guint counter = 0;
    g_signal_connect_swapped(field, "data-changed",
                             G_CALLBACK(record_signal), &counter);
    gwy_field_data_changed(field, NULL);
    g_assert_cmpuint(counter, ==, 1);
    g_object_unref(field);

    // Specified part argument
    field = gwy_field_new_sized(8, 8, FALSE);
    GwyFieldPart fpart = { 1, 2, 3, 4 };
    g_signal_connect_swapped(field, "data-changed",
                             G_CALLBACK(field_part_assert_equal), &fpart);
    gwy_field_data_changed(field, &fpart);
    g_object_unref(field);

    // NULL part argument
    field = gwy_field_new_sized(2, 3, FALSE);
    g_signal_connect_swapped(field, "data-changed",
                             G_CALLBACK(field_part_assert_equal), NULL);
    gwy_field_data_changed(field, NULL);
    g_object_unref(field);
}

void
test_field_units(void)
{
    enum { max_size = 411 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        guint height = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(width, height, FALSE);
        g_object_set(field,
                     "x-real", -log(1.0 - g_rand_double(rng)),
                     "y-real", -log(1.0 - g_rand_double(rng)),
                     NULL);
        GString *prev = g_string_new(NULL), *next = g_string_new(NULL);
        GwyValueFormat *format = gwy_field_format_xy(field,
                                                     GWY_VALUE_FORMAT_PLAIN);
        gdouble dx = gwy_field_dx(field);
        for (gint i = 0; i <= 10; i++) {
            GWY_SWAP(GString*, prev, next);
            g_string_assign(next,
                            gwy_value_format_print_number(format, (i - 5)*dx));
            if (i)
                g_assert_cmpstr(prev->str, !=, next->str);
        }
        gdouble dy = gwy_field_dy(field);
        for (gint i = 0; i <= 10; i++) {
            GWY_SWAP(GString*, prev, next);
            g_string_assign(next,
                            gwy_value_format_print_number(format, (i - 5)*dy));
            if (i)
                g_assert_cmpstr(prev->str, !=, next->str);
        }
        g_object_unref(format);
        g_object_unref(field);
        g_string_free(prev, TRUE);
        g_string_free(next, TRUE);
    }
    g_rand_free(rng);
}

void
test_field_units_assign(void)
{
    GwyField *field = gwy_field_new(), *field2 = gwy_field_new();
    GwyUnit *xunit = gwy_field_get_xunit(field);
    GwyUnit *yunit = gwy_field_get_yunit(field);
    GwyUnit *zunit = gwy_field_get_zunit(field);
    guint count_x = 0;
    guint count_y = 0;
    guint count_z = 0;

    g_signal_connect_swapped(xunit, "changed",
                             G_CALLBACK(record_signal), &count_x);
    g_signal_connect_swapped(yunit, "changed",
                             G_CALLBACK(record_signal), &count_y);
    g_signal_connect_swapped(zunit, "changed",
                             G_CALLBACK(record_signal), &count_z);

    gwy_unit_set_from_string(gwy_field_get_xunit(field), "m", NULL);
    g_assert_cmpuint(count_x, ==, 1);
    g_assert_cmpuint(count_y, ==, 0);
    g_assert_cmpuint(count_z, ==, 0);

    gwy_unit_set_from_string(gwy_field_get_yunit(field), "m", NULL);
    g_assert_cmpuint(count_x, ==, 1);
    g_assert_cmpuint(count_y, ==, 1);
    g_assert_cmpuint(count_z, ==, 0);

    gwy_unit_set_from_string(gwy_field_get_zunit(field), "s", NULL);
    g_assert_cmpuint(count_x, ==, 1);
    g_assert_cmpuint(count_y, ==, 1);
    g_assert_cmpuint(count_z, ==, 1);

    gwy_field_assign(field, field2);
    g_assert_cmpuint(count_x, ==, 2);
    g_assert_cmpuint(count_y, ==, 2);
    g_assert_cmpuint(count_z, ==, 2);
    g_assert(gwy_field_get_xunit(field) == xunit);
    g_assert(gwy_field_get_yunit(field) == yunit);
    g_assert(gwy_field_get_zunit(field) == zunit);

    // Try again to see if the signal counts change.
    gwy_field_assign(field, field2);
    g_assert_cmpuint(count_x, ==, 2);
    g_assert_cmpuint(count_y, ==, 2);
    g_assert_cmpuint(count_z, ==, 2);
    g_assert(gwy_field_get_xunit(field) == xunit);
    g_assert(gwy_field_get_yunit(field) == yunit);
    g_assert(gwy_field_get_zunit(field) == zunit);

    g_object_unref(field2);
    g_object_unref(field);
}

void
test_field_serialize(void)
{
    enum { max_size = 55 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = g_test_slow() ? 50 : 20;

    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        guint height = g_rand_int_range(rng, 1, max_size/4);
        GwyField *original = gwy_field_new_sized(width, height, FALSE);
        field_randomize(original, rng);
        GwyField *copy;

        if (g_rand_int(rng) % 5)
            unit_randomize(gwy_field_get_xunit(original), rng);
        if (g_rand_int(rng) % 5)
            unit_randomize(gwy_field_get_yunit(original), rng);
        if (g_rand_int(rng) % 5)
            unit_randomize(gwy_field_get_zunit(original), rng);

        if (g_rand_int(rng) % 3 == 0)
            gwy_field_set_xoffset(original, g_rand_double(rng));
        if (g_rand_int(rng) % 3 == 0)
            gwy_field_set_yoffset(original, g_rand_double(rng));

        serializable_duplicate(GWY_SERIALIZABLE(original),
                               field_assert_equal_object);
        serializable_assign(GWY_SERIALIZABLE(original),
                            field_assert_equal_object);
        copy = GWY_FIELD(serialize_and_back(G_OBJECT(original),
                                            field_assert_equal_object));
        g_object_unref(copy);

        g_object_unref(original);
    }
    g_rand_free(rng);
}

void
test_field_serialize_failure_xres0(void)
{
    GOutputStream *stream = g_memory_output_stream_new(NULL, 0,
                                                       g_realloc, g_free);
    GDataOutputStream *datastream = g_data_output_stream_new(stream);
    g_data_output_stream_set_byte_order(datastream,
                                        G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

    data_stream_put_string0(datastream, "GwyField", NULL, NULL);
    g_data_output_stream_put_uint64(datastream, 0, NULL, NULL);
    data_stream_put_string0(datastream, "xres", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32,
                                  NULL, NULL);
    g_data_output_stream_put_uint32(datastream, 0, NULL, NULL);
    data_stream_put_string0(datastream, "yres", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32,
                                  NULL, NULL);
    g_data_output_stream_put_uint32(datastream, 2, NULL, NULL);
    data_stream_put_string0(datastream, "xreal", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE,
                                  NULL, NULL);
    data_stream_put_double(datastream, 1.0, NULL, NULL);
    data_stream_put_string0(datastream, "yreal", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE,
                                  NULL, NULL);
    data_stream_put_double(datastream, 1.0, NULL, NULL);

    GwyErrorList *error_list = NULL;
    gwy_error_list_add(&error_list,
                       GWY_DESERIALIZE_ERROR, GWY_DESERIALIZE_ERROR_INVALID,
                       "Dimension %u×%u of ‘GwyField’ is invalid.", 0, 2);

    deserialize_assert_failure(G_MEMORY_OUTPUT_STREAM(stream), error_list);
    gwy_error_list_clear(&error_list);
    g_object_unref(datastream);
    g_object_unref(stream);
}

void
test_field_serialize_failure_yres0(void)
{
    GOutputStream *stream = g_memory_output_stream_new(NULL, 0,
                                                       g_realloc, g_free);
    GDataOutputStream *datastream = g_data_output_stream_new(stream);
    g_data_output_stream_set_byte_order(datastream,
                                        G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

    data_stream_put_string0(datastream, "GwyField", NULL, NULL);
    g_data_output_stream_put_uint64(datastream, 0, NULL, NULL);
    data_stream_put_string0(datastream, "xres", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32,
                                  NULL, NULL);
    g_data_output_stream_put_uint32(datastream, 3, NULL, NULL);
    data_stream_put_string0(datastream, "yres", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32,
                                  NULL, NULL);
    g_data_output_stream_put_uint32(datastream, 0, NULL, NULL);
    data_stream_put_string0(datastream, "xreal", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE,
                                  NULL, NULL);
    data_stream_put_double(datastream, 1.0, NULL, NULL);
    data_stream_put_string0(datastream, "yreal", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE,
                                  NULL, NULL);
    data_stream_put_double(datastream, 1.0, NULL, NULL);

    GwyErrorList *error_list = NULL;
    gwy_error_list_add(&error_list,
                       GWY_DESERIALIZE_ERROR, GWY_DESERIALIZE_ERROR_INVALID,
                       "Dimension %u×%u of ‘GwyField’ is invalid.", 3, 0);

    deserialize_assert_failure(G_MEMORY_OUTPUT_STREAM(stream), error_list);
    gwy_error_list_clear(&error_list);
    g_object_unref(datastream);
    g_object_unref(stream);
}

void
test_field_serialize_failure_size(void)
{
    GOutputStream *stream = g_memory_output_stream_new(NULL, 0,
                                                       g_realloc, g_free);
    GDataOutputStream *datastream = g_data_output_stream_new(stream);
    g_data_output_stream_set_byte_order(datastream,
                                        G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

    data_stream_put_string0(datastream, "GwyField", NULL, NULL);
    g_data_output_stream_put_uint64(datastream, 0, NULL, NULL);
    data_stream_put_string0(datastream, "xres", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32,
                                  NULL, NULL);
    g_data_output_stream_put_uint32(datastream, 3, NULL, NULL);
    data_stream_put_string0(datastream, "yres", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32,
                                  NULL, NULL);
    g_data_output_stream_put_uint32(datastream, 2, NULL, NULL);
    data_stream_put_string0(datastream, "xreal", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE,
                                  NULL, NULL);
    data_stream_put_double(datastream, 1.0, NULL, NULL);
    data_stream_put_string0(datastream, "yreal", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE,
                                  NULL, NULL);
    data_stream_put_double(datastream, 1.0, NULL, NULL);
    guint len = 5;
    data_stream_put_string0(datastream, "data", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE_ARRAY,
                                  NULL, NULL);
    g_data_output_stream_put_uint64(datastream, len, NULL, NULL);
    for (guint i = 0; i < len; i++)
        data_stream_put_double(datastream, i, NULL, NULL);

    GwyErrorList *error_list = NULL;
    gwy_error_list_add(&error_list,
                       GWY_DESERIALIZE_ERROR, GWY_DESERIALIZE_ERROR_INVALID,
                       "GwyField dimensions %u×%u do not match data size %lu.",
                       3, 2, (gulong)len);

    deserialize_assert_failure(G_MEMORY_OUTPUT_STREAM(stream), error_list);
    gwy_error_list_clear(&error_list);
    g_object_unref(datastream);
    g_object_unref(stream);
}

void
test_field_set_size(void)
{
    GwyField *field = gwy_field_new_sized(13, 11, TRUE);
    guint xres_changed = 0, yres_changed = 0;

    g_signal_connect_swapped(field, "notify::x-res",
                             G_CALLBACK(record_signal), &xres_changed);
    g_signal_connect_swapped(field, "notify::y-res",
                             G_CALLBACK(record_signal), &yres_changed);

    gwy_field_set_size(field, 13, 11, TRUE);
    g_assert_cmpuint(field->xres, ==, 13);
    g_assert_cmpuint(field->yres, ==, 11);
    g_assert_cmpuint(xres_changed, ==, 0);
    g_assert_cmpuint(yres_changed, ==, 0);

    gwy_field_set_size(field, 13, 10, TRUE);
    g_assert_cmpuint(field->xres, ==, 13);
    g_assert_cmpuint(field->yres, ==, 10);
    g_assert_cmpuint(xres_changed, ==, 0);
    g_assert_cmpuint(yres_changed, ==, 1);

    gwy_field_set_size(field, 11, 10, TRUE);
    g_assert_cmpuint(field->xres, ==, 11);
    g_assert_cmpuint(field->yres, ==, 10);
    g_assert_cmpuint(xres_changed, ==, 1);
    g_assert_cmpuint(yres_changed, ==, 1);

    gwy_field_set_size(field, 15, 14, TRUE);
    g_assert_cmpuint(field->xres, ==, 15);
    g_assert_cmpuint(field->yres, ==, 14);
    g_assert_cmpuint(xres_changed, ==, 2);
    g_assert_cmpuint(yres_changed, ==, 2);

    g_object_unref(field);
}

static void
field_check_part_good(guint xres, guint yres,
                      const GwyFieldPart *fpart,
                      guint expected_col, guint expected_row,
                      guint expected_width, guint expected_height)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    guint col, row, width, height;

    g_assert(gwy_field_check_part(field, fpart, &col, &row, &width, &height));
    g_assert_cmpuint(col, ==, expected_col);
    g_assert_cmpuint(row, ==, expected_row);
    g_assert_cmpuint(width, ==, expected_width);
    g_assert_cmpuint(height, ==, expected_height);
    g_object_unref(field);
}

void
test_field_check_part_good(void)
{
    field_check_part_good(17, 25, &(GwyFieldPart){ 0, 0, 17, 25 },
                          0, 0, 17, 25);
    field_check_part_good(17, 25, NULL,
                          0, 0, 17, 25);
    field_check_part_good(17, 25, &(GwyFieldPart){ 0, 0, 3, 24 },
                          0, 0, 3, 24);
    field_check_part_good(17, 25, &(GwyFieldPart){ 16, 20, 1, 4 },
                          16, 20, 1, 4);
}

static void
field_check_part_empty(guint xres, guint yres,
                       const GwyFieldPart *fpart)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    guint col, row, width, height;

    g_assert(!gwy_field_check_part(field, fpart, &col, &row, &width, &height));
    g_object_unref(field);
}

void
test_field_check_part_empty(void)
{
    field_check_part_empty(17, 25, &(GwyFieldPart){ 0, 0, 0, 0 });
    field_check_part_empty(17, 25, &(GwyFieldPart){ 17, 25, 0, 0 });
    field_check_part_empty(17, 25, &(GwyFieldPart){ 1000, 1000, 0, 0 });
}

static void
test_field_check_part_bad_subprocess_one(guint xres, guint yres,
                                         const GwyFieldPart *fpart)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    guint col, row, width, height;
    gwy_field_check_part(field, fpart, &col, &row, &width, &height);
}

void
test_field_check_part_bad_subprocess_00181(void)
{
    test_field_check_part_bad_subprocess_one(17, 25,
                                             &(GwyFieldPart){ 0, 0, 18, 1 });
}

void
test_field_check_part_bad_subprocess_00126(void)
{
    test_field_check_part_bad_subprocess_one(17, 25,
                                             &(GwyFieldPart){ 0, 0, 1, 26 });
}

void
test_field_check_part_bad_subprocess_17011(void)
{
    test_field_check_part_bad_subprocess_one(17, 25,
                                             &(GwyFieldPart){ 17, 0, 1, 1 });
}

void
test_field_check_part_bad_subprocess_02511(void)
{
    test_field_check_part_bad_subprocess_one(17, 25,
                                             &(GwyFieldPart){ 0, 25, 1, 1 });
}

void
test_field_check_part_bad(void)
{
    assert_subprocesses_critical_fail("/testlibgwy/field/check-part/bad",
                                      0, 0,
                                      "/00181",
                                      "/00126",
                                      "/17011",
                                      "/02511",
                                      NULL);
}

static void
field_check_target_part_good(guint xres, guint yres,
                             const GwyFieldPart *fpart,
                             guint width_full, guint height_full,
                             guint expected_col, guint expected_row,
                             guint expected_width, guint expected_height)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    guint col, row, width, height;

    g_assert(gwy_field_check_target_part(field, fpart,
                                         width_full, height_full,
                                         &col, &row, &width, &height));
    g_assert_cmpuint(col, ==, expected_col);
    g_assert_cmpuint(row, ==, expected_row);
    g_assert_cmpuint(width, ==, expected_width);
    g_assert_cmpuint(height, ==, expected_height);
    g_object_unref(field);
}

void
test_field_check_target_part_good(void)
{
    field_check_target_part_good(17, 25, &(GwyFieldPart){ 0, 0, 17, 25 },
                                 1234, 5678,
                                 0, 0, 17, 25);
    field_check_target_part_good(17, 25, NULL,
                                 17, 25,
                                 0, 0, 17, 25);
    field_check_target_part_good(17, 25, &(GwyFieldPart){ 0, 0, 3, 24 },
                                 17, 25,
                                 0, 0, 3, 24);
    field_check_target_part_good(17, 25, &(GwyFieldPart){ 0, 0, 3, 24 },
                                 1234, 5678,
                                 0, 0, 3, 24);
    field_check_target_part_good(17, 25, &(GwyFieldPart){ 16, 20, 1, 4 },
                                 17, 25,
                                 16, 20, 1, 4);
    field_check_target_part_good(17, 25, &(GwyFieldPart){ 16, 20, 1, 4 },
                                 1234, 5678,
                                 16, 20, 1, 4);
    field_check_target_part_good(17, 25, &(GwyFieldPart){ 123, 456, 17, 25 },
                                 1234, 5678,
                                 0, 0, 17, 25);
}

static void
field_check_target_part_empty(guint xres, guint yres,
                              const GwyFieldPart *fpart,
                              guint width_full, guint height_full)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    guint col, row, width, height;

    g_assert(!gwy_field_check_target_part(field, fpart,
                                          width_full, height_full,
                                          &col, &row, &width, &height));
    g_object_unref(field);
}

void
test_field_check_target_part_empty(void)
{
    field_check_target_part_empty(17, 25,
                                  &(GwyFieldPart){ 0, 0, 0, 0 },
                                  123, 456);
    field_check_target_part_empty(17, 25,
                                  &(GwyFieldPart){ 17, 25, 0, 0 },
                                  123, 456);
    field_check_target_part_empty(17, 25,
                                  &(GwyFieldPart){ 1000, 1000, 0, 0 },
                                  123, 456);
    field_check_target_part_empty(17, 25,
                                  &(GwyFieldPart){ 1000, 1000, 0, 0 },
                                  0, 0);
}

static void
field_check_target_part_bad_subprocess_one(guint xres, guint yres,
                                           const GwyFieldPart *fpart,
                                           guint width_full, guint height_full)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    guint col, row, width, height;
    gwy_field_check_target_part(field, fpart, width_full, height_full,
                                &col, &row, &width, &height);
}

void
test_field_check_target_part_bad_subprocess_00181(void)
{
    field_check_target_part_bad_subprocess_one(17, 25,
                                               &(GwyFieldPart){ 0, 0, 18, 1 },
                                               123, 456);
}

void
test_field_check_target_part_bad_subprocess_00126(void)
{
    field_check_target_part_bad_subprocess_one(17, 25,
                                               &(GwyFieldPart){ 0, 0, 1, 26 },
                                               123, 456);
}

void
test_field_check_target_part_bad_subprocess_17011(void)
{
    field_check_target_part_bad_subprocess_one(17, 25,
                                               &(GwyFieldPart){ 17, 0, 1, 1 },
                                               123, 456);
}

void
test_field_check_target_part_bad_subprocess_02511(void)
{
    field_check_target_part_bad_subprocess_one(17, 25,
                                               &(GwyFieldPart){ 0, 25, 1, 1 },
                                               123, 456);
}

void
test_field_check_target_part_bad_subprocess_NULLx(void)
{
    field_check_target_part_bad_subprocess_one(17, 25, NULL, 123, 25);
}

void
test_field_check_target_part_bad_subprocess_NULLy(void)
{
    field_check_target_part_bad_subprocess_one(17, 25, NULL, 17, 456);
}

void
test_field_check_target_part_bad(void)
{
    assert_subprocesses_critical_fail("/testlibgwy/field/check-target-part/bad",
                                      0, 0,
                                      "/00181",
                                      "/00126",
                                      "/17011",
                                      "/02511",
                                      "/NULLx",
                                      "/NULLy",
                                      NULL);
}

static void
field_check_target_good(guint xres, guint yres,
                        guint txres, guint tyres,
                        const GwyFieldPart *fpart,
                        guint expected_col, guint expected_row)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    GwyField *target = gwy_field_new_sized(txres, tyres, FALSE);
    guint col, row;

    g_assert(gwy_field_check_target(field, target, fpart, &col, &row));
    g_assert_cmpuint(col, ==, expected_col);
    g_assert_cmpuint(row, ==, expected_row);
    g_object_unref(target);
    g_object_unref(field);
}

void
test_field_check_target_good(void)
{
    field_check_target_good(17, 25, 17, 25, NULL, 0, 0);
    field_check_target_good(17, 25, 17, 25,
                            &(GwyFieldPart){ 0, 0, 17, 25 }, 0, 0);
    field_check_target_good(17, 25, 4, 3,
                            &(GwyFieldPart){ 0, 0, 4, 3 }, 0, 0);
    field_check_target_good(17, 25, 4, 3,
                            &(GwyFieldPart){ 13, 22, 4, 3 }, 0, 0);
    field_check_target_good(17, 25, 17, 25,
                            &(GwyFieldPart){ 13, 22, 4, 3 }, 13, 22);
}

static void
field_check_target_empty(guint xres, guint yres,
                         guint txres, guint tyres,
                         const GwyFieldPart *fpart)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    GwyField *target = gwy_field_new_sized(txres, tyres, FALSE);
    guint col, row;

    g_assert(!gwy_field_check_target(field, target, fpart, &col, &row));
    g_object_unref(target);
    g_object_unref(field);
}

void
test_field_check_target_empty(void)
{
    field_check_target_empty(17, 25, 17, 25, &(GwyFieldPart){ 0, 0, 0, 0 });
    field_check_target_empty(17, 25, 17, 25, &(GwyFieldPart){ 17, 25, 0, 0 });
    field_check_target_empty(17, 25, 17, 25, &(GwyFieldPart){ 100, 100, 0, 0 });
    field_check_target_empty(17, 25, 34, 81, &(GwyFieldPart){ 100, 100, 0, 0 });
}

static void
field_check_target_bad_subprocess_one(guint xres, guint yres,
                                      guint txres, guint tyres,
                                      const GwyFieldPart *fpart)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    GwyField *target = gwy_field_new_sized(txres, tyres, FALSE);
    guint col, row;
    gwy_field_check_target(field, target, fpart, &col, &row);
}

void
test_field_check_target_bad_subprocess_17251625(void)
{
    field_check_target_bad_subprocess_one(17, 25, 16, 25,
                                          &(GwyFieldPart){ 0, 0, 17, 25 });
}

void
test_field_check_target_bad_subprocess_17251726(void)
{
    field_check_target_bad_subprocess_one(17, 25, 17, 26,
                                          &(GwyFieldPart){ 0, 0, 17, 25 });
}

void
test_field_check_target_bad_subprocess_172577(void)
{
    field_check_target_bad_subprocess_one(17, 25, 7, 7,
                                          &(GwyFieldPart){ 3, 4, 7, 8 });
}

void
test_field_check_target_bad_subprocess_172588(void)
{
    field_check_target_bad_subprocess_one(17, 25, 8, 8,
                                          &(GwyFieldPart){ 3, 4, 7, 8 });
}

void
test_field_check_target_bad(void)
{
    assert_subprocesses_critical_fail("/testlibgwy/field/check-target/bad",
                                      0, 0,
                                      "/17251625",
                                      "/17251726",
                                      "/172577",
                                      "/172588",
                                      NULL);
}

static void
field_check_mask_good(guint xres, guint yres,
                      guint mxres, guint myres,
                      const GwyFieldPart *fpart,
                      GwyMasking masking,
                      guint expected_col, guint expected_row,
                      guint expected_width, guint expected_height,
                      guint expected_maskcol, guint expected_maskrow,
                      GwyMasking expected_masking)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    GwyMaskField *mask = ((mxres && myres)
                          ? gwy_mask_field_new_sized(mxres, myres, FALSE)
                          : NULL);
    guint col, row, width, height, maskcol, maskrow;

    g_assert(gwy_field_check_mask(field, fpart, mask, &masking,
                                  &col, &row, &width, &height,
                                  &maskcol, &maskrow));
    g_assert_cmpuint(masking, ==, expected_masking);
    g_assert_cmpuint(col, ==, expected_col);
    g_assert_cmpuint(row, ==, expected_row);
    g_assert_cmpuint(width, ==, expected_width);
    g_assert_cmpuint(height, ==, expected_height);
    g_assert_cmpuint(maskcol, ==, expected_maskcol);
    g_assert_cmpuint(maskrow, ==, expected_maskrow);
    GWY_OBJECT_UNREF(mask);
    g_object_unref(field);
}

void
test_field_check_mask_good(void)
{
    // Full field, ignoring the mask.
    field_check_mask_good(17, 25, 0, 0, NULL, GWY_MASK_IGNORE,
                          0, 0, 17, 25, 0, 0, GWY_MASK_IGNORE);
    field_check_mask_good(17, 25, 0, 0, NULL, GWY_MASK_INCLUDE,
                          0, 0, 17, 25, 0, 0, GWY_MASK_IGNORE);
    field_check_mask_good(17, 25, 0, 0, NULL, GWY_MASK_EXCLUDE,
                          0, 0, 17, 25, 0, 0, GWY_MASK_IGNORE);
    field_check_mask_good(17, 25, 17, 25, NULL, GWY_MASK_IGNORE,
                          0, 0, 17, 25, 0, 0, GWY_MASK_IGNORE);

    // Full field, full mask.
    field_check_mask_good(17, 25, 17, 25, NULL, GWY_MASK_INCLUDE,
                          0, 0, 17, 25, 0, 0, GWY_MASK_INCLUDE);
    field_check_mask_good(17, 25, 17, 25, NULL, GWY_MASK_EXCLUDE,
                          0, 0, 17, 25, 0, 0, GWY_MASK_EXCLUDE);

    // Partial field, partial mask.
    field_check_mask_good(17, 25, 17, 25, &(GwyFieldPart){ 1, 2, 14, 19 },
                          GWY_MASK_INCLUDE,
                          1, 2, 14, 19, 1, 2, GWY_MASK_INCLUDE);
    field_check_mask_good(17, 25, 17, 25, &(GwyFieldPart){ 1, 2, 14, 19 },
                          GWY_MASK_EXCLUDE,
                          1, 2, 14, 19, 1, 2, GWY_MASK_EXCLUDE);
    field_check_mask_good(17, 25, 17, 25, &(GwyFieldPart){ 1, 2, 14, 19 },
                          GWY_MASK_IGNORE,
                          1, 2, 14, 19, 0, 0, GWY_MASK_IGNORE);

    // Partial field, full mask.
    field_check_mask_good(17, 25, 14, 19, &(GwyFieldPart){ 1, 2, 14, 19 },
                          GWY_MASK_INCLUDE,
                          1, 2, 14, 19, 0, 0, GWY_MASK_INCLUDE);
    field_check_mask_good(17, 25, 14, 19, &(GwyFieldPart){ 1, 2, 14, 19 },
                          GWY_MASK_EXCLUDE,
                          1, 2, 14, 19, 0, 0, GWY_MASK_EXCLUDE);
    field_check_mask_good(17, 25, 14, 19, &(GwyFieldPart){ 1, 2, 14, 19 },
                          GWY_MASK_IGNORE,
                          1, 2, 14, 19, 0, 0, GWY_MASK_IGNORE);
}

static void
field_check_mask_empty(guint xres, guint yres,
                       guint mxres, guint myres,
                       const GwyFieldPart *fpart,
                       GwyMasking masking)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    GwyMaskField *mask = ((mxres && myres)
                          ? gwy_mask_field_new_sized(mxres, myres, FALSE)
                          : NULL);

    guint col, row, width, height, maskcol, maskrow;
    g_assert(!gwy_field_check_mask(field, fpart, mask, &masking,
                                   &col, &row, &width, &height,
                                   &maskcol, &maskrow));
    GWY_OBJECT_UNREF(mask);
    g_object_unref(field);
}

void
test_field_check_mask_empty(void)
{
    field_check_mask_empty(17, 25, 0, 0, &(GwyFieldPart){ 0, 0, 0, 0 },
                           GWY_MASK_INCLUDE);
    field_check_mask_empty(17, 25, 0, 0, &(GwyFieldPart){ 17, 25, 0, 0 },
                           GWY_MASK_INCLUDE);
    field_check_mask_empty(17, 25, 0, 0, &(GwyFieldPart){ 1000, 1000, 0, 0 },
                           GWY_MASK_INCLUDE);
    field_check_mask_empty(17, 25, 17, 25, &(GwyFieldPart){ 0, 0, 0, 0 },
                           GWY_MASK_INCLUDE);
    field_check_mask_empty(17, 25, 17, 25, &(GwyFieldPart){ 17, 25, 0, 0 },
                           GWY_MASK_INCLUDE);
    field_check_mask_empty(17, 25, 17, 25, &(GwyFieldPart){ 1000, 1000, 0, 0 },
                           GWY_MASK_INCLUDE);
}

static void
field_check_mask_bad_subprocess_one(guint xres, guint yres,
                     guint mxres, guint myres,
                     const GwyFieldPart *fpart,
                     GwyMasking masking)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    GwyMaskField *mask = ((mxres && myres)
                          ? gwy_mask_field_new_sized(mxres, myres, FALSE)
                          : NULL);
    guint col, row, width, height, maskcol, maskrow;
    gwy_field_check_mask(field, fpart, mask, &masking,
                         &col, &row, &width, &height,
                         &maskcol, &maskrow);
}

void
test_field_check_mask_bad_subprocess_172500231619(void)
{
    field_check_mask_bad_subprocess_one(17, 25, 0, 0,
                                        &(GwyFieldPart){ 2, 3, 16, 19 },
                                        GWY_MASK_INCLUDE);
}

void
test_field_check_mask_bad_subprocess_172500231123(void)
{
    field_check_mask_bad_subprocess_one(17, 25, 0, 0,
                                        &(GwyFieldPart){ 2, 3, 11, 23 },
                                        GWY_MASK_INCLUDE);
}

void
test_field_check_mask_bad_subprocess_17251726(void)
{
    field_check_mask_bad_subprocess_one(17, 25, 17, 26,
                                        NULL, GWY_MASK_INCLUDE);
}

void
test_field_check_mask_bad_subprocess_17251525(void)
{
    field_check_mask_bad_subprocess_one(17, 25, 15, 25,
                                        NULL, GWY_MASK_INCLUDE);
}

void
test_field_check_mask_bad_subprocess_1725772378(void)
{
    field_check_mask_bad_subprocess_one(17, 25, 7, 7,
                                        &(GwyFieldPart){ 2, 3, 7, 8 },
                                        GWY_MASK_INCLUDE);
}

void
test_field_check_mask_bad_subprocess_1725882378(void)
{
    field_check_mask_bad_subprocess_one(17, 25, 8, 8,
                                        &(GwyFieldPart){ 2, 3, 7, 8 },
                                        GWY_MASK_INCLUDE);
}

void
test_field_check_mask_bad(void)
{
    assert_subprocesses_critical_fail("/testlibgwy/field/check-mask/bad",
                                      0, 0,
                                      "/172500231619",
                                      "/172500231123",
                                      "/17251726",
                                      "/17251525",
                                      "/1725772378",
                                      "/1725882378",
                                      NULL);
}

static void
field_check_target_mask_good(guint xres, guint yres,
                             guint txres, guint tyres,
                             const GwyFieldPart *fpart,
                             guint expected_col, guint expected_row)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    GwyMaskField *target = gwy_mask_field_new_sized(txres, tyres, FALSE);
    guint col, row;

    g_assert(gwy_field_check_target_mask(field, target, fpart, &col, &row));
    g_assert_cmpuint(col, ==, expected_col);
    g_assert_cmpuint(row, ==, expected_row);
    g_object_unref(target);
    g_object_unref(field);
}

void
test_field_check_target_mask_good(void)
{
    field_check_target_mask_good(17, 25, 17, 25, NULL, 0, 0);
    field_check_target_mask_good(17, 25, 17, 25,
                                 &(GwyFieldPart){ 0, 0, 17, 25 }, 0, 0);
    field_check_target_mask_good(17, 25, 4, 3,
                                 &(GwyFieldPart){ 0, 0, 4, 3 }, 0, 0);
    field_check_target_mask_good(17, 25, 4, 3,
                                 &(GwyFieldPart){ 13, 22, 4, 3 }, 0, 0);
    field_check_target_mask_good(17, 25, 17, 25,
                                 &(GwyFieldPart){ 13, 22, 4, 3 }, 13, 22);
}

static void
field_check_target_mask_empty(guint xres, guint yres,
                              guint txres, guint tyres,
                              const GwyFieldPart *fpart)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    GwyMaskField *target = gwy_mask_field_new_sized(txres, tyres, FALSE);
    guint col, row;

    g_assert(!gwy_field_check_target_mask(field, target, fpart, &col, &row));
    g_object_unref(target);
    g_object_unref(field);
}

void
test_field_check_target_mask_empty(void)
{
    field_check_target_mask_empty(17, 25, 17, 25,
                                  &(GwyFieldPart){ 0, 0, 0, 0 });
    field_check_target_mask_empty(17, 25, 17, 25,
                                  &(GwyFieldPart){ 17, 25, 0, 0 });
    field_check_target_mask_empty(17, 25, 17, 25,
                                  &(GwyFieldPart){ 100, 100, 0, 0 });
    field_check_target_mask_empty(17, 25, 34, 81,
                                  &(GwyFieldPart){ 100, 100, 0, 0 });
}

static void
field_check_target_mask_bad_subprocess_one(guint xres, guint yres,
                                           guint txres, guint tyres,
                                           const GwyFieldPart *fpart)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    GwyMaskField *target = gwy_mask_field_new_sized(txres, tyres, FALSE);
    guint col, row;
    gwy_field_check_target_mask(field, target, fpart, &col, &row);
}

void
test_field_check_target_mask_bad_subprocess_17251625001725(void)
{
    field_check_target_mask_bad_subprocess_one(17, 25, 16, 25,
                                               &(GwyFieldPart){ 0, 0, 17, 25 });
}

void
test_field_check_target_mask_bad_subprocess_17251726001725(void)
{
    field_check_target_mask_bad_subprocess_one(17, 25, 17, 26,
                                               &(GwyFieldPart){ 0, 0, 17, 25 });
}

void
test_field_check_target_mask_bad_subprocess_1725773478(void)
{
    field_check_target_mask_bad_subprocess_one(17, 25, 7, 7,
                                               &(GwyFieldPart){ 3, 4, 7, 8 });
}

void
test_field_check_target_mask_bad_subprocess_1725883478(void)
{
    field_check_target_mask_bad_subprocess_one(17, 25, 8, 8,
                                               &(GwyFieldPart){ 3, 4, 7, 8 });
}

void
test_field_check_target_mask_bad(void)
{
    assert_subprocesses_critical_fail("/testlibgwy/field/check-target-mask/bad",
                                      0, 0,
                                      "/17251625001725",
                                      "/17251726001725",
                                      "/1725773478",
                                      "/1725883478",
                                      NULL);
}

void
test_field_get(void)
{
    enum { max_size = 55, niter = 40 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);

        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++) {
                g_assert_cmpfloat(field->data[i*xres + j],
                                  ==,
                                  gwy_field_get(field, j, i));
            }
        }
        g_object_unref(field);
    }

    g_rand_free(rng);
}

void
test_field_set(void)
{
    enum { max_size = 55, niter = 40 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++)
                gwy_field_set(field, j, i, i*xres + j);
        }

        for (guint k = 0; k < xres*yres; k++) {
            g_assert_cmpfloat(field->data[k], ==, k);
        }
        g_object_unref(field);
    }

    g_rand_free(rng);
}

void
test_field_get_data_unmasked(void)
{
    enum { max_size = 50, niter = 100 };
    GRand *rng = g_rand_new_with_seed(42);

    for (gsize iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        GwyField *reference = gwy_field_duplicate(field);
        guint width = g_rand_int_range(rng, 0, xres+1);
        guint height = g_rand_int_range(rng, 0, yres+1);
        guint col = g_rand_int_range(rng, 0, xres+1 - width);
        guint row = g_rand_int_range(rng, 0, yres+1 - height);
        GwyFieldPart fpart = { col, row, width, height };

        guint ndata;
        gdouble *data = gwy_field_get_data(field, &fpart, NULL, GWY_MASK_IGNORE,
                                           &ndata);
        g_assert_cmpuint(ndata, ==, width*height);
        for (guint i = 0; i < ndata; i++)
            data[i] *= 2.0;
        gwy_field_set_data(field, &fpart, NULL, GWY_MASK_IGNORE, data, ndata);

        gwy_field_multiply(reference, &fpart, NULL, GWY_MASK_IGNORE, 2.0);
        field_assert_equal(field, reference);

        g_free(data);
        g_object_unref(field);
        g_object_unref(reference);
    }

    g_rand_free(rng);
}

void
test_field_get_data_masked(void)
{
    enum { max_size = 50, niter = 100 };
    GRand *rng = g_rand_new_with_seed(42);

    for (gsize iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        GwyField *reference = gwy_field_duplicate(field);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres+1 - width);
        guint row = g_rand_int_range(rng, 0, yres+1 - height);
        GwyFieldPart fpart = { col, row, width, height };
        GwyMaskField *mask = random_mask_field(width, height, rng);
        GwyMasking masking = (g_rand_boolean(rng)
                              ? GWY_MASK_INCLUDE
                              : GWY_MASK_EXCLUDE);

        guint ndata;
        gdouble *data = gwy_field_get_data(field, &fpart, mask, masking,
                                           &ndata);
        for (guint i = 0; i < ndata; i++)
            data[i] *= 2.0;
        gwy_field_set_data(field, &fpart, mask, masking, data, ndata);

        gwy_field_multiply(reference, &fpart, mask, masking, 2.0);
        field_assert_equal(field, reference);

        g_free(data);
        g_object_unref(mask);
        g_object_unref(field);
        g_object_unref(reference);
    }

    g_rand_free(rng);
}

void
test_field_get_data_full(void)
{
    enum { max_size = 50, niter = 10 };
    GRand *rng = g_rand_new_with_seed(42);

    for (gsize iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);

        guint ndata, ndata_full, ndata_expected = xres*yres;
        gdouble *data = gwy_field_get_data(field, NULL, NULL, GWY_MASK_IGNORE,
                                           &ndata);
        const gdouble *fulldata = gwy_field_get_data_full(field, &ndata_full);
        g_assert_cmpuint(ndata, ==, ndata_expected);
        g_assert_cmpuint(ndata_full, ==, ndata_expected);
        for (guint i = 0; i < ndata; i++)
            data[i] *= 2.0;
        gwy_field_set_data_full(field, data, ndata);

        for (guint i = 0; i < ndata; i++)
            g_assert_cmpfloat(fulldata[i], ==, data[i]);

        g_free(data);
        g_object_unref(field);
    }

    g_rand_free(rng);
}

void
test_field_clear_offsets(void)
{
    GwyField *field = gwy_field_new_sized(4, 4, TRUE);
    guint xoff_counter = 0, yoff_counter = 0;
    g_signal_connect_swapped(field, "notify::x-offset",
                             G_CALLBACK(record_signal), &xoff_counter);
    g_signal_connect_swapped(field, "notify::y-offset",
                             G_CALLBACK(record_signal), &yoff_counter);

    gwy_field_clear_offsets(field);
    g_assert_cmpuint(xoff_counter, ==, 0);
    g_assert_cmpuint(yoff_counter, ==, 0);

    gwy_field_set_xoffset(field, -0.5);
    g_assert_cmpuint(xoff_counter, ==, 1);
    g_assert_cmpuint(yoff_counter, ==, 0);
    gwy_field_clear_offsets(field);
    g_assert_cmpuint(xoff_counter, ==, 2);
    g_assert_cmpuint(yoff_counter, ==, 0);

    gwy_field_set_yoffset(field, 0.5);
    g_assert_cmpuint(xoff_counter, ==, 2);
    g_assert_cmpuint(yoff_counter, ==, 1);
    gwy_field_clear_offsets(field);
    g_assert_cmpuint(xoff_counter, ==, 2);
    g_assert_cmpuint(yoff_counter, ==, 2);

    gwy_field_set_xoffset(field, 1.0);
    gwy_field_set_yoffset(field, 1.0);
    g_assert_cmpuint(xoff_counter, ==, 3);
    g_assert_cmpuint(yoff_counter, ==, 3);
    gwy_field_clear_offsets(field);
    g_assert_cmpuint(xoff_counter, ==, 4);
    g_assert_cmpuint(yoff_counter, ==, 4);

    g_object_unref(field);
}

static void
field_part_copy_dumb(const GwyField *src,
                     guint col,
                     guint row,
                     guint width,
                     guint height,
                     GwyField *dest,
                     guint destcol,
                     guint destrow)
{
    for (guint i = 0; i < height; i++) {
        if (row + i >= src->yres || destrow + i >= dest->yres)
            continue;
        for (guint j = 0; j < width; j++) {
            if (col + j >= src->xres || destcol + j >= dest->xres)
                continue;

            gdouble val = gwy_field_index(src, col + j, row + i);
            gwy_field_index(dest, destcol + j, destrow + i) = val;
        }
    }
}

void
test_field_copy(void)
{
    enum { max_size = 19 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = g_test_slow() ? 1000 : 200;

    for (gsize iter = 0; iter < niter; iter++) {
        guint sxres = g_rand_int_range(rng, 1, max_size);
        guint syres = g_rand_int_range(rng, 1, max_size/2);
        guint dxres = g_rand_int_range(rng, 1, max_size);
        guint dyres = g_rand_int_range(rng, 1, max_size/2);
        GwyField *source = gwy_field_new_sized(sxres, syres, FALSE);
        GwyField *dest = gwy_field_new_sized(dxres, dyres, FALSE);
        GwyField *reference = gwy_field_new_sized(dxres, dyres, FALSE);
        field_randomize(source, rng);
        field_randomize(reference, rng);
        gwy_field_copy(reference, NULL, dest, 0, 0);
        guint width = g_rand_int_range(rng, 0, MAX(sxres, dxres));
        guint height = g_rand_int_range(rng, 0, MAX(syres, dyres));
        guint col = g_rand_int_range(rng, 0, sxres);
        guint row = g_rand_int_range(rng, 0, syres);
        guint destcol = g_rand_int_range(rng, 0, dxres);
        guint destrow = g_rand_int_range(rng, 0, dyres);
        if (sxres == dxres && g_rand_int_range(rng, 0, 2) == 0) {
            // Check the fast path
            col = destcol = 0;
            width = sxres;
        }
        GwyFieldPart fpart = { col, row, width, height };
        gwy_field_copy(source, &fpart, dest, destcol, destrow);
        field_part_copy_dumb(source, col, row, width, height,
                             reference, destcol, destrow);
        field_assert_equal(dest, reference);
        g_object_unref(source);
        g_object_unref(dest);
        g_object_unref(reference);
    }

    GwyField *field = gwy_field_new_sized(37, 41, TRUE);
    field_randomize(field, rng);
    GwyField *dest = gwy_field_new_alike(field, FALSE);
    gwy_field_copy_full(field, dest);
    g_assert_cmpint(memcmp(field->data, dest->data,
                           field->xres*field->yres*sizeof(gdouble)), ==, 0);
    g_object_unref(dest);
    g_object_unref(field);

    g_rand_free(rng);
}

void
test_field_new_part(void)
{
    enum { max_size = 23 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = g_test_slow() ? 1000 : 200;

    for (gsize iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size/2);
        GwyField *source = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(source, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };
        GwyField *part = gwy_field_new_part(source, &fpart, TRUE);
        GwyField *reference = gwy_field_new_sized(width, height, FALSE);
        gwy_field_set_xreal(reference, source->xreal*width/xres);
        gwy_field_set_yreal(reference, source->yreal*height/yres);
        gwy_field_set_xoffset(reference, source->xreal*col/xres);
        gwy_field_set_yoffset(reference, source->yreal*row/yres);
        field_part_copy_dumb(source, col, row, width, height,
                             reference, 0, 0);
        field_assert_equal(part, reference);
        g_object_unref(source);
        g_object_unref(part);
        g_object_unref(reference);
    }
    g_rand_free(rng);
}

static void
field_filter_gradient_one(GwyStandardFilter hfilter,
                          GwyStandardFilter vfilter,
                          GwyStandardFilter absfilter,
                          const gdouble *signature)
{
    static const gdouble step_pattern[] = { -1.0, -1.0, 0.0, 1.0, 1.0 };
    static const gdouble step_gradient[] = { 0.0, 1.0, 2.0, 1.0, 0.0 };

    GwyLine *patline = gwy_line_new_sized(5, FALSE);
    gwy_assign(patline->data, step_pattern, 5);

    GwyLine *gradline = gwy_line_new_sized(5, FALSE);
    gwy_assign(gradline->data, step_gradient, 5);

    GwyLine *sigline = gwy_line_new_sized(5, FALSE);
    gwy_assign(sigline->data, signature, 5);

    GwyField *pattern = gwy_line_outer_product(patline, patline);
    GwyField *hfield = gwy_field_new_alike(pattern, FALSE);
    GwyField *hreference = gwy_line_outer_product(sigline, gradline);
    gwy_field_filter_standard(pattern, NULL, hfield, hfilter,
                              GWY_EXTERIOR_BORDER_EXTEND, 0.0);
    field_assert_numerically_equal(hfield, hreference, 1e-14);

    GwyField *vreference = gwy_line_outer_product(gradline, sigline);
    GwyField *vfield = gwy_field_new_alike(pattern, FALSE);
    gwy_field_filter_standard(pattern, NULL, vfield, vfilter,
                              GWY_EXTERIOR_BORDER_EXTEND, 0.0);
    field_assert_numerically_equal(vfield, vreference, 1e-14);

    GwyField *absreference = gwy_field_new_alike(pattern, FALSE);
    gwy_field_hypot_field(absreference, hreference, vreference);
    GwyField *absfield = gwy_field_new_alike(pattern, FALSE);
    gwy_field_filter_standard(pattern, NULL, absfield, absfilter,
                              GWY_EXTERIOR_BORDER_EXTEND, 0.0);
    field_assert_numerically_equal(absfield, absreference, 1e-14);

    g_object_unref(absreference);
    g_object_unref(vreference);
    g_object_unref(hreference);
    g_object_unref(absfield);
    g_object_unref(vfield);
    g_object_unref(hfield);
    g_object_unref(pattern);
    g_object_unref(sigline);
    g_object_unref(gradline);
    g_object_unref(patline);
}

void
test_field_filter_standard_sobel(void)
{
    static const gdouble signature[] = { -1.0, -3.0/4.0, 0.0, 3.0/4.0, 1.0 };
    field_filter_gradient_one(GWY_STANDARD_FILTER_HSOBEL,
                              GWY_STANDARD_FILTER_VSOBEL,
                              GWY_STANDARD_FILTER_SOBEL,
                              signature);
}

void
test_field_filter_standard_prewitt(void)
{
    static const gdouble signature[] = { -1.0, -2.0/3.0, 0.0, 2.0/3.0, 1.0 };
    field_filter_gradient_one(GWY_STANDARD_FILTER_HPREWITT,
                              GWY_STANDARD_FILTER_VPREWITT,
                              GWY_STANDARD_FILTER_PREWITT,
                              signature);
}

void
test_field_filter_standard_scharr(void)
{
    static const gdouble signature[] = { -1.0, -13.0/16.0, 0.0, 13.0/16.0, 1.0 };
    field_filter_gradient_one(GWY_STANDARD_FILTER_HSCHARR,
                              GWY_STANDARD_FILTER_VSCHARR,
                              GWY_STANDARD_FILTER_SCHARR,
                              signature);
}

void
test_field_filter_standard_step(void)
{
    static const gdouble step_pattern[] = {
        -1.0, -1.0, -1.0, -1.0, 0.0, 1.0, 1.0, 1.0, 1.0
    };
    static const gdouble expected[] = {
        0.0, 0.0, 0.0, 1.0, 2.0, 1.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 1.0, 2.0, 1.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 1.0, 2.0, 1.0, 1.0, 0.0, 0.0,
        1.0, 1.0, 1.0, 1.0, 2.0, 1.0, 1.0, 1.0, 1.0,
        2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0,
        1.0, 1.0, 1.0, 1.0, 2.0, 1.0, 1.0, 1.0, 1.0,
        0.0, 0.0, 1.0, 1.0, 2.0, 1.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 1.0, 2.0, 1.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 1.0, 2.0, 1.0, 0.0, 0.0, 0.0,
    };

    GwyLine *patline = gwy_line_new_sized(9, FALSE);
    gwy_assign(patline->data, step_pattern, 9);

    GwyField *pattern = gwy_line_outer_product(patline, patline);
    GwyField *field = gwy_field_new_alike(pattern, FALSE);
    GwyField *reference = gwy_field_new_alike(pattern, FALSE);
    gwy_assign(reference->data, expected, 9*9);
    gwy_field_filter_standard(pattern, NULL, field, GWY_STANDARD_FILTER_STEP,
                              GWY_EXTERIOR_BORDER_EXTEND, 0.0);
    field_assert_numerically_equal(field, reference, 1e-14);

    g_object_unref(reference);
    g_object_unref(field);
    g_object_unref(pattern);
    g_object_unref(patline);
}

void
test_field_filter_standard_dechecker(void)
{
    enum { res = 66 };

    // A smooth function
    GwyField *source = gwy_field_new_sized(res, res, FALSE);
    for (guint i = 0; i < res; i++) {
        for (guint j = 0; j < res; j++) {
            gdouble x = 2.0*G_PI*(j + 0.5)/res, y = 2.0*G_PI*(i + 0.5)/res;
            source->data[i*res + j] = cos(x)*cos(y);
        }
    }

    // With checker pattern added
    GwyField *field = gwy_field_duplicate(source);
    for (guint i = 0; i < res; i++) {
        for (guint j = 0; j < res; j++)
            field->data[i*res + j] += (i + j) % 2 ? 1 : -1;
    }
    gwy_field_invalidate(field);

    gwy_field_filter_standard(field, NULL, field, GWY_STANDARD_FILTER_DECHECKER,
                              GWY_EXTERIOR_PERIODIC, 0.0);
    // Large tolerance, the reconstruction is approximate
    field_assert_numerically_equal(field, source, 3e-4);

    g_object_unref(field);
    g_object_unref(source);
}

static void
median_filter_dumb(const GwyField *field,
                   const GwyFieldPart *fpart,
                   GwyField *target,
                   const GwyMaskField *kernel,
                   GwyExterior exterior,
                   gdouble fill_value)
{
    guint col, row, width, height, targetcol, targetrow;
    if (!gwy_field_check_part(field, fpart, &col, &row, &width, &height)
        || !gwy_field_check_target(field, target,
                                   &(GwyFieldPart){ col, row, width, height },
                                   &targetcol, &targetrow)) {
        g_return_if_reached();
    }

    guint kxres = kernel->xres, kyres = kernel->yres;
    guint extx = kxres - 1, exty = kyres - 1;
    GwyField *extended = gwy_field_new_extended(field, fpart,
                                                extx/2, extx - extx/2,
                                                exty/2, exty - exty/2,
                                                exterior, fill_value, TRUE);
    GwyField *workspace = gwy_field_new_sized(kxres, kyres, FALSE);

    for (guint i = 0; i < height; i++) {
        for (guint j = 0; j < width; j++) {
            gwy_field_copy(extended, &(GwyFieldPart){ j, i, kxres, kyres },
                           workspace, 0, 0);
            gdouble median = gwy_field_median(workspace, NULL, kernel,
                                              GWY_MASK_INCLUDE);
            gwy_field_index(target, targetcol + j, targetrow + i) = median;
        }
    }

    g_object_unref(workspace);
    g_object_unref(extended);
}

static void
field_filter_median_small_one(guint shape)
{
    enum { max_size = 36, niter = 100 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        guint kxres = g_rand_int_range(rng, 1, max_size);
        guint kyres = g_rand_int_range(rng, 1, max_size);
        if (shape == 1) {
            kxres = MAX(3, kxres);
            kyres = MAX(3, kyres);
        }

        GwyField *source = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(source, rng);
        GwyField *target = gwy_field_new_sized(width, height, FALSE);
        GwyField *reference = gwy_field_new_alike(target, FALSE);
        GwyMaskField *kernel = gwy_mask_field_new_sized(kxres, kyres, FALSE);

        if (shape == 0)
            gwy_mask_field_fill(kernel, NULL, TRUE);
        else if (shape == 1) {
            gwy_mask_field_fill(kernel, NULL, FALSE);
            gwy_mask_field_fill(kernel,
                                &(GwyFieldPart){ 1, 1, kxres-1, kyres-1 },
                                TRUE);
        }
        else
            gwy_mask_field_fill_ellipse(kernel, NULL, TRUE, TRUE);

        GwyFieldPart fpart = { col, row, width, height };
        gwy_field_filter_median(source, &fpart, target, kernel,
                                GWY_EXTERIOR_MIRROR_EXTEND, NAN);
        median_filter_dumb(source, &fpart, reference, kernel,
                           GWY_EXTERIOR_MIRROR_EXTEND, NAN);

        field_assert_equal(target, reference);

        g_object_unref(reference);
        g_object_unref(kernel);
        g_object_unref(target);
        g_object_unref(source);
    }
    g_rand_free(rng);
}

void
test_field_filter_median_direct_small_rect(void)
{
    gwy_tune_algorithms("median-filter-method", "direct");
    field_filter_median_small_one(0);
    gwy_tune_algorithms("median-filter-method", "auto");
}

void
test_field_filter_median_bucket_small_rect(void)
{
    gwy_tune_algorithms("median-filter-method", "bucket");
    field_filter_median_small_one(0);
    gwy_tune_algorithms("median-filter-method", "auto");
}

void
test_field_filter_median_direct_small_shrunkrect(void)
{
    gwy_tune_algorithms("median-filter-method", "direct");
    field_filter_median_small_one(1);
    gwy_tune_algorithms("median-filter-method", "auto");
}

void
test_field_filter_median_bucket_small_shrunkrect(void)
{
    gwy_tune_algorithms("median-filter-method", "bucket");
    field_filter_median_small_one(1);
    gwy_tune_algorithms("median-filter-method", "auto");
}

void
test_field_filter_median_direct_small_ellipse(void)
{
    gwy_tune_algorithms("median-filter-method", "direct");
    field_filter_median_small_one(2);
    gwy_tune_algorithms("median-filter-method", "auto");
}

void
test_field_filter_median_bucket_small_ellipse(void)
{
    gwy_tune_algorithms("median-filter-method", "bucket");
    field_filter_median_small_one(2);
    gwy_tune_algorithms("median-filter-method", "auto");
}

static void
field_filter_median_large_rect_one(void)
{
    enum { max_ksize = 20, niter = 5 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 300, 2000);
        guint yres = g_rand_int_range(rng, 300, 2000);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        guint kxres = g_rand_int_range(rng, 1, max_ksize);
        guint kyres = g_rand_int_range(rng, 1, max_ksize);

        GwyField *source = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(source, rng);
        GwyField *target = gwy_field_new_sized(width, height, FALSE);
        GwyField *reference = gwy_field_new_alike(target, FALSE);
        GwyMaskField *kernel = gwy_mask_field_new_sized(kxres, kyres, FALSE);
        gwy_mask_field_fill(kernel, NULL, TRUE);
        GwyFieldPart fpart = { col, row, width, height };
        gwy_field_filter_median(source, &fpart, target, kernel,
                                GWY_EXTERIOR_MIRROR_EXTEND, NAN);
        median_filter_dumb(source, &fpart, reference, kernel,
                           GWY_EXTERIOR_MIRROR_EXTEND, NAN);

        field_assert_equal(target, reference);

        g_object_unref(reference);
        g_object_unref(kernel);
        g_object_unref(target);
        g_object_unref(source);
    }
    g_rand_free(rng);
}

void
test_field_filter_median_direct_large_rect(void)
{
    gwy_tune_algorithms("median-filter-method", "direct");
    field_filter_median_large_rect_one();
    gwy_tune_algorithms("median-filter-method", "auto");
}

void
test_field_filter_median_bucket_large_rect(void)
{
    gwy_tune_algorithms("median-filter-method", "bucket");
    field_filter_median_large_rect_one();
    gwy_tune_algorithms("median-filter-method", "auto");
}

static void
minmax_filter_dumb(const GwyField *field,
                   const GwyFieldPart *fpart,
                   GwyField *target,
                   const GwyMaskField *kernel,
                   gboolean maximum,
                   GwyExterior exterior,
                   gdouble fill_value)
{
    guint col, row, width, height, targetcol, targetrow;
    if (!gwy_field_check_part(field, fpart, &col, &row, &width, &height)
        || !gwy_field_check_target(field, target,
                                   &(GwyFieldPart){ col, row, width, height },
                                   &targetcol, &targetrow)) {
        g_return_if_reached();
    }

    guint kxres = kernel->xres, kyres = kernel->yres;
    guint extx = kxres - 1, exty = kyres - 1;
    GwyField *extended = gwy_field_new_extended(field, fpart,
                                                extx/2, extx - extx/2,
                                                exty/2, exty - exty/2,
                                                exterior, fill_value, TRUE);
    GwyField *workspace = gwy_field_new_sized(kxres, kyres, FALSE);

    for (guint i = 0; i < height; i++) {
        for (guint j = 0; j < width; j++) {
            gwy_field_copy(extended, &(GwyFieldPart){ j, i, kxres, kyres },
                           workspace, 0, 0);
            gdouble min, max;
            gwy_field_min_max(workspace, NULL, kernel,
                              GWY_MASK_INCLUDE, &min, &max);
            gdouble v = maximum ? max : min;
            gwy_field_index(target, targetcol + j, targetrow + i) = v;
        }
    }

    g_object_unref(workspace);
    g_object_unref(extended);
}

static void
field_filter_minmax_small_one(guint shape, gboolean maximum)
{
    enum { max_size = 36, niter = 100 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        guint kxres = g_rand_int_range(rng, 1, max_size);
        guint kyres = g_rand_int_range(rng, 1, max_size);
        if (shape == 1) {
            kxres = MAX(3, kxres);
            kyres = MAX(3, kyres);
        }

        GwyField *source = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(source, rng);
        GwyField *target = gwy_field_new_sized(width, height, FALSE);
        GwyField *reference = gwy_field_new_alike(target, FALSE);
        GwyMaskField *kernel = gwy_mask_field_new_sized(kxres, kyres, FALSE);

        if (shape == 0)
            gwy_mask_field_fill(kernel, NULL, TRUE);
        else if (shape == 1) {
            gwy_mask_field_fill(kernel, NULL, FALSE);
            gwy_mask_field_fill(kernel,
                                &(GwyFieldPart){ 1, 1, kxres-1, kyres-1 },
                                TRUE);
        }
        else
            gwy_mask_field_fill_ellipse(kernel, NULL, TRUE, TRUE);

        GwyFieldPart fpart = { col, row, width, height };
        if (maximum) {
            gwy_field_filter_max(source, &fpart, target, kernel,
                                 GWY_EXTERIOR_MIRROR_EXTEND, NAN);
        }
        else {
            gwy_field_filter_min(source, &fpart, target, kernel,
                                 GWY_EXTERIOR_MIRROR_EXTEND, NAN);
        }
        minmax_filter_dumb(source, &fpart, reference, kernel, maximum,
                           GWY_EXTERIOR_MIRROR_EXTEND, NAN);

        field_assert_equal(target, reference);

        g_object_unref(reference);
        g_object_unref(kernel);
        g_object_unref(target);
        g_object_unref(source);
    }
    g_rand_free(rng);
}

void
test_field_filter_max_direct_small_rect(void)
{
    field_filter_minmax_small_one(0, TRUE);
}

void
test_field_filter_max_direct_small_shrunkrect(void)
{
    field_filter_minmax_small_one(1, TRUE);
}

void
test_field_filter_max_direct_small_ellipse(void)
{
    field_filter_minmax_small_one(2, TRUE);
}

void
test_field_filter_min_direct_small_rect(void)
{
    field_filter_minmax_small_one(0, TRUE);
}

void
test_field_filter_min_direct_small_shrunkrect(void)
{
    field_filter_minmax_small_one(1, TRUE);
}

void
test_field_filter_min_direct_small_ellipse(void)
{
    field_filter_minmax_small_one(2, TRUE);
}

static void
field_filter_minmax_large_rect_one(gboolean maximum)
{
    enum { max_ksize = 20, niter = 5 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 300, 2000);
        guint yres = g_rand_int_range(rng, 300, 2000);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        guint kxres = g_rand_int_range(rng, 1, max_ksize);
        guint kyres = g_rand_int_range(rng, 1, max_ksize);

        GwyField *source = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(source, rng);
        GwyField *target = gwy_field_new_sized(width, height, FALSE);
        GwyField *reference = gwy_field_new_alike(target, FALSE);
        GwyMaskField *kernel = gwy_mask_field_new_sized(kxres, kyres, FALSE);
        gwy_mask_field_fill(kernel, NULL, TRUE);
        GwyFieldPart fpart = { col, row, width, height };
        if (maximum) {
            gwy_field_filter_max(source, &fpart, target, kernel,
                                 GWY_EXTERIOR_MIRROR_EXTEND, NAN);
        }
        else {
            gwy_field_filter_min(source, &fpart, target, kernel,
                                 GWY_EXTERIOR_MIRROR_EXTEND, NAN);
        }
        minmax_filter_dumb(source, &fpart, reference, kernel, maximum,
                           GWY_EXTERIOR_MIRROR_EXTEND, NAN);

        field_assert_equal(target, reference);

        g_object_unref(reference);
        g_object_unref(kernel);
        g_object_unref(target);
        g_object_unref(source);
    }
    g_rand_free(rng);
}

void
test_field_filter_max_direct_large_rect(void)
{
    field_filter_minmax_large_rect_one(TRUE);
}

void
test_field_filter_min_direct_large_rect(void)
{
    field_filter_minmax_large_rect_one(TRUE);
}

static void
field_move_periodically(GwyField *field,
                        gint xmove, gint ymove)
{
    GwyField *extended = gwy_field_new_extended(field, NULL,
                                                MAX(xmove, 0), MAX(-xmove, 0),
                                                MAX(ymove, 0), MAX(-ymove, 0),
                                                GWY_EXTERIOR_PERIODIC, 0.0,
                                                FALSE);
    GwyFieldPart fpart = {
        MAX(-xmove, 0), MAX(-ymove, 0), field->xres, field->yres
    };
    gwy_field_copy(extended, &fpart, field, 0, 0);
    g_object_unref(extended);
}

void
test_field_correlate_crosscorrelate(void)
{
    enum { max_size = 44, maxsearch = 5, niter = 40 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        // With too small kernels we can get higher correlation score elsewhere
        // by a mere chance (at least without normalising).  And with too large
        // moves the wrapped-around copy can be nearer than the supposedly
        // closest one.
        guint xres = g_rand_int_range(rng, 8, max_size);
        guint yres = g_rand_int_range(rng, 8, max_size);
        guint kxres = 2*g_rand_int_range(rng, 2, xres/4+1) + 1;
        guint kyres = 2*g_rand_int_range(rng, 2, yres/4+1) + 1;
        guint colsearch = g_rand_int_range(rng, 1, MIN(maxsearch+1, xres/2));
        guint rowsearch = g_rand_int_range(rng, 1, MIN(maxsearch+1, yres/2));
        gint xmove = g_rand_int_range(rng, -(gint)colsearch, colsearch+1);
        gint ymove = g_rand_int_range(rng, -(gint)rowsearch, rowsearch+1);

        GwyMaskField *kernel = gwy_mask_field_new_sized(kxres, kyres, FALSE);
        gwy_mask_field_fill_ellipse(kernel, NULL, TRUE, TRUE);

        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        gwy_field_normalize(field, NULL, NULL, GWY_MASK_IGNORE, 0.0, 1.0,
                            GWY_NORMALIZE_MEAN | GWY_NORMALIZE_RMS);
        GwyField *moved = gwy_field_duplicate(field);
        field_move_periodically(moved, xmove, ymove);

        GwyField *score = gwy_field_new_alike(field, FALSE);
        GwyField *xoff = gwy_field_new_alike(field, FALSE);
        GwyField *yoff = gwy_field_new_alike(field, FALSE);

        gwy_field_crosscorrelate(moved, field, NULL, score, xoff, yoff,
                                 kernel, colsearch,rowsearch,
                                 GWY_CROSSCORRELATION_LEVEL
                                 | GWY_CROSSCORRELATION_NORMALIZE,
                                 GWY_EXTERIOR_PERIODIC, 0.0);

        for (guint i = 0; i < xres*yres; i++) {
            gwy_assert_floatval(score->data[i], 1.0, 0.001);
            // This should be safe to require exactly, integeres are preserved.
            g_assert_cmpfloat(xoff->data[i], ==, xmove);
            g_assert_cmpfloat(yoff->data[i], ==, ymove);
        }

        g_object_unref(yoff);
        g_object_unref(xoff);
        g_object_unref(score);
        g_object_unref(kernel);
        g_object_unref(moved);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

static void
field_read_exterior_one(GwyExterior exterior)
{
    enum { max_size = 31, niter = 80 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);

        guint left = g_rand_int_range(rng, 0, 2*xres);
        guint right = g_rand_int_range(rng, 0, 2*xres);
        guint up = g_rand_int_range(rng, 0, 2*yres);
        guint down = g_rand_int_range(rng, 0, 2*yres);

        GwyField *extended = gwy_field_new_extended(field, NULL,
                                                    left, right, up, down,
                                                    exterior, G_PI, TRUE);

        for (guint i = 0; i < yres + up + down; i++) {
            for (guint j = 0; j < xres + left + right; j++) {
                gdouble zext = gwy_field_value(extended, j, i, exterior, NAN);
                gdouble zread = gwy_field_value(field,
                                                (gint)j - left, (gint)i - up,
                                                exterior, G_PI);
                g_assert_cmpfloat(zread, ==, zext);
            }
        }

        g_object_unref(extended);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_read_exterior_fixed(void)
{
    field_read_exterior_one(GWY_EXTERIOR_FIXED_VALUE);
}

void
test_field_read_exterior_border(void)
{
    field_read_exterior_one(GWY_EXTERIOR_BORDER_EXTEND);
}

void
test_field_read_exterior_mirror(void)
{
    field_read_exterior_one(GWY_EXTERIOR_MIRROR_EXTEND);
}

void
test_field_read_exterior_periodic(void)
{
    field_read_exterior_one(GWY_EXTERIOR_PERIODIC);
}

void
test_field_read_interpolated(void)
{
    enum { max_size = 54, niter = 40, niiter = 10 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 8, max_size);
        guint yres = g_rand_int_range(rng, 8, max_size);
        gdouble xreal = exp(4.0*g_rand_double(rng) - 2.0);
        gdouble yreal = exp(4.0*g_rand_double(rng) - 2.0);
        gdouble phi = g_rand_double_range(rng, 0.0, 2.0*G_PI);
        gdouble q = exp(4.0*g_rand_double(rng) - 2.0);

        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        gwy_field_set_xreal(field, xreal);
        gwy_field_set_yreal(field, yreal);

        for (guint i = 0; i < yres; i++) {
            gdouble y = (i + 0.5)*gwy_field_dy(field) + field->yoff;
            for (guint j = 0; j < xres; j++) {
                gdouble x = (j + 0.5)*gwy_field_dx(field) + field->xoff;
                gdouble z = x*q*cos(phi) + y*q*sin(phi);
                field->data[i*xres + j] = z;
            }
        }

        for (guint iiter = 0; iiter < niiter; iiter++) {
            guint col = g_rand_int_range(rng, 1, xres-1);
            guint row = g_rand_int_range(rng, 1, yres-1);

            gdouble zpix = gwy_field_value(field, col, row,
                                           GWY_EXTERIOR_BORDER_EXTEND, NAN);
            gdouble zint = gwy_field_value_interpolated
                                          (field, col + 0.5, row + 0.5,
                                           GWY_INTERPOLATION_LINEAR,
                                           GWY_EXTERIOR_BORDER_EXTEND, NAN);

            gwy_assert_floatval(zint, zpix, 1e-14);
        }

        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_read_averaged(void)
{
    enum { max_size = 54, niter = 40, niiter = 10 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 8, max_size);
        guint yres = g_rand_int_range(rng, 8, max_size);
        gdouble xreal = exp(4.0*g_rand_double(rng) - 2.0);
        gdouble yreal = exp(4.0*g_rand_double(rng) - 2.0);
        gdouble phi = g_rand_double_range(rng, 0.0, 2.0*G_PI);
        gdouble q = exp(4.0*g_rand_double(rng) - 2.0);

        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        gwy_field_set_xreal(field, xreal);
        gwy_field_set_yreal(field, yreal);

        for (guint i = 0; i < yres; i++) {
            gdouble y = (i + 0.5)*gwy_field_dy(field) + field->yoff;
            for (guint j = 0; j < xres; j++) {
                gdouble x = (j + 0.5)*gwy_field_dx(field) + field->xoff;
                gdouble z = x*q*cos(phi) + y*q*sin(phi);
                field->data[i*xres + j] = z;
            }
        }

        for (guint iiter = 0; iiter < niiter; iiter++) {
            guint col = g_rand_int_range(rng, 1, xres-1);
            guint row = g_rand_int_range(rng, 1, yres-1);
            guint ax = g_rand_int_range(rng, 0, MIN(col, xres-col));
            guint ay = g_rand_int_range(rng, 0, MIN(row, yres-row));

            gdouble zpix = gwy_field_value(field, col, row,
                                           GWY_EXTERIOR_BORDER_EXTEND, NAN);
            gdouble zar0 = gwy_field_value_averaged
                                          (field, NULL, GWY_MASK_IGNORE,
                                           col, row, 0, 0, FALSE,
                                           GWY_EXTERIOR_BORDER_EXTEND, NAN);
            gdouble zae0 = gwy_field_value_averaged
                                          (field, NULL, GWY_MASK_IGNORE,
                                           col, row, 0, 0, TRUE,
                                           GWY_EXTERIOR_BORDER_EXTEND, NAN);
            gdouble zar1 = gwy_field_value_averaged
                                          (field, NULL, GWY_MASK_IGNORE,
                                           col, row, ax, ay, FALSE,
                                           GWY_EXTERIOR_BORDER_EXTEND, NAN);
            gdouble zae1 = gwy_field_value_averaged
                                          (field, NULL, GWY_MASK_IGNORE,
                                           col, row, ax, ay, TRUE,
                                           GWY_EXTERIOR_BORDER_EXTEND, NAN);

            gwy_assert_floatval(zar0, zpix, 1e-14);
            gwy_assert_floatval(zae0, zpix, 1e-14);
            gwy_assert_floatval(zar1, zpix, 1e-14);
            gwy_assert_floatval(zae1, zpix, 1e-14);
        }

        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_read_slope(void)
{
    enum { max_size = 54, niter = 40, niiter = 10 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 8, max_size);
        guint yres = g_rand_int_range(rng, 8, max_size);
        gdouble xreal = exp(4.0*g_rand_double(rng) - 2.0);
        gdouble yreal = exp(4.0*g_rand_double(rng) - 2.0);
        gdouble phi = g_rand_double_range(rng, 0.0, 2.0*G_PI);
        gdouble q = exp(4.0*g_rand_double(rng) - 2.0);

        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        gwy_field_set_xreal(field, xreal);
        gwy_field_set_yreal(field, yreal);

        for (guint i = 0; i < yres; i++) {
            gdouble y = (i + 0.5)*gwy_field_dy(field) + field->yoff;
            for (guint j = 0; j < xres; j++) {
                gdouble x = (j + 0.5)*gwy_field_dx(field) + field->xoff;
                gdouble z = x*q*cos(phi) + y*q*sin(phi);
                field->data[i*xres + j] = z;
            }
        }

        for (guint iiter = 0; iiter < niiter; iiter++) {
            guint col = g_rand_int_range(rng, 1, xres-1);
            guint row = g_rand_int_range(rng, 1, yres-1);
            guint ax = g_rand_int_range(rng, 0, MIN(col+1, xres-col));
            guint ay = g_rand_int_range(rng, 0, MIN(row+1, yres-row));

            gdouble zpix = gwy_field_value(field, col, row,
                                           GWY_EXTERIOR_BORDER_EXTEND, NAN);
            gdouble ar, bxr, byr, ae, bxe, bye;
            gboolean okr, oke;
            okr = gwy_field_slope(field, NULL, GWY_MASK_IGNORE,
                                  col, row, ax, ay,
                                  FALSE, GWY_EXTERIOR_BORDER_EXTEND, NAN,
                                  &ar, &bxr, &byr);
            oke = gwy_field_slope(field, NULL, GWY_MASK_IGNORE,
                                  col, row, ax, ay,
                                  TRUE, GWY_EXTERIOR_BORDER_EXTEND, NAN,
                                  &ae, &bxe, &bye);

            gwy_assert_floatval(ar, zpix, 1e-14);
            gwy_assert_floatval(ae, zpix, 1e-14);
            if (ax == 0 || ay == 0) {
                g_assert(!okr);
                g_assert(!oke);
                g_assert_cmpfloat(bxr, ==, 0.0);
                g_assert_cmpfloat(bxe, ==, 0.0);
                g_assert_cmpfloat(byr, ==, 0.0);
                g_assert_cmpfloat(bye, ==, 0.0);
            }
            else {
                g_assert(okr);
                g_assert(oke);
                g_assert_cmpfloat(fabs(bxr - q*cos(phi)), <=, 1e-13);
                g_assert_cmpfloat(fabs(bxe - q*cos(phi)), <=, 1e-13);
                g_assert_cmpfloat(fabs(byr - q*sin(phi)), <=, 1e-13);
                g_assert_cmpfloat(fabs(bye - q*sin(phi)), <=, 1e-13);
            }
        }

        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_read_curvature_at_centre(void)
{
    enum { max_size = 54, niter = 40, niiter = 100 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 8, max_size);
        guint yres = g_rand_int_range(rng, 8, max_size);
        gdouble xreal = exp(4.0*g_rand_double(rng) - 2.0);
        gdouble yreal = exp(4.0*g_rand_double(rng) - 2.0);
        gdouble xoff = 40.0*g_rand_double(rng) - 20.0;
        gdouble yoff = 40.0*g_rand_double(rng) - 20.0;
        gdouble a = 40.0*g_rand_double(rng) - 20.0;
        gdouble bx = 10.0*g_rand_double(rng) - 5.0;
        gdouble by = 10.0*g_rand_double(rng) - 5.0;
        gdouble cxx = 4.0*g_rand_double(rng) - 2.0;
        gdouble cxy = 4.0*g_rand_double(rng) - 2.0;
        gdouble cyy = 4.0*g_rand_double(rng) - 2.0;

        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        gwy_field_set_xreal(field, xreal);
        gwy_field_set_yreal(field, yreal);
        gwy_field_set_xoffset(field, xoff);
        gwy_field_set_yoffset(field, yoff);

        for (guint i = 0; i < yres; i++) {
            gdouble y = (i + 0.5)*gwy_field_dy(field) + field->yoff;
            for (guint j = 0; j < xres; j++) {
                gdouble x = (j + 0.5)*gwy_field_dx(field) + field->xoff;
                gdouble z = a + bx*x + by*y + cxx*x*x + cxy*x*y + cyy*y*y;
                field->data[i*xres + j] = z;
            }
        }

        GwyCurvatureParams curv, curvr, curve;
        gdouble coeffs[] = { a, bx, by, cxx, cxy, cyy };
        gint ndims = gwy_math_curvature_at_centre(coeffs, &curv);

        for (guint iiter = 0; iiter < niiter; iiter++) {
            guint col = g_rand_int_range(rng, 2, xres-2);
            guint row = g_rand_int_range(rng, 2, yres-2);
            guint ax = g_rand_int_range(rng, 2, MIN(col+1, xres-col));
            guint ay = g_rand_int_range(rng, 2, MIN(row+1, yres-row));

            // Allow quite large relative errors as we have not done precise
            // uncertainty analysis and they can be large for the ‘thin’
            // sizes.
            gdouble eps = 1e-4/sqrt(2*ax + 2*ay - 3);

            gint ndimsr = gwy_field_curvature
                                     (field, NULL, GWY_MASK_IGNORE,
                                      col, row, ax, ay,
                                      FALSE, TRUE,
                                      GWY_EXTERIOR_BORDER_EXTEND, NAN,
                                      &curvr);
            g_assert_cmpint(ndimsr, ==, ndims);
            gwy_assert_floatval(curvr.k1/curv.k1, 1.0, eps);
            gwy_assert_floatval(curvr.k2/curv.k2, 1.0, eps);
            gwy_assert_floatval(curvr.phi1/curv.phi1, 1.0, eps);
            gwy_assert_floatval(curvr.phi2/curv.phi2, 1.0, eps);
            gwy_assert_floatval(curvr.xc/curv.xc, 1.0, eps);
            gwy_assert_floatval(curvr.yc/curv.yc, 1.0, eps);
            gwy_assert_floatval(curvr.zc/curv.zc, 1.0, eps);

            gint ndimse = gwy_field_curvature
                                     (field, NULL, GWY_MASK_IGNORE,
                                      col, row, ax, ay,
                                      TRUE, TRUE,
                                      GWY_EXTERIOR_BORDER_EXTEND, NAN,
                                      &curve);
            g_assert_cmpint(ndimse, ==, ndims);
            gwy_assert_floatval(curve.k1/curv.k1, 1.0, eps);
            gwy_assert_floatval(curve.k2/curv.k2, 1.0, eps);
            gwy_assert_floatval(curve.phi1/curv.phi1, 1.0, eps);
            gwy_assert_floatval(curve.phi2/curv.phi2, 1.0, eps);
            gwy_assert_floatval(curve.xc/curv.xc, 1.0, eps);
            gwy_assert_floatval(curve.yc/curv.yc, 1.0, eps);
            gwy_assert_floatval(curve.zc/curv.zc, 1.0, eps);
        }

        g_object_unref(field);
    }
    g_rand_free(rng);
}

static void
field_mark_outliers_one(GwyMasking masking,
                        GwyDeviation deviation)
{
    enum { max_size = 178 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 8, max_size);
        guint yres = g_rand_int_range(rng, 8, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);

        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };

        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        guint count = width*height;
        if (masking == GWY_MASK_INCLUDE)
            count = gwy_mask_field_part_count(mask, &fpart, TRUE);
        else if (masking == GWY_MASK_EXCLUDE)
            count = gwy_mask_field_part_count(mask, &fpart, FALSE);

        guint nout = g_rand_int_range(rng, 0, MAX(count/20, 1) + 1), ti = nout;
        guint nupper = 0, nlower = 0;
        while (ti) {
            guint i = g_rand_int_range(rng, row, row+height);
            guint j = g_rand_int_range(rng, col, col+width);
            if (masking == GWY_MASK_INCLUDE && !gwy_mask_field_get(mask, j, i))
                continue;
            if (masking == GWY_MASK_EXCLUDE && gwy_mask_field_get(mask, j, i))
                continue;
            // Already changed this value.
            if (fabs(field->data[i*xres + j]) > 2.0)
                continue;

            gdouble v = 1e30*(g_rand_double(rng) - 0.5);
            field->data[i*xres + j] = v;
            if (v > 0.0)
                nupper++;
            else
                nlower++;
            ti--;
        }
        g_assert_cmpuint(nout, ==, nlower + nupper);

        GwyMaskField *outliers = gwy_mask_field_new_sized(width, height, FALSE);
        guint result = gwy_field_mark_outliers(field, &fpart, outliers,
                                               mask, masking, deviation, 4.0);
        if (masking == GWY_MASK_EXCLUDE)
            gwy_mask_field_logical(mask, NULL, NULL, GWY_LOGICAL_NA);

        guint nset;
        if (masking != GWY_MASK_IGNORE) {
            GwyMaskField *maskpart = gwy_mask_field_new_part(mask, &fpart);
            nset = gwy_mask_field_count(outliers, maskpart, TRUE);
            g_object_unref(maskpart);
        }
        else
            nset = gwy_mask_field_count(outliers, NULL, TRUE);
        g_assert_cmpuint(nset, ==, result);

        if (count >= 6) {
            if (deviation == GWY_DEVIATION_UP)
                g_assert_cmpuint(result, ==, nupper);
            else if (deviation == GWY_DEVIATION_DOWN)
                g_assert_cmpuint(result, ==, nlower);
            else if (deviation == GWY_DEVIATION_BOTH)
                g_assert_cmpuint(result, ==, nupper + nlower);
            else
                g_assert_not_reached();
        }
        else {
            // FIXME: This is the current implementation, not an absolute
            // criterion.  If we can detect outliers in very small sample sizes
            // update this.
            g_assert_cmpuint(result, ==, 0);
        }

        g_object_unref(outliers);
        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_mark_outliers_ignore_up(void)
{
    field_mark_outliers_one(GWY_MASK_IGNORE, GWY_DEVIATION_UP);
}

void
test_field_mark_outliers_ignore_down(void)
{
    field_mark_outliers_one(GWY_MASK_IGNORE, GWY_DEVIATION_DOWN);
}

void
test_field_mark_outliers_ignore_both(void)
{
    field_mark_outliers_one(GWY_MASK_IGNORE, GWY_DEVIATION_BOTH);
}

void
test_field_mark_outliers_include_up(void)
{
    field_mark_outliers_one(GWY_MASK_INCLUDE, GWY_DEVIATION_UP);
}

void
test_field_mark_outliers_include_down(void)
{
    field_mark_outliers_one(GWY_MASK_INCLUDE, GWY_DEVIATION_DOWN);
}

void
test_field_mark_outliers_include_both(void)
{
    field_mark_outliers_one(GWY_MASK_INCLUDE, GWY_DEVIATION_BOTH);
}

void
test_field_mark_outliers_exclude_up(void)
{
    field_mark_outliers_one(GWY_MASK_EXCLUDE, GWY_DEVIATION_UP);
}

void
test_field_mark_outliers_exclude_down(void)
{
    field_mark_outliers_one(GWY_MASK_EXCLUDE, GWY_DEVIATION_DOWN);
}

void
test_field_mark_outliers_exclude_both(void)
{
    field_mark_outliers_one(GWY_MASK_EXCLUDE, GWY_DEVIATION_BOTH);
}

static void
field_mark_extrema_one(const gdouble *data, guint xres, guint yres,
                       const gchar *expected_str,
                       gboolean maxima)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    gwy_assign(field->data, data, xres*yres);
    GwyMaskField *mask = gwy_mask_field_new_sized(xres, yres, FALSE);
    gwy_field_mark_extrema(field, mask, maxima);
    GwyMaskField *expected = mask_field_from_string(expected_str);
    mask_field_assert_equal(mask, expected);
    g_object_unref(expected);
    g_object_unref(mask);
    g_object_unref(field);
}

void
test_field_mark_maxima_maze(void)
{
    enum { xres = 11, yres = 9 };
    static const gdouble data[xres*yres] = {
        1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1,
        1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
        1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
        1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
        1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1,
        1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0,
        1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1,
        1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0,
        1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    };
    const gchar *expected_str =
        "### ### ###\n"
        "# # # # # #\n"
        "# # # # # #\n"
        "# # # # # #\n"
        "# # # ### #\n"
        "# # #      \n"
        "# # #######\n"
        "# #     #  \n"
        "# #########\n";
    field_mark_extrema_one(data, xres, yres, expected_str, TRUE);
}

void
test_field_mark_maxima_badmaze(void)
{
    enum { xres = 11, yres = 9 };
    static const gdouble data[xres*yres] = {
        1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1,
        1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
        1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
        1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
        1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 2,
        1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0,
        1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1,
        1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0,
        1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    };
    const gchar *expected_str =
        "           \n"
        "           \n"
        "           \n"
        "           \n"
        "          #\n"
        "           \n"
        "           \n"
        "           \n"
        "           \n";
    field_mark_extrema_one(data, xres, yres, expected_str, TRUE);
}

void
test_field_mark_maxima_pinhole(void)
{
    enum { xres = 11, yres = 9 };
    static const gdouble data[xres*yres] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    };
    const gchar *expected_str =
        "###########\n"
        "###########\n"
        "###########\n"
        "###########\n"
        "###########\n"
        "###########\n"
        "######## ##\n"
        "###########\n"
        "###########\n";
    field_mark_extrema_one(data, xres, yres, expected_str, TRUE);
}

void
test_field_mark_maxima_tip(void)
{
    enum { xres = 11, yres = 9 };
    static const gdouble data[xres*yres] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    };
    const gchar *expected_str =
        "           \n"
        "           \n"
        "           \n"
        "           \n"
        "           \n"
        "           \n"
        "        #  \n"
        "           \n"
        "           \n";
    field_mark_extrema_one(data, xres, yres, expected_str, TRUE);
}

void
test_field_mark_maxima_chessboard(void)
{
    enum { xres = 12, yres = 9 };
    static const gdouble data[xres*yres] = {
        1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1,
        1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1,
        1, 1, 1, 2, 2, 2, 0, 0, 0, 1, 1, 1,
        0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0,
        0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0,
        0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0,
        1, 1, 1, 0, 0, 0, 1, 1, 1, 2, 2, 2,
        1, 1, 1, 0, 0, 0, 1, 1, 1, 2, 2, 2,
        1, 1, 1, 0, 0, 0, 1, 1, 1, 2, 2, 2,
    };
    const gchar *expected_str =
        "   ###   ###\n"
        "   ###   ###\n"
        "   ###   ###\n"
        "      ###   \n"
        "      ###   \n"
        "      ###   \n"
        "###      ###\n"
        "###      ###\n"
        "###      ###\n";
    field_mark_extrema_one(data, xres, yres, expected_str, TRUE);
}

void
test_field_mark_minima_maze(void)
{
    enum { xres = 11, yres = 9 };
    static const gdouble data[xres*yres] = {
        -1, -1, -1,  0, -1, -1, -1,  0, -1, -1, -1,
        -1,  0, -1,  0, -1,  0, -1,  0, -1,  0, -1,
        -1,  0, -1,  0, -1,  0, -1,  0, -1,  0, -1,
        -1,  0, -1,  0, -1,  0, -1,  0, -1,  0, -1,
        -1,  0, -1,  0, -1,  0, -1, -1, -1,  0, -1,
        -1,  0, -1,  0, -1,  0,  0,  0,  0,  0,  0,
        -1,  0, -1,  0, -1, -1, -1, -1, -1, -1, -1,
        -1,  0, -1,  0,  0,  0,  0,  0, -1,  0,  0,
        -1,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    };
    const gchar *expected_str =
        "### ### ###\n"
        "# # # # # #\n"
        "# # # # # #\n"
        "# # # # # #\n"
        "# # # ### #\n"
        "# # #      \n"
        "# # #######\n"
        "# #     #  \n"
        "# #########\n";
    field_mark_extrema_one(data, xres, yres, expected_str, FALSE);
}

void
test_field_mark_minima_badmaze(void)
{
    enum { xres = 11, yres = 9 };
    static const gdouble data[xres*yres] = {
        -1, -1, -1,  0, -1, -1, -1,  0, -1, -1, -1,
        -1,  0, -1,  0, -1,  0, -1,  0, -1,  0, -1,
        -1,  0, -1,  0, -1,  0, -1,  0, -1,  0, -1,
        -1,  0, -1,  0, -1,  0, -1,  0, -1,  0, -1,
        -1,  0, -1,  0, -1,  0, -1, -1, -1,  0, -2,
        -1,  0, -1,  0, -1,  0,  0,  0,  0,  0,  0,
        -1,  0, -1,  0, -1, -1, -1, -1, -1, -1, -1,
        -1,  0, -1,  0,  0,  0,  0,  0, -1,  0,  0,
        -1,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    };
    const gchar *expected_str =
        "           \n"
        "           \n"
        "           \n"
        "           \n"
        "          #\n"
        "           \n"
        "           \n"
        "           \n"
        "           \n";
    field_mark_extrema_one(data, xres, yres, expected_str, FALSE);
}

void
test_field_mark_minima_pinhole(void)
{
    enum { xres = 11, yres = 9 };
    static const gdouble data[xres*yres] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,  0, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    };
    const gchar *expected_str =
        "###########\n"
        "###########\n"
        "###########\n"
        "###########\n"
        "###########\n"
        "###########\n"
        "######## ##\n"
        "###########\n"
        "###########\n";
    field_mark_extrema_one(data, xres, yres, expected_str, FALSE);
}

void
test_field_mark_minima_tip(void)
{
    enum { xres = 11, yres = 9 };
    static const gdouble data[xres*yres] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -2, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    };
    const gchar *expected_str =
        "           \n"
        "           \n"
        "           \n"
        "           \n"
        "           \n"
        "           \n"
        "        #  \n"
        "           \n"
        "           \n";
    field_mark_extrema_one(data, xres, yres, expected_str, FALSE);
}

void
test_field_mark_minima_chessboard(void)
{
    enum { xres = 12, yres = 9 };
    static const gdouble data[xres*yres] = {
        -1, -1, -1, -2, -2, -2,  0,  0,  0, -1, -1, -1,
        -1, -1, -1, -2, -2, -2,  0,  0,  0, -1, -1, -1,
        -1, -1, -1, -2, -2, -2,  0,  0,  0, -1, -1, -1,
         0,  0,  0, -1, -1, -1, -2, -2, -2,  0,  0,  0,
         0,  0,  0, -1, -1, -1, -2, -2, -2,  0,  0,  0,
         0,  0,  0, -1, -1, -1, -2, -2, -2,  0,  0,  0,
        -1, -1, -1,  0,  0,  0, -1, -1, -1, -2, -2, -2,
        -1, -1, -1,  0,  0,  0, -1, -1, -1, -2, -2, -2,
        -1, -1, -1,  0,  0,  0, -1, -1, -1, -2, -2, -2,
    };
    const gchar *expected_str =
        "   ###   ###\n"
        "   ###   ###\n"
        "   ###   ###\n"
        "      ###   \n"
        "      ###   \n"
        "      ###   \n"
        "###      ###\n"
        "###      ###\n"
        "###      ###\n";
    field_mark_extrema_one(data, xres, yres, expected_str, FALSE);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
